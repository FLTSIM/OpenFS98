#!/usr/bin/env python3
"""Survey FS98's localised dialog modules without loading the game.

    python3 tools/dialogmap.py
    python3 tools/dialogmap.py --format json > /tmp/dialogmap.json
    python3 tools/dialogmap.py --format markdown > /tmp/dialogmap.md
    python3 tools/dialogmap.py --format registry > /tmp/dialog_registry.tsv

Reads original PE32 files, not installed/rebuilt modules. Registry entries and
resource templates are exact; optional objdump factory analysis is deliberately
narrow, and reports unresolved factories rather than guessing a resource id.
No extracted strings, bitmaps or templates are written to the repository.
"""

import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys


class SurveyError(ValueError):
    pass


class Fs98Pe:
    """Small, bounds-checked PE32 reader; no third-party Python dependencies."""

    def __init__(self, pPath):
        self.pPath = Path(pPath)
        self.byData = self.pPath.read_bytes()
        if self.read_bytes(0, 2) != b'MZ':
            raise SurveyError('missing MZ signature')
        iPe = self.read_uint32(0x3c)
        if self.read_bytes(iPe, 4) != b'PE\0\0':
            raise SurveyError('missing PE signature')
        iOptional = iPe + 24
        if self.read_uint16(iOptional) != 0x10b:
            raise SurveyError('expected PE32')
        self.uiBase = self.read_uint32(iOptional + 28)
        self.uiTimestamp = self.read_uint32(iPe + 8)
        self.iHeaders = self.read_uint32(iOptional + 60)
        iOptionalSize = self.read_uint16(iPe + 20)
        iDirectories = min(self.read_uint32(iOptional + 92), 16)
        if iOptionalSize < 96 + iDirectories * 8:
            raise SurveyError('truncated optional header')
        self.aDirectories = [
            (self.read_uint32(iOptional + 96 + iIndex * 8),
             self.read_uint32(iOptional + 100 + iIndex * 8))
            for iIndex in range(iDirectories)]
        self.aSections = []
        for iIndex in range(self.read_uint16(iPe + 6)):
            iSection = iOptional + iOptionalSize + iIndex * 40
            self.aSections.append({
                'name': self.read_bytes(iSection, 8).rstrip(b'\0').decode('ascii'),
                'virtual_size': self.read_uint32(iSection + 8),
                'rva': self.read_uint32(iSection + 12),
                'raw_size': self.read_uint32(iSection + 16),
                'raw_offset': self.read_uint32(iSection + 20),
                'flags': self.read_uint32(iSection + 36)})

    def read_bytes(self, iOffset, iSize):
        if iOffset < 0 or iSize < 0 or iOffset + iSize > len(self.byData):
            raise SurveyError(f'file read outside image: {iOffset:#x}+{iSize:#x}')
        return self.byData[iOffset:iOffset + iSize]

    def read_uint16(self, iOffset):
        return struct.unpack('<H', self.read_bytes(iOffset, 2))[0]

    def read_uint32(self, iOffset):
        return struct.unpack('<I', self.read_bytes(iOffset, 4))[0]

    def file_offset(self, uiRva, iSize=1):
        if 0 <= uiRva < self.iHeaders and uiRva + iSize <= self.iHeaders:
            self.read_bytes(uiRva, iSize)
            return uiRva
        for dSection in self.aSections:
            iDelta = uiRva - dSection['rva']
            if 0 <= iDelta and iDelta + iSize <= dSection['raw_size']:
                iOffset = dSection['raw_offset'] + iDelta
                self.read_bytes(iOffset, iSize)
                return iOffset
        raise SurveyError(f'RVA has no file-backed bytes: {uiRva:#x}+{iSize:#x}')

    def va_uint32(self, uiVa):
        return self.read_uint32(self.file_offset(uiVa - self.uiBase, 4))

    def is_code(self, uiVa):
        return any(dSection['flags'] & 0x20000000 and
                   dSection['rva'] <= uiVa - self.uiBase <
                   dSection['rva'] + dSection['virtual_size']
                   for dSection in self.aSections)

    def c_string(self, uiRva):
        byText = bytearray()
        for iIndex in range(4096):
            byChar = self.byData[self.file_offset(uiRva + iIndex)]
            if byChar == 0:
                return byText.decode('ascii')
            byText.append(byChar)
        raise SurveyError('unterminated PE string')

    def directory(self, iIndex):
        return self.aDirectories[iIndex] if iIndex < len(self.aDirectories) else (0, 0)

    def exports(self):
        uiRva, uiSize = self.directory(0)
        if not uiRva:
            return {}
        iOffset = self.file_offset(uiRva, 40)
        uiCount = self.read_uint32(iOffset + 20)
        uiNames = self.read_uint32(iOffset + 24)
        uiFunctions = self.read_uint32(iOffset + 28)
        uiNameTable = self.read_uint32(iOffset + 32)
        uiOrdinals = self.read_uint32(iOffset + 36)
        if max(uiCount, uiNames) > 65536:
            raise SurveyError('unreasonable export table')
        dExports = {}
        for iIndex in range(uiNames):
            uiName = self.read_uint32(self.file_offset(uiNameTable + iIndex * 4, 4))
            unOrdinal = self.read_uint16(self.file_offset(uiOrdinals + iIndex * 2, 2))
            if unOrdinal >= uiCount:
                raise SurveyError('export ordinal outside function table')
            uiFunction = self.read_uint32(self.file_offset(uiFunctions + unOrdinal * 4, 4))
            dExports[self.c_string(uiName)] = uiFunction
        return dExports

    def imports(self):
        uiRva, uiSize = self.directory(1)
        aImports = []
        if not uiRva:
            return aImports
        for iIndex in range(uiSize // 20):
            iOffset = self.file_offset(uiRva + iIndex * 20, 20)
            aFields = struct.unpack('<5I', self.read_bytes(iOffset, 20))
            if not any(aFields):
                return aImports
            uiLookup, _, _, uiName, uiIat = aFields
            szDll = self.c_string(uiName)
            for iThunk in range(65536):
                uiValue = self.read_uint32(self.file_offset((uiLookup or uiIat) + iThunk * 4, 4))
                if not uiValue:
                    break
                aImports.append({
                    'dll': szDll,
                    'name': f'#{uiValue & 0xffff}' if uiValue & 0x80000000
                    else self.c_string(uiValue + 2),
                    'iat_va': self.uiBase + uiIat + iThunk * 4})
            else:
                raise SurveyError('unterminated import thunks')
        raise SurveyError('unterminated import directory')

    def resources(self):
        uiRoot, uiSize = self.directory(2)
        aLeaves = []
        if not uiRoot:
            return aLeaves

        def resource_offset(iRelative, iSize):
            if iRelative < 0 or iRelative + iSize > uiSize:
                raise SurveyError('resource directory offset outside resource extent')
            return self.file_offset(uiRoot + iRelative, iSize)

        def walk_directory(iRelative, aKeys, aAncestors):
            if iRelative in aAncestors or len(aKeys) >= 3:
                raise SurveyError('cyclic or over-deep resource tree')
            iOffset = resource_offset(iRelative, 16)
            iCount = self.read_uint16(iOffset + 12) + self.read_uint16(iOffset + 14)
            for iIndex in range(iCount):
                iEntry = resource_offset(iRelative + 16 + iIndex * 8, 8)
                uiKey = self.read_uint32(iEntry)
                uiTarget = self.read_uint32(iEntry + 4)
                if uiKey & 0x80000000:
                    iName = resource_offset(uiKey & 0x7fffffff, 2)
                    iLength = self.read_uint16(iName)
                    iName = resource_offset((uiKey & 0x7fffffff) + 2, iLength * 2)
                    vKey = self.read_bytes(iName, iLength * 2).decode('utf-16-le')
                else:
                    vKey = uiKey
                aNextKeys = aKeys + [vKey]
                if uiTarget & 0x80000000:
                    walk_directory(uiTarget & 0x7fffffff, aNextKeys,
                                   aAncestors + [iRelative])
                else:
                    if len(aNextKeys) != 3:
                        raise SurveyError('expected type/name/language resource leaf')
                    iLeaf = resource_offset(uiTarget, 16)
                    uiData = self.read_uint32(iLeaf)
                    uiLength = self.read_uint32(iLeaf + 4)
                    self.file_offset(uiData, uiLength)
                    aLeaves.append({'type': aNextKeys[0], 'id': aNextKeys[1],
                                    'language': aNextKeys[2], 'rva': uiData,
                                    'size': uiLength})

        walk_directory(0, [], [])
        return aLeaves

    def linkage(self):
        dExports = self.exports()
        if 'Linkage' not in dExports:
            return None
        uiLinkage = self.uiBase + dExports['Linkage']
        dLinkage = {'va': uiLinkage, 'id': self.va_uint32(uiLinkage),
                    'init_va': self.va_uint32(uiLinkage + 4),
                    'deinit_va': self.va_uint32(uiLinkage + 8),
                    'flags': self.va_uint32(uiLinkage + 0x10),
                    'dialogs_va': self.va_uint32(uiLinkage + 0x14),
                    'imports': [], 'dialogs': []}
        if 'ImportTable' in dExports:
            uiImports = self.uiBase + dExports['ImportTable']
            for iIndex in range(256):
                uiId = self.va_uint32(uiImports + iIndex * 8)
                uiBound = self.va_uint32(uiImports + iIndex * 8 + 4)
                if uiId == 0 and uiBound == 0:
                    break
                dLinkage['imports'].append({'id': uiId, 'link_va': uiImports + iIndex * 8 + 4})
            else:
                raise SurveyError('unterminated FS98 imports')
        if dLinkage['dialogs_va']:
            for iIndex in range(1024):
                uiRecord = dLinkage['dialogs_va'] + iIndex * 12
                uiId = self.va_uint32(uiRecord)
                # The host stops on id alone (0x409c18 and 0x409c42).
                if not uiId:
                    break
                uiCreate = self.va_uint32(uiRecord + 4)
                uiFlags = self.va_uint32(uiRecord + 8)
                if not self.is_code(uiCreate):
                    raise SurveyError(f'dialog {uiId:#x} factory is not executable')
                dLinkage['dialogs'].append({'id': uiId, 'create_va': uiCreate,
                                           'flags': uiFlags, 'index': iIndex})
            else:
                raise SurveyError('unterminated dialog registry')
        return dLinkage


def read_template(pPe, dResource):
    """Parse DLGTEMPLATE/DLGTEMPLATEEX including aligned control records."""
    byTemplate = pPe.read_bytes(pPe.file_offset(dResource['rva'], dResource['size']),
                               dResource['size'])
    iCursor = 0

    def take(szFormat):
        nonlocal iCursor
        iSize = struct.calcsize(szFormat)
        if iCursor + iSize > len(byTemplate):
            raise SurveyError(f'truncated dialog template {dResource["id"]}')
        aValues = struct.unpack_from(szFormat, byTemplate, iCursor)
        iCursor += iSize
        return aValues

    def read_name():
        unValue, = take('<H')
        if unValue == 0xffff:
            unOrdinal, = take('<H')
            return unOrdinal
        byText = bytearray()
        while unValue:
            byText.extend(struct.pack('<H', unValue))
            unValue, = take('<H')
        return byText.decode('utf-16-le')

    bExtended = byTemplate[:4] == b'\x01\x00\xff\xff'
    if bExtended:
        _, _, uiHelp, uiExStyle, uiStyle, unCount, nX, nY, nWidth, nHeight = take('<HHIIIHhhhh')
    else:
        uiStyle, uiExStyle, unCount, nX, nY, nWidth, nHeight = take('<IIHhhhh')
    vMenu = read_name()
    vClass = read_name()
    vTitle = read_name()
    dTemplate = dict(dResource, extended=bExtended, style=uiStyle,
                     ex_style=uiExStyle, rect=[nX, nY, nWidth, nHeight],
                     menu=vMenu, window_class=vClass, title=vTitle, controls=[])
    if uiStyle & 0x40:  # DS_SETFONT
        unPointSize, = take('<H')
        if bExtended:
            unWeight, byItalic, byCharset = take('<HBB')
            dTemplate['font_weight'] = unWeight
            dTemplate['font_italic'] = byItalic
            dTemplate['font_charset'] = byCharset
        dTemplate['font_size'] = unPointSize
        dTemplate['font_face'] = read_name()
    for iIndex in range(unCount):
        iCursor = (iCursor + 3) & ~3
        if bExtended:
            uiHelp, uiExStyle, uiStyle, nX, nY, nWidth, nHeight, uiId = take('<IIIhhhhI')
        else:
            uiStyle, uiExStyle, nX, nY, nWidth, nHeight, uiId = take('<IIhhhhH')
        vClass = read_name()
        vTitle = read_name()
        unExtra, = take('<H')
        if unExtra:
            # Standard creation-data length includes its own WORD. Extended
            # extraCount counts only the bytes that follow it.
            if not bExtended and unExtra < 2:
                raise SurveyError('invalid control creation-data size')
            iCursor += unExtra if bExtended else unExtra - 2
            if iCursor > len(byTemplate):
                raise SurveyError('truncated control creation data')
        dTemplate['controls'].append({'id': uiId, 'class': vClass,
                                      'title': vTitle, 'style': uiStyle,
                                      'ex_style': uiExStyle,
                                      'rect': [nX, nY, nWidth, nHeight]})
    dTemplate['parsed_size'] = iCursor
    return dTemplate


def disassemble(pPe):
    pResult = subprocess.run(['objdump', '-d', '-M', 'intel', '--no-show-raw-insn',
                              str(pPe.pPath)], check=True, capture_output=True, text=True)
    dInstructions = {}
    for szLine in pResult.stdout.splitlines():
        pMatch = re.match(r'\s*([0-9a-f]+):\s+(\S+)\s*(.*)', szLine)
        if pMatch:
            dInstructions[int(pMatch[1], 16)] = (pMatch[2], pMatch[3].strip())
    return dInstructions


def read_message_maps(pPe, dInstructions):
    """Recognise MFC maps returned by a literal-address getter followed by ret.

    Each candidate must have a local base-map getter and a terminated array of
    24-byte {message, code, first_id, last_id, signature, handler} records with
    executable handlers. These are static maps, not evidence that an event ran.
    """
    aMaps = []
    aInstructions = list(dInstructions.items())
    for iIndex, (uiGetter, (szOpcode, szOperands)) in enumerate(aInstructions[:-1]):
        pMatch = re.fullmatch(r'eax,0x([0-9a-f]+)', szOperands)
        if szOpcode != 'mov' or not pMatch or aInstructions[iIndex + 1][1][0] != 'ret':
            continue
        uiMap = int(pMatch[1], 16)
        try:
            uiBaseGetter = pPe.va_uint32(uiMap)
            uiEntries = pPe.va_uint32(uiMap + 4)
            if not pPe.is_code(uiBaseGetter):
                continue
            aEntries = []
            for iEntry in range(512):
                aFields = [pPe.va_uint32(uiEntries + iEntry * 24 + iField * 4)
                           for iField in range(6)]
                if not any(aFields):
                    aMaps.append({'getter_va': uiGetter, 'map_va': uiMap,
                                  'base_getter_va': uiBaseGetter, 'entries_va': uiEntries,
                                  'entries': aEntries})
                    break
                uiMessage, uiCode, uiFirst, uiLast, uiSignature, uiHandler = aFields
                if not 0 < uiMessage <= 0xffff or uiFirst > uiLast or not pPe.is_code(uiHandler):
                    break
                aEntries.append({'message': uiMessage, 'code': uiCode,
                                 'first_id': uiFirst, 'last_id': uiLast,
                                 'signature': uiSignature, 'handler_va': uiHandler})
        except SurveyError:
            continue  # Most literal-return getters are not message-map getters.
    return aMaps


def analyse_factories(pPe, dLinkage, aImports):
    """Recognise straight-line MFC factories, following local IAT jump thunks.

    Matching a push whose value happens to equal a resource id is insufficient:
    require a call to a resource-creation entry with that immediate at the top
    of the explicit push list. Branches and calls clear that list. MFC42 #2086
    is pinned to this retail ABI: the CD's implementation at 0x5f406539 uses
    its first stack argument in an RT_DIALOG (5) resource lookup. This is not
    an ordinal-name database for arbitrary MFC versions.
    """
    dInstructions = disassemble(pPe)
    dIat = {dImport['iat_va']: dImport for dImport in aImports}
    aAddresses = sorted(dInstructions)
    dIndices = {uiAddress: iIndex for iIndex, uiAddress in enumerate(aAddresses)}

    def resolve_call(szOperands):
        pMatch = re.fullmatch(r'DWORD PTR ds:0x([0-9a-f]+)', szOperands)
        if pMatch:
            return dIat.get(int(pMatch[1], 16))
        pMatch = re.match(r'0x([0-9a-f]+)(?:\s|$)', szOperands)
        if pMatch:
            uiTarget = int(pMatch[1], 16)
            szOpcode, szTarget = dInstructions.get(uiTarget, ('', ''))
            if szOpcode == 'jmp':
                pMatch = re.fullmatch(r'DWORD PTR ds:0x([0-9a-f]+)', szTarget)
                if pMatch:
                    return dIat.get(int(pMatch[1], 16))
        return None

    def scan_factory(uiStart):
        aPushes = []
        aMatches = []
        aReturns = []
        aLocalCalls = []
        iStart = dIndices.get(uiStart)
        if iStart is None:
            raise SurveyError('factory not found in objdump disassembly')
        # These are hints from the factory itself, not a transitive call graph.
        for uiAddress in aAddresses[iStart:iStart + 160]:
            szOpcode, szOperands = dInstructions[uiAddress]
            if szOpcode == 'push':
                pMatch = re.fullmatch(r'0x([0-9a-f]+)', szOperands)
                aPushes.append(int(pMatch[1], 16) if pMatch else None)
            elif szOpcode == 'call':
                dImport = resolve_call(szOperands)
                pMatch = re.fullmatch(r'0x([0-9a-f]+)', szOperands)
                if not dImport and pMatch:
                    aLocalCalls.append(int(pMatch[1], 16))
                if dImport and aPushes and aPushes[-1] is not None:
                    szName = dImport['name']
                    if (dImport['dll'].upper() == 'MFC42.DLL' and szName == '#2086'
                            or szName.startswith('?Init@CSimsDialog@@')):
                        aMatches.append({'resource_id': aPushes[-1], 'call_va': uiAddress,
                                         'import': szName, 'dll': dImport['dll']})
                aPushes = []
            elif szOpcode.startswith('j') or szOpcode in ('pop', 'leave'):
                aPushes = []
            elif szOpcode == 'ret':
                aReturns.append(int(szOperands, 16) if szOperands else 0)
                break
        return {'template_calls': aMatches, 'first_ret_pop': aReturns[0] if aReturns else None,
                'local_calls': aLocalCalls}

    aMaps = read_message_maps(pPe, dInstructions)
    for dDialog in dLinkage['dialogs']:
        dAnalysis = scan_factory(dDialog['create_va'])
        dDialog.update(dAnalysis)
        # Flag 2 tells the host to expect a null-terminated HWND array.
        # Its factory calls ordinary factories for the individual windows.
        if dDialog['flags'] & 2:
            dDialog['child_factories'] = [dict(scan_factory(uiTarget), create_va=uiTarget)
                                          for uiTarget in dAnalysis['local_calls']
                                          if uiTarget in dIndices]
    return aMaps


def scan_callers(pPath):
    """Conservative basic-block scan of calls to host +0x10c/+0x110.

    Reports unresolved ids too. No claim of completeness: pointers preserved
    across a call, jump tables, and ids supplied by a caller need manual work.
    A site is emitted only when the service-table register comes from the
    module's actual host ImportTable entry. Host direct calls use the pinned
    reference addresses in docs/modules/host.md.
    """
    pPe = Fs98Pe(pPath)
    dLinkage = pPe.linkage()
    aHostLinks = {dImport['link_va'] for dImport in dLinkage['imports']
                  if dImport['id'] == 1} if dLinkage else set()
    bHost = Path(pPath).name.upper() == 'FLTSIM98.EXE'
    dHostRegisters = {}
    dConstants = {}
    aPushes = []
    aSites = []
    szRegisters = r'eax|ebx|ecx|edx|esi|edi|ebp'
    for uiAddress, (szOpcode, szOperands) in disassemble(pPe).items():
        if szOpcode == 'push':
            pMatch = re.fullmatch(r'0x([0-9a-f]+)', szOperands)
            aPushes.append(int(pMatch[1], 16) if pMatch else dConstants.get(szOperands))
            continue
        if szOpcode == 'call':
            uiSlot = None
            pMatch = re.fullmatch(rf'DWORD PTR \[({szRegisters})\+0x(10c|110)\]', szOperands)
            if pMatch and dHostRegisters.get(pMatch[1]):
                uiSlot = int(pMatch[2], 16)
            if bHost:
                pMatch = re.fullmatch(r'0x(409720|409bd0)', szOperands)
                if pMatch:
                    uiSlot = 0x10c if pMatch[1] == '409720' else 0x110
            if uiSlot is not None:
                aSites.append({'call_va': uiAddress, 'host_slot': uiSlot,
                               'dialog_id': aPushes[-1] if aPushes else None})
            # Start fresh even for callee-saved registers: this is a local
            # syntactic survey, not an ABI-aware symbolic execution engine.
            dHostRegisters.clear()
            dConstants.clear()
            aPushes = []
            continue
        if szOpcode.startswith('j') or szOpcode in ('ret', 'leave', 'pop'):
            dHostRegisters.clear()
            dConstants.clear()
            aPushes = []
            continue
        pMatch = re.match(rf'({szRegisters}),(.+)$', szOperands)
        if not pMatch or szOpcode in ('cmp', 'test'):
            continue
        szDestination, szSource = pMatch.groups()
        bHostSource = False
        uiConstant = None
        if szOpcode == 'mov':
            pLink = re.fullmatch(r'(?:DWORD PTR )?ds:0x([0-9a-f]+)', szSource)
            bHostSource = (bool(pLink and int(pLink[1], 16) in aHostLinks)
                           or dHostRegisters.get(szSource, False))
            pConstant = re.fullmatch(r'0x([0-9a-f]+)', szSource)
            uiConstant = int(pConstant[1], 16) if pConstant else dConstants.get(szSource)
        dHostRegisters[szDestination] = bHostSource
        dConstants[szDestination] = uiConstant
    return {'path': str(pPath), 'name': Path(pPath).name, 'sites': aSites}


def survey_file(pPath, bDisassemble):
    pPe = Fs98Pe(pPath)
    aImports = pPe.imports()
    aResources = pPe.resources()
    dLinkage = pPe.linkage()
    aMaps = []
    if bDisassemble:
        if dLinkage and dLinkage['dialogs']:
            aMaps = analyse_factories(pPe, dLinkage, aImports)
        elif any(dImport['dll'].upper() == 'MFC42.DLL' for dImport in aImports):
            aMaps = read_message_maps(pPe, disassemble(pPe))
    return {'path': str(pPath), 'name': pPe.pPath.name,
            'sha256': hashlib.sha256(pPe.byData).hexdigest(),
            'image_base': pPe.uiBase, 'timestamp': pPe.uiTimestamp,
            'text_size': sum(dSection['virtual_size'] for dSection in pPe.aSections
                             if dSection['name'] == '.text'),
            'export_count': len(pPe.exports()), 'linkage': dLinkage,
            'pe_imports': aImports, 'mfc_message_maps': aMaps,
            'resource_counts': dict(collections.Counter(str(dResource['type'])
                                                       for dResource in aResources)),
            'templates': [read_template(pPe, dResource) for dResource in aResources
                          if dResource['type'] == 5]}


def module_names(pRoot):
    dNames = {1: 'HOST'}
    for pPath in sorted((pRoot / 'Modules').glob('*')):
        if pPath.suffix.lower() != '.dll':
            continue
        pPe = Fs98Pe(pPath)
        dExports = pPe.exports()
        if 'Linkage' in dExports:
            dNames[pPe.va_uint32(pPe.uiBase + dExports['Linkage'])] = pPath.stem.upper()
    return dNames


def format_id(uiId):
    return f'0x{uiId:08x}'


def print_report(aFiles, dNames, bMarkdown, aCallers):
    szSeparator = ' | ' if bMarkdown else '\t'

    def row(aCells):
        szLine = szSeparator.join(str(vCell).replace('|', '\\|').replace('\n', ' ')
                                  for vCell in aCells)
        print(f'| {szLine} |' if bMarkdown else szLine)

    row(['Module', 'ID', '.text bytes', 'Dialogs', 'Templates', 'FS98 imports'])
    if bMarkdown:
        row(['---'] * 6)
    for dFile in aFiles:
        dLinkage = dFile['linkage']
        row([dFile['name'], format_id(dLinkage['id']) if dLinkage else '-',
             dFile['text_size'], len(dLinkage['dialogs']) if dLinkage else 0,
             len(dFile['templates']),
             ', '.join(dNames.get(dImport['id'], format_id(dImport['id']))
                       for dImport in dLinkage['imports']) if dLinkage else '-'])
    print()
    row(['Owner', 'Registry ID', 'Factory VA', 'Flags', 'Ret pop', 'Factory template calls'])
    if bMarkdown:
        row(['---'] * 6)
    for dFile in aFiles:
        if not dFile['linkage']:
            continue
        for dDialog in dFile['linkage']['dialogs']:
            aTemplates = [f'0x{dCall["resource_id"]:x}'
                          for dCall in dDialog.get('template_calls', [])]
            if dDialog.get('child_factories'):
                aTemplates += [f'0x{dCall["resource_id"]:x}'
                               for dChild in dDialog['child_factories']
                               for dCall in dChild['template_calls']]
            row([dFile['name'], format_id(dDialog['id']), format_id(dDialog['create_va']),
                 f'0x{dDialog["flags"]:x}', dDialog.get('first_ret_pop', '-'),
                 ', '.join(aTemplates) or 'unresolved'])
    print()
    iTotal = sum(len(dFile['linkage']['dialogs']) for dFile in aFiles if dFile['linkage'])
    print(f'{iTotal} registry entries; '
          f'{sum(len(dFile["templates"]) for dFile in aFiles)} resource template leaves.')
    if aCallers:
        print()
        row(['Caller', 'Call VA', 'Host slot', 'Registry ID'])
        if bMarkdown:
            row(['---'] * 4)
        for dCaller in aCallers:
            for dSite in dCaller['sites']:
                row([dCaller['name'], format_id(dSite['call_va']),
                     f'0x{dSite["host_slot"]:x}', format_id(dSite['dialog_id'])
                     if dSite['dialog_id'] is not None else 'unresolved'])


def print_registry(aFiles):
    print('# Generated by tools/dialogmap.py --format registry; do not edit.')
    print('module\tmodule_id\tindex\tdialog_id\tflags\tfactory_va\ttemplate_ids')
    for dFile in aFiles:
        if not dFile['linkage']:
            continue
        for dDialog in dFile['linkage']['dialogs']:
            aCalls = list(dDialog.get('template_calls', []))
            aCalls += [dCall for dChild in dDialog.get('child_factories', [])
                       for dCall in dChild['template_calls']]
            print('\t'.join([dFile['name'], format_id(dFile['linkage']['id']),
                             str(dDialog['index']), format_id(dDialog['id']),
                             f'0x{dDialog["flags"]:x}', format_id(dDialog['create_va']),
                             ','.join(f'0x{dCall["resource_id"]:x}' for dCall in aCalls) or '-']))


def main():
    pParser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    pParser.add_argument('--orig', type=Path, default=Path('orig'), help='retail reference tree')
    pParser.add_argument('--format', choices=['text', 'json', 'markdown', 'registry'], default='text')
    pParser.add_argument('--no-disassemble', action='store_true', help='do not run objdump')
    pParser.add_argument('--callers', action='store_true',
                         help='also scan host/module/UI code for dialog calls (heuristic)')
    pArgs = pParser.parse_args()
    if pArgs.callers and pArgs.no_disassemble:
        pParser.error('--callers requires disassembly')
    try:
        aPaths = sorted(pPath for pPath in (pArgs.orig / 'English').iterdir()
                        if pPath.suffix.lower() == '.dll')
        if not aPaths:
            raise SurveyError('no English dialog DLLs found')
        aPaths += [pArgs.orig / szName for szName in ('SIMSUI.DLL', 'DIALOG.DLL', 'ENGLISH.FLL')]
        aPaths += sorted(pPath for pPath in (pArgs.orig / 'Modules').glob('*')
                         if pPath.name.upper() == 'MPCHAT.DLL')
        aFiles = [survey_file(pPath, not pArgs.no_disassemble) for pPath in aPaths]
        dNames = module_names(pArgs.orig)
        for dFile in aFiles:
            if dFile['linkage']:
                dNames[dFile['linkage']['id']] = Path(dFile['name']).stem.upper()
        dOwners = {}
        for dFile in aFiles:
            if not dFile['linkage']:
                continue
            for dDialog in dFile['linkage']['dialogs']:
                if dDialog['id'] in dOwners:
                    raise SurveyError(f'duplicate registry id {dDialog["id"]:#x}')
                dOwners[dDialog['id']] = dFile['name']
        aCallers = []
        if pArgs.callers:
            aCallerPaths = sorted(set(aPaths + [pArgs.orig / 'FLTSIM98.EXE'] +
                                     [pPath for pPath in (pArgs.orig / 'Modules').glob('*')
                                      if pPath.suffix.lower() == '.dll']))
            aCallers = [scan_callers(pPath) for pPath in aCallerPaths]
        if pArgs.format == 'json':
            print(json.dumps({'schema_version': 1, 'files': aFiles,
                              'module_names': dNames, 'callers': aCallers},
                             indent=2, sort_keys=True))
        elif pArgs.format == 'registry':
            print_registry(aFiles)
        else:
            print_report(aFiles, dNames, pArgs.format == 'markdown', aCallers)
    except (OSError, ValueError, struct.error, subprocess.CalledProcessError) as pError:
        print(f'dialogmap: {pError}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
