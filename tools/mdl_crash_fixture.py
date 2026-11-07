#!/usr/bin/env python3
"""Set a private flight to detect crashes without opening the analysis graph.

MAIN's situation table maps the one-byte record 0x16 to GLOBAL +0x830;
REALITY interprets 1 as detection and 2 as detection plus the modal graph.
Only explicitly selected disposable game copies should use this fixture.
"""
import argparse
from pathlib import Path
import struct


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game',type=Path)
    parser.add_argument('flight')
    parser.add_argument('--altitude',type=int)
    parser.add_argument('--cockpit-only',action='store_true')
    parser.add_argument('--suppress-analysis-only',action='store_true',
                        help='change crash mode 2 to 1; preserve modes 0 and 1')
    args=parser.parse_args()
    relative=Path(args.flight.replace('\\','/'))
    if relative.is_absolute() or '..' in relative.parts: raise ValueError('invalid flight path')
    path=args.game/relative
    data=bytearray(path.read_bytes());cursor=0x12c;changed=0;poses=0
    remove=[];cockpits=0
    while cursor+4<=len(data):
        tag,size=struct.unpack_from('<HH',data,cursor)
        if size<4 or cursor+size>len(data): raise ValueError('invalid situation record')
        if tag==0x16:
            if size!=5: raise ValueError('crash mode record changed size')
            if not args.suppress_analysis_only or data[cursor+4]==2:
                data[cursor+4]=1
            changed+=1
        if tag==3 and args.altitude is not None:
            if size!=34: raise ValueError('position record changed size')
            data[cursor+16:cursor+22]=(args.altitude*65536).to_bytes(6,'little',signed=True)
            poses+=1
        if tag==0x3de and args.cockpit_only:
            if size!=100: raise ValueError('window layout record changed size')
            # WINDOW's saved-field table puts view +0x10 at payload +44.
            # REALITY tests the currently painted view, including visible maps.
            kind=struct.unpack_from('<I',data,cursor+48)[0]
            if kind==0: cockpits+=1
            else: remove.append((cursor,cursor+size))
        cursor+=size
    if data[cursor:]==b'\xff\xff': cursor+=2
    if cursor!=len(data) or changed!=1: raise ValueError('expected one crash mode record and the situation terminator')
    if args.altitude is not None and poses!=1: raise ValueError('expected exactly one position record')
    if args.cockpit_only and (not remove or not cockpits):
        raise ValueError('expected cockpit and non-cockpit window records')
    for start,end in reversed(remove): del data[start:end]
    path.write_bytes(data)
    print(f'Private crash fixture: {relative}, analysis graph disabled, '
          f'detection={"preserved" if args.suppress_analysis_only else "enabled"}, '
          f'altitude={args.altitude}, removed windows={len(remove)}')


if __name__=='__main__': main()
