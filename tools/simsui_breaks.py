#!/usr/bin/env python3
"""Break src/simsui/ on purpose and check simsuidiff reports each break.

    python3 -B tools/simsui_breaks.py [scenarios] [name...]

Only a reported DIFFER counts as caught: a build failure, a fault or a timeout does not.
Run from a frozen copy of the tree if the sources may change meanwhile (copy first, then
run there), as tools/flights_breaks.py says for Flights.

Left out as proven equivalent: CDIBSection's operator HBITMAP testing its CBitmap's address
for null (this + 0x5c is null only for an object at 0xffffffa4).
"""
from pathlib import Path
import re
import subprocess
import sys

SOURCES = Path('tools/simsui.mk').read_text()
UNITS = re.search(r'^SIMSUI_SOURCES := (.*)$', SOURCES, re.M)[1].split()

BREAKS = {
    'dibsection': [
        ('dib-construct-live', '    if (!DIB_LIVE) DIB_LIVE = 1;\n', ''),
        ('dib-construct-pal', '    D32(dib, 0x58) = 0;\n    if (!DIB_LIVE)', '    if (!DIB_LIVE)'),
        ('dib-destroy-handle', 'CALL_STD(DeleteObject, int32_t, (uint32_t))(CALL_THIS(MFC_2452, uint32_t, (void *))(bitmap));\n    PTR(bitmap, 0)',
         'CALL_STD(DeleteObject, int32_t, (uint32_t))(D32(bitmap, 4));\n    PTR(bitmap, 0)'),
        ('delete-array-count', 'fs98_simsui_vector_destroy(object, size, I32(object, -4), destroy);', 'fs98_simsui_vector_destroy(object, size, I32(object, -4) - 1, destroy);'),
        ('create-flip', 'if (I32(info, 8) > 0) I32(info, 8) = -I32(info, 8);', 'if (I32(info, 8) >= 0) I32(info, 8) = -I32(info, 8) - 1;'),
        ('create-usage', '        D32(dc, 4), info, usage, AT(dib, 0x64), 0, 0);', '        D32(dc, 4), info, 0, AT(dib, 0x64), 0, 0);'),
        ('create-exit', '    if (!handle) fs98_simsui_exit_cleanly();\n    if (!handle ||', '    if (!handle ||'),
        ('create-object', '(handle, 0x54, AT(dib, 4));\n    CALL_THIS(MFC_640', '(handle, 0x50, AT(dib, 4));\n    CALL_THIS(MFC_640'),
        ('load-type', '(uint32_t)id & 0xffff, 2);', '(uint32_t)id & 0xffff, 3);'),
        ('load-size', 'if (D32(header, 0) != 0x28) goto failed;', 'if (D32(header, 0) != 0x28 && 0) goto failed;'),
        ('load-four', 'case 4: width = (width & 1) ? (width >> 1) + 1 : width >> 1; break;', 'case 4: width = width >> 1; break;'),
        ('load-24', 'case 24: width *= 3; break;', 'case 24: width *= 4; break;'),
        ('load-abs', '    if (image < 0) image = -image;\n', ''),
        ('load-flip', '    if (!flipped) I32(info, 8) = -I32(info, 8);\n', ''),
        ('load-table', 'fs98_simsui_copy(info + 0x28, data + 0x28, table);', 'fs98_simsui_copy(info + 0x28, data + 0x28, table - 4);'),
        ('load-rows', 'source + from - row, (uint32_t)row);', 'source + from, (uint32_t)row);'),
        ('load-palette-flags', 'if (pal && (flags & 0xa))', 'if (pal && (flags & 0x2))'),
        ('load-static', '    if (flags & 8) fs98_simsui_pal_set_static_colours(pal);\n', ''),
        ('load-remap-flag', 'if (!pal || !(flags & 4)) goto done;', 'if (!pal || !(flags & 2)) goto done;'),
        ('load-quads', 'quads[index + 2] = entries[index];', 'quads[index + 2] = entries[index + 2];'),
        ('load-colour', '((uint32_t)old_quads[index * 4] << 16) |', '((uint32_t)old_quads[index * 4 + 2] << 16) |'),
        ('load-map', 'for (; bits < end; ++bits) *bits = map[*bits];', 'for (; bits + 1 < end; ++bits) *bits = map[*bits];'),
        ('load-restore', 'D32(dc, 4), old ? D32(old, 4) : 0);', 'D32(dc, 4), 0);'),
        ('load-free', 'done:\n    mfc_operator_delete(info);', 'done:'),
        ('fill-union', 'CALL_STD(UnionRect, int32_t, (void *, const void *, const void *))(&rect, &rect, &whole);', 'CALL_STD(UnionRect, int32_t, (void *, const void *, const void *))(&rect, &whole, &rect);'),
        ('fill-normalise', '    CALL_THIS(MFC_4317, void, (void *))(&rect);\n', ''),
        ('fill-skip', 'at += width + skip;', 'at += width + skip + 1;'),
        ('fill-whole', 'rect.right = I32(dib, 8); rect.bottom = I32(dib, 0xc);\n    fs98_simsui_dib_fill_rect', 'rect.right = I32(dib, 8); rect.bottom = I32(dib, 8);\n    fs98_simsui_dib_fill_rect'),
        ('clip-source-x', 'if (value >= I32(source, 8)) return 0;\n    size = *width; sum = size + value;', 'if (value > I32(source, 8)) return 0;\n    size = *width; sum = size + value;'),
        ('clip-negative', 'if (value < 0) { *dest_x = *dest_x + (size - sum); *source_x = 0; *width = sum; }', 'if (value < 0) { *dest_x = *dest_x + (size - sum); *width = sum; *source_x = 0; }'),
        ('clip-dest-y', 'if (value < 0) { value = *source_y + (size - sum); *source_y = value; *dest_y = 0; *height = sum; }',
         'if (value < 0) { value = *source_y + (size - sum); *dest_y = 0; *source_y = value; *height = sum; }'),
        ('clip-height', 'if (sum >= size) *height = size - sum + value;\n    if (*width', 'if (sum > size) *height = size - sum + value;\n    if (*width'),
        ('clip-final', '    if (*height <= 0) return 0;\n    return 1;', '    return 1;'),
        ('sprite-first', 'if (key == -1) key = *(uint8_t *)PTR(dib, 0x64);', 'if (key == -1) key = 0;'),
        ('sprite-zero', '                if (pixel) *to = pixel;', '                *to = pixel;'),
        ('sprite-key', 'if (pixel != (uint8_t)key) *to = pixel;', 'if (pixel != key) *to = pixel;'),
        ('sprite-rect', 'fs98_simsui_dib_sprite(dib, dest, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 0, 0, key);', 'fs98_simsui_dib_sprite(dib, dest, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[0], 0, 0, key);'),
        ('sprite-from', 'point[0], point[1],\n                           key);', 'point[1], point[0],\n                           key);'),
        ('mask-all', 'if (key == -1 && !all) key', 'if (key == -1) key'),
        ('mask-test', 'if (*from != (uint8_t)key || all) *to = (uint8_t)colour;', 'if (*from != (uint8_t)key) *to = (uint8_t)colour;'),
        ('copy-skip', 'from += source_stride - (uint32_t)width;\n        to += dest_stride - (uint32_t)width;\n    }\n}\n/* 0x204786f0', 'from += source_stride - (uint32_t)width;\n        to += dest_stride;\n    }\n}\n/* 0x204786f0'),
        ('copy-from', 'point[0], point[1]);\n}\n/* a row', 'point[0], 0);\n}\n/* a row'),
        ('tile-period', 'if (!--remaining) { remaining = tile_width; from = middle; }', 'if (!--remaining) { remaining = tile_width; }'),
        ('tile-ninth', 'third = I32(dib, 0xc) / 9;', 'third = I32(dib, 0xc) / 3;'),
        ('tile-cycle', 'if (!--cycle) {\n            cycle = third;', 'if (!--cycle) {\n            cycle = third + 1;'),
        ('tile-bottom', 'left = bits + tile * 6; middle = bits + tile * 7; right = bits + tile * 8;', 'left = bits + tile * 6; middle = bits + tile * 8; right = bits + tile * 7;'),
        ('tile-left-width', 'to = tile_row(to, left, middle, right, width, (uint32_t)I32(dib, 8), dest_width - width * 2);', 'to = tile_row(to, left, middle, right, width, (uint32_t)width, dest_width - width * 2);'),
        ('blit-attrib', '(D32(memory, 8), 0x26) & 0x100))\n        table = AT(pal, 0x408);', '(D32(memory, 4), 0x26) & 0x100))\n        table = AT(pal, 0x408);'),
        ('blit-source', 'D32(dc, 4), x, y, width, height, D32(memory, 4), source_x, source_y, rop);', 'D32(dc, 4), x, y, width, height, D32(memory, 4), source_y, source_x, rop);'),
        ('blit-null-dc', '(dc ? D32(dc, 4) : 0));\n    old', '(D32(dc, 4)));\n    old'),
        ('unused-end', 'for (at = bits; at <= end; ++at)', 'for (at = bits; at < end; ++at)'),
        ('unused-last', '} while (++colour < 0xff);', '} while (++colour < 0xfe);'),
        ('blur-start', '(uint8_t *)PTR(dib, 0x64) + (uint32_t)row * stride;\n    uint8_t *end', '(uint8_t *)PTR(dib, 0x64);\n    uint8_t *end'),
        ('blur-level', 'else if (SET(at + 3) || SET(down2 + mask_stride)) level = 2;', 'else if (SET(at + 3) || SET(down2 + mask_stride)) level = 3;'),
        ('blur-bound', '#define SET(pointer) ((pointer) <= mask_end && *(pointer))', '#define SET(pointer) ((pointer) < mask_end && *(pointer))'),
        ('blur-table', '0x204f0 + level + (uint32_t)*to * 6);', '0x204f0 + level + (uint32_t)*to * 5);'),
    ],
    'fonts': [
        ('font-size', 'mfc_operator_new(8);', 'mfc_operator_new(12);'),
        ('font-handle-zero', '        D32(font, 4) = 0;\n        PTR(font, 0) = IMAGE(VTABLE_FONT);', '        PTR(font, 0) = IMAGE(VTABLE_FONT);'),
        ('font-weight', '(height, 0, 0, 0, weight, 0, 0, 0, charset & 0xff,', '(height, 0, 0, 0, weight, 0, 0, 0, charset & 0x7f,'),
        ('font-quality', '4, 2, 2, 0, face);', '4, 2, 1, 0, face);'),
        ('font-index', '    index = FONT_COUNT;\n', '    index = FONT_COUNT - 1;\n'),
        ('add-font-order', '(name, 0), height,\n                                    charset.word, weight);', '(name, 0), weight,\n                                    charset.word, height);'),
        ('clear-direction', 'for (index = FONT_COUNT - 1; index >= 0; --index)', 'for (index = 0; index < FONT_COUNT; ++index)'),
        ('clear-null', '        if (font) VIRTUAL(', '        VIRTUAL('),
        ('clear-flag', '(font, 1);\n    }\n    CALL_THIS(MFC_6141', '(font, 0);\n    }\n    CALL_THIS(MFC_6141'),
        ('base-bound', 'if (index >= FONT_COUNT) return 0;', 'if (index > FONT_COUNT) return 0;'),
        ('exit-length', 'if (!length || length > 0x200) ERROR_TEXT[0] = 0;', 'if (!length || length >= 0x200) ERROR_TEXT[0] = 0;'),
        ('exit-break', '    if (length) fs98_simsui_append(ERROR_TEXT, LINE_BREAK);\n', ''),
        ('exit-type', '(0, ERROR_TEXT, 0, 0x1000);', '(0, ERROR_TEXT, 0, 0x10);'),
        ('remap-dialog-id', '(dialog, 0x7530, parent);', '(dialog, 0x7531, parent);'),
        ('remap-init-y', 'SCALE_Y = 0x1000000 / (rect[3] - rect[1]);', 'SCALE_Y = 0x1000000 / (rect[2] - rect[1]);'),
        ('remap-null', '    if (control) {\n        void *parent;', '    if (1) {\n        void *parent;'),
        ('remap-second', '(D32(parent, 0x20), rect + 2);', '(D32(parent, 0x20), rect + 1);'),
        ('remap-shift', 'rect[3] = (SCALE_Y * rect[3]) >> 16;', 'rect[3] = (SCALE_Y * rect[3]) >> 15;'),
        ('remap-axis', 'rect[2] = (SCALE_X * rect[2]) >> 16;', 'rect[2] = (SCALE_Y * rect[2]) >> 16;'),
        ('point-null', '    if (!control)\n        CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(rect, point[0], point[1],\n                                                                                     point[0], point[1]);',
         '    CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(rect, point[0], point[1],\n                                                                                     point[0], point[1]);'),
        ('width-right', '*width + point[0], point[1]);', '*width, point[1]);'),
        ('width-out', '*width = rect[2] - rect[0];', '*width = rect[2] - rect[1];'),
        ('main-instance', '        MAIN_INSTANCE = instance;\n        INSTANCE = instance;', '        INSTANCE = instance;'),
        ('main-library', 'if (library) CALL_THIS(MFC_342, void *, (void *, void *, int32_t))(library, EXTENSION, 0);',
         'CALL_THIS(MFC_342, void *, (void *, void *, int32_t))(library, EXTENSION, 0);'),
        ('main-font-order', 'fs98_simsui_add_font(0xf836); fs98_simsui_add_font(0xf833);', 'fs98_simsui_add_font(0xf833); fs98_simsui_add_font(0xf836);'),
        ('main-scale', 'if (!SCALE_Y) SCALE_Y = 0x10000;', 'if (!SCALE_Y) SCALE_Y = 0x1000;'),
        ('main-detach', '        MAIN_INSTANCE = 0;\n        fs98_simsui_clear_fonts();', '        fs98_simsui_clear_fonts();'),
        ('raw-chain', 'D32(state, 0x28) = D32(state, 0x2c);', 'D32(state, 0x28) = D32(state, 0x20);'),
    ],
    'crt': [
        ('vector-order', 'element -= size;\n        destroy(element);', 'destroy(element);\n        element -= size;'),
        ('vector-count', 'while (--count >= 0)', 'while (--count > 0)'),
    ],
    'util': [
        ('ui-new-pool', '))(5, size, &result, 1);', '))(4, size, &result, 1);'),
        ('ui-new-flag', '))(5, size, &result, 1);', '))(5, size, &result, 0);'),
        ('ui-delete-slot', 'PTR(HOST_TABLE, 0x214)', 'PTR(HOST_TABLE, 0x210)'),
        ('find-first', '    if (character != wanted_char)\n', '    if (character != wanted_char || 1)\n'),
        ('find-end', 'return *at == wanted_char ? at : 0;', 'return at;'),
        ('token-null', '    if (!text) return 0;\n    found', '    found'),
        ('token-rest', '!= 0) {}\n        return 0;', '!= 0) {}\n        return source;'),
        ('token-no-out', '    if (out) {\n        from', '    if (1) {\n        from'),
        ('token-prev', 'if (from != text) to = CALL_STD', 'if (from != next) to = CALL_STD'),
        ('token-terminate', '        *to = 0;\n    }\n    return next;', '    }\n    return next;'),
        ('name-separator', "fs98_simsui_next_token((char *)spec, buffer, '#');", "fs98_simsui_next_token((char *)spec, buffer, ',');"),
        ('name-release', '    CALL_THIS(MFC_5572, void, (void *, int32_t))(name, -1);\n    return next;',
         '    CALL_THIS(MFC_5572, void, (void *, int32_t))(name, 0);\n    return next;'),
        ('point-check', '    point[0] = ATOI(token);\n    fs98_simsui_next_token(next, token', '    point[0] = ATOI(token);\n    fs98_simsui_next_token(spec, token'),
        ('point-and-third', '    *value = ATOI(token);\n    return 1;\n}\n/* 0x20471d70', '    *value = 0;\n    return 1;\n}\n/* 0x20471d70'),
        ('rect-order', '    rect[2] = ATOI(token);\n    fs98_simsui_next_token(next, token, \',\');\n    rect[3] = ATOI(token);\n    return 1;',
         '    rect[3] = ATOI(token);\n    fs98_simsui_next_token(next, token, \',\');\n    rect[2] = ATOI(token);\n    return 1;'),
        ('rect-and-check', '    rect[3] = ATOI(token);\n    fs98_simsui_next_token(next, token, \',\');\n    *value',
         '    rect[3] = ATOI(token);\n    next = fs98_simsui_next_token(next, token, \',\');\n    if (!next) return 0;\n    *value'),
        ('font-instance', '(INSTANCE, id, buffer, 0x100)', '(INSTANCE, id, buffer, 0xff)'),
        ('font-error-text', 'IMAGE(LOAD_ERROR), 0, 0)', 'IMAGE(LOAD_ERROR), 0, 1)'),
        ('font-byte', '*b = (uint8_t)ATOI(token);', '*(int32_t *)b = ATOI(token);'),
        ('font-buffer-zero', '(text, 0), buffer, \',\');', '(text, 0x100), buffer, \',\');'),
        ('font-no-free', '    if (!next) {\n        CALL_THIS(MFC_800, void, (void *))(text);\n', '    if (!next) {\n'),
    ],
    'dib': [
        ('pal-create-version', 'D16(logical, 0) = 0x300;', 'D16(logical, 0) = 0x301;'),
        ('pal-create-swap', '        entry[0] = colour[2];\n        entry[1] = colour[1];', '        entry[0] = colour[0];\n        entry[1] = colour[1];'),
        ('pal-create-plain', '*plain++ = D32(colour, 0);', '*plain++ = D32(colour, 0) & 0xffffff;'),
        ('pal-create-free', '    CALL_C(free, void, (void *))(logical);\n    return result;', '    return result;'),
        ('pal-count-read', 'return read ? (int32_t)count : 0;', 'return (int32_t)count;'),
        ('pal-draw-row', 'bottom = rect[3] * (row + 1) / 16 + 1;', 'bottom = rect[3] * (row + 1) / 16;'),
        ('pal-draw-index', '((((uint32_t)row << 4) + column) & 0xffff) | 0x1000000)', '((((uint32_t)column << 4) + row) & 0xffff) | 0x1000000)'),
        ('pal-draw-restore', '    if (old) CALL_THIS(MFC_5791', '    if (0) CALL_THIS(MFC_5791'),
        ('pal-draw-left', 'right, bottom);\n            CALL_THIS(MFC_5787', 'right, bottom + 1);\n            CALL_THIS(MFC_5787'),
        ('static-flags', 'for (index = 0; index < 0xec; ++index) entries[0x2b + index * 4] = 4;', 'for (index = 0; index < 0xeb; ++index) entries[0x2b + index * 4] = 4;'),
        ('static-last', 'fs98_simsui_copy(entries + 0x3d8, last, 0x28);', 'fs98_simsui_copy(entries + 0x3d4, last, 0x28);'),
        ('tables-high', 'if (!(high > (double)blue)) high = blue;', 'if (!(high >= (double)blue)) high = green;'),
        ('tables-threshold', 'else if (lightness > k65 && lightness < k144) chosen = colour_248;', 'else if (lightness >= k65 && lightness < k144) chosen = colour_248;'),
        ('tables-pick', 'else if (lightness > k177 && lightness < k216) chosen = colour_7;', 'else if (lightness > k177 && lightness < k216) chosen = colour_255;'),
        ('nearest-strict', '            if (distance < best) { best = distance; chosen = index; }\n        }\n    } else {', '            if (distance <= best) { best = distance; chosen = index; }\n        }\n    } else {'),
        ('nearest-plain', 'SQUARE(green - entry[1]) + SQUARE(red - entry[2]) + SQUARE(blue - entry[0]);', 'SQUARE(green - entry[1]) + SQUARE(red - entry[0]) + SQUARE(blue - entry[2]);'),
        ('nearest-range', '        for (index = 10; index < 0xf6; ++index) {\n            const uint8_t *entry = entries', '        for (index = 10; index < 0xf5; ++index) {\n            const uint8_t *entry = entries'),
        ('table-unused-order', 'if (active) CALL_STD', 'if (foreground) CALL_STD'),
        ('table-flag', 'if (!D32(pal, 0x808) &&', 'if (!D32(pal, 0x80c) &&'),
        ('table-caps', '(D32(dc, 8), RASTERCAPS_)', '(D32(dc, 4), RASTERCAPS_)'),
        ('table-palette', "table = AT(pal, 0x408);", "table = AT(pal, 8);"),
        ('table-count', '(handle, 0, 0x100, table)', '(handle, 0, 0xff, table)'),
        ('pal-construct-handle', '    D32(pal, 4) = 0;\n    PTR(pal, 0) = IMAGE(VTABLE_PAL);', '    PTR(pal, 0) = IMAGE(VTABLE_PAL);'),
        ('pal-destroy-vtable', '    PTR(pal, 0) = IMAGE(VTABLE_OBJECT);\n}', '}'),
        ('object-delete-flag', '    PTR(object, 0) = IMAGE(VTABLE_OBJECT);\n    if (flags & 1)', '    PTR(object, 0) = IMAGE(VTABLE_OBJECT);\n    if (flags & 2)'),
        ('gdi-delete-destroy', '    fs98_simsui_pal_destroy(object);\n    if (flags & 1) mfc_operator_delete(object);',
         '    if (flags & 1) mfc_operator_delete(object);'),
        ('pal-delete-array-size', 'PAL_SIZE, I32(pal, -4)', 'PAL_SIZE - 4, I32(pal, -4)'),
        ('pal-delete-array-block', 'mfc_operator_delete((uint8_t *)pal - 4);', 'mfc_operator_delete(pal);'),
        ('pal-delete-single', 'if (flags & 1) fs98_simsui_ui_delete(pal);', 'if (flags & 1) mfc_operator_delete(pal);'),
        ('dib-handle-offset', 'return bitmap ? D32(bitmap, 4) : 0;', 'return bitmap ? D32(bitmap, 0) : 0;'),
        ('dib-width-bytes', '(D32(dib, 8) + 3) & 0xfffffffc);', '(D32(dib, 8) + 4) & 0xfffffffc);'),
        ('dib-valid-low', 'if ((uint32_t)(uintptr_t)pointer < bits) return 0;', 'if ((uint32_t)(uintptr_t)pointer <= bits) return 0;'),
        ('dib-valid-high', '* D32(dib, 0xc) + bits - 1) return 0;', '* D32(dib, 0xc) + bits) return 0;'),
        ('info-core', 'return D32(info, 0) != 0xc;', 'return D32(info, 0) != 0x28;'),
        ('colours-bits', '? D16(info, 0xe) : D16(info, 0xa);', '? D16(info, 0xe) : D16(info, 0xe);'),
        ('colours-four', 'case 4: count = 0x10; break;', 'case 4: count = 0x0f; break;'),
        ('colours-used', 'if (fs98_simsui_info_is_header(info) && D32(info, 0x20))', 'if (D32(info, 0x20))'),
    ],
}


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    only = set(sys.argv[2:])
    if count < 100:
        raise ValueError('use at least 100 scenarios')
    directory = Path('bin/simsui-diff/breaks')
    directory.mkdir(parents=True, exist_ok=True)
    breaks = [(name, unit, before, after) for unit, entries in BREAKS.items() for name, before, after in entries]
    if only:
        breaks = [entry for entry in breaks if entry[0] in only or entry[1] in only]
    caught = 0
    for name, unit, before, after in breaks:
        path = f'src/simsui/simsui_{unit}.c'
        original = Path(path).read_text()
        if original.count(before) != 1:
            raise ValueError(f'{name}: the anchor occurs {original.count(before)} times')
        source = directory / f'{name}.c'
        executable = directory / f'{name}.exe'
        source.write_text(original.replace(before, after, 1))
        executable.unlink(missing_ok=True)
        others = [other for other in UNITS if other != path]
        build = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Wno-unused',
            '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-fno-tree-loop-distribute-patterns',
            '-fno-tree-vectorize', '-Isrc/simsui',
            '-Wl,--image-base=0x400000', '-o', str(executable),
            'tools/difftest/simsuidiff.c', str(source), *others],
            text=True, capture_output=True)
        if build.returncode:
            print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
            continue
        try:
            run = subprocess.run(['wine', str(executable), str(count), '0x51c5e1'],
                                 text=True, errors='replace', capture_output=True, timeout=600)
        except subprocess.TimeoutExpired:
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            continue
        output = run.stdout + run.stderr
        (directory / f'{name}.log').write_text(output)
        if run.returncode == 1 and 'DIFFER scenario=' in output:
            caught += 1
            print(f'{name}: caught', flush=True)
        else:
            print(f'{name}: NOT CAUGHT (exit {run.returncode})\n{output[-600:]}', flush=True)
    print(f'SIMSUI breaks: {caught}/{len(breaks)} caught', flush=True)
    return 0 if caught == len(breaks) else 1


if __name__ == '__main__':
    sys.exit(main())
