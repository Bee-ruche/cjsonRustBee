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
        {"parse_number", leaf_parse_number}, {"compare_double", leaf_compare_double}, {"print_number", leaf_print_number}};
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
    return (at == (size_t)size && fclose(out) == 0) ? 0 : 3;
}
