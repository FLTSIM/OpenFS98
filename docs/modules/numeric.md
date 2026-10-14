# NUMERIC.DLL — module id 17

NUMERIC is FS98's value-to-text and text-to-value library. It exports three
services, imports the host, GLOBAL and UTIL, and uses MSVCRT for the two
floating-point presentation operations that must agree with retail. The
formatter and parser are reimplemented in `src/numeric/numeric.c`; the complete
module is built as `bin/NUMERIC.DLL`.

| slot | retail address | service |
| --- | --- | --- |
| `+0x04` | `0x20874870` | init — reads hemisphere letters and two `[MAIN]` settings |
| `+0x08` | `0x20874930` | deinit — a bare `ret` |
| `+0x18` | `0x20873adc` | `format(value, id, output)` |
| `+0x1c` | `0x208711c4` | `parse(text, id, output)` — calls `+0x20` with no range |
| `+0x20` | `0x208711e1` | `parse_internal(text, id, output, range)` |
| `+0x30`…`+0x40` | data | pointers to `"%d* %2.Nf'"`, N = 0…4 |

The retail implementation is hand-written assembly. Values generally travel
through `EDX:EAX`, strings through `EBX` and `EDI`, and GLOBAL through `ESI`.
The C deliberately retains the resulting fixed-width operations and odd edge
behavior instead of replacing them with more natural arithmetic.

## Formatter, `+0x18`

Clearing bit `0x10000` from the format argument selects one of 81 handlers.
That bit asks for leading zeroes to be removed. An invalid id returns null; a
null output pointer selects NUMERIC's internal buffer. Ids 5, 8, 9, 0xd, 0x10,
0x11, 0x18 and 0x35 intentionally produce an empty string.

The common digit helper divides by a table of powers of ten and adds `'0'` to
the low byte of every quotient. It does not clamp or carry an oversized digit,
so out-of-range input can produce punctuation. Fixed-point handlers use 4, 8,
12 or 16 fraction bits and truncate to one through four printed decimal
digits.

Notable retail behavior retained by the implementation:

- The zero-trimming code attempts to exempt ids 4, 0xe, 0x14, 0x15 and 0x36,
  but compares those ids with the still-flagged argument. None is actually
  exempt.
- Metres-to-feet conversion is an integer multiply by `0x347e5` followed by a
  shift. The two 48-bit variants use the retail x87 instruction sequence.
- Angle conversion multiplies the 16-bit circle by 360; zero is displayed as
  360 degrees.
- Id 0x12 advances the output cursor with a 16-bit `inc di`, not a 32-bit
  increment.
- Longitude and latitude use fixed-point restoring division for DMS output.
  Fractional-minute output calls UTIL's signed or unsigned fixed-to-double
  service, then MSVCRT `floor` and `sprintf`. The x87 rounding sequence is
  retained.

## Parser, `+0x20`

The public service at `+0x1c` is only a wrapper which passes a null range.
`+0x20` additionally checks an optional pair of minimum/maximum values before
writing output. Its common decimal reader builds a signed 48.16 intermediate,
then each format handler performs the inverse unit conversion and stores the
type belonging to that id.

The reader accepts leading signs and spaces or tabs around a completed number.
It keeps four decimal digits. Further zeroes are harmless; a further non-zero
digit returns 13 for lost precision while still storing the truncated result.
Among the less obvious compatibility cases:

- A repeated minus is accepted by the validator because retail checks a flag
  it never sets. The unsigned wrappers still reject the resulting negative
  value.
- Id 0x19 accepts only octal characters, but converts the resulting decimal
  digit string to packed BCD after validation.
- The scaled 16-bit formats round from the low fixed-point word and reproduce
  the word-sized overflow behavior.
- The ordinary integer and fixed-point ranges are inclusive. The upper end of
  a signed 48-bit range is exclusive; equality returns 6 and does not write.
- Longitude and latitude accept hemisphere letters followed by degrees,
  minutes and seconds, or fractional minutes. Their shift/add constants and
  intermediate rounding are the retail sequence rather than a floating-point
  degrees conversion. The default hemisphere comes from GLOBAL when no letter
  is present.

The return codes are part of the calling contract: 1 is an unsupported format,
2 a null output, 4 an invalid character, 5/6 below/above range, 8 invalid sign
or decimal placement, 9 no digits, 10 a fraction for an integer-only format,
11/12 format-specific high/low limits, 13 precision loss, 14 a negative value
where unsigned is required, 15 the octal field limit, and 16 an invalid angle
hemisphere.

## Initialization and extracted data

Init loads string `0x9100` and takes bytes 0, 2, 4 and 6 as the positive and
negative latitude/longitude letters. It reads these `[MAIN]` keys through host
slot `+0xa8`:

- `DISPLAY_FRACTIONAL_MINUTES` selects fractional-minute or DMS output.
- `FRACTIONAL_MINUTE_DIGITS` is clamped to 1…4, then used to build its decimal
  scale.

`tools/gen_numeric_tables.py` extracts the initialized retail `.data` image
and the bit-exact `.rdata` constants into `src/numeric/numeric_data.inc`. This
keeps all digit, angle and formatting constants traceable to the original DLL.

## Verification

`make numericdiff-run` loads retail NUMERIC beside the reimplementation, binds
both to the same fake host, GLOBAL block and UTIL conversion services, and
compares return values, output strings and output bytes. It covers every
format id, both zero-trimming states, all three unit modes, both angle display
modes, all four fractional-minute precisions, edge and random values, parser
error strings, formatter/parser round trips, and ranged `+0x20` calls.

Current result:

```text
numeric formatter: 505440 comparisons, 0 mismatches
numeric parser: 40338 comparisons, 0 mismatches
```

The harness was also built with `FS98_NUMERIC_TEST_BREAK`, which corrupts one
formatter result. That build exits non-zero immediately with mismatches, so the
all-green result is not a comparison path that silently accepts differences.

The rebuilt DLL is installed in the test game alongside the OpenFS98 GLOBAL,
SLEW and UTIL modules. The full `make smoke` boot reaches the panel with all
27 modules and 95 gauges loaded.
