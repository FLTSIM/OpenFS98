"""The semantic compile flags every gcc invocation takes: parsed from cmake/toolchains/win32-retail.cmake, the one place that spells them (AD-11, AD-20)."""
import re
import sys
from pathlib import Path

TOOLCHAIN = Path(__file__).resolve().parents[1] / 'cmake/toolchains/win32-retail.cmake'
_line = re.search(r'^set\(OFS_MODULE_FLAGS ([^)]*)\)', TOOLCHAIN.read_text(), re.M)
if not _line:
    sys.exit(f'{TOOLCHAIN}: no set(OFS_MODULE_FLAGS ...) line')
FLAGS = _line.group(1).split()
