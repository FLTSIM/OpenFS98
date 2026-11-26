#!/usr/bin/env python3
"""Build the eight normal MDL DLLs from the committed images (src/mdl/images/<NAME>/, src/mdl/mdl_image.c).

The metadata is src/mdl/mdl_manifest.json (tools/gen_mdl_images.py writes it); retail is read only for the
resources (model.res).  Generated artifacts stay under ignored bin/mdl-diff/.
"""
import module_flags
import argparse
import json
from pathlib import Path
import struct
import subprocess

from dialogmap import Fs98Pe

NAMES = ('BOEING737-400_N', 'BELL_206B_JETRANGER_N', 'CESSNA182RRG_N',
         'CESSNA182S_N', 'EXTRA300S_N', 'LEAR45_N', 'SCHWEIZER2_32_N', 'SOPWITHCAMEL_N')
MANIFEST = Path('src/mdl/mdl_manifest.json')
IMAGES = Path('src/mdl/images')


def manifest(family):
    """The committed records of 'normal' or 'crash' models."""
    return json.loads(MANIFEST.read_text())[family]


def project(entry, output):
    """One model's modules.json record: the manifest's, with the rebuilt DLL's absolute path and without import_rva."""
    record = {}
    for key, value in entry.items():
        if key == 'import_rva':
            continue
        record[key] = value
        if key == 'installed':
            record['rebuilt'] = str((output / 'models' / Path(entry['installed']).name).resolve())
    return record


def generate(entry, directory):
    """model.res (retail's resources), model.def and model_config.h; the model's data is committed C, not read here."""
    path = Path('orig') / entry['installed']
    pe = Fs98Pe(path)
    if hex(pe.uiBase) != entry['image_base']:
        raise ValueError(f'{path}: image base moved (docs/building.md, "Which CD")')
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'model_config.h').write_text(f'/* Generated from {path.name}. */\n#define MDL_KIND {entry["kind"]}\n')
    (directory / 'model.def').write_text('EXPORTS\n    ImportTable DATA\n    Linkage DATA\n')
    resources = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0))
    resource_meta = []
    for resource in pe.resources():
        if not all(isinstance(resource[key], int) and 0 <= resource[key] <= 65535
                   for key in ('type', 'id', 'language')):
            raise ValueError(f'{path}: unsupported named resource')
        resources += struct.pack('<IIHHHHIHHII', resource['size'], 32, 0xffff,
                                 resource['type'], 0xffff, resource['id'], 0,
                                 0x1030, resource['language'], 0, 0)
        resources += pe.read_bytes(pe.file_offset(resource['rva'], resource['size']), resource['size'])
        resources += bytes(-len(resources) % 4)
        resource_meta.append({key: resource[key] for key in ('type', 'id', 'language', 'size')})
    if resource_meta != entry['resources']:
        raise ValueError(f'{path}: resources differ from {MANIFEST} (docs/building.md, "Which CD")')
    (directory / 'model.res').write_bytes(resources)


def image_object(record, directory, images=None, image_source='src/mdl/mdl_image.c'):
    """Compile the model's image (mdl_image.c over its layout.h and data.inc) to directory/mdl_image.o; returns the object."""
    images = images or IMAGES / Path(record['path']).stem
    target = directory / 'mdl_image.o'
    result = subprocess.run(['i686-w64-mingw32-gcc', *module_flags.FLAGS, '-std=c99', '-Os', '-Wall', '-Wextra', '-fno-ident',
                             '-ffreestanding', '-fno-builtin', '-fno-asynchronous-unwind-tables',
                             '-Iinclude', '-I' + str(images), '-c', '-o', str(target), str(image_source)], text=True, capture_output=True)
    if result.returncode or result.stderr:
        raise RuntimeError(f'{record["path"]}: image compiler output\n{result.stdout}{result.stderr}')
    return target


def build(record, directory, module_source='src/mdl/mdl_module.c', update_source='src/mdl/mdl_normal.c',
          images=None, image_source='src/mdl/mdl_image.c'):
    output = Path(record['rebuilt'])
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['i686-w64-mingw32-windres', '-J', 'res', '-O', 'coff',
                    str(directory / 'model.res'), str(directory / 'resource.o')], check=True)
    image = image_object(record, directory, images, image_source)
    command = ['i686-w64-mingw32-gcc', *module_flags.FLAGS, '-std=c99', '-Itools/difftest', '-Os', '-Wall', '-Wextra',
               '-ffreestanding', '-fno-builtin', '-fno-asynchronous-unwind-tables',
               '-Iinclude', '-Isrc/mdl', '-I' + str(directory), '-shared', '-nostdlib',
               '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--disable-nxcompat',
               '-Wl,--image-base=' + record['image_base'], '-o', str(output),
               str(update_source), str(module_source), str(image),
               str(directory / 'model.def'), str(directory / 'resource.o'), '-lkernel32']
    result = subprocess.run(command, text=True, capture_output=True)
    (directory / 'build.log').write_text(result.stdout + result.stderr)
    if result.returncode or result.stderr:
        raise RuntimeError(f'{output.name}: compiler output\n{result.stdout}{result.stderr}')
    print(f'{output.name}: built, {len(record["model_relocs"])} data relocations', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('bin/mdl-diff'))
    args = parser.parse_args()
    entries = manifest('normal')
    if tuple(Path(entry['installed']).stem for entry in entries) != NAMES:
        raise ValueError(f'{MANIFEST}: expected the eight reference normal MDLs in known kind order')
    records = []
    for entry in entries:
        directory = args.output / Path(entry['installed']).stem
        generate(entry, directory)
        record = project(entry, args.output)
        build(record, directory)
        records.append(record)
    (args.output / 'modules.json').write_text(json.dumps(records, indent=2) + '\n')
    lines = ['/* Generated MDL boundaries and relocation/resource metadata. */']
    for index, record in enumerate(records):
        lines += [f'static const struct model_reloc relocs_{index}[] = {{']
        lines += [f'    {{0x{address:x}, 0x{target:x}}},'
                  for address, target in record['model_relocs']]
        lines += ['};', f'static const struct model_resource resources_{index}[] = {{']
        lines += ['    {%d, %d, %d, %d},' % tuple(resource[key] for key in ('type', 'id', 'language', 'size'))
                  for resource in record['resources']]
        lines += ['};']
    lines += ['static const struct model_spec models[] = {']
    for index, (record, entry) in enumerate(zip(records, entries)):
        data = record['sections']['.data']
        values = [index, data['rva'], data['size'], entry['import_rva'], record['linkage_rva'],
                  int(record['header_rva'], 16), record['active_end']]
        lines += ['    {' + json.dumps(str(Path(record['path']).resolve())) + ', '
                  + json.dumps(record['rebuilt']) + ', ' + ', '.join(hex(value) for value in values)
                  + f', relocs_{index}, {len(record["model_relocs"])}, resources_{index}, {len(record["resources"])}' + '},']
    lines += ['};']
    (args.output / 'modules.inc').write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
