#!/usr/bin/env python3
r"""Install the CD's regional scenery into a staged game tree.

stage_game.py copies the CD's fs98 tree, which holds only the default world:
the terrain base, Chicago, the metro areas, and no airport outside the
heliports of heloafd.bgl.  Every other airport and all the regional scenery are
in the CD's Boosters directory.  Setup offers each of the 18 regions either on
the CD, read through a cache, or on the hard drive (Custom install); this does
what the hard drive choice does, so nothing depends on a CD drive under wine:

  - copies each region's files where Setup's own table says, Scenery\<region>\
    {scenery,texture}\, with the booster's FILESnnn directories flattened;
  - adds "Local HD-<region>" areas to Scenery\world.vis, which the CD ships
    with only the three default-world areas.

world.vis is always rebuilt from the CD's copy, so this can be rerun, and it
leaves the rest of the tree (installed modules, the profile, pilots) alone.
FACIL sees the new AFD files and rebuilds INDEX_FILE at the next start.

Usage: tools/stage_scenery.py [--cd DIR] [--out DIR]
"""
import argparse, os, re, shutil, struct, sys

# Setup.exe 0x407c36..0x408e82 builds this table on the stack; the loop at
# 0x408ebe..0x4091a9 adds the areas.  (title, area directory, layer), in its order.
REGIONS = (
    ('Africa',           'Africa',   0x14),
    ('Antartica',        'Antarcti', 0x14),  # Setup's spelling
    ('Asia',             'Asia',     0x14),
    ('Canada',           'Canada',   0x14),
    ('Europe',           'Europe',   0x14),
    ('Las Vegas',        'LasVegas', 0x1e),
    ('Hawaii',           'Hawaii',   0x1e),
    ('Oceania',          'Oceania',  0x14),
    ('South America',    'SAmerica', 0x14),
    ('US Great Lakes',   'USGL',     0x14),
    ('US North Central', 'USNC',     0x14),
    ('US South Central', 'USSC',     0x14),
    ('US Northeast',     'USNE',     0x14),
    ('US Northwest',     'USNW',     0x14),
    ('US Southeast',     'USSE',     0x14),
    ('US Southwest',     'USSW',     0x14),
    ('Hong Kong',        'Hong',     0x1e),
    ('Central America',  'CAmerica', 0x14),
)


def resolve(root, parts):
    """Existing path under root matching path parts, ignoring case."""
    cur = root
    for part in parts:
        try:
            hit = next((e for e in os.listdir(cur) if e.lower() == part.lower()), None)
        except OSError:
            return None
        if hit is None:
            return None
        cur = os.path.join(cur, hit)
    return cur


def dest_path(root, parts):
    """Destination path, reusing existing directories whose case differs."""
    cur = root
    for part in parts[:-1]:
        cur = resolve(cur, [part]) or os.path.join(cur, part)
        os.makedirs(cur, exist_ok=True)
    return os.path.join(cur, parts[-1])


def hard_drive_rows(setup):
    """(source, destination) of Setup's hard drive file records for the boosters.

    A record is source NUL destination NUL component-mask, and the mask is hex
    digits running straight into the next record's binary header.  Rows whose
    destination is under Cache\\ belong to the CD choice and are not matched.
    """
    blob = open(setup, 'rb').read()
    rows = re.findall(rb'(Boosters\\[\x20-\x7e]+)\0(Scenery\\[\x20-\x7e]+)\0[0-9a-fA-F]+', blob)
    return [(s.decode('latin1'), d.decode('latin1')) for s, d in rows]


def add_area(areas, record):
    """Setup's 0x40c720: before the first area of the same layer, else last."""
    for i, area in enumerate(areas):
        if area[1] == record[1]:
            areas.insert(i, record)
            return
    areas.append(record)


def world_vis(shipped):
    """The world file Setup writes for all 18 regions on the hard drive.

    The header holds offsets: +0x0a the area list, +0x0c the cache list after
    it, +0x00 the size.  An area is three bytes, then path and title, each NUL
    terminated; a zero byte ends the list.  (Setup's 0x40bcf0, 0x40c640.)
    """
    area_off, cache_off = struct.unpack_from('<HH', shipped, 0x0a)
    if shipped[cache_off - 1] != 0 or shipped[cache_off:] != b'\0':
        sys.exit('unexpected world.vis: its cache list is not empty')
    areas = []
    pos = area_off
    while shipped[pos]:
        path_end = shipped.index(b'\0', pos + 3)
        title_end = shipped.index(b'\0', path_end + 1)
        areas.append(shipped[pos:title_end + 1])
        pos = title_end + 1
    for title, directory, layer in REGIONS:
        add_area(areas, bytes((1, layer, 1)) +
                 b'=:\\=\\scenery\\%s\\*.BGL\0Local HD-%s\0' %
                 (directory.encode(), title.encode()))
    body = b''.join(areas) + b'\0'
    out = bytearray(shipped[:area_off] + body + b'\0')
    struct.pack_into('<H', out, 0x00, len(out))
    struct.pack_into('<H', out, 0x0c, area_off + len(body))
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cd', default='cd')
    ap.add_argument('--out', default='game')
    args = ap.parse_args()

    rows = hard_drive_rows(resolve(args.cd, ['Setup.exe']) or
                           sys.exit('no Setup.exe in %s' % args.cd))
    if not rows:
        sys.exit('no booster records found in Setup.exe')

    print('copying regional scenery ...')
    copied = 0
    for src, dst in rows:
        path = resolve(args.cd, src.split('\\'))
        if path is None:
            sys.exit('Setup.exe names %s, not on the CD' % src)
        target = dest_path(args.out, dst.split('\\'))
        if not (os.path.isfile(target) and
                os.path.getsize(target) == os.path.getsize(path)):
            shutil.copy2(path, target)
            copied += 1

    shipped = open(resolve(args.cd, ['fs98', 'Scenery', 'world.vis']), 'rb').read()
    with open(resolve(args.out, ['Scenery', 'world.vis']), 'wb') as f:
        f.write(world_vis(shipped))
    print('  %d files in %d regions (%d copied); world.vis has them as areas'
          % (len(rows), len(REGIONS), copied))


main()
