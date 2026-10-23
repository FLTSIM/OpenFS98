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
sections = {}
for section in ('palette', 'draw', 'matrix', 'polygon', 'fill', 'textured', 'perspective', 'perspective_fill', 'affine', 'gouraud', 'blit', 'disc'):
    sections[section] = (root / ('src/hg2d/hg2d_%s.inc' % section)).read_text()
    source = source.replace('#include "hg2d_%s.inc"' % section, sections[section])
build = root / os.environ.get('HG2D_BREAKS_DIR', 'bin/hg2d-breaks')
build.mkdir(parents=True, exist_ok=True)
shutil.copyfile(root / os.environ.get('HG2D_BASELINE_DIR', 'bin/hg2d-diff') / 'hg2ddiff.exe', build / 'hg2ddiff.exe')
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
    ('projection view load order', 'polygon_project(x, scale_x, depth, 0) + (offset_x << 16)', 'polygon_project(x, view[0], depth, 0) + (view[1] << 16)'),
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
    ('edge flat delta', 'edge[2] = low_x + 0x8000;\n        edge[3] = high_x - low_x;', 'edge[2] = low_x + 0x8000;\n        edge[3] = low_x - high_x;'),
    ('edge scanline fraction', 'edge[3] = draw_slope(high_x - low_x, high_y - low_y);\n        advance = (low_y & 0xffff) ^ 0xffff', 'edge[3] = draw_slope(high_x - low_x, high_y - low_y);\n        advance = (low_y & 0xffff) ^ 0xfffe'),
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
textured_mutations = [
    ('textured enable word', 'if (!GLOBAL_HALF(0xcb6))', 'if (!GLOBAL_WORD(0xcb6))'),
    ('textured fallback depth', 'polygon_vertex_body(record, x, y, depth);', 'polygon_vertex_body(record, x, y, depth + 1);'),
    ('textured first attribute order', 'WORD(0x914) = WORD(0x92c) = second_attribute;', 'WORD(0x914) = WORD(0x92c) = first_attribute;'),
    ('textured second attribute order', 'WORD(0x918) = WORD(0x930) = first_attribute;', 'WORD(0x918) = WORD(0x930) = second_attribute;'),
    ('textured first eax', 'registers[2] = screen_y;', 'registers[2] = screen_x;'),
    ('textured first ecx', 'registers[1] = WORD(0x8dc) << 16;', 'registers[1] = WORD(0x8d8) << 16;'),
    ('textured projection remainder', '*remainder = (uint32_t)(product % (int32_t)depth);', '*remainder = 0;'),
    ('textured repaired remainder', '*remainder = 0x7fffffff;', '*remainder = 0xffffffff;'),
    ('textured counter', 'GLOBAL_WORD(0x2f0) += 1;', 'GLOBAL_WORD(0x2f0) += 2;'),
    ('textured capacity no-op', 'if (screen_y < WORD(0x8c8))', 'if (GLOBAL_WORD(0xc14) + 0x60 >= ADDRESS(hg2d_image + 0x884)) return; if (screen_y < WORD(0x8c8))'),
    ('textured minimum unsigned', 'screen_y < WORD(0x8c8)', '(int32_t)screen_y < (int32_t)WORD(0x8c8)'),
    ('textured maximum unsigned', 'screen_y > WORD(0x8cc)', '(int32_t)screen_y > (int32_t)WORD(0x8cc)'),
    ('textured previous attribute', 'WORD(0x924) = WORD(0x92c);', 'WORD(0x924) = WORD(0x930);'),
    ('textured previous second attribute', 'WORD(0x928) = WORD(0x930);', 'WORD(0x928) = WORD(0x92c);'),
    ('textured current attribute', 'WORD(0x91c) = second_attribute;', 'WORD(0x91c) = first_attribute;'),
    ('textured ordering unsigned', 'if (screen_y < previous_y)', 'if ((int32_t)screen_y < (int32_t)previous_y)'),
    ('textured exchanged attribute', 'WORD(0x924) = WORD(0x91c); WORD(0x91c) = temporary;', 'WORD(0x924) = WORD(0x91c); WORD(0x91c) = 0;'),
    ('textured exchanged second attribute', 'WORD(0x928) = WORD(0x920); WORD(0x920) = temporary;', 'WORD(0x928) = WORD(0x920); WORD(0x920) = 0;'),
    ('textured edge attribute base', 'edge[4] = WORD(0x924);', 'edge[4] = WORD(0x91c);'),
    ('textured edge second base', 'edge[5] = WORD(0x928);', 'edge[5] = WORD(0x920);'),
    ('textured flat x rounding', 'edge[2] = low_x;', 'edge[2] = low_x + 0x8000;'),
    ('textured flat first delta', 'edge[6] = WORD(0x91c) - WORD(0x924);', 'edge[6] = WORD(0x924) - WORD(0x91c);'),
    ('textured flat second delta', 'edge[7] = WORD(0x920) - WORD(0x928);', 'edge[7] = WORD(0x928) - WORD(0x920);'),
    ('textured flat ecx', 'registers[1] = (uint32_t)((int32_t)low_x >> 16);', 'registers[1] = low_x;'),
    ('textured flat edx', 'registers[0] = (uint32_t)((int32_t)low_y >> 16);', 'registers[0] = low_y;'),
    ('textured slope first delta', 'draw_slope(WORD(0x91c) - WORD(0x924), height)', 'draw_slope(WORD(0x924) - WORD(0x91c), height)'),
    ('textured slope second delta', 'draw_slope(WORD(0x920) - WORD(0x928), height)', 'draw_slope(WORD(0x928) - WORD(0x920), height)'),
    ('textured slope x', 'draw_slope(high_x - low_x, height)', 'draw_slope(low_x - high_x, height)'),
    ('textured fractional advance', 'edge[7] = draw_slope(WORD(0x920) - WORD(0x928), height);\n        advance = (low_y & 0xffff) ^ 0xffff;', 'edge[7] = draw_slope(WORD(0x920) - WORD(0x928), height);\n        advance = low_y & 0xffff;'),
    ('textured first advance', 'edge[4] += (uint32_t)', 'edge[4] -= (uint32_t)'),
    ('textured second advance', 'edge[5] += adjustment;', 'edge[5] -= adjustment;'),
    ('textured nonflat ecx', 'registers[1] = edge[7];', 'registers[1] = edge[6];'),
    ('textured nonflat edx', 'registers[0] = adjustment;', 'registers[0] = edge[5];'),
    ('textured edge eax', 'registers[2] = ADDRESS(GLOBAL);', 'registers[2] = 0;'),
    ('textured edge stride', 'GLOBAL_WORD(0xc14) = ADDRESS(edge) + 32;', 'GLOBAL_WORD(0xc14) = ADDRESS(edge) + 16;'),
]
mutations += [(*mutation, 'textured') for mutation in textured_mutations]
# The perspective entry shares projection and two attributes with +0x48;
# target its owning section while checking those same obligations.
perspective_mutations = []
for name, old, new in textured_mutations:
    if name == 'textured first ecx':
        old, new = 'registers[1] = third_attribute;', 'registers[1] = first_attribute;'
    elif name == 'textured fractional advance':
        old, new = 'advance = (low_y & 0xffff) ^ 0xffff;', 'advance = low_y & 0xffff;'
    elif name == 'textured nonflat ecx':
        old, new = 'registers[1] = edge[9];', 'registers[1] = edge[7];'
    elif name == 'textured edge stride':
        old, new = 'GLOBAL_WORD(0xc14) = ADDRESS(edge) + 40;', 'GLOBAL_WORD(0xc14) = ADDRESS(edge) + 32;'
    perspective_mutations.append((name.replace('textured', 'perspective'), old, new))
perspective_mutations += [
    ('perspective first third', 'WORD(0x934) = WORD(0x938) = third_attribute;', 'WORD(0x934) = WORD(0x938) = first_attribute;'),
    ('perspective current third', 'WORD(0x93c) = third_attribute;', 'WORD(0x93c) = first_attribute;'),
    ('perspective previous third', 'WORD(0x940) = WORD(0x938);', 'WORD(0x940) = WORD(0x934);'),
    ('perspective exchange third', 'WORD(0x940) = WORD(0x93c); WORD(0x93c) = temporary;', 'WORD(0x940) = WORD(0x93c); WORD(0x93c) = 0;'),
    ('perspective edge third base', 'edge[8] = WORD(0x940);', 'edge[8] = WORD(0x93c);'),
    ('perspective flat third delta', 'edge[9] = WORD(0x93c) - WORD(0x940);', 'edge[9] = WORD(0x940) - WORD(0x93c);'),
    ('perspective slope third delta', 'draw_slope(WORD(0x93c) - WORD(0x940), height)', 'draw_slope(WORD(0x940) - WORD(0x93c), height)'),
    ('perspective third advance', 'edge[8] += adjustment;', 'edge[8] -= adjustment;'),
]
mutations += [(*mutation, 'perspective') for mutation in perspective_mutations]
perspective_fill_mutations = [
    ('shaded texel channel order', '(((first + bias) >> 24) << 8) | ((second + bias) >> 24)', '(((second + bias) >> 24) << 8) | ((first + bias) >> 24)'),
    ('shaded first phase advance', '*one += WORD(0x4dc4);', '*one -= WORD(0x4dc4);'),
    ('shaded second phase advance', '*two += WORD(0x4dc4);', '*two -= WORD(0x4dc4);'),
    ('shaded lookup phase bits', '(phase >> 19) & 0xff00', '(phase >> 18) & 0xff00'),
    ('shaded lookup column', '((phase >> 19) & 0xff00) | value', '((phase >> 19) & 0xff00) | (uint8_t)(value + 1)'),
    ('shaded lookup table base', 'value = BYTE(0x4e00 + index);', 'value = BYTE(0x4e01 + index);'),
    ('shaded distance row', '(BYTE(0x100) << 8) + value', '((BYTE(0x100) ^ 1) << 8) + value'),
    ('shaded pair first coordinate advance', 'perspective_sample(*first, *second, bias, one, two, 0, 0);\n    *first += WORD(0x4dd0);', 'perspective_sample(*first, *second, bias, one, two, 0, 0);\n    *first -= WORD(0x4dd0);'),
    ('shaded pair second coordinate advance', '*second += WORD(0x4dd4);\n    perspective_sample(*first', '*second -= WORD(0x4dd4);\n    perspective_sample(*first'),
    ('shaded pair second dither', 'bias + (HALF(0x944) ? 0x800000 : 0)', 'bias + (HALF(0x944) ? 0x400000 : 0)'),
    ('shaded pair store order', 'pixel[0] = (uint8_t)*one; pixel[1] = (uint8_t)(*one >> 8);', 'pixel[1] = (uint8_t)*one; pixel[0] = (uint8_t)(*one >> 8);'),
    ('shaded pair read ordering', 'perspective_sample(*first, *second, bias, one, two, 0, 0);', 'perspective_sample(*first, *second, bias, one, two, 0, 0); pixel[0] = (uint8_t)*one;'),
    ('shaded inclusive width', 'count = last_x - first_x + 1;', 'count = last_x - first_x + 2;'),
    ('shaded software width cap', 'WORD(0x4ddc) = count;', 'WORD(0x4ddc) = count; if (count > 1500) return;'),
    ('shaded third slope doubling', '(int32_t)(right[8] - left[8]) / count) * 2)', '(int32_t)(right[8] - left[8]) / count) * 1)'),
    ('shaded third slope low word', '& 0xffff0000u;', '& 0xffffffffu;'),
    ('shaded third base scratch', 'WORD(0x4dc8) = left[8];', 'WORD(0x4dc8) = right[8];'),
    ('shaded uv first slope', 'affine_divide(right[4] - left[4], count)', 'affine_divide(left[4] - right[4], count)'),
    ('shaded uv second slope', 'affine_divide(right[5] - left[5], count)', 'affine_divide(left[5] - right[5], count)'),
    ('shaded first coordinate shift', 'first = left[4] << 8;', 'first = left[4] << 7;'),
    ('shaded second coordinate shift', 'second = left[5] << 8;', 'second = left[5] << 7;'),
    ('shaded phase bias', 'left[8] - 0x06000000u', 'left[8] - 0x04000000u'),
    ('shaded first phase row', 'WORD(0x4de0 + (y & 3) * 4)', 'WORD(0x4de0 + (y & 1) * 4)'),
    ('shaded second phase row', 'WORD(0x4df0 + (y & 3) * 4)', 'WORD(0x4df0 + (y & 1) * 4)'),
    ('shaded first phase predecrement', 'one += WORD(0x4de0 + (y & 3) * 4) - WORD(0x4dcc);', 'one += WORD(0x4de0 + (y & 3) * 4);'),
    ('shaded second phase predecrement', 'two = one + WORD(0x4df0 + (y & 3) * 4) - WORD(0x4dcc);', 'two = one + WORD(0x4df0 + (y & 3) * 4);'),
    ('shaded dither row parity', 'HALF(0x944) && !(y & 1)', 'HALF(0x944) && (y & 1)'),
    ('shaded odd prefix phase', '&one, &two, 1, 0);', '&one, &two, 0, 0);'),
    ('shaded block pointer advance', 'WORD(0x4dd8) += 8;', 'WORD(0x4dd8) += 4;'),
    ('shaded block count', 'WORD(0x4ddc) -= 8;', 'WORD(0x4ddc) -= 4;'),
    ('shaded tail singleton equality', 'if ((int32_t)WORD(0x4ddc) >= 0)', 'if ((int32_t)WORD(0x4ddc) > 0)'),
    ('shaded final eax', 'registers[2] = one;', 'registers[2] = two;'),
    ('shaded final ecx', 'registers[1] = first;', 'registers[1] = second;'),
    ('shaded final edx', 'registers[0] = WORD(0x4dd0);', 'registers[0] = WORD(0x4dd4);'),
    ('shaded edge search stride', 'edge += 10;', 'edge += 5;'),
    ('shaded final flat row', 'if (y == WORD(0x8cc))', 'if (y != WORD(0x8cc))'),
    ('shaded flat third attribute', 'WORD(0x4dbc) = edge[8] + edge[9];', 'WORD(0x4dbc) = edge[8];'),
    ('shaded flat first attribute', 'WORD(0x4dac) = edge[4] + edge[6];', 'WORD(0x4dac) = edge[4];'),
    ('shaded flat second attribute', 'WORD(0x4db0) = edge[5] + edge[7];', 'WORD(0x4db0) = edge[5];'),
    ('shaded dither word', 'HALF(0x944) = GLOBAL_HALF(0xcce);', 'HALF(0x944) = GLOBAL_WORD(0xcce) != 0;'),
    ('shaded texture record', '*(uint32_t *)POINTER(GLOBAL_WORD(0xc8c))', 'GLOBAL_WORD(0xc8c)'),
    ('shaded setup return', 'registers[2] = WORD(0x948);', 'registers[2] = 0;'),
    ('shaded left third advance', 'left[8] += left[9];', 'left[8] -= left[9];'),
    ('shaded right third advance', 'right[8] += right[9];', 'right[8] -= right[9];'),
    ('shaded close third first', 'WORD(0x93c) = WORD(0x934);', 'WORD(0x93c) = WORD(0x938);'),
    ('shaded close third previous', 'WORD(0x940) = WORD(0x938);', 'WORD(0x940) = WORD(0x934);'),
    ('shaded close third exchange', 'WORD(0x940) = WORD(0x93c); WORD(0x93c) = temporary;', 'WORD(0x940) = WORD(0x93c); WORD(0x93c) = 0;'),
    ('shaded close bounds shift', 'WORD(0x8c8) >>= 16;', 'WORD(0x8c8) >>= 15;'),
]
perspective_fill_mutations += [
    ('shaded upper lookup rows', '(phase >> 19) & 0xff00', '(phase >> 19) & 0x0f00'),
    ('shaded unwanted raw key', 'uint8_t value = *(uint8_t *)POINTER(WORD(0x948) + index);', 'uint8_t value = *(uint8_t *)POINTER(WORD(0x948) + index); if (value < GLOBAL_HALF(0xca0)) return;'),
    ('shaded unwanted quality reduction', 'count = last_x - first_x + 1;', 'count = last_x - first_x + 1; if (!GLOBAL_HALF(0xc92)) count = (count + 1) / 2;'),
    ('shaded unwanted pixel remap', 'value = BYTE(0xd70 + (BYTE(0x100) << 8) + value);', 'value = BYTE(0xd70 + (BYTE(0x100) << 8) + value); if (GLOBAL_WORD(0x1298)) value = *(uint8_t *)POINTER(WORD(0x1d88) + value);'),
    ('shaded partial eax preservation', 'value = BYTE(0xd70 + (BYTE(0x100) << 8) + value);\n    *one = (*one & ~mask) | ((uint32_t)value << shift);', 'value = BYTE(0xd70 + (BYTE(0x100) << 8) + value);\n    *one = (uint32_t)value << shift;'),
]
mutations += [(*mutation, 'perspective_fill') for mutation in perspective_fill_mutations]
matrix_vertex_mutations = [
    ('matrix normalization offset', 'position - (offset << 16)', 'position - (offset << 15)'),
    ('matrix normalization divisor', 'scale << 1', 'scale << 2'),
    ('matrix signed quotient', '(int64_t)(int32_t)value / (int32_t)denominator', '(int64_t)value / denominator'),
    ('matrix repaired sign', '(((int32_t)value < 0) != ((int32_t)denominator < 0))', '(((int32_t)value < 0) == ((int32_t)denominator < 0))'),
    ('matrix negative normalization restore', 'return value ^ sign;', 'return (value ^ sign) - sign;'),
    ('matrix normalization clamp', 'if (value > 0x7fff) value = 0x7fff;', 'if (value > 0x7ffe) value = 0x7ffe;'),
    ('matrix signed x coefficient', '(int16_t)GLOBAL_HALF(0xcfa + component * 2)', '(uint16_t)GLOBAL_HALF(0xcfa + component * 2)'),
    ('matrix signed y coefficient', '(int16_t)GLOBAL_HALF(0xd00 + component * 2)', '(uint16_t)GLOBAL_HALF(0xd00 + component * 2)'),
    ('matrix addition before shift', '(int32_t)(first + second)', '((int64_t)(int32_t)first + (int32_t)second)'),
    ('matrix shift mask', 'GLOBAL[0xd42] & 31', 'GLOBAL[0xd42] & 15'),
    ('matrix attribute offset', 'GLOBAL_WORD(0xd0c + component * 4)', 'GLOBAL_WORD(0xd10 + component * 4)'),
    ('matrix slope complement magnitude', 'magnitude = delta ^ sign;', 'magnitude = (delta ^ sign) - sign;'),
    ('matrix slope fraction', '(uint64_t)remainder << 32', '(uint64_t)remainder << 31'),
    ('matrix slope integer clamp', 'if (quotient > 0x7fff) quotient = 0x7fff;', 'if (quotient > 0x7ffe) quotient = 0x7ffe;'),
    ('matrix slope sign adjustment', 'return ((quotient << 16) | (fraction >> 16)) ^ sign;', 'return (((quotient << 16) | (fraction >> 16)) ^ sign) - sign;'),
    ('matrix edge flat rounding', 'edge[2] = low_x;', 'edge[2] = low_x + 0x8000;'),
    ('matrix low normalized x', 'matrix_coordinate(low_x, WORD(0x8d8)', 'matrix_coordinate(high_x, WORD(0x8d8)'),
    ('matrix high normalized y', 'matrix_coordinate(high_y, WORD(0x8dc)', 'matrix_coordinate(low_y, WORD(0x8dc)'),
    ('matrix component count', 'component < 3; component++)\n        edge[4 + component]', 'component < 2; component++)\n        edge[4 + component]'),
    ('matrix flat raw deltas', 'edge[7 + component] = WORD(0x4184 + component * 4) - edge[4 + component];', 'edge[7 + component] = edge[7 + component];'),
    ('matrix fractional advance', 'advance = (low_y & 0xffff) ^ 0xffff;', 'advance = low_y & 0xffff;'),
    ('matrix x advance', 'edge[2] += (uint32_t)', 'edge[2] -= (uint32_t)'),
    ('matrix attribute advance', 'edge[4 + component] += (uint32_t)', 'edge[4 + component] -= (uint32_t)'),
    ('matrix signed start row', '(uint32_t)((int32_t)low_y >> 16)', 'low_y >> 16'),
    ('matrix edge stride', 'GLOBAL_WORD(0xc14) = ADDRESS(edge) + 40;', 'GLOBAL_WORD(0xc14) = ADDRESS(edge) + 32;'),
    ('matrix vertex capacity reservation', '+ 0x60 >= ADDRESS(hg2d_image + 0x884)', '+ 0x28 >= ADDRESS(hg2d_image + 0x884)'),
]
mutations += [(*mutation, 'matrix_vertices') for mutation in matrix_vertex_mutations]
matrix_fill_mutations = [
    ('matrix setup base multiplier', '(uint64_t)0xd00 * GLOBAL_WORD(0x320)', '(uint64_t)0xc00 * GLOBAL_WORD(0x320)'),
    ('matrix setup numerator scaling', 'while (denominator >> 32) { denominator >>= 1; numerator >>= 1; }\n    ratio', 'while (denominator >> 32) { denominator >>= 1; numerator >>= 2; }\n    ratio'),
    ('matrix setup first ratio', 'numerator << 16', 'numerator << 15'),
    ('matrix setup horizontal ratio', 'matrix_unsigned_divide(0x3e00000, ratio)', 'matrix_unsigned_divide(0x3c00000, ratio)'),
    ('matrix setup vertical ratio', 'matrix_unsigned_divide(0x2400000, ratio)', 'matrix_unsigned_divide(0x2200000, ratio)'),
    ('matrix setup first row copy', 'GLOBAL_WORD(0xcdc + index * 4)', 'GLOBAL_WORD(0xce0 + index * 4)'),
    ('matrix setup second row copy', 'GLOBAL_WORD(0xc94 + index * 4)', 'GLOBAL_WORD(0xc98 + index * 4)'),
    ('matrix setup tail copy', 'index < 4; index++) WORD(0x41e0', 'index < 3; index++) WORD(0x41e0'),
    ('matrix fog cutoff equality', 'GLOBAL_HALF(0xe88) < 0x7d00', 'GLOBAL_HALF(0xe88) <= 0x7d00'),
    ('matrix fog depth sign', '(int32_t)WORD(0x8d4) > 0', 'WORD(0x8d4) > 0'),
    ('matrix far depth test', 'WORD(0x8d0) == 0x7fffffffu', 'WORD(0x8d0) >= 0x7ffffffeu'),
    ('matrix fog threshold multiplication', '(uint64_t)GLOBAL_HALF(0xe88) * GLOBAL_WORD(0x320)', '(uint64_t)(GLOBAL_HALF(0xe88) + 1) * GLOBAL_WORD(0x320)'),
    ('matrix fog absolute scale', '-(int32_t)(int16_t)GLOBAL_HALF(0x30c)', '(int32_t)(int16_t)GLOBAL_HALF(0x30c)'),
    ('matrix fog reduction numerator', '(uint64_t)maximum << 21', '(uint64_t)maximum << 20'),
    ('matrix fog final ratio', '(uint64_t)WORD(0x41f8) << 16', '(uint64_t)WORD(0x41f8) << 15'),
    ('matrix selector signed mode', '(int16_t)GLOBAL_HALF(0x2e8) > 0', 'GLOBAL_HALF(0x2e8) > 0'),
    ('matrix selector plane row', '(int32_t)WORD(0x41cc) > (int32_t)GLOBAL_WORD(0xc98)', '(int32_t)WORD(0x41cc) < (int32_t)GLOBAL_WORD(0xc98)'),
    ('matrix selector key byte', 'BYTE(0x435c) = GLOBAL[0xca0];', 'BYTE(0x435c) = GLOBAL_HALF(0xca0) != 0;'),
    ('matrix selector hardware feature bit', '(GLOBAL_HALF(0x1172) & 1)', '(GLOBAL_HALF(0x1172) != 0)'),
    ('matrix selector hardware enable', '&& GLOBAL_HALF(0x324)', '&& 1'),
    ('matrix selector dither sign', '(int16_t)GLOBAL_HALF(0xcb4) >= 1', 'GLOBAL_HALF(0xcb4) >= 1'),
    ('matrix selector dither word', 'GLOBAL_HALF(0xcce) != 0', 'GLOBAL_WORD(0xcce) != 0'),
    ('matrix selector table index', 'WORD(0x4204 + selector * 4)', 'WORD(0x4204 + (selector ^ 1) * 4)'),
    ('matrix setup pixels word', 'WORD(0x948) = texture[0];', 'WORD(0x948) = ADDRESS(texture);'),
    ('matrix far colour', 'BYTE(0x90c) = BYTE(0x1d06);', 'BYTE(0x90c) = BYTE(0x1d07);'),
    ('matrix far remap frame', 'draw_map_pixels(draw_address(surface, first, y), last - first + 1, POINTER(WORD(0x1d88)));', 'draw_map_span(draw_address(surface, first, y), last - first + 1);'),
    ('matrix walker stride', 'edge += 10;', 'edge += 5;'),
    ('matrix walker end rows', '(int32_t)y >= (int32_t)left[1]', '(int32_t)y > (int32_t)left[1]'),
    ('matrix walker left attributes', 'left[4 + component] += left[7 + component];', 'left[4 + component] -= left[7 + component];'),
    ('matrix walker right attributes', 'right[4 + component] += right[7 + component];', 'right[4 + component] -= right[7 + component];'),
    ('matrix final flat row', 'if (y == WORD(0x8cc))', 'if (y != WORD(0x8cc))'),
    ('matrix flat last coordinate', 'WORD(0x4334) = edge[2] + edge[3];', 'WORD(0x4334) = edge[2];'),
    ('matrix flat last attributes', 'WORD(0x4344) = edge[6] + edge[9];', 'WORD(0x4344) = edge[6];'),
    ('matrix closing x', 'high_x = WORD(0x8f8)', 'high_x = WORD(0x900)'),
    ('matrix closing bounds arithmetic', '(int32_t)WORD(0x8c8) >> 16', '(int32_t)(WORD(0x8c8) >> 16)'),
]
mutations += [(*mutation, 'matrix_fills') for mutation in matrix_fill_mutations]
affine_mutations = [
    ('affine low dividend shift', '(uint32_t)(delta << 7)', '(uint32_t)(delta << 6)'),
    ('affine signed high dividend', 'delta & 0x80000000u ? -0x100000000LL : 0', 'delta & 0x80000000u ? 0 : 0'),
    ('affine quotient doubling', 'return (uint32_t)quotient * 2;', 'return (uint32_t)quotient;'),
    ('affine texel channel order', '(((first + bias) >> 24) << 8) | ((second + bias) >> 24)', '(((second + bias) >> 24) << 8) | ((first + bias) >> 24)'),
    ('affine key equality', 'value < BYTE(0x946)', 'value <= BYTE(0x946)'),
    ('affine key shade order', 'if ((selector & 2) && value < BYTE(0x946))', 'if ((selector & 2) && BYTE(0xd70 + WORD(0x100) * 256 + value) < BYTE(0x946))'),
    ('affine second channel advance', 'two = affine_texel(*first, *second, bias);\n    *first += WORD(0x4618); *second += WORD(0x461c);', 'two = affine_texel(*first, *second, bias);\n    *first += WORD(0x4618); *second -= WORD(0x461c);'),
    ('affine dither row parity', 'selector & 4 ? (y & 1 ? 0 : 0x400000) : 0', 'selector & 4 ? (y & 1 ? 0x400000 : 0) : 0'),
    ('affine dither second sample', 'bias += 0x800000;', 'bias += 0x400000;'),
    ('affine doubled selection', '(selector & 2) || !(selector & 8)', '(selector & 2) && !(selector & 8)'),
    ('affine doubled alignment', 'first_x &= ~1u; last_x &= ~1u;', 'first_x &= ~0u; last_x &= ~0u;'),
    ('affine width bound', 'if (count > 1500) return;', 'if (count >= 1500) return;'),
    ('affine low count denominator', 'affine_divide(right[4] - left[4], doubled ? count / 2 : count)', 'affine_divide(right[4] - left[4], count)'),
    ('affine step first subtraction', 'affine_divide(right[4] - left[4], doubled ? count / 2 : count)', 'affine_divide(left[4] - right[4], doubled ? count / 2 : count)'),
    ('affine step second subtraction', 'affine_divide(right[5] - left[5], doubled ? count / 2 : count)', 'affine_divide(left[5] - right[5], doubled ? count / 2 : count)'),
    ('affine first channel shift', 'first = left[4] << 8; second = left[5] << 8;', 'first = left[4] << 7; second = left[5] << 8;'),
    ('affine second channel shift', 'second = left[5] << 8;', 'second = left[5] << 7;'),
    ('affine block eax leftover', 'registers[2] = saved_eax;', 'registers[2] = saved_eax ^ 1;'),
    ('affine tail signed count', 'if ((int32_t)WORD(0x4630) >= 0)', 'if ((int32_t)WORD(0x4630) > 0)'),
    ('affine find stride', 'edge += 8;', 'edge += 4;'),
    ('affine final flat row', 'if (y == WORD(0x8cc)) {', 'if (y != WORD(0x8cc)) {'),
    ('affine flat first channel', 'WORD(0x4668) = edge[4] + edge[6];', 'WORD(0x4668) = edge[4];'),
    ('affine flat second channel', 'WORD(0x466c) = edge[5] + edge[7];', 'WORD(0x466c) = edge[5];'),
    ('affine texture handle', '*(uint32_t *)POINTER(GLOBAL_WORD(0xc8c))', 'GLOBAL_WORD(0xc8c)'),
    ('affine threshold word', 'HALF(0x946) = GLOBAL_HALF(0xca0);', 'HALF(0x946) = 0;'),
    ('affine distance selector', '(WORD(0x100) != 0)', '(WORD(0x100) == 0)'),
    ('affine quality selector', 'GLOBAL_HALF(0xc92) >= 1', 'GLOBAL_HALF(0xc92) > 1'),
    ('affine walker first channel', 'left[4] += left[6];', 'left[4] -= left[6];'),
    ('affine walker second channel', 'right[5] += right[7];', 'right[5] -= right[7];'),
    ('affine bounds logical shift', 'WORD(0x8c8) >>= 16;', 'WORD(0x8c8) >>= 15;'),
]
mutations += [(*mutation, 'affine') for mutation in affine_mutations]
gouraud_mutations = [
    ('gouraud low colour scale', 'WORD(0x4604) = (uint32_t)low_colour << 8;', 'WORD(0x4604) = (uint32_t)low_colour << 7;'),
    ('gouraud flat rounding', 'edge[2] = low_x + 0x8000;', 'edge[2] = low_x;'),
    ('gouraud colour slope', 'draw_slope(difference, high_y - low_y)', 'draw_slope(0u - difference, high_y - low_y)'),
    ('gouraud colour advance', 'edge[4] += difference;', 'edge[4] -= difference;'),
    ('gouraud edge stride', 'GLOBAL_WORD(0xc14) = ADDRESS(edge) + 24;', 'GLOBAL_WORD(0xc14) = ADDRESS(edge) + 32;'),
    ('gouraud first colour', 'HALF(0x90d) = HALF(0x90f) = (uint16_t)colour;', 'HALF(0x90d) = HALF(0x910) = (uint16_t)colour;'),
    ('gouraud first register', 'registers[2] = y; registers[1] = x;', 'registers[2] = x; registers[1] = y;'),
    ('gouraud capacity no-op', 'if (y < WORD(0x8c8)) WORD(0x8c8) = y;', 'if (GLOBAL_WORD(0xc14) + 0x60 >= ADDRESS(hg2d_image + 0x884)) return; if (y < WORD(0x8c8)) WORD(0x8c8) = y;'),
    ('gouraud y maximum unsigned', 'if (y > WORD(0x8cc))', 'if ((int32_t)y > (int32_t)WORD(0x8cc))'),
    ('gouraud current colour', 'HALF(0x90d) = (uint16_t)colour;', 'HALF(0x90d) = previous_colour;'),
    ('gouraud ordering unsigned', 'if (y < previous_y)', 'if ((int32_t)y < (int32_t)previous_y)'),
    ('gouraud exchanged colour', 'colour = previous_colour; previous_colour = (uint16_t)swap;', 'colour = (uint16_t)swap; previous_colour = (uint16_t)swap;'),
    ('gouraud shift register sample', '*phase >> 24', '*phase >> 23'),
    ('gouraud sample increment', '*phase += increment;', '*phase -= increment;'),
    ('gouraud distance map', '(BYTE(0x100) << 8)', '(BYTE(0x100) << 7)'),
    ('gouraud pixel count', '(uint16_t)(count + 1)', '(uint16_t)(count + 2)'),
    ('gouraud horizontal slope', 'increment *= 2;', 'increment *= 1;'),
    ('gouraud first phase', 'one += WORD(0x45e4 + ((y << 2) & 15));', 'one += WORD(0x45f4 + ((y << 2) & 15));'),
    ('gouraud pair order', 'pixel[0] = (uint8_t)registers[2]; pixel[1] = (uint8_t)(registers[2] >> 8);', 'pixel[1] = (uint8_t)registers[2]; pixel[0] = (uint8_t)(registers[2] >> 8);'),
    ('gouraud first shade byte', '(uint16_t)(left[4] >> 8)', '(uint16_t)(left[4] >> 7)'),
    ('gouraud flat shade end', '(left[5] >> 8)', '(left[5] >> 7)'),
    ('gouraud shade swap', 'first_colour = (uint16_t)registers[0]; last_colour = (uint16_t)registers[1];', 'first_colour = (uint16_t)registers[1]; last_colour = (uint16_t)registers[0];'),
    ('gouraud find stride', 'edge += 6;', 'edge += 3;'),
    ('gouraud closing first colour', 'high_colour = HALF(0x90f)', 'high_colour = HALF(0x90d)'),
    ('gouraud walker colour', 'left[4] += left[5];', 'left[4] -= left[5];'),
]
mutations += [(*mutation, 'gouraud') for mutation in gouraud_mutations]
blit_mutations = [
    ('blit row mask', 'row = row_mode & 0xfff,', 'row = row_mode & 0xff,'),
    ('blit row signed comparison', '(int32_t)row >= (int32_t)registers[2]', 'row >= registers[2]'),
    ('blit saved ecx', 'registers[1] = row_mode & 0xffff;', 'registers[1] = row_mode;'),
    ('blit first x mask', 'first_x &= 0xffff;', 'first_x &= 0xfffe;'),
    ('blit source row', '(coordinate & 0xff00)', '(coordinate & 0xff)'),
    ('blit source column', '(coordinate >> 24)', '(coordinate >> 23)'),
    ('blit packed initial', '(first_u << 16) + first_v', '(first_u << 15) + first_v'),
    ('blit packed step', '(blit_divide((int32_t)last_u - first_u, count) << 16)', '(blit_divide((int32_t)last_u - first_u, count) << 15)'),
    ('blit inclusive count', '(int32_t)last_x - first_x + 1', '(int32_t)last_x - first_x'),
    ('blit zero divisor', 'if (!denominator) return numerator < 0 ? 0x80000001u : 0x7fffffffu;', 'if (!denominator) return 0;'),
    ('blit solid full word', 'colour = HALF(0x34ca)', 'colour = BYTE(0x34ca)'),
    ('blit solid eax', 'registers[2] = value * 0x1010101u;', 'registers[2] = value;'),
    ('blit band threshold', '(int32_t)first_v <= 0x7f00', '(int32_t)first_v < 0x7f00'),
    ('blit band second palette', 'mode == 2 ? 0x29a0 : 0x29a2', 'mode == 2 ? 0x29a2 : 0x29a0'),
    ('blit transparent raw key', 'if (value) *pixel = mapped;', 'if (mapped) *pixel = mapped;'),
    ('blit transparent ecx mask', 'if (count <= 0) { registers[0] = (uint32_t)count; return; }', 'if (count <= 0) { registers[0] = 1; return; }'),
    ('blit block threshold', 'if (count > 4) {', 'if (count >= 4) {'),
    ('blit block register', '(uint32_t)values[3] << 24', '(uint32_t)values[2] << 24'),
]
mutations += [(*mutation, 'blits') for mutation in blit_mutations]
optimized_mutations = [
    ('optimized row dither', 'bias = (y & 1 ? 0 : 0x400000)', 'bias = (y & 1 ? 0x400000 : 0)'),
    ('optimized column dither', '(address & 1 ? 0x800000 : 0)', '(address & 1 ? 0 : 0x800000)'),
    ('optimized coordinate mask', '((first + bias) & 0xff000000u)', '((first + bias) & 0xffff0000u)'),
    ('optimized second bias', 'second + (bias >> 8)', 'second + (bias >> 9)'),
    ('optimized stack state', '32 - (flat ? 44 : 40)', '32 - (flat ? 40 : 44)'),
    ('optimized x order', 'if (left[2] > right[2])', 'if (left[2] < right[2])'),
    ('optimized inclusive width', '(right[2] >> 16) - (left[2] >> 16) + 1', '(right[2] >> 16) - (left[2] >> 16) + 2'),
    ('optimized signed first delta', '(int64_t)(int32_t)(right[4] - left[4]) / count', '(int64_t)(uint32_t)(right[4] - left[4]) / count'),
    ('optimized signed second delta', '(int64_t)(int32_t)(right[5] - left[5]) / count', '(int64_t)(uint32_t)(right[5] - left[5]) / count'),
    ('optimized first step shift', 'step_first <<= 8;', 'step_first <<= 7;'),
    ('optimized keyed coordinate or', '((first & 0xff000000u) | second)', '((first & 0xff000000u) + second)'),
    ('optimized key equality', 'value >= BYTE(0x946)', 'value > BYTE(0x946)'),
    ('optimized shaded key pixel', 'pixel[1] = value; /* Retail', 'pixel[0] = value; /* Retail'),
    ('optimized unshaded key pixel', 'pixel[0] = value;\n                }', 'pixel[1] = value;\n                }'),
    ('optimized key remainder', '(remainder & 0xffffff00u) | BYTE(0x946)', 'BYTE(0x946)'),
    ('optimized block count scratch', 'WORD(0x5e08) = count - 4;', 'WORD(0x5e08) = count;'),
    ('optimized final coordinate', 'registers[1] = second;', 'registers[1] = first;'),
    ('optimized block read ordering', 'values[index] = BYTE(0xd70 + WORD(0x100) * 256 + values[index]);', 'values[index] = BYTE(0xd70 + WORD(0x100) * 256 + values[index]); pixel[index] = values[index];'),
    ('optimized setup eax', 'if (registers[1]) registers[2] = WORD(0x948);', 'if (registers[1]) registers[2] = 0;'),
]
mutations += [(*mutation, 'optimized') for mutation in optimized_mutations]
disc_mutations = [
    ('disc radius low word', 'radius &= 0xffff;', 'radius &= 0xffffffff;'),
    ('disc double threshold', 'if (radius >= 48) {', 'if (radius > 48) {'),
    ('disc radius halve', 'radius >>= 1;', 'radius >>= 2;'),
    ('disc radius clamp', 'radius = 47;', 'radius = 46;'),
    ('disc profile pointer', 'WORD(0x4784 + radius * 4)', 'WORD(0x4784 + (radius ? radius - 1 : 0) * 4)'),
    ('disc initial row', 'rows * (BYTE(0x4d48) ? 2 : 1)', 'rows * (BYTE(0x4d48) ? 1 : 1)'),
    ('disc first half width', 'if (!pass) width++;', 'if (pass) width++;'),
    ('disc reverse initial byte', 'if (pass) profile--;\n        remaining', 'if (pass) profile -= 2;\n        remaining'),
    ('disc doubled width', 'width = (uint8_t)(width * 2);', 'width = (uint8_t)(width * 1);'),
    ('disc signed left clip', '(int16_t)first < (int16_t)HALF(0x4d40)', 'first < HALF(0x4d40)'),
    ('disc signed right clip', '(int16_t)last > (int16_t)HALF(0x4d42)', 'last > HALF(0x4d42)'),
    ('disc inclusive x clip', '(int16_t)first <= (int16_t)last', '(int16_t)first < (int16_t)last'),
    ('disc lower y bound', '(int16_t)row >= (int16_t)HALF(0x4d44)', '(int16_t)row > (int16_t)HALF(0x4d44)'),
    ('disc upper y bound', '(int16_t)row > (int16_t)HALF(0x4d46)', '(int16_t)row >= (int16_t)HALF(0x4d46)'),
    ('disc doubled y', 'y = doubled_y >> 1', 'y = doubled_y >> 2'),
    ('disc row advance', 'row += 2;', 'row += 4;'),
    ('disc repeat count', 'if (--BYTE(0x4d49)) continue;', 'if (0) continue;'),
    ('disc colour pair', 'GLOBAL_HALF(0xcc8), &mapped_pair', 'GLOBAL_HALF(0xcc2), &mapped_pair'),
    ('disc stipple mode', '== 0x68;', '== 0x69;'),
    ('disc stipple phase', '(y ^ first) & 1', '(y ^ first ^ 1) & 1'),
    ('disc stipple singleton', 'do { *pixel = BYTE(0x90c); pixel += 2; count -= 2; } while ((int32_t)count > 0);', 'while ((int32_t)count > 0) { *pixel = BYTE(0x90c); pixel += 2; count -= 2; }'),
    ('disc height equality', 'y >= SURFACE_WORD(surface, 8)', 'y > SURFACE_WORD(surface, 8)'),
    ('disc remap frame', 'draw_map_pixels_state(pixel, count, POINTER(WORD(0x1d88)), registers);', 'draw_map_span(pixel, count);'),
    ('disc map leftover edx', 'registers[0] = values[6];', 'registers[0] = values[7];'),
]
mutations += [(*mutation, 'discs') for mutation in disc_mutations]
lifecycle_mutations = [
    ('teardown eax', r'_module_deinit:\n\tpushl %ebx', r'_module_deinit:\n\txorl %eax,%eax\n\tpushl %ebx'),
    ('teardown ecx', r'_module_deinit:\n\tpushl %ebx', r'_module_deinit:\n\txorl %ecx,%ecx\n\tpushl %ebx'),
]
mutations += [(*mutation, 'lifecycle') for mutation in lifecycle_mutations]
if len(sys.argv) > 2 and sys.argv[2] not in {suite for _, _, _, suite in mutations}:
    raise SystemExit('Unknown mutation family: ' + sys.argv[2])
missed = []
tested = 0
resume_after = os.environ.get('HG2D_RESUME_AFTER')
mutation_name = os.environ.get('HG2D_MUTATION_NAME')
if mutation_name and mutation_name not in {name for name, _, _, _ in mutations}:
    raise SystemExit('Unknown mutation: ' + mutation_name)
if resume_after and resume_after not in {name for name, _, _, _ in mutations}:
    raise SystemExit('Unknown resume mutation: ' + resume_after)
for name, old, new, suite in mutations:
    if resume_after:
        if name == resume_after:
            resume_after = None
        continue
    if len(sys.argv) > 2 and sys.argv[2] != suite:
        continue
    if mutation_name and mutation_name != name:
        continue
    tested += 1
    owner = {'colours': 'palette', 'palette': 'palette', 'drawing': 'draw',
             'polygons': 'polygon', 'fills': 'fill', 'textured': 'textured', 'affine': 'affine', 'gouraud': 'gouraud', 'blits': 'blit', 'optimized': 'affine', 'perspective': 'perspective', 'discs': 'disc', 'perspective_fill': 'perspective_fill', 'matrix_vertices': 'matrix', 'matrix_fills': 'matrix'}.get(suite)
    if source.count(old) != 1 and owner:
        scope = sections[owner]
        assert scope.count(old) == 1 and source.count(scope) == 1, (name, scope.count(old))
        mutant = source.replace(scope, scope.replace(old, new))
    else:
        assert source.count(old) == 1, (name, source.count(old))
        mutant = source.replace(old, new)
    if os.environ.get('HG2D_AUDIT_ONLY'):
        continue
    variant = build / 'hg2d_mutant.c'
    variant.write_text(mutant)
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
if os.environ.get('HG2D_AUDIT_ONLY'):
    print('%d mutation anchors verified; no mutants executed' % tested)
else:
    print('%d mutations, %d caught, %d unresolved' % (tested, tested - len(missed), len(missed)))
sys.exit(bool(missed))
