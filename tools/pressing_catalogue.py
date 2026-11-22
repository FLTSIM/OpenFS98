#!/usr/bin/env python3
"""Catalogue an FS98 CD pressing, and compare two catalogues (Story 6.1).

    python3 -B tools/pressing_catalogue.py catalogue SOURCE OUT.json
    python3 -B tools/pressing_catalogue.py compare REFERENCE.json OTHER.json

SOURCE is an ISO 9660 image, a .cue (with its .bin, MODE1/2048 or MODE1/2352),
or a folder (a mounted image or a drive). Images are unpacked with 7z.

A catalogue records each file's size and SHA-256 and, for each PE, its link
date, image base and one SHA-256 per resource (type/id/language), so a pressing
whose code was rebuilt but whose resources are unchanged is told apart from one
whose resources differ. Hashes only: no Microsoft bytes are written.
"""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dialogmap import Fs98Pe  # noqa: E402

# FLTSIM98.EXE of the reference pressing (docs/building.md, "Which CD").
REFERENCE_EXE_SHA256 = 'f47c124a1ad44475a8a3a5ecbce83f5dc5d6e7dad78cf120b20616738bd99f6a'
SECTOR = 2048
RAW_SECTOR = 2352
RAW_DATA_OFFSET = 16  # MODE1/2352: 12 sync + 4 header bytes before the user data


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def cue_to_iso(cue, out):
    """Write the first data track of a MODE1 BIN/CUE as a 2048-byte-sector ISO."""
    bin_name = mode = None
    for line in cue.read_text(errors='replace').splitlines():
        words = line.split()
        if words[:1] == ['FILE'] and bin_name is None:
            bin_name = line.split('"')[1] if '"' in line else words[1]
        elif words[:1] == ['TRACK'] and mode is None and len(words) > 2:
            mode = words[2].upper()
    if bin_name is None or mode not in ('MODE1/2048', 'MODE1/2352'):
        raise SystemExit(f'{cue}: need a FILE and a MODE1/2048 or MODE1/2352 first track, found {mode}')
    raw = cue.with_name(bin_name)
    # shortcut: assumes the data track is first in the BIN (true of FS98's single-track CD)
    if mode == 'MODE1/2048':
        return raw
    with raw.open('rb') as src, out.open('wb') as dst:
        while sector := src.read(RAW_SECTOR):
            if len(sector) != RAW_SECTOR:
                raise SystemExit(f'{raw}: size is not a multiple of {RAW_SECTOR}')
            dst.write(sector[RAW_DATA_OFFSET:RAW_DATA_OFFSET + SECTOR])
    return out


def unpack(source, work):
    if source.is_dir():
        return source
    image = cue_to_iso(source, work / 'track.iso') if source.suffix.lower() == '.cue' else source
    tree = work / 'tree'
    run = subprocess.run(['7z', 'x', '-y', f'-o{tree}', str(image)], capture_output=True, text=True)
    if run.returncode != 0:
        raise SystemExit(f'7z failed on {image}:\n{run.stdout[-2000:]}{run.stderr[-2000:]}')
    return tree


def pe_record(data, path):
    if data[:2] != b'MZ':
        return None
    try:
        pe = Fs98Pe(path)
        resources = {f"{r['type']}/{r['id']}/{r['language']}":
                     sha256(pe.read_bytes(pe.file_offset(r['rva'], r['size']), r['size']))
                     for r in pe.resources()}
    except ValueError as error:
        return {'error': str(error)}  # MZ but not a PE32 we can read (a DOS stub, a 16-bit DLL)
    return {'linked': time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(pe.uiTimestamp)),
            'image_base': f'{pe.uiBase:#010x}', 'resources': resources}


def catalogue(source, out):
    source = Path(source)
    with tempfile.TemporaryDirectory() as temp:
        root = unpack(source, Path(temp))
        files = {}
        for path in sorted(root.rglob('*'), key=lambda p: str(p.relative_to(root)).upper()):
            if not path.is_file():
                continue
            name = path.relative_to(root).as_posix()
            if name.upper() in files:
                raise SystemExit(f'two files differ only in case: {name}')
            data = path.read_bytes()
            entry = {'name': name, 'size': len(data), 'sha256': sha256(data)}
            pe = pe_record(data, path)
            if pe:
                entry['pe'] = pe
            files[name.upper()] = entry
    exe = next((e for k, e in files.items() if k.endswith('/FLTSIM98.EXE') or k == 'FLTSIM98.EXE'), None)
    result = {'source': source.name, 'files': files,
              'fltsim98_exe': exe and {'name': exe['name'], 'sha256': exe['sha256'],
                                       'linked': exe.get('pe', {}).get('linked'),
                                       'is_reference': exe['sha256'] == REFERENCE_EXE_SHA256}}
    Path(out).write_text(json.dumps(result, indent=1, sort_keys=True) + '\n')
    pes = sum('resources' in e.get('pe', {}) for e in files.values())
    print(f'{source.name}: {len(files)} files, {pes} PE32 files -> {out}')
    print('FLTSIM98.EXE: ' + ('not found' if not exe else
          f"linked {exe.get('pe', {}).get('linked')}, "
          + ('the reference pressing' if result['fltsim98_exe']['is_reference'] else 'NOT the reference')))


def compare(reference_path, other_path):
    ref = json.loads(Path(reference_path).read_text())['files']
    other = json.loads(Path(other_path).read_text())['files']
    same = [k for k in ref if k in other and ref[k]['sha256'] == other[k]['sha256']]
    only_ref = [ref[k]['name'] for k in ref if k not in other]
    only_other = [other[k]['name'] for k in other if k not in ref]
    changed = [k for k in ref if k in other and ref[k]['sha256'] != other[k]['sha256']]
    print(f'# {Path(other_path).name} against {Path(reference_path).name}\n')
    print(f'- identical: {len(same)}; changed: {len(changed)}; '
          f'only in reference: {len(only_ref)}; only in other: {len(only_other)}')
    for title, names in (('Only in reference', only_ref), ('Only in other', only_other)):
        if names:
            print(f'\n## {title}\n')
            print('\n'.join(f'- `{n}`' for n in names))
    if changed:
        print('\n## Changed files\n')
        print('| File | Linked (ref → other) | Base (ref → other) | Resources |')
        print('| --- | --- | --- | --- |')
    for k in changed:
        a, b = ref[k].get('pe') or {}, other[k].get('pe') or {}
        if 'resources' in a and 'resources' in b:
            ra, rb = a['resources'], b['resources']
            diff = sorted(r for r in set(ra) | set(rb) if ra.get(r) != rb.get(r))
            res = 'identical' if not diff else f"{len(diff)} differ: {', '.join(diff[:8])}" + (' …' if len(diff) > 8 else '')
            print(f"| `{ref[k]['name']}` | {a['linked']} → {b['linked']} | {a['image_base']} → {b['image_base']} | {res} |")
        else:
            print(f"| `{ref[k]['name']}` | not a PE | | size {ref[k]['size']} → {other[k]['size']} |")
    return 0 if not (changed or only_ref or only_other) else 1


def main(argv):
    if len(argv) == 4 and argv[1] == 'catalogue':
        if shutil.which('7z') is None and not Path(argv[2]).is_dir():
            raise SystemExit('7z is needed to read images (apt install p7zip-full, or 7-Zip on Windows)')
        catalogue(argv[2], argv[3])
        return 0
    if len(argv) == 4 and argv[1] == 'compare':
        return compare(argv[2], argv[3])
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
