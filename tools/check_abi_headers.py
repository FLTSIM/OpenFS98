#!/usr/bin/env python3
"""Story 1.7: the module ABI headers are 32-bit (AD-14, AD-4).

  python3 -B tools/check_abi_headers.py              the directive check, the type check and the probe compiles
  python3 -B tools/check_abi_headers.py --self-test  the same checks against failing fixtures

1. Directives: FS98_STDCALL is defined once, in include/fs98/abi.h, with no second definition in src, tools or include.
2. Types: abi.h, linkage.h, host.h and global.h are preprocessed by each probe compiler and every declaration they
   contain is checked, typedefs resolved: no pointer (array and function parameters included), long, size_t, wchar_t,
   bitfield or long double.  The retail bridge (the `_call` types and the fs98_import pointer view) is exempt by name,
   and on gcc -m32 and gcc it must not exist at all (also as a macro: FS98_RETAIL_PTR, FS98_LINK).
3. Probes: the four headers, together and each alone, with their _Static_asserts and a calling-convention check of
   fs98_host_alloc_fn, compile with -std=c99 -Wall -Wextra -Werror on i686-w64-mingw32-gcc (with the module flags),
   gcc -m32 and gcc (x86-64).
"""
import os
import re
import subprocess
import sys
import tempfile
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
HEADERS = ['abi.h', 'linkage.h', 'host.h', 'global.h']
FORBIDDEN_WORDS = {'long', 'size_t', 'wchar_t', 'ptrdiff_t', 'intptr_t', 'uintptr_t', 'ssize_t', 'intmax_t', 'uintmax_t'}
FIXED_INT = re.compile(r'^u?int(8|16|32|64)_t$')
DEFINE = re.compile(r'^\s*#\s*define\s+FS98_STDCALL\b', re.M)


def defines(text):
    """FS98_STDCALL definitions in a file, after line splices and comments are gone."""
    text = re.sub(r'\\\n', '', text)
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    return DEFINE.findall(text)


def check_directives(root):
    """Every FS98_STDCALL definition outside abi.h, and the count in abi.h."""
    errs = []
    abi = root / 'include/fs98/abi.h'
    n = len(defines(abi.read_text())) if abi.exists() else 0
    if n != 1:
        errs.append(f'{abi}: {n} definitions of FS98_STDCALL, want exactly 1')
    for d in ('src', 'tools', 'include'):
        for p in sorted((root / d).rglob('*')):
            if p == abi or not p.is_file() or p.suffix not in ('.c', '.h', '.S', '.inc', '.py', '.sh', '.cmake', '.txt'):
                continue
            if p.name == Path(__file__).name:
                continue
            if defines(p.read_text(errors='replace')):
                errs.append(f'{p.relative_to(root)}: defines FS98_STDCALL outside abi.h')
    return errs


def preprocess(incdir, cc, extra=()):
    src = ''.join(f'#include <fs98/{h}>\n' for h in HEADERS)
    r = subprocess.run(cc + ['-E', *extra, '-std=c99', '-I', str(incdir), '-x', 'c', '-'], input=src,
                       capture_output=True, text=True)
    if r.returncode:
        raise SystemExit('preprocess failed:\n' + r.stderr)
    return r.stdout


def tokens(text):
    return re.findall(r'[A-Za-z_]\w*|\d\w*|\S', text)


def declarations(pp):
    """(file, token list) per top-level declaration of the preprocessed text, from the line markers."""
    decls, cur, depth, fil = [], [], 0, ''
    for line in pp.split('\n'):
        m = re.match(r'# \d+ "([^"]*)"', line)
        if m:
            fil = m.group(1)
            continue
        if line.startswith('#'):
            continue                                    # #pragma pack
        for t in tokens(line):
            cur.append(t)
            if t == '{':
                depth += 1
            elif t == '}':
                depth -= 1
            elif t == ';' and depth == 0:
                decls.append((fil, cur))
                cur = []
    return decls


def strip_attributes(d):
    t = list(d)
    while '__attribute__' in t:                         # drop `__attribute__((x))`
        i = t.index('__attribute__')
        j, depth = i + 1, 0
        while True:
            depth += (t[j] == '(') - (t[j] == ')')
            j += 1
            if depth == 0:
                break
        del t[i:j]
    return t


def parameters(d):
    """The parameters of a function declarator in a declaration, each a token list ([] if there is none)."""
    t = strip_attributes(d)
    if '(' not in t:
        return []
    i = t.index('(')
    if t[i + 1] == '*':                                 # T (*name)(args): the args are the next group
        i = t.index(')', i) + 1
        if i >= len(t) or t[i] != '(':
            return []
    depth, cur, out = 0, [], []
    for tok in t[i:]:
        if tok == '(':
            depth += 1
            if depth == 1:
                continue
        if tok == ')':
            depth -= 1
            if depth == 0:
                break
        if tok == ',' and depth == 1:
            out.append(cur)
            cur = []
        else:
            cur.append(tok)
    out.append(cur)
    return out


def typedef_name(d):
    """The name a typedef declares: `T name;`, `T conv name(args);` or `T (*name)(args);`."""
    t = strip_attributes(d[:-1])
    if '(' in t:
        i = t.index('(')
        return t[i + 2] if t[i + 1] == '*' else t[i - 1]
    return t[-1]


def check_types(incdir):
    errs = []
    for name, cc in probe_compilers():
        errs += [f'{name}: {e}' for e in check_types_one(incdir, cc, name != 'i686-w64-mingw32-gcc')]
    return errs


def check_types_one(incdir, cc, native):
    pp = preprocess(incdir, cc)
    decls = declarations(pp)
    typedefs = {}
    for _, d in decls:
        if d[0] == 'typedef':
            typedefs[typedef_name(d)] = d

    def bad(toks, seen=()):
        """The first forbidden thing in a token list, following typedef names."""
        for i, t in enumerate(toks):
            if t == '*':
                return 'a pointer'
            if t in FORBIDDEN_WORDS:
                return t
            if t == ':' :
                return 'a bitfield'
            if t == 'double' and i and toks[i - 1] == 'long':
                return 'long double'
            if FIXED_INT.match(t):
                continue
            if t in typedefs and t not in seen:
                inner = bad(typedefs[t][1:-1], seen + (t,))
                if inner:
                    return f'{t} -> {inner}'
        return None

    errs = []
    for fil, d in decls:
        if not any(fil.endswith('fs98/' + h) for h in HEADERS):
            continue
        if d[0] in ('_Static_assert', '__extension__', 'extern', 'enum'):
            continue
        if d[0] == 'struct' and d[-2] != '}':
            continue                                    # a forward declaration
        if d[0] == 'typedef' and re.fullmatch(r'fs98_host_\w+_call', typedef_name(d)):
            continue                                    # the retail bridge, exempt by name
        text = ' '.join(d)
        for prm in (parameters(d) if d[0] != 'struct' else []):
            if '[' in prm:
                errs.append(f'{Path(fil).name}: an array parameter (a pointer) in `{text[:160]}`')
            elif '(' in prm:
                errs.append(f'{Path(fil).name}: a function parameter (a pointer) in `{text[:160]}`')
        body = d[1:-1] if d[0] == 'typedef' else d[:-1]
        if d[0] == 'struct':
            # members only: skip the `struct tag {` and the trailing `}`; split the members at `;`
            inner = d[d.index('{') + 1:len(d) - 2]
            members, cur = [], []
            for t in inner:
                if t == ';':
                    members.append(cur)
                    cur = []
                else:
                    cur.append(t)
            for m in members:
                if 'link_ptr' in m:
                    continue                            # the retail pointer view of fs98_import.link, exempt by name
                b = bad(m)
                if b:
                    errs.append(f'{Path(fil).name}: {b} in member `{" ".join(m)}`')
            continue
        b = bad(body)
        if b:
            errs.append(f'{Path(fil).name}: {b} in `{text[:160]}`')
    if native:
        if re.search(r'\bfs98_host_\w+_call\b|\blink_ptr\b', pp):
            errs.append('the retail-only bridge (_call types, link_ptr) is visible outside _WIN32 && __i386__')
        macros = subprocess.run(cc + ['-dM', '-E', '-std=c99', '-I', str(incdir), '-x', 'c', '-'],
                                input=''.join(f'#include <fs98/{h}>\n' for h in HEADERS),
                                capture_output=True, text=True).stdout
        if re.search(r'^#define (FS98_RETAIL_PTR|FS98_LINK|fs98_host_\w+_call)\b', macros, re.M):
            errs.append('FS98_RETAIL_PTR, FS98_LINK or a _call macro is defined outside _WIN32 && __i386__')
    return errs


def probe_compilers():
    from module_flags import FLAGS
    return [('i686-w64-mingw32-gcc', ['i686-w64-mingw32-gcc'] + FLAGS), ('gcc -m32', ['gcc', '-m32']), ('gcc', ['gcc'])]


# The convention a service type must have on each target: stdcall on win32-retail, the platform default elsewhere.
CONVENTION = {'i686-w64-mingw32-gcc': '__attribute__((stdcall))', 'gcc -m32': '', 'gcc': ''}


def run_probes(incdir):
    errs = []
    together = ''.join(f'#include <fs98/{h}>\n' for h in HEADERS)
    convention = ('#include <fs98/host.h>\ntypedef int32_t %s fs98_probe_ref_fn(int32_t, uint32_t, fs98_addr, int32_t);\n'
                  '_Static_assert(__builtin_types_compatible_p(fs98_host_alloc_fn, fs98_probe_ref_fn), '
                  '"calling convention of fs98_host_alloc_fn");\n')
    for name, cc in probe_compilers():
        sources = {'all four': together + 'int fs98_abi_probe_unit;\n',
                   'calling convention': convention % CONVENTION[name]}
        sources.update({h: f'#include <fs98/{h}>\nint fs98_abi_probe_unit;\n' for h in HEADERS})
        for what, src in sources.items():
            r = subprocess.run(cc + ['-std=c99', '-Wall', '-Wextra', '-Werror', '-I', str(incdir), '-c', '-o', os.devnull, '-x', 'c', '-'],
                               input=src, capture_output=True, text=True)
            if r.returncode:
                errs.append(f'{name}: probe of {what} failed\n{r.stderr[:20000]}')
    return errs


def check_all(root):
    return check_directives(root) + check_types(root / 'include') + run_probes(root / 'include')


def self_test():
    tmp = Path(tempfile.mkdtemp(prefix='abicheck'))
    fails = []

    def expect(label, errs, want_fail):
        if bool(errs) != want_fail:
            fails.append(f'{label}: {"unexpectedly passed" if want_fail else "unexpectedly failed: " + str(errs)[:300]}')

    try:
        # a clean copy of the real headers passes
        base = tmp / 'clean'
        shutil.copytree(ROOT / 'include', base / 'include')
        expect('clean headers', check_types(base / 'include') + run_probes(base / 'include'), False)

        # second FS98_STDCALL definition, in several spellings
        for i, spelling in enumerate(('#define FS98_STDCALL x', '#  define   FS98_STDCALL x', '# define\tFS98_STDCALL',
                                      '#define/**/FS98_STDCALL x', '#define \\\nFS98_STDCALL x')):
            r = tmp / f'dir{i}'
            (r / 'include/fs98').mkdir(parents=True)
            (r / 'src').mkdir()
            (r / 'tools').mkdir()
            shutil.copy(ROOT / 'include/fs98/abi.h', r / 'include/fs98/abi.h')
            expect('abi.h alone', check_directives(r), False)
            (r / 'src/x.c').write_text(spelling + '\n')
            expect(f'second definition {spelling!r}', check_directives(r), True)
            (r / 'src/x.c').write_text('/* ' + spelling.replace('/**/', ' ') + ' */\n// ' + spelling.split('\\')[0] + '\n')
            expect(f'commented-out definition {spelling!r}', check_directives(r), False)

        # forbidden contract types, typedef-hidden included
        fixtures = {
            'pointer': 'struct fs98_host_file { void *handle; };',
            'long': 'struct fs98_host_file { long handle; };',
            'size_t': 'struct fs98_host_file { size_t handle; };',
            'wchar_t': 'struct fs98_host_file { wchar_t handle; };',
            'bitfield': 'struct fs98_host_file { unsigned handle : 3; };',
            'long double': 'struct fs98_host_file { long double handle; };',
            'typedef-hidden pointer': 'typedef char *hidden_t;\nstruct fs98_host_file { hidden_t handle; };',
            'pointer parameter': 'typedef int FS98_STDCALL fs98_host_x_fn(char *s);',
            'pointer return': 'typedef char *FS98_STDCALL fs98_host_x_fn(void);',
            'array parameter': 'typedef int FS98_STDCALL fs98_host_x_fn(char path[]);',
            'function parameter': 'typedef int FS98_STDCALL fs98_host_x_fn(int cb(int));',
            'retail-only pointer': '#if defined(_WIN32) && defined(__i386__)\nstruct fs98_host_file2 { char *path; };\n#endif',
        }
        for label, text in fixtures.items():
            d = tmp / ('t_' + re.sub(r'\W', '_', label))
            shutil.copytree(ROOT / 'include', d / 'include')
            h = d / 'include/fs98/host.h'
            h.write_text(h.read_text() + '\n' + text + '\n')
            expect(f'forbidden type: {label}', check_types(d / 'include'), True)

        # layout drift and a GLOBAL offset past the end must stop the build on every probe compiler
        def mutate(label, header, old, new, message, failing=3):
            d = tmp / ('m_' + re.sub(r'\W', '_', label))
            shutil.copytree(ROOT / 'include', d / 'include')
            h = d / 'include/fs98' / header
            t = h.read_text()
            if old not in t:
                fails.append(f'{label}: fixture text not found')
                return
            h.write_text(t.replace(old, new, 1))
            errs = run_probes(d / 'include')
            compilers = {e.split(':')[0] for e in errs}
            if len(compilers) != failing or not all(message in e for e in errs):
                fails.append(f'{label}: failed on {sorted(compilers)}, want {failing} compilers, each with {message!r}')

        mutate('moved field', 'host.h', '    int32_t  grow;          /* capacity step when full */\n',
               '    int32_t  extra_a;\n    int32_t  grow;          /* capacity step when full */\n', 'fs98_host_list')
        mutate('resized struct', 'linkage.h', '    fs98_code init;', '    uint64_t  init;', 'fs98_linkage header')
        mutate('outside GLOBAL', 'global.h', 'FS98_G_ALT_OFFSET     = 0x120c,', 'FS98_G_ALT_OFFSET     = 0x12a8,', 'outside GLOBAL')
        # stdcall guard without _WIN32: native linux-i386 would take stdcall; only gcc -m32 sees it
        mutate('stdcall without _WIN32', 'abi.h', '#if defined(_WIN32) && defined(__i386__)\n#  define FS98_ABI_STDCALL_',
               '#if defined(__i386__)\n#  define FS98_ABI_STDCALL_', 'calling convention', failing=1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for f in fails:
        print('SELF-TEST FAIL:', f)
    if not fails:
        print('check_abi_headers self-test: ok')
    return 1 if fails else 0


def main():
    if '--self-test' in sys.argv:
        return self_test()
    errs = check_all(ROOT)
    for e in errs:
        print('FAIL:', e)
    if not errs:
        print('abi headers: one FS98_STDCALL, contract types clean, probes pass on mingw i686, gcc -m32 and gcc')
    return 1 if errs else 0


if __name__ == '__main__':
    sys.exit(main())
