#!/usr/bin/env python3
"""Survey of FLTSIM98.EXE's runtime: which routines the executable reaches.

    from host_rt_survey import HostSurvey

Reads the CD's executable with objdump and follows its control flow from the entry, the
window procedures and every code address its .rdata and .data hold or its instructions push,
stopping at the 194 services (HOST's table at 0x417ba0). Nothing is translated: this is the
list of routines, their instructions, switch tables and stdcall pops that
tools/gen_host_rt_tables.py turns into the runtime's tables (src/host/runtime/host_rt_tables.inc).
"""
from collections import deque
import hashlib
from pathlib import Path
import re
import subprocess

from dialogmap import Fs98Pe

SHA256 = 'f47c124a1ad44475a8a3a5ecbce83f5dc5d6e7dad78cf120b20616738bd99f6a'

# The conditional branches the executable uses.
CONDITIONS = ('je', 'jne', 'jb', 'jae', 'ja', 'jbe', 'jl', 'jg', 'jle', 'jns', 'js')


class HostSurvey:
    def __init__(self, path='orig/FLTSIM98.EXE'):
        self.path = Path(path)
        if hashlib.sha256(self.path.read_bytes()).hexdigest() != SHA256:
            raise ValueError('host reference hash differs')
        self.pe = Fs98Pe(self.path)
        self.instructions, self.next, self.tables = {}, {}, {}
        self.islands = set()
        self.pointers = {}
        self.services = {}
        self.decode(0x1000)
        for slot in range(0x18, 0x320, 4):
            va = self.pe.va_uint32(0x417ba0 + slot)
            if va:
                self.services.setdefault(va - self.pe.uiBase, slot)
        self.roots = {0x1000, 0x10b0, 0x1504a, 0x1a70, 0x20b0, 0x2050, 0x119a0, 0x1950, 0x7cc0, 0x7d10,
                      0x7e40, 0x7ec0, 0x7ef0}
        for section in self.pe.aSections:
            if section['name'] not in ('.rdata', '.data'):
                continue
            for site in range(section['rva'], section['rva'] + section['raw_size'] - 3, 4):
                value = self.pe.va_uint32(self.pe.uiBase + site)
                if 0x401000 <= value < 0x41c5cc:
                    self.pointers[site] = value - self.pe.uiBase
                    if self.pe.is_code(value):
                        self.roots.add(value - self.pe.uiBase)
        # Byte-index maps, each bounded by the preceding unsigned switch check.
        for start, count in ((0x3378, 16), (0x3818, 106), (0x5884, 17)):
            self.islands.update(range(start, start + count))
        self.functions = {}
        pending = deque(sorted(self.roots))
        while pending:
            start = pending.popleft()
            if start in self.functions:
                continue
            # A service is HOST's (src/host/host.c), not the runtime's.
            if start in self.services:
                self.functions[start] = set()
                continue
            body = self.discover(start)
            self.functions[start] = body
            for address in body:
                op, args = self.instructions[address]
                if op == 'call' and args.startswith('0x'):
                    pending.append(int(args, 16) - self.pe.uiBase)
                # A code address pushed or moved is a callback: a routine of its own.
                if op in ('push', 'mov') and re.fullmatch(r'0x[0-9a-f]+', args.split(',')[-1]):
                    for immediate in re.findall(r'0x[0-9a-f]+', args):
                        value = int(immediate, 16)
                        if self.pe.is_code(value):
                            root = value - self.pe.uiBase
                            self.roots.add(root)
                            pending.append(root)
        reached = set().union(*self.functions.values())
        occupied = {}
        for address in reached:
            for byte in range(address, self.next[address]):
                if byte in occupied and occupied[byte] != address:
                    raise ValueError(f'overlapping instructions {address:x}/{occupied[byte]:x}')
                occupied[byte] = address
        if set(occupied) & self.islands:
            raise ValueError('switch data overlaps reachable instructions')
        self.signatures = {}
        for start, body in self.functions.items():
            returns = {int(self.instructions[a][1] or '0', 0)
                       for a in body if self.instructions[a][0] == 'ret'}
            if len(returns) == 1:
                self.signatures[start] = returns.pop()

    def decode(self, start):
        output = subprocess.check_output(
            ['objdump', '-d', '-M', 'intel', '--insn-width=16',
             f'--start-address={self.pe.uiBase + start}', str(self.path)], text=True)
        for line in output.splitlines():
            match = re.fullmatch(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s+(\S+)\s*(.*)', line)
            if match:
                address = int(match[1], 16) - self.pe.uiBase
                op, args = match[3], re.sub(r' <[^>]+>$', '', match[4].strip())
                self.instructions[address] = op, args
                self.next[address] = address + len(match[2].split())

    def jump_targets(self, address, args):
        match = re.fullmatch(r'DWORD PTR \[[a-z]+\*4\+(0x[0-9a-f]+)\]', args)
        if not match:
            raise ValueError(f'indirect jump {address:x}: {args}')
        table = int(match[1], 16) - self.pe.uiBase
        targets, cursor = [], table
        while True:
            value = self.pe.va_uint32(self.pe.uiBase + cursor)
            if not self.pe.is_code(value):
                break
            targets.append(value - self.pe.uiBase)
            self.islands.update(range(cursor, cursor + 4))
            cursor += 4
        if not targets:
            raise ValueError(f'empty switch table {table:x}')
        self.tables[address] = table, targets
        return targets

    def discover(self, start):
        queue, body = deque([start]), set()
        while queue:
            address = queue.popleft()
            if address in body:
                continue
            if address not in self.instructions:
                self.decode(address)
            if address not in self.instructions:
                raise ValueError(f'missing instruction {address:x}')
            body.add(address)
            op, args = self.instructions[address]
            if op == 'ret':
                continue
            if op == 'jmp' or op in CONDITIONS or op in ('jcxz', 'jecxz', 'loop'):
                if args.startswith('0x'):
                    queue.append(int(args, 16) - self.pe.uiBase)
                else:
                    # A jump through an import slot is a thunk: it leaves the executable.
                    if args.startswith('DWORD PTR ds:'):
                        continue
                    queue.extend(self.jump_targets(address, args))
                if op == 'jmp':
                    continue
            queue.append(self.next[address])
        return body
