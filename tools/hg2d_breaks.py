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
import tempfile

root = Path(__file__).resolve().parent.parent
source = (root / 'src/hg2d/hg2d.c').read_text()
sections = {}
for section in ('palette', 'draw', 'mip_build', 'mip', 'dither', 'projective', 'mip_dither', 'matrix', 'polygon', 'fill', 'textured', 'perspective', 'perspective_fill', 'affine', 'gouraud', 'blit', 'disc', 'fan', 'resource', 'init', 'device_palette', 'device_pixels', 'device_states', 'device_shutdown', 'device_config', 'device_formats', 'device_select_format', 'device_flush', 'device_colour'):
    sections[section] = (root / ('src/hg2d/hg2d_%s.inc' % section)).read_text()
    if section == 'projective':
        sections[section] = sections[section].replace('#include "hg2d_dither.inc"', sections['dither'])
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
    ('last pool slot', 'for (offset = POOL; offset < POOL_END; offset += 0x34)\n        if (WORD(offset) == 0)', 'for (offset = POOL; offset < POOL_END - 0x34; offset += 0x34)\n        if (WORD(offset) == 0)'),
    ('palette byte', 'pixels[index] * 2]', 'pixels[index] * 2 + 1]'),
    ('palette last pixel', 'index < 0x10000;', 'index < 0xffff;'),
    ('FE result', 'if (!load(texture[0],', 'if (load(texture[0],'),
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
    ('hardware scratch', '        for (index = 0; index < 8; index++)\n            WORD(0x8820', '        for (index = 0; index < 7; index++)\n            WORD(0x8820'),
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
projective_mutations = [
    ('projective signed denominator', 'quotient = dividend / (int32_t)denominator;', 'quotient = dividend / denominator;'),
    ('projective valid minimum quotient', 'quotient >= -2147483648LL', 'quotient > -2147483648LL'),
    ('projective repaired sign', '((dividend < 0) != ((int32_t)denominator < 0))', '((dividend < 0) == ((int32_t)denominator < 0))'),
    ('projective product high bits', '(uint32_t)(((int64_t)(int32_t)attribute * (int32_t)factor) >> 16)', '((uint32_t)((int64_t)(int32_t)attribute * (int32_t)factor) >> 16)'),
    ('projective first base row', 'WORD(0x41c8 + axis * 8)', 'WORD(0x41c8 + axis * 4)'),
    ('projective second base row', 'WORD(0x41d4 + axis * 8)', 'WORD(0x41d4 + axis * 4)'),
    ('projective lower clip sign', '(int32_t)value < (int32_t)WORD(0x41e0 + axis * 4)', 'value < WORD(0x41e0 + axis * 4)'),
    ('projective upper clip sign', '(int32_t)value > (int32_t)WORD(0x41e8 + axis * 4)', 'value > WORD(0x41e8 + axis * 4)'),
    ('projective clip gate order', 'else if ((int32_t)value > (int32_t)WORD(0x41e8 + axis * 4))', 'if ((int32_t)value > (int32_t)WORD(0x41e8 + axis * 4))'),
    ('projective numerator shift', '(WORD(0x41cc) - WORD(0x41d8)) * 256', '(WORD(0x41cc) - WORD(0x41d8)) * 128'),
    ('projective denominator shift sign', '(uint32_t)((int32_t)WORD(attributes + 4) >> 8)', '(WORD(attributes + 4) >> 8)'),
    ('projective odd denominator', 'WORD(attributes + 4) >> 8) | 1;', 'WORD(attributes + 4) >> 8) | 0;'),
    ('projective factor negation', 'factor = 0u - projective_divide', 'factor = projective_divide'),
    ('projective texel channel order', 'WORD(0x948) + *u + ((uint32_t)*v << 8)', 'WORD(0x948) + *v + ((uint32_t)*u << 8)'),
    ('projective u fractional carry', '*u + u_step + (uint32_t)(sum >> 32)', '*u + u_step'),
    ('projective v fractional carry', '*v + v_step + (uint32_t)(sum >> 32)', '*v + v_step'),
    ('projective initial u fraction', 'u_fraction = WORD(start) << 16', 'u_fraction = WORD(start) << 15'),
    ('projective initial v fraction', 'v_fraction = WORD(start + 4) << 16', 'v_fraction = WORD(start + 4) << 15'),
    ('projective u step byte', 'u_step = (uint8_t)(WORD(0x41a8) >> 16)', 'u_step = (uint8_t)(WORD(0x41a8) >> 8)'),
    ('projective v step byte', 'v_step = (uint8_t)(WORD(0x41ac) >> 16)', 'v_step = (uint8_t)(WORD(0x41ac) >> 8)'),
    ('projective shifted u scratch', 'WORD(0x41a8) <<= 16;', 'WORD(0x41a8) <<= 15;'),
    ('projective shifted v scratch', 'WORD(0x41ac) <<= 16;', 'WORD(0x41ac) <<= 15;'),
    ('projective alignment prefix', '((pixel_address & 2) != 0) == (direction > 0)', '((pixel_address & 2) != 0) != (direction > 0)'),
    ('projective batch read ordering', 'uint8_t second = projective_sample', '*(uint16_t *)POINTER(pixel_address) = first * 0x101u; uint8_t second = projective_sample'),
    ('projective forward pair order', 'pixel[0] = pixel[1] = first; pixel[2] = pixel[3] = second;', 'pixel[0] = pixel[1] = second; pixel[2] = pixel[3] = first;'),
    ('projective reverse pair order', 'pixel[0] = pixel[1] = second; pixel[2] = pixel[3] = first;', 'pixel[0] = pixel[1] = first; pixel[2] = pixel[3] = second;'),
    ('projective doubled spacing', 'pixel_address += direction * 4;', 'pixel_address += direction * 2;'),
    ('projective even endpoints', 'first_x = left[2] & 0xfffeffffu', 'first_x = left[2] & 0xfffdffffu'),
    ('projective traversal direction', 'direction = forward ? 1 : -1;', 'direction = forward ? -1 : 1;'),
    ('projective endpoint swap', 'if (!forward) { uint32_t *swap', 'if (forward) { uint32_t *swap'),
    ('projective initial attribute endpoint', 'WORD(0x4178 + component * 4) = left[4 + component];', 'WORD(0x4178 + component * 4) = right[4 + component];'),
    ('projective saved row', 'WORD(0x41f4) = y;', 'WORD(0x41f4) = y + 1;'),
    ('projective sample count', 'count = (uint32_t)((int32_t)(last_x - first_x) >> 17) + 1;\n    if (dither)', 'count = (uint32_t)((int32_t)(last_x - first_x) >> 16) + 1;\n    if (dither)'),
    ('projective negative ratio magnitude', 'minimum = (int32_t)right_factor < 0 ? 0u - right_factor : right_factor;', 'minimum = right_factor;'),
    ('projective ratio denominator guard', '(maximum + 1)', '(maximum)'),
    ('projective strict correction threshold', 'ratio < 0x10000000', 'ratio <= 0x10000000'),
    ('projective block selection', 'shift = ratio < 0x10000000 ? 4 : 5;', 'shift = ratio < 0x10000000 ? 5 : 4;'),
    ('projective wide homogeneous step', '(int64_t)(int32_t)(WORD(0x4184 + component * 4) - WORD(0x4178 + component * 4)) * block', '(int64_t)(int32_t)((WORD(0x4184 + component * 4) - WORD(0x4178 + component * 4)) * block)'),
    ('projective stack call depth', '(flat ? 68 : 64)', '(flat ? 64 : 68)'),
    ('projective block homogeneous advance', 'WORD(0x4178 + component * 4) += WORD(0x41b0 + component * 4);', 'WORD(0x4178 + component * 4) -= WORD(0x41b0 + component * 4);'),
    ('projective block old u scratch', 'WORD(0x4198) = WORD(0x4190);', 'WORD(0x4198) = WORD(0x41a0);'),
    ('projective block u slope rounding', '(uint32_t)((int32_t)(WORD(0x4190) - WORD(0x4198)) >> shift)', '(uint32_t)((int32_t)(WORD(0x4190) - WORD(0x4198)) / (int32_t)(1u << shift))'),
    ('projective block saved count', 'WORD(0x4160) = remaining;\n        if (mip && fog)', 'WORD(0x4160) = remaining - block;\n        if (mip && fog)'),
    ('projective block destination advance', 'pixel_address += direction * block * (dither ? 1 : 2);', 'pixel_address += direction * block;'),
    ('projective zero tail return', 'if ((int32_t)remaining <= 0) return;', 'if ((int32_t)remaining < 0) return;'),
    ('projective tail u division', 'projective_divide((int32_t)(WORD(0x41a0) - WORD(0x4190)), remaining)', 'projective_divide((int32_t)(WORD(0x41a0) - WORD(0x4190)), remaining + 1)'),
    ('projective tail target direction', 'projective_tails[forward]', 'projective_tails[!forward]'),
    ('projective tail target block', 'projective_tails[forward][shift == 4][remaining]', 'projective_tails[forward][shift != 4][remaining]'),
    ('projective height clipping', 'int forward = WORD(0x4200) == 0x2073751e;', 'int forward = WORD(0x4200) == 0x2073751e; if (y >= SURFACE_WORD(POINTER(WORD(0x8c4)), 8)) return;'),
    ('projective unwanted pixel remap', 'return value;\n}\n\nstatic void projective_pixels', 'if (GLOBAL_WORD(0x1298)) value = *(uint8_t *)POINTER(WORD(0x1d88) + value); return value;\n}\n\nstatic void projective_pixels'),
]
mutations += [(*mutation, 'projective') for mutation in projective_mutations]
keyed_loop = 'if (keyed) {\n        while (remaining--) {'
keyed_batch = keyed_loop + """
            if (remaining) {
                uint8_t first = projective_sample(&u_fraction, &v_fraction, &u, &v, u_step, v_step);
                uint8_t second = projective_sample(&u_fraction, &v_fraction, &u, &v, u_step, v_step);
                uint8_t *pixel = POINTER(pixel_address);
                if (first >= BYTE(0x435c)) pixel[0] = pixel[1] = first;
                pixel = POINTER(pixel_address + direction * 2);
                if (second >= BYTE(0x435c)) pixel[0] = pixel[1] = second;
                pixel_address += direction * 4; remaining--; continue;
            }"""
keyed_store = """            if (value >= BYTE(0x435c)) {
                uint8_t *pixel = POINTER(pixel_address);
                pixel[0] = value; pixel[1] = value;
            }
"""
keyed_advance = """            uint64_t step_sum = (uint64_t)u_fraction + WORD(0x41a8);
            u_fraction = (uint32_t)step_sum;
            u = (uint8_t)(u + u_step + (uint32_t)(step_sum >> 32));
            step_sum = (uint64_t)v_fraction + WORD(0x41ac);
            v_fraction = (uint32_t)step_sum;
            v = (uint8_t)(v + v_step + (uint32_t)(step_sum >> 32));
"""
keyed_mutations = [
    ('keyed advance before store', keyed_store + keyed_advance, keyed_advance + keyed_store),
    ('keyed reverse target', 'keyed = WORD(0x4200) == 0x2073ae7b', 'keyed = WORD(0x4200) == 0x20733bbb'),
    ('keyed forward target', '|| WORD(0x4200) == 0x2073e1ec;', '|| WORD(0x4200) == 0x2073751e;'),
    ('keyed traversal direction', 'forward |= WORD(0x4200) == 0x2073e1ec;', 'forward |= WORD(0x4200) == 0x2073ae7b;'),
    ('keyed equality', 'if (value >= BYTE(0x435c))', 'if (value > BYTE(0x435c))'),
    ('keyed unsigned comparison', 'if (value >= BYTE(0x435c))', 'if ((int8_t)value >= (int8_t)BYTE(0x435c))'),
    ('keyed threshold direction', 'if (value >= BYTE(0x435c))', 'if (value <= BYTE(0x435c))'),
    ('keyed threshold address', 'if (value >= BYTE(0x435c))', 'if (value >= BYTE(0x435d))'),
    ('keyed threshold width', 'if (value >= BYTE(0x435c))', 'if (value >= HALF(0x435c))'),
    ('keyed first pixel', 'pixel[0] = value; pixel[1] = value;\n            }', 'pixel[1] = value;\n            }'),
    ('keyed second pixel', 'pixel[0] = value; pixel[1] = value;\n            }', 'pixel[0] = value;\n            }'),
    ('keyed destination spacing', 'pixel_address += direction * 2;\n        }\n        return;', 'pixel_address += direction;\n        }\n        return;'),
    ('keyed skipped coordinate advance', 'uint64_t step_sum = (uint64_t)u_fraction + WORD(0x41a8);', 'if (value < BYTE(0x435c)) { pixel_address += direction * 2; continue; } uint64_t step_sum = (uint64_t)u_fraction + WORD(0x41a8);'),
    ('keyed immediate sample store', keyed_loop, keyed_batch),
    ('keyed full block gate', 'projective_pixels(pixel_address, block, 0x4198, direction, 0, keyed);', 'projective_pixels(pixel_address, block, 0x4198, direction, 0, 0);'),
    ('keyed tail gate', 'projective_pixels(pixel_address, remaining, 0x4190, direction, 1, keyed);', 'projective_pixels(pixel_address, remaining, 0x4190, direction, 1, 0);'),
    ('keyed tail direction', 'projective_key_tails[forward]', 'projective_key_tails[!forward]'),
    ('keyed tail block', 'projective_key_tails[forward][shift == 4]', 'projective_key_tails[forward][shift != 4]'),
    ('keyed tail count', 'projective_key_tails[forward][shift == 4][remaining]', 'projective_key_tails[forward][shift == 4][(remaining + 1) & 31]'),
    ('keyed tail family', 'keyed ? projective_key_tails', 'keyed ? projective_tails'),
]
mutations += [(*mutation, 'projective_keyed') for mutation in keyed_mutations]
fog_mutations = [
    ('fog reverse target', 'fog = WORD(0x4200) == 0x207352a4', 'fog = WORD(0x4200) == 0x20733bbb'),
    ('fog forward target', '|| WORD(0x4200) == 0x20738c03;', '|| WORD(0x4200) == 0x2073751e;'),
    ('fog traversal direction', 'forward |= WORD(0x4200) == 0x20738c03;', 'forward |= WORD(0x4200) == 0x207352a4;'),
    ('fog complement magnitude', 'factor ^ ((int32_t)factor < 0 ? 0xffffffffu : 0)', '(int32_t)factor < 0 ? 0u - factor : factor'),
    ('fog signed magnitude test', '(int32_t)factor < 0 ? 0xffffffffu : 0', '0'),
    ('fog scale source', 'magnitude * WORD(0x41f8)', 'magnitude * WORD(0x41fc)'),
    ('fog wide scale product', '((uint64_t)magnitude * WORD(0x41f8)) >> 16', '(uint32_t)(magnitude * WORD(0x41f8)) >> 16'),
    ('fog scale shift', 'magnitude * WORD(0x41f8)) >> 16', 'magnitude * WORD(0x41f8)) >> 15'),
    ('fog level clamp', 'return level < 0xfff ? level : 0xfff;', 'return level < 0xfff ? level : 0xffe;'),
    ('fog initial endpoint level', 'if (fog) WORD(0x41bc) = projective_fog_level(right_factor);', 'if (fog) WORD(0x41bc) = 0;'),
    ('fog final endpoint level', 'WORD(0x41c4) = projective_fog_level(left_factor);', 'WORD(0x41c4) = WORD(0x41bc);'),
    ('fog shortcut row equality', 'if (BYTE(0x41c5) == BYTE(0x41bd))', 'if (1)'),
    ('fog zero row shortcut', 'if (!BYTE(0x41c5)) fog = 0;', 'if (!BYTE(0x41c5)) fog = 1;'),
    ('fog maximum row shortcut', 'else if (BYTE(0x41c5) == 15)', 'else if (BYTE(0x41c5) == 14)'),
    ('fog old block level', 'if (fog) WORD(0x41c0) = WORD(0x41bc);', 'if (fog) WORD(0x41c0) = WORD(0x41c4);'),
    ('fog block projection level', 'WORD(0x41bc) = projective_fog_level(factor);', 'WORD(0x41bc) = projective_fog_level(left_factor);'),
    ('fog block step sign', '(int32_t)(WORD(0x41bc) - WORD(0x41c0)) >> shift', '(int32_t)(WORD(0x41c0) - WORD(0x41bc)) >> shift'),
    ('fog block rounding', '(uint32_t)((int32_t)(WORD(0x41bc) - WORD(0x41c0)) >> shift)', '(uint32_t)((int32_t)(WORD(0x41bc) - WORD(0x41c0)) / (int32_t)(1u << shift))'),
    ('fog small negative suppression', 'if (WORD(0x4358) >= 0xfffffffeu)', 'if (0)'),
    ('fog minus two suppression', 'WORD(0x4358) >= 0xfffffffeu', 'WORD(0x4358) >= 0xffffffffu'),
    ('fog suppression unsigned test', 'WORD(0x4358) >= 0xfffffffeu', '(int32_t)WORD(0x4358) >= -2'),
    ('fog block starting level', 'block, 0x4198, WORD(0x41c0), direction', 'block, 0x4198, WORD(0x41bc), direction'),
    ('fog tail level division', '        WORD(0x4358) = projective_divide((int32_t)(WORD(0x41c4) - WORD(0x41bc)), remaining);\n        WORD(0x4354) = keyed', '        WORD(0x4358) = projective_divide((int32_t)(WORD(0x41c4) - WORD(0x41bc)), remaining + 1);\n        WORD(0x4354) = keyed'),
    ('fog tail starting level', 'remaining, 0x4190, WORD(0x41bc), direction', 'remaining, 0x4190, WORD(0x41c4), direction'),
    ('fog lookup row byte', '((shade >> 8) & 255) * 256 + raw', '((shade >> 9) & 255) * 256 + raw'),
    ('fog lookup source', '256 + raw);', '256 + (raw ^ 1));'),
    ('fog shade wrap', 'shade = (shade + WORD(0x4358)) & 0xffff0fffu;\n        destination[0]', 'shade = (shade + WORD(0x4358)) & 0xffffffffu;\n        destination[0]'),
    ('fog shade step', 'shade = (shade + WORD(0x4358)) & 0xffff0fffu;\n        destination[0]', 'shade = (shade - WORD(0x4358)) & 0xffff0fffu;\n        destination[0]'),
    ('fog first doubled pixel', 'destination[0] = colour; destination[1] = colour;', 'destination[1] = colour;'),
    ('fog second doubled pixel', 'destination[0] = colour; destination[1] = colour;', 'destination[0] = colour;'),
    ('fog doubled spacing', 'destination[0] = colour; destination[1] = colour;\n        address += direction * 2;', 'destination[0] = colour; destination[1] = colour;\n        address += direction;'),
    ('fog shifted u step', 'WORD(0x41a8) = WORD(0x41a8) << 16;', 'WORD(0x41a8) = WORD(0x41a8) << 15;'),
    ('fog shifted v step', 'WORD(0x41ac) = WORD(0x41ac) << 16;', 'WORD(0x41ac) = WORD(0x41ac) << 15;'),
    ('fog tail target direction', 'projective_fog_tails[forward]', 'projective_fog_tails[!forward]'),
    ('fog tail target block', 'projective_fog_tails[forward][shift == 4]', 'projective_fog_tails[forward][shift != 4]'),
    ('fog tail target count', 'projective_fog_tails[forward][shift == 4][remaining]', 'projective_fog_tails[forward][shift == 4][(remaining + 1) & 31]'),
    ('fog tail target family', 'projective_fog_tails[forward][shift == 4][remaining];', 'projective_tails[forward][shift == 4][remaining];'),
    ('fog sample store ordering', 'while (count--) {\n        uint8_t raw = projective_sample(&u_bits, &v_bits, &x_byte, &y_byte, x_step, y_step);', 'uint8_t pending = projective_sample(&u_bits, &v_bits, &x_byte, &y_byte, x_step, y_step);\n    while (count--) {\n        uint8_t raw = pending; if (count) pending = projective_sample(&u_bits, &v_bits, &x_byte, &y_byte, x_step, y_step);'),
]
mutations += [(*mutation, 'projective_fog') for mutation in fog_mutations]
key_fog_store = """            if (texel >= BYTE(0x435c)) {
                uint8_t mapped = BYTE(0xd70 + ((shade >> 8) & 255) * 256 + texel);
                uint8_t *output = POINTER(address);
                output[0] = mapped; output[1] = mapped;
            }
"""
key_fog_comment = '            /* Retail stores before advancing; a destination may alias steps. */\n'
key_fog_advance = """            uint64_t carry_sum = (uint64_t)u_bits + WORD(0x41a8);
            u_bits = (uint32_t)carry_sum;
            x_byte = (uint8_t)(x_byte + x_step + (uint32_t)(carry_sum >> 32));
            carry_sum = (uint64_t)v_bits + WORD(0x41ac);
            v_bits = (uint32_t)carry_sum;
            y_byte = (uint8_t)(y_byte + y_step + (uint32_t)(carry_sum >> 32));
"""
key_fog_shade = '            shade = (shade + WORD(0x4358)) & 0xffff0fffu;\n'
key_fog_prefix = """if (keyed) {
        while (count--) {
            uint8_t texel = *(uint8_t *)POINTER(WORD(0x948) + x_byte + ((uint32_t)y_byte << 8));
            if (texel >= BYTE(0x435c))"""
key_fog_mutations = [
    ('key fog key gate', 'keyed |= WORD(0x4200) == 0x2073c402 || WORD(0x4200) == 0x2073f771;', 'keyed |= 0;'),
    ('key fog shade gate', 'fog |= WORD(0x4200) == 0x2073c402 || WORD(0x4200) == 0x2073f771;', 'fog |= 0;'),
    ('key fog direction', 'forward |= WORD(0x4200) == 0x2073f771;', 'forward |= WORD(0x4200) == 0x2073c402;'),
    ('key fog equality', 'if (texel >= BYTE(0x435c))', 'if (texel > BYTE(0x435c))'),
    ('key fog unsigned comparison', 'if (texel >= BYTE(0x435c))', 'if ((int8_t)texel >= (int8_t)BYTE(0x435c))'),
    ('key fog raw comparison', 'if (texel >= BYTE(0x435c))', 'if (BYTE(0xd70 + ((shade >> 8) & 255) * 256 + texel) >= BYTE(0x435c))'),
    ('key fog mapped zero', 'output[0] = mapped; output[1] = mapped;', 'if (mapped) { output[0] = mapped; output[1] = mapped; }'),
    ('key fog threshold width', 'if (texel >= BYTE(0x435c))', 'if (texel >= HALF(0x435c))'),
    ('key fog cached threshold', key_fog_prefix, key_fog_prefix.replace('while (count--)', 'uint8_t saved_key = BYTE(0x435c);\n        while (count--)').replace('texel >= BYTE(0x435c)', 'texel >= saved_key')),
    ('key fog first pixel', 'output[0] = mapped; output[1] = mapped;', 'output[1] = mapped;'),
    ('key fog second pixel', 'output[0] = mapped; output[1] = mapped;', 'output[0] = mapped;'),
    ('key fog texel byte order', 'WORD(0x948) + x_byte + ((uint32_t)y_byte << 8)', 'WORD(0x948) + y_byte + ((uint32_t)x_byte << 8)'),
    ('key fog lookup', '* 256 + texel);', '* 256 + (texel ^ 1));'),
    ('key fog u carry', 'x_byte + x_step + (uint32_t)(carry_sum >> 32)', 'x_byte + x_step'),
    ('key fog v carry', 'y_byte + y_step + (uint32_t)(carry_sum >> 32)', 'y_byte + y_step'),
    ('key fog u fraction width', 'u_bits = (uint32_t)carry_sum;', 'u_bits = (uint16_t)carry_sum;'),
    ('key fog v step', 'carry_sum = (uint64_t)v_bits + WORD(0x41ac);', 'carry_sum = (uint64_t)v_bits + WORD(0x41a8);'),
    ('key fog advance before store', key_fog_store + key_fog_comment + key_fog_advance, key_fog_advance + key_fog_store),
    ('key fog shade before store', key_fog_store + key_fog_comment + key_fog_advance + key_fog_shade, '            uint32_t next_shade = (shade + WORD(0x4358)) & 0xffff0fffu;\n' + key_fog_store + key_fog_advance + '            shade = next_shade;\n'),
    ('key fog skipped coordinates', key_fog_comment, '            if (texel < BYTE(0x435c)) { shade = (shade + WORD(0x4358)) & 0xffff0fffu; address += direction * 2; continue; }\n' + key_fog_comment),
    ('key fog skipped shade', key_fog_shade, '            if (texel >= BYTE(0x435c)) shade = (shade + WORD(0x4358)) & 0xffff0fffu;\n'),
    ('key fog shade wrap', key_fog_shade, '            shade = shade + WORD(0x4358);\n'),
    ('key fog destination spacing', 'address = address + direction * 2;', 'address = address + direction;'),
    ('key fog zero shortcut key', 'if (!BYTE(0x41c5)) fog = 0;', 'if (!BYTE(0x41c5)) { fog = 0; keyed = 0; }'),
    ('key fog far shortcut key', 'else if (BYTE(0x41c5) == 15)', 'else if (BYTE(0x41c5) == 15 && !keyed)'),
    ('key fog tail direction', 'projective_key_fog_tails[forward]', 'projective_key_fog_tails[!forward]'),
    ('key fog tail block', 'projective_key_fog_tails[forward][shift == 4]', 'projective_key_fog_tails[forward][shift != 4]'),
    ('key fog tail count', 'projective_key_fog_tails[forward][shift == 4][remaining]', 'projective_key_fog_tails[forward][shift == 4][(remaining + 1) & 31]'),
    ('key fog tail family', 'keyed ? projective_key_fog_tails', 'keyed ? projective_fog_tails'),
]
mutations += [(*mutation, 'projective_key_fog') for mutation in key_fog_mutations]
dither_tail_prefix = """if (tail) {
        for (uint32_t index = 0; index < count; index++) {
            if (index + 1 == count && WORD(0x45d5)) return;"""
dither_mutations = [
    ('dither reverse target', 'dither = WORD(0x4200) == 0x20749d95', 'dither = WORD(0x4200) == 0x20733bbb'),
    ('dither forward target', '|| WORD(0x4200) == 0x207541ee;', '|| WORD(0x4200) == 0x2073751e;'),
    ('dither direction', 'forward |= WORD(0x4200) == 0x207541ee;', 'forward |= WORD(0x4200) == 0x20749d95;'),
    ('dither endpoint fractions', 'if (dither) { first_x = left[2]; last_x = right[2]; }', 'if (dither) { first_x = left[2] & 0xfffeffffu; last_x = right[2] & 0xfffeffffu; }'),
    ('dither full pixel count', 'if (dither) count = (uint32_t)((int32_t)(last_x - first_x) >> 16) + 1;', 'if (dither) count = (uint32_t)((int32_t)(last_x - first_x) >> 17) + 1;'),
    ('dither start complement', 'right_factor ^ ((int32_t)right_factor < 0 ? 0xffffffffu : 0)', '(int32_t)right_factor < 0 ? 0u - right_factor : right_factor'),
    ('dither end complement', 'left_factor ^ ((int32_t)left_factor < 0 ? 0xffffffffu : 0)', '(int32_t)left_factor < 0 ? 0u - left_factor : left_factor'),
    ('dither saved factor', 'WORD(0x4365) = WORD(0x4361);', 'WORD(0x4365) = 0;'),
    ('dither threshold equality', 'WORD(0x4361) >= WORD(0x436d) && WORD(0x4365) >= WORD(0x436d)', 'WORD(0x4361) > WORD(0x436d) && WORD(0x4365) > WORD(0x436d)'),
    ('dither both endpoints', 'WORD(0x4361) >= WORD(0x436d) && WORD(0x4365) >= WORD(0x436d)', 'WORD(0x4361) >= WORD(0x436d) || WORD(0x4365) >= WORD(0x436d)'),
    ('dither threshold source', 'WORD(0x4361) >= WORD(0x436d) && WORD(0x4365) >= WORD(0x436d)', 'WORD(0x4361) >= WORD(0x4371) && WORD(0x4365) >= WORD(0x4371)'),
    ('dither fallback mask', 'first_x &= 0xfffeffffu; last_x &= 0xfffeffffu;', 'first_x &= 0xfffdffffu; last_x &= 0xfffdffffu;'),
    ('dither fallback reordering', 'first_x &= 0xfffeffffu; last_x &= 0xfffeffffu;', 'first_x &= 0xfffeffffu; last_x &= 0xfffeffffu; if (first_x > last_x) { uint32_t swap = first_x; first_x = last_x; last_x = swap; }'),
    ('dither row phase', '(y & 1 ? 0 : 0x40000000u)', '(y & 1 ? 0x40000000u : 0)'),
    ('dither address phase', 'address & 1 ? 0x80000000u : 0', 'address & 2 ? 0x80000000u : 0'),
    ('dither half texel', 'address & 1 ? 0x80000000u : 0', 'address & 1 ? 0x40000000u : 0'),
    ('dither quarter texel', 'y & 1 ? 0 : 0x40000000u', 'y & 1 ? 0 : 0'),
    ('dither v bias', '(v + bias) >> 32', '(v + 0) >> 32'),
    ('dither u bias', '(u + bias) >> 32', '(u + 0) >> 32'),
    ('dither texture byte order', 'texture + column + row * 256', 'texture + row + column * 256'),
    ('dither u integer step', '((uint64_t)u_integer << 32)', '((uint64_t)u_integer << 31)'),
    ('dither v integer step', '((uint64_t)v_integer << 32)', '((uint64_t)v_integer << 31)'),
    ('dither u fractional step', '+ WORD(0x41a8)) & 0xffffffffffULL', '+ 0) & 0xffffffffffULL'),
    ('dither v fractional step', '+ WORD(0x41ac)) & 0xffffffffffULL', '+ 0) & 0xffffffffffULL'),
    ('dither shifted u scratch', 'WORD(0x41a8) = WORD(0x41a8) * 65536u;', 'WORD(0x41a8) = WORD(0x41a8) * 32768u;'),
    ('dither shifted v scratch', 'WORD(0x41ac) = WORD(0x41ac) * 65536u;', 'WORD(0x41ac) = WORD(0x41ac) * 32768u;'),
    ('dither alignment prefix', '((address & 1) != 0) == (direction > 0)', '((address & 1) != 0) != (direction > 0)'),
    ('dither pair read ordering', 'uint8_t second = dither_texel', '*(uint8_t *)POINTER(address) = first; uint8_t second = dither_texel'),
    ('dither pair first byte', 'output[0] = direction > 0 ? first : second;', 'output[0] = direction > 0 ? second : first;'),
    ('dither pair second byte', 'output[1] = direction > 0 ? second : first;', 'output[1] = direction > 0 ? first : second;'),
    ('dither block destination', 'block * (dither ? 1 : 2)', 'block * 2'),
    ('dither tail address flag', 'WORD(0x45d1) = pixel_address & 1;', 'WORD(0x45d1) = (pixel_address ^ 1) & 1;'),
    ('dither tail skip flag', 'WORD(0x45d5) = (pixel_address ^ remaining) & 1;', 'WORD(0x45d5) = (pixel_address ^ remaining ^ 1) & 1;'),
    ('dither tail padded count', 'WORD(0x45d9) = remaining + WORD(0x45d5);', 'WORD(0x45d9) = remaining;'),
    ('dither tail saved count', 'WORD(0x4160) = remaining;\n        WORD(0x4354)', 'WORD(0x4160) = remaining + 1;\n        WORD(0x4354)'),
    ('dither tail live skip', dither_tail_prefix, dither_tail_prefix.replace('for (uint32_t index', 'uint32_t saved_skip = WORD(0x45d5);\n        for (uint32_t index').replace('count && WORD(0x45d5)', 'count && saved_skip')),
    ('dither tail target direction', 'projective_dither_tails[forward]', 'projective_dither_tails[!forward]'),
    ('dither tail target block', 'projective_dither_tails[forward][shift == 4]', 'projective_dither_tails[forward][shift != 4]'),
    ('dither tail target row', 'projective_dither_tails[forward][shift == 4][y & 1]', 'projective_dither_tails[forward][shift == 4][(y ^ 1) & 1]'),
    ('dither tail target count', 'projective_dither_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_dither_tails[forward][shift == 4][y & 1][(WORD(0x45d9) + 1) % 33]'),
]
mutations += [(*mutation, 'projective_dither') for mutation in dither_mutations]
dither_fog_store = '        *(uint8_t *)POINTER(address) = BYTE(0xd70 + texel + ((level >> 8) & 255) * 256);\n'
dither_fog_advance = '        dither_advance(&sample_u, &sample_v, u_integer, v_integer);\n'
dither_fog_level = '        level = (level + WORD(0x4358)) & 0xffff0fffu;\n'
dither_fog_loop = """    for (uint32_t pixel = 0; pixel < count; pixel++) {
        if (tail && pixel + 1 == count && WORD(0x45d5)) return;
        uint8_t texel = dither_texel(texture, sample_u, sample_v, dither_bias(address, y));"""
dither_fog_prefetch = """    uint8_t pending = dither_texel(texture, sample_u, sample_v, dither_bias(address, y));
    for (uint32_t pixel = 0; pixel < count; pixel++) {
        if (tail && pixel + 1 == count && WORD(0x45d5)) return;
        uint8_t texel = pending;
        uint64_t next_u = sample_u, next_v = sample_v;
        dither_advance(&next_u, &next_v, u_integer, v_integer);
        pending = dither_texel(texture, next_u, next_v, dither_bias(address + direction, y));"""
dither_fog_mutations = [
    ('dither fog shade gate', 'fog |= WORD(0x4200) == 0x2074e49d || WORD(0x4200) == 0x207588f0;', 'fog |= 0;'),
    ('dither fog dither gate', 'dither |= WORD(0x4200) == 0x2074e49d || WORD(0x4200) == 0x207588f0;', 'dither |= 0;'),
    ('dither fog direction', 'forward |= WORD(0x4200) == 0x207588f0;', 'forward |= WORD(0x4200) == 0x2074e49d;'),
    ('dither fog block starting level', 'block, 0x4198, y, direction, 0, WORD(0x41c0)', 'block, 0x4198, y, direction, 0, WORD(0x41bc)'),
    ('dither fog tail starting level', 'WORD(0x45d9), 0x4190, y, direction, 1, WORD(0x41bc)', 'WORD(0x45d9), 0x4190, y, direction, 1, WORD(0x41c4)'),
    ('dither fog shade row', '0xd70 + texel + ((level >> 8) & 255) * 256', '0xd70 + texel + ((level >> 9) & 255) * 256'),
    ('dither fog lookup', '0xd70 + texel +', '0xd71 + texel +'),
    ('dither fog opaque zero', dither_fog_store, '        if (BYTE(0xd70 + texel + ((level >> 8) & 255) * 256))' + dither_fog_store.lstrip()),
    ('dither fog doubled store', dither_fog_store, '        *(uint16_t *)POINTER(address) = BYTE(0xd70 + texel + ((level >> 8) & 255) * 256) * 0x101u;\n'),
    ('dither fog coordinate order', dither_fog_store + dither_fog_advance, dither_fog_advance + dither_fog_store),
    ('dither fog shade order', dither_fog_store + dither_fog_advance + dither_fog_level, '        uint32_t next_level = (level + WORD(0x4358)) & 0xffff0fffu;\n' + dither_fog_store + dither_fog_advance + '        level = next_level;\n'),
    ('dither fog shade step', dither_fog_level, '        level = (level - WORD(0x4358)) & 0xffff0fffu;\n'),
    ('dither fog shade wrap', dither_fog_level, '        level = level + WORD(0x4358);\n'),
    ('dither fog u step shift', 'WORD(0x41a8) *= 65536u;\n    WORD(0x41ac) *= 65536u;\n    for (uint32_t pixel', 'WORD(0x41a8) *= 32768u;\n    WORD(0x41ac) *= 65536u;\n    for (uint32_t pixel'),
    ('dither fog v step shift', 'WORD(0x41ac) *= 65536u;\n    for (uint32_t pixel', 'WORD(0x41ac) *= 32768u;\n    for (uint32_t pixel'),
    ('dither fog destination spacing', '        level = (level + WORD(0x4358)) & 0xffff0fffu;\n        address += direction;', '        level = (level + WORD(0x4358)) & 0xffff0fffu;\n        address += direction * 2;'),
    ('dither fog sample prefetch', dither_fog_loop, dither_fog_prefetch),
    ('dither fog live tail flag', dither_fog_loop, dither_fog_loop.replace('for (uint32_t pixel', 'uint32_t saved_skip = WORD(0x45d5);\n    for (uint32_t pixel').replace('count && WORD(0x45d5)', 'count && saved_skip')),
    ('dither fog tail level division', 'if (fog) WORD(0x4358) = projective_divide((int32_t)(WORD(0x41c4) - WORD(0x41bc)), remaining);', 'if (fog) WORD(0x4358) = projective_divide((int32_t)(WORD(0x41c4) - WORD(0x41bc)), remaining + 1);'),
    ('dither fog zero shortcut', 'if (!BYTE(0x41c5)) fog = 0;', 'if (!BYTE(0x41c5)) { fog = 0; dither = 0; }'),
    ('dither fog far shortcut', 'else if (BYTE(0x41c5) == 15)', 'else if (BYTE(0x41c5) == 15 && !dither)'),
    ('dither fog distance shortcut', 'dither = 0;\n            first_x', 'dither = fog = 0;\n            first_x'),
    ('dither fog tail target direction', 'projective_dither_fog_tails[forward]', 'projective_dither_fog_tails[!forward]'),
    ('dither fog tail target block', 'projective_dither_fog_tails[forward][shift == 4]', 'projective_dither_fog_tails[forward][shift != 4]'),
    ('dither fog tail target row', 'projective_dither_fog_tails[forward][shift == 4][y & 1]', 'projective_dither_fog_tails[forward][shift == 4][(y ^ 1) & 1]'),
    ('dither fog tail target count', 'projective_dither_fog_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_dither_fog_tails[forward][shift == 4][y & 1][(WORD(0x45d9) + 1) % 33]'),
    ('dither fog tail target family', 'fog ? projective_dither_fog_tails', 'fog ? projective_dither_tails'),
]
mutations += [(*mutation, 'projective_dither_fog') for mutation in dither_fog_mutations]
dither_key_store = """        if (indexed >= BYTE(0x435c)) {
            uint8_t painted = fog ? BYTE(0xd70 + indexed + ((level >> 8) & 255) * 256) : indexed;
            *(uint8_t *)POINTER(address) = painted;
        }
"""
dither_key_advance = '        dither_advance(&key_u, &key_v, u_integer, v_integer);\n'
dither_key_loop = """    for (uint32_t key_pixel = 0; key_pixel < count; key_pixel++) {
        if (tail && key_pixel + 1 == count && WORD(0x45d5)) return;
        uint8_t indexed = dither_texel(texture, key_u, key_v, tail ? dither_bias(address, y) : 0);
"""
dither_key_prefetch = """    uint8_t pending = dither_texel(texture, key_u, key_v, tail ? dither_bias(address, y) : 0);
    for (uint32_t key_pixel = 0; key_pixel < count; key_pixel++) {
        if (tail && key_pixel + 1 == count && WORD(0x45d5)) return;
        uint8_t indexed = pending;
        uint64_t next_u = key_u, next_v = key_v;
        dither_advance(&next_u, &next_v, u_integer, v_integer);
        pending = dither_texel(texture, next_u, next_v, tail ? dither_bias(address + direction, y) : 0);
"""
dither_key_mutations = [
    ('dither key key gate', 'keyed |= WORD(0x4200) == 0x2075e63f || WORD(0x4200) == 0x20766058;', 'keyed |= 0;'),
    ('dither key dither gate', 'dither |= WORD(0x4200) == 0x2075e63f || WORD(0x4200) == 0x20766058;', 'dither |= 0;'),
    ('dither key direction', 'forward |= WORD(0x4200) == 0x20766058;', 'forward |= WORD(0x4200) == 0x2075e63f;'),
    ('dither key equality', 'indexed >= BYTE(0x435c)', 'indexed > BYTE(0x435c)'),
    ('dither key signed comparison', 'indexed >= BYTE(0x435c)', '(int8_t)indexed >= (int8_t)BYTE(0x435c)'),
    ('dither key threshold width', 'indexed >= BYTE(0x435c)', 'indexed >= HALF(0x435c)'),
    ('dither key cached threshold', dither_key_loop + dither_key_store, '    uint8_t saved_key = BYTE(0x435c);\n' + dither_key_loop + dither_key_store.replace('BYTE(0x435c)', 'saved_key')),
    ('dither key full block bias', 'tail ? dither_bias(address, y) : 0', 'dither_bias(address, y)'),
    ('dither key tail bias', 'tail ? dither_bias(address, y) : 0', '0'),
    ('dither key doubled store', dither_key_store, '        if (indexed >= BYTE(0x435c)) *(uint16_t *)POINTER(address) = indexed * 0x101u;\n'),
    ('dither key unconditional store', dither_key_store, '        *(uint8_t *)POINTER(address) = indexed;\n'),
    ('dither key skip coordinate advance', dither_key_advance, '        if (indexed >= BYTE(0x435c)) dither_advance(&key_u, &key_v, u_integer, v_integer);\n'),
    ('dither key skip destination advance', dither_key_advance + '        if (fog) level = (level + WORD(0x4358)) & 0xffff0fffu;\n        address += direction;', dither_key_advance + '        if (fog) level = (level + WORD(0x4358)) & 0xffff0fffu;\n        if (indexed >= BYTE(0x435c)) address += direction;'),
    ('dither key coordinate order', dither_key_store + dither_key_advance, dither_key_advance + dither_key_store),
    ('dither key sample prefetch', dither_key_loop, dither_key_prefetch),
    ('dither key destination spacing', dither_key_advance + '        if (fog) level = (level + WORD(0x4358)) & 0xffff0fffu;\n        address += direction;', dither_key_advance + '        if (fog) level = (level + WORD(0x4358)) & 0xffff0fffu;\n        address += direction * 2;'),
    ('dither key live tail flag', dither_key_loop, dither_key_loop.replace('for (uint32_t key_pixel', 'uint32_t saved_skip = WORD(0x45d5);\n    for (uint32_t key_pixel').replace('count && WORD(0x45d5)', 'count && saved_skip')),
    ('dither key u step shift', 'WORD(0x41a8) *= 65536u;\n    WORD(0x41ac) *= 65536u;\n    for (uint32_t key_pixel', 'WORD(0x41a8) *= 32768u;\n    WORD(0x41ac) *= 65536u;\n    for (uint32_t key_pixel'),
    ('dither key v step shift', 'WORD(0x41ac) *= 65536u;\n    for (uint32_t key_pixel', 'WORD(0x41ac) *= 32768u;\n    for (uint32_t key_pixel'),
    ('dither key fallback key gate', 'dither = 0;\n            first_x', 'dither = keyed = 0;\n            first_x'),
    ('dither key tail target direction', 'projective_dither_key_tails[forward]', 'projective_dither_key_tails[!forward]'),
    ('dither key tail target block', 'projective_dither_key_tails[forward][shift == 4]', 'projective_dither_key_tails[forward][shift != 4]'),
    ('dither key tail target row', 'projective_dither_key_tails[forward][shift == 4][y & 1]', 'projective_dither_key_tails[forward][shift == 4][(y ^ 1) & 1]'),
    ('dither key tail target count', 'projective_dither_key_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_dither_key_tails[forward][shift == 4][y & 1][(WORD(0x45d9) + 1) % 33]'),
    ('dither key tail target family', 'keyed ? projective_dither_key_tails', 'keyed ? projective_dither_tails'),
]
mutations += [(*mutation, 'projective_dither_key') for mutation in dither_key_mutations]
dither_key_fog_level = '        if (fog) level = (level + WORD(0x4358)) & 0xffff0fffu;\n'
dither_key_fog_mutations = [
    ('dither key fog key gate', 'keyed |= WORD(0x4200) == 0x20761af9 || WORD(0x4200) == 0x20769510;', 'keyed |= 0;'),
    ('dither key fog shade gate', 'fog |= WORD(0x4200) == 0x20761af9 || WORD(0x4200) == 0x20769510;', 'fog |= 0;'),
    ('dither key fog dither gate', 'dither |= WORD(0x4200) == 0x20761af9 || WORD(0x4200) == 0x20769510;', 'dither |= 0;'),
    ('dither key fog direction', 'forward |= WORD(0x4200) == 0x20769510;', 'forward |= WORD(0x4200) == 0x20761af9;'),
    ('dither key fog block dispatch', 'if (dither && keyed) dither_key_pixels', 'if (dither && keyed && !fog) dither_key_pixels'),
    ('dither key fog tail dispatch', 'if (keyed) dither_key_pixels', 'if (keyed && !fog) dither_key_pixels'),
    ('dither key fog raw key', 'indexed >= BYTE(0x435c)', '(fog ? BYTE(0xd70 + indexed + ((level >> 8) & 255) * 256) : indexed) >= BYTE(0x435c)'),
    ('dither key fog opaque zero', '*(uint8_t *)POINTER(address) = painted;', 'if (painted) *(uint8_t *)POINTER(address) = painted;'),
    ('dither key fog row byte', '0xd70 + indexed + ((level >> 8) & 255) * 256', '0xd70 + indexed + ((level >> 9) & 255) * 256'),
    ('dither key fog lookup', '0xd70 + indexed +', '0xd71 + indexed +'),
    ('dither key fog block level', '0, fog, WORD(0x41c0)', '0, fog, WORD(0x41bc)'),
    ('dither key fog tail level', 'dither_key_pixels(pixel_address, WORD(0x45d9), 0x4190, y, direction, 1, fog, WORD(0x41bc))', 'dither_key_pixels(pixel_address, WORD(0x45d9), 0x4190, y, direction, 1, fog, WORD(0x41c4))'),
    ('dither key fog skipped shade', dither_key_fog_level, '        if (fog && indexed >= BYTE(0x435c)) level = (level + WORD(0x4358)) & 0xffff0fffu;\n'),
    ('dither key fog shade step', dither_key_fog_level, '        if (fog) level = (level - WORD(0x4358)) & 0xffff0fffu;\n'),
    ('dither key fog shade wrap', dither_key_fog_level, '        if (fog) level = level + WORD(0x4358);\n'),
    ('dither key fog shade order', dither_key_store + dither_key_advance + dither_key_fog_level, '        uint32_t next_level = (level + WORD(0x4358)) & 0xffff0fffu;\n' + dither_key_store + dither_key_advance + '        if (fog) level = next_level;\n'),
    ('dither key fog full bias', 'tail ? dither_bias(address, y) : 0', '(tail || fog) ? dither_bias(address, y) : 0'),
    ('dither key fog tail bias', 'tail ? dither_bias(address, y) : 0', 'tail && !fog ? dither_bias(address, y) : 0'),
    ('dither key fog table direction', 'projective_dither_key_fog_tails[forward]', 'projective_dither_key_fog_tails[!forward]'),
    ('dither key fog table block', 'projective_dither_key_fog_tails[forward][shift == 4]', 'projective_dither_key_fog_tails[forward][shift != 4]'),
    ('dither key fog table row', 'projective_dither_key_fog_tails[forward][shift == 4][y & 1]', 'projective_dither_key_fog_tails[forward][shift == 4][(y ^ 1) & 1]'),
    ('dither key fog table count', 'projective_dither_key_fog_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_dither_key_fog_tails[forward][shift == 4][y & 1][(WORD(0x45d9) + 1) % 33]'),
    ('dither key fog table family', 'fog && keyed ? projective_dither_key_fog_tails', 'fog && keyed ? projective_dither_key_tails'),
]
mutations += [(*mutation, 'projective_dither_key_fog') for mutation in dither_key_fog_mutations]
device_colour_mutations = [
    ('vertex colour early gate', 'GLOBAL_HALF(0xf10) &&', 'GLOBAL[0xf10] &&'),
    ('vertex colour dce gate', '!GLOBAL_HALF(0xdce)', '!GLOBAL[0xdce]'),
    ('vertex colour visibility gate', 'GLOBAL_WORD(0x398) &&', '1 &&'),
    ('vertex colour mode equality', 'GLOBAL_HALF(0x1172) == 1', 'GLOBAL_HALF(0x1172) != 0'),
    ('vertex colour early alpha', '0xeeffffff', '0xedffffff'),
    ('vertex colour texture word', 'GLOBAL_HALF(0xc90) && GLOBAL_HALF(0xca0)', 'GLOBAL[0xc90] && GLOBAL_HALF(0xca0)'),
    ('vertex colour shadow key', 'GLOBAL_HALF(0xca0) == 0x98', 'GLOBAL[0xca0] == 0x98'),
    ('vertex colour shadow e82', '!GLOBAL_HALF(0xe82)', '!GLOBAL[0xe82]'),
    ('vertex colour shadow alpha', '0xccffffff', '0xcbffffff'),
    ('vertex colour texture metadata', 'GLOBAL_WORD(0xc8c)))[2]', 'GLOBAL_WORD(0xc8c)))[1]'),
    ('vertex colour translucent gate', 'GLOBAL_HALF(0xdca)', 'GLOBAL[0xdca]'),
    ('vertex colour half equality', 'GLOBAL_HALF(0xb34) < 0x8000', 'GLOBAL_HALF(0xb34) <= 0x8000'),
    ('vertex colour half black', '0x80000000 : 0x80ffffff', '0x80000001 : 0x80ffffff'),
    ('vertex colour override last', 'index < 28 ? index : 3', 'index < 27 ? index : 3'),
    ('vertex colour override fallback', 'index < 28 ? index : 3', 'index < 28 ? index : 2'),
    ('vertex colour water equality', 'GLOBAL_HALF(0xb34) >= 0x8000', 'GLOBAL_HALF(0xb34) > 0x8000'),
    ('vertex colour water dce', 'GLOBAL_HALF(0xdce) ? 0x80ff3f00', 'GLOBAL[0xdce] ? 0x80ff3f00'),
    ('vertex colour water constant', '0x80ff3f00', '0x80ff3f01'),
    ('vertex colour red', 'BYTE(0x39c2 + index * 3)', 'BYTE(0x39c4 + index * 3)'),
    ('vertex colour green', 'BYTE(0x39c3 + index * 3)', 'BYTE(0x39c2 + index * 3)'),
    ('vertex colour palette index', 'index = BYTE(0x90c)', 'index = BYTE(0x90d)'),
]
mutations += [(*mutation, 'device_colour') for mutation in device_colour_mutations]
device_flush_mutations = [
    ('flush vertex count', '>> 5', '>> 4'),
    ('flush precision', 'control = (control & ~0x100u) | 0x200u;', 'control = (control & ~0x300u);'),
    ('flush empty path', 'if (!count) return;', 'if (0) return;'),
    ('flush fog gate', 'if (WORD(0x5f24))', 'if (1)'),
    ('flush fog boolean', 'WORD(0x884c) != 0', 'WORD(0x884c) == 1'),
    ('flush fog clear', 'WORD(0x884c) = 0;', 'WORD(0x884c) = 1;'),
    ('flush key boolean', 'WORD(0x8844) != 0', 'WORD(0x8844) == 1'),
    ('flush key clear', 'WORD(0x8844) = 0;', 'WORD(0x8844) = 1;'),
    ('flush points live mode', 'device_primitive(mode,', 'device_primitive(WORD(0x8850),'),
    ('flush points type', 'device_primitive(mode,', 'device_primitive(mode + 1,'),
    ('flush flat alpha state', 'mode == 4 ? 1 : 0', 'mode != 4 ? 1 : 0'),
    ('flush flat shade state', 'mode == 8 ? 2 : 1', 'mode == 8 ? 1 : 2'),
    ('flush texture flags', 'texture[1] & 0x100000', 'texture[1] & 0x200000'),
    ('flush texture flag mode', 'mode == 5 && texture', 'mode != 5 && texture'),
    ('flush texture min filter', 'hg2d_device_draw_state(17, 1);', 'hg2d_device_draw_state(17, 2);'),
    ('flush texture mag filter', 'hg2d_device_draw_state(18, 1);', 'hg2d_device_draw_state(18, 2);'),
    ('flush default min filter', 'WORD(0x289e4)', 'WORD(0x289e8)'),
    ('flush alpha mask', 'WORD(0x8858) & 0xff000000u', 'WORD(0x8858) & 0x00ff0000u'),
    ('flush alpha clear', 'WORD(0x8858) = 0;', 'WORD(0x8858) = 1;'),
    ('flush texture blend', 'mode == 6 ? 4 : 2', 'mode == 6 ? 2 : 4'),
    ('flush shade ordering', 'if (mode != 7) hg2d_device_draw_state(9, 2);', 'hg2d_device_draw_state(9, 2);'),
    ('flush cached alpha mode', 'hg2d_device_draw_state(27, mode == 6 ? 1 : 0);', 'hg2d_device_draw_state(27, WORD(0x8850) == 6 ? 1 : 0);'),
    ('flush primitive type', '(instance, kind, 3, vertices, count, 0xd)', '(instance, kind, 2, vertices, count, 0xd)'),
    ('flush primitive flags', '(instance, kind, 3, vertices, count, 0xd)', '(instance, kind, 3, vertices, count, 0xc)'),
    ('flush capacity reset', 'WORD(0x5ee8) = 0x1000;', 'WORD(0x5ee8) = 0xfff;'),
    ('flush first cursor reset', 'WORD(0x5ee0) = WORD(0x5ee4) = ADDRESS(hg2d_image + 0x8860);', 'WORD(0x5ee4) = ADDRESS(hg2d_image + 0x8860);'),
    ('flush texture reset', 'WORD(0x8840) = WORD(0x885c) = WORD(0x8850) = 0;', 'WORD(0x8840) = WORD(0x8850) = 0;'),
]
mutations += [(*mutation, 'device_flush') for mutation in device_flush_mutations]
device_select_format_mutations = [
    ('select format configured default', 'configured = 0xffffffffu', 'configured = 0'),
    ('select format config key', 'hg2d_image + 0x5e8c', 'hg2d_image + 0x5e9c'),
    ('select format configured gate', 'configured != 0xffffffffu', 'configured == 0xffffffffu'),
    ('select format requested alpha', 'configured, 1, out', 'configured, 0, out'),
    ('select format indexed flag', 'flags & 0x400', 'flags & 0x200'),
    ('select format indexed alpha', '8, 1, out', '8, 0, out'),
    ('select format first fallback', '15, 1, out', '16, 0, out'),
    ('select format alpha fallback', '12, 4, out', '12, 0, out'),
    ('select format final fallback', '16, 0, out', '15, 0, out'),
    ('select format order', 'if (hg2d_device_formats(WORD(0x289d0), 15, 1, out)) return 1;\n    if (hg2d_device_formats(WORD(0x289d0), 12, 4, out)) return 1;', 'if (hg2d_device_formats(WORD(0x289d0), 12, 4, out)) return 1;\n    if (hg2d_device_formats(WORD(0x289d0), 15, 1, out)) return 1;'),
    ('select format destination', 'hg2d_image + 0x28ae0', 'hg2d_image + 0x28ae4'),
]
mutations += [(*mutation, 'device_select_format') for mutation in device_select_format_mutations]
device_formats_mutations = [
    ('formats mask popcount', 'bits += mask & 1', 'bits += mask & 3'),
    ('formats alpha capability', 'BYTE(0x28a84) & 8', 'BYTE(0x28a84) & 4'),
    ('formats indexed alpha', 'context[1] != 1', 'context[1] != 0'),
    ('formats signed alpha', '(int32_t)device_mask_bits(format[7]) < (int32_t)context[1]', 'device_mask_bits(format[7]) < context[1]'),
    ('formats alpha equality', '(int32_t)device_mask_bits(format[7]) < (int32_t)context[1]', '(int32_t)device_mask_bits(format[7]) <= (int32_t)context[1]'),
    ('formats indexed minimum', 'if (format[3] < requested)', 'if (format[3] <= requested)'),
    ('formats colour minimum', 'if ((int32_t)bits < (int32_t)requested)', 'if ((int32_t)bits <= (int32_t)requested)'),
    ('formats signed minimum', 'if ((int32_t)bits < (int32_t)requested)', 'if (bits < requested)'),
    ('formats exact indexed', 'if (requested == 8 && format[3] != requested) return 1;', 'if (0) return 1;'),
    ('formats recognition', 'if (!hg2d_device_format(format)) return 1;', 'if (0) return 1;'),
    ('formats candidate tie', 'format[3] - requested >= context[5] - requested', 'format[3] - requested > context[5] - requested'),
    ('formats candidate priority', 'format[3] - requested >= context[5] - requested', 'format[3] - requested <= context[5] - requested'),
    ('formats selected descriptor', '__builtin_memcpy(context + 2, format, sizeof format);', '__builtin_memcpy(context + 2, format, sizeof format - 4);'),
    ('formats initialized output', '__builtin_memcpy(out, context + 2, 32);', '__builtin_memcpy(out, context + 2, 28);'),
    ('formats boolean result', 'return context[3] != 0;', 'return context[3];'),
    ('formats query HRESULT', '((call_three)(uintptr_t)vtable[9])(instance, ADDRESS(device_format_match), ADDRESS(context));', 'if (((call_three)(uintptr_t)vtable[9])(instance, ADDRESS(device_format_match), ADDRESS(context))) context[3] = 0;'),
    ('formats depth 16 alpha', 'if (bits == 16 && alpha == 1) bits = 15;', 'if (bits == 16 && alpha == 1) bits = 16;'),
    ('formats depth 12 alpha', 'if (bits == 12 && alpha == 1) alpha = 4;', 'if (bits == 12 && alpha == 1) alpha = 1;'),
    ('formats legacy 22b', 'if (bits == 0x22b) { bits = 15; alpha = 0; }', 'if (bits == 0x22b) { bits = 16; alpha = 0; }'),
    ('formats legacy 15af', 'if (bits == 0x15af) { bits = 15; alpha = 1; }', 'if (bits == 0x15af) { bits = 16; alpha = 1; }'),
    ('formats legacy 235', 'if (bits == 0x235) { bits = 16; alpha = 0; }', 'if (bits == 0x235) { bits = 17; alpha = 0; }'),
    ('formats legacy 1bc', 'if (bits == 0x1bc) { bits = 12; alpha = 0; }', 'if (bits == 0x1bc) { bits = 13; alpha = 0; }'),
    ('formats legacy 115c', 'if (bits == 0x115c) { bits = 12; alpha = 4; }', 'if (bits == 0x115c) { bits = 13; alpha = 4; }'),
    ('formats legacy 378', 'if (bits == 0x378) { bits = 24; alpha = 0; }', 'if (bits == 0x378) { bits = 25; alpha = 0; }'),
    ('formats legacy 22b8', 'if (bits == 0x22b8) { bits = 24; alpha = 8; }', 'if (bits == 0x22b8) { bits = 25; alpha = 8; }'),
]
mutations += [(*mutation, 'device_formats') for mutation in device_formats_mutations]
device_shutdown_mutations = [
    ('shutdown last interface', 'index < sizeof interfaces / sizeof interfaces[0]', 'index + 1 < sizeof interfaces / sizeof interfaces[0]'),
    ('shutdown release order', '{0x28940, 0x28ccc,', '{0x28ccc, 0x28940,'),
    ('shutdown cache count', 'index < 8; index++', 'index < 7; index++'),
    ('shutdown last texture', 'offset < POOL_END', 'offset + 0x34 < POOL_END'),
    ('shutdown textures skipped', 'if (texture[3]) texture_release(texture);', 'if (0) texture_release(texture);'),
    ('shutdown clear object', 'WORD(offset) = 0;', 'WORD(offset) = instance;'),
    ('shutdown publish release', '((call_one)(uintptr_t)vtable[2])(instance);\n            WORD(offset) = 0;', 'WORD(offset) = 0;\n            ((call_one)(uintptr_t)vtable[2])(instance);'),
    ('shutdown accounting', 'WORD(0x289ec) = WORD(0x289d4) = WORD(0x5f40) = 0;', 'WORD(0x289ec) = WORD(0x289d4) = WORD(0x5f40) = 1;'),
    ('shutdown interface 28940', '0x28940', '0x289d8'),
    ('shutdown interface 28ccc', '0x28ccc', '0x289d8'),
    ('shutdown interface 289f4', '0x289f4', '0x289d8'),
    ('shutdown interface 289d0', '0x289d0', '0x289d8'),
    ('shutdown interface 28cc0', '0x28cc0', '0x289d8'),
    ('shutdown interface 289f0', '0x289f0', '0x289d8'),
]
mutations += [(*mutation, 'device_shutdown') for mutation in device_shutdown_mutations]
device_config_mutations = [
    ('config blend default', 'WORD(0x289e0) = 0xffffffffu;', 'WORD(0x289e0) = 0;'),
    ('config blend disabled', 'if (WORD(0x289e0))', 'if (1)'),
    ('config blend alpha cap', 'capabilities & 0x200 ? 1', 'capabilities & 0x100 ? 1'),
    ('config blend second cap', '((capabilities & 255) >> 6) & 2', '((capabilities & 255) >> 6) & 1'),
    ('config fog flag', 'flags & 0x1000', 'flags & 0x800'),
    ('config fog capability', 'BYTE(0x28a6c) & 0x80', 'BYTE(0x28a6c) & 0x40'),
    ('config fog enabled', 'WORD(0x5f24) = 1;', 'WORD(0x5f24) = 0;'),
    ('config fog table state', 'hg2d_device_state(35, 0);', 'hg2d_device_state(35, 1);'),
    ('config filter selector', 'flags & 0x10 ? 2 : 1', 'flags & 0x20 ? 2 : 1'),
    ('config filter min slot', 'WORD(0x289d8) = WORD(0x289dc) = filter;', 'WORD(0x289d8) = filter;'),
    ('config filter mip scale', 'filter + 2 : filter', 'filter + 1 : filter'),
    ('config filter mag slot', 'WORD(0x289e4) = filter;', 'WORD(0x289e4) = filter + 1;'),
    ('config mip capability', 'BYTE(0x28a88) & 0x18', 'BYTE(0x28a88) & 0x8'),
    ('config mip flag', '&& (flags & 0x80)', '&& (flags & 0x40)'),
    ('config texture format', 'WORD(0x28aec) != 8', 'WORD(0x28aec) == 8'),
    ('config texture flag', 'flags & 0x800', 'flags & 0x400'),
    ('config mip default', 'WORD(0x5f30) = 0xffffffffu;', 'WORD(0x5f30) = 0x40;'),
    ('config override 256', 'WORD(0x5f30) == 0x100', 'WORD(0x5f30) == 0x101'),
    ('config override 128', 'WORD(0x5f30) == 0x80', 'WORD(0x5f30) == 0x81'),
    ('config override 64', 'WORD(0x5f30) == 0x40', 'WORD(0x5f30) == 0x41'),
    ('config mip key', 'ADDRESS(hg2d_image + 0x5ea8)', 'ADDRESS(hg2d_image + 0x5e9c)'),
    ('config blend key', 'ADDRESS(hg2d_image + 0x5e9c)', 'ADDRESS(hg2d_image + 0x5ea8)'),
]
mutations += [(*mutation, 'device_config') for mutation in device_config_mutations]
device_states_mutations = [
    ('state cache equality', 'if (WORD(0x28b00 + state * 4) != value)', 'if (1)'),
    ('state cache index', 'WORD(0x28b00 + state * 4) = value;', 'WORD(0x28b00 + state * 4 + 4) = value;'),
    ('state cache ordering', 'WORD(0x28b00 + state * 4) = value;\n        WORD(result_offset) = ((call_three)(uintptr_t)vtable[23])(instance, state, value);', 'WORD(result_offset) = ((call_three)(uintptr_t)vtable[23])(instance, state, value);\n        WORD(0x28b00 + state * 4) = value;'),
    ('state method value', '(instance, state, value);', '(instance, state, value ^ 1);'),
    ('state init HRESULT', 'device_state_set(state, value, 0x5f20);', 'device_state_set(state, value, 0x5f34);'),
    ('state draw HRESULT', 'device_state_set(state, value, 0x5f34);', 'device_state_set(state, value, 0x5f20);'),
    ('state first query', 'state = 1; state < 64', 'state = 2; state < 64'),
    ('state last query', 'state < 64; state++', 'state < 63; state++'),
    ('state query order', 'state = 1; state < 64; state++', 'state = 63; state > 0; state--'),
    ('state failed query', '((call_three)(uintptr_t)vtable[22])(instance, state, ADDRESS(hg2d_image + 0x28b00 + state * 4));', 'WORD(0x28b00 + state * 4) = 0;\n        ((call_three)(uintptr_t)vtable[22])(instance, state, ADDRESS(hg2d_image + 0x28b00 + state * 4));'),
    ('state query result discarded', '((call_three)(uintptr_t)vtable[22])', 'WORD(0x5f20) = ((call_three)(uintptr_t)vtable[22])'),
    ('state default 7', 'hg2d_device_state(7, 0);', 'hg2d_device_state(7, 1);'),
    ('state default 14', 'hg2d_device_state(14, 0);', 'hg2d_device_state(14, 1);'),
    ('state default 9', 'hg2d_device_state(9, 1);', 'hg2d_device_state(9, 0);'),
    ('state default 22', 'hg2d_device_state(22, 1);', 'hg2d_device_state(22, 0);'),
    ('state default 28', 'hg2d_device_state(28, 0);', 'hg2d_device_state(28, 1);'),
    ('state default 4', 'hg2d_device_state(4, 1);', 'hg2d_device_state(4, 0);'),
    ('state default 1', 'hg2d_device_state(1, 0);', 'hg2d_device_state(1, 1);'),
    ('state default 21', 'hg2d_device_state(21, 2);', 'hg2d_device_state(21, 3);'),
    ('state default 3', 'hg2d_device_state(3, 1);', 'hg2d_device_state(3, 0);'),
    ('state default 27', 'hg2d_device_state(27, 0);', 'hg2d_device_state(27, 1);'),
    ('state default 41', 'hg2d_device_state(41, 1);', 'hg2d_device_state(41, 0);'),
    ('state default 39', 'hg2d_device_state(39, 0);', 'hg2d_device_state(39, 1);'),
    ('state default 19', 'hg2d_device_state(19, 5);', 'hg2d_device_state(19, 4);'),
    ('state default 20', 'hg2d_device_state(20, 6);', 'hg2d_device_state(20, 7);'),
    ('state flag selector', '(flags & 0x20) != 0', '(flags & 0x10) != 0'),
    ('state flag value', '(flags & 0x20) != 0', 'flags & 0x20'),
    ('state source 17', 'hg2d_device_state(17, WORD(0x289e4));', 'hg2d_device_state(17, WORD(0x289e8));'),
    ('state source 18', 'hg2d_device_state(18, WORD(0x289e8));', 'hg2d_device_state(18, WORD(0x289e4));'),
]
mutations += [(*mutation, 'device_states') for mutation in device_states_mutations]
device_pixels_mutations = [
    ('format indexed flag', '(format[1] & 0x20)', '(format[1] & 0x10)'),
    ('format indexed depth', 'format[3] == 8', 'format[3] == 9'),
    ('format 555 red mask', 'format[4] == 0x7c00', 'format[4] == 0x7c01'),
    ('format 565 green mask', 'format[5] == 0x7e0', 'format[5] == 0x7e1'),
    ('format 444 blue mask', 'format[6] == 0xf)', 'format[6] == 0xe)'),
    ('format 32 masks', 'format[6] == 0xff', 'format[6] == 0xfe'),
    ('format unsupported', 'return 0;\n}', 'return 1;\n}'),
    ('convert odd pitch', '((int32_t)destination_pitch / 2) * 2', '((int32_t)destination_pitch >> 1) * 2'),
    ('convert pitch bytes', 'stride = (uint32_t)(((int32_t)destination_pitch / 2) * 2);', 'stride = destination_pitch;'),
    ('convert last row', '(int32_t)y < (int32_t)height', '(int32_t)y < (int32_t)height - 1'),
    ('convert full key', 'indexed < key', 'indexed < (key & 255)'),
    ('convert unsigned key', 'indexed < key', '(int32_t)indexed < (int32_t)key'),
    ('convert key equality', 'indexed < key', 'indexed <= key'),
    ('convert special key', '(uint8_t)key == 0x98 ? 0x96', '(uint8_t)key == 0x98 ? 0x97'),
    ('convert other key', '(uint8_t)(key - 1)', '(uint8_t)key'),
    ('convert 32 blue', 'destination[x * 4] = BYTE(0x39c4 + indexed * 3);', 'destination[x * 4] = BYTE(0x39c2 + indexed * 3);'),
    ('convert 32 live palette', 'destination[x * 4] = BYTE(0x39c4 + indexed * 3);\n                destination[x * 4 + 1] = BYTE(0x39c3 + indexed * 3);\n                destination[x * 4 + 2] = BYTE(0x39c2 + indexed * 3);', 'uint8_t saved_red = BYTE(0x39c2 + indexed * 3), saved_green = BYTE(0x39c3 + indexed * 3);\n                destination[x * 4] = BYTE(0x39c4 + indexed * 3);\n                destination[x * 4 + 1] = saved_green;\n                destination[x * 4 + 2] = saved_red;'),
    ('convert 32 alpha', 'indexed >= (key & 255) ? 255 : 0', 'indexed > (key & 255) ? 255 : 0'),
    ('convert 555 red', '((red >> 3) << 10)', '((red >> 3) << 11)'),
    ('convert 555 alpha', 'packed |= 0x8000', 'packed |= 0x4000'),
    ('convert 565 green', '((green >> 2) << 5)', '((green >> 3) << 5)'),
    ('convert 444 red', '((red >> 4) << 8)', '((red >> 3) << 8)'),
    ('convert 444 alpha', 'packed |= 0xf000', 'packed |= 0x8000'),
    ('convert source rows', 'ADDRESS(source) + source_pitch', 'ADDRESS(source) + source_pitch + 1'),
    ('convert destination rows', 'ADDRESS(destination) + stride', 'ADDRESS(destination) + stride + 1'),
]
mutations += [(*mutation, 'device_pixels') for mutation in device_pixels_mutations]
device_palette_mutations = [
    ('device palette binding gate', 'if (!WORD(0x28940)) return 0;', 'if (0) return 0;'),
    ('device palette cache count', 'for (index = 0; index < 8; index++) WORD(0x8820 + index * 4) = 0;', 'for (index = 0; index < 7; index++) WORD(0x8820 + index * 4) = 0;'),
    ('device palette release guard', 'if (texture[3]) texture_release(texture);', 'texture_release(texture);'),
    ('device palette accounting reset', 'WORD(0x289ec) = WORD(0x289d4) = WORD(0x5f40) = 0;', 'WORD(0x289ec) = WORD(0x289d4) = WORD(0x5f40) = 1;'),
    ('device palette dirty ownership', 'if (WORD(offset + 12)) WORD(offset + 4) |= 0x20000;', 'if (WORD(offset + 8)) WORD(offset + 4) |= 0x20000;'),
    ('device palette dirty flag', 'WORD(offset + 4) |= 0x20000;', 'WORD(offset + 4) |= 0x80000;'),
    ('device palette red channel', 'entries[index * 4] = BYTE(0x39c2 + index * 3);', 'entries[index * 4] = BYTE(0x39c3 + index * 3);'),
    ('device palette alpha', 'entries[index * 4 + 3] = 0;', 'entries[index * 4 + 3] = 1;'),
    ('device palette factory flags', '(instance, 4, ADDRESS(entries), ADDRESS(&palette), 0)', '(instance, 5, ADDRESS(entries), ADDRESS(&palette), 0)'),
    ('device palette output initialization', 'index, palette = 0, instance', 'index, palette = 1, instance'),
    ('device palette release on failure', 'if (instance) {', 'if (instance && palette) {'),
    ('device palette zero replacement', 'WORD(0x289f4) = palette;', 'if (palette) WORD(0x289f4) = palette;'),
    ('device fog base scale', 'GLOBAL_HALF(0xcba) * 180u >> 16', 'GLOBAL_HALF(0xcba) * 181u >> 16'),
    ('device fog intensity equality', 'intensity >= 100', 'intensity > 100'),
    ('device fog lower depth equality', 'depth >= 0x1c52', 'depth > 0x1c52'),
    ('device fog upper depth equality', 'depth < 0x7d00', 'depth <= 0x7d00'),
    ('device fog time boundary', 'GLOBAL[0x238] < 12', 'GLOBAL[0x238] <= 12'),
    ('device fog angle offset', 'angle -= 0x40000000u;', 'angle -= 0x20000000u;'),
    ('device fog signed magnitude', '(int32_t)magnitude <= 0x30000000', 'magnitude <= 0x30000000'),
    ('device fog visibility flag', 'WORD(0x28860) = 1;', 'WORD(0x28860) = 0;'),
    ('device fog signed division', '(int32_t)product / 12288', '(int32_t)product / 12289'),
    ('device fog blend scale', '(uint32_t)amount * 850u', '(uint32_t)amount * 849u'),
    ('device fog blend divisor', '(int32_t)scaled / 1024', '(int32_t)scaled / 1023'),
    ('device fog render state cache', 'if (WORD(0x28b88) != packed)', 'if (1)'),
    ('device fog render state id', '(instance, 0x22, packed)', '(instance, 0x23, packed)'),
    ('device fog HRESULT', 'WORD(0x5f20) = ((call_three)', 'WORD(0x5f20) = 0x100 + ((call_three)'),
    ('device scaled last colour', 'index < 28;', 'index < 27;'),
    ('device scaled alpha', 'value = source & 0xff000000u;', 'value = source & 0xffff0000u;'),
    ('device scaled red fraction', '((source >> 16) & 255) * brightness >> 16', '((source >> 16) & 255) * brightness >> 15'),
    ('device scaled return', 'return value;', 'return 0;'),
]
mutations += [(*mutation, 'device_palettes') for mutation in device_palette_mutations]
fan_mutations = [
    ('fan kind gate', 'if (kind != 6 || format != 100)', 'if (kind != 7 || format != 100)'),
    ('fan format gate', 'format != 100', 'format != 99'),
    ('fan enable word', '!GLOBAL_HALF(0xcb6)', '!GLOBAL[0xcb6]'),
    ('fan key word', '|| GLOBAL_HALF(0xca0)', '|| GLOBAL[0xca0]'),
    ('fan early eax', 'return entry_eax;', 'return 0;'),
    ('fan global eax', 'return ADDRESS(GLOBAL);', 'return 0;'),
    ('fan short count', '(int32_t)(count - 1) <= 1', '(int32_t)(count - 1) <= 0'),
    ('fan last vertex return', 'return ADDRESS(vertices + (count - 1) * 5);', 'return ADDRESS(vertices + (count - 2) * 5);'),
    ('fan triangle count', 'index < count - 1;', 'index < count - 2;'),
    ('fan first vertex anchor', 'fan_triangle(surface, vertices,', 'fan_triangle(surface, vertices + 5,'),
    ('fan slope floating arithmetic', '(long double)numerator * 65536.0L / (int32_t)height', '(long double)(height ? numerator * 65536 / (int32_t)height : 0)'),
    ('fan slope truncation', 'orl $0xc00,%%eax', 'andl $0xf3ff,%%eax'),
    ('fan slope control restore', 'fistpq %0\\n\\tfldcw %1', 'fistpq %0\\n\\tfldcw %2'),
    ('fan long u unsigned delta', 'fan_slope(bottom[3] - top[3], height, 1)', 'fan_slope(bottom[3] - top[3], height, 0)'),
    ('fan short v unsigned delta', 'fan_slope(middle[4] - top[4], height, 1)', 'fan_slope(middle[4] - top[4], height, 0)'),
    ('fan span signed x sort', '(int32_t)first[0] > (int32_t)last[0]', 'first[0] > last[0]'),
    ('fan span inclusive count', '(high_x - low_x + 1)', '(high_x - low_x)'),
    ('fan span signed u delta', '(int64_t)(int32_t)(last[2] - u)', '(int64_t)(uint32_t)(last[2] - u)'),
    ('fan span v start', 'v = first[4];', 'v = first[2];'),
    ('fan colour threshold equality', '(int32_t)texture < 255', '(int32_t)texture <= 255'),
    ('fan colour threshold sign', '(int32_t)texture < 255', 'texture < 255'),
    ('fan texture u byte', '((u >> 16) & 255)', '((u >> 15) & 255)'),
    ('fan texture row stride', '((v >> 16) & 255) * 256', '((v >> 16) & 255) * 128'),
    ('fan vertex signed y sort', 'if ((int32_t)top[1] > (int32_t)middle[1]) { swap = top; top = middle; middle = swap; }\n    if ((int32_t)middle[1]', 'if (top[1] > middle[1]) { swap = top; top = middle; middle = swap; }\n    if ((int32_t)middle[1]'),
    ('fan flat row rejection', 'if (!((top[1] ^ bottom[1]) & 0xffff0000u)) return;', 'if (((top[1] ^ bottom[1]) & 0xffff0000u)) return;'),
    ('fan long edge unsigned x', 'fan_slope(bottom[0] - top[0], height, 0)', 'fan_slope(bottom[0] - top[0], height, 1)'),
    ('fan fixed row stride', 'row += stride;', 'row += stride + 1;'),
    ('fan short edge advance', 'short_edge[component] += short_edge[component + 1];', 'short_edge[component] -= short_edge[component + 1];'),
    ('fan long edge advance', 'long_edge[component] += long_edge[component + 1];', 'long_edge[component] -= long_edge[component + 1];'),
    ('fan second half', 'top = middle; middle = bottom;', 'return;'),
]
mutations += [(*mutation, 'fans') for mutation in fan_mutations]
mip_build_mutations = [
    ('mip build tie order', 'if (distance < minimum)', 'if (distance <= minimum)'),
    ('mip build RGB source', 'average[channel] += BYTE(0x39c2 + samples[index] * 3 + channel)', 'average[channel] += BYTE(0x36c2 + samples[index] * 3 + channel)'),
    ('mip build average divide', 'average[channel] >>= 2;', 'average[channel] >>= 1;'),
    ('mip build distance metric', '(uint32_t)(difference * difference)', '(uint32_t)(difference < 0 ? -difference : difference)'),
    ('mip build RGB average channels', 'channel < 3; channel++) average[channel] +=', 'channel < 2; channel++) average[channel] +='),
    ('mip build RGB distance channels', 'for (channel = 0; channel < 3; channel++) {', 'for (channel = 0; channel < 2; channel++) {'),
    ('mip build candidate count', 'for (index = 0; index < 4; index++) {', 'for (index = 0; index < 3; index++) {'),
    ('mip build range check', '!texture_inside(ADDRESS(texture))', '0'),
    ('mip build resident cache', '!(texture[1] & 0x40000)', '!(texture[1] & 0x80000)'),
    ('mip build growth result', 'if (grow(4, 0x18000, ADDRESS(texture))) return 0;', 'if (!grow(4, 0x18000, ADDRESS(texture))) return 0;'),
    ('mip build zero padding', '*(uint8_t *)POINTER(destination + index) = 0;', '*(uint8_t *)POINTER(destination + index) = 0x5a;'),
    ('mip build final level', 'size > 1; size >>= 1', 'size > 2; size >>= 1'),
    ('mip build width', 'x < size; x += 2', 'x < size; x += 4'),
    ('mip build height', 'y < size; y += 2', 'y < size; y += 4'),
    ('mip build source pitch', 'source + y * 256 + x', 'source + y * 128 + x'),
    ('mip build destination pitch', 'destination + (y >> 1) * 256', 'destination + (y >> 1) * 128'),
    ('mip build packed offsets', 'destination += size >> 1;', 'destination += size >> 2;'),
    ('mip build resident flag', 'texture[1] |= 0x40000;', 'texture[1] |= 0x80000;'),
    ('mip build fallback test', '&& !texture_mip_build(texture, 1)', '&& texture_mip_build(texture, 1)'),
    ('mip build fallback selector', 'selector &= ~16u;', 'selector = (selector & ~16u) | 8;'),
]
mutations += [(*mutation, 'projective_mip_build') for mutation in mip_build_mutations]
init_mutations = [
    ('init edge base', 'GLOBAL_WORD(0xc10) = GLOBAL_WORD(0xc14) = ADDRESS(hg2d_image + 0x104);', 'GLOBAL_WORD(0xc10) = GLOBAL_WORD(0xc14) = ADDRESS(hg2d_image + 0x108);'),
    ('init shadow count', 'index < 33;', 'index < 32;'),
    ('init glyph base', 'resource_remap_rows(0x4e00);', 'resource_remap_rows(0x4f00);'),
    ('init colour count', 'index < 0x29a4;', 'index < 0x29a3;'),
    ('init initial haze update', 'resource_haze_tick_body(0, 0, result);', 'result = result;'),
    ('init architecture', '!information.wProcessorArchitecture && result >= 586', 'information.wProcessorArchitecture == 1 && result >= 586'),
    ('init processor equality', 'result >= 586', 'result > 586'),
    ('init cached protection', 'if (!result) {', 'if (1) {'),
    ('init protection result', 'WORD(0x5e00) = result;', 'WORD(0x5e00) = 0;'),
    ('init optimized selector', 'if (result) WORD(0x4678) = 16;', 'if (result) WORD(0x4678) = 8;'),
    ('haze enabled target', 'GLOBAL_HALF(0xcd2) &&', '0 &&'),
    ('haze signed target', '(int16_t)GLOBAL_HALF(0xe8a) < 0x12f3', 'GLOBAL_HALF(0xe8a) < 0x12f3'),
    ('haze target ceiling', '? GLOBAL_HALF(0xe8a) : 0x12f3', '? GLOBAL_HALF(0xe8a) : 0x12f2'),
    ('haze previous target', 'HALF(0xd56) = previous;', 'HALF(0xd56) = target;'),
    ('haze immediate flag', 'GLOBAL[0x365] & 8', 'GLOBAL[0x365] & 4'),
    ('haze unchanged return', 'if (!delta) return', 'if (delta == 1) return'),
    ('haze elapsed multiplier', '300u * GLOBAL_HALF(0x2aa)', '301u * GLOBAL_HALF(0x2aa)'),
    ('haze signed current', '(int64_t)(int16_t)HALF(0xd54)', '(int64_t)(uint16_t)HALF(0xd54)'),
    ('haze doubled movement', '* elapsed * 2;', '* elapsed;'),
    ('haze movement fraction', '(uint64_t)product >> 8', '(uint64_t)product >> 9'),
    ('haze maximum step', '(change >> 4) + 1', '(change >> 3) + 1'),
    ('haze step cap', 'if (movement_integer > change) movement_integer = change;', 'if (movement_integer > change) movement_integer = (uint16_t)(change + 1);'),
    ('haze arrival equality', '(int16_t)movement_integer <= (int16_t)distance', '(int16_t)movement_integer < (int16_t)distance'),
    ('haze movement integer', '(uint32_t)movement_integer << 16', '(uint32_t)movement_integer << 15'),
    ('haze movement direction', 'WORD(0xd52) += (int16_t)delta < 0', 'WORD(0xd52) += (int16_t)delta > 0'),
    ('haze breakpoint signed overflow', '(int16_t)current > (int16_t)HALF(index)', '(int16_t)difference > 0'),
    ('haze breakpoint last record', 'index == 0x1d6c', 'index == 0x1d70'),
    ('haze breakpoint accumulation', 'accumulated + (uint32_t)difference * HALF(index + 2)', 'accumulated + difference'),
    ('haze fog scale', 'GLOBAL_HALF(0xe88) = accumulated >> 1;', 'GLOBAL_HALF(0xe88) = accumulated >> 2;'),
    ('haze early eax upper', '(entry_eax & 0xffff0000u) | previous', 'previous'),
]
mutations += [(*mutation, 'initialization') for mutation in init_mutations]
resource_mutations = [
    ('resource row count', 'row < 16;', 'row < 15;'),
    ('resource clear byte', 'BYTE(source + index) = 0;', 'BYTE(source + index) = 1;'),
    ('resource destination word', 'BYTE(source + HALF(0x34c0 + index * 2))', 'BYTE(source + BYTE(0x34c0 + index * 2))'),
    ('resource source remap', 'BYTE(0x34c0 + BYTE(0xc4e + index) * 2)', 'BYTE(0xc4e + index)'),
    ('resource scratch copy', 'BYTE(0xc4e + index) = BYTE(source + index);', 'BYTE(0xc4e + index) = 0;'),
    ('resource neutral red equality', 'red - green <= tolerance', 'red - green < tolerance'),
    ('resource neutral blue equality', 'blue - green <= tolerance', 'blue - green < tolerance'),
    ('resource neutral cross equality', 'red - blue <= tolerance', 'red - blue < tolerance'),
    ('resource neutral darkness', '(int32_t)brightness <= (int32_t)WORD(0x4d64)', '(int32_t)brightness < (int32_t)WORD(0x4d64)'),
    ('resource neutral brightness', 'uint32_t brightness = red + green + blue;', 'uint32_t brightness = red + green;'),
    ('resource neutral search equality', '(int16_t)brightness <= (int16_t)sum', '(int16_t)brightness < (int16_t)sum'),
    ('resource neutral ramp start', 'result = WORD(0x4d58);', 'result = WORD(0x4d5c);'),
    ('resource neutral start bounds', 'WORD(0x4d58) = HALF(0x34c0 + WORD(0x4d50) * 2);', 'WORD(0x4d58) = WORD(0x4d50);'),
    ('resource neutral last row', 'index < 12;', 'index < 11;'),
    ('resource neutral secondary source', 'index < 8 ? 0xd70 : 0xe70', 'index < 8 ? 0xd70 : 0xd70'),
    ('resource neutral secondary tolerance', 'index < 8 ? 256 : 130', 'index < 8 ? 256 : 129'),
    ('resource haze clamp', 'if ((int16_t)quotient > 6) quotient = 6;', 'if ((int16_t)quotient > 5) quotient = 5;'),
    ('resource haze divisor', 'GLOBAL_HALF(0xe88) / divisor', 'GLOBAL_HALF(0xe88) / (divisor + 1)'),
    ('resource haze source row', '0xa00 - ((quotient & 255) << 8)', '0x900 - ((quotient & 255) << 8)'),
    ('resource haze final row', 'row < 0xf00;', 'row < 0xe00;'),
    ('resource haze final colour', 'colour < 0x150;', 'colour < 0x14e;'),
    ('resource palette load failure', 'if (!load(ADDRESS(hg2d_image + 0x32e0), ADDRESS(name), selector, 9)) return 0;', 'load(ADDRESS(hg2d_image + 0x32e0), ADDRESS(name), selector, 9);'),
    ('resource haze extension', 'HALF(destination) = 0x6168;', 'HALF(destination) = 0x6167;'),
    ('resource haze selector', 'resource_haze_load(ADDRESS(hg2d_image + 0x3fd8), selector);', 'resource_haze_load(ADDRESS(hg2d_image + 0x3fd8), 0);'),
    ('resource palette success', 'palette_refresh();\n    return 1;', 'palette_refresh();\n    return 0;'),
    ('resource default dirty flag', 'HALF(0x36c0) = 1;\n    palette_force_body();', 'HALF(0x36c0) = 0;\n    palette_refresh();'),
    ('resource default haze name', 'resource_haze_load(ADDRESS(hg2d_image + 0x3fd0), 0)', 'resource_haze_load(ADDRESS(hg2d_image + 0x3fd8), 0)'),
]
mutations += [(*mutation, 'resources') for mutation in resource_mutations]
mip_dither_fog_mutations = [
    ('mip dither fog mip gate', 'mip |= WORD(0x4200) == 0x20772c04 || WORD(0x4200) == 0x2077e569;', 'mip |= 0;'),
    ('mip dither fog dither gate', 'dither |= WORD(0x4200) == 0x20772c04 || WORD(0x4200) == 0x2077e569;', 'dither |= 0;'),
    ('mip dither fog shade gate', 'fog |= WORD(0x4200) == 0x20772c04 || WORD(0x4200) == 0x2077e569;', 'fog |= 0;'),
    ('mip dither fog direction', 'forward |= WORD(0x4200) == 0x2077e569;', 'forward |= 0;'),
    ('mip dither fog lookup', 'BYTE(0xd70 + texel + ((shade >> 8) & 255) * 256)', 'BYTE(0xd71 + texel + ((shade >> 8) & 255) * 256)'),
    ('mip dither fog shade row', 'BYTE(0xd70 + texel + ((shade >> 8) & 255) * 256)', 'BYTE(0xd70 + texel + ((shade >> 9) & 255) * 256)'),
    ('mip dither fog opaque zero', '*(uint8_t *)POINTER(address) = BYTE(0xd70 + texel + ((shade >> 8) & 255) * 256);', 'if (BYTE(0xd70 + texel + ((shade >> 8) & 255) * 256)) *(uint8_t *)POINTER(address) = BYTE(0xd70 + texel + ((shade >> 8) & 255) * 256);'),
    ('mip dither fog shade step', 'shade = (shade + WORD(0x4358)) & 0xffff0fffu;', 'shade = (shade - WORD(0x4358)) & 0xffff0fffu;'),
    ('mip dither fog shade wrap', 'shade = (shade + WORD(0x4358)) & 0xffff0fffu;', 'shade += WORD(0x4358);'),
    ('mip dither fog block level', 'projective_mip_dither_pixels(pixel_address, block, 0x4198, y, direction, 0, 1, WORD(0x41c0))', 'projective_mip_dither_pixels(pixel_address, block, 0x4198, y, direction, 0, 1, WORD(0x41bc))'),
    ('mip dither fog tail skip', 'if (tail && index + 1 == count && WORD(0x45d5)) return;', 'if (tail && index + 1 == count && WORD(0x45d1)) return;'),
    ('mip dither fog tail direction', 'projective_mip_dither_fog_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_mip_dither_fog_tails[!forward][shift == 4][y & 1][WORD(0x45d9)]'),
    ('mip dither fog tail block', 'projective_mip_dither_fog_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_mip_dither_fog_tails[forward][shift != 4][y & 1][WORD(0x45d9)]'),
    ('mip dither fog tail row', 'projective_mip_dither_fog_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_mip_dither_fog_tails[forward][shift == 4][(y ^ 1) & 1][WORD(0x45d9)]'),
    ('mip dither fog tail count', 'projective_mip_dither_fog_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_mip_dither_fog_tails[forward][shift == 4][y & 1][WORD(0x45d9) - 1]'),
    ('mip dither fog tail family', 'projective_mip_dither_fog_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_dither_fog_tails[forward][shift == 4][y & 1][WORD(0x45d9)]'),
]
mutations += [(*mutation, 'projective_mip_dither_fog') for mutation in mip_dither_fog_mutations]
mip_dither_mutations = [
    ('mip dither mip gate', 'mip |= WORD(0x4200) == 0x2076da6d || WORD(0x4200) == 0x207793d8;', 'mip |= 0;'),
    ('mip dither dither gate', 'dither |= WORD(0x4200) == 0x2076da6d || WORD(0x4200) == 0x207793d8;', 'dither |= 0;'),
    ('mip dither direction', 'forward |= WORD(0x4200) == 0x207793d8;', 'forward |= 0;'),
    ('mip dither u mask', '(uint8_t)WORD(0x4375) << 32', '(uint8_t)255 << 32'),
    ('mip dither v mask', '(uint8_t)(WORD(0x4375) >> 8) << 32', '(uint8_t)255 << 32'),
    ('mip dither u bias removal', '*u = (sampled_u - bias)', '*u = sampled_u'),
    ('mip dither v bias removal', '*v = (sampled_v - bias)', '*v = sampled_v'),
    ('mip dither u persistent mask', '*u = (sampled_u - bias) & 0xffffffffffULL;', '*u = *u;'),
    ('mip dither v persistent mask', '*v = (sampled_v - bias) & 0xffffffffffULL;', '*v = *v;'),
    ('mip dither u fraction', '(uint32_t)(WORD(start) << 16)', '(uint32_t)(WORD(start) << 15)'),
    ('mip dither u step', 'WORD(0x41a8) = WORD(0x41a8) * 65536u;', 'WORD(0x41a8) = WORD(0x41a8) * 32768u;'),
    ('mip dither v step', 'WORD(0x41ac) = WORD(0x41ac) * 65536u;', 'WORD(0x41ac) = WORD(0x41ac) * 32768u;'),
    ('mip dither prefix', '((address & 1) != 0) == (direction > 0)', '((address & 1) != 0) != (direction > 0)'),
    ('mip dither pair direction', 'direction > 0 ? first : second;', 'direction > 0 ? second : first;'),
    ('mip dither pair spacing', 'address += direction * 2; remaining -= 2;', 'address += direction; remaining -= 2;'),
    ('mip dither live skip', 'if (index + 1 == count && WORD(0x45d5)) return;', 'if (index + 1 == count && WORD(0x45d1)) return;'),
    ('mip dither tail direction', 'projective_mip_dither_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_mip_dither_tails[!forward][shift == 4][y & 1][WORD(0x45d9)]'),
    ('mip dither tail block', 'projective_mip_dither_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_mip_dither_tails[forward][shift != 4][y & 1][WORD(0x45d9)]'),
    ('mip dither tail row', 'projective_mip_dither_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_mip_dither_tails[forward][shift == 4][(y ^ 1) & 1][WORD(0x45d9)]'),
    ('mip dither tail count', 'projective_mip_dither_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_mip_dither_tails[forward][shift == 4][y & 1][WORD(0x45d9) - 1]'),
    ('mip dither tail family', 'projective_mip_dither_tails[forward][shift == 4][y & 1][WORD(0x45d9)]', 'projective_dither_tails[forward][shift == 4][y & 1][WORD(0x45d9)]'),
]
mutations += [(*mutation, 'projective_mip_dither') for mutation in mip_dither_mutations]
mip_fog_mutations = [
    ('mip fog mip gate', 'mip |= WORD(0x4200) == 0x207431a7 || WORD(0x4200) == 0x207475c4;', 'mip |= 0;'),
    ('mip fog shade gate', 'fog |= WORD(0x4200) == 0x207431a7 || WORD(0x4200) == 0x207475c4;', 'fog |= 0;'),
    ('mip fog direction', 'forward |= WORD(0x4200) == 0x207475c4;', 'forward |= 0;'),
    ('mip fog row', 'BYTE(0xd70 + texel + ((shade >> 8) & 255) * 256)', 'BYTE(0xd70 + texel + ((shade >> 9) & 255) * 256)'),
    ('mip fog lookup', 'BYTE(0xd70 + texel + ((shade >> 8) & 255) * 256)', 'BYTE(0xd71 + texel + ((shade >> 8) & 255) * 256)'),
    ('mip fog doubled byte', 'pixel[0] = pixel[1] = painted;', 'pixel[0] = painted; pixel[1] = 0;'),
    ('mip fog opaque zero', 'pixel[0] = pixel[1] = painted;', 'if (painted) pixel[0] = pixel[1] = painted;'),
    ('mip fog shade step', 'shade = (shade + WORD(0x4358)) & 0xffff0fffu;', 'shade = (shade - WORD(0x4358)) & 0xffff0fffu;'),
    ('mip fog shade wrap', 'shade = (shade + WORD(0x4358)) & 0xffff0fffu;', 'shade += WORD(0x4358);'),
    ('mip fog block level', 'projective_mip_fog_pixels(pixel_address, block, 0x4198, direction, WORD(0x41c0))', 'projective_mip_fog_pixels(pixel_address, block, 0x4198, direction, WORD(0x41bc))'),
    ('mip fog tail level', 'projective_mip_fog_pixels(pixel_address, remaining, 0x4190, direction, WORD(0x41bc))', 'projective_mip_fog_pixels(pixel_address, remaining, 0x4190, direction, WORD(0x41c4))'),
    ('mip fog tail direction', 'projective_mip_fog_tails[forward][shift == 4][remaining]', 'projective_mip_fog_tails[!forward][shift == 4][remaining]'),
    ('mip fog tail block', 'projective_mip_fog_tails[forward][shift == 4][remaining]', 'projective_mip_fog_tails[forward][shift != 4][remaining]'),
    ('mip fog tail count', 'projective_mip_fog_tails[forward][shift == 4][remaining]', 'projective_mip_fog_tails[forward][shift == 4][remaining - 1]'),
    ('mip fog tail family', 'projective_mip_fog_tails[forward][shift == 4][remaining]', 'projective_mip_tails[forward][shift == 4][remaining]'),
]
mutations += [(*mutation, 'projective_mip_fog') for mutation in mip_fog_mutations]
mip_mutations = [
    ('mip dispatch gate', 'int mip = WORD(0x4200) == 0x20741559 || WORD(0x4200) == 0x2074597a;', 'int mip = 0;'),
    ('mip forward direction', 'forward |= WORD(0x4200) == 0x2074597a;', 'forward |= 0;'),
    ('mip initial complement', 'WORD(0x4365) = projective_mip_magnitude(right_factor);', 'WORD(0x4365) = 0;'),
    ('mip final complement', 'WORD(0x4361) = projective_mip_magnitude(left_factor);', 'WORD(0x4361) = 0;'),
    ('mip near equality', 'WORD(0x4361) <= WORD(0x4371) && WORD(0x4365) <= WORD(0x4371)', 'WORD(0x4361) < WORD(0x4371) && WORD(0x4365) < WORD(0x4371)'),
    ('mip near both endpoints', 'WORD(0x4361) <= WORD(0x4371) && WORD(0x4365) <= WORD(0x4371)', 'WORD(0x4361) <= WORD(0x4371) || WORD(0x4365) <= WORD(0x4371)'),
    ('mip block magnitude', 'if (mip) WORD(0x4361) = projective_mip_magnitude(factor);', 'if (mip) WORD(0x4361) = WORD(0x4365);'),
    ('mip distance product', '((uint64_t)WORD(0x4361) * WORD(0x4369)) >> 21', '((uint32_t)(WORD(0x4361) * WORD(0x4369))) >> 21'),
    ('mip distance shift', '* WORD(0x4369)) >> 21', '* WORD(0x4369)) >> 20'),
    ('mip distance clamp', 'if (distance >= 127) distance = 127;', 'if (distance >= 126) distance = 126;'),
    ('mip lookup', 'WORD(0x439d + distance * 4)', 'WORD(0x439d + (distance ? distance - 1 : 0) * 4)'),
    ('mip scratch level', 'WORD(0x435d) = level;', 'WORD(0x435d) = 0;'),
    ('mip packed base', 'WORD(0x948) + WORD(0x4379 + level * 4)', 'WORD(0x948)'),
    ('mip coordinate mask', 'WORD(0x4375) = WORD(0x45ad + level * 4);', 'WORD(0x4375) = 0xffff;'),
    ('mip u coordinate shift', 'WORD(start) >>= shift;', 'WORD(start) >>= (shift + 1);'),
    ('mip v coordinate shift', 'WORD(start + 4) >>= shift;', 'WORD(start + 4) >>= (shift + 1);'),
    ('mip u step shift', '(uint32_t)((int32_t)WORD(0x41a8) >> shift)', '(uint32_t)((int32_t)WORD(0x41a8) >> (shift + 1))'),
    ('mip v step shift', '(uint32_t)((int32_t)WORD(0x41ac) >> shift)', '(uint32_t)((int32_t)WORD(0x41ac) >> (shift + 1))'),
    ('mip u sample mask', '*u &= (uint8_t)WORD(0x4375);', '*u &= 255;'),
    ('mip v sample mask', '*v &= (uint8_t)(WORD(0x4375) >> 8);', '*v &= 255;'),
    ('mip u carry', '*u = (uint8_t)(*u + u_step + (uint32_t)(carry >> 32));', '*u = (uint8_t)(*u + u_step);'),
    ('mip alignment prefix', '((pixel_address & 2) != 0) == (direction > 0)', '((pixel_address & 2) != 0) != (direction > 0)'),
    ('mip pair spacing', 'pixel_address += direction * 4; remaining -= 2;', 'pixel_address += direction * 2; remaining -= 2;'),
    ('mip doubled byte', 'pixel[0] = pixel[1] = first; pixel[2] = pixel[3] = second;', 'pixel[0] = first; pixel[1] = 0; pixel[2] = pixel[3] = second;'),
    ('mip tail direction', 'projective_mip_tails[forward][shift == 4][remaining]', 'projective_mip_tails[!forward][shift == 4][remaining]'),
    ('mip tail block', 'projective_mip_tails[forward][shift == 4][remaining]', 'projective_mip_tails[forward][shift != 4][remaining]'),
    ('mip tail count', 'projective_mip_tails[forward][shift == 4][remaining]', 'projective_mip_tails[forward][shift == 4][remaining - 1]'),
]
mutations += [(*mutation, 'projective_mip') for mutation in mip_mutations]
plane_mutations = [
    ('plane first fraction', 'HALF(0x8ea) = (uint16_t)first;', 'WORD(0x8e8) = first << 16;'),
    ('plane last fraction', 'HALF(0x8f2) = (uint16_t)last;', 'WORD(0x8f0) = last << 16;'),
    ('plane row fraction', 'HALF(0x8ee) = (uint16_t)y;', 'WORD(0x8ec) = y << 16;'),
    ('plane first coordinate', 'WORD(0x4168) = matrix_coordinate(WORD(0x8e8),', 'WORD(0x4168) = matrix_coordinate(WORD(0x8f0),'),
    ('plane last coordinate', 'WORD(0x4170) = matrix_coordinate(WORD(0x8f0),', 'WORD(0x4170) = matrix_coordinate(WORD(0x8e8),'),
    ('plane row coordinate', 'WORD(0x416c) = matrix_coordinate(WORD(0x8ec),', 'WORD(0x416c) = matrix_coordinate(WORD(0x8ec) + 0x10000,'),
    ('plane first attributes', 'WORD(0x4314 + component * 4) = matrix_attribute(WORD(0x4168),', 'WORD(0x4314 + component * 4) = matrix_attribute(WORD(0x4170),'),
    ('plane last attributes', 'WORD(0x433c + component * 4) = matrix_attribute(WORD(0x4170),', 'WORD(0x433c + component * 4) = matrix_attribute(WORD(0x4168),'),
    ('plane component count', 'WORD(0x4314 + component * 4) = matrix_attribute(WORD(0x4168), WORD(0x416c), component);', 'WORD(0x4314 + component * 4) = matrix_attribute(WORD(0x4168), WORD(0x416c), component ^ 1);'),
    ('plane first endpoint', 'WORD(0x430c) = WORD(0x8e8);', 'WORD(0x430c) = WORD(0x8f0);'),
    ('plane last endpoint', 'WORD(0x4334) = WORD(0x8f0);', 'WORD(0x4334) = WORD(0x8e8);'),
    ('plane stack depth', 'entry_sp + 64 - stack_depth;', 'entry_sp + 68 - stack_depth;'),
    ('plane dispatch row', '(uint16_t)y, 0);', '(uint16_t)(y + 1), 0);'),
    ('plane span selector', 'if (WORD(0x402c) == 0x20784d69)', 'if (WORD(0x402c) == 0x20784d68)'),
    ('plane setup selector', 'WORD(0x402c) = 0x20784d69;', 'WORD(0x402c) = 0x20731134;'),
    ('plane flat scan stack', 'polygon_flat_span(edge, y, 1, 72);', 'polygon_flat_span(edge, y, 1, 68);'),
    ('plane flat walker stack', 'polygon_flat_span(edge, y, 0, 64);', 'polygon_flat_span(edge, y, 0, 60);'),
    ('plane pair stack', 'pairs == 2 && !index ? 68 : 64', 'pairs == 2 && !index ? 64 : 68'),
    ('plane walker stack', 'polygon_span(first, last, y, 60);', 'polygon_span(first, last, y, 64);'),
    ('plane full coordinate clamp', 'if (value > 0x7fff) value = 0x7fff;', 'if (value > 0x7ffe) value = 0x7ffe;'),
]
mutations += [(*mutation, 'projective_plane') for mutation in plane_mutations]
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
             'polygons': 'polygon', 'fills': 'fill', 'textured': 'textured', 'affine': 'affine', 'gouraud': 'gouraud', 'blits': 'blit', 'optimized': 'affine', 'perspective': 'perspective', 'discs': 'disc', 'perspective_fill': 'perspective_fill', 'matrix_vertices': 'matrix', 'matrix_fills': 'matrix', 'projective': 'projective', 'projective_keyed': 'projective', 'projective_fog': 'projective', 'projective_key_fog': 'projective', 'projective_dither': 'projective', 'projective_dither_fog': 'projective', 'projective_dither_key': 'projective', 'projective_dither_key_fog': 'projective', 'projective_plane': 'matrix', 'projective_mip': 'mip', 'projective_mip_fog': 'mip', 'projective_mip_dither': 'mip_dither', 'projective_mip_dither_fog': 'mip_dither', 'resources': 'resource', 'initialization': 'init', 'projective_mip_build': 'mip_build', 'fans': 'fan', 'device_palettes': 'device_palette', 'device_pixels': 'device_pixels', 'device_states': 'device_states', 'device_shutdown': 'device_shutdown', 'device_config': 'device_config', 'device_formats': 'device_formats', 'device_select_format': 'device_select_format', 'device_flush': 'device_flush', 'device_colour': 'device_colour'}.get(suite)
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
        str(root / 'src/hg2d/hg2d.def'), '-lgcc', '-lkernel32'], check=True)
    # Wine helpers can inherit pipe handles after the test process exits.
    # Files let wait() use the actual process status without waiting for EOF.
    with tempfile.TemporaryFile(mode='w+t') as stdout, tempfile.TemporaryFile(mode='w+t') as stderr:
        result = subprocess.run(['wine', 'hg2ddiff.exe', sys.argv[1] if len(sys.argv) > 1 else '64',
                                 suite, 'full' if suite == 'colours' else 'scripts'],
                                cwd=build, env={**os.environ, 'WINEDEBUG': '-all'},
                                stdout=stdout, stderr=stderr, timeout=120)
        stdout.seek(0); stderr.seek(0)
        result_stdout, result_stderr = stdout.read(), stderr.read()
    # Only a reported differential mismatch counts; a broken fixture or a
    # harness crash must be investigated, not counted as a caught mutation.
    caught = result.returncode == 1 and ' differs:' in result_stdout
    print(('%s: ' % name) + ('caught' if caught else 'MISSED / ERROR'), flush=True)
    if not caught:
        print(result_stdout + result_stderr, flush=True)
        missed.append(name)
if os.environ.get('HG2D_AUDIT_ONLY'):
    print('%d mutation anchors verified; no mutants executed' % tested)
else:
    print('%d mutations, %d caught, %d unresolved' % (tested, tested - len(missed), len(missed)))
sys.exit(bool(missed))
