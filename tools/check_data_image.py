#!/usr/bin/env python3
"""Compare a module's .data image with retail's, byte for byte (Story 1.4).

Usage: tools/check_data_image.py [--kept] [--rdata RVA:LEN] <retail.dll> <ours.dll>
       tools/check_data_image.py --hash <MODULE> <ours.dll> [<MODULE> <ours.dll> ...] [--manifest FILE]
       tools/check_data_image.py --gauge-pairs GAUGES_MANIFEST.json [--manifest FILE]
       tools/check_data_image.py --orig DIR --manifest FILE [--update] [--self-test]
Retail's .data (virtual size, zero past the raw data) has each of its own
relocation targets zeroed (ours installs those pointers at load).  Ours is read
for the same number of bytes, starting at our ImportTable export minus retail's
ImportTable offset into its .data (both DLLs must export it, inside .data), with
our own relocation targets zeroed too (statically initialised code pointers);
each must also be one of retail's, else exit 1.  Exit 0 if identical.
--linked (manifest flag, with at=; the MDLs): the image's relocations are retail's exactly, at the same offsets: a data target must be the
same place relative to the image start (retail's .data, our at= symbol), a code target must lie in retail's .text and in our .text.  --hash
also hashes the data targets (relative) and which slots are code.  at= may name a symbol in .mdlimg.
--kept (ATC, NUMERIC, UTIL): every retail relocation target that is not one of ours must
hold retail's raw, unzeroed 4 bytes (a code address kept as a value); a zeroed
or changed one fails.  The output counts them as "K kept".
--mixed (manifest flag; HG2D, G3D): each retail relocation target that is not one of ours must be retail's raw value, or zero and
listed in ptrs= (given the usual value check), or zero and listed in cset=; anything else fails.  Not with --kept.  The output counts
the raw ones as "K kept" and the cset= ones as "S set in C".
cset=LIST (manifest field, only with --mixed): the zero slots the module sets in C: comma-separated offsets, and A..B for every 4th
offset from A to B inclusive.  Each must be a slot retail relocates that is not ours, not in ptrs=, and zero in ours.  --hash hashes the
parsed list too.
ptrs= may also hold G3D_DATA_POINTER / MAIN_DATA_POINTER(at, target) (image start + target) and G3D_CODE_TARGET / MAIN_CODE_TARGET(address)
lines: under --mixed the set of code-target addresses must equal the distinct raw values of the kept slots, and a repeated line is an error.
MAIN_LINKAGE_RETAIL(at, value) lines (MAIN only) must be exactly 51, at ascending offsets 0xc0..0x188 in steps of 4, with no duplicates,
and each value must equal retail's raw word at that offset; they are metadata, so they never count as listed or installed slots.
tail=SYMBOL:VA:LEN[:SHA256] (manifest field, MAIN's text tail): our COFF symbol's LEN bytes must equal retail's mapped image (.text, the zero
gap and .rdata, as the loader maps them at the preferred base, unrelocated) from VA.  Both windows must lie inside their images (retail's
between .text and .data, ours inside one of our sections) and none of OUR relocations may overlap ours; retail's own relocations over the
window are allowed (it is retail's code, kept as raw bytes).  --hash hashes our window.
--rdata RVA:LEN: retail's .rdata bytes there must appear exactly once in our
.data and .rdata together (MULTIP keeps its copy in .data), with none of our relocations overlapping the match.
--hash: no retail; one or more MODULE DLL pairs (an odd count is a usage error; every failing pair is reported, exit 1 if any
fails).  Checks our image (normalized as above, plus the sorted list
of our relocation offsets in it) and each .rdata range's window against the
sha256s in tools/data_images.txt.  With UPDATE=1 in the environment it warns,
and sets the DLL's mtime to 0 so that the next build reruns the step.
at=SYMBOL (manifest field): our image starts at that COFF symbol of our DLL (NUMERIC's _fs98_numeric_data,
UTIL's _fs98_util_data: each a verbatim copy of retail's whole .data, retail's ImportTable and Linkage bytes included; the
module's live ImportTable and Linkage are separate objects) instead of at our ImportTable export; with at=, the image may lie in .data or .rdata, and off= is 0.
GAUGE_REBIND(off, rva) lines (a whole gauge's .data, src/gauges/images/<NAME>/rebind.inc): slots count from .data's start, the target is an
RVA (target base zero) that must lie in retail's .text, .data or .rdata, and the lines must be retail's relocations in retail's order.
ptrs=PATH[:SHA256] (manifest field): the pointer list a module installs at attach (FACIL_REL_*, FE_REL_*,
ADVDRV_CODE/ADVDRV_DATA, PANELS' C/L, or MULTIP's { owner, off, kind, target }).  With orig/, each slot and target must equal retail's relocated
value, and retail's relocations in the image must all be ours, kept or listed.  --hash needs no orig/: it
hashes the parsed entries.
rawptrs=PATH[:SHA256] (manifest field, HOST): an image without relocations (FLTSIM98.EXE keeps retail's absolute addresses raw and
rebases them at start-up) and a file whose HOST_RT_CODE_POINTER(site, target) and HOST_RT_DATA_POINTER(site, target) rows must equal
exactly the set retail gives: every aligned word of its .rdata and .data (raw size) whose value lies in .text (a code row) or in
base+0x16000..base+0x1f000 (a data row), each target being that word minus the image base; a repeated, missing, extra or mistargeted
row fails.  Neither image may have relocations.  --hash hashes the parsed rows.
bound=PATH[:SHA256] (manifest field, SIMSUI; with at=): a pointer image bound at load (SIMSUI.DLL's .rdata and .data from RVA 0x15000 hold
retail's raw virtual addresses and bind_image() rewrites each at attach from src/simsui/simsui_native_tables.inc).  Retail's window is
mapped section by section (.rdata, the gap, .data with zero past its raw data, up to 0x21400), unrelocated and with nothing zeroed; our
symbol's bytes must equal it, with none of our relocations inside.  PATH is parsed strictly (the two #defines, which must equal
0x15000 and 0x21400, SIMSUI_RELOC_DATA/CODE/IMPORT/NONE rows, and comments); its sites must be unique, ascending and equal retail's
relocations in the window, and each row's kind and target must equal what the generator's classifier (tools/gen_simsui_native.py)
gives for retail's raw word: DATA when it lies in the window plus 0x10; else in .text, an IMPORT when a jmp [slot] thunk of an import
(simsui_tables.inc), a CODE when the harness's operation table (tools/difftest/simsui_operations.inc) has it, a NONE when it is a
funclet (0x13560 and up, or one of four).  sha256= hashes the window; the bound= hash covers the bounds and the rows in order.
switch=IDX_RVA:LEN:JT_RVA:T=A,...[:SHA256] (manifest field): a table rebuilt from retail's compressed
switch (byte table, jump table, target T's action A); it must appear once in ours.  --hash hashes the window.
obj=SYMBOL, extent=prefix:SIZE[:zero]|suffix:OFF (manifest fields; obj= on every row): the image window W = [start, start+len) must be exactly one C
object of ours, whose real extent is read from the linked DLL: the COFF symbol _SYMBOL and its local alias _SYMBOL_end (= _SYMBOL + sizeof,
emitted by FS98_IMAGE_END in include/fs98/image_end.h, once per image object, in the defining TU).  Both must resolve once, in one section,
the start inside the section and the end at or before its upper bound (max of vsize and rawsize; the boundary itself is allowed), the end
after the start; uniqueness counts every primary record of the name in the table.  No extent=: start == _SYMBOL and start + len == _SYMBOL_end.  extent=prefix:SIZE
(ATC, NUMERIC: a declared part of a larger object; also the three rows whose objects are rounded up, DEMO, MULTIP and WEATHERM): start == _SYMBOL,
start + len <= _SYMBOL_end and _SYMBOL_end - _SYMBOL == SIZE, the object's pinned size.  With :zero (the rounded rows) the slack from start + len to
the end must be zero bytes with no relocation in them.  extent=suffix:OFF (the MDLs): start ==
_SYMBOL + OFF and start + len == _SYMBOL_end.  The check runs in the retail comparison (--orig and --manifest), in --hash (single and batch), and
for every --linked row; --gauge-pairs checks at configure time that the gauge rows are obj=gauge_data with no extent=.  tail= also needs
len == _SYMBOL_end - _SYMBOL of its symbol; bound= needs start == _SYMBOL and len == BOUND_END - BOUND_START == _SYMBOL_end - _SYMBOL.  A
missing symbol or a failed equation fails, naming the row, the symbol and the numbers, and is a hard error under UPDATE=1 (--hash does
not downgrade it to a warning; --orig never rewrites len= of a row whose extent failed).  UPDATE=1 never writes obj= or extent=: they are
edited by hand.  Out of scope: the two-file command line (<retail.dll> <ours.dll>), which has no object metadata and keeps its behaviour,
and rdata= and switch=, which are content searches (already bounded to a section, unique and relocation-checked).
--manifest with --orig: every row against retail.  UPDATE=1 (or --update)
rewrites the hashes of the rows that pass.  --self-test: see self_test().
"""
import hashlib, os, re, struct, subprocess, sys, tempfile, shutil

def load(path):
    b = open(path, "rb").read()
    try:
        pe = struct.unpack_from("<I", b, 0x3C)[0]
        if b[pe:pe + 4] != b"PE\0\0":
            raise ValueError("no PE signature")
        n = struct.unpack_from("<H", b, pe + 6)[0]
        opt = pe + 24
        base = struct.unpack_from("<I", b, opt + 28)[0]
        dirs = opt + 96
        osz = struct.unpack_from("<H", b, pe + 20)[0]
        s = opt + osz
        if osz < 96 + 16 * 8 or s + 40 * n > len(b):
            raise ValueError("the optional header or the section table runs past the end of the file")
        secs = {}
        for i in range(n):
            o = s + 40 * i
            name = b[o:o + 8].rstrip(b"\0").decode()
            secs[name] = struct.unpack_from("<IIII", b, o + 8)  # vsize, rva, rawsize, raw
    except (struct.error, ValueError, UnicodeDecodeError) as e:
        raise SystemExit("%s: malformed PE: %s" % (path, e))
    return b, base, dirs, secs

def read_rva(b, secs, rva, size):
    for vs, va, rs, raw in secs.values():
        if va <= rva < va + max(vs, rs):
            off = rva - va
            data = b[raw + off: raw + min(rs, off + size)] if off < rs else b""
            return data + bytes(size - len(data))
    raise SystemExit("rva 0x%x is in no section" % rva)

def targets(b, dirs, secs):
    """RVAs of every HIGHLOW relocation."""
    rrva, rsize = struct.unpack_from("<II", b, dirs + 5 * 8)
    out = []
    if not rrva:
        return out
    reloc = read_rva(b, secs, rrva, rsize)
    cur = 0
    while cur + 8 <= len(reloc):
        page, blk = struct.unpack_from("<II", reloc, cur)
        if blk == 0:
            break
        for e in range(8, blk, 2):
            ent = struct.unpack_from("<H", reloc, cur + e)[0]
            if ent >> 12 == 3:
                out.append(page + (ent & 0xFFF))
            elif ent >> 12:
                raise SystemExit("unexpected relocation type")
        cur += blk
    return out

def export(ob, odirs, os_):
    erva = struct.unpack_from("<I", ob, odirs)[0]
    if not erva:
        return None
    ex = read_rva(ob, os_, erva, 40)
    nnames, faddr, naddr, nord = struct.unpack_from("<IIII", ex, 24)
    names = read_rva(ob, os_, naddr, 4 * nnames)
    ords = read_rva(ob, os_, nord, 2 * nnames)
    funcs = read_rva(ob, os_, faddr, 4 * struct.unpack_from("<I", ex, 20)[0])
    for i in range(nnames):
        nrva = struct.unpack_from("<I", names, 4 * i)[0]
        if read_rva(ob, os_, nrva, 12) == b"ImportTable\0":
            return struct.unpack_from("<I", funcs, 4 * struct.unpack_from("<H", ords, 2 * i)[0])[0]
    return None


def hexint(x):
    return int(x, 0)

def sec_headers(b):
    """[(vsize, rva, rawsize, rawptr)] of the PE's section headers in file order (the numbering the COFF symbols use; names may repeat)."""
    pe = struct.unpack_from("<I", b, 0x3C)[0]
    n = struct.unpack_from("<H", b, pe + 6)[0]
    s0 = pe + 24 + struct.unpack_from("<H", b, pe + 20)[0]
    return [struct.unpack_from("<IIII", b, s0 + 40 * i + 8) for i in range(n)]

def coff_symbol_in(b, secs, name):
    """(RVA, section number, None) of the COFF symbol `name` in a PE image, or (None, None, Structural message): no symbol table, a malformed
    one, no such symbol, more than one primary record of that name anywhere in the table, or one not in a section.  Needs the DLL's COFF symbol
    table, so a module linked with -s / --strip-all cannot use at=."""
    try:
        pe = struct.unpack_from("<I", b, 0x3C)[0]
        ptr, n = struct.unpack_from("<II", b, pe + 12)
        if not ptr:
            return None, None, Structural("no COFF symbol table in our DLL (at=; was it stripped?)")
        strtab = ptr + 18 * n
        if strtab + 4 > len(b):
            raise ValueError("the symbol table runs past the end of the file")
        strsize = struct.unpack_from("<I", b, strtab)[0]
        if strsize < 4 or strtab + strsize > len(b):
            raise ValueError("the string table runs past the end of the file")
        hdrs = sec_headers(b)
        found = []
        i = 0
        while i < n:
            o = ptr + 18 * i
            raw = b[o:o + 8]
            if raw[:4] == bytes(4):
                off = struct.unpack_from("<I", raw, 4)[0]
                stop = b.find(b"\0", strtab + off, strtab + strsize) if 4 <= off < strsize else -1
                if stop < 0:
                    raise ValueError("a symbol name lies outside the string table")
                nm = b[strtab + off:stop]
            else:
                nm = raw.rstrip(b"\0")
            naux = b[o + 17]
            if i + 1 + naux > n:
                raise ValueError("aux records run past the symbol table")
            if nm == name.encode():
                found.append(struct.unpack_from("<Ih", b, o + 8))
            i += 1 + naux
    except (struct.error, ValueError, IndexError) as e:
        return None, None, Structural("malformed COFF symbol table in our DLL: %s" % e)
    if len(found) != 1:
        return None, None, Structural("no symbol %s in our DLL (at=)" % name if not found else "%d symbols named %s in our DLL (at=)" % (len(found), name))
    value, sec = found[0]
    if not 1 <= sec <= len(hdrs):
        return None, None, Structural("symbol %s in our DLL is not in a section (section number %d) (at=)" % (name, sec))
    vs, rva, rs, _ = hdrs[sec - 1]
    if value > max(vs, rs):  # the value is section-relative: an address outside its declared section is no anchor (the section's end itself is allowed)
        return None, None, Structural("symbol %s in our DLL lies outside its declared section %d (value 0x%x, section size 0x%x) (at=)" % (name, sec, value, max(vs, rs)))
    return rva + value, sec, None

def coff_symbol(b, secs, name):
    """(RVA, None) of the COFF symbol `name`, or (None, message); see coff_symbol_in."""
    rva, _, err = coff_symbol_in(b, secs, name)
    return rva, err

def retag(err, old, new):
    return Structural(err.replace(old, new))

def section_bounds(ob, sec, lo, hi=None):
    """An error string unless lo (and hi, if given) lie in section number sec: lo inside it, hi at or before its upper bound (max of vsize and rawsize;
    the boundary itself is allowed), as our_image bounds a window."""
    vs, rva, rs, _ = sec_headers(ob)[sec - 1]
    top = rva + max(vs, rs)
    if not rva <= lo < top or (hi is not None and not lo < hi <= top):
        return "RVA 0x%x..%s is not inside section %d (RVA 0x%x..0x%x)" % (lo, "0x%x" % hi if hi is not None else "?", sec, rva, top)
    return None

def object_symbols(ob, os_, who, obj):
    """(start, end, section, error): the object's two symbols resolve once, in one section, the end after the start, both inside the section."""
    lo, slo, err = coff_symbol_in(ob, os_, "_" + obj)
    if err:
        return None, None, None, retag("%s: %s" % (who, err), "(at=)", "(obj=)")
    hi, shi, err = coff_symbol_in(ob, os_, "_%s_end" % obj)
    if err:
        return None, None, None, (Structural("%s: no end symbol _%s_end in our DLL (FS98_IMAGE_END(%s) missing or dropped?)" % (who, obj, obj)) if err.startswith("no symbol")
                                  else retag("%s: %s" % (who, err), "(at=)", "(obj=)"))
    if slo != shi:
        return None, None, None, Structural("%s: _%s is in section %d but _%s_end in section %d" % (who, obj, slo, obj, shi))
    if hi <= lo:
        return None, None, None, Structural("%s: _%s_end (RVA 0x%x) is not after _%s (RVA 0x%x)" % (who, obj, hi, obj, lo))
    err = section_bounds(ob, slo, lo, hi)
    if err:
        return None, None, None, Structural("%s: _%s..._%s_end %s" % (who, obj, obj, err))
    return lo, hi, slo, None

EXTENT_PREFIX = re.compile(r"prefix:(0x[0-9a-fA-F]+)(:zero)?")

def extent_problem(ob, odirs, os_, ours, row, start, ln):
    """An error (Structural) unless the window [start, start+ln) is the row's whole image object, or the prefix or suffix the row declares; None
    if it is.  row is (module, obj, extent): obj= names the object's COFF symbol _<obj>, and the FS98_IMAGE_END macro (include/fs98/image_end.h)
    gives it the local symbol _<obj>_end = _<obj> + sizeof.  Both must resolve once, in one section, the start inside it and the end at or
    before its upper bound, the end after the start.  extent: None (whole), ("prefix", size, zero) or ("suffix", off); a prefix row pins the
    object's size, and with zero its slack past the window must be zero bytes with no relocation in them."""
    module, obj, extent = row
    who = "%s (%s): obj=%s" % (ours, module, obj)
    lo, hi, slo, err = object_symbols(ob, os_, who, obj)
    if err:
        return err
    kind = extent[0] if extent else "whole"
    off = extent[1] if kind == "suffix" else 0
    if kind == "prefix" and hi - lo != extent[1]:
        return Structural("%s: _%s is 0x%x bytes (RVA 0x%x..0x%x), the row declares the object size 0x%x" % (who, obj, hi - lo, lo, hi, extent[1]))
    if start != lo + off or (start + ln > hi if kind == "prefix" else start + ln != hi):
        return Structural("%s: image window RVA 0x%x..0x%x (len 0x%x) is not %s of _%s, RVA 0x%x..0x%x (size 0x%x)"
                          % (who, start, start + ln, ln, {"whole": "the whole object", "prefix": "a prefix", "suffix": "the suffix at +0x%x" % off}[kind], obj, lo, hi, hi - lo))
    if kind == "prefix" and extent[2] and start + ln < hi:
        if any(read_rva(ob, os_, start + ln, hi - start - ln)):
            return Structural("%s: the slack of _%s past the window (RVA 0x%x..0x%x) is not all zero" % (who, obj, start + ln, hi))
        for r in targets(ob, odirs, os_):
            if r + 4 > start + ln and r < hi:
                return Structural("%s: our relocation at RVA 0x%x is in the slack of _%s (RVA 0x%x..0x%x)" % (who, r, obj, start + ln, hi))
    return None

def our_image(ob, odirs, os_, ours, off, vs, at=None):
    """Ours with our relocation targets zeroed, and the sorted relocation offsets in it."""
    if at:
        start, asec, err = coff_symbol_in(ob, os_, at)  # the DLL must keep its COFF symbol table (no -s / --strip-all)
        if err:
            return None, None, Structural("%s: %s" % (ours, err))
        if section_bounds(ob, asec, start, start + vs):
            return None, None, Structural("%s: at=%s: the window 0x%x..0x%x is not inside the anchor's declared section" % (ours, at, start, start + vs))
    else:
        start = export(ob, odirs, os_)
        if start is None:
            raise SystemExit("ours has no ImportTable export")
        start -= off
    names = [n for n in ((".data", ".rdata", ".mdlimg") if at else (".data",)) if n in os_]
    homes = [os_[n] for n in names]
    if not any(h[1] <= start and start + vs <= h[1] + max(h[0], h[2]) for h in homes):
        raise SystemExit("%s: our image (0x%x..0x%x) is not inside our %s"
                         % (ours, start, start + vs, " or ".join("%s (0x%x..0x%x)" % (n, h[1], h[1] + max(h[0], h[2]))
                                                                for n, h in zip(names, homes))))
    mine = bytearray(read_rva(ob, os_, start, vs))
    offs = []
    for r in targets(ob, odirs, os_):
        t = r - start
        if -4 < t < 0 or 0 <= t < vs and t + 4 > vs:
            return start, None, "%s: our relocation at +0x%x straddles an edge of the image" % (ours, t)
        if 0 <= t < vs:
            offs.append(t)
    return start, mine, offs

RDATA_HOMES = (".data", ".rdata")  # a copy of retail's .rdata may live in our .data (MULTIP's s_multip_rdata)

def home_bytes(ob, os_):
    """(rva, bytes) of each of our initialised data sections that holds a .rdata range."""
    out = [(os_[n][1], read_rva(ob, os_, os_[n][1], os_[n][0])) for n in RDATA_HOMES if n in os_]
    if not out:
        raise SystemExit("ours has no .data or .rdata")
    return out

def rdata_matches(ob, os_, pat):
    """RVAs in our .data and .rdata where pat appears."""
    out = []
    for rva, rd in home_bytes(ob, os_):
        i = rd.find(pat)
        while i >= 0:
            out.append(rva + i)
            i = rd.find(pat, i + 1)
    return out

def rdata_windows(ob, os_, ln, sha):
    """RVAs of the length-ln windows in our .data and .rdata whose sha256 is sha."""
    return [rva + i for rva, rd in home_bytes(ob, os_) for i in range(len(rd) - ln + 1)
            if hashlib.sha256(rd[i:i + ln]).hexdigest() == sha]

def rdata_overlap(ob, odirs, os_, ours, m, ln):
    """An error string if one of our relocations overlaps our match at RVA m, else None."""
    for r in targets(ob, odirs, os_):
        if r + 4 > m and r < m + ln:
            return "%s: our relocation at RVA 0x%x overlaps the .rdata match at RVA 0x%x" % (ours, r, m)

def rdata_check(rb, rs, ob, odirs, os_, ours, rva, ln):
    """Retail's range must be in our .rdata exactly once, with none of our relocations over it."""
    pat = read_rva(rb, rs, rva, ln)
    ms = rdata_matches(ob, os_, pat)
    if len(ms) != 1:
        return None, "%s: .rdata 0x%x:0x%x found %d times in ours (.data and .rdata), not once" % (ours, rva, ln, len(ms))
    err = rdata_overlap(ob, odirs, os_, ours, ms[0], ln)
    if err:
        return None, err
    return (ms[0], hashlib.sha256(pat).hexdigest()), None

PTR_RES = [re.compile(r"^\s*(FACIL_REL_DATA|FACIL_REL_TEXT|FE_REL_DATA|FE_REL_TEXT|ADVDRV_CODE|ADVDRV_DATA|HG2D_DATA_POINTER|G3D_DATA_POINTER|MAIN_DATA_POINTER|MAIN_LINKAGE_RETAIL|C)"
                      r"\((0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)\s*$"),
           re.compile(r"^\s*\{\s*(\d+),\s*(0x[0-9a-fA-F]+),\s*(\d+),\s*(0x[0-9a-fA-F]+)\s*\},?\s*$"),
           re.compile(r"^\s*L\((0x[0-9a-fA-F]+)\)\s*$"),
           re.compile(r"^\s*((?:G3D|MAIN)_CODE_TARGET)\((0x[0-9a-fA-F]+)\)\s*$"),
           re.compile(r"^\s*GAUGE_DATA\((0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)\s*$"),
           re.compile(r"^\s*GAUGE_CODE\((0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*[A-Za-z_]\w*\)\s*$"),
           re.compile(r"^\s*GAUGE_REBIND\((0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)\s*$")]
GLOBAL_LINK = 0xc  # PANELS' L(at): the slot holds image start + 0xc, the GLOBAL link

def parse_ptrs(path):
    """(entries, error): (form, off, target) or (owner, off, kind, target) tuples in source order."""
    try:
        text = open(path).read()
    except OSError as e:
        return None, "%s: %s" % (path, e.strerror)
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    out = []
    targets_seen = set()
    for n, line in enumerate(text.split("\n"), 1):
        if not line.strip():
            continue
        m = PTR_RES[3].match(line)
        if m:
            v = hexint(m.group(2))
            if v in targets_seen:
                return None, "%s:%d: duplicate %s 0x%x" % (path, n, m.group(1), v)
            targets_seen.add(v)
            out.append((m.group(1), v))
            continue
        m = PTR_RES[0].match(line)
        if m:
            out.append((m.group(1), hexint(m.group(2)), hexint(m.group(3))))
            continue
        for form, rx in (("GAUGE_DATA", PTR_RES[4]), ("GAUGE_CODE", PTR_RES[5]), ("GAUGE_REBIND", PTR_RES[6])):  # the routine of a GAUGE_CODE is not checked
            m = rx.match(line)
            if m:
                out.append((form, hexint(m.group(1)), hexint(m.group(2))))
                break
        if m:
            continue
        m = PTR_RES[2].match(line)
        if m:
            out.append(("L", hexint(m.group(1)), GLOBAL_LINK))
            continue
        m = PTR_RES[1].match(line)
        if not m:
            return None, "%s:%d: not a pointer entry: %s" % (path, n, line.strip())
        out.append((int(m.group(1)), hexint(m.group(2)), int(m.group(3)), hexint(m.group(4))))
    return out, None

def ptrs_hash(entries):
    return hashlib.sha256("\n".join(",".join(str(x) for x in e) for e in entries).encode()).hexdigest()

RAWPTR_RE = re.compile(r"^\s*HOST_RT_(CODE|DATA)_POINTER\((0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)\s*$")
RAWPTR_DATA = (0x16000, 0x1f000)  # RVAs: .rdata through .idata, the data a raw pointer may name

def parse_rawptrs(path):
    """(rows, error): ("CODE"|"DATA", site, target) in source order.  Every other line of the file (the other HOST_RT_* tables, #ifdef and
    comments) belongs to tools/ofs host-rt-tables-check and is skipped."""
    try:
        text = open(path).read()
    except OSError as e:
        return None, "%s: %s" % (path, e.strerror)
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    rows = []
    for n, line in enumerate(text.split("\n"), 1):
        m = RAWPTR_RE.match(line)
        if m:
            rows.append((m.group(1), hexint(m.group(2)), hexint(m.group(3))))
        elif re.search(r"HOST_RT_(CODE|DATA)_POINTER", line) and not re.match(r"^\s*#\s*(ifdef|define|undef)\b", line):
            return None, "%s:%d: not a plain HOST_RT_CODE/DATA_POINTER(0x.., 0x..) row: %s" % (path, n, line.strip())
    return rows, None

def rawptrs_hash(rows, path):
    """The parsed pointer rows plus the sha256 of the file's text after its leading header comment, so that an edit to any other row
    (imports, functions, services, thunks, direct) fails --hash too."""
    text = open(path).read()
    body = text[text.index("*/") + 2:] if text.startswith("/*") else text
    return hashlib.sha256(("\n".join(",".join(str(x) for x in r) for r in rows) + "\n" + hashlib.sha256(body.encode()).hexdigest()).encode()).hexdigest()

def rawptrs_check(path, rb, base, rs):
    """The rows of path against the set the generator's rule gives on retail; (rows, error)."""
    rows, err = parse_rawptrs(path)
    if err:
        return None, err
    tv, tva = rs[".text"][:2]
    want = {}
    for name in (".rdata", ".data"):
        vs, rva, rsz, raw = rs[name]
        for site in range(rva, rva + rsz - 3, 4):
            v = struct.unpack_from("<I", rb, raw + site - rva)[0]
            if base + tva <= v < base + tva + tv:
                want[site] = ("CODE", site, v - base)
            elif base + RAWPTR_DATA[0] <= v < base + RAWPTR_DATA[1]:
                want[site] = ("DATA", site, v - base)
    seen = set()
    for kind, site, tgt in rows:
        if site in seen:
            return None, "%s: HOST_RT_%s_POINTER at 0x%x is repeated" % (path, kind, site)
        seen.add(site)
        if site not in want:
            return None, "%s: HOST_RT_%s_POINTER at 0x%x is not a pointer word of retail's .rdata or .data" % (path, kind, site)
        if (kind, site, tgt) != want[site]:
            return None, "%s: HOST_RT_%s_POINTER at 0x%x targets 0x%x, retail's word gives HOST_RT_%s_POINTER 0x%x" % (path, kind, site, tgt, want[site][0], want[site][2])
    for site in sorted(set(want) - seen):
        return None, "%s: retail's pointer word at 0x%x (HOST_RT_%s_POINTER) is not listed" % (path, site, want[site][0])
    return rows, None

BOUND_START, BOUND_END, BOUND_BASE = 0x15000, 0x21400, 0x20470000
BOUND_FUNCLETS = (0x131f3, 0x13270, 0x1327d, 0x1337f)  # the unwind funclets below 0x13560 (tools/gen_simsui_native.py)
BOUND_ROW = re.compile(r"^SIMSUI_RELOC_(DATA|CODE|IMPORT|NONE)\((0x[0-9a-f]+)(?:, (0x[0-9a-f]+|\w+))?\)$")
BOUND_DEFINE = re.compile(r"^#define (SIMSUI_IMAGE_START|SIMSUI_IMAGE_END) (0x[0-9a-f]+)$")
BOUND_OPS, BOUND_TABLES = "tools/difftest/simsui_operations.inc", "src/simsui/simsui_tables.inc"

def parse_bound(path):
    """((start, end), rows, error): rows are (kind, site, arg) in source order; arg is a target, a routine name, an import index or None."""
    try:
        text = open(path).read()
    except OSError as e:
        return None, None, "%s: %s" % (path, e.strerror)
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    bounds, rows = {}, []
    for n, line in enumerate(text.split("\n"), 1):
        line = line.strip()
        if not line:
            continue
        d, m = BOUND_DEFINE.match(line), BOUND_ROW.match(line)
        if d and d.group(1) not in bounds:
            bounds[d.group(1)] = hexint(d.group(2))
        elif m and (m.group(3) is None) == (m.group(1) == "NONE") and (m.group(1) != "DATA" or m.group(3).startswith("0x")) and (m.group(1) != "IMPORT" or re.fullmatch(r"0|[1-9][0-9]*", m.group(3))):
            kind, arg = m.group(1), m.group(3)
            rows.append((kind, hexint(m.group(2)), hexint(arg) if kind == "DATA" else int(arg) if kind == "IMPORT" else arg))
        else:
            return None, None, "%s:%d: not a #define of the bounds or a plain SIMSUI_RELOC_ row: %s" % (path, n, line)
    if bounds != {"SIMSUI_IMAGE_START": BOUND_START, "SIMSUI_IMAGE_END": BOUND_END}:
        return None, None, "%s: the bounds must be SIMSUI_IMAGE_START 0x%x and SIMSUI_IMAGE_END 0x%x, got %s" % (path, BOUND_START, BOUND_END, {k: hex(v) for k, v in bounds.items()})
    return (BOUND_START, BOUND_END), rows, None

def bound_hash(bounds, rows):
    return hashlib.sha256(("0x%x 0x%x\n" % bounds + "\n".join("%s,0x%x,%s" % (k, s, a) for k, s, a in rows)).encode()).hexdigest()

def bound_window(rb, base, rs):
    """Retail's mapped window, as tools/gen_simsui_native.py maps it: (bytes, error)."""
    if base != BOUND_BASE:
        return None, "image base moved (0x%x, not 0x%x)" % (base, BOUND_BASE)
    vs, rva, _, _ = rs[".data"]
    if rs[".rdata"][1] != BOUND_START or ((rva + vs + 15) & ~15) != BOUND_END:
        return None, "retail's .rdata or .data moved: the image is not 0x%x..0x%x" % (BOUND_START, BOUND_END)
    img = bytearray(BOUND_END - BOUND_START)
    for name in (".rdata", ".data"):
        vs, rva, rsz, raw = rs[name]
        n = min(rsz, vs, BOUND_END - rva)
        img[rva - BOUND_START:rva - BOUND_START + n] = rb[raw:raw + n]
    return bytes(img), None

def bound_check(path, rb, base, rs, rdirs, window):
    """The rows of path against retail's relocations in the window and the generator's classifier; (rows, counts, error)."""
    bounds, rows, err = parse_bound(path)
    if err:
        return None, None, err
    sites = sorted(r for r in targets(rb, rdirs, rs) if BOUND_START <= r < BOUND_END)
    if any(r + 4 > BOUND_END for r in sites):
        return None, None, "retail's relocation at 0x%x straddles the end of the window" % [r for r in sites if r + 4 > BOUND_END][0]
    seen = set()
    for kind, site, _ in rows:
        if site in seen:
            return None, None, "%s: SIMSUI_RELOC_%s at 0x%x is repeated" % (path, kind, site)
        seen.add(site)
    if [r[1] for r in rows] != sorted(seen) or not seen <= set(sites):
        bad = [r[1] for r in rows if r[1] not in sites]
        return None, None, ("%s: SIMSUI_RELOC_ row at 0x%x is not one of retail's relocations in the window" % (path, bad[0]) if bad
                            else "%s: the rows are not in ascending order of site" % path)
    for site in sites:
        if site not in seen:
            return None, None, "%s: retail's relocation at 0x%x is not listed" % (path, site)
    ops, op_rvas = {}, {}
    for rva, name in re.findall(r"OPV?\(0x([0-9a-f]+), (\w+)", open(BOUND_OPS).read()):
        ops.setdefault(int(rva, 16), "fs98_simsui_" + name)
        op_rvas.setdefault("fs98_simsui_" + name, set()).add(int(rva, 16))
    imports = {int(slot, 16): int(i) for i, slot in re.findall(r'SIMSUI_IMPORT\((\d+), \w+, "[^"]+", "[^"]+", (0x[0-9a-f]+),', open(BOUND_TABLES).read())}
    tv, trva = rs[".text"][:2]
    counts = {"DATA": 0, "CODE": 0, "IMPORT": 0, "NONE": 0}
    for kind, site, arg in rows:
        target = struct.unpack_from("<I", window, site - BOUND_START)[0] - base
        why = "SIMSUI_RELOC_%s at 0x%x (retail's word 0x%x)" % (kind, site, target + base)
        if BOUND_START <= target < BOUND_END + 0x10:
            want = "DATA"
            if (kind, arg) != (want, target):
                return None, None, "%s: %s: retail's word is a pointer into the image, SIMSUI_RELOC_DATA 0x%x" % (path, why, target)
        elif trva <= target < trva + tv:
            thunk = read_rva(rb, rs, target, 6)
            index = imports.get(struct.unpack_from("<I", thunk, 2)[0] - base) if thunk[:2] == b"\xff\x25" else None
            if index is not None:
                if (kind, arg) != ("IMPORT", index):
                    return None, None, "%s: %s: retail's word is the thunk of import %d, SIMSUI_RELOC_IMPORT(0x%x, %d)" % (path, why, index, site, index)
            elif target in ops:
                if kind != "CODE" or target not in op_rvas.get(arg, ()):
                    return None, None, "%s: %s: retail's word is routine 0x%x, which the harness knows as %s" % (path, why, target, ops[target])
            elif target >= 0x13560 or target in BOUND_FUNCLETS:
                if kind != "NONE":
                    return None, None, "%s: %s: retail's word is a funclet, SIMSUI_RELOC_NONE(0x%x)" % (path, why, site)
            else:
                return None, None, "%s: %s: retail's word is routine 0x%x, which the harness does not verify" % (path, why, target)
        else:
            return None, None, "%s: %s: retail's word is outside the image and .text" % (path, why)
        counts[kind] += 1
    return rows, counts, None

def compare_bound(rb, base, rdirs, rs, ours, at, bound, ext=None):
    """compare() for a bound= row: our symbol's window against retail's unrelocated one, then the table; (rc, info)."""
    window, err = bound_window(rb, base, rs)
    ob, _, odirs, os_ = load(ours)
    if not err and not at:
        err = "%s: bound= needs at=" % ours
    if not err:
        start, mine, offs = our_image(ob, odirs, os_, ours, 0, len(window), at)
        err = offs if mine is None else "%s: our relocation at +0x%x is inside the bound= window" % (ours, offs[0]) if offs else None
        if not err and ext:  # the window is the whole object: start == _obj and len == BOUND_END - BOUND_START == _obj_end - _obj
            err = extent_problem(ob, odirs, os_, ours, ext, start, len(window))
    if not err:
        diff = next((i for i in range(len(window)) if window[i] != mine[i]), None)
        if diff is not None:
            err = "%s: first difference at +0x%x (RVA 0x%x): retail 0x%02x, ours 0x%02x" % (ours, diff, BOUND_START + diff, window[diff], mine[diff])
    rows = counts = None
    if not err:
        rows, counts, err = bound_check(bound, rb, base, rs, rdirs, window)
    if err:
        print(err)
        return 1, None
    name = ours.split("/")[-1].split(".")[0].upper()
    print("%s: image 0x%x bytes identical to retail's unrelocated window; bound rows %s: %d DATA, %d CODE, %d IMPORT, %d NONE (%d)"
          % (name, len(window), bound, counts["DATA"], counts["CODE"], counts["IMPORT"], counts["NONE"], len(rows)))
    return 0, dict(name=name, off=0, vs=len(window), relocated=len(rows), hash=image_hash(mine, []), rdata=[], ptrs=None, switch=None, tail=None,
                   rawptrs=None, bound=(bound, bound_hash((BOUND_START, BOUND_END), rows), len(rows)))

def ptrs_check(path, rb, base, rs, va, rset, offs, kept_mode, ours, mixed=False, kept_values=None, rorder=None):
    """Check a pointer list against retail; returns (entries, error).  kept_mode and mixed both relax completeness to: every other slot is kept or (mixed) zero."""
    ents, err = parse_ptrs(path)
    if err:
        return None, err
    seen = set()
    code_targets = set()
    linkage = []
    for e in ents:
        if len(e) == 2:
            code_targets.add(e[1])
            continue
        if e[0] == "MAIN_LINKAGE_RETAIL":
            linkage.append(e[1:])
            continue
        if len(e) == 3:
            form, off, tgt = e
            # ADVDRV_CODE holds a VA, so its target base is minus the image base
            tbase = {"FACIL_REL_DATA": va, "FE_REL_DATA": va, "ADVDRV_DATA": va, "HG2D_DATA_POINTER": va, "G3D_DATA_POINTER": va, "MAIN_DATA_POINTER": va, "L": va, "FACIL_REL_TEXT": 0,
                     "FE_REL_TEXT": 0, "GAUGE_DATA": va, "GAUGE_CODE": 0, "GAUGE_REBIND": 0, "ADVDRV_CODE": -base, "C": rs[".text"][1]}[form]
            if form == "GAUGE_CODE" and not rs[".text"][1] <= tgt < rs[".text"][1] + rs[".text"][0]:
                return None, "%s: slot +0x%x: GAUGE_CODE rva 0x%x is not in retail's .text" % (path, off, tgt)
            if form == "GAUGE_REBIND" and not any(rs[n][1] <= tgt < rs[n][1] + rs[n][0] for n in (".text", ".data", ".rdata")):
                return None, "%s: slot +0x%x: GAUGE_REBIND rva 0x%x is not in retail's .text, .data or .rdata" % (path, off, tgt)
            if form == "GAUGE_DATA" and not 0 <= tgt < rs[".data"][0]:
                return None, "%s: slot +0x%x: GAUGE_DATA target 0x%x is not inside the .data image" % (path, off, tgt)
        else:
            owner, off, kind, tgt = e
            if owner != 0:
                return None, "%s: owner %d is unsupported (only owner 0)" % (path, owner)
            if kind not in (1, 2, 3):
                return None, "%s: slot +0x%x has unknown kind %d" % (path, off, kind)
            tbase = {1: rs[".text"][1], 2: va, 3: rs[".rdata"][1]}[kind]
        if off in seen:
            return None, "%s: duplicate slot +0x%x" % (path, off)
        seen.add(off)
        if off in offs:
            return None, "%s: listed slot +0x%x is one of our own relocations" % (path, off)
        if off not in rset:
            return None, "%s: slot +0x%x is one retail does not relocate" % (path, off)
        want = struct.unpack("<I", read_rva(rb, rs, va + off, 4))[0]
        ours_v = base + tbase + tgt
        if want != ours_v:
            return None, "%s: slot +0x%x: retail 0x%x, ours 0x%x" % (path, off, want, ours_v)
    if not (kept_mode or mixed):
        for t in sorted(rset - offs - seen):
            return None, "%s: retail relocation at +0x%x is not ours, kept or listed" % (ours, t)
    if rorder is not None and any(e[0] == "GAUGE_REBIND" for e in ents):  # GAUGE_REBIND lines are in retail's relocation order
        want = [o for o in rorder if o in rset]
        for got, wanted in zip([e[1] for e in ents], want):
            if got != wanted:
                return None, "%s: slot +0x%x is out of retail's relocation order (+0x%x comes next)" % (path, got, wanted)
    kind = next((e[0].split("_")[0] for e in ents if isinstance(e[0], str) and e[0].split("_")[0] in ("G3D", "MAIN") and e[0] != "L"), None)
    if kind:  # a G3D or MAIN list: an empty target set must fail too
        if kept_values is None:
            return None, "%s: %s_CODE_TARGET needs a --mixed row" % (path, kind)
        for v in sorted(code_targets ^ kept_values):
            return None, "%s: %s_CODE_TARGET 0x%x %s" % (path, kind, v, "is not a raw value of the image's kept slots" if v in code_targets else "is a raw value of the image's kept slots but not listed")
    if kind == "MAIN":
        ats = [a for a, _ in linkage]
        if ats != list(range(0xc0, 0x18c, 4)):
            return None, "%s: MAIN_LINKAGE_RETAIL lines must be exactly 51, at ascending offsets 0xc0..0x188 in steps of 4, none repeated (got %d)" % (path, len(ats))
        for a, v in linkage:
            want = struct.unpack("<I", read_rva(rb, rs, va + a, 4))[0]
            if v != want:
                return None, "%s: MAIN_LINKAGE_RETAIL at +0x%x: retail 0x%x, listed 0x%x" % (path, a, want, v)
    return ents, None

def parse_cset(text):
    """Sorted offsets of a cset= value ("0x5c,0x60,0x70..0xbc"); raises ValueError."""
    out = []
    for part in text.split(","):
        a, dots, b = part.partition("..")
        lo, hi = hexint(a), hexint(b) if dots else hexint(a)
        if hi < lo or (hi - lo) % 4:
            raise ValueError
        out += range(lo, hi + 1, 4)
    if len(set(out)) != len(out):
        raise ValueError
    return sorted(out)

def parse_switch(v):
    """(idx_rva, len, jt_rva, [(target, action)], sha or None)"""
    p = v.split(":")
    if len(p) not in (4, 5):
        raise ValueError
    m = [tuple(int(x, 0) for x in kv.split("=")) for kv in p[3].split(",")]
    if any(len(x) != 2 for x in m):
        raise ValueError
    return (hexint(p[0]), hexint(p[1]), hexint(p[2]), m, p[4] if len(p) > 4 else None)

def switch_table(rb, rs, rt, base, sw):
    """Retail's action table rebuilt from its switch; (bytes, error)."""
    idx, ln, jt, m, _ = sw
    mp = dict(m)
    ib = read_rva(rb, rs, idx, ln)
    acts = []
    for i in range(max(ib) + 1):
        if jt + 4 * i not in rt:
            return None, "switch 0x%x: jump table dword %d (RVA 0x%x) is not a retail relocation" % (idx, i, jt + 4 * i)
        t = struct.unpack("<I", read_rva(rb, rs, jt + 4 * i, 4))[0] - base
        if t not in mp:
            return None, "switch 0x%x: jump table entry %d targets 0x%x, which is not in the map" % (idx, i, t)
        acts.append(mp[t])
    return bytes(acts[b] for b in ib), None

def parse_tail(v):
    """(symbol, VA, length, sha or None) of a tail= value; raises ValueError."""
    p = v.split(":")
    if len(p) not in (3, 4) or not p[0]:
        raise ValueError
    return (p[0], hexint(p[1]), hexint(p[2]), p[3] if len(p) > 3 else None)

def our_tail(ob, odirs, os_, ours, sym, ln):
    """(bytes, None) of our COFF symbol's ln bytes, or (None, Structural error): the window must be positive, inside the symbol's own section, the
    whole object (its _end symbol, same section, ln == end - start) and clear of our relocations."""
    if ln <= 0:
        return None, Structural("%s: tail= length 0x%x is not positive" % (ours, ln))
    start, ssec, err = coff_symbol_in(ob, os_, sym)
    if err:
        return None, retag("%s: %s" % (ours, err), "(at=)", "(tail=)")
    if section_bounds(ob, ssec, start, start + ln):
        return None, Structural("%s: tail= window 0x%x..0x%x is not inside one of our sections" % (ours, start, start + ln))
    end, esec, err = coff_symbol_in(ob, os_, sym + "_end")  # the tail= window is the whole object: its length is the object's size
    if err:
        return None, (Structural("%s: tail= %s: no end symbol %s_end in our DLL (FS98_IMAGE_END missing or dropped?)" % (ours, sym, sym)) if err.startswith("no symbol")
                      else retag("%s: tail= %s: %s" % (ours, sym, err), "(at=)", "(tail=)"))
    if esec != ssec or end - start != ln:
        return None, Structural("%s: tail= %s: len 0x%x is not the object's size: %s_end is RVA 0x%x in section %d, %s RVA 0x%x in section %d (size 0x%x)" % (ours, sym, ln, sym, end, esec, sym, start, ssec, end - start))
    for r in targets(ob, odirs, os_):
        if r + 4 > start and r < start + ln:
            return None, Structural("%s: our relocation at RVA 0x%x overlaps the tail= window at RVA 0x%x" % (ours, r, start))
    return read_rva(ob, os_, start, ln), None

def retail_tail(rb, base, rs, va, ln):
    """Retail's mapped image from VA for ln bytes (.text, the zero gap, .rdata; zero up to .data); (bytes, error).  Retail's own relocations
    over it are allowed: it is retail's code, kept as raw bytes at the preferred base."""
    lo, hi = rs[".text"][1], rs[".data"][1]
    if va - base < lo or va - base + ln > hi:
        return None, "tail= window 0x%x..0x%x is not inside retail's mapped .text..end of .rdata (0x%x..0x%x)" % (va, va + ln, base + lo, base + hi)
    img = bytearray(hi - lo)
    for name in (".text", ".rdata"):
        vs, rva, rsz, raw = rs[name]
        n = min(rsz, vs)
        img[rva - lo:rva - lo + n] = rb[raw:raw + n]
    return bytes(img[va - base - lo:va - base - lo + ln]), None

def link_targets(ob, obase, os_, start, vs, mine, offs, ours):
    """(entries, error) for a --linked image: per relocation (in offset order) of ours, (offset, "data", target relative to the
    image start) or (offset, "code", RVA in our .text); mine is our image before its relocated words are zeroed."""
    text = os_.get(".text")
    out = []
    for t in sorted(offs):
        if offs.count(t) > 1:
            return None, "%s: our relocation at +0x%x is listed more than once (--linked)" % (ours, t)
        rva = struct.unpack_from("<I", mine, t)[0] - obase
        if start <= rva < start + vs:
            out.append((t, "data", rva - start))
        elif text and text[1] <= rva < text[1] + text[0]:
            out.append((t, "code", rva))
        else:
            return None, "%s: our relocation at +0x%x targets RVA 0x%x, in neither the image nor our .text (--linked)" % (ours, t, rva & 0xFFFFFFFF)
    return out, None

def image_hash(mine, offs, cset=(), link=None):
    extra = b"cset" + b"".join(struct.pack("<I", t) for t in cset) if cset else b""
    if link is not None:  # the data targets, relative to the image, and which slots are code
        extra += b"link" + b"".join(struct.pack("<cI", k[:1].encode(), r if k == "data" else 0) for _, k, r in link)
    return hashlib.sha256(bytes(mine) + b"".join(struct.pack("<I", t) for t in sorted(offs)) + extra).hexdigest()

def compare(retail, ours, kept_mode, rdatas=(), ptrs=None, switch=None, at=None, mixed=False, cset=None, tail=None, rawptrs=None, bound=None, linked=False, ext=None):
    """Returns (rc, info); info is None unless rc == 0.  Prints what it always has.  ext is (module, obj, extent) of a manifest row: the
    window must be that image object (extent_problem); None for the two-file CLI, which has no object metadata."""
    rb, base, rdirs, rs = load(retail)
    if bound:
        return compare_bound(rb, base, rdirs, rs, ours, at, bound, ext)
    vs, va, rawsize, raw = rs[".data"]
    img = bytearray(read_rva(rb, rs, va, vs))
    rt = targets(rb, rdirs, rs)
    for r in rt:
        if va <= r < va + vs:
            img[r - va:r - va + 4] = bytes(4)
    ob, obase, odirs, os_ = load(ours)
    if at:
        off = 0
    else:
        rit = export(rb, rdirs, rs)
        if rit is None or not va <= rit < va + vs:
            raise SystemExit("retail has no ImportTable export inside .data")
        off = rit - va
    start, mine, offs = our_image(ob, odirs, os_, ours, off, vs, at)
    if mine is None:
        print(offs)
        return 1, None
    err = extent_problem(ob, odirs, os_, ours, ext, start, vs) if ext else None
    if err:
        print(err)
        return 1, None
    rset = {r - va for r in rt if va <= r < va + vs}
    for t in offs:
        if t not in rset:
            print("%s: our relocation at +0x%x is not one of retail's" % (ours, t))
            return 1, None
    link = None
    if linked:
        link, err = link_targets(ob, obase, os_, start, vs, mine, offs, ours)
        if err:
            print(err)
            return 1, None
    for t in offs:
        mine[t:t + 4] = bytes(4)
    if linked:  # every relocation of retail's in the window is ours, at the same offset, bound to the same kind of place
        for t in sorted(rset - set(offs)):
            print("%s: retail's relocation at +0x%x is not one of ours (--linked)" % (ours, t))
            return 1, None
        for t, kind, target in link:
            raw = struct.unpack("<I", read_rva(rb, rs, va + t, 4))[0] - base
            rkind, rtarget = ("data", raw - va) if va <= raw < va + vs else ("code", raw)
            if rkind == "code" and not (rs[".text"][1] <= raw < rs[".text"][1] + rs[".text"][0]):
                print("%s: retail's relocation at +0x%x targets RVA 0x%x, not in its .text or .data (--linked)" % (ours, t, raw))
                return 1, None
            if (kind, target if kind == "data" else None) != (rkind, rtarget if rkind == "data" else None):
                print("%s: our relocation at +0x%x is a %s pointer%s, retail's a %s pointer%s (--linked)"
                      % (ours, t, kind, " to image+0x%x" % target if kind == "data" else "", rkind, " to image+0x%x" % rtarget if rkind == "data" else ""))
                return 1, None
    ours_n = len(offs)
    hashed = bytes(mine)  # kept slots still raw
    kept = 0
    kept_values = None
    if mixed:
        listed = set()
        if ptrs and not off:
            ents, err = parse_ptrs(ptrs)
            if err:
                print(err)
                return 1, None
            listed = {e[1] for e in ents if len(e) > 2 and e[0] != "MAIN_LINKAGE_RETAIL"}
        csl = parse_cset(cset) if cset else []
        for c in csl:
            why = ("is one retail does not relocate" if c not in rset else "is one of our own relocations" if c in offs
                   else "is also listed in ptrs=" if c in listed else "is not zero in ours" if c + 4 > vs or any(mine[c:c + 4]) else None)
            if why:
                print("%s: cset= slot +0x%x %s" % (ours, c, why))
                return 1, None
        kept_values = set()
        for t in sorted(rset - set(offs)):
            raw = read_rva(rb, rs, va + t, 4)
            if t + 4 > vs or not (mine[t:t + 4] == raw or (mine[t:t + 4] == bytes(4) and (t in listed or t in csl))):
                print("%s: slot at +0x%x is neither retail's raw value nor zero and listed in ptrs= or cset= (--mixed)" % (ours, t))
                return 1, None
            if mine[t:t + 4] == raw:
                kept += 1
                kept_values.add(struct.unpack("<I", raw)[0])
                mine[t:t + 4] = bytes(4)
    elif kept_mode:
        for t in sorted(rset - set(offs)):
            raw = read_rva(rb, rs, va + t, 4)
            if t + 4 > vs or raw == bytes(4) or mine[t:t + 4] != raw:
                print("%s: kept slot at +0x%x is not retail's raw value" % (ours, t))
                return 1, None
            mine[t:t + 4] = bytes(4)
            kept += 1
    for i in range(vs):
        if mine[i] != img[i]:
            print("%s: first difference at +0x%x: retail 0x%02x, ours 0x%02x"
                  % (ours, i, img[i], mine[i]))
            return 1, None
    name = ours.split("/")[-1].split(".")[0].upper()
    print("%s: .data image 0x%x bytes identical (%d relocated, %d ours%s)"
          % (name, vs, len(rset), ours_n, ", %d kept" % kept + (", %d set in C" % len(csl) if cset else "") if kept_mode or mixed else ""))
    info = dict(name=name, off=off, vs=vs, relocated=len(rset), hash=image_hash(hashed, offs, parse_cset(cset) if cset else (), link), rdata=[])
    for rva, ln in rdatas:
        res, err = rdata_check(rb, rs, ob, odirs, os_, ours, rva, ln)
        if err:
            print(err)
            return 1, None
        print("%s: .rdata 0x%x:0x%x bytes identical (at our RVA 0x%x)" % (name, rva, ln, res[0]))
        info["rdata"].append((rva, ln, res[1]))
    info["ptrs"] = info["switch"] = info["tail"] = info["rawptrs"] = info["bound"] = None
    if tail:
        sym, tva, tln, _ = tail
        want, err = retail_tail(rb, base, rs, tva, tln)
        got, err2 = (None, None) if err else our_tail(ob, odirs, os_, ours, sym, tln)
        err = err or err2
        if not err:
            for i in range(tln):
                if want[i] != got[i]:
                    err = "%s: tail= %s: first difference at +0x%x (VA 0x%x): retail 0x%02x, ours 0x%02x" % (ours, sym, i, tva + i, want[i], got[i])
                    break
        if err:
            print(err)
            return 1, None
        print("%s: tail %s 0x%x bytes identical to retail from 0x%x" % (name, sym, tln, tva))
        info["tail"] = (sym, tva, tln, hashlib.sha256(got).hexdigest())
    if rawptrs:
        if rt or offs:
            print("%s: rawptrs= is for an image without relocations (retail has %d, ours %d in the image)" % (ours, len(rt), len(offs)))
            return 1, None
        rows, err = rawptrs_check(rawptrs, rb, base, rs)
        if err:
            print(err)
            return 1, None
        print("%s: raw pointer rows %s: %d code, %d data match retail" % (name, rawptrs, sum(r[0] == "CODE" for r in rows), sum(r[0] == "DATA" for r in rows)))
        info["rawptrs"] = (rawptrs, rawptrs_hash(rows, rawptrs), len(rows))
    gauge_list = False
    if ptrs and off:  # a GAUGE_* list counts from .data's start, as the image does; the others from the ImportTable
        ents, err = parse_ptrs(ptrs)
        if err:
            print(err)
            return 1, None
        gauge_list = bool(ents) and all(isinstance(e[0], str) and e[0].startswith("GAUGE_") for e in ents)
    if ptrs and off and not gauge_list:
        print("%s: ptrs= needs the image to start at .data (off=0), not +0x%x" % (ours, off))
        return 1, None
    if ptrs:
        ents, err = ptrs_check(ptrs, rb, base, rs, va, rset, set(offs), kept_mode, ours, mixed, kept_values, [r - va for r in rt])
        if err:
            print(err)
            return 1, None
        print("%s: pointer list %s: %d entries match retail%s" % (name, ptrs, len(ents),
              " (%d data pointers listed)" % sum(1 for e in ents if len(e) == 3 and e[0].endswith("_DATA_POINTER")) if mixed else ""))
        info["ptrs"] = (ptrs, ptrs_hash(ents), len(ents))
    if switch:
        pat, err = switch_table(rb, rs, set(rt), base, switch)
        ms = [] if err else rdata_matches(ob, os_, pat)
        if not err and len(ms) != 1:
            err = "%s: switch 0x%x table found %d times in ours, not once" % (ours, switch[0], len(ms))
        if not err:
            err = rdata_overlap(ob, odirs, os_, ours, ms[0], len(pat))
        if err:
            print(err)
            return 1, None
        print("%s: switch 0x%x rebuilt, %d bytes identical (at our RVA 0x%x)" % (name, switch[0], len(pat), ms[0]))
        info["switch"] = switch[:4] + (hashlib.sha256(pat).hexdigest(),)
    return 0, info

# --- the manifest: MODULE retail ours [--kept|--mixed] [at=SYMBOL] [rdata=RVA:LEN[:SHA256] ...] [ptrs=PATH[:SHA256]] [rawptrs=PATH[:SHA256]] [bound=PATH[:SHA256]] [switch=...] [off=0x.. len=0x.. sha256=..]

def read_manifest(path):
    """Rows as dicts (with their text as read); comment and blank lines as plain strings."""
    rows = []
    modules = {}
    lines = open(path).read().split("\n")
    if lines[-1] == "":
        lines.pop()
    for n, line in enumerate(lines, 1):
        f = line.split("#")[0].split()
        if not f:
            rows.append(line)
            continue
        def bad(why):
            raise SystemExit("%s:%d: %s" % (path, n, why))
        if len(f) < 3:
            bad("a row needs MODULE retail ours")
        if f[0] in modules:
            bad("duplicate module %s (first at line %d)" % (f[0], modules[f[0]]))
        modules[f[0]] = n
        row = dict(module=f[0], retail=f[1], ours=f[2], kept=False, mixed=False, linked=False, cset=None, tail=None, at=None, obj=None, extent=None, rdata=[], ptrs=None, rawptrs=None, bound=None, switch=None, off=None, len=None, sha=None, text=line)
        seen = set()
        for w in f[3:]:
            k, _, v = w.partition("=")
            if (k if "=" in w else w) in seen and k != "rdata":
                bad("duplicate field %s" % (k if "=" in w else w))
            seen.add(k if "=" in w else w)
            try:
                if w == "--kept":
                    row["kept"] = True
                elif w == "--mixed":
                    row["mixed"] = True
                elif w == "--linked":
                    row["linked"] = True
                elif k == "cset" and v:
                    parse_cset(v)
                    row["cset"] = v
                elif k == "tail" and v:
                    row["tail"] = parse_tail(v)
                elif k == "at" and v:
                    row["at"] = v
                elif k == "obj" and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", v):
                    row["obj"] = v
                elif k == "extent" and EXTENT_PREFIX.fullmatch(v):
                    m = EXTENT_PREFIX.fullmatch(v)
                    row["extent"] = ("prefix", hexint(m.group(1)), bool(m.group(2)))
                elif k == "extent" and re.fullmatch(r"suffix:0x[0-9a-fA-F]+", v):
                    row["extent"] = ("suffix", hexint(v[7:]))
                elif k == "rdata":
                    p = v.split(":")
                    if len(p) not in (2, 3):
                        bad("rdata= needs RVA:LEN[:SHA256], got %r" % w)
                    row["rdata"].append((hexint(p[0]), hexint(p[1]), p[2] if len(p) > 2 else None))
                elif k == "ptrs":
                    file, _, sha = v.partition(":")
                    row["ptrs"] = (file, sha or None)
                elif k == "rawptrs" and v:
                    file, _, sha = v.partition(":")
                    row["rawptrs"] = (file, sha or None)
                elif k == "bound" and v:
                    file, _, sha = v.partition(":")
                    row["bound"] = (file, sha or None)
                elif k == "switch":
                    row["switch"] = parse_switch(v)
                elif k == "off":
                    row["off"] = hexint(v)
                elif k == "len":
                    row["len"] = hexint(v)
                elif k == "sha256" and v:
                    row["sha"] = v
                else:
                    bad("bad field %r" % w)
            except ValueError:
                bad("bad number in %r" % w)
        if not row["obj"]:
            bad("a row needs obj=SYMBOL, its image object (the FS98_IMAGE_END symbol, without the leading underscore)")
        if row["extent"] and row["extent"][0] == "suffix" and not row["extent"][1]:
            bad("extent=suffix:OFF needs a nonzero OFF (a window at the object's start is a prefix)")
        if row["bound"] and row["extent"]:
            bad("bound= rows are the whole object: no extent=")
        if row["kept"] and row["mixed"]:
            bad("--mixed and --kept are exclusive")
        if row["linked"] and (not row["at"] or row["kept"] or row["mixed"] or row["bound"] or row["ptrs"] or row["rawptrs"] or row["switch"] or row["tail"]):
            bad("--linked needs at= and excludes --kept, --mixed and the pointer-list fields")
        if row["cset"] and not row["mixed"]:
            bad("cset= is only valid with --mixed")
        if row["bound"] and (not row["at"] or row["kept"] or row["mixed"] or row["rdata"] or row["ptrs"] or row["rawptrs"] or row["switch"] or row["tail"]):
            bad("bound= needs at= and excludes the other image fields")
        if row["at"] and row["off"]:
            bad("at= images start at the symbol, so off= must be 0")
        if (row["off"] is None) != (row["len"] is None) or (row["off"] is None) != (row["sha"] is None):
            bad("off=, len= and sha256= go together; one is missing")
        rows.append(row)
    return rows

def format_row(r):
    f = [r["module"], r["retail"], r["ours"]] + (["--kept"] if r["kept"] else []) + (["--mixed"] if r["mixed"] else []) + (["--linked"] if r["linked"] else []) + (["at=" + r["at"]] if r["at"] else []) + ["obj=" + r["obj"]] + (["extent=" + ("prefix:0x%x%s" % (r["extent"][1], ":zero" if r["extent"][2] else "") if r["extent"][0] == "prefix" else "suffix:0x%x" % r["extent"][1])] if r["extent"] else []) + (["cset=" + r["cset"]] if r["cset"] else [])
    f += ["rdata=0x%x:0x%x%s" % (a, b, ":" + c if c else "") for a, b, c in r["rdata"]]
    if r["ptrs"]:
        f.append("ptrs=" + r["ptrs"][0] + (":" + r["ptrs"][1] if r["ptrs"][1] else ""))
    if r["rawptrs"]:
        f.append("rawptrs=" + r["rawptrs"][0] + (":" + r["rawptrs"][1] if r["rawptrs"][1] else ""))
    if r["bound"]:
        f.append("bound=" + r["bound"][0] + (":" + r["bound"][1] if r["bound"][1] else ""))
    if r["tail"]:
        t = r["tail"]
        f.append("tail=%s:0x%x:0x%x%s" % (t[0], t[1], t[2], ":" + t[3] if t[3] else ""))
    if r["switch"]:
        i, n, j, m, c = r["switch"]
        f.append("switch=0x%x:0x%x:0x%x:%s%s" % (i, n, j, ",".join("0x%x=%d" % x for x in m), ":" + c if c else ""))
    if r["off"] is not None:
        f += ["off=0x%x" % r["off"], "len=0x%x" % r["len"], "sha256=%s" % r["sha"]]
    return " ".join(f)

class Structural(str):
    """A failure of the symbol lookups, bounds or extent equations (a type, not a message test)."""

class Mismatch(str):
    """A hash_check message about a hash the manifest holds (or lacks): the only kind UPDATE=1 may downgrade to a warning."""

def hash_check(row, ours):
    ob, obase, odirs, os_ = load(ours)
    bad = []
    offs = []
    if row["off"] is None:  # a row without off=, len= and sha256=: the symbol checks still run, and the row stays a hard error (UPDATE=1 fills it via --orig)
        bad.append(Structural("%s (%s): the manifest row has no off=, len= and sha256= yet; tools/ofs data-images UPDATE=1 writes them" % (ours, row["module"])))
        if row["at"]:
            _, _, err = coff_symbol_in(ob, os_, row["at"])
            if err:
                bad.append(Structural("%s: %s" % (ours, err)))
        _, _, _, err = object_symbols(ob, os_, "%s (%s): obj=%s" % (ours, row["module"], row["obj"]), row["obj"])
        if err:
            bad.append(err)
    else:
        start, mine, offs = our_image(ob, odirs, os_, ours, row["off"], row["len"], row["at"])
        if mine is not None:
            err = extent_problem(ob, odirs, os_, ours, (row["module"], row["obj"], row["extent"]), start, row["len"])
            if err:
                bad.append(err)
        link = None
        if mine is not None and row["linked"]:
            link, err = link_targets(ob, obase, os_, start, row["len"], mine, offs, ours)
            if err:
                bad.append(err)
                mine = None
        if mine is None:
            if not bad:
                bad.append(Structural(offs))
        else:
            for t in offs:
                mine[t:t + 4] = bytes(4)
            if image_hash(mine, offs, parse_cset(row["cset"]) if row["cset"] else (), link) != row["sha"]:
                bad.append(Mismatch("%s: our .data image (or its relocations) differs from the manifest" % ours))
    wins = [("%s: .rdata 0x%x:0x%x" % (ours, a, b), b, c) for a, b, c in row["rdata"]]
    if row["switch"]:
        wins.append(("%s: switch 0x%x table" % (ours, row["switch"][0]), row["switch"][1], row["switch"][4]))
    for what, ln, sha in wins:
        if sha is None:
            bad.append(Mismatch("%s: no hash" % what))
            continue
        ms = rdata_windows(ob, os_, ln, sha)
        if len(ms) != 1:
            bad.append(Structural("%s window found %d times, not once" % (what, len(ms))))
            continue
        err = rdata_overlap(ob, odirs, os_, ours, ms[0], ln)
        if err:
            bad.append(err)
    if row["tail"]:
        sym, tva, tln, tsha = row["tail"]
        got, err = our_tail(ob, odirs, os_, ours, sym, tln)
        if err:
            bad.append(err)
        elif tsha is None:
            bad.append(Mismatch("%s: no hash for the tail= window" % ours))
        elif hashlib.sha256(got).hexdigest() != tsha:
            bad.append(Mismatch("%s: tail= window %s differs from the manifest" % (ours, sym)))
    if row["ptrs"]:
        ents, err = parse_ptrs(row["ptrs"][0])
        if err:
            bad.append(err)
        elif row["ptrs"][1] is None:
            bad.append(Mismatch("%s: no hash for pointer list %s" % (ours, row["ptrs"][0])))
        elif ptrs_hash(ents) != row["ptrs"][1]:
            bad.append(Mismatch("%s: pointer list %s differs from the manifest" % (ours, row["ptrs"][0])))
    if row["bound"]:
        bounds, rows, err = parse_bound(row["bound"][0])
        if err:
            bad.append(err)
        elif row["bound"][1] is None:
            bad.append(Mismatch("%s: no hash for bound rows %s" % (ours, row["bound"][0])))
        elif bound_hash(bounds, rows) != row["bound"][1]:
            bad.append(Mismatch("%s: bound rows %s differ from the manifest" % (ours, row["bound"][0])))
        if not err and offs:  # whatever the digest says: no relocation of ours may lie inside the bound= window
            bad.append(Structural("%s: our relocation inside the bound= window" % ours))
        if row["len"] is not None and row["len"] != BOUND_END - BOUND_START:
            bad.append(Structural("%s (%s): bound= len 0x%x is not BOUND_END - BOUND_START (0x%x)" % (ours, row["module"], row["len"], BOUND_END - BOUND_START)))
    if row["rawptrs"]:
        rows, err = parse_rawptrs(row["rawptrs"][0])
        if err:
            bad.append(err)
        elif row["rawptrs"][1] is None:
            bad.append(Mismatch("%s: no hash for raw pointer rows %s" % (ours, row["rawptrs"][0])))
        elif rawptrs_hash(rows, row["rawptrs"][0]) != row["rawptrs"][1]:
            bad.append(Mismatch("%s: raw pointer rows %s differ from the manifest" % (ours, row["rawptrs"][0])))
    return bad

def run_hash(module, ours, manifest, warn, rows=None, warn_code=0):
    rows = [r for r in (rows if rows is not None else read_manifest(manifest)) if isinstance(r, dict) and r["module"] == module]
    if len(rows) != 1:
        raise SystemExit("%s: no single row for %s" % (manifest, module))
    bad = hash_check(rows[0], ours)
    if not bad:
        return 0
    for b in bad:
        print(b)
    soft = all(isinstance(b, Mismatch) for b in bad)  # only a hash mismatch may be warned about under UPDATE=1; anything else is a hard error
    print("%s: data image changed since tools/data_images.txt; if intended, run tools/ofs data-images UPDATE=1" % module if soft
          else "%s: a failure that is not a hash mismatch (symbols, extent, files); UPDATE=1 does not downgrade it" % module)
    if warn and soft:
        os.utime(ours, (0, 0))  # rerun this step next build, against the manifest as it is then
        print("WARNING only (UPDATE=1)")
        return warn_code
    return 1

def run_manifest(orig, manifest, update):
    rows = read_manifest(manifest)
    fails = nd = nbytes = nrel = nr = nrb = nsw = npl = npe = ntl = ntb = nrp = nrr = nbd = nbr = 0
    changed = False
    for r in rows:
        if not isinstance(r, dict):
            continue
        rc, info = compare(os.path.join(orig, r["retail"]), r["ours"], r["kept"],
                           [(a, b) for a, b, _ in r["rdata"]], r["ptrs"] and r["ptrs"][0], r["switch"], r["at"], r["mixed"], r["cset"], r["tail"], r["rawptrs"] and r["rawptrs"][0], r["bound"] and r["bound"][0], r["linked"], (r["module"], r["obj"], r["extent"]))
        if not rc and r["bound"] and r["len"] is not None and r["len"] != BOUND_END - BOUND_START:  # the whole SIMSUI block, whatever retail's window gave
            print("%s (%s): bound= len 0x%x is not BOUND_END - BOUND_START (0x%x)" % (r["ours"], r["module"], r["len"], BOUND_END - BOUND_START))
            rc = 1
        if rc:
            fails += 1
            continue
        nd += 1; nbytes += info["vs"]; nrel += info["relocated"]
        nr += len(info["rdata"]); nrb += sum(b for _, b, _ in info["rdata"])
        nsw += bool(info["switch"])
        if info["tail"]:
            ntl += 1; ntb += info["tail"][2]
        if info["ptrs"]:
            npl += 1; npe += info["ptrs"][2]
        if info["rawptrs"]:
            nrp += 1; nrr += info["rawptrs"][2]
        if info["bound"]:
            nbd += 1; nbr += info["bound"][2]
        new = (info["off"], info["vs"], info["hash"], info["rdata"], info["ptrs"] and info["ptrs"][:2], info["switch"], info["tail"], info["rawptrs"] and info["rawptrs"][:2], info["bound"] and info["bound"][:2])
        if update and new != (r["off"], r["len"], r["sha"], r["rdata"], r["ptrs"], r["switch"], r["tail"], r["rawptrs"], r["bound"]):
            r["off"], r["len"], r["sha"], r["rdata"], r["ptrs"], r["switch"], r["tail"], r["rawptrs"], r["bound"] = new
            r["text"] = format_row(r)
            changed = True
    if changed:
        tmp = manifest + ".tmp"
        with open(tmp, "w") as f:
            f.write("\n".join(r["text"] if isinstance(r, dict) else r for r in rows) + "\n")
        os.replace(tmp, manifest)
    return fails, "%d .data images identical (%d bytes, %d relocated), %d .rdata ranges identical (%d bytes), %d switch tables, %d tail windows (%d bytes), %d pointer lists (%d entries), %d raw pointer lists (%d rows), %d bound images (%d rows)" \
        % (nd, nbytes, nrel, nr, nrb, nsw, ntl, ntb, npl, npe, nrp, nrr, nbd, nbr)

# --- G3D's built-in bytecode, re-parsed from src/g3d/g3d_image.inc

G3D_IMAGE_INC = "src/g3d/g3d_image.inc"
G3D_PROGRAMS = ((0xf840, 0x15c22), (0x16180, 0x16e12), (0x17b1e, 0x18448))
G3D_OPCODE_TABLE = 0x4220  # 148 {handler, length} pairs; the length rule is service +0x64's (docs/modules/g3d.md)
INC_SIZES = {"U8": 1, "U16": 2, "OP": 2, "W": 2, "U32": 4}

def inc_lines(text):
    """(pos, [(macro, value, pos)]) per source line of g3d_image.inc: AT(off) sets the position, each value macro advances it."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    out, pos = [], None
    for line in text.split("\n"):
        if line.lstrip().startswith("#"):
            continue
        toks, start = [], None
        for name, arg in re.findall(r"(\w+)\(([^()]*)\)", line):
            if name == "AT":
                pos = hexint(arg)
                continue
            size = INC_SIZES.get(name) or (int(name[3:]) if re.fullmatch(r"STR\d+", name) else None)
            if size is None or pos is None:
                raise ValueError("%s(%s): unknown macro or no AT() before it" % (name, arg))
            if start is None:
                start = pos
            toks.append((name, None if name.startswith("STR") else hexint(arg), pos))
            pos += size
        if toks:
            out.append((start, toks, pos))
    return out

def opcode_length(table, b):
    """The length service +0x64 gives the instruction whose bytes start b, in 32-bit registers as retail has them; None for an opcode
    past 0x93, a b too short, or a result that is not positive (next_instruction returns 0 for it)."""
    s32 = lambda v: (v & 0xFFFFFFFF) - (1 << 32 if v & 0x80000000 else 0)
    op = struct.unpack_from("<H", b, 0)[0]
    if op > 0x93:
        return None
    base, at, mul = struct.unpack("<hbb", struct.pack("<I", table[op]))
    n = base
    try:
        if mul:
            n = s32(n + (s32(s32(struct.unpack_from("<H", b, at)[0] * mul) + 1) & ~1))
        if op == 0x15:
            n = s32(n + s32(s32((struct.unpack_from("<H", b, 2)[0] + 1) * (struct.unpack_from("<H", b, 4)[0] + 1)) * 4))
    except struct.error:
        return None
    return n if n > 0 else None

def bytecode_problems(rb, base, rs, rt, text):
    """Problems with the OP() lines of g3d_image.inc's two programs against retail's opcode table and relocations; [] if none."""
    va, tva = rs[".data"][1], rs[".text"]
    table = [struct.unpack("<I", read_rva(rb, rs, va + G3D_OPCODE_TABLE + 8 * i + 4, 4))[0] for i in range(148)]
    code = {r - va for r in rt if va <= r < va + rs[".data"][0]
            and tva[1] <= struct.unpack("<I", read_rva(rb, rs, r, 4))[0] - base < tva[1] + tva[0]}
    try:
        lines = inc_lines(text)
    except ValueError as e:
        return [str(e)]
    probs, seen_code = [], set()
    for lo, hi in G3D_PROGRAMS:
        cursor = lo
        groups = []  # an instruction is its OP() line plus, if it is long, the W() lines that continue it
        for start, toks, end in lines:
            if end <= lo or start >= hi:
                continue
            if toks[0][0] != "OP" and groups and groups[-1][2] == start and all(t[0] in ("W", "U32", "U8") for t in toks):
                groups[-1] = (groups[-1][0], groups[-1][1] + toks, end)
            else:
                groups.append((start, toks, end))
        for start, toks, end in groups:
            if start != cursor:
                probs.append("program +0x%x..+0x%x: the line at +0x%x does not follow the one before (expected +0x%x)" % (lo, hi, start, cursor))
            names = [t[0] for t in toks]
            if names[0] != "OP" or "OP" in names[1:] or any(n not in ("W", "U32", "U8") for n in names[1:]):
                probs.append("program +0x%x..+0x%x: the line at +0x%x is not one OP() line with W()/U32() operands" % (lo, hi, start))
            else:
                b = b"".join(v.to_bytes(INC_SIZES[n], "little") for n, v, _ in toks)
                if opcode_length(table, b) != len(b):
                    probs.append("program +0x%x..+0x%x: OP(0x%02x) at +0x%x is %d bytes, the opcode table's rule gives %s"
                                 % (lo, hi, toks[0][1], start, len(b), opcode_length(table, b)))
                for n, v, pos in toks:
                    if n == "U32":
                        seen_code.add(pos)
                        if pos not in code:
                            probs.append("program +0x%x..+0x%x: U32 at +0x%x is not a code address retail relocates" % (lo, hi, pos))
                        if toks[0][1] != 0x8c or pos != start + 2:
                            probs.append("embedded code address at +0x%x is not the U32 operand at instruction +2 of an OP(0x8c) line" % pos)
            cursor = end
        if cursor != hi:
            probs.append("program +0x%x..+0x%x: the OP() lines stop at +0x%x" % (lo, hi, cursor))
    for r in sorted(c for c in code if any(lo <= c < hi for lo, hi in G3D_PROGRAMS) and c not in seen_code):
        probs.append("embedded code address at +0x%x is not the U32 operand at instruction +2 of an OP(0x8c) line" % r)
    return probs

# --- self-test

def file_off(b, secs, rva):
    for vs, va, rs, raw in secs.values():
        if va <= rva < va + rs:
            return raw + rva - va
    raise SystemExit("rva 0x%x not in the file" % rva)

def reloc_blocks(b, dirs, secs):
    """(file offset of block header, [file offsets of its HIGHLOW entries]) per block."""
    rrva, rsize = struct.unpack_from("<II", b, dirs + 5 * 8)
    fo = file_off(b, secs, rrva)
    out, cur = [], 0
    while cur + 8 <= rsize:
        page, blk = struct.unpack_from("<II", b, fo + cur)
        if blk == 0:
            break
        out.append((fo + cur, [fo + cur + e for e in range(8, blk, 2)
                               if b[fo + cur + e + 1] >> 4 == 3]))
        cur += blk
    return out

def add_reloc(b, dirs, secs, rva):
    """Append one block holding exactly one HIGHLOW entry at rva, in the .reloc section's slack."""
    rrva, rsize = struct.unpack_from("<II", b, dirs + 5 * 8)
    fo = file_off(b, secs, rrva) + rsize
    if fo + 12 > file_off(b, secs, rrva) + secs[".reloc"][2]:
        raise SystemExit("self-test: no room in .reloc")
    struct.pack_into("<IIHH", b, fo, rva & ~0xFFF, 12, 0x3000 | (rva & 0xFFF), 0)
    struct.pack_into("<I", b, dirs + 5 * 8 + 4, rsize + 12)

def self_test(orig, manifest):
    """Mutated temp copies of built DLLs, each of which must fail; returns (failures, cases)."""
    rows = {r["module"]: r for r in read_manifest(manifest) if isinstance(r, dict)}
    tmp = tempfile.mkdtemp(prefix="cdi-selftest-")
    env = {k: v for k, v in os.environ.items() if k != "UPDATE"}
    me = os.path.abspath(__file__)
    with_orig = ["--orig", orig, "--manifest"]
    fails = []
    cases = [0]

    def mutated(module, edit):
        """Temp copy of module's DLL edited by edit(bytearray, dirs, secs, start, row); returns (manifest, row)."""
        row = dict(rows[module])
        b = bytearray(open(row["ours"], "rb").read())
        _, _, dirs, secs = load(row["ours"])
        if row["at"]:
            start, err = coff_symbol(bytes(b), secs, row["at"])
            if err:
                raise SystemExit("self-test: %s" % err)
        else:
            start = export(bytes(b), dirs, secs) - row["off"]
        edit(b, dirs, secs, start, row)
        d = tempfile.mkdtemp(dir=tmp)
        row["ours"] = os.path.join(d, module + ".DLL")
        open(row["ours"], "wb").write(b)
        m = os.path.join(d, "m.txt")
        open(m, "w").write(format_row(row) + "\n")
        return m, row

    def run(args, extra_env=None):
        return subprocess.run([sys.executable, "-B", me] + args, capture_output=True, text=True,
                              env=dict(env, **(extra_env or {})))

    def expect(name, p, needle, rc=1):
        if p.returncode != rc or needle not in p.stdout + p.stderr:
            fails.append("%s (exit %d, wanted exit %d and %r in: %s)" % (name, p.returncode, rc, needle, (p.stdout + p.stderr).strip()))

    def case(fn):
        cases[0] += 1
        try:
            fn()
        except (Exception, SystemExit) as e:
            fails.append("%s raised %r" % (fn.__name__, e))

    def flip_data(b, dirs, secs, start, row):
        b[file_off(b, secs, start + 0x100)] ^= 0xFF

    def flip_at(off):  # one byte of the image at +off
        return lambda b, dirs, secs, start, row: b.__setitem__(file_off(b, secs, start + off), b[file_off(b, secs, start + off)] ^ 0xFF)

    def flip_rdata(b, dirs, secs, start, row):
        b[file_off(b, secs, secs[".rdata"][1])] ^= 0xFF

    def reloc_at(t):  # t: image offset, may lie outside the image
        return lambda b, dirs, secs, start, row: add_reloc(b, dirs, secs, start + (t(row) if callable(t) else t))

    def kept_slot(change):
        def edit(b, dirs, secs, start, row):
            rb, _, rdirs, rs = load(os.path.join(orig, row["retail"]))
            va = rs[".data"][1]
            rset = {r - va for r in targets(rb, rdirs, rs) if va <= r < va + row["len"]}
            t = sorted(rset - {r - start for r in targets(bytes(b), dirs, secs)})[0]
            o = file_off(b, secs, start + t)
            if change:
                b[o] ^= 0xFF
            else:
                b[o:o + 4] = bytes(4)
        return edit

    def foreign(b, dirs, secs, start, row):  # retarget one existing entry onto a slot retail does not relocate
        rb, _, rdirs, rs = load(os.path.join(orig, row["retail"]))
        va = rs[".data"][1]
        rset = {r - va for r in targets(rb, rdirs, rs)}
        mine = {r - start for r in targets(bytes(b), dirs, secs)}
        for hdr, ents in reloc_blocks(b, dirs, secs):
            page = struct.unpack_from("<I", b, hdr)[0]
            for e in ents:
                lo = struct.unpack_from("<H", b, e)[0] & 0xFFF
                if 0 <= page + lo - start < row["len"]:
                    for t in range(row["len"] - 3):
                        if (start + t) >> 12 == page >> 12 and not any(abs(t - x) < 4 for x in rset | mine):
                            struct.pack_into("<H", b, e, 0x3000 | ((start + t) & 0xFFF))
                            return
        raise SystemExit("no relocation to retarget")

    def null_slot(b, dirs, secs, start, row):
        mine = {r - start for r in targets(bytes(b), dirs, secs)}
        for t in range(0, row["len"] - 3, 4):
            if not any(b[file_off(b, secs, start + t):][:4]) and t not in mine:
                return add_reloc(b, dirs, secs, start + t)
        raise SystemExit("no NULL slot")

    def copy_rva(b, secs, row):  # where MULTIP's .rdata copy lives in our .data
        rva, ln, sha = row["rdata"][0]
        return rdata_windows(bytes(b), secs, ln, sha)[0]

    def flip_copy(b, dirs, secs, start, row):
        b[file_off(b, secs, copy_rva(b, secs, row))] ^= 0xFF

    def over_copy(b, dirs, secs, start, row):
        add_reloc(b, dirs, secs, copy_rva(b, secs, row) + 0x10)

    def unmix(b, dirs, secs, start, row):
        row["mixed"] = False
        row["cset"] = None

    def unmix_keep_cset(b, dirs, secs, start, row):
        row["mixed"] = False

    def raw_slot(change):  # a slot whose value in ours is retail's raw one: flip it, or zero it
        def edit(b, dirs, secs, start, row):
            rb, _, rdirs, rs = load(os.path.join(orig, row["retail"]))
            va = rs[".data"][1]
            for t in sorted(r - va for r in targets(rb, rdirs, rs) if va <= r < va + row["len"]):
                o = file_off(b, secs, start + t)
                if b[o:o + 4] == bytes(read_rva(rb, rs, va + t, 4)) != bytes(4):
                    if change:
                        b[o] ^= 0xFF
                    else:
                        b[o:o + 4] = bytes(4)
                    return
            raise SystemExit("no raw slot")
        return edit

    def with_cset(text):
        def edit(b, dirs, secs, start, row):
            row["cset"] = row["cset"] + "," + text
        return edit

    def no_symbol(b, dirs, secs, start, row):
        row["at"] = "_fs98_no_such_symbol"

    def tail_start(b, secs, row):
        return coff_symbol(bytes(b), secs, row["tail"][0])[0]

    def flip_tail(at):  # one byte of our tail= window: at 0 a season word, past it the rest
        def edit(b, dirs, secs, start, row):
            b[file_off(b, secs, tail_start(b, secs, row) + at)] ^= 0xFF
        return edit

    def over_tail(b, dirs, secs, start, row):
        add_reloc(b, dirs, secs, tail_start(b, secs, row) + 0x10)

    def tail_row(**kw):
        def edit(b, dirs, secs, start, row):
            sym, va, ln, sha = row["tail"]
            row["tail"] = (sym, kw.get("va", va), kw.get("ln", ln), sha)
        return edit

    def link_edit(fn):  # fn(lines, indexes of the MAIN_LINKAGE_RETAIL lines)
        def edit(t, o):
            ls = t.split("\n")
            fn(ls, [i for i, l in enumerate(ls) if l.startswith("MAIN_LINKAGE_RETAIL")])
            return "\n".join(ls)
        return edit

    def del_last_target(t, o):
        i = t.rindex("MAIN_CODE_TARGET")
        return t[:i] + t[t.index("\n", i) + 1:]

    over_rdata = lambda b, dirs, secs, start, row: add_reloc(b, dirs, secs, secs[".rdata"][1] + 0x10)
    end_edge = reloc_at(lambda row: row["len"] - 2)
    start_edge = reloc_at(-2)

    def retail(module, edit, needle, name):
        def f():
            m, _ = mutated(module, edit)
            expect(name, run(with_orig + [m]), needle)
        f.__name__ = name
        return f

    def hashed(module, edit, needle, name, extra_env=None, rc=1):
        def f():
            m, row = mutated(module, edit)
            p = run(["--hash", module, row["ours"], "--manifest", manifest], extra_env)
            expect(name, p, needle, rc)
            if extra_env and os.stat(row["ours"]).st_mtime != 0:
                fails.append(name + ": mtime not reset")
        f.__name__ = name
        return f

    def hash_row(module, edit, needle, name):  # --hash against the edited manifest row, not the real one
        def f():
            m, row = mutated(module, edit)
            expect(name, run(["--hash", module, row["ours"], "--manifest", m]), needle)
        f.__name__ = name
        return f

    def listcase(module, textedit, name, retail_needle, hash_needle, rc=1, field="ptrs"):
        """Run both checks on a temp copy of module's pointer list edited by textedit(text, row, offs)."""
        def f():
            m, row = mutated(module, lambda *a: None)
            _, _, odirs, os_ = load(row["ours"])
            ob = open(row["ours"], "rb").read()
            offs = our_image(ob, odirs, os_, row["ours"], row["off"], row["len"], row["at"])[2]
            body = open(row[field][0]).read()
            head = body.index("*/") + 2
            new = os.path.join(os.path.dirname(m), "list.inc")
            open(new, "w").write(body[:head] + textedit(body[head:], offs))
            row[field] = (new, row[field][1])
            open(m, "w").write(format_row(row) + "\n")
            expect(name + " (retail)", run(with_orig + [m]), retail_needle, rc)
            expect(name + " (hash)", run(["--hash", module, row["ours"], "--manifest", m]), hash_needle, rc)
        f.__name__ = name
        return f

    def first_entry(text):  # (start, end) of the first entry line
        s = text.index("\n") + 1 if text.startswith("\n") else 0
        return s, text.index("\n", s) + 1
    def on_first(fn):
        def edit(text, offs):
            s, e = first_entry(text)
            return text[:s] + fn(text[s:e], offs) + text[e:]
        return edit
    def on_gauge_code(fn):  # the first GAUGE_CODE line
        def edit(text, offs):
            m = re.search(r"^GAUGE_CODE\([^\n]*\n", text, re.M)
            return text[:m.start()] + fn(m.group(0), offs) + text[m.end():]
        return edit
    hexre = re.compile(r"0x[0-9a-f]+")
    def sub_nth(line, n, val):
        ms = list(hexre.finditer(line))
        return line[:ms[n].start()] + val + line[ms[n].end():]
    def last_hex(line):
        return len(hexre.findall(line)) - 1
    def switch_edit(fn):
        def f(b, dirs, secs, start, row):
            i, n, j, m, c = row["switch"]
            row["switch"] = fn(i, n, j, m, c)
        return f
    def flip_switch(b, dirs, secs, start, row):
        b[file_off(b, secs, rdata_windows(bytes(b), secs, row["switch"][1], row["switch"][4])[0])] ^= 0xFF
    def flip_nametag(b, dirs, secs, start, row):
        rva, ln, sha = row["rdata"][1]
        b[file_off(b, secs, rdata_windows(bytes(b), secs, ln, sha)[0])] ^= 0xFF

    def raw_rows(kind):  # edit the first HOST_RT_<kind>_POINTER row of a rawptrs list: fn(line) -> replacement
        def on(fn):
            def edit(text, offs):
                m = re.search(r"^HOST_RT_%s_POINTER\([^\n]*\n" % kind, text, re.M)
                return text[:m.start()] + fn(m.group(0)) + text[m.end():]
            return edit
        return on
    raw_code, raw_data = raw_rows("CODE"), raw_rows("DATA")

    def bytecode_cases():
        rb, base, rdirs, rs = load(os.path.join(orig, rows["G3D"]["retail"]))
        rt = targets(rb, rdirs, rs)
        text = open(G3D_IMAGE_INC).read()
        def check(name, edited, needle):
            probs = bytecode_problems(rb, base, rs, rt, edited)
            if needle is None and probs or needle and not any(needle in p for p in probs):
                fails.append("%s: %r not in %s" % (name, needle, probs[:3]))
        check("67 bytecode: the committed file re-parses", text, None)
        check("68 bytecode: an OP() line with a wrong operand count",
              re.sub(r"(OP\(0x0f\)[^\n]*?), W\(0x[0-9a-f]+\),", r"\1,", text, count=1), "opcode table's rule")
        check("69 bytecode: a line missing from a program",
              re.sub(r"\n[^\n]*\n(/\* \+0x16e12 )", r"\n\1", text, count=1), "stop at")
        check("70 bytecode: an embedded address moved off its operand",
              text.replace("OP(0x8c), U32(0x206f28b7),", "OP(0x8c), W(0x28b7), W(0x206f),", 1), "embedded code address")
        cases[0] += 4

    def zero_range_twice():  # retail zeros past REALITY's .rdata sit in our zero pad many times
        row = dict(rows["REALITY"])
        rva0, ln0, _ = row["rdata"][0]
        row["rdata"] = [(rva0 + ln0, 0x10, None)]
        m = os.path.join(tempfile.mkdtemp(dir=tmp), "m.txt")
        open(m, "w").write(format_row(row) + "\n")
        expect("7 .rdata found twice", run(with_orig + [m]), "times in ours")

    def update_leaves_failing_row():
        m, _ = mutated("ANALYSIS", flip_data)
        before = open(m).read()
        expect("update on a failing row", run(with_orig + [m, "--update"]), "first difference")
        if open(m).read() != before:
            fails.append("--update rewrote a failing row")

    def update_tail_hash():  # a MAIN row with its tail sha256 stale: UPDATE=1 rewrites it, then --hash passes
        m, row = mutated("MAIN", lambda *x: None)
        want = row["tail"]
        row["tail"] = want[:3] + ("0" * 64,)
        open(m, "w").write(format_row(row) + "\n")
        p = run(with_orig + [m, "--update"])
        expect("update the tail hash", p, "data-images:", 0)
        new = [r for r in read_manifest(m) if isinstance(r, dict)][0]
        if new["tail"] != want:
            fails.append("--update left the tail hash %r, wanted %r" % (new["tail"], want))
        expect("hash after the tail update", run(["--hash", "MAIN", row["ours"], "--manifest", m]), "", 0)

    def update_rawptrs_hash():  # a HOST row with its rawptrs sha256 stale: UPDATE=1 rewrites it, then --hash passes
        m, row = mutated("HOST", lambda *x: None)
        want = row["rawptrs"]
        row["rawptrs"] = (want[0], "0" * 64)
        open(m, "w").write(format_row(row) + "\n")
        expect("update the rawptrs hash", run(with_orig + [m, "--update"]), "data-images:", 0)
        new = [r for r in read_manifest(m) if isinstance(r, dict)][0]
        if new["rawptrs"] != want:
            fails.append("--update left the rawptrs hash %r, wanted %r" % (new["rawptrs"], want))
        expect("hash after the rawptrs update", run(["--hash", "HOST", row["ours"], "--manifest", m]), "", 0)

    def rawptrs_header_edit():  # the header comment is outside the hash: an edit to it passes both checks
        m, row = mutated("HOST", lambda *x: None)
        text = open(row["rawptrs"][0]).read()
        new = os.path.join(os.path.dirname(m), "tables.inc")
        open(new, "w").write(text.replace("/*", "/* x ", 1))
        row["rawptrs"] = (new, row["rawptrs"][1])
        open(m, "w").write(format_row(row) + "\n")
        expect("95 rawptrs: a header edit (retail)", run(with_orig + [m]), "data-images:", 0)
        expect("95 rawptrs: a header edit (hash)", run(["--hash", "HOST", row["ours"], "--manifest", m]), "", 0)

    def rawptrs_other_row():  # a changed HOST_RT_FUNCTION row passes the retail check (pointer rows only) but fails --hash
        m, row = mutated("HOST", lambda *x: None)
        text = open(row["rawptrs"][0]).read()
        i = text.index("HOST_RT_FUNCTION(", text.index("#ifdef HOST_RT_FUNCTION"))
        new = os.path.join(os.path.dirname(m), "tables.inc")
        open(new, "w").write(text[:i] + "HOST_RT_FUNCTION(0x1, 0, 0)\n" + text[text.index("\n", i) + 1:])
        row["rawptrs"] = (new, row["rawptrs"][1])
        open(m, "w").write(format_row(row) + "\n")
        expect("changed HOST_RT_FUNCTION row (retail)", run(with_orig + [m]), "data-images:", 0)
        expect("changed HOST_RT_FUNCTION row (hash)", run(["--hash", "HOST", row["ours"], "--manifest", m]), "raw pointer rows")

    def bound_rows(kind):  # edit the first SIMSUI_RELOC_<kind> row of a bound= table: fn(line) -> replacement
        def on(fn):
            def edit(text, offs):
                m = re.search(r"^SIMSUI_RELOC_%s\([^\n]*\n" % kind, text, re.M)
                return text[:m.start()] + fn(m.group(0)) + text[m.end():]
            return edit
        return on
    bd_data, bd_code, bd_import = bound_rows("DATA"), bound_rows("CODE"), bound_rows("IMPORT")
    as_none = lambda l: "SIMSUI_RELOC_NONE(%s)\n" % l[l.index("(") + 1:l.index(",")]

    def swap_routine(text, offs):  # the first CODE row takes the routine of the first row that names another
        rows = re.findall(r"^SIMSUI_RELOC_CODE\((0x[0-9a-f]+), (\w+)\)$", text, re.M)
        other = next(r for r in rows if r[1] != rows[0][1])[1]
        return text.replace("%s, %s)" % rows[0], "%s, %s)" % (rows[0][0], other), 1)

    def swap_rows(text, offs):  # two neighbouring rows out of ascending order
        m = re.search(r"^(SIMSUI_RELOC_[^\n]*\n)(SIMSUI_RELOC_[^\n]*\n)", text, re.M)
        return text[:m.start()] + m.group(2) + m.group(1) + text[m.end():]

    def shared_routine():  # one routine stands for several retail RVAs and the table, read as a whole, still passes (a duplicate-routine CODE row)
        path = rows["SIMSUI"]["bound"][0]
        rb, base, rdirs, rs = load(os.path.join(orig, rows["SIMSUI"]["retail"]))
        window, _ = bound_window(rb, base, rs)
        targets_of = {}
        for kind, site, arg in parse_bound(path)[1]:
            if kind == "CODE":
                targets_of.setdefault(arg, set()).add(struct.unpack_from("<I", window, site - BOUND_START)[0] - base)
        if len(targets_of.get("fs98_simsui_nothing", ())) < 2:
            fails.append("112 bound: fs98_simsui_nothing stands for fewer than two retail RVAs")
        if bound_check(path, rb, base, rs, rdirs, window)[2]:
            fails.append("112 bound: the table with a routine standing for several RVAs does not pass")

    def import_row(site, new):  # an IMPORT row at site keeps its kind and takes another index
        def edit(text, offs):
            line = "SIMSUI_RELOC_IMPORT(0x%x, " % site
            i = text.index(line)
            return text[:i] + line + new + text[text.index(")", i):]
        return edit

    def rebind_line(n):  # a GAUGE_REBIND line (index n) edited by fn
        def pick(fn):
            def edit(text, offs):
                ls = text.split("\n")
                at = [i for i, l in enumerate(ls) if l.startswith("GAUGE_REBIND")][n]
                fn(ls, at)
                return "\n".join(ls)
            return edit
        return pick

    def gauge_rdata(b, dirs, secs, start, row):  # one byte of 737ADI's kept .rdata window
        rva, ln, sha = row["rdata"][0]
        b[file_off(b, secs, rdata_windows(bytes(b), secs, ln, sha)[0])] ^= 0xFF

    def swap_next(ls, at):
        ls[at], ls[at + 1] = ls[at + 1], ls[at]

    def synthetic_rdata_target():  # no real gauge slot points into .rdata: retarget one slot of a copy of retail's file, and its row
        d = tempfile.mkdtemp(dir=tmp)
        row = dict(rows["737ADI"])
        os.makedirs(os.path.join(d, os.path.dirname(row["retail"])))
        b = bytearray(open(os.path.join(orig, row["retail"]), "rb").read())
        _, base, rdirs, rs = load(os.path.join(orig, row["retail"]))
        ents = parse_ptrs(row["ptrs"][0])[0]
        slot = ents[0][1]
        struct.pack_into("<I", b, rs[".data"][3] + slot, base + rs[".rdata"][1] + 8)
        open(os.path.join(d, row["retail"]), "wb").write(b)
        text = open(row["ptrs"][0]).read()
        new = os.path.join(d, "rebind.inc")
        open(new, "w").write(text.replace("GAUGE_REBIND(0x%x, 0x%x)" % ents[0][1:], "GAUGE_REBIND(0x%x, 0x%x)" % (slot, rs[".rdata"][1] + 8), 1))
        row["ptrs"] = (new, None)
        m = os.path.join(d, "m.txt")
        open(m, "w").write(format_row(row) + "\n")
        expect("139 ptrs: a GAUGE_REBIND target in .rdata (synthetic) passes", run(["--orig", d, "--manifest", m]), "data-images:", 0)

    # --- the image-extent check (extent_problem, our_tail): edits of our COFF symbol table, and of the row's obj= and extent=
    def sym_rec(b, name):  # the file offset of the one primary symbol record named `name`
        pe = struct.unpack_from("<I", b, 0x3C)[0]
        ptr, n = struct.unpack_from("<II", b, pe + 12)
        found, i = [], 0
        while i < n:
            o = ptr + 18 * i
            nm = (b[o:o + 8].rstrip(b"\0") if b[o:o + 4] != bytes(4) else
                  b[ptr + 18 * n + struct.unpack_from("<I", b, o + 4)[0]:].split(b"\0")[0])
            if nm == name.encode():
                found.append(o)
            i += 1 + b[o + 17]
        if len(found) != 1:
            raise SystemExit("self-test: %d symbols named %s" % (len(found), name))
        return found[0]

    def add_symbol(b, name, value, sec):  # a local symbol appended to the table (the string table ends the file)
        pe = struct.unpack_from("<I", b, 0x3C)[0]
        ptr, n = struct.unpack_from("<II", b, pe + 12)
        st = ptr + 18 * n
        size = struct.unpack_from("<I", b, st)[0]
        if len(b) != st + size:
            raise SystemExit("self-test: the string table does not end the file")
        b[st:st] = struct.pack("<IIIhHBB", 0, size, value, sec, 0, 3, 0)
        b += name.encode() + b"\0"
        struct.pack_into("<I", b, st + 18, size + len(name) + 1)
        struct.pack_into("<I", b, pe + 16, n + 1)

    def obj_name(row):
        return "_" + row["obj"]

    def end_of(sym):  # the symbol the edits below move: _<obj>_end, or tail='s _end
        return lambda row: (sym(row) if sym else obj_name(row)) + "_end"

    def end_moved(delta, sym=None):
        def edit(b, dirs, secs, start, row):
            o = sym_rec(b, end_of(sym)(row))
            struct.pack_into("<I", b, o + 8, struct.unpack_from("<I", b, o + 8)[0] + delta)
        return edit

    def end_in_section(b, dirs, secs, start, row):
        o = sym_rec(b, end_of(None)(row))
        sec = struct.unpack_from("<h", b, o + 12)[0]
        struct.pack_into("<h", b, o + 12, 1 if sec != 1 else 2)

    def end_at_start(b, dirs, secs, start, row):  # the end symbol on the object's own address: not after it
        s0, e0 = sym_rec(b, obj_name(row)), sym_rec(b, end_of(None)(row))
        struct.pack_into("<I", b, e0 + 8, struct.unpack_from("<I", b, s0 + 8)[0])

    def twice(name):  # a second symbol of the same name, section and value
        def edit(b, dirs, secs, start, row):
            o = sym_rec(b, name(row))
            add_symbol(b, name(row), struct.unpack_from("<I", b, o + 8)[0], struct.unpack_from("<h", b, o + 12)[0])
        return edit

    def row_edit(**kw):  # the row's own obj= / extent= / len
        def edit(b, dirs, secs, start, row):
            row.update(kw)
        return edit

    def extent_case(name, module, edit, needle, rc=1):  # both checks on a temp copy of the module: --orig, and --hash against the real manifest row
        def f():
            m, row = mutated(module, edit)
            expect(name + " (retail)", run(with_orig + [m]), needle if rc else "data-images:", rc)
            expect(name + " (hash)", run(["--hash", module, row["ours"], "--manifest", m]), needle if rc else "", rc)
        f.__name__ = name
        return f

    def extent_manifest(name, module, edit, needle):  # the manifest row edited (not the DLL); both read the manifest
        def f():
            row = dict(rows[module])
            edit(row)
            m = os.path.join(tempfile.mkdtemp(dir=tmp), "m.txt")
            open(m, "w").write(format_row(row) + "\n")
            expect(name + " (retail)", run(with_orig + [m]), needle)
            expect(name + " (hash)", run(["--hash", module, row["ours"], "--manifest", m]), needle)
        f.__name__ = name
        return f

    def bad_row_text(name, edit, needle, module="WINMSG"):  # a manifest line edited as text: it must not parse
        def f():
            m = os.path.join(tempfile.mkdtemp(dir=tmp), "m.txt")
            open(m, "w").write(edit(format_row(rows[module])) + "\n")
            expect(name, run(with_orig + [m]), needle)
        f.__name__ = name
        return f

    def truncate_symbols(b, dirs, secs, start, row):  # NumberOfSymbols past the end of the file
        pe = struct.unpack_from("<I", b, 0x3C)[0]
        struct.pack_into("<I", b, pe + 16, struct.unpack_from("<I", b, pe + 16)[0] + 100000)

    def end_unsectioned(b, dirs, secs, start, row):
        struct.pack_into("<h", b, sym_rec(b, end_of(None)(row)) + 12, 0)

    def bad_optsize(b, dirs, secs, start, row):  # SizeOfOptionalHeader 0xffff
        struct.pack_into("<H", b, struct.unpack_from("<I", b, 0x3C)[0] + 20, 0xffff)

    def anchor_in_text(b, dirs, secs, start, row):  # the at= symbol tagged .text, its value moved so that the address stays where it was (in .data)
        o = sym_rec(b, row["at"])
        hdrs = sec_headers(bytes(b))
        sec = struct.unpack_from("<h", b, o + 12)[0]
        value = struct.unpack_from("<I", b, o + 8)[0] + hdrs[sec - 1][1] - hdrs[0][1]
        struct.pack_into("<I", b, o + 8, value)
        struct.pack_into("<h", b, o + 12, 1)

    def both(*edits):
        def edit(b, dirs, secs, start, row):
            for e in edits:
                e(b, dirs, secs, start, row)
        return edit

    def stale_bound(b, dirs, secs, start, row):
        row["bound"] = (row["bound"][0], "0" * 64)

    def hard_update(name, module, edit, needle):  # UPDATE=1 downgrades a hash mismatch only: this fails, single pair and batch
        def f():
            m, row = mutated(module, edit)
            _, good = mutated(module, lambda *x: None)
            for label, pairs, rc in (("single", [module, row["ours"]], 1), ("batch", [module, row["ours"], module, good["ours"]], 1)):
                p = run(["--hash"] + pairs + ["--manifest", m], {"UPDATE": "1"})
                expect("%s (%s)" % (name, label), p, needle, rc)
                if (label == "single" and "WARNING only" in p.stdout) or os.stat(row["ours"]).st_mtime == 0:
                    fails.append("%s (%s): downgraded to a warning (%s)" % (name, label, p.stdout.strip()[:200]))
        f.__name__ = name
        return f

    def update_extent():  # UPDATE=1 downgrades a hash mismatch to a warning, never an extent failure
        m, row = mutated("WINMSG", end_moved(4))
        p = run(["--hash", "WINMSG", row["ours"], "--manifest", m], {"UPDATE": "1"})
        expect("UPDATE=1 --hash: an extent failure is a hard error", p, "is not the whole object")
        if "WARNING only" in p.stdout or os.stat(row["ours"]).st_mtime == 0:
            fails.append("UPDATE=1 --hash: an extent failure was downgraded (%s)" % p.stdout.strip())
        _, good = mutated("WINMSG", lambda *x: None)
        p = run(["--hash", "WINMSG", row["ours"], "WINMSG", good["ours"], "--manifest", m], {"UPDATE": "1"})
        expect("UPDATE=1 --hash, two pairs: an extent failure exits 1, not 3", p, "is not the whole object")
        m2, row2 = mutated("WINMSG", lambda *x: None)  # a stale hash: UPDATE=1 only warns (the old behaviour stands)
        row2["sha"] = "0" * 64
        open(m2, "w").write(format_row(row2) + "\n")
        expect("UPDATE=1 --hash: a stale hash is still a warning", run(["--hash", "WINMSG", row2["ours"], "--manifest", m2], {"UPDATE": "1"}), "WARNING only", 0)
        m3, row3 = mutated("WINMSG", end_moved(4))  # --orig --update with a stale hash and a broken extent: exit 1, nothing rewritten
        row3["sha"] = "0" * 64
        open(m3, "w").write(format_row(row3) + "\n")
        before = open(m3).read()
        expect("UPDATE=1 --orig: an extent failure is a hard error", run(with_orig + [m3, "--update"], {"UPDATE": "1"}), "is not the whole object")
        if open(m3).read() != before:
            fails.append("UPDATE=1 --orig rewrote a row whose extent failed")

    def gauge_pairs():  # configure time: every gauge row names gauge_data, whole
        d = tempfile.mkdtemp(dir=tmp)
        text = open(manifest).read()
        m = os.path.join(d, "m.txt")
        open(m, "w").write(text)
        expect("gauge-pairs: the real manifest passes", run(["--gauge-pairs", "src/gauges/gauges_manifest.json", "--manifest", m]), "737ADI", 0)
        open(m, "w").write(re.sub(r"^(737ADI .*?)obj=gauge_data", r"\1obj=gauge_rdata", text, count=1, flags=re.M))
        expect("gauge-pairs: a gauge row with another obj=", run(["--gauge-pairs", "src/gauges/gauges_manifest.json", "--manifest", m]), "obj=gauge_data")
        open(m, "w").write(re.sub(r"^(737ADI .*?obj=gauge_data)", r"\1 extent=prefix:0x10", text, count=1, flags=re.M))
        expect("gauge-pairs: a gauge row with an extent=", run(["--gauge-pairs", "src/gauges/gauges_manifest.json", "--manifest", m]), "no extent=")

    def batch_hash():  # --hash with several pairs
        good, other = rows["737ADI"], rows["737ALTIM"]
        bad1, bad2 = (mutated(x, flip_at(0))[1]["ours"] for x in ("737ADI", "737ALTIM"))
        expect("145 hash: one pair passes", run(["--hash", "737ADI", good["ours"], "--manifest", manifest]), "", 0)
        expect("146 hash: two pairs pass", run(["--hash", "737ADI", good["ours"], "737ALTIM", other["ours"], "--manifest", manifest]), "", 0)
        p = run(["--hash", "737ADI", good["ours"], "737ALTIM", bad2, "--manifest", manifest])
        expect("147 hash: a later failing pair", p, bad2)
        if bad2 not in p.stdout or good["ours"] in p.stdout:
            fails.append("147 hash: the report must name the failing pair only (%s)" % p.stdout.strip())
        p = run(["--hash", "737ADI", bad1, "737ALTIM", bad2, "--manifest", manifest])
        expect("148 hash: two failing pairs", p, bad2)
        if bad1 not in p.stdout:
            fails.append("148 hash: every failing pair is reported (%s)" % p.stdout.strip())
        expect("149 hash: an odd count is a usage error", run(["--hash", "737ADI", good["ours"], "737ALTIM", "--manifest", manifest]), "Usage:")
        cases[0] += 4

    def mdl_slots(b, dirs, secs, start, row):  # our relocated slots of an MDL image, as image offsets
        return sorted(r - start for r in targets(bytes(b), dirs, secs) if 0 <= r - start < row["len"])

    def mdl_slot(i, change):  # change(b, file offset of slot i, our image base, image start RVA)
        def edit(b, dirs, secs, start, row):
            change(b, file_off(b, secs, start + mdl_slots(b, dirs, secs, start, row)[i]), load(row["ours"])[1], start)
        return edit

    def drop_slot(i):  # slot i of the image is no longer a relocation of ours
        def edit(b, dirs, secs, start, row):
            want = start + mdl_slots(b, dirs, secs, start, row)[i]
            for hdr, ents in reloc_blocks(b, dirs, secs):
                page = struct.unpack_from("<I", b, hdr)[0]
                for e in ents:
                    if page + (struct.unpack_from("<H", b, e)[0] & 0xFFF) == want:
                        struct.pack_into("<H", b, e, 0)
                        return
            raise SystemExit("no relocation to drop")
        return edit

    dup_slot = lambda b, dirs, secs, start, row: add_reloc(b, dirs, secs, start + mdl_slots(b, dirs, secs, start, row)[0])
    nudge = mdl_slot(0, lambda b, o, base, start: b.__setitem__(o, (b[o] + 2) & 255))  # the header's data pointer, two bytes on
    code_out = mdl_slot(2, lambda b, o, base, start: struct.pack_into("<I", b, o, base + 0x1000000))  # a code slot aimed past everything
    code_in = mdl_slot(2, lambda b, o, base, start: struct.pack_into("<I", b, o, base + start + 0x10))  # a code slot aimed into the image

    try:
        for fn in (
                update_tail_hash,
                update_rawptrs_hash,
                rawptrs_other_row,

                retail("ANALYSIS", flip_data, "first difference", "1 retail: .data byte"),
                retail("ACONTAIN", foreign, "not one of retail's", "2 retail: foreign relocation"),
                retail("ATC", kept_slot(False), "kept slot", "3 retail: kept slot zeroed"),
                retail("ATC", kept_slot(True), "kept slot", "4 retail: kept slot changed"),
                retail("ANALYSIS", flip_rdata, ".rdata", "5 retail: .rdata byte"),
                retail("ANALYSIS", over_rdata, "overlaps", "6 retail: relocation over .rdata"),
                zero_range_twice,
                retail("ANALYSIS", end_edge, "straddles an edge", "8 retail: relocation over the image end"),
                retail("ANALYSIS", start_edge, "straddles an edge", "9 retail: relocation over the image start"),
                hashed("ANALYSIS", flip_data, "differs from the manifest", "10 hash: .data byte"),
                hashed("ANALYSIS", null_slot, "differs from the manifest", "11 hash: relocation on a NULL slot"),
                hashed("ANALYSIS", flip_rdata, "window found 0 times", "12 hash: .rdata byte"),
                hashed("ANALYSIS", over_rdata, "overlaps", "13 hash: relocation over .rdata"),
                hashed("ATC", kept_slot(True), "differs from the manifest", "14 hash: kept slot changed"),
                hashed("LANDME", flip_data, "differs from the manifest", "15 hash: .data byte, row with off != 0"),
                hashed("ANALYSIS", end_edge, "straddles an edge", "16 hash: relocation over the image end"),
                hashed("ANALYSIS", start_edge, "straddles an edge", "17 hash: relocation over the image start"),
                hashed("ANALYSIS", flip_data, "WARNING", "18 hash with UPDATE=1 warns", {"UPDATE": "1"}, 0),
                retail("MULTIP", flip_copy, "found 0 times", "20 retail: byte of a .rdata range living in .data"),
                retail("MULTIP", over_copy, "overlaps", "21 retail: relocation over a .rdata range living in .data"),
                hashed("MULTIP", flip_copy, "window found 0 times", "22 hash: byte of a .rdata range living in .data"),
                hashed("MULTIP", over_copy, "overlaps", "23 hash: relocation over a .rdata range living in .data"),
                update_leaves_failing_row,
                listcase("FE", lambda t, o: "/* x */\n" + t, "24 ptrs: comment edit", "data-images:", "", 0),
                listcase("FE", on_first(lambda l, o: sub_nth(l, last_hex(l), "0x1d12")), "25 ptrs: mistyped target",
                         "retail 0x", "pointer list"),
                listcase("FACIL", on_first(lambda l, o: ""), "26 ptrs: dropped entry",
                         "is not ours, kept or listed", "pointer list"),
                listcase("FE", on_first(lambda l, o: sub_nth(l, 0, "0x1d03")), "27 ptrs: foreign slot",
                         "does not relocate", "pointer list"),
                listcase("FE", on_first(lambda l, o: l + l), "28 ptrs: duplicate slot", "duplicate slot", "pointer list"),
                listcase("FE", on_first(lambda l, o: sub_nth(l, 0, "0x%x" % o[0])), "29 ptrs: slot is ours",
                         "our own relocations", "pointer list"),
                listcase("MULTIP", on_first(lambda l, o: l.replace("{ 0,", "{ 1,")), "30 ptrs: owner 1",
                         "owner 1", "pointer list"),
                listcase("FE", on_first(lambda l, o: "garbage\n" + l), "31 ptrs: unparsable line",
                         "not a pointer entry", "not a pointer entry"),
                retail("MULTIP", switch_edit(lambda i, n, j, m, c: (i, n, j, m[1:], c)), "not in the map",
                       "35 retail: switch map entry dropped"),
                retail("MULTIP", switch_edit(lambda i, n, j, m, c: (i, n, j + 4, m, c)), "is not a retail relocation",
                       "36 retail: switch jump table shifted"),
                listcase("MULTIP", on_first(lambda l, o: re.sub(r"(0x[0-9a-f]+,\s*)\d(,)", r"\g<1>4\2", l)),
                         "37 ptrs: unknown kind", "unknown kind", "pointer list"),
                listcase("ADVDRV", on_first(lambda l, o: sub_nth(l, last_hex(l), "0x20611234")),
                         "38 ptrs: mistyped ADVDRV code target", "retail 0x", "pointer list"),
                listcase("PANELS", on_first(lambda l, o: sub_nth(l, last_hex(l), "0x0021")),
                         "39 ptrs: mistyped PANELS target", "retail 0x", "pointer list"),
                listcase("PANELS", lambda t, o: re.sub(r"(\n\s*L\()(0x[0-9a-f]+)\)", lambda m: "%s0x%04x)" % (m.group(1), int(m.group(2), 16) + 2), t, count=1),
                         "40 ptrs: PANELS L() slot moved", "does not relocate", "pointer list"),
                retail("UTIL", kept_slot(True), "kept slot", "41 retail: kept slot of an image in .rdata changed"),
                hashed("UTIL", kept_slot(True), "differs from the manifest", "42 hash: kept slot of an image in .rdata changed"),
                retail("UTIL", no_symbol, "no symbol _fs98_no_such_symbol", "43 retail: at= symbol missing"),
                hash_row("UTIL", no_symbol, "no symbol _fs98_no_such_symbol", "44 hash: at= symbol missing"),
                retail("NUMERIC", flip_data, "first difference", "45 retail: .data byte of an at= image"),
                hashed("NUMERIC", flip_data, "differs from the manifest", "46 hash: .data byte of an at= image"),
                listcase("HG2D", lambda t, o: "/* x */\n" + t, "47 ptrs: --mixed row, raw and unlisted zero slots", "data-images:", "", 0),
                retail("HG2D", raw_slot(True), "neither retail's raw value", "48 retail: --mixed slot neither zero nor raw"),
                hashed("HG2D", kept_slot(True), "differs from the manifest", "49 hash: --mixed slot changed"),
                listcase("HG2D", on_first(lambda l, o: sub_nth(l, last_hex(l), "0x00001")),
                         "50 ptrs: mistyped HG2D target", "retail 0x", "pointer list"),
                retail("HG2D", unmix, "first difference", "51 retail: the HG2D row without --mixed"),
                retail("G3D", raw_slot(False), "neither retail's raw value", "54 retail: a kept slot of a --mixed row zeroed"),
                retail("G3D", raw_slot(True), "neither retail's raw value", "55 retail: a kept slot of a --mixed row changed"),
                hashed("G3D", raw_slot(False), "differs from the manifest", "56 hash: a kept slot of a --mixed row zeroed"),
                listcase("G3D", on_first(lambda l, o: ""), "57 ptrs: a --mixed pointer entry deleted",
                         "neither retail's raw value", "pointer list"),
                listcase("HG2D", on_first(lambda l, o: ""), "58 ptrs: an HG2D pointer entry deleted",
                         "neither retail's raw value", "pointer list"),
                retail("G3D", with_cset("0x4"), "does not relocate", "59 retail: cset= slot retail does not relocate"),
                retail("G3D", with_cset("0x12a"), "also listed in ptrs=", "60 retail: cset= slot also in ptrs="),
                retail("G3D", with_cset("0x4220"), "is not zero in ours", "61 retail: cset= slot that is not zero"),
                retail("G3D", unmix_keep_cset, "cset= is only valid with --mixed", "62 retail: cset= without --mixed"),
                hash_row("G3D", lambda b, d, s_, st, row: row.update(cset="0x5c,0x60,0x70..0xb8"), "differs from the manifest",
                         "63 hash: a cset= slot dropped"),
                listcase("G3D", lambda t, o: t + "G3D_CODE_TARGET(0x206e1000)\n", "64 ptrs: a G3D_CODE_TARGET added",
                         "is not a raw value of the image's kept slots", "pointer list"),
                listcase("G3D", lambda t, o: t[:t.rindex("G3D_CODE_TARGET")], "65 ptrs: a G3D_CODE_TARGET removed",
                         "is a raw value of the image's kept slots but not listed", "pointer list"),
                listcase("G3D", lambda t, o: t + t[t.rindex("G3D_CODE_TARGET"):], "66 ptrs: a G3D_CODE_TARGET repeated",
                         "duplicate G3D_CODE_TARGET", "duplicate G3D_CODE_TARGET"),
                listcase("G3D", lambda t, o: re.sub(r"G3D_CODE_TARGET\([^\n]*\n", "", t), "71 ptrs: every G3D_CODE_TARGET removed",
                         "but not listed", "pointer list"),
                bytecode_cases,
                rawptrs_header_edit,
                listcase("HOST", raw_code(lambda l: ""), "96 rawptrs: a HOST_RT_CODE_POINTER row removed",
                         "is not listed", "raw pointer rows", 1, "rawptrs"),
                listcase("HOST", raw_data(lambda l: ""), "97 rawptrs: a HOST_RT_DATA_POINTER row removed",
                         "is not listed", "raw pointer rows", 1, "rawptrs"),
                listcase("HOST", lambda t, o: t + "HOST_RT_CODE_POINTER(0x17000, 0x1000)\n", "98 rawptrs: a row added at a word retail does not list",
                         "is not a pointer word", "raw pointer rows", 1, "rawptrs"),
                listcase("HOST", raw_code(lambda l: l + l), "99 rawptrs: a HOST_RT_CODE_POINTER row repeated",
                         "is repeated", "raw pointer rows", 1, "rawptrs"),
                listcase("HOST", raw_data(lambda l: l + l), "100 rawptrs: a HOST_RT_DATA_POINTER row repeated",
                         "is repeated", "raw pointer rows", 1, "rawptrs"),
                listcase("HOST", raw_code(lambda l: sub_nth(l, 1, "0x1000")), "101 rawptrs: a HOST_RT_CODE_POINTER row with a wrong target",
                         "retail's word gives", "raw pointer rows", 1, "rawptrs"),
                listcase("HOST", raw_data(lambda l: l.replace("DATA", "CODE")), "102 rawptrs: a data row filed as a code row",
                         "retail's word gives", "raw pointer rows", 1, "rawptrs"),
                listcase("HOST", lambda t, o: t + "HOST_RT_DATA_POINTER (0x17024, 0x16000)\n", "107 rawptrs: a row the pattern does not match",
                         "not a plain", "not a plain", 1, "rawptrs"),
                retail("HOST", flip_data, "first difference", "103 retail: a byte of HOST's .data image"),
                hashed("HOST", flip_data, "differs from the manifest", "104 hash: a byte of HOST's .data image"),
                retail("HOST", flip_copy, "found 0 times", "105 retail: a byte of HOST's .rdata"),
                hashed("HOST", flip_copy, "window found 0 times", "106 hash: a byte of HOST's .rdata"),
                retail("SIM1", flip_data, "first difference", "108 retail: a byte of SIM1's .data"),
                hashed("SIM1", flip_data, "differs from the manifest", "109 hash: a byte of SIM1's .data"),
                retail("SIM1", flip_copy, "found 0 times", "110 retail: a byte of SIM1's .rdata window"),
                hashed("SIM1", flip_copy, "window found 0 times", "111 hash: a byte of SIM1's .rdata window"),
                listcase("MAIN", lambda t, o: "/* x */\n" + t, "72 ptrs: the MAIN row passes with its lists", "data-images:", "", 0),
                listcase("MAIN", link_edit(lambda ls, ix: ls.__setitem__(ix[3], sub_nth(ls[ix[3]], 1, "0x12345678"))),
                         "73 ptrs: a MAIN_LINKAGE_RETAIL value changed", "MAIN_LINKAGE_RETAIL", "pointer list"),
                listcase("MAIN", link_edit(lambda ls, ix: ls.pop(ix[5])), "74 ptrs: a MAIN_LINKAGE_RETAIL line removed",
                         "MAIN_LINKAGE_RETAIL", "pointer list"),
                listcase("MAIN", link_edit(lambda ls, ix: ls.insert(ix[5], ls[ix[5]])), "75 ptrs: a MAIN_LINKAGE_RETAIL line repeated",
                         "MAIN_LINKAGE_RETAIL", "pointer list"),
                listcase("MAIN", link_edit(lambda ls, ix: ls.__setitem__(slice(ix[5], ix[5] + 2), [ls[ix[5] + 1], ls[ix[5]]])),
                         "76 ptrs: two MAIN_LINKAGE_RETAIL lines out of order", "MAIN_LINKAGE_RETAIL", "pointer list"),
                listcase("MAIN", link_edit(lambda ls, ix: [ls.pop(i) for i in reversed(ix)]), "77 ptrs: every MAIN_LINKAGE_RETAIL line removed",
                         "MAIN_LINKAGE_RETAIL", "pointer list"),
                listcase("MAIN", lambda t, o: t + "MAIN_CODE_TARGET(0x207e1000)\n", "78 ptrs: a MAIN_CODE_TARGET added",
                         "is not a raw value of the image's kept slots", "pointer list"),
                listcase("MAIN", del_last_target, "79 ptrs: a MAIN_CODE_TARGET removed",
                         "is a raw value of the image's kept slots but not listed", "pointer list"),
                listcase("MAIN", lambda t, o: t + t[t.rindex("MAIN_CODE_TARGET"):t.index("\n", t.rindex("MAIN_CODE_TARGET")) + 1],
                         "80 ptrs: a MAIN_CODE_TARGET repeated", "duplicate MAIN_CODE_TARGET", "duplicate MAIN_CODE_TARGET"),
                listcase("MAIN", lambda t, o: re.sub(r"MAIN_CODE_TARGET\([^\n]*\n", "", t), "81 ptrs: every MAIN_CODE_TARGET removed",
                         "but not listed", "pointer list"),
                listcase("MAIN", on_first(lambda l, o: ""), "82 ptrs: a MAIN_DATA_POINTER entry deleted",
                         "neither retail's raw value", "pointer list"),
                hashed("MAIN", flip_data, "differs from the manifest", "83 hash: one byte of MAIN's image"),
                retail("MAIN", flip_tail(0), "first difference", "84 retail: a season word of the tail"),
                retail("MAIN", flip_tail(0x1000), "first difference", "85 retail: a byte of the rest of the tail"),
                hashed("MAIN", flip_tail(0), "tail= window", "86 hash: a season word of the tail"),
                hashed("MAIN", flip_tail(0x1000), "tail= window", "87 hash: a byte of the rest of the tail"),
                retail("MAIN", tail_row(ln=0x1000000), "not inside", "88 retail: a tail= window past retail's image"),
                retail("MAIN", tail_row(va=0x207e0000), "not inside", "89 retail: a tail= window before retail's .text"),
                retail("MAIN", tail_row(ln=0), "not positive", "90 retail: an empty tail= window"),
                hash_row("MAIN", tail_row(ln=0x1000000), "not inside one of our sections", "91 hash: a tail= window outside our sections"),
                hash_row("MAIN", tail_row(ln=0), "not positive", "92 hash: an empty tail= window"),
                retail("MAIN", over_tail, "overlaps the tail= window", "93 retail: our relocation over the tail= window"),
                hashed("MAIN", over_tail, "overlaps the tail= window", "94 hash: our relocation over the tail= window"),
                shared_routine,
                listcase("SIMSUI", lambda t, o: "/* x */\n" + t, "113 bound: the SIMSUI row passes with an added comment", "data-images:", "", 0, "bound"),
                retail("SIMSUI", flip_data, "first difference", "114 retail: a byte of SIMSUI's image"),
                hashed("SIMSUI", flip_data, "differs from the manifest", "115 hash: a byte of SIMSUI's image"),
                retail("SIMSUI", reloc_at(0x100), "inside the bound= window", "116 retail: our relocation inside SIMSUI's window"),
                hashed("SIMSUI", reloc_at(0x100), "differs from the manifest", "117 hash: our relocation inside SIMSUI's window"),
                listcase("SIMSUI", swap_routine, "118 bound: a CODE row's routine swapped", "the harness knows as", "differ from the manifest", 1, "bound"),
                listcase("SIMSUI", bd_import(lambda l: ""), "119 bound: a row dropped", "is not listed", "differ from the manifest", 1, "bound"),
                listcase("SIMSUI", bd_import(lambda l: l + l), "120 bound: a row duplicated", "is repeated", "differ from the manifest", 1, "bound"),
                listcase("SIMSUI", swap_rows, "121 bound: two rows out of order", "ascending", "differ from the manifest", 1, "bound"),
                listcase("SIMSUI", bd_data(as_none), "122 bound: a DATA row made NONE", "pointer into the image", "differ from the manifest", 1, "bound"),
                listcase("SIMSUI", bd_import(as_none), "123 bound: an IMPORT row made NONE", "thunk of import", "differ from the manifest", 1, "bound"),
                listcase("SIMSUI", bd_code(as_none), "124 bound: a CODE row made NONE", "the harness knows as", "differ from the manifest", 1, "bound"),
                listcase("SIMSUI", bd_data(lambda l: sub_nth(l, 1, "0x15000")), "125 bound: a DATA row with a wrong target", "pointer into the image", "differ from the manifest", 1, "bound"),
                listcase("SIMSUI", import_row(0x15000, "123"), "128 bound: an IMPORT row keeps its kind and changes its index", "thunk of import", "differ from the manifest", 1, "bound"),
                listcase("SIMSUI", import_row(0x15000, "0122"), "129 bound: an IMPORT index written with a leading zero (octal in C)", "not a #define", "not a #define", 1, "bound"),
                listcase("SIMSUI", lambda t, o: t.replace("SIMSUI_IMAGE_END 0x21400", "SIMSUI_IMAGE_END 0x21410"), "126 bound: a changed bound",
                         "the bounds must be", "the bounds must be", 1, "bound"),
                listcase("SIMSUI", lambda t, o: t + "SIMSUI_RELOC_NONE (0x17850)\n", "127 bound: a row the pattern does not match", "not a #define", "not a #define", 1, "bound"),
                retail("GLOBAL", flip_data, "first difference", "52 retail: GLOBAL byte"),
                hashed("GLOBAL", flip_data, "differs from the manifest", "53 hash: GLOBAL byte"),
                retail("MULTIP", flip_switch, "found 0 times", "32 retail: switch table byte"),
                hashed("MULTIP", flip_switch, "table window found 0 times", "33 hash: switch table byte"),
                hashed("MULTIP", flip_nametag, "window found 0 times", "34 hash: s_nametag byte"),
                retail("WARNING", flip_at(0x4c), "first difference", "130 retail: an in-range non-pointer byte of WARNING's image"),
                hashed("WARNING", flip_at(0x4c), "differs from the manifest", "131 hash: an in-range non-pointer byte of WARNING's image"),
                retail("SUCTION", flip_at(0x68), "first difference", "132 retail: an in-range non-pointer byte of SUCTION's image"),
                hashed("SUCTION", flip_at(0x68), "differs from the manifest", "133 hash: an in-range non-pointer byte of SUCTION's image"),
                listcase("WARNING", on_first(lambda l, o: sub_nth(l, 1, "0x4")), "134 ptrs: a GAUGE_DATA target changed",
                         "retail 0x", "pointer list"),
                listcase("SUCTION", on_gauge_code(lambda l, o: sub_nth(l, 1, "0x1020")), "135 ptrs: a GAUGE_CODE rva changed",
                         "retail 0x", "pointer list"),
                listcase("WARNING", on_first(lambda l, o: "GAUGE_CODE(0x24, 0x3000, warning_noop)\n"), "138 ptrs: a GAUGE_CODE row standing in a data slot",
                         "not in retail's .text", "pointer list"),
                listcase("WARNING", on_first(lambda l, o: ""), "136 ptrs: a GAUGE_* line dropped",
                         "is not ours, kept or listed", "pointer list"),
                listcase("SUCTION", lambda t, o: "/* x */\n" + t, "137 ptrs: a GAUGE_* list with an added comment", "data-images:", "", 0),
                retail("737ADI", flip_at(0), "first difference", "140 retail: an in-range non-pointer byte of a gauge's image"),
                hashed("737ADI", flip_at(0), "differs from the manifest", "141 hash: an in-range non-pointer byte of a gauge's image"),
                retail("737ADI", gauge_rdata, "found 0 times", "142 retail: a byte of a kept .rdata window"),
                hashed("737ADI", gauge_rdata, "window found 0 times", "143 hash: a byte of a kept .rdata window"),
                listcase("737ADI", rebind_line(0)(lambda ls, at: ls.__setitem__(at, sub_nth(ls[at], 1, "0x3004"))), "150 rebind: a target changed",
                         "retail 0x", "pointer list"),
                listcase("737ADI", rebind_line(0)(lambda ls, at: ls.__setitem__(at, sub_nth(ls[at], 0, "0x19"))), "151 rebind: a slot changed",
                         "one retail does not relocate", "pointer list"),
                listcase("737ADI", rebind_line(3)(lambda ls, at: ls.pop(at)), "152 rebind: a line dropped",
                         "is not ours, kept or listed", "pointer list"),
                listcase("737ADI", rebind_line(3)(lambda ls, at: ls.insert(at, ls[at])), "153 rebind: a line duplicated",
                         "duplicate slot", "pointer list"),
                listcase("737ADI", rebind_line(3)(swap_next), "154 rebind: two lines reordered",
                         "out of retail's relocation order", "pointer list"),
                listcase("737ADI", rebind_line(3)(lambda ls, at: ls.__setitem__(at, sub_nth(ls[at], 1, "0x5000"))), "155 rebind: a target outside the three sections",
                         "not in retail's .text, .data or .rdata", "pointer list"),
                listcase("737ADI", lambda t, o: "/* x */\n" + t, "156 rebind: a list with an added comment", "data-images:", "", 0),
                synthetic_rdata_target,
                batch_hash,
                retail("BOEING737-400_N", nudge, "image+0x36", "157 linked: a data target moved"),
                hashed("BOEING737-400_N", nudge, "differs from the manifest", "158 hash: a data target moved"),
                retail("BOEING737-400_N", code_out, "in neither the image nor our .text", "159 linked: a code target outside our .text"),
                retail("BOEING737-400_N", code_in, "retail's a code pointer", "160 linked: a code slot aimed into the image"),
                hashed("BOEING737-400_N", code_in, "differs from the manifest", "161 hash: a code slot aimed into the image"),
                retail("BOEING737-400_N", drop_slot(3), "is not one of ours (--linked)", "162 linked: one of retail's relocations is not ours"),
                hashed("BOEING737-400_N", drop_slot(3), "differs from the manifest", "163 hash: one of our relocations dropped"),
                retail("BOEING737-400_N", foreign, "not one of retail's", "164 linked: a relocation at a slot retail does not relocate"),
                retail("EXTRA300S_C", flip_at(0x100), "first difference", "165 retail: a byte of a crash model's .mdlimg image"),
                hashed("EXTRA300S_C", flip_at(0x100), "differs from the manifest", "166 hash: a byte of a crash model's .mdlimg image"),
                retail("SOPWITHCAMEL_N", flip_copy, "found 0 times", "167 retail: Sopwith's literal"),
                hashed("SOPWITHCAMEL_N", flip_copy, "window found 0 times", "168 hash: Sopwith's literal"),
                retail("BOEING737-400_N", dup_slot, "listed more than once", "169 linked: a relocation of ours repeated"),
                hashed("BOEING737-400_N", dup_slot, "listed more than once", "170 hash: a relocation of ours repeated"),
                extent_case("171 extent: an image shorter than its object (the end symbol moved up)", "WINMSG", end_moved(4), "is not the whole object"),
                extent_case("172 extent: an image longer than its object (the end symbol moved down)", "WINMSG", end_moved(-4), "is not the whole object"),
                extent_case("173 extent: a gauge's image shorter than its object", "737ADI", end_moved(4), "is not the whole object"),
                extent_case("174 extent: no end symbol", "WINMSG", row_edit(obj="ImportTable"), "no end symbol _ImportTable_end"),
                extent_case("175 extent: no such object", "WINMSG", row_edit(obj="winmsg_no_such"), "no symbol _winmsg_no_such"),
                extent_case("176 extent: two end symbols", "WINMSG", twice(end_of(None)), "2 symbols named _winmsg_image_end"),
                extent_case("177 extent: two object symbols", "WINMSG", twice(obj_name), "2 symbols named _winmsg_image"),
                extent_case("178 extent: the end in another section", "WINMSG", end_in_section, "in section"),
                extent_case("179 extent: the end not after the start", "WINMSG", end_at_start, "is not after"),
                extent_case("180 extent: a prefix row whose object shrank below its window", "ATC", end_moved(-0x700), "declares the object size 0x2000"),
                extent_case("181 extent: a prefix row whose object lost 4 bytes but still holds its window", "ATC", end_moved(-4), "declares the object size 0x2000"),
                extent_case("182 extent: NUMERIC's object shrank but still holds its window", "NUMERIC", end_moved(-0x80), "declares the object size 0x400"),
                extent_case("184 extent: a suffix that stops short of the object's end", "BOEING737-400_N", end_moved(4), "is not the suffix at +0x3000"),
                extent_case("185 extent: a crash model's suffix that overruns the object", "EXTRA300S_C", end_moved(-4), "is not the suffix at +0x5000"),
                extent_manifest("186 extent: a wrong suffix offset (+4)", "BOEING737-400_N", lambda r: r.update(extent=("suffix", 0x3004)), "is not the suffix at +0x3004"),
                extent_manifest("187 extent: a wrong suffix offset (-4)", "EXTRA300S_C", lambda r: r.update(extent=("suffix", 0x4ffc)), "is not the suffix at +0x4ffc"),
                extent_manifest("188 extent: a whole-object row declared a prefix at the wrong start", "BOEING737-400_N", lambda r: r.update(extent=("prefix", 0xeab8, False)), "is not a prefix"),
                extent_case("189 extent: bound= window shorter than its object", "SIMSUI", end_moved(4), "is not the whole object"),
                extent_case("190 extent: bound= window longer than its object", "SIMSUI", end_moved(-4), "is not the whole object"),
                extent_manifest("191 extent: bound= len not the window", "SIMSUI", lambda r: r.update(len=0xc3fc), "BOUND_END - BOUND_START"),
                extent_case("192 extent: tail= longer than its object", "MAIN", end_moved(-4, lambda r: r["tail"][0]), "object's size"),
                extent_case("193 extent: tail= shorter than its object", "MAIN", end_moved(4, lambda r: r["tail"][0]), "object's size"),
                extent_case("194 extent: tail= with no end symbol", "MAIN", lambda b, d, sc, st, row: row.update(tail=("_ImportTable",) + row["tail"][1:]), "no end symbol _ImportTable_end"),
                bad_row_text("196 manifest: a row without obj=", lambda t: t.replace(" obj=winmsg_image", ""), "a row needs obj="),
                bad_row_text("197 manifest: extent= that is neither prefix nor suffix:OFF", lambda t: t.replace(" obj=winmsg_image", " obj=winmsg_image extent=middle"), "bad field"),
                bad_row_text("198 manifest: extent=suffix:0x0", lambda t: t.replace(" obj=winmsg_image", " obj=winmsg_image extent=suffix:0x0"), "nonzero OFF"),
                bad_row_text("199 manifest: an obj= that is not a symbol name", lambda t: t.replace("obj=winmsg_image", "obj=winmsg-image"), "bad field"),
                update_extent,
                gauge_pairs,
                extent_case("200 extent: DEMO's slack is not zero", "DEMO", flip_at(0x1165), "is not all zero"),
                extent_case("201 extent: MULTIP's slack is not zero", "MULTIP", flip_at(0xf38 + 8), "is not all zero"),
                extent_case("202 extent: WEATHERM's slack is not zero", "WEATHERM", flip_at(0x1c5 + 2), "is not all zero"),
                extent_case("203 extent: a relocation in MULTIP's slack", "MULTIP", reloc_at(0xf40), "is in the slack"),
                extent_case("204 extent: MULTIP's object shrunk into its slack", "MULTIP", end_moved(-0x10), "declares the object size 0x1000"),
                bad_row_text("205 manifest: extent= on a bound= row", lambda t: t.replace(" obj=fs98_simsui_block", " obj=fs98_simsui_block extent=prefix:0xc400"), "no extent=", "SIMSUI"),
                bad_row_text("206 manifest: a duplicate field", lambda t: t + " obj=winmsg_image", "duplicate field obj"),
                bad_row_text("207 manifest: a duplicate module", lambda t: t + "\n" + t, "duplicate module WINMSG"),
                extent_case("208 extent: a truncated symbol table is a clean error", "WINMSG", truncate_symbols, "malformed COFF symbol table"),
                extent_case("209 extent: an end symbol beyond its section", "WINMSG", end_moved(0x100000), "outside its declared section"),
                extent_case("210 extent: an end symbol in no section", "WINMSG", end_unsectioned, "is not in a section"),
                hard_update("211 UPDATE=1: a missing at= symbol is a hard error", "NUMERIC", no_symbol, "no symbol _fs98_no_such_symbol"),
                hard_update("212 UPDATE=1: a missing tail= start is a hard error", "MAIN", lambda b, d, sc, st, row: row.update(tail=("_no_such_tail",) + row["tail"][1:]), "no symbol _no_such_tail"),
                hard_update("213 UPDATE=1: a duplicate tail= start is a hard error", "MAIN", twice(lambda row: row["tail"][0]), "2 symbols named _main_text_tail"),
                hard_update("214 UPDATE=1: a missing end symbol is a hard error", "WINMSG", row_edit(obj="ImportTable"), "no end symbol _ImportTable_end"),
                hard_update("215 UPDATE=1: a row without off=, len=, sha256= still checks obj= (WINMSG)", "WINMSG", row_edit(off=None, len=None, sha=None, obj="no_such_obj"), "no symbol _no_such_obj"),
                hard_update("216 UPDATE=1: a row without off=, len=, sha256= still checks obj= (SIMSUI)", "SIMSUI", row_edit(off=None, len=None, sha=None, obj="no_such_obj"), "no symbol _no_such_obj"),
                hard_update("217 UPDATE=1: a row without off=, len=, sha256= is itself a hard error", "WINMSG", row_edit(off=None, len=None, sha=None), "has no off=, len= and sha256="),
                hard_update("218 UPDATE=1: a stale bound= digest does not hide a relocation in the image", "SIMSUI", both(reloc_at(0x100), stale_bound), "our relocation inside the bound= window"),
                extent_case("219 anchor: an at= symbol whose value lies outside its declared section", "NUMERIC", anchor_in_text, "outside its declared section"),
                hard_update("220 UPDATE=1: the same anchor is a hard error", "NUMERIC", anchor_in_text, "outside its declared section"),
                hard_update("221 UPDATE=1: an .rdata window matching many times is a hard error", "ANALYSIS", row_edit(rdata=[(0x3000, 4, hashlib.sha256(bytes(4)).hexdigest())]), "times, not once"),
                hard_update("222 UPDATE=1: an .rdata window matching nowhere is a hard error", "ANALYSIS", row_edit(rdata=[(0x3000, 4, "0" * 64)]), "found 0 times, not once"),
                extent_case("223 a malformed PE header is a clean error", "WINMSG", bad_optsize, "malformed PE"),
                bad_row_text("224 manifest: a duplicate field after ptrs= is reported at the manifest line", lambda t: t + " obj=x", "m.txt:1: duplicate field obj", "FACIL"),
                ):
            case(fn)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return fails, cases[0]

def cli():
    a = sys.argv[1:]
    def opt(name, arg=True):
        if name not in a:
            return None
        i = a.index(name)
        if arg and i + 1 >= len(a):
            sys.exit(__doc__)
        v = a[i + 1] if arg else True
        del a[i:i + 1 + arg]
        return v
    env_update = os.environ.get("UPDATE") == "1"
    kept, update, st = opt("--kept", False), opt("--update", False), opt("--self-test", False)
    rdata, orig = opt("--rdata"), opt("--orig")
    manifest = opt("--manifest") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "data_images.txt")
    if "--gauge-pairs" in a:  # MODULE DLL pairs of the manifest's gauge rows; exit 1 unless they are exactly the gauges of the given JSON
        import json
        i = a.index("--gauge-pairs")
        if len(a) != i + 2:
            sys.exit(__doc__)
        gauges = json.load(open(a[i + 1]))
        rows = [r for r in read_manifest(manifest) if isinstance(r, dict) and r["ours"].startswith("bin/gauges-all/Gauges/")]
        if sorted((r["module"], r["ours"]) for r in rows) != sorted((g["name"], "bin/gauges-all/Gauges/" + g["installed"]) for g in gauges):
            sys.exit("%s: the gauge rows do not cover exactly the %d gauges of %s" % (manifest, len(gauges), a[i + 1]))
        for r in rows:  # one image per separately linked gauge: src/gauges/gauge.h's gauge_data, whole
            if r["obj"] != "gauge_data" or r["extent"]:
                sys.exit("%s: gauge row %s must have obj=gauge_data and no extent= (has obj=%s%s)" % (manifest, r["module"], r["obj"], ", extent=" + r["extent"][0] if r["extent"] else ""))
        print(" ".join("%s %s" % (r["module"], r["ours"]) for r in rows))
        return 0
    if "--hash" in a:
        i = a.index("--hash")
        pairs = a[i + 1:]
        if not pairs or len(pairs) % 2:
            sys.exit(__doc__)
        rows = read_manifest(manifest)  # one pair behaves as before; several report every failing pair
        def one(module, ours):
            try:
                return run_hash(module, ours, manifest, env_update, rows, 3 if len(pairs) > 2 else 0)
            except (Exception, SystemExit) as e:
                print("%s %s: %s" % (module, ours, e))
                return 1
        codes = [one(pairs[j], pairs[j + 1]) for j in range(0, len(pairs), 2)]
        # exit 3 (several pairs, UPDATE=1 only): a mismatch was only warned about, so the caller must not record the step as done
        return 1 if 1 in codes else 3 if 3 in codes else 0
    if orig:
        if a:
            sys.exit(__doc__)
        fails, summary = run_manifest(orig, manifest, update or env_update)
        sf, cases = self_test(orig, manifest) if st and not fails else ([], 0)  # it needs good DLLs
        for f in sf:
            print("self-test FAILED: " + f)
        if fails or sf:
            print("data-images: FAILED (%d rows, %d self-test cases)%s" % (fails, len(sf),
                  ", self-test skipped (%d rows failed)" % fails if st and fails else ""))
            return 1
        print("data-images: " + summary + (", self-test passed (%d cases)" % cases if st else ""))
        return 0
    if len(a) != 2 or any(x.startswith("--") for x in a):
        sys.exit(__doc__)
    rd = []
    if rdata:
        p = rdata.split(":")
        rd = [(hexint(p[0]), hexint(p[1]))]
    return compare(a[0], a[1], bool(kept), rd)[0]

if __name__ == "__main__":
    sys.exit(cli())
