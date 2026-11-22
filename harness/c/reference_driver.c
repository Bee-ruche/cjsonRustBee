/* The reference's driver: « reference <leaf> [cases [answers]] » reads a file of cases (/cases/cases) and writes a
   file of answers (/answers/answers), one answer per case.
   It holds the ORIGINAL whole (reference/cJSON.c is included as it is, so that its internal functions are reached
   here) and says, leaf by leaf, what the input is and what the answer is: port/LEAVES.md says the same in words.
   Each leaf is made a total function: where the original may not be called, the driver refuses and does not call it.

   Framing (the judge's): cases « <u32 length><bytes> »…, answers « <u32 index><u8 status><u32 length><bytes> »…,
   little-endian; status 0 with the answer, or 1, a refusal, with nothing. */
#ifdef ENABLE_LOCALES
#error "the leaves of numbers take '.' as the decimal point: the reference is built without ENABLE_LOCALES"
#endif
#include "../../reference/cJSON.c"

typedef int (*leaf_function)(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len);

static int leaf_hex4(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    unsigned value;
    if (n != 4 || cap < 2)
        return 1; /* the original reads exactly four bytes */
    value = parse_hex4(in);
    out[0] = (unsigned char)(value >> 8);
    out[1] = (unsigned char)value;
    *len = 2;
    return 0;
}

static int leaf_utf16(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    unsigned char written[8] = {0}, *at = written, read;
    if (cap < 5)
        return 1;
    read = utf16_literal_to_utf8(in, in + n, &at);
    if (read == 0)
        return 1;
    out[0] = read;
    memcpy(out + 1, written, (size_t)(at - written));
    *len = 1 + (size_t)(at - written);
    return 0;
}

static int leaf_minify(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    char *text;
    if (memchr(in, 0, n) || cap < n)
        return 1; /* the original takes a text ended by a zero byte */
    text = calloc(n + 8, 1);
    if (!text)
        exit(3);
    memcpy(text, in, n);
    cJSON_Minify(text);
    *len = strlen(text);
    memcpy(out, text, *len);
    free(text);
    return 0;
}

static int leaf_print_string(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    printbuffer buffer;
    unsigned char *text;
    if (memchr(in, 0, n))
        return 1; /* the original takes a text ended by a zero byte */
    text = calloc(n + 1, 1);
    if (!text)
        exit(3);
    memcpy(text, in, n);
    memset(&buffer, 0, sizeof buffer);
    buffer.buffer = out;
    buffer.length = cap;
    buffer.noalloc = true;
    if (!print_string_ptr(text, &buffer))
        exit(4); /* the buffer is large enough for every answer: this is the harness's own fault */
    free(text);
    *len = strlen((const char *)out);
    return 0;
}

static int leaf_strcasecmp(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    const unsigned char *zero = memchr(in, 0, n);
    unsigned value;
    /* two texts, each ended by one zero byte: the first zero byte is not the last byte, the second one is */
    if (cap < 4 || !zero || zero == in + n - 1 || in[n - 1] != 0 || memchr(zero + 1, 0, (size_t)(in + n - 1 - (zero + 1))))
        return 1;
    value = (unsigned)case_insensitive_strcmp(in, zero + 1);
    out[0] = (unsigned char)value;
    out[1] = (unsigned char)(value >> 8);
    out[2] = (unsigned char)(value >> 16);
    out[3] = (unsigned char)(value >> 24);
    *len = 4;
    return 0;
}

/* The leaves of numbers: a double is given (and parse_number answers one) as its eight bytes, the IEEE 754 binary64
   pattern, low byte first; an int as its four, low byte first, two's complement. The original's decimal point is the
   locale's only when it is built with ENABLE_LOCALES: it is not (above), so it is '.', whatever the locale. */
static double double_at(const unsigned char *in)
{
    unsigned long long bits = 0;
    double d;
    int i;
    for (i = 7; i >= 0; i--)
        bits = bits << 8 | in[i];
    memcpy(&d, &bits, sizeof d);
    return d;
}

static void put_le(unsigned char *out, unsigned long long value, int count)
{
    int i;
    for (i = 0; i < count; i++)
        out[i] = (unsigned char)(value >> (8 * i));
}

static int leaf_parse_number(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    parse_buffer buffer;
    cJSON item;
    unsigned long long bits;
    if (cap < 14)
        return 1;
    /* the input is the whole buffer, read from its start (the count read fits two bytes: the unit's cases are 256
       bytes at most) */
    memset(&buffer, 0, sizeof buffer);
    buffer.content = in;
    buffer.length = n;
    buffer.hooks = global_hooks;
    memset(&item, 0, sizeof item);
    if (!parse_number(&item, &buffer))
        return 1;
    memcpy(&bits, &item.valuedouble, sizeof bits);
    put_le(out, buffer.offset, 2);
    put_le(out + 2, bits, 8);
    put_le(out + 10, (unsigned)item.valueint, 4);
    *len = 14;
    return 0;
}

static int leaf_compare_double(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    if (n != 16 || cap < 1)
        return 1; /* two doubles, exactly */
    out[0] = compare_double(double_at(in), double_at(in + 8)) ? 1 : 0;
    *len = 1;
    return 0;
}

static int leaf_print_number(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    printbuffer buffer;
    cJSON item;
    unsigned valueint;
    if (n != 12)
        return 1; /* a double and an int, exactly */
    valueint = in[8] | in[9] << 8 | in[10] << 16 | (unsigned)in[11] << 24;
    memset(&item, 0, sizeof item);
    item.valuedouble = double_at(in);
    item.valueint = valueint <= INT_MAX ? (int)valueint : -(int)~valueint - 1;
    memset(&buffer, 0, sizeof buffer);
    buffer.buffer = out;
    buffer.length = cap;
    buffer.noalloc = true;
    if (!print_number(&item, &buffer))
        exit(4); /* the buffer is large enough for every answer: this is the harness's own fault */
    *len = buffer.offset;
    return 0;
}

/* The leaves of buffers: of the buffer it is given (a parse_buffer; a printbuffer for update_offset), the original
   reads the content, its length and the offset, and moves the offset. The input is the offset (two bytes, low byte
   first), then the content: the length is the count of bytes that follow. The answer is the offset the original
   leaves, two bytes, low byte first (it fits: the original moves an offset only up to the length, and the units' cases
   are 256 bytes at most). The buffer's other fields are not read by these three: they are zero here. */
static size_t offset_at(const unsigned char *in) { return (size_t)in[0] | (size_t)in[1] << 8; }

static int answer_offset(size_t offset, unsigned char *out, size_t *len)
{
    if (offset > 0xFFFF)
        exit(4); /* never: see above */
    put_le(out, offset, 2);
    *len = 2;
    return 0;
}

static int leaf_buffer_skip_whitespace(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    parse_buffer buffer;
    if (n < 2 || cap < 2)
        return 1; /* no offset */
    memset(&buffer, 0, sizeof buffer);
    buffer.content = in + 2;
    buffer.length = n - 2;
    buffer.offset = offset_at(in);
    buffer.hooks = global_hooks;
    if (buffer_skip_whitespace(&buffer) != &buffer)
        exit(4); /* never: its buffer and its content are not NULL */
    return answer_offset(buffer.offset, out, len);
}

static int leaf_skip_utf8_bom(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    parse_buffer buffer;
    if (n < 2 || cap < 2)
        return 1; /* no offset */
    memset(&buffer, 0, sizeof buffer);
    buffer.content = in + 2;
    buffer.length = n - 2;
    buffer.offset = offset_at(in);
    buffer.hooks = global_hooks;
    if (skip_utf8_bom(&buffer) == NULL)
        return 1; /* the original fails: the offset is not 0 */
    return answer_offset(buffer.offset, out, len);
}

static int leaf_update_offset(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    printbuffer buffer;
    size_t offset;
    if (n < 2 || cap < 2)
        return 1; /* no offset */
    offset = offset_at(in);
    if (offset >= n - 2 || !memchr(in + 2 + offset, 0, n - 2 - offset))
        return 1; /* no zero byte at or after the offset: the original would read past the buffer's end */
    memset(&buffer, 0, sizeof buffer);
    buffer.buffer = (unsigned char *)(in + 2); /* (the original only reads it) */
    buffer.length = n - 2;
    buffer.offset = offset;
    buffer.noalloc = true;
    buffer.hooks = global_hooks;
    update_offset(&buffer);
    return answer_offset(buffer.offset, out, len);
}

/* The leaves that allocate their result: the original asks its hooks for one block (allocate). The input's first two
   bytes, low byte first, are the room: the driver's allocator gives a zeroed block of exactly the size asked when that
   size is at most the room, and fails (returns NULL) above it. The answer holds the block as the original leaves it. */
static size_t room, asked;

static void *room_allocate(size_t size)
{
    asked = size;
    return size <= room ? calloc(size ? size : 1, 1) : NULL;
}

static int leaf_parse_string(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    parse_buffer buffer;
    cJSON item;
    cJSON_bool parsed;
    if (n < 4 || offset_at(in + 2) >= n - 4)
        return 1; /* no room or no offset; or no byte at the offset: the original reads it without a check */
    room = offset_at(in);
    asked = 0;
    memset(&buffer, 0, sizeof buffer);
    buffer.content = in + 4;
    buffer.length = n - 4;
    buffer.offset = offset_at(in + 2);
    buffer.hooks.allocate = room_allocate;
    buffer.hooks.deallocate = free;
    buffer.hooks.reallocate = NULL;
    memset(&item, 0, sizeof item);
    parsed = parse_string(&item, &buffer);
    if (buffer.offset > 0xFFFF || (parsed && (item.valuestring == NULL || 3 + asked > cap)))
        exit(4); /* never: the offset stays within the content, and the block within its length */
    out[0] = parsed ? 1 : 0;
    put_le(out + 1, buffer.offset, 2);
    *len = 3;
    if (parsed) {
        memcpy(out + 3, item.valuestring, asked);
        *len += asked;
        free(item.valuestring);
    }
    return 0;
}

static int leaf_strdup(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    internal_hooks hooks;
    unsigned char *text, *copy;
    if (n < 2 || memchr(in + 2, 0, n - 2))
        return 1; /* no room; or a zero byte: the original takes a text ended by one */
    text = calloc(n - 1, 1);
    if (!text)
        exit(3);
    memcpy(text, in + 2, n - 2);
    room = offset_at(in);
    asked = 0;
    hooks.allocate = room_allocate;
    hooks.deallocate = free;
    hooks.reallocate = NULL;
    copy = cJSON_strdup(text, &hooks);
    free(text);
    if (copy == NULL)
        return 1; /* the original fails: its allocation fails */
    if (asked > cap)
        exit(4); /* never */
    memcpy(out, copy, asked);
    *len = asked;
    free(copy);
    return 0;
}

/* The tree of items (port/LEAVES.md): an item is its type byte, then what that type holds (a Number its double and
   its int; a String or a Raw its valuestring; an Array or an Object the count of its children, then each child, an
   object's child with its name right after its type byte); a text is its length, two bytes, low byte first (FF FF:
   NULL), then its bytes. The decoder builds the C tree with every field the form holds set as given and every other
   zero, the children linked as the original links them (child, next, prev; the first child's prev is the last). It
   refuses a form cut short and a text with a zero byte: a C string cannot hold one. */
typedef struct { const unsigned char *at, *end; } reader;

static int read_text(reader *r, char **text)
{
    size_t length;
    if (r->end - r->at < 2)
        return 1;
    length = offset_at(r->at);
    r->at += 2;
    if (length == 0xFFFF)
        return 0;
    if ((size_t)(r->end - r->at) < length || memchr(r->at, 0, length))
        return 1;
    *text = malloc(length + 1);
    if (!*text)
        exit(3);
    memcpy(*text, r->at, length);
    (*text)[length] = '\0';
    r->at += length;
    return 0;
}

/* One item and all it holds; 1 when the form is refused. Whatever it built is in *made, linked, for cJSON_Delete. */
static int read_item(reader *r, int named, cJSON **made)
{
    cJSON *item, *last = NULL;
    size_t count, i;
    unsigned valueint;
    if (r->at == r->end)
        return 1;
    item = calloc(1, sizeof *item);
    if (!item)
        exit(3);
    *made = item;
    item->type = *r->at++;
    if (named && read_text(r, &item->string))
        return 1;
    switch (item->type) {
    case cJSON_Number:
        if (r->end - r->at < 12)
            return 1;
        item->valuedouble = double_at(r->at);
        valueint = r->at[8] | r->at[9] << 8 | r->at[10] << 16 | (unsigned)r->at[11] << 24;
        item->valueint = valueint <= INT_MAX ? (int)valueint : -(int)~valueint - 1;
        r->at += 12;
        return 0;
    case cJSON_String:
    case cJSON_Raw:
        return read_text(r, &item->valuestring);
    case cJSON_Array:
    case cJSON_Object:
        if (r->end - r->at < 2)
            return 1;
        count = offset_at(r->at);
        r->at += 2;
        for (i = 0; i < count; i++) {
            cJSON *child = NULL;
            int refused = read_item(r, item->type == cJSON_Object, &child);
            if (child) {
                if (last)
                    last->next = child, child->prev = last;
                else
                    item->child = child;
                last = child;
                item->child->prev = last;
            }
            if (refused)
                return 1;
        }
        return 0;
    default:
        return 0;
    }
}

/* The printer: the tree given, printed by cJSON_PrintPreallocated into a buffer of exactly the length given (two
   bytes, low byte first: a write past it is seen by the sanitizer), the format the byte given (any cJSON_bool). The
   answer is the text without its zero, or a refusal where the original fails. The length is at most the answer's
   buffer, `cap`: the text and its zero fit in that length, so the answer fits. */
static int leaf_print_value(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    reader r;
    cJSON *root = NULL;
    char *buffer;
    size_t length;
    cJSON_bool printed;
    if (n < 3 || offset_at(in) > cap)
        return 1; /* no length or no format; or a length the answer's buffer could not hold */
    length = offset_at(in);
    r.at = in + 3;
    r.end = in + n;
    if (read_item(&r, 0, &root) || r.at != r.end) {
        cJSON_Delete(root);
        return 1; /* the item cut short, a text with a zero byte, or bytes after the item */
    }
    buffer = calloc(length ? length : 1, 1);
    if (!buffer)
        exit(3);
    printed = cJSON_PrintPreallocated(root, buffer, (int)length, (cJSON_bool)in[2]);
    cJSON_Delete(root);
    if (!printed) {
        free(buffer);
        return 1; /* the original fails */
    }
    *len = strnlen(buffer, length);
    if (*len >= length)
        exit(4); /* never: the original ends its text with a zero, within the length */
    memcpy(out, buffer, *len);
    free(buffer);
    return 0;
}

/* The parsers: parse_value, given a root item (zeroed, the driver's: not counted in the room) and a parse buffer (the
   content, its length, the offset given, the depth 0), with an allocator that gives a zeroed block of the size asked
   for each of the first `room` calls and fails from the next on. The answer: the success byte, the offset left (two
   bytes, low byte first), and on success the root in the tree's form; a text as the C string it is, its bytes up to its
   zero. Refused when that answer would not fit the answer's buffer. */
static size_t calls;

static void *counted_allocate(size_t size)
{
    if (calls >= room)
        return NULL;
    calls++;
    return calloc(size ? size : 1, 1);
}

typedef struct { unsigned char *out; size_t cap, at; } writer;

static int write_byte(writer *w, unsigned value)
{
    if (w->at >= w->cap)
        return 1;
    w->out[w->at++] = (unsigned char)value;
    return 0;
}

static int write_text(writer *w, const char *text)
{
    size_t length = text ? strlen(text) : 0xFFFF, i;
    if (text && length >= 0xFFFF)
        exit(4); /* never: the content is shorter */
    if (write_byte(w, length & 255) || write_byte(w, length >> 8 & 255))
        return 1;
    for (i = 0; text && i < length; i++)
        if (write_byte(w, (unsigned char)text[i]))
            return 1;
    return 0;
}

static int write_item(writer *w, const cJSON *item, int named)
{
    const cJSON *child;
    unsigned long long bits;
    size_t count = 0;
    int i;
    if (item->type & ~0xFF)
        exit(4); /* never: the parsers set no flag */
    if (write_byte(w, (unsigned)item->type) || (named && write_text(w, item->string)))
        return 1;
    switch (item->type) {
    case cJSON_Number:
        memcpy(&bits, &item->valuedouble, sizeof bits);
        for (i = 0; i < 8; i++)
            if (write_byte(w, (unsigned)(bits >> (8 * i) & 255)))
                return 1;
        for (i = 0; i < 4; i++)
            if (write_byte(w, (unsigned)item->valueint >> (8 * i) & 255))
                return 1;
        return 0;
    case cJSON_String:
    case cJSON_Raw:
        return write_text(w, item->valuestring);
    case cJSON_Array:
    case cJSON_Object:
        for (child = item->child; child; child = child->next)
            count++;
        if (count > 0xFFFF)
            exit(4); /* never: the content is shorter */
        if (write_byte(w, count & 255) || write_byte(w, count >> 8 & 255))
            return 1;
        for (child = item->child; child; child = child->next)
            if (write_item(w, child, item->type == cJSON_Object))
                return 1;
        return 0;
    default:
        return 0;
    }
}

static int leaf_parse_value(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    parse_buffer buffer;
    cJSON *root;
    cJSON_bool parsed;
    writer w;
    if (n < 4)
        return 1; /* no room or no offset */
    room = offset_at(in);
    calls = 0;
    memset(&buffer, 0, sizeof buffer);
    buffer.content = in + 4;
    buffer.length = n - 4;
    buffer.offset = offset_at(in + 2);
    buffer.hooks.allocate = counted_allocate;
    buffer.hooks.deallocate = free;
    buffer.hooks.reallocate = NULL;
    root = calloc(1, sizeof *root);
    if (!root)
        exit(3);
    parsed = parse_value(root, &buffer);
    if (buffer.offset > 0xFFFF || cap < 3)
        exit(4); /* never: the offset moves only up to the content's length, beyond which nothing is read */
    out[0] = parsed ? 1 : 0;
    put_le(out + 1, buffer.offset, 2);
    w.out = out, w.cap = cap, w.at = 3;
    if (parsed && write_item(&w, root, 0)) {
        cJSON_Delete(root);
        return 1; /* the answer would not fit the answer's buffer */
    }
    cJSON_Delete(root);
    *len = w.at;
    return 0;
}

/* The functions on whole trees. cJSON_Duplicate and cJSON_ParseWithLengthOpts allocate through cJSON's own hooks: for one
   call the driver makes the counted allocator above cJSON's (cJSON_InitHooks: with an allocator other than malloc, the
   original does not use realloc), then gives malloc and free back; nothing else of the driver allocates through them. */
static void counted_hooks(size_t given)
{
    cJSON_Hooks hooks;
    hooks.malloc_fn = counted_allocate;
    hooks.free_fn = free;
    room = given, calls = 0;
    cJSON_InitHooks(&hooks);
}

/* The whole parse: the room (allocations, the root's item first), the flag require_null_terminated (any byte: a
   cJSON_bool), then the text: cJSON_ParseWithLengthOpts(text, its length, &end, flag), the text in the case's own block,
   so that a read beyond it is seen. The answer: the success byte, end - text (two bytes, low byte first), and on success
   the root in the tree's form. Refused when that answer would not fit the answer's buffer. */
static int leaf_parse_with_length_opts(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    const char *text, *end = NULL;
    cJSON *root;
    writer w;
    if (n < 3)
        return 1; /* no room or no flag */
    text = (const char *)in + 3;
    counted_hooks(offset_at(in));
    root = cJSON_ParseWithLengthOpts(text, n - 3, &end, (cJSON_bool)in[2]);
    cJSON_InitHooks(NULL);
    if (end == NULL || end < text || (size_t)(end - text) > n - 3 || n - 3 > 0xFFFF || cap < 3)
        exit(4); /* never: the original sets end, within the text */
    out[0] = root ? 1 : 0;
    put_le(out + 1, (size_t)(end - text), 2);
    w.out = out, w.cap = cap, w.at = 3;
    if (root && write_item(&w, root, 0)) {
        cJSON_Delete(root);
        return 1; /* the answer would not fit the answer's buffer */
    }
    cJSON_Delete(root);
    *len = w.at;
    return 0;
}

/* The comparison: the flag case_sensitive (any byte), then a, then b, or nothing: then b is a itself (the same pointer
   given twice). Both trees are built as for the printer. The answer: one byte, 1 when the original says equal. */
static int leaf_compare(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    reader r;
    cJSON *a = NULL, *b = NULL;
    int refused;
    if (n < 1 || cap < 1)
        return 1; /* no flag */
    r.at = in + 1;
    r.end = in + n;
    refused = read_item(&r, 0, &a);
    if (!refused && r.at != r.end)
        refused = read_item(&r, 0, &b) || r.at != r.end;
    if (refused) {
        cJSON_Delete(a);
        cJSON_Delete(b);
        return 1; /* an item cut short, a text with a zero byte, or bytes after b */
    }
    out[0] = cJSON_Compare(a, b ? b : a, (cJSON_bool)in[0]) ? 1 : 0;
    cJSON_Delete(a);
    cJSON_Delete(b);
    *len = 1;
    return 0;
}

/* The copy: the room (allocations), the flag recurse (any byte), then one item, named: its name right after its type
   byte, as an object's child is written. cJSON_Duplicate(item, flag). The answer: the copy in the tree's form, named
   likewise; refused where the original fails (it returns NULL). */
static int leaf_duplicate(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *len)
{
    reader r;
    cJSON *item = NULL, *copy;
    writer w;
    if (n < 3)
        return 1; /* no room or no flag */
    r.at = in + 3;
    r.end = in + n;
    if (read_item(&r, 1, &item) || r.at != r.end) {
        cJSON_Delete(item);
        return 1; /* the item cut short, a text with a zero byte, or bytes after it */
    }
    counted_hooks(offset_at(in));
    copy = cJSON_Duplicate(item, (cJSON_bool)in[2]);
    cJSON_InitHooks(NULL);
    cJSON_Delete(item);
    if (copy == NULL)
        return 1; /* the original fails: an allocation fails */
    w.out = out, w.cap = cap, w.at = 0;
    if (write_item(&w, copy, 1))
        exit(4); /* never: the copy's form is no longer than the item given */
    cJSON_Delete(copy);
    *len = w.at;
    return 0;
}

static unsigned long u32_at(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned long)p[3] << 24); }

static void put_u32(FILE *f, unsigned long v)
{
    unsigned char b[4];
    b[0] = (unsigned char)v, b[1] = (unsigned char)(v >> 8), b[2] = (unsigned char)(v >> 16), b[3] = (unsigned char)(v >> 24);
    fwrite(b, 1, 4, f);
}

int main(int argc, char **argv)
{
    static const struct { const char *name; leaf_function leaf; } leaves[] = {
        {"hex4", leaf_hex4}, {"utf16", leaf_utf16}, {"minify", leaf_minify}, {"print_string", leaf_print_string}, {"strcasecmp", leaf_strcasecmp},
        {"parse_number", leaf_parse_number}, {"compare_double", leaf_compare_double}, {"print_number", leaf_print_number},
        {"buffer_skip_whitespace", leaf_buffer_skip_whitespace}, {"skip_utf8_bom", leaf_skip_utf8_bom}, {"update_offset", leaf_update_offset},
        {"parse_string", leaf_parse_string}, {"strdup", leaf_strdup}, {"print_value", leaf_print_value}, {"parse_value", leaf_parse_value},
        {"parse_with_length_opts", leaf_parse_with_length_opts}, {"compare", leaf_compare}, {"duplicate", leaf_duplicate}};
    leaf_function leaf = NULL;
    FILE *in, *out;
    unsigned char *cases;
    long size;
    size_t at = 0, i;
    unsigned long index = 0;
    for (i = 0; argc >= 2 && argc <= 4 && i < sizeof leaves / sizeof *leaves; i++)
        if (!strcmp(argv[1], leaves[i].name))
            leaf = leaves[i].leaf;
    if (!leaf)
        return 2;
    in = fopen(argc > 2 ? argv[2] : "/cases/cases", "rb");
    if (!in || fseek(in, 0, SEEK_END) || (size = ftell(in)) < 0 || fseek(in, 0, SEEK_SET))
        return 3;
    cases = malloc((size_t)size + 1);
    if (!cases || fread(cases, 1, (size_t)size, in) != (size_t)size)
        return 3;
    fclose(in);
    out = fopen(argc > 3 ? argv[3] : "/answers/answers", "wb");
    if (!out)
        return 3;
    while (at + 4 <= (size_t)size) {
        unsigned long n = u32_at(cases + at);
        size_t cap = 6 * n + 16, len = 0;
        unsigned char *copy, *buffer;
        int status;
        at += 4;
        if (n > (size_t)size - at)
            return 3;
        /* the case in a block of its own, of exactly its length, so that a read beyond it is seen by the sanitizer */
        copy = malloc(n ? n : 1);
        buffer = calloc(cap, 1);
        if (!copy || !buffer)
            return 3;
        memcpy(copy, cases + at, n);
        status = leaf(copy, n, buffer, cap, &len);
        if (status == 0 && len > cap)
            return 4;
        put_u32(out, index++);
        fputc(status, out);
        put_u32(out, status == 0 ? len : 0);
        if (status == 0)
            fwrite(buffer, 1, len, out);
        free(copy);
        free(buffer);
        at += n;
    }
    free(cases); /* (LeakSanitizer would stop the reference on it, should a build leave no pointer to it on the stack) */
    return (at == (size_t)size && fclose(out) == 0) ? 0 : 3;
}
