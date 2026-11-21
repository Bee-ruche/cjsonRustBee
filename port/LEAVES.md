# What each leaf answers

A leaf is right when it answers exactly as cJSON's own function does (`reference/cJSON.c`, read it: it is the
specification), through the reference's driver (`harness/c/reference_driver.c`), which says here what the input
and the answer are. « Refused » means `Err(())`.

| Leaf | The original | Input | Answer |
| --- | --- | --- | --- |
| `hex4` | `parse_hex4` | exactly four bytes (any other length: refused) | two bytes: the number the original returns, high byte first (the original returns 0 for digits that are not hexadecimal) |
| `utf16` | `utf16_literal_to_utf8` | any bytes: the text of a `\uXXXX` escape, or of two for a surrogate pair, and what follows | one byte, the count of input bytes the original says it read (6 or 12), then the UTF-8 bytes it wrote; refused when the original fails |
| `minify` | `cJSON_Minify` | a text without a zero byte (with one: refused) | the minified text |
| `print_string` | `print_string_ptr` | a text without a zero byte (with one: refused) | the text as a JSON string, quotes included |
| `strcasecmp` | `case_insensitive_strcmp` | two texts, each ended by one zero byte: `first 0 second 0` (anything else: refused) | four bytes: the number the original returns, low byte first, as a 32-bit two's complement |
| `parse_number` | `parse_number` | any bytes: the text the original is asked to read a number from, at its start, and what follows | fourteen bytes: the count of input bytes the original read (two bytes, low byte first), then the `valuedouble` it set (a double, below), then the `valueint` it set (four bytes, low byte first, as a 32-bit two's complement); refused when the original fails |
| `compare_double` | `compare_double` | exactly sixteen bytes: two doubles (below), `a` then `b` (any other length: refused) | one byte: 1 when the original says they are equal, else 0 |
| `print_number` | `print_number` | exactly twelve bytes: an item's `valuedouble` (a double, below), then its `valueint` (four bytes, low byte first, as a 32-bit two's complement) (any other length: refused) | the text the original writes, without its zero byte |
| `buffer_skip_whitespace` | `buffer_skip_whitespace` | a buffer (below): its offset, then its content (fewer than two bytes: refused) | two bytes: the offset the original leaves, low byte first |
| `skip_utf8_bom` | `skip_utf8_bom` | a buffer (below): its offset, then its content (fewer than two bytes: refused) | two bytes: the offset the original leaves, low byte first; refused when the original fails (it returns `NULL`) |
| `update_offset` | `update_offset` | a print buffer (below): its offset, then its content; refused unless a zero byte lies at or after the offset (fewer than two bytes: refused) | two bytes: the offset the original leaves, low byte first |

## The leaves of buffers

The original reads its input through a buffer: a `parse_buffer`, a `printbuffer` for `update_offset`. Of it, these
three read the content, its `length` and the `offset`, and move the offset; they read nothing else of it. A buffer is
given as its offset, two bytes, low byte first, then its content: its length is the count of bytes that follow.

Any offset may be given, at the end of the content or beyond it too, and the answer is then what the original does,
but in one case: `update_offset` is not called where no zero byte lies at or after the offset (the original would
read past the buffer's end), and the leaf refuses.

## The leaves of numbers

A double, given or answered, is its eight bytes: the IEEE 754 binary64 pattern, low byte first. Any pattern may be
given: zeros of both signs, subnormals, infinities, NaNs.

The original takes its decimal point from the C library's locale only when it is built with `ENABLE_LOCALES`; the
reference is not, so the decimal point is `.`. Where the original calls the C library (`strtod`, `sprintf`,
`sscanf`), the library's answer is part of the specification: the reference's is the GNU C library of the judge's
image (Ubuntu 24.04), which converts exactly: a decimal text to the nearest double (the even one in a tie), and a
double to as many digits as asked (its exact value rounded to the nearest, the even digit in a tie).

The judge builds with rustc 1.75, whose `core` gives `f64` less than `std` does: no `abs`, `floor` or `powi`
there (they came to `core` later); its bits (`to_bits`, `from_bits`) are there.

## What a leaf's file may hold

A leaf is plain Rust over `core`: no `std`, no allocation, no `unsafe` (the build refuses them). Its file also
holds none of these words, in code, comments or strings: `static` (the lifetime `'static` is fine), `unsafe`,
`extern`, `macro_rules`, `include`, `include_str`, `include_bytes`, `env`, `option_env`, `concat_idents`, `asm`,
`global_asm`, `mod`; nowhere the character `#` (so no attribute); and no type written `for<'a> …`. The build
looks for these in the file's text, comments included: it parses nothing, so a comment that says « no unsafe »
is refused too. This is a rule of the project, read again by its maintainers on each leaf: the text of a file
cannot tell everything a leaf does.

A leaf is a function of its input alone: it keeps nothing from one call to the next, reads nothing but its
input, and its answer does not depend on where it lies in memory. The build cannot check all of that; the judge
does what it can about it: it builds each leaf several times, laid out differently each time, runs each build
several times on the cases in different orders, and wants every answer right every time. Maintainers read a
leaf before it is published: one that is right by a trick is removed.

## The buffer

The output buffer a leaf is given is zeroed and holds `6 × (input length) + 16` bytes: more than any right answer.
A leaf is a function of its input alone: the same input, the same answer, whatever was asked before.

The build checks integer arithmetic (`-C overflow-checks=on`): an overflow, like an index out of bounds, ends the
run at once, and the leaf is refused.
