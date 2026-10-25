#!/usr/bin/env python3
"""Synthetic regressions for dialogmap; never needs proprietary binaries.

    python3 tools/test_dialogmap.py

The real-tree acceptance checks are documented in docs/modules/dialogs.md.
"""

from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

import dialogmap


def make_pe():
    """One section, a named export, and room for >32 dialog records."""
    byImage = bytearray(0x1400)
    byImage[:2] = b'MZ'
    struct.pack_into('<I', byImage, 0x3c, 0x80)
    byImage[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', byImage, 0x84, 0x14c, 1, 0, 0, 0, 224, 0x2102)
    struct.pack_into('<H', byImage, 0x98, 0x10b)
    struct.pack_into('<I', byImage, 0x98 + 28, 0x20000000)
    struct.pack_into('<I', byImage, 0x98 + 60, 0x400)
    struct.pack_into('<I', byImage, 0x98 + 92, 16)
    struct.pack_into('<II', byImage, 0x98 + 96, 0x1000, 0x80)
    byImage[0x178:0x180] = b'.text\0\0\0'
    struct.pack_into('<IIII', byImage, 0x180, 0x1800, 0x1000, 0x1000, 0x400)
    struct.pack_into('<I', byImage, 0x178 + 36, 0x60000020)
    struct.pack_into('<IIIIII', byImage, 0x400 + 16, 1, 1, 1, 0x1040, 0x1044, 0x1048)
    struct.pack_into('<IIH', byImage, 0x440, 0x1100, 0x1050, 0)
    byImage[0x450:0x458] = b'Linkage\0'
    struct.pack_into('<6I', byImage, 0x500, 0x80000009, 0x20001800, 0x20001800,
                     0, 0, 0x20001200)
    return byImage


def make_name(szText):
    return szText.encode('utf-16-le') + b'\0\0'


class DialogMapTests(unittest.TestCase):
    def read_pe(self, byImage):
        pTemporary = tempfile.TemporaryDirectory(prefix='fs98-dialogmap-')
        self.addCleanup(pTemporary.cleanup)
        pPath = Path(pTemporary.name) / 'fixture.dll'
        pPath.write_bytes(byImage)
        return dialogmap.Fs98Pe(pPath)

    def test_registry_above_32_and_id_only_terminator(self):
        byImage = make_pe()
        for iIndex in range(65):
            struct.pack_into('<III', byImage, 0x600 + iIndex * 12,
                             0x13080000 + iIndex, 0x20001800, 4 if iIndex == 40 else 0)
        # The host stops at the first zero id, even with other words nonzero.
        struct.pack_into('<III', byImage, 0x600 + 65 * 12, 0, 0xfeedface, 0xdeadbeef)
        dLinkage = self.read_pe(byImage).linkage()
        self.assertEqual(len(dLinkage['dialogs']), 65)
        self.assertEqual(dLinkage['dialogs'][40]['flags'], 4)
        self.assertEqual(dLinkage['dialogs'][-1]['id'], 0x13080040)

    def test_registry_requires_executable_factory(self):
        byImage = make_pe()
        struct.pack_into('<III', byImage, 0x600, 2, 0x30000000, 0)
        with self.assertRaisesRegex(dialogmap.SurveyError, 'not executable'):
            self.read_pe(byImage).linkage()

    def test_virtual_tail_is_not_file_backed(self):
        pPe = self.read_pe(make_pe())
        with self.assertRaisesRegex(dialogmap.SurveyError, 'file-backed'):
            pPe.file_offset(0x2000)
        with self.assertRaises(dialogmap.SurveyError):
            pPe.file_offset(0x1ffe, 4)

    def test_truncated_image_fails(self):
        with self.assertRaises(dialogmap.SurveyError):
            self.read_pe(make_pe()[:0x185])

    def test_resource_tree_cycle_fails(self):
        byImage = make_pe()
        struct.pack_into('<II', byImage, 0x98 + 96 + 16, 0x1300, 0x80)
        struct.pack_into('<H', byImage, 0x700 + 14, 1)
        struct.pack_into('<II', byImage, 0x710, 5, 0x80000000)
        with self.assertRaisesRegex(dialogmap.SurveyError, 'cyclic'):
            self.read_pe(byImage).resources()

    def test_resource_directory_cannot_escape_extent(self):
        byImage = make_pe()
        struct.pack_into('<II', byImage, 0x98 + 96 + 16, 0x1300, 0x20)
        struct.pack_into('<H', byImage, 0x700 + 14, 1)
        struct.pack_into('<II', byImage, 0x710, 5, 0x80000040)
        with self.assertRaisesRegex(dialogmap.SurveyError, 'resource extent'):
            self.read_pe(byImage).resources()

    def test_resource_tree_reads_all_declared_leaves_and_unicode_names(self):
        byImage = make_pe()
        struct.pack_into('<II', byImage, 0x98 + 96 + 16, 0x1300, 0x600)
        struct.pack_into('<H', byImage, 0x700 + 12, 1)  # One named root entry.
        struct.pack_into('<II', byImage, 0x710, 0x80000540, 0x80000020)
        struct.pack_into('<H', byImage, 0x720 + 14, 1)
        struct.pack_into('<II', byImage, 0x730, 0x5e1, 0x80000040)
        struct.pack_into('<H', byImage, 0x740 + 14, 40)
        for iIndex in range(40):
            struct.pack_into('<II', byImage, 0x750 + iIndex * 8,
                             0x400 + iIndex, 0x200 + iIndex * 16)
            struct.pack_into('<IIII', byImage, 0x900 + iIndex * 16,
                             0x1800 + iIndex, 1, 0, 0)
        byName = 'TYPE Ω'.encode('utf-16-le')
        struct.pack_into('<H', byImage, 0xc40, len(byName) // 2)
        byImage[0xc42:0xc42 + len(byName)] = byName
        aResources = self.read_pe(byImage).resources()
        self.assertEqual(len(aResources), 40)
        self.assertEqual({dResource['type'] for dResource in aResources}, {'TYPE Ω'})
        self.assertEqual([dResource['language'] for dResource in aResources],
                         list(range(0x400, 0x428)))

    def parse_template(self, byTemplate):
        byImage = make_pe()
        byImage[0xa00:0xa00 + len(byTemplate)] = byTemplate
        return dialogmap.read_template(self.read_pe(byImage),
                                       {'rva': 0x1600, 'size': len(byTemplate), 'id': 42})

    def test_standard_template_ordinals_alignment_and_creation_data(self):
        byTemplate = bytearray(struct.pack('<IIHhhhh', 0x40, 0, 2, -2, 3, 80, 40))
        byTemplate += b'\0\0\0\0' + make_name('Test Ω')
        byTemplate += struct.pack('<H', 9) + make_name('Sans')
        for unId in (100, 101):
            byTemplate += b'\0' * (-len(byTemplate) % 4)
            byTemplate += struct.pack('<IIhhhhH', 0x50000000, 0, 1, 2, 30, 10, unId)
            byTemplate += struct.pack('<HH', 0xffff, 0x80) + make_name('Go')
            byTemplate += struct.pack('<H', 4) + b'xy'
        dTemplate = self.parse_template(byTemplate)
        self.assertEqual(dTemplate['title'], 'Test Ω')
        self.assertEqual(dTemplate['rect'], [-2, 3, 80, 40])
        self.assertEqual([dControl['id'] for dControl in dTemplate['controls']], [100, 101])
        self.assertEqual(dTemplate['controls'][0]['class'], 0x80)
        self.assertEqual(dTemplate['parsed_size'], len(byTemplate))
        with self.assertRaises(dialogmap.SurveyError):
            self.parse_template(byTemplate[:-1])

    def test_extended_template_32bit_control_id_and_extra_bytes(self):
        byTemplate = bytearray(struct.pack('<HHIIIHhhhh', 1, 0xffff, 0, 0, 0x40,
                                          1, 0, 0, 80, 40))
        byTemplate += b'\0\0\0\0' + make_name('Extended')
        byTemplate += struct.pack('<HHBB', 10, 700, 1, 1) + make_name('Sans')
        byTemplate += b'\0' * (-len(byTemplate) % 4)
        byTemplate += struct.pack('<IIIhhhhI', 0, 0, 0, 1, 2, 30, 10, 0x12345678)
        byTemplate += make_name('Custom') + struct.pack('<HH', 0xffff, 12)
        byTemplate += struct.pack('<H', 3) + b'xyz'
        dTemplate = self.parse_template(byTemplate)
        self.assertTrue(dTemplate['extended'])
        self.assertEqual(dTemplate['font_weight'], 700)
        self.assertEqual(dTemplate['controls'][0]['id'], 0x12345678)
        self.assertEqual(dTemplate['controls'][0]['title'], 12)
        self.assertEqual(dTemplate['parsed_size'], len(byTemplate))

    def test_factory_must_call_creation_import_not_just_push_template_id(self):
        pPe = self.read_pe(make_pe())
        dLinkage = {'dialogs': [{'create_va': 0x20001800, 'flags': 0}]}
        dInstructions = {
            0x20001800: ('push', '0x8a'),
            0x20001805: ('call', '0x20001840'),
            0x2000180a: ('ret', '0x8'),
            0x20001840: ('jmp', 'DWORD PTR ds:0x20001900')}
        with patch.object(dialogmap, 'disassemble', return_value=dInstructions):
            dialogmap.analyse_factories(pPe, dLinkage,
                                        [{'iat_va': 0x20001900, 'dll': 'MFC42.DLL', 'name': '#6215'}])
        self.assertEqual(dLinkage['dialogs'][0]['template_calls'], [])
        with patch.object(dialogmap, 'disassemble', return_value=dInstructions):
            dialogmap.analyse_factories(pPe, dLinkage,
                                        [{'iat_va': 0x20001900, 'dll': 'MFC42.DLL', 'name': '#2086'}])
        self.assertEqual(dLinkage['dialogs'][0]['template_calls'][0]['resource_id'], 0x8a)

    def test_message_map_requires_getter_and_executable_handler(self):
        byImage = make_pe()
        struct.pack_into('<II', byImage, 0x900, 0x20001880, 0x20001520)
        struct.pack_into('<6I', byImage, 0x920, 0x111, 0, 100, 102, 12, 0x200018a0)
        pPe = self.read_pe(byImage)
        dInstructions = {0x20001800: ('mov', 'eax,0x20001500'),
                         0x20001805: ('ret', '')}
        aMaps = dialogmap.read_message_maps(pPe, dInstructions)
        self.assertEqual(len(aMaps), 1)
        self.assertEqual(aMaps[0]['entries'][0]['last_id'], 102)
        struct.pack_into('<I', byImage, 0x920 + 20, 0x30000000)
        self.assertEqual(dialogmap.read_message_maps(self.read_pe(byImage), dInstructions), [])

    def test_callers_require_host_provenance_and_do_not_keep_id_across_call(self):
        byImage = make_pe()
        # Replace export name by ImportTable and add a second export for Linkage.
        struct.pack_into('<IIIIII', byImage, 0x400 + 16, 1, 2, 2, 0x1040, 0x1050, 0x1060)
        struct.pack_into('<II', byImage, 0x440, 0x1100, 0x1180)
        struct.pack_into('<II', byImage, 0x450, 0x1070, 0x1080)
        struct.pack_into('<HH', byImage, 0x460, 0, 1)
        byImage[0x470:0x478] = b'Linkage\0'
        byImage[0x480:0x48c] = b'ImportTable\0'
        struct.pack_into('<II', byImage, 0x580, 1, 0)
        pPe = self.read_pe(byImage)
        dInstructions = {
            0x20001800: ('push', '0x1308'),
            0x20001805: ('call', 'DWORD PTR [eax+0x10c]'),  # Not a known host link.
            0x2000180b: ('mov', 'eax,ds:0x20001184'),
            0x20001810: ('push', '0x140a'),
            0x20001815: ('call', 'DWORD PTR [eax+0x10c]'),
            0x2000181b: ('mov', 'eax,ds:0x20001184'),
            0x20001820: ('push', 'ecx'),
            0x20001821: ('call', 'DWORD PTR [eax+0x110]')}
        with patch.object(dialogmap, 'disassemble', return_value=dInstructions):
            dCaller = dialogmap.scan_callers(pPe.pPath)
        self.assertEqual([dSite['dialog_id'] for dSite in dCaller['sites']], [0x140a, None])


if __name__ == '__main__':
    unittest.main()
