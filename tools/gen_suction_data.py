#!/usr/bin/env python3
"""Extract Cessna suction's data and two bitmaps; bind every callback to C."""
import struct
from dialogmap import Fs98Pe, SurveyError
from gen_warning_data import extract


def main():
    pe = Fs98Pe('orig/Gauges/CESSSUC.GAU')
    if struct.unpack('<dd', pe.read_bytes(pe.file_offset(0x2000, 16), 16)) != (8.0, 2.0):
        raise SurveyError('reference suction clamp bounds changed')
    extract('suction', 'orig/Gauges/CESSSUC.GAU', 0x214c0000, 0x298, 0x260, 0x270, {
        0x1020: 'suction_clamp', 0x1060: 'suction_install',
        0x10b0: 'suction_unmark', 0x10c0: 'suction_prepare',
        0x10d0: 'suction_update', 0x10f0: 'suction_draw',
        0x1100: 'suction_release', 0x1130: 'suction_noop',
        0x1140: 'suction_failures',
    }, 19, [(0x1000, 0x409, 0x16ac8), (0x2000, 0x409, 0x1148)])


if __name__ == '__main__':
    main()
