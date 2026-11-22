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
| `parse_string` | `parse_string` | two bytes, the room (below), then a buffer (below): its offset, then its content; refused unless a byte lies at the offset (the original reads it without a check) (fewer than four bytes: refused) | one byte, 1 when the original succeeds and 0 when it fails; then the offset it leaves (two bytes, low byte first; it moves the offset when it fails too); then, when it succeeds, the block it asked for, as it leaves it (below) |
| `strdup` | `cJSON_strdup` | two bytes, the room (below), then a text without a zero byte (with one: refused) (fewer than two bytes: refused) | the block it asked for, as it leaves it (below): the copy, its zero included; refused when the original fails (it returns `NULL`) |
| `print_value` | `print_value`, as `cJSON_PrintPreallocated` calls it, with all it calls (`print_array`, `print_object`, `print_number`, `print_string_ptr`) | two bytes, low byte first, the length of the buffer the original prints into (the tree of items, below), then one byte, `format`, then one item in the tree's form (below); refused when that length is more than `6 × (input length) + 16` (the whole input's length, these three bytes included), when the item is cut short or bytes follow it, or when a text in it holds a zero byte (fewer than three bytes: refused) | the text the original prints, without its zero; refused when the original fails (it returns false) |
| `parse_value` | `parse_value`, with all it calls (`parse_array`, `parse_object`, `parse_string`, `parse_number`, `buffer_skip_whitespace`) | two bytes, low byte first, the room, counted in allocations (the tree of items, below), then a buffer (below): its offset, then its content (fewer than four bytes: refused) | one byte, 1 when the original succeeds and 0 when it fails; then the offset it leaves (two bytes, low byte first; it moves the offset when it fails too); then, when it succeeds, the item it parsed, in the tree's form (below); refused when that answer is longer than `6 × (input length) + 16` bytes (the buffer the leaf answers in) |
| `parse_with_length_opts` | `cJSON_ParseWithLengthOpts`, with all it calls (`skip_utf8_bom`, `buffer_skip_whitespace`, `parse_value` and all that calls) | two bytes, low byte first, the room, counted in allocations (the tree of items, below), then one byte, `require_null_terminated` (0: false; any other value: true), then the text: `value` is its first byte and `buffer_length` its length, the count of bytes that follow these three (zero bytes among them too; 0 when none) (fewer than three bytes: refused) | one byte, 1 when the original returns an item and 0 when it returns `NULL`; then where it leaves `*return_parse_end`, as its distance from `value` (two bytes, low byte first); then, when it returns an item, that item in the tree's form (below); refused when that answer is longer than `6 × (input length) + 16` bytes (the buffer the leaf answers in) |
| `compare` | `cJSON_Compare`, with all it calls (`compare_double`, `get_object_item`, `case_insensitive_strcmp`) | one byte, `case_sensitive` (0: false; any other value: true), then the item `a` in the tree's form (below), then the item `b` in the same form, or nothing: then `b` is `a` itself (the original is given the same pointer twice); refused when an item is cut short, when a text in them holds a zero byte, or when bytes follow `b` (fewer than two bytes: refused) | one byte: 1 when the original returns true, else 0 |
| `duplicate` | `cJSON_Duplicate`, with all it calls (`cJSON_Duplicate_rec`, `cJSON_New_Item`, `cJSON_strdup`) | two bytes, low byte first, the room, counted in allocations (the tree of items, below), then one byte, `recurse` (0: false; any other value: true), then one item in the tree's form (below), named: its name (a text) right after its type byte, as a child of an object is written; refused when the item is cut short, when a text in it holds a zero byte, or when bytes follow it (fewer than four bytes: refused) | the copy the original returns, in the same form, named likewise; refused when the original fails (it returns `NULL`) |

## The leaves that allocate their result

These leaves' original asks its allocator (`hooks.allocate`) for one block, for its result. The allocator here is
the driver's: it gives a block of exactly the size asked, zeroed, when that size is at most the room, and fails
(returns `NULL`) when it is larger. The room is the input's first two bytes, low byte first. So the size the
original asks is part of what it does: the leaf computes it, fails as the original does when the room is too small,
and answers the whole block as the original leaves it, the bytes it wrote first (a zero byte among them too, as
`\u0000` writes one), then its terminating zero, then the zeros the original did not write over, to the block's end.
(The buffer a leaf answers in is zeroed already: those last zeros are there without writing them; what the leaf must
get right is the block's length, the size the original asks.)

## The leaves of buffers

The original reads its input through a buffer: a `parse_buffer`, a `printbuffer` for `update_offset`. Of it,
`buffer_skip_whitespace`, `skip_utf8_bom`, `update_offset`, `parse_string` and `parse_value` read the content, its
`length` and the `offset`, and move the offset; they read nothing else of it but the allocator of `parse_string`
(above) and of `parse_value` (below), and the depth of `parse_value` (below). A buffer is given
as its offset, two bytes, low byte first, then its content: its length is the count of bytes that follow.

Any offset may be given, at the end of the content or beyond it too, and the answer is then what the original does,
but in two cases, where the leaf refuses: `update_offset` is not called where no zero byte lies at or after the offset
(the original would read past the buffer's end), and `parse_string` where no byte lies at the offset (the original
reads it without a check).

## The tree of items

A tree of `cJSON` items is given in one byte form: an item, then its children inside it, in order (preorder). An
item is its `type`, one byte (the field's low byte: `print_value` reads no more of it), then what that type holds:

- `cJSON_Number` (8): its `valuedouble` (a double, below), then its `valueint` (four bytes, low byte first, as a
  32-bit two's complement): the twelve bytes `print_number` is given;
- `cJSON_String` (16) and `cJSON_Raw` (128): its `valuestring`, a text;
- `cJSON_Array` (32) and `cJSON_Object` (64): the count of its children (two bytes, low byte first), then each
  child (`child`, then each `next`); a child of an object (its parent's type byte is 64) holds its name
  (`string`, a text) right after its type byte, before what its type holds;
- any other byte, `cJSON_False` (1), `cJSON_True` (2) and `cJSON_NULL` (4) as well as the bytes that are no type
  (0, 3, 255…): nothing more.

A text is its length (two bytes, low byte first), then its bytes. The length 65535 (`FF FF`) stands for `NULL`, and
no byte follows it. A field the form does not hold is zero, or `NULL`: the printers do not read it. A String whose
text is `NULL`, and a `NULL` name, print as the empty string `""`; a Raw whose text is `NULL` is not printed (below).
Only the children of an object are named: not the item given, not the children of an array. So `{"a": [true]}` is
`40 01 00`, `20 01 00 61 01 00`, `02`: the object (64, one child); its child, an array (32), named `a` (one byte,
`61`), with one child; that child, `true` (2), not named.

For `print_value`, the driver builds that tree and calls `cJSON_PrintPreallocated(item, buffer, length, format)`
with a buffer of exactly the length given (it does not allocate: its print buffer is `noalloc`), and `format` the
byte given (0: unformatted; any other value: formatted, as the original takes any `cJSON_bool` that is not 0). So the
original fails where `ensure` finds the buffer too short for what it is asked (as `ensure` counts: the bytes asked,
the offset, and one more, at most the length), and where it cannot print an item: a type that is none of the seven,
a Raw whose `valuestring` is `NULL`, an array or an object entered at the depth `CJSON_NESTING_LIMIT` (1000, the
value `reference/cJSON.h` gives it: the reference does not change it; the item given is entered at depth 0). The
length given is at most the buffer the leaf answers in (the section « The buffer », below): every text the original
prints in it fits there. Every failure is a refusal, of the form or of the original: a leaf may read the form as it
prints. A leaf that goes down one call for each array or object nested in another goes 1000 calls deep:
the driver runs with the usual stack of 8 MB, so each such call must keep its own locals well under 8 KB.

For `parse_value`, the driver gives the original a root item, zeroed (not counted in the room), and a parse buffer:
the content, its length, the offset given, the depth 0, and an allocator. The allocator is the driver's: for each of
the first `room` calls, it gives a zeroed block of the size asked, and from the next call on it fails (returns
`NULL`). So the room counts allocations, not bytes. The original asks for a block at these points and no others.
For each child of an array or an object, the child's item (`cJSON_New_Item`), before it reads anything of the child
(an object's child's name included): after the opening bracket, once blanks are skipped, unless nothing at all
follows the bracket or the closing bracket comes first, so also when only blanks follow to the end of the content
(`buffer_skip_whitespace` steps back onto the last blank), the parse then failing; and after each comma, whatever
follows, nothing included. When that block is refused, the offset is left on the comma, or, for the first child, one
byte before where the skipping of blanks after the bracket stopped. For each string, a name as much as a value, a
block once its closing quote is found, before its escapes are decoded (a string with a bad escape has taken its
block). For each value that begins with `-` or a digit, `parse_number`'s copy of the bytes it copies for `strtod`,
before `strtod` reads them, even when `strtod` then reads nothing (freed at once). It fails where an allocation
fails, and on an array or an object entered at the depth `CJSON_NESTING_LIMIT` (1000; the root is entered at depth
0), whose item its parent has asked for already; when it fails, it frees what it built, and the answer is its success
byte and its offset only. When it succeeds, the answer
then holds the root in the tree's form, as the original left it: the root is not named, the children of an object
are, with the names it parsed. A text is answered as the C string it is: its bytes up to its first zero byte (a
`\u0000` ends it). The form holds no more of an item than the fields above: not the `valueint` 1 a `true` is given,
not the `prev` links.

For `parse_with_length_opts`, the driver makes its allocator cJSON's own for the call (`cJSON_InitHooks`, with `free`
to free; with an allocator other than `malloc`, the original does not use `realloc`): the allocator `parse_value` is
given above, which counts its calls and fails from the call after the first `room` on. The original asks it first for
the root's item, before it reads anything (so a room of 0 fails at once; an empty text fails before that). It then
skips the UTF-8 mark (`EF BB BF`) when the text begins with it and is 5 bytes long or more
(`can_access_at_index(buffer, 4)`), then blanks, as `buffer_skip_whitespace` skips them (every byte up to 32, the zero
byte too; when it skips up to the end of the text, it steps back onto the last byte; at the end already, it moves
nothing), and calls `parse_value` there, at the depth 0: all that is said of `parse_value` above holds. With
`require_null_terminated`, after the value, it skips blanks again the same way and fails unless a zero byte lies where
it stops: so it succeeds only when every byte after the value is a blank and the last of them, the text's last byte,
is a zero byte. Where it leaves `*return_parse_end`: on success, at the parse buffer's offset then (just after the
value; with `require_null_terminated`, the text's last byte); on failure, at the offset the parse buffer was left at
when that lies within the text, else at the text's last byte (its length minus 1), and at 0 for an empty text.

For `compare`, the driver builds the two trees as for `print_value` (every field the form does not hold is zero, or
`NULL`) and calls `cJSON_Compare(a, b, case_sensitive)`. The original reads the types (the low byte), a Number's
`valuedouble` (compared by `compare_double`: its `valueint` is not compared), a String's and a Raw's `valuestring` (by
`strcmp`, with their case; not equal when either is `NULL`), an array's children in order (not equal when one array
is longer), and an object's children by their names: each child of `a` is looked up among the children of `b`
(`get_object_item`: the first child of that name; with `case_sensitive`, by `strcmp`, stopping at the first child
whose name is `NULL`; without it, by `case_insensitive_strcmp`, which lowers the letters `A` to `Z` alone, as the C
library's `tolower` does in the C locale, and passes over a child whose name is `NULL`; a `NULL` name finds nothing)
and compared with the child found, then each child of `b` among those of `a`, likewise. Given `a` twice, it says
equal at once, without reading further, when the type is one of `cJSON_False`, `cJSON_True`, `cJSON_NULL`,
`cJSON_Number`, `cJSON_String`, `cJSON_Raw`, `cJSON_Array` and `cJSON_Object`; any other type byte is never equal, not
even to itself. Its work doubles with each level of equal objects nested in one another (each child is compared from
both sides): the cases keep that within the judge's time for a leaf that does the same work.

For `duplicate`, the driver builds the item as for `print_value`, its name in its `string` field, and makes its
allocator cJSON's own for the call, as for `parse_with_length_opts`: it counts its calls, and fails from the call
after the first `room` on. The original asks for the item's block (`cJSON_New_Item`), then for a block for its
`valuestring` when that is not `NULL`, then for one for its name when that is not `NULL` (an empty text takes a block
too, of one byte); then, with `recurse`, it does the same for each child in order, each whole (a child is copied with
its children). It fails where an allocation fails, and frees what it built. The flags `cJSON_IsReference` and
`cJSON_StringIsConst` are not in the form: the item's type is its byte alone, so every name is copied. A copy made
without `recurse` has no children: an array or an object then has the count 0. Its `valueint` and `valuedouble` are
copied whatever its type; the form holds them for a Number alone. The cases never nest `CJSON_CIRCULAR_LIMIT` (10000)
deep.

`print_value`, `print_array` and `print_object` call one another, and `print_number` and `print_string_ptr`; a leaf
is built alone (`port/src/lib.rs`), so this one is the whole printer: its file holds all the code it runs. It may
copy what it needs of the accepted leaves `print_number` and `print_string` (their files under `port/src/` on the
branch `hive`); it cannot call them.

`parse_value`, `parse_array` and `parse_object` call one another, and `parse_string`, `parse_number` and
`buffer_skip_whitespace`; this leaf too is built alone, so it is the whole parser: its file holds all the code it
runs. It may copy what it needs of the accepted leaves `parse_number`, `parse_string`, `buffer_skip_whitespace`,
`utf16` and `hex4` (their files on the branch `hive`); it cannot call them. Their bounds are not this unit's: here a
number runs to thousands of digits (`parse_number` copies every byte it takes, up to the whole content), so a copied
`parse_number` with a fixed capacity will not do. The driver calls `parse_value` itself, at the offset given: nothing
skips blanks or the UTF-8 mark (`EF BB BF`) before the value, and nothing asks for the end of the text after it, as
`cJSON_ParseWithLengthOpts` would.

`cJSON_ParseWithLengthOpts` calls `skip_utf8_bom`, `buffer_skip_whitespace` and `parse_value` with all it calls;
`cJSON_Compare` calls itself, and `compare_double`, `get_object_item` and `case_insensitive_strcmp`; `cJSON_Duplicate`
calls `cJSON_Duplicate_rec`, which calls itself, `cJSON_New_Item` and `cJSON_strdup`. Each of these leaves too is built
alone, so its file holds all the code it runs. Each may copy what it needs of the accepted leaves (their files on the
branch `hive`): `parse_with_length_opts` of `parse_value` and `skip_utf8_bom` (the bounds of `parse_value` hold here
too: numbers of thousands of digits), `compare` of `compare_double` and `strcasecmp`, `duplicate` of `strdup`; it
cannot call them. A leaf that goes down one call for each array or object nested in another goes about 1000 calls deep
in these cases too: each such call must keep its own locals well under 8 KB.

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

The output buffer a leaf is given is zeroed and holds `6 × (input length) + 16` bytes: more than any right answer,
but for a success of `parse_value` or of `parse_with_length_opts`, which can be longer and is then refused (their
lines, above).
A leaf is a function of its input alone: the same input, the same answer, whatever was asked before.

The build checks integer arithmetic (`-C overflow-checks=on`): an overflow, like an index out of bounds, ends the
run at once, and the leaf is refused.
