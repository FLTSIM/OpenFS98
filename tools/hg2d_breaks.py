#!/usr/bin/env python3
"""Build isolated, deliberately wrong HG2D DLLs and require detection.

Run from the repository root after make -f tools/hg2d.mk hg2d.
Does not edit the source or the baseline DLL. Wine needs normal host access.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
source = (root / 'src/hg2d/hg2d.c').read_text()
for section in ('palette', 'draw', 'polygon', 'fill'):
    source = source.replace('#include "hg2d_%s.inc"' % section, (root / ('src/hg2d/hg2d_%s.inc' % section)).read_text())
build = root / 'bin/hg2d-breaks'
build.mkdir(parents=True, exist_ok=True)
shutil.copyfile(root / 'bin/hg2d-diff/hg2ddiff.exe', build / 'hg2ddiff.exe')
shutil.copyfile(root / 'orig/Modules/HG2D.DLL', build / 'HG2D_retail.dll')
shutil.copyfile(root / 'orig/Modules/UTIL.DLL', build / 'UTIL_retail.dll')
mutations = [
    ('descriptor width', 'descriptor[1] != 0x100', 'descriptor[1] != 0xff'),
    ('descriptor kind', 'descriptor[0] != 1', 'descriptor[0] != 0'),
    ('create flags', 'flags != 0)', 'flags > 1)'),
    ('last pool slot', 'offset < POOL_END;', 'offset < POOL_END - 0x34;'),
    ('palette byte', 'pixels[index] * 2]', 'pixels[index] * 2 + 1]'),
    ('palette last pixel', 'index < 0x10000;', 'index < 0xffff;'),
    ('FE result', 'if (!load(', 'if (load('),
    ('side texture flag', 'texture[1] |= 0x100000;', 'texture[1] |= 0x200000;'),
    ('load flags byte', '(flags ? 0xff00u : 0)', '(flags ? 0xfe00u : 0)'),
    ('destroy ownership', '!(flags & 1)', '!(flags & 2)'),
    ('destroy first flags', 'texture[0] = texture[1] = texture[2] = texture[3] = 0;', 'texture[0] = texture[2] = texture[3] = 0;'),
    ('access flag', 'texture[1] |= 0x10000;', 'texture[1] |= 0x20000;'),
    ('unaligned handle', 'return handle >= ADDRESS', 'return (handle % 4 == ADDRESS(hg2d_image + POOL) % 4) && handle >= ADDRESS'),
    ('update clear accessed', 'texture[1] &= ~0x10000u;', 'texture[1] &= ~0x20000u;'),
    ('update clear hardware', '(texture[1] & ~0x40000u) | 0x20000', '(texture[1] & ~0x80000u) | 0x20000'),
    ('update low eax', '(flags & 0xff);', '(flags & 0xffff);'),
    ('hardware accounting', 'WORD(0x8800 + index * 4)--;', 'WORD(0x8800 + index * 4)++;'),
    ('hardware scratch', 'index < 8; index++)\n            WORD(0x8820', 'index < 7; index++)\n            WORD(0x8820'),
    ('hardware tail', 'texture[3] = texture[2] = texture[7] = 0;', 'texture[3] = texture[2] = 0;'),
]
mutations = [(*mutation, 'textures') for mutation in mutations]
colour_mutations = [
    ('direct colour 69', 'else if (mode != 0x69 &&', 'else if (mode != 0x68 &&'),
    ('direct colour e0', 'mode != 0xe0)', 'mode != 0xe1)'),
    ('legacy colour bank', 'mode == 0x67 ? 0x34af : 0x349f', 'mode == 0x67 ? 0x349f : 0x34af'),
    ('colour nibble', 'index = colour & 15;', 'index = colour & 7;'),
    ('gradient cutoff', 'pair <= 0x1800', 'pair < 0x1800'),
    ('gradient selector', 'BYTE(0x3402 + index) == 0', 'BYTE(0x3402 + index) != 0'),
    ('gradient delta wrap', '(uint8_t)(BYTE(0x3401 + index) - BYTE(0x3400 + index))', '(uint16_t)(BYTE(0x3401 + index) - BYTE(0x3400 + index))'),
    ('gradient partial doubling', '(product >> 8) + ((product >> 7) & 1)', '(product >> 7)'),
    ('gradient pair', 'pair = HALF(0x3400 + index);', 'pair = HALF(0x3401 + index);'),
    ('current colour', 'BYTE(0x90c) = value;', 'BYTE(0x90c) = 0;'),
    ('second output word', 'out[1] = mapped_pair;', 'out[1] = 0;'),
    ('remap return width', 'return (entry_eax & 0xffff0000u) | result;', 'return result;'),
    ('remap high byte', '((colour >> 8) & 0xff) * 2)', '((colour >> 8) & 0xff) * 2 + 1)'),
    ('layout signed level', '(int16_t)level >= (int16_t)group', 'level >= group'),
    ('layout stride', '& 0xffff) / 32;', '& 0xffff) / 31;'),
    ('layout first run', 'index < 32; index++) {\n        HALF', 'index < 31; index++) {\n        HALF'),
    ('layout group boundary', '(int16_t)level >= (int16_t)(group + 8)', '(int16_t)level > (int16_t)(group + 8)'),
    ('layout distinct zero', 'HALF(0x34c2)++;', 'HALF(0x34c2) += 0;'),
]
mutations += [(*mutation, 'colours') for mutation in colour_mutations]
palette_mutations = [
    ('palette ramp source', 'BYTE(source + 0x5d) << 8', 'BYTE(source + 0x5c) << 8'),
    ('palette destination offset', '(int16_t)product >> 1', '(int16_t)product >> 2'),
    ('palette average carry', '(value + BYTE(*source + channel + 3)) >> 1', '(uint8_t)(value + BYTE(*source + channel + 3)) >> 1'),
    ('palette final run', 'palette_copy(&source, &destination, 143, 0);', 'palette_copy(&source, &destination, 142, 0);'),
    ('palette packed group', 'if (level >= (int16_t)group)', 'if (level > (int16_t)group)'),
    ('palette ramp reserved colours', 'index < 0x74 || index >= 0x80', 'index < 0x74 || index >= 0x7f'),
    ('palette ramp divisor', 'palette_brightness(index, 14)', 'palette_brightness(index, 13)'),
    ('palette shade divisor', 'palette_brightness(index, 16)', 'palette_brightness(index, 15)'),
    ('palette shade generation', 'BYTE(0x4034) ^= 0xa5;', 'BYTE(0x4034) ^= 0xa4;'),
    ('palette inverse word index', 'BYTE(0xa4e + colour) = value;', 'BYTE(0xa4e + (uint8_t)colour) = value;'),
    ('palette shade word index', 'BYTE(0xb4e + colour) = (uint8_t)result;', 'BYTE(0xb4e + (uint8_t)colour) = (uint8_t)result;'),
    ('palette mode notification', 'send(0x14, 0x70, 0);', '(void)send;'),
    ('palette green channel', 'green = BYTE(0x30a1 + index * 3)', 'green = BYTE(0x30a0 + index * 3)'),
    ('night threshold', 'uint8_t night = brightness < 0x80;', 'uint8_t night = brightness < 0x7f;'),
    ('night override', 'if (!(GLOBAL_HALF(0xc46) & 0x20))', 'if (!(GLOBAL_HALF(0xc46) & 0x10))'),
    ('ramp disabled mode', 'if (flag == 1)', 'if (flag == 2)'),
    ('palette cache carry', 'hg2d_palette_changed = 0;', 'hg2d_palette_changed = 1;'),
    ('palette colour source', 'HALF(0x3fc2) ? 0x33d0 : 0x33a0', 'HALF(0x3fc2) ? 0x33a0 : 0x33d0'),
    ('palette scale divisor', 'BYTE(0x30a0 + index) * brightness) >> 8', 'BYTE(0x30a0 + index) * brightness) >> 7'),
    ('palette count clamp', 'count > 64) count = 64;', 'count > 64) count = 63;'),
    ('palette red saturation', 'sum > 255 ? 255 : sum', 'sum > 255 ? 0 : sum'),
    ('palette sky ramp end', 'if (!value) value--;', 'if (!value) value++;'),
    ('palette hardware-count mode', 'if (!WORD(0x4005) || count > 8)', 'if (!WORD(0x4005) || count >= 8)'),
    ('palette realized dirty flag', 'HALF(0x36c0) = 0;', 'HALF(0x36c0) = 1;'),
    ('palette realize high colours', 'GLOBAL_HALF(0xcbe) == 2', 'GLOBAL_HALF(0xcbe) == 1'),
    ('palette realized full source', 'palette_pack(0x30a0, 0x3cc2);', 'palette_pack(0x36c2, 0x3cc2);'),
    ('palette selector low word', 'HALF(0x3fc2) != (uint16_t)selector', 'HALF(0x3fc2) != selector'),
]
mutations += [(*mutation, 'palette') for mutation in palette_mutations]
draw_mutations = [
    ('point pixel', '*pixel = draw_colour();', '(void)draw_colour();'),
    ('point row table', 'rows + y * 4', 'rows + y * 8'),
    ('drawing colour mode', 'colour = (uint16_t)(0x6700 | (colour & 0xff));', 'colour = (uint16_t)(0x6900 | (colour & 0xff));'),
    ('horizontal row boundary', '\n    if (y >= SURFACE_WORD(surface, 8)) return;', '\n    if (y > SURFACE_WORD(surface, 8)) return;'),
    ('horizontal negative word', '    uint8_t *pixel;\n    if (y >= SURFACE_WORD(surface, 8)) return;\n    if ((int16_t)first < 0 || (int16_t)last < 0) {\n        if ((int16_t)first < 0) first = 0;\n        if ((int16_t)last < 0) last = 0;', '    uint8_t *pixel;\n    if (y >= SURFACE_WORD(surface, 8)) return;\n    if ((int32_t)first < 0 || (int32_t)last < 0) {\n        if ((int16_t)first < 0) first = 0;\n        if ((int16_t)last < 0) last = 0;'),
    ('horizontal negative clamp', '    uint8_t *pixel;\n    if (y >= SURFACE_WORD(surface, 8)) return;\n    if ((int16_t)first < 0 || (int16_t)last < 0) {\n        if ((int16_t)first < 0) first = 0;\n        if ((int16_t)last < 0) last = 0;', '    uint8_t *pixel;\n    if (y >= SURFACE_WORD(surface, 8)) return;\n    if ((int16_t)first < 0 || (int16_t)last < 0) {\n        if ((int16_t)first < 0) first = 0;\n        if ((int16_t)last < 0) last = 1;'),
    ('horizontal endpoint', 'count = last - first + 1;', 'count = last - first;'),
    ('vertical direction', 'start = last;', 'start = first;'),
    ('vertical endpoint', 'count = delta + 1;', 'count = delta;'),
    ('line endpoint', 'index < length; index++', 'index <= length; index++'),
    ('line error half', 'error = -(int32_t)(length >> 1);', 'error = -(int32_t)(length >> 2);'),
    ('line negative x order', 'if ((int32_t)width < 0) {', 'if ((int32_t)width > 0) {'),
    ('line coincident point', 'draw_point_body(surface, x, y);', '(void)surface;'),
    ('line error tie', 'if (error >= 0)', 'if (error > 0)'),
    ('frame zero dimensions', 'if (!width || !height) return;', 'if (!width && !height) return;'),
    ('frame right edge', 'draw_vertical_body(surface, x + width - 1, y, y + height - 1);', 'draw_vertical_body(surface, x + width, y, y + height - 1);'),
    ('depth signed threshold', '(int16_t)depth < (int16_t)threshold', 'depth < threshold'),
    ('depth threshold equality', '(int16_t)depth < (int16_t)threshold', '(int16_t)depth <= (int16_t)threshold'),
    ('depth bucket maximum', 'if (value >= 15) value = 15;', 'if (value >= 15) value = 14;'),
    ('depth scale', 'draw_divide_word((uint32_t)value * maximum, scale)', 'draw_divide_word((uint32_t)value * scale, maximum)'),
    ('depth dither low byte', 'GLOBAL_HALF(0x127e) & 0xff', 'GLOBAL_HALF(0x127e) & 0xffff'),
    ('depth dither right edge', '(int16_t)(x + 1) < (int16_t)SURFACE_WORD(surface, 4)', '(int16_t)(x + 1) <= (int16_t)SURFACE_WORD(surface, 4)'),
    ('depth dither above', '(int16_t)(y - 1) >= 0', '(int16_t)(y - 1) > 0'),
    ('distance bucket', 'if (value > 15) value = 15;', 'if (value > 15) value = 14;'),
    ('fixed complement division', 'magnitude = minor ^ sign;', 'magnitude = (minor ^ sign) - sign;'),
    ('fixed first y fraction', '(y & 0xffff) ^ 0xffff', '(y & 0xffff) ^ 0xfffe'),
    ('fixed count word', 'count = (count - 1) & 0xffff;', 'count = (count + 1) & 0xffff;'),
    ('fixed small count', 'if ((int16_t)count <= 0) break;', 'if ((int16_t)count < 0) break;'),
    ('fixed negative carry', 'if ((int32_t)slope < 0) step_y--;', 'if ((int32_t)slope < 0) step_y++;'),
    ('text substitution', 'if (code == 0xe1) code = 0xa6;', 'if (code == 0xe1) code = 0xa7;'),
    ('text second substitution', 'if (code == 0xfd) code = 0xbd;', 'if (code == 0xfd) code = 0xbc;'),
    ('text centering', 'WORD(0x29b0) -= total >> 1;', 'WORD(0x29b0) -= total >> 2;'),
    ('text vertical centering', 'WORD(0x29b4) -= 6;', 'WORD(0x29b4) -= 5;'),
    ('text horizontal gate', '(int32_t)(x + 8) < (int32_t)WORD(0x29b8)', '(int32_t)(x + 8) <= (int32_t)WORD(0x29b8)'),
    ('text vertical gate', '(int32_t)(y + 12) < (int32_t)WORD(0x29bc)', '(int32_t)(y + 12) <= (int32_t)WORD(0x29bc)'),
    ('text descender', 'if (BYTE(glyph) & 0x80)', 'if (BYTE(glyph) & 0x40)'),
    ('text last row', 'row < 10; row++', 'row < 9; row++'),
    ('text bitmap direction', 'bits & (0x80 >> column)', 'bits & (1 << column)'),
    ('text advance', 'WORD(0x29b0) += width + 1;', 'WORD(0x29b0) += width;'),
    ('text pointer', 'WORD(0x29cc)++;', 'WORD(0x29cc) += 2;'),
    ('flush dirty flag', 'HALF(0x36c0) ? palette_realize() : entry_eax', 'HALF(0x36c0) ? entry_eax : entry_eax'),
    ('span remap frame fault', '__asm__ volatile("movl 0, %%eax" ::: "eax", "memory");', '(void)map;'),
    ('span block load ordering', 'values[0] = map[values[0]];', 'values[0] = map[values[0]]; pixel[0] = values[0];'),
    ('draw edx leftover', 'movb 0xcc6(%eax),%dl', 'movb 0xcc5(%eax),%dl'),
]
draw_mutations += [
    ('span zero fill count', 'do { *pixel++ = BYTE(0x90c); } while (--count);', 'while (count--) *pixel++ = BYTE(0x90c);'),
    ('span zero map count', 'int short_span = count < 16;', 'int short_span = count && count < 16;'),
    ('depth divide saturation', 'quotient > 0xffff ? 0xffff : quotient', 'quotient > 0xffff ? 0 : quotient'),
    ('depth zero divide', 'if (!denominator) return 0xffff;', 'if (!denominator) return 0;'),
    ('distance zero divide', 'threshold ? distance / threshold : 0xffffffffu', 'threshold ? distance / threshold : 0'),
    ('distance threshold boundary', 'threshold < 0x7d00 &&', 'threshold <= 0x7d00 &&'),
]
mutations += [(*mutation, 'drawing') for mutation in draw_mutations]
polygon_mutations = [
    ('projection view load order', 'polygon_project(x, scale_x, depth) + (offset_x << 16)', 'polygon_project(x, view[0], depth) + (view[1] << 16)'),
    ('projection scale shift', '(int32_t)(scale << 16)', '(int32_t)(scale << 15)'),
    ('projection signed depth', 'product / (int32_t)depth', 'product / depth'),
    ('projection zero repair sign', '((product < 0) != ((int32_t)depth < 0))', '((product < 0) == ((int32_t)depth < 0))'),
    ('projection minimum quotient', 'quotient >= -2147483648LL', 'quotient > -2147483648LL'),
    ('projection positive saturation', '? 0x80000001u : 0x7fffffffu', '? 0x80000001u : 0x7ffffffeu'),
    ('projection x offset', '(offset_x << 16)', '(offset_x << 15)'),
    ('projection y offset', '(offset_y << 16)', '(offset_y << 15)'),
    ('vertex count', 'GLOBAL_WORD(0x2f0)++;', 'GLOBAL_WORD(0x2f0) += 2;'),
    ('vertex first position', 'WORD(0x8f8) = WORD(0x900) = x;', 'WORD(0x8f8) = WORD(0x900) = x + 1;'),
    ('vertex depth minimum', '(int32_t)depth < (int32_t)WORD(0x8d0)', 'depth < WORD(0x8d0)'),
    ('vertex depth maximum', '(int32_t)depth > (int32_t)WORD(0x8d4)', 'depth > WORD(0x8d4)'),
    ('vertex capacity equality', '+ 0x60 >= ADDRESS(hg2d_image + 0x884)', '+ 0x60 > ADDRESS(hg2d_image + 0x884)'),
    ('vertex capacity reservation', '+ 0x60 >= ADDRESS(hg2d_image + 0x884)', '+ 0x10 >= ADDRESS(hg2d_image + 0x884)'),
    ('vertex minimum y unsigned', 'if (y < WORD(0x8c8))', 'if ((int32_t)y < (int32_t)WORD(0x8c8))'),
    ('vertex maximum y signed', '(int32_t)y > (int32_t)WORD(0x8cc)', 'y > WORD(0x8cc)'),
    ('vertex edge ordering', '(int32_t)y < (int32_t)previous_y', 'y < previous_y'),
    ('vertex texture flag width', 'GLOBAL_HALF(0xc90) && GLOBAL_HALF(0x2f4)', 'GLOBAL_WORD(0xc90) && GLOBAL_WORD(0x2f4)'),
    ('edge flat rounding', 'edge[2] = low_x + 0x8000;', 'edge[2] = low_x;'),
    ('edge flat delta', 'edge[3] = high_x - low_x;', 'edge[3] = low_x - high_x;'),
    ('edge scanline fraction', '(low_y & 0xffff) ^ 0xffff', '(low_y & 0xffff) ^ 0xfffe'),
    ('edge pointer advance', 'GLOBAL_WORD(0xc14) = ADDRESS(edge) + 16;', 'GLOBAL_WORD(0xc14) = ADDRESS(edge) + 20;'),
]
mutations += [(*mutation, 'polygons') for mutation in polygon_mutations]
fill_mutations = [
    ('fill remap threshold', '(int32_t)mode < 16', '(int32_t)mode <= 16'),
    ('fill remap signed mode', '(int32_t)mode < 16', 'mode < 16'),
    ('fill remap limit equality', '(int32_t)mode > (int32_t)WORD(0x298c)', '(int32_t)mode >= (int32_t)WORD(0x298c)'),
    ('fill remap fallback bank', '0xd70 + 3 * 256', '0xd70 + 2 * 256'),
    ('fill remap alternate bank', 'hg2d_image + 0x1d8c', 'hg2d_image + 0x1d88'),
    ('fill remap colour override', 'GLOBAL_WORD(0x3c0) = 0x6900;', 'GLOBAL_WORD(0x3c0) = 0x6901;'),
    ('fill stipple selector', 'stipple = colour >> 8 == 0x68;', 'stipple = colour >> 8 == 0x67;'),
    ('fill stipple direct colour', 'if (stipple) colour += 0x100;', 'if (stipple) colour += 0x200;'),
    ('fill colour pair', 'colour_translate(colour, GLOBAL_HALF(0xcc8), &pair)', 'colour_translate(colour, GLOBAL_HALF(0xcc2), &pair)'),
    ('fill distance shade', 'BYTE(0x90c) = draw_distance((uint8_t)colour);', 'BYTE(0x90c) = (uint8_t)colour;'),
    ('fill shadow sign', '(int16_t)GLOBAL_HALF(0xb34) < 0', '(int16_t)GLOBAL_HALF(0xb34) > 0'),
    ('fill shadow map selector', 'HALF(0x94c) = GLOBAL_HALF(0xdce);', 'HALF(0x94c) = 0;'),
    ('fill closing order', 'if (first_y < last_y)', 'if ((int32_t)first_y < (int32_t)last_y)'),
    ('fill closing x', 'polygon_edge(last_x, last_y, first_x, first_y);', 'polygon_edge(first_x, last_y, last_x, first_y);'),
    ('fill min row shift', '(int32_t)WORD(0x8c8) >> 16', '(int32_t)WORD(0x8c8) >> 15'),
    ('fill max row shift', '(int32_t)WORD(0x8cc) >> 16', '(int32_t)WORD(0x8cc) >> 15'),
    ('fill slow flag width', 'GLOBAL_HALF(0x2f4) ||', 'GLOBAL_WORD(0x2f4) ||'),
    ('fill triangle cutoff', '(int32_t)GLOBAL_WORD(0x2f0) < 3', '(int32_t)GLOBAL_WORD(0x2f0) < 2'),
    ('fill last scanline', 'while (WORD(0x4020) >= y);', 'while (WORD(0x4020) > y);'),
    ('fill extra edge record', 'index < WORD(0x4028) + 2;', 'index < WORD(0x4028) + 1;'),
    ('fill edge starting row', '(uint16_t)edge[0] > (uint16_t)y', '(uint16_t)edge[0] >= (uint16_t)y'),
    ('fill intersection word', '(uint16_t)(edge[2] >> 16)', '(uint16_t)(edge[2] >> 15)'),
    ('fill fast edge advance', 'edge[2] += edge[3];', 'edge[2] -= edge[3];'),
    ('fill fast negative clamp', 'if ((int32_t)edge[2] < 0) edge[2] = 0;', 'if ((int32_t)edge[2] < 0) edge[2] = 1;'),
    ('fill pair count', 'pairs = used / 2;', 'pairs = (used + 1) / 2;'),
    ('fill large signed sort', 'value <= (int16_t)intersections[index]', 'value >= (int16_t)intersections[index]'),
    ('fill large sort tail', 'end = pairs * 2 - 1;', 'end = pairs * 2 - 2;'),
    ('fill flat negative sum', 'if ((int32_t)last < 0) last = 0;', 'if ((int32_t)last < 0) last = 0x10000;'),
    ('fill flat endpoint', 'first >> 16, last >> 16, y', '(first >> 16) + 1, last >> 16, y'),
    ('fill walker max row flat', 'if (y == WORD(0x8cc)) polygon_flat_span', 'if (y != WORD(0x8cc)) polygon_flat_span'),
    ('fill walker left end', '(int32_t)y >= (int32_t)left[1]', '(int32_t)y > (int32_t)left[1]'),
    ('fill walker right end', '(int32_t)y >= (int32_t)right[1]', '(int32_t)y > (int32_t)right[1]'),
    ('fill walker left advance', 'left[2] += left[3];', 'left[2] -= left[3];'),
    ('fill walker right advance', 'right[2] += right[3];', 'right[2] -= right[3];'),
    ('fill stipple parity', '(y ^ first) & 1', '(y ^ first ^ 1) & 1'),
    ('fill stipple sole sample', 'count -= phase;', 'count -= phase; if (!count) return;'),
    ('fill shadow cache', 'if (factor == BYTE(0x4034)) return;', 'if (factor == BYTE(0x4034)) {}'),
    ('fill shadow byte factor', '(int8_t)factor * 2', '(uint8_t)factor * 2'),
    ('fill shadow ramp cutoff', 'if (value < 32) {', 'if (value <= 32) {'),
    ('fill shadow ramp doubling', '(int8_t)factor * 2', '(int8_t)factor'),
    ('fill shadow last pixel', 'if ((int32_t)count > 0) *pixel = map[*pixel];', 'if ((int32_t)count >= 0) *pixel = map[*pixel];'),
    ('fill shadow odd singleton', 'if ((int32_t)count <= 0) goto shadow_tail;', 'if ((int32_t)count <= 0) return;'),
    ('fill remap frame preservation', 'draw_map_pixels(draw_address(surface, first, y), last - first + 1, POINTER(WORD(0x1d88)));', 'draw_map_span(draw_address(surface, first, y), last - first + 1);'),
]
mutations += [(*mutation, 'fills') for mutation in fill_mutations]
if len(sys.argv) > 2 and sys.argv[2] not in {suite for _, _, _, suite in mutations}:
    raise SystemExit('Unknown mutation family: ' + sys.argv[2])
missed = []
tested = 0
for name, old, new, suite in mutations:
    if len(sys.argv) > 2 and sys.argv[2] != suite:
        continue
    tested += 1
    assert source.count(old) == 1, (name, source.count(old))
    variant = build / 'hg2d_mutant.c'
    variant.write_text(source.replace(old, new))
    library = build / 'HG2D_ours.dll'
    library.unlink(missing_ok=True)
    subprocess.run([
        'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra',
        '-fno-strict-aliasing', '-shared', '-nostdlib', '-Wl,--entry=_DllMain@12',
        '-Wl,--image-base=0x30000000', '-Wl,--disable-dynamicbase',
        '-I' + str(root / 'src/hg2d'), '-o', str(library), str(variant),
        str(root / 'src/hg2d/hg2d.def'), '-lgcc'], check=True)
    result = subprocess.run(['wine', 'hg2ddiff.exe', sys.argv[1] if len(sys.argv) > 1 else '64',
                             suite, 'full' if suite == 'colours' else 'scripts'],
                            cwd=build, env={**os.environ, 'WINEDEBUG': '-all'},
                            capture_output=True, text=True, timeout=120)
    # Only a reported differential mismatch counts; a broken fixture or a
    # harness crash must be investigated, not counted as a caught mutation.
    caught = result.returncode == 1 and ' differs:' in result.stdout
    print(('%s: ' % name) + ('caught' if caught else 'MISSED / ERROR'), flush=True)
    if not caught:
        print(result.stdout + result.stderr, flush=True)
        missed.append(name)
print('%d mutations, %d caught, %d unresolved' % (tested, tested - len(missed), len(missed)))
sys.exit(bool(missed))
