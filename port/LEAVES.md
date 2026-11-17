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
