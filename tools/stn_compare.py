#!/usr/bin/env python3
"""Compare the flights (situation files, Pilots/*.STN) that the game saved under retail and
under ours, for tools/flights_play.sh.

    stn_compare.py RETAIL_DIR RETAIL_DIR OURS_DIR [OURS_DIR...]

Each directory holds the .STN files one run saved.  A file is a 0x12c-byte header and
then records, each a 16-bit id and a 16-bit length that counts those four bytes.

The sim runs between loading a flight and pausing it before the save, for as long as wine
happens to take, so some records differ between two retail runs of the same steps: the
position (0x03), the aircraft's state (0x38e), the clock (0x96) and a few others.  The
live records are LIVE, every record seen to differ between two retail runs of these steps
(2026-11-03), and any other that the two retail runs given differ in.  Every other record,
and the header, must be byte for byte the same in every run of ours.  The record list
itself (ids, order, lengths) must always match.  Of the live records the position is still
checked, within a tolerance far below what a different flight or airport would give:
latitude and longitude (48-bit, about 1.4e-7 m a unit) to 2^33 (about 1 km), altitude
(48-bit, 1/65536 m) to 100 m, the three angles (32-bit) to 2^27 (11 degrees: with eight
games at once one drifted by 2 before its pause); and the time
of day, hour and minute (0x1f) and second (0x20), to two minutes, since the clock runs on
and a save can fall either side of a minute.  The other live records are reported, not
compared.
"""
import os
import struct
import sys

HEADER = 0x12c
POSITION = 0x03
CLOCK, SECONDS = 0x1f, 0x20
LIVE = {0x03, CLOCK, SECONDS, 0x76, 0x94, 0x96, 0x38e}


def records(path):
    data = open(path, 'rb').read()
    out = [('header', data[:HEADER])]
    offset = HEADER
    while offset < len(data):
        if offset + 4 > len(data):
            out.append(('tail', data[offset:]))
            break
        rid, size = struct.unpack_from('<HH', data, offset)
        if size < 4 or offset + size > len(data):
            out.append(('tail', data[offset:]))
            break
        out.append((rid, data[offset:offset + size]))
        offset += size
    return out


def position_fields(record):
    body = record[4:]
    if len(body) != 30:
        return None
    ints = [int.from_bytes(body[i:i + 6], 'little', signed=True) for i in (0, 6, 12)]
    ints += [int.from_bytes(body[i:i + 4], 'little', signed=True) for i in (18, 22, 26)]
    return ints


def position_close(a, b):
    fa, fb = position_fields(a), position_fields(b)
    if fa is None or fb is None:
        return False
    limits = [1 << 33, 1 << 33, 100 << 16] + [1 << 27] * 3
    for i, (x, y, limit) in enumerate(zip(fa, fb, limits)):
        d = x - y
        if i >= 3:          # angles wrap
            d = (d + (1 << 31)) % (1 << 32) - (1 << 31)
        if abs(d) > limit:
            return False
    return True


def time_of_day(recs):
    found = dict((rid, d) for rid, d in recs if not isinstance(rid, str))
    clock, seconds = found.get(CLOCK), found.get(SECONDS)
    if clock is None or seconds is None or len(clock) != 6 or len(seconds) != 5:
        return None
    return clock[4] * 3600 + clock[5] * 60 + seconds[4]


def time_close(a, b):
    if a is None or b is None:
        return a == b
    d = abs(a - b) % 86400
    return min(d, 86400 - d) <= 120


def name(rid):
    return rid if isinstance(rid, str) else '%#x' % rid


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    retail, ours = argv[1:3], argv[3:]
    files = sorted(f for f in os.listdir(retail[0]) if f.upper().endswith('.STN'))
    if not files:
        print('no saved flights in %s' % retail[0])
        return 1
    failed = 0
    for f in files:
        paths = [os.path.join(d, f) for d in retail + ours]
        missing = [p for p in paths if not os.path.exists(p)]
        if missing:
            print('%s: not saved in %s' % (f, ', '.join(os.path.dirname(p) for p in missing)))
            failed = 1
            continue
        r1, r2 = records(paths[0]), records(paths[1])
        shape = [(rid, len(d)) for rid, d in r1]
        if [(rid, len(d)) for rid, d in r2] != shape:
            print('%s: the two retail runs saved different records' % f)
            failed = 1
            continue
        live = {i for i, ((rid, a), (_, b)) in enumerate(zip(r1, r2)) if a != b or rid in LIVE}
        problems = []
        for d, path in zip(['ours run %d' % (k + 1) for k in range(len(ours))], paths[2:]):
            o = records(path)
            if [(rid, len(x)) for rid, x in o] != shape:
                problems.append('%s: a different record list' % d)
                continue
            for i, ((rid, a), (_, b), (_, x)) in enumerate(zip(r1, r2, o)):
                if i not in live:
                    if x != a:
                        at = [k for k in range(len(a)) if a[k] != x[k]]
                        problems.append('%s: record %s differs at %s' % (d, name(rid), at[:8]))
                elif rid == POSITION and not (position_close(a, x) or position_close(b, x)):
                    problems.append('%s: position %s, retail %s' % (d, position_fields(x), position_fields(a)))
            if not (time_close(time_of_day(r1), time_of_day(o)) or time_close(time_of_day(r2), time_of_day(o))):
                problems.append('%s: time of day %s s, retail %s s' % (d, time_of_day(o), time_of_day(r1)))
        print('%s: %d records, %d live (%s)%s' % (
            f, len(r1), len(live), ' '.join(name(r1[i][0]) for i in sorted(live)),
            ''.join('\n    ' + p for p in problems) or ', ours the same'))
        if problems:
            failed = 1
    return failed


if __name__ == '__main__':
    sys.exit(main(sys.argv))
