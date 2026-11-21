/* The generator of cases: « generate <leaf> <count> [cases] », the seed (hexadecimal) on its input; writes /cases/cases
   (the judge's framing: « <u32 length><bytes> »…). For each leaf:
   - FIXED cases, at every draw: every constant the original compares with, and its neighbours (the bounds of a
     UTF-8 length, of the surrogates, of a digit, of an int, of the doubles, of a byte skipped, of a buffer's end,
     of the nesting); the longest input the manifest allows (LONGEST below is the manifest's largest « case_bytes »,
     and each leaf's own bound is the one its unit has there); the empty input;
   - DRAWN cases, from what the leaf's inputs are made of: every byte value, every length up to the leaf's bound,
     the hard paths on purpose (escapes, surrogate pairs whole and broken, texts cut short, comments left open,
     control characters, zero bytes where a leaf refuses them, trees of every kind of item, nested deep, with a
     buffer around the least length the printer takes, JSON texts with the room around the allocations the parser
     asks for), and the longest answers on purpose too (a text kept whole, a text whose every byte is written as six,
     two texts of unequal lengths up to the bound).
   The order is drawn too. A wrong leaf that is accepted names a path these cases do not reach: add the case here. */
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LONGEST = 3072, SHORT = 256 };   /* (SHORT: the bound of the leaves of buffers and of those that allocate, which drew their lengths from LONGEST when it was 256) */

static unsigned long long state[2];

static unsigned long long next(void)
{
    unsigned long long a = state[0], b = state[1];
    state[0] = b;
    a ^= a << 23;
    state[1] = a ^ b ^ (a >> 17) ^ (b >> 26);
    return state[1] + b;
}

static size_t below(size_t n) { return (size_t)(next() % n); }

typedef struct { unsigned char bytes[LONGEST]; size_t length; } text;

static void put(text *t, int byte)
{
    if (t->length < LONGEST)
        t->bytes[t->length++] = (unsigned char)byte;
}

static void puts_in(text *t, const char *s)
{
    while (*s)
        put(t, *s++);
}

static void hex_digits(text *t, unsigned value, int digits)
{
    static const char *lower = "0123456789abcdef", *upper = "0123456789ABCDEF";
    int i;
    for (i = digits - 1; i >= 0; i--)
        put(t, (below(2) ? lower : upper)[(value >> (4 * i)) & 15]);
}

static void escape(text *t, unsigned code)
{
    puts_in(t, "\\u");
    hex_digits(t, code, 4);
}

static void spoil(text *t, size_t most)
{
    /* one change now and then: a byte replaced, a byte dropped, a cut, a byte added */
    switch (below(10)) {
    case 0: if (t->length) t->bytes[below(t->length)] = (unsigned char)below(256); break;
    case 1: if (t->length) { size_t at = below(t->length); memmove(t->bytes + at, t->bytes + at + 1, t->length - at - 1); t->length--; } break;
    case 2: if (t->length) t->length = below(t->length + 1); break;
    case 3: if (t->length < most) put(t, (int)below(256)); break;
    default: break;
    }
    if (t->length > most)
        t->length = most;
}

static void hex4(text *t)
{
    static const char *some = "0123456789abcdefABCDEFgGxX -:/@`";
    size_t i, n = below(8) ? 4 : below(17);   /* (the leaf's bound: 16 bytes) */
    for (i = 0; i < n; i++)
        put(t, below(12) ? some[below(22)] : below(3) ? some[below(strlen(some))] : (int)below(256));
}

static void utf16(text *t)
{
    static const unsigned edges[] = {0x0000, 0x007F, 0x0080, 0x07FF, 0x0800, 0xD7FF, 0xD800, 0xDBFF, 0xDC00, 0xDFFF, 0xE000, 0xFFFF};
    unsigned first, second;
    switch (below(8)) {
    case 6: case 7: first = edges[below(sizeof edges / sizeof *edges)]; break;   /* a constant of the original, itself */
    case 0: first = (unsigned)below(0x80); break;
    case 1: first = 0x80 + (unsigned)below(0x780); break;
    case 2: first = 0x800 + (unsigned)below(0xD000); break;
    case 3: first = 0xE000 + (unsigned)below(0x2000); break;
    case 4: first = 0xDC00 + (unsigned)below(0x400); break;          /* a low surrogate first: the original fails */
    default: first = 0xD800 + (unsigned)below(0x400); break;         /* a high surrogate: a second escape must follow */
    }
    escape(t, first);
    if (first >= 0xD800 && first <= 0xDBFF) {
        switch (below(10)) {
        case 8: case 9: escape(t, edges[below(sizeof edges / sizeof *edges)]); break;   /* a constant, as the second half */
        case 0: break;                                               /* nothing follows */
        case 1: second = (unsigned)below(0xD800); escape(t, second); break;
        case 2: second = 0xD800 + (unsigned)below(0x400); escape(t, second); break;
        case 3: puts_in(t, "\\x"); hex_digits(t, 0xDC00 + (unsigned)below(0x400), 4); break;
        default: escape(t, 0xDC00 + (unsigned)below(0x400)); break;  /* a whole pair */
        }
    }
    if (below(3) == 0) { size_t more = below(21); while (more--) put(t, below(2) ? 'a' + (int)below(26) : (int)below(256)); }
    spoil(t, 32);                                                    /* (the leaf's bound: 32 bytes) */
}

static void json_string(text *t, size_t most)
{
    size_t n = below(12);
    put(t, '"');
    while (n-- && t->length < most) {
        switch (below(9)) {
        case 0: puts_in(t, "\\\""); break;
        case 1: puts_in(t, "\\\\"); break;
        case 2: put(t, ' '); break;
        case 3: put(t, '/'); put(t, below(2) ? '/' : '*'); break;    /* what looks like a comment, inside a string */
        case 4: put(t, '\\'); break;
        case 5: put(t, 1 + (int)below(255)); break;                  /* any byte but zero, inside a string */
        default: put(t, 'a' + (int)below(26)); break;
        }
    }
    if (below(8))
        put(t, '"');                                                 /* now and then the string is left open */
}

/* a byte the original keeps outside a string: neither white space, nor a slash, nor a quote, nor zero */
static int kept(void)
{
    int c = 1 + (int)below(255);
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '/' || c == '"' ? 'k' : c;
}

static void minify(text *t)
{
    static const char *marks = "{}[]:,";
    size_t parts = below(4) ? below(24) : below(120), most = 256;    /* (the leaf's bound: 256 bytes) */
    if (below(10) == 0) {
        /* a text the original keeps whole, of any length and often the longest: the answer is as long as the input */
        size_t n = below(2) ? most - below(4) : below(most + 1);
        if (below(3) == 0 && n) {
            put(t, '"');                                             /* one string, left open: all of it is kept */
            while (--n) { int c = 1 + (int)below(255); put(t, c == '"' ? 'q' : c); }
            return;
        }
        while (n--)
            put(t, below(3) ? kept() : below(2) ? marks[below(6)] : '0' + (int)below(10));
        return;
    }
    while (parts-- && t->length < most) {
        switch (below(14)) {
        case 12: put(t, 1 + (int)below(255)); break;                 /* any byte but zero: every control character, every high byte */
        case 13: put(t, "\v\f\b\x7f\x1f\x01\x80\xff"[below(8)]); break;
        case 0: case 1: put(t, " \t\r\n"[below(4)]); break;
        case 2: json_string(t, most); break;
        case 3: puts_in(t, "//"); { size_t n = below(8); while (n--) put(t, below(6) ? 'a' + (int)below(26) : 1 + (int)below(255)); } if (below(4)) put(t, '\n'); break;
        case 4: puts_in(t, "/*"); { size_t n = below(8); while (n--) put(t, below(5) ? (below(6) ? 'a' + (int)below(26) : 1 + (int)below(255)) : '*'); } if (below(4)) puts_in(t, "*/"); break;
        case 5: put(t, '/'); break;
        case 6: put(t, '*'); break;
        case 7: put(t, '0' + (int)below(10)); break;
        case 8: if (below(40) == 0) put(t, 0); else put(t, marks[below(6)]); break;   /* a zero byte: refused */
        default: put(t, marks[below(6)]); break;
        }
    }
    if (t->length > most)
        t->length = most;
}

/* a byte print_string writes as it is: nothing to escape */
static int plain_byte(void)
{
    static const char *plain = "abcdefghijklmnopqrstuvwxyz ABC0189/'{}[]:,~";
    return below(8) ? plain[below(43)] : 127 + (int)below(129);
}

static void print_string(text *t)
{
    size_t n = below(4) ? below(40) : below(257);                    /* (the leaf's bound: 256 bytes) */
    int dense = below(5) == 0;                                       /* nearly every byte needs an escape */
    size_t kind = below(10);
    if (kind == 0) {
        /* the longest answers: a text near the bound whose bytes are nearly all written as six */
        static const char *six = "\001\002\003\004\005\006\007\013\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037";
        n = 200 + below(57);
        while (n--)
            put(t, below(12) ? six[below(26)] : 32 + (int)below(95));
        return;
    }
    if (kind == 1) {
        /* a text of any length, often the longest, with nothing to escape */
        n = below(2) ? below(257) : 256 - below(4);
        while (n--)
            put(t, plain_byte());
        return;
    }
    while (n--) {
        switch (dense ? below(3) : below(10)) {
        case 0: put(t, 1 + (int)below(31)); break;                   /* a control character */
        case 1: put(t, "\"\\\b\f\n\r\t"[below(7)]); break;
        case 2: put(t, 128 + (int)below(128)); break;
        case 3: put(t, 127); break;
        case 4: if (below(30) == 0) put(t, 0); else put(t, ' '); break;  /* a zero byte: refused */
        default: put(t, 32 + (int)below(95)); break;
        }
    }
}

static void strcasecmp_case(text *t)
{
    unsigned char first[64];
    size_t shape = below(16), wide = below(4) == 0, n, m, i;         /* (the leaf's bound: 64 bytes, two texts and their zeros) */
    /* one case in four, the two texts share as they like the 62 bytes their zeros leave: a long text and a short one,
       either way round; otherwise two texts of the same length or nearly */
    n = wide ? below(63) : below(32);
    m = wide ? below((n < 62 - n ? n : 62 - n) + 1) : n;             /* how much of the first text the second begins with */
    if (shape == 4)
        return;                                                      /* the empty input: refused */
    for (i = 0; i < n; i++)
        first[i] = (unsigned char)(below(8) ? (below(2) ? 'a' : 'A') + (int)below(26) : below(2) ? 1 + (int)below(255) : "[\\]^_`@{"[below(8)]);
    for (i = 0; i < n; i++)
        put(t, first[i]);
    if (shape == 5)
        return;                                                      /* no zero byte at all: refused */
    put(t, 0);
    if (shape == 0)
        return;                                                      /* one text only: refused */
    for (i = 0; i < m; i++) {
        int c = first[i];
        if (below(3) == 0 && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
            c ^= 32;                                                 /* the other case */
        put(t, c);
    }
    if (below(3) == 0 && m)
        t->bytes[t->length - 1 - below(m)] = (unsigned char)(1 + below(255));   /* one byte differs */
    if (wide) {
        size_t more = below(62 - n - m + 1);                         /* as much more as the bound leaves */
        while (more--) put(t, 'a' + (int)below(26));
    } else {
        if (below(4) == 0)
            t->length -= below(m + 1);                               /* a prefix */
        if (below(4) == 0) { size_t more = 1 + below(3); while (more--) put(t, 'a' + (int)below(26)); }
    }
    if (shape == 1)
        return;                                                      /* no zero byte at the end: refused */
    if (shape == 6 && t->length > n + 1)
        t->bytes[n + 1 + below(t->length - n - 1)] = 0;              /* a zero byte inside the second text: refused */
    put(t, 0);
    if (shape == 2)
        put(t, 0);                                                   /* three zero bytes: refused */
    if (shape == 3)
        put(t, 'x');                                                 /* something after the second: refused */
}

static void digits(text *t, size_t n)
{
    while (n--)
        put(t, '0' + (int)below(10));
}

/* The exact text of the middle of two neighbouring doubles: (2m+1)·2^-k for a mantissa m of 53 bits, written in decimal
   as (2m+1)·5^k with the point k digits from its end (k > 0), or as the integer (2m+1)·2^-k (k ≤ 0); strtod rounds it
   to the even one. Now and then a little more is added, or taken (it must round up, or down), and now and then the
   point is written as an exponent instead. */
static void middle(text *t)
{
    unsigned char digit[200];                                        /* least significant first */
    size_t length = 0, i, j, times;
    int k = (int)below(161) - 10, more = (int)below(3), exponent = k > 0 && below(3) == 0, point = 0;
    unsigned long long odd = 1ULL << 53 | (next() & ((1ULL << 53) - 1)) | 1;
    for (; odd; odd /= 10)
        digit[length++] = (unsigned char)(odd % 10);
    for (times = (size_t)(k > 0 ? k : -k); times--; ) {
        unsigned carry = 0;
        for (j = 0; j < length; j++) {
            unsigned v = digit[j] * (k > 0 ? 5u : 2u) + carry;
            digit[j] = (unsigned char)(v % 10), carry = v / 10;
        }
        if (carry)
            digit[length++] = (unsigned char)carry;
    }
    if (more == 2 && digit[0] == 0)
        more = 0;                                                    /* (taking a little would borrow: the middle itself) */
    if (more == 2)
        digit[0]--;                                                  /* below: the last digit one less, then nines */
    if (below(4) == 0)
        put(t, '-');
    if (k > 0 && !exponent && length <= (size_t)k) {
        puts_in(t, "0.");
        for (i = length; i < (size_t)k; i++)
            put(t, '0');
        point = 1;
    }
    for (i = length; i-- > 0; ) {
        put(t, '0' + digit[i]);
        if (i == (size_t)k && k > 0 && !exponent && i != 0 && !point)
            put(t, '.'), point = 1;
    }
    if (more && !point)
        put(t, '.');
    if (more == 1) {                                                 /* above: zeros, then a one */
        for (i = below(4); i--; )
            put(t, '0');
        put(t, '1');
    } else if (more == 2) {
        for (i = 1 + below(4); i--; )
            put(t, '9');
    }
    if (exponent) {
        char e[16];
        snprintf(e, sizeof e, "%c-%s%d", below(2) ? 'e' : 'E', below(4) ? "" : "0", k);
        puts_in(t, e);
    }
}

static void parse_number_case(text *t)
{
    static const char *copied = "0123456789+-eE.";                  /* what the original copies for strtod: it stops at any other byte */
    size_t n;
    char s[40];
    switch (below(12)) {
    case 0: case 1:                                                  /* the bytes the original copies, in any order */
        for (n = below(12); n--; )
            put(t, below(6) ? copied[below(15)] : (int)below(256));
        break;
    case 2: case 3:
        middle(t);
        break;
    case 4:                                                          /* at the bounds of the doubles: around 1e308, 1e-308 and 1e-324 */
        if (below(3) == 0)
            put(t, '-');
        put(t, '1' + (int)below(9));
        if (below(2)) { put(t, '.'); digits(t, below(20)); }
        snprintf(s, sizeof s, "%c%d", below(2) ? 'e' : 'E', below(2) ? 290 + (int)below(40) : -(290 + (int)below(45)));
        puts_in(t, s);
        break;
    case 5:                                                          /* around an int's bounds: valueint's saturation */
        {
            long long v = 2147483647LL + (long long)below(5) - 2;
            snprintf(s, sizeof s, "%lld", below(2) ? v : -v - 1);
            puts_in(t, s);
            if (below(2)) { put(t, '.'); digits(t, 1 + below(10)); }
        }
        break;
    default:                                                         /* a number as JSON writes it, or nearly */
        if (below(3) == 0)
            put(t, below(4) ? '-' : '+');
        digits(t, below(6) ? below(18) : below(300));
        if (below(2)) { put(t, '.'); digits(t, below(6) ? below(18) : below(300)); }
        if (below(2)) {
            put(t, below(2) ? 'e' : 'E');
            if (below(2))
                put(t, below(2) ? '+' : '-');
            digits(t, below(5) ? 1 + below(3) : below(30));
        }
        break;
    }
    if (below(3) == 0)                                               /* and what follows it */
        for (n = 1 + below(3); n--; )
            put(t, below(2) ? copied[below(15)] : (int)below(256));
}

/* The leaves of numbers are given a double as its eight bytes, the IEEE 754 binary64 pattern, low byte first. */
static void put_bits(text *t, unsigned long long bits)
{
    int i;
    for (i = 0; i < 8; i++)
        put(t, (int)(bits >> (8 * i) & 255));
}

static unsigned long long bits_of(double d)
{
    unsigned long long bits;
    memcpy(&bits, &d, sizeof bits);
    return bits;
}

static double double_of(unsigned long long bits)
{
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

static void put_int(text *t, unsigned value)
{
    put(t, (int)(value & 255)), put(t, (int)(value >> 8 & 255)), put(t, (int)(value >> 16 & 255)), put(t, (int)(value >> 24 & 255));
}

#define SIGN 0x8000000000000000ULL
#define ONE 0x3FF0000000000000ULL
#define TWO 0x4000000000000000ULL
#define INFINITE 0x7FF0000000000000ULL
#define QUIET_NAN 0x7FF8000000000000ULL
#define GREATEST 0x7FEFFFFFFFFFFFFFULL

/* the doubles every leaf of numbers meets often, as patterns: the zeros, the subnormals' bounds, the greatest double,
   the infinities and NaNs (quiet, signalling, negative), 1, DBL_EPSILON, an int's bounds, 2^53, 2^-1023 and its next */
static const unsigned long long EDGE_BITS[] = {
    0x0000000000000000ULL, 0x8000000000000000ULL, 0x0000000000000001ULL, 0x000FFFFFFFFFFFFFULL, 0x0010000000000000ULL,
    0x7FEFFFFFFFFFFFFFULL, 0x7FF0000000000000ULL, 0xFFF0000000000000ULL, 0x7FF8000000000000ULL, 0xFFF8000000000000ULL,
    0x7FF0000000000001ULL, 0x7FFFFFFFFFFFFFFFULL, 0x3FF0000000000000ULL, 0x3CB0000000000000ULL, 0x41DFFFFFFFC00000ULL,
    0xC1E0000000000000ULL, 0x4340000000000000ULL, 0x0008000000000000ULL, 0x0008000000000001ULL};

/* and as values: what %g writes in one style or the other (its bounds 1e-5, 1e-4, 1e15, 1e17), what 15 digits do not give
   back exactly (0.1 + 0.2: the original's comparison takes it anyway; 1/3: it does not), powers of two (2^-25: its 18
   digits end in a 5 alone after the 17th, a tie, rounded to the even digit), an int's bounds and their halves */
static const double EDGE_VALUES[] = {
    0.1, 0.2, 0.3, 0.1 + 0.2, 1.0 / 3, 2.0 / 3, 0.5, 1.5, 2.5, 100, 1e-5, 1e-4, 0.000123456789012345, 9.99999999999999e-5,
    1e15, 1e16, 1e17, 1e21, 1e22, 1e23, 1e100, 123456789012345.0, 999999999999999.0, 1234567890123456.0, 0x1p-25, 0x1p-26,
    0x1p-30, 0x1p-60, 2147483646.5, 2147483647.5, -2147483647.5, -2147483648.5, 4294967296.0, 1.7976931348623157e308,
    2.2250738585072014e-308, 4.9406564584124654e-324};

/* a double, its pattern: an edge, any pattern, an integer, a short decimal (the nearest double to k·10^±j), a power of
   two, a subnormal, any between 2^-70 and 2^70; now and then of the other sign, or a few units in the last place away */
static unsigned long long some_double(void)
{
    unsigned long long bits, k;
    double scale = 1;
    size_t j;
    switch (below(10)) {
    case 0: bits = EDGE_BITS[below(sizeof EDGE_BITS / sizeof *EDGE_BITS)]; break;
    case 1: bits = bits_of(EDGE_VALUES[below(sizeof EDGE_VALUES / sizeof *EDGE_VALUES)]); break;
    case 2: bits = next(); break;
    case 3: bits = bits_of((double)((long long)below(4000001) - 2000000)); break;
    case 4: case 5:
        for (k = 1, j = 1 + below(17); j--; )
            k *= 10;
        k = next() % k;
        for (j = below(23); j--; )
            scale *= 10;                                             /* (10^22 at most: exact) */
        bits = bits_of(below(2) ? (double)k / scale : (double)k * scale);
        break;
    case 6: bits = (unsigned long long)below(2047) << 52; break;
    case 7: bits = next() & 0xFFFFFFFFFFFFFULL; break;
    default: bits = (unsigned long long)(1023 - 70 + below(141)) << 52 | (next() & 0xFFFFFFFFFFFFFULL); break;
    }
    if (below(4) == 0)
        bits ^= SIGN;
    if (below(4) == 0)
        bits += (unsigned long long)below(9) - 4;
    return bits;
}

static void compare_double_case(text *t)
{
    unsigned long long a = some_double(), b, kept;
    switch (below(6)) {
    case 0: b = a; break;
    case 1: b = some_double(); break;
    case 2: b = a ^ SIGN; break;                                     /* the same, of the other sign */
    default: b = a + (unsigned long long)below(9) - 4; break;        /* a few units in the last place away, across a power of two too */
    }
    if (below(2))
        kept = a, a = b, b = kept;
    put_bits(t, a);
    put_bits(t, b);
    switch (below(60)) {
    case 0: t->length = below(t->length); break;                     /* cut short: refused */
    case 1: put(t, (int)below(256)); break;                          /* a byte too many: refused */
    default: break;
    }
}

static void print_number_case(text *t)
{
    unsigned long long bits = some_double();
    double d = double_of(bits);
    unsigned valueint;
    switch (below(6)) {
    case 0: valueint = (unsigned)next(); break;                      /* any int */
    case 1: valueint = 0; break;
    default:                                                         /* the one the original sets beside the double (cJSON_SetNumberHelper's) */
        valueint = d != d ? 0u : d >= 2147483647.0 ? 2147483647u : d <= -2147483648.0 ? 2147483648u : (unsigned)(int)d;
        if (below(8) == 0)
            valueint += (unsigned)below(3) - 1;
        break;
    }
    put_bits(t, bits);
    put_int(t, valueint);
    switch (below(60)) {
    case 0: t->length = below(t->length); break;                     /* cut short: refused */
    case 1: put(t, (int)below(256)); break;                          /* a byte too many: refused */
    default: break;
    }
}

/* The leaves of buffers: an offset (two bytes, low byte first), then the buffer's content, whose length is the count of
   bytes that follow (port/LEAVES.md). The offset is drawn where the original's tests change: at the start, inside, on
   the last byte, at the end and beyond it (where buffer_skip_whitespace moves nothing), far beyond (the high byte set) */
static void put_offset(text *t, size_t offset)
{
    put(t, (int)(offset & 255));
    put(t, (int)(offset >> 8 & 255));
}

static size_t some_offset(size_t n)
{
    switch (below(10)) {
    case 0: return 0;
    case 1: return n;                                                /* at the end */
    case 2: return n + 1 + below(3);                                 /* beyond it */
    case 3: return n ? n - 1 : 0;                                    /* the last byte */
    case 4: return below(2) ? 65535 : 256 * (1 + below(255)) + below(2) * below(256);   /* far beyond */
    default: return below(n + 1);
    }
}

static int skipped_byte(void)                                        /* a byte buffer_skip_whitespace skips: 0 to 32 */
{
    return below(2) ? " \t\n\r"[below(4)] : below(4) ? (int)below(33) : below(2) ? 0 : 32;
}

static int stopping_byte(void)                                       /* a byte it stops at: 33 to 255 */
{
    switch (below(6)) {
    case 0: return 33;
    case 1: return 128 + (int)below(128);                            /* (a signed char would be less than 32) */
    case 2: return below(2) ? 127 : 255;
    case 3: return 160;                                              /* (32 + 128) */
    default: return 33 + (int)below(223);
    }
}

static void buffer_skip_whitespace_case(text *t)
{
    unsigned char content[SHORT - 2];
    size_t n = below(4) ? below(SHORT - 1) : below(9), i = 0;        /* (the leaf's bound: 256 bytes, the offset's two included) */
    while (i < n) {                                                  /* runs of bytes skipped and of bytes kept, short and long */
        size_t run = below(4) ? 1 + below(4) : 1 + below(n);
        int skipped = (int)below(2);
        for (; run > 0 && i < n; run--)
            content[i++] = (unsigned char)(skipped ? skipped_byte() : stopping_byte());
    }
    if (n && below(4) == 0)                                          /* nothing but bytes skipped to the end: the original steps back one */
        for (i = below(n); i < n; i++)
            content[i] = (unsigned char)skipped_byte();
    put_offset(t, some_offset(n));
    for (i = 0; i < n; i++)
        put(t, content[i]);
    if (below(60) == 0)
        t->length = below(2);                                        /* no offset: refused */
}

/* Marks that are not UTF-8's (its length first), which the original does not skip: UTF-16 and UTF-32 both ways,
   UTF-7, UTF-1, a UTF-16 mark and a zero byte; and UTF-8's twice, which it skips once */
static const unsigned char OTHER_MARKS[][7] = {
    {2, 0xFE, 0xFF}, {2, 0xFF, 0xFE}, {4, 0x00, 0x00, 0xFE, 0xFF}, {4, 0xFF, 0xFE, 0x00, 0x00}, {3, 0x2B, 0x2F, 0x76},
    {3, 0xF7, 0x64, 0x4C}, {3, 0xFE, 0xFF, 0x00}, {6, 0xEF, 0xBB, 0xBF, 0xEF, 0xBB, 0xBF}};

static void skip_utf8_bom_case(text *t)
{
    static const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    size_t n = below(2) ? 2 + below(5) : below(15), i, from = 0;     /* (the leaf's bound: 16 bytes, the offset's two included) */
    if (below(6) == 0)                                               /* an offset that is not 0: refused (the original fails) */
        put_offset(t, below(3) ? 1 + below(n + 2) : below(2) ? 256 * (1 + below(255)) : 1 + below(65535));
    else
        put_offset(t, 0);
    if (below(5) == 0) {                                             /* another mark at the start */
        const unsigned char *mark = OTHER_MARKS[below(sizeof OTHER_MARKS / sizeof *OTHER_MARKS)];
        for (from = 0; from < mark[0]; from++)
            put(t, mark[1 + from]);
        n = from + below(15 - from);
    }
    for (i = from; i < n; i++) {
        int c = (int)below(256);
        if (i < 3 && below(6))                                       /* the mark, whole or nearly */
            c = below(8) ? bom[i] : below(2) ? bom[i] + 1 - 2 * (int)below(2) : bom[(i + 1) % 3];
        else if (below(5) == 0)
            c = bom[below(3)];                                       /* a byte of the mark elsewhere */
        else if (below(4) == 0)
            c = 0;
        put(t, c);
    }
    if (below(60) == 0)
        t->length = below(2);                                        /* no offset: refused */
}

static void update_offset_case(text *t)
{
    unsigned char content[SHORT - 2];
    size_t n = below(4) ? below(SHORT - 1) : below(9), zeros, i, offset;
    for (i = 0; i < n; i++)
        content[i] = (unsigned char)(below(4) ? 1 + (int)below(255) : (unsigned char)"a \x7f\x80\xff"[below(5)]);
    switch (below(5)) {                                              /* zero bytes: none (refused), one, a few, many */
    case 0: zeros = 0; break;
    case 1: case 2: zeros = 1; break;
    case 3: zeros = 2 + below(3); break;
    default: zeros = n ? 1 + below(n) : 0; break;
    }
    for (i = 0; i < zeros && n; i++)
        content[below(n)] = 0;
    if (n && below(5) == 0)
        content[n - 1] = 0;                                          /* the last byte */
    offset = some_offset(n);                                         /* (where no zero byte lies at or after it: refused) */
    if (n && below(3) == 0) {                                        /* on a zero byte, or just after it */
        size_t at = below(n);
        while (at < n && content[at])
            at++;
        if (at < n)
            offset = at + below(2);
    }
    put_offset(t, offset);
    for (i = 0; i < n; i++)
        put(t, content[i]);
    if (below(60) == 0)
        t->length = below(2);                                        /* no offset: refused */
}

/* The leaves that allocate their result: the room first (two bytes, low byte first; put as an offset is), the most the
   allocator gives. parse_string's room is drawn around the size the original asks: from its opening quote to its
   closing one, less one byte for each backslash, plus one (0: it fails before it asks) */
static size_t string_size(const unsigned char *content, size_t n, size_t offset)
{
    size_t end = offset + 1, skipped = 0;
    if (offset >= n || content[offset] != '"')
        return 0;
    while (end < n && content[end] != '"') {
        if (content[end] == '\\') {
            if (end + 1 >= n)
                return 0;
            skipped++;
            end++;
        }
        end++;
    }
    return end < n ? end - offset - skipped + 1 : 0;
}

static size_t some_room(size_t size)
{
    switch (below(9)) {
    case 0: return size;                                             /* exactly the size asked */
    case 1: return size ? size - 1 : 0;                              /* one byte short: the allocation fails */
    case 2: return size + 1;
    case 3: return below(size + 1);
    case 4: return 0;
    case 5: return 256 * (1 + below(255));                           /* the high byte alone */
    default: return below(2) ? 65535 : size + below(65536 - size);
    }
}

static void string_body(text *t, size_t most)                        /* what lies between the quotes */
{
    static const unsigned bmp[] = {0x41, 0x7f, 0x80, 0xe9, 0x7ff, 0x800, 0x20ac, 0xd7ff, 0xe000, 0xfffd, 0xffff, 0};
    while (t->length < most) {
        switch (below(14)) {
        case 0: case 1: case 2: case 3: put(t, 'a' + (int)below(26)); break;
        case 4: { int c = (int)below(256); put(t, c == '"' || c == '\\' ? ' ' : c); break; }   /* any other byte, as it is */
        case 5: put(t, '\\'); put(t, "bfnrt\"\\/"[below(8)]); break;                        /* the escapes it knows */
        case 6: put(t, '\\'); put(t, (int)below(256)); break;                               /* any escape */
        case 7: escape(t, below(2) ? bmp[below(sizeof bmp / sizeof *bmp)] : (unsigned)below(0xd800)); break;
        case 8: escape(t, 0xd800 + (unsigned)below(0x400)); escape(t, 0xdc00 + (unsigned)below(0x400)); break;   /* a pair */
        case 9:                                                                             /* a pair broken */
            escape(t, (below(2) ? 0xd800 : 0xdc00) + (unsigned)below(0x400));
            switch (below(4)) {
            case 0: escape(t, (unsigned)below(0x10000)); break;
            case 1: puts_in(t, "\\U"); hex_digits(t, 0xdc00 + (unsigned)below(0x400), 4); break;
            case 2: puts_in(t, "\\u"); hex_digits(t, 0xdc00 + (unsigned)below(0x400), (int)below(4)); break;
            default: break;
            }
            break;
        case 10: puts_in(t, "\\u"); hex_digits(t, (unsigned)below(0x10000), (int)below(4)); put(t, "g /\"zG"[below(6)]); break;   /* cut short, or not hexadecimal */
        case 11: put(t, below(4) ? (int)below(32) : 0); break;                              /* control bytes and zero bytes, as they are */
        case 12: put(t, 128 + (int)below(128)); break;
        default: put(t, ' '); break;
        }
    }
}

static void parse_string_case(text *t)
{
    text c;
    size_t prefix = below(4) ? 0 : 1 + below(3), most = below(4) ? 2 + below(SHORT - 6) : 2 + below(24), offset, i;
    memset(&c, 0, sizeof c);
    for (i = 0; i < prefix; i++)                                     /* what lies before the offset */
        put(&c, " a\"\\"[below(4)]);
    put(&c, below(12) ? '"' : (int)below(256));                      /* (not a quote: it fails) */
    string_body(&c, prefix + 1 + below(most));
    switch (below(20)) {
    case 0: break;                                                   /* no closing quote: it fails */
    case 1: put(&c, '\\'); break;                                    /* a backslash last: it fails */
    default: put(&c, '"'); break;
    }
    if (below(4) == 0)                                               /* something after the string */
        for (i = 1 + below(4); i > 0; i--)
            put(&c, below(2) ? '"' : 'z');
    if (c.length > SHORT - 4)
        c.length = SHORT - 4;                                        /* (the leaf's bound: 256 bytes, room and offset included) */
    switch (below(12)) {
    case 0: offset = below(c.length + 1); break;                     /* anywhere (at the end: refused) */
    case 1: offset = c.length + below(3); break;                     /* at the end or beyond: refused */
    case 2: offset = 256 * (1 + below(255)) + (below(2) ? prefix : below(256)); break;   /* far beyond, the high byte set: refused */
    default: offset = prefix; break;                                 /* at the opening quote */
    }
    put_offset(t, some_room(string_size(c.bytes, c.length, offset)));
    put_offset(t, offset);
    for (i = 0; i < c.length; i++)
        put(t, c.bytes[i]);
    if (below(60) == 0)
        t->length = below(4);                                        /* no room or no offset: refused */
}

static void strdup_case(text *t)
{
    size_t n = below(4) ? below(SHORT - 1) : below(9), i;            /* (the leaf's bound: 256 bytes, the room's two included) */
    put_offset(t, some_room(n + 1));                                 /* (the size it asks: the text and its zero) */
    for (i = 0; i < n; i++)
        put(t, below(4) ? 'a' + (int)below(26) : 1 + (int)below(255));
    if (n && below(20) == 0)
        t->bytes[2 + below(n)] = 0;                                  /* a zero byte: refused */
    if (below(60) == 0)
        t->length = below(2);                                        /* no room: refused */
}

/* The tree of items (port/LEAVES.md): an item in preorder, its type byte, then what that type holds; a text, its length
   (two bytes, low byte first; FF FF: NULL), then its bytes. The generator draws a tree and, beside it, the length of the
   text the original prints for it, or -1 where the original cannot print it (a type that is none of the seven, a Raw
   without its text, arrays and objects nested CJSON_NESTING_LIMIT deep), so that the length of the buffer is drawn
   where the original's answer changes: the text, its zero, and the one byte more ensure asks for. (Only the cases are
   drawn from that length: the answers are the reference's.) */
enum { NOT_A_TEXT = 0xFFFF, NESTING = 1000 };

static void put_two(text *t, size_t value)
{
    put(t, (int)(value & 255));
    put(t, (int)(value >> 8 & 255));
}

static void set_two(text *t, size_t at, size_t value)
{
    if (at + 1 < t->length)
        t->bytes[at] = (unsigned char)(value & 255), t->bytes[at + 1] = (unsigned char)(value >> 8 & 255);
}

static double magnitude(double d) { return d < 0 ? -d : d; }

/* the length print_number writes: « null » for NaN and the infinities; the int when the double equals it; else 15
   digits when they give back a double compare_double says is equal, else 17 */
static long printed_number(double d, int valueint)
{
    char s[40];
    double test = 0;
    int length;
    if (d != d || d - d != d - d)
        return 4;
    if (d == (double)valueint)
        return snprintf(s, sizeof s, "%d", valueint);
    length = snprintf(s, sizeof s, "%1.15g", d);
    if (sscanf(s, "%lg", &test) == 1
        && magnitude(test - d) <= (magnitude(test) > magnitude(d) ? magnitude(test) : magnitude(d)) * DBL_EPSILON)
        return length;
    return snprintf(s, sizeof s, "%1.17g", d);
}

/* what print_string_ptr writes for one byte: two for the seven it escapes by a letter, six for the other controls */
static long printed_byte(int byte)
{
    return byte && strchr("\"\\\b\f\n\r\t", byte) ? 2 : byte < 32 ? 6 : 1;
}

static int chaining;                                                 /* a tree drawn as a chain: one child, nested deep */

/* a text of at most `most` bytes: plain bytes, bytes escaped by a letter, controls, high bytes; a zero byte once in a
   while (refused: a C string cannot hold one). Returns the length the original prints for it: as a string, quotes and
   escapes included; as a Raw, its bytes as they are. NULL now and then when `null_too`: printed « "" » as a string, a
   failure as a Raw (the caller's) */
static long tree_text(text *t, size_t most, int null_too, int raw)
{
    size_t at = t->length, n, i;
    long printed = raw ? 0 : 2;
    if (null_too && below(raw ? 40 : 10) == 0) {
        put_two(t, NOT_A_TEXT);
        return raw ? -1 : 2;
    }
    n = below(3) ? below(9) : below(most + 1);
    if (n > most)
        n = most;
    put_two(t, 0);
    for (i = 0; i < n; i++) {
        int byte;
        switch (below(10)) {
        case 0: byte = "\"\\\b\f\n\r\t/"[below(8)]; break;
        case 1: byte = 1 + (int)below(31); break;
        case 2: byte = 128 + (int)below(128); break;
        default: byte = plain_byte(); break;
        }
        if (below(2000) == 0)
            byte = 0;
        put(t, byte);
        printed += raw ? 1 : printed_byte(byte);
    }
    set_two(t, at, t->length - at - 2);
    return printed;
}

static long tree_item(text *t, size_t depth, int format, int named, size_t most);

/* an array's or an object's children, at most `most` bytes; `depth` is the container's (the printbuffer's depth when
   print_array or print_object is called). Returns the length the original prints for the container, or -1 */
static long tree_children(text *t, size_t depth, int format, int object, size_t most)
{
    size_t at = t->length, start, count = 0, wanted, inner = depth + 1;
    long printed = object && format ? 2 : 1, each;
    int failed = depth >= NESTING;
    wanted = chaining ? (below(8) ? 1 : 2) : below(3) ? below(4) : below(4) ? below(12) : below(16) ? below(48) : below(400);
    put_two(t, 0);
    start = t->length;
    while (count < wanted && t->length - start < most) {
        size_t left = most - (t->length - start), share = count + 1 == wanted || chaining ? left : 1 + below(left);
        if (share < (object ? 3u : 1u))
            break;
        each = tree_item(t, inner, format, object, share);
        if (each < 0)
            failed = 1;
        else if (object)
            printed += (format ? (long)inner : 0) + each + (format ? 2 : 1) + (format ? 1 : 0);
        else
            printed += each + (count ? (format ? 2 : 1) : 0);
        count++;
    }
    set_two(t, at, count);
    if (object)
        printed += (count ? (long)count - 1 : 0) + (format ? (long)inner - 1 : 0) + 1;
    else
        printed += 1;
    return failed ? -1 : printed;
}

/* one item, its name first when `named` (an object's child), at most about `most` bytes (at least one, three when named);
   returns the length the original prints for it, its name included, or -1 */
static long tree_item(text *t, size_t depth, int format, int named, size_t most)
{
    static const int OTHER[] = {0, 3, 5, 6, 12, 48, 96, 129, 255};
    size_t begin = t->length, pick = below(100), left, need;
    long name = 0, value;
    int type;
    if (chaining && most > 8)
        pick = below(2) ? 60 : 80;
    type = pick < 6 ? 1 : pick < 12 ? 2 : pick < 18 ? 4 : pick < 36 ? 8 : pick < 54 ? 16 : pick < 59 ? 128 : pick < 79 ? 32
         : pick < 99 ? 64 : below(2) ? OTHER[below(sizeof OTHER / sizeof *OTHER)] : (int)below(256);
    need = type == 8 ? 12 : type == 16 || type == 128 || type == 32 || type == 64 ? 2 : 0;
    if (1 + (named ? 2 : 0) + need > most)
        type = 1 << below(3), need = 0;                             /* no room for more: false, true or null */
    put(t, type);
    if (named) {
        size_t longest = below(4) ? 6 : below(3) ? 24 : most;          /* a name, within what the value leaves */
        name = tree_text(t, longest < most - 3 - need ? longest : most - 3 - need, 1, 0);
    }
    left = most > t->length - begin ? most - (t->length - begin) : 0;
    switch (type) {
    case 1: return name + 5;
    case 2: case 4: return name + 4;
    case 8: {
        unsigned long long bits = some_double();
        double d = double_of(bits);
        int valueint;
        switch (below(6)) {
        case 0: valueint = (int)(unsigned)next(); break;             /* any int */
        case 1: valueint = 0; break;
        default:                                                     /* the one cJSON_SetNumberHelper sets beside the double */
            valueint = d != d ? 0 : d >= 2147483647.0 ? 2147483647 : d <= -2147483648.0 ? -2147483647 - 1 : (int)d;
            break;
        }
        put_bits(t, bits);
        put_int(t, (unsigned)valueint);
        return name + printed_number(d, valueint);
    }
    case 16: case 128:
        value = left >= 2 ? tree_text(t, left - 2, 1, type == 128) : (put_two(t, NOT_A_TEXT), type == 128 ? -1 : 2);
        return value < 0 ? -1 : name + value;
    case 32: case 64:
        if (left < 2) {
            put_two(t, 0);
            return -1;                                               /* (never: the type was chosen for the room) */
        }
        value = tree_children(t, depth, format, type == 64, left - 2);
        return value < 0 ? -1 : name + value;
    default:
        return -1;
    }
}

static void print_value_case(text *t)
{
    static text tree;
    size_t most, n, cap, room, least;
    long printed;
    int format = (int)below(2);
    tree.length = 0;
    chaining = below(12) == 0;
    most = below(4) ? 1 + below(96) : below(4) ? 1 + below(600) : 1 + below(LONGEST - 3);   /* (the leaf's bound: 3072 bytes, the length and the format included) */
    printed = tree_item(&tree, 0, format, 0, most);
    chaining = 0;
    if (format && below(8) == 0)
        format = 2 + (int)below(254);                                /* any byte but 0: formatted too */
    n = 3 + tree.length, cap = 6 * n + 16;
    if (printed < 0)
        room = below(3) ? cap : below(cap + 1);
    else {
        least = (size_t)printed + 2;                                 /* the text, its zero, and the byte more ensure asks for */
        switch (below(12)) {
        case 0: case 1: case 2: room = least; break;
        case 3: case 4: room = least - 1; break;
        case 5: room = least + 1 + below(2); break;
        case 6: room = below(least); break;
        case 7: room = 0; break;
        case 8: room = cap; break;
        case 9: room = cap + 1 + below(16); break;                   /* more than the answer's buffer: refused */
        default: room = least + below(64); break;
        }
    }
    put_two(t, room > 65535 ? 65535 : room);
    put(t, format);
    for (n = 0; n < tree.length; n++)
        put(t, tree.bytes[n]);
    switch (below(40)) {
    case 0: t->length = 3 + below(tree.length); break;               /* the item cut short: refused */
    case 1: put(t, (int)below(256)); break;                          /* a byte after it: refused */
    case 2: if (tree.length) t->bytes[3 + below(tree.length)] = (unsigned char)below(256); break;   /* a byte changed: anything */
    default: break;
    }
}

/* a fixed tree, written short: n f t (null, false, true), i (the number 7, its int 7), d (0.1, its int 0), s ("ab"), S (a
   String whose valuestring is NULL), E (""), c ("\x01\t\"/": a byte escaped by six, two by a letter, one as it is), r (the
   Raw « [1] »), z (the Raw ""), R (a Raw whose valuestring is NULL), x and y (the types 0 and 3: none), [ … ] an array,
   { … } an object, each of its children a name then an item; the names: k ("k"), K (NULL), e (""), q ("a\"b"). Puts its
   form; returns the length the original prints for it, or -1 */
static long fixed_tree(text *t, const char **p, size_t depth, int format, int named)
{
    static const char *types = "nftidsSEcrzRxy[{", *bytes = "\004\001\002\010\010\020\020\020\020\200\200\200\000\003\040\100";
    const char *texts[] = {"ab", "", "\001\t\"/", "[1]", "", "k", "", "a\"b"}, *order = "sEcrz";
    char name = named ? *(*p)++ : 0, kind = *(*p)++;
    long printed = 0, each;
    size_t at, count = 0, inner = depth + 1, i;
    int failed = depth >= NESTING;
    put(t, (unsigned char)bytes[strchr(types, kind) - types]);
    if (name) {
        const char *s = name == 'k' ? texts[5] : name == 'e' ? texts[6] : texts[7];
        if (name == 'K')
            put_two(t, NOT_A_TEXT), printed = 2;
        else {
            put_two(t, strlen(s));
            for (printed = 2, i = 0; s[i]; i++)
                put(t, s[i]), printed += printed_byte(s[i]);
        }
    }
    switch (kind) {
    case 'n': case 't': return printed + 4;
    case 'f': return printed + 5;
    case 'i': put_bits(t, bits_of(7)); put_int(t, 7); return printed + 1;
    case 'd': put_bits(t, bits_of(0.1)); put_int(t, 0); return printed + 3;
    case 'S': put_two(t, NOT_A_TEXT); return printed + 2;
    case 'R': put_two(t, NOT_A_TEXT); return -1;
    case 'x': case 'y': return -1;
    case 's': case 'E': case 'c': case 'r': case 'z': {
        const char *s = texts[strchr(order, kind) - order];
        put_two(t, strlen(s));
        printed += kind == 'r' || kind == 'z' ? 0 : 2;
        for (i = 0; s[i]; i++)
            put(t, s[i]), printed += kind == 'r' || kind == 'z' ? 1 : printed_byte(s[i]);
        return printed;
    }
    default:                                                         /* [ or { */
        at = t->length;
        put_two(t, 0);
        printed += kind == '{' && format ? 2 : 1;
        while (**p != ']' && **p != '}') {
            each = fixed_tree(t, p, inner, format, kind == '{');
            if (each < 0)
                failed = 1;
            else if (kind == '{')
                printed += (format ? (long)inner : 0) + each + (format ? 2 : 1) + (format ? 1 : 0);
            else
                printed += each + (count ? (format ? 2 : 1) : 0);
            count++;
        }
        (*p)++;
        set_two(t, at, count);
        printed += kind == '{' ? (count ? (long)count - 1 : 0) + (format ? (long)inner - 1 : 0) + 1 : 1;
        return failed ? -1 : printed;
    }
}

/* a case of print_value: its length (`delta` from the least the original takes, or `fixed` when not 0), its format, the
   tree described; the least is the text, its zero, and the byte more ensure asks for */
static void put_fixed_tree(text *t, const char *description, int format, long delta, size_t fixed)
{
    static text tree;
    const char *p = description;
    long printed;
    size_t cap, n;
    tree.length = 0;
    printed = fixed_tree(&tree, &p, 0, format != 0, 0);
    cap = 6 * (3 + tree.length) + 16;
    if (!fixed)
        fixed = printed < 0 ? cap : (size_t)(printed + 2 + delta);
    put_two(t, fixed);
    put(t, format);
    for (n = 0; n < tree.length; n++)
        put(t, tree.bytes[n]);
}

/* The parser's cases: JSON texts drawn as trees, with whitespace between their tokens (any byte up to 32:
   buffer_skip_whitespace skips zero bytes too), numbers, strings with every escape, near misses of the literals, cut
   short or spoiled now and then. Beside each text, the count of allocations the original asks for to parse it whole
   (an item for each child of an array or an object, a block for each name and each string, a copy for each number),
   so that the room is drawn around it. */
static void json_space(text *t)
{
    size_t n = below(3) ? 0 : below(4) ? 1 : below(6);
    while (n--)
        put(t, below(4) ? " \t\n\r"[below(4)] : (int)below(33));
}

static void json_number(text *t)
{
    if (below(8) == 0) {
        parse_number_case(t);                                        /* the hard numbers, and what may follow them */
        return;
    }
    if (below(3) == 0)
        put(t, '-');
    if (below(4) == 0)
        put(t, '0');
    else
        put(t, '1' + (int)below(9)), digits(t, below(5) ? below(4) : below(20));
    if (below(3) == 0)
        put(t, '.'), digits(t, 1 + below(below(4) ? 3 : 20));
    if (below(5) == 0) {
        put(t, below(2) ? 'e' : 'E');
        if (below(2))
            put(t, below(2) ? '+' : '-');
        digits(t, 1 + below(3));
    }
}

static void parser_string(text *t, size_t most)                        /* a string, its quotes included */
{
    size_t end = t->length + 2 + (below(3) ? below(8) : below(most + 1));
    put(t, '"');
    while (t->length + 1 < end && t->length < LONGEST)
        switch (below(12)) {
        case 0: put(t, '\\'); put(t, "bfnrt\"\\/"[below(8)]); break;
        case 1: escape(t, (unsigned)below(0xd800)); break;
        case 2: escape(t, 0xd800 + (unsigned)below(0x400)); escape(t, 0xdc00 + (unsigned)below(0x400)); break;
        case 3: if (below(12) == 0) string_body(t, t->length + 1 + below(6)); else put(t, 128 + (int)below(128)); break;
        default: put(t, plain_byte()); break;
        }
    if (below(40))
        put(t, '"');                                                 /* (else: left open) */
}

static void json_value(text *t, size_t depth, size_t most, size_t *allocations)
{
    static const char *near[] = {"nul", "fals", "tru", "NULL", "True", "nulL", "falsE", "t", "n", "f", "+1", ".5", "x", ""};
    size_t start = t->length, pick = below(100), wanted, k;
    int object;
    if (chaining && most > 6)
        pick = below(2) ? 70 : 90;
    if (most < 8 && pick >= 64)
        pick = below(64);
    if (pick < 6) { puts_in(t, "null"); return; }
    if (pick < 12) { puts_in(t, "false"); return; }
    if (pick < 18) { puts_in(t, "true"); return; }
    if (pick < 20) { puts_in(t, near[below(sizeof near / sizeof *near)]); return; }
    if (pick < 38) { json_number(t); ++*allocations; return; }
    if (pick < 64) { parser_string(t, most > 2 ? most - 2 : 0); ++*allocations; return; }
    object = pick >= 82;
    wanted = chaining ? 1 : below(3) ? below(4) : below(4) ? below(12) : below(40);
    put(t, object ? '{' : '[');
    json_space(t);
    for (k = 0; k < wanted && t->length - start + 4 < most && t->length < LONGEST; k++) {
        size_t left = most - (t->length - start) - 2, share = chaining || k + 1 == wanted ? left : 1 + below(left);
        if (k)
            put(t, ','), json_space(t);
        ++*allocations;                                              /* the item */
        if (object) {
            parser_string(t, share / 4 + 1);
            ++*allocations;                                          /* its name */
            json_space(t), put(t, ':'), json_space(t);
        }
        json_value(t, depth + 1, share, allocations);
        json_space(t);
    }
    put(t, object ? '}' : ']');
}

static void parse_value_case(text *t)
{
    static text content;
    size_t most, allocations = 0, prefix = 0, room, offset, i;
    content.length = 0;
    if (below(6) == 0) {                                             /* bytes before the value, the offset past them */
        for (i = 1 + below(4); i--; )
            put(&content, below(2) ? ' ' : (int)below(256));
        prefix = content.length;
    }
    chaining = below(12) == 0;
    most = below(4) ? 1 + below(96) : below(4) ? 1 + below(600) : 1 + below(LONGEST - 8);   /* (the leaf's bound: 3072 bytes, the room and the offset included) */
    json_value(&content, 0, most, &allocations);
    chaining = 0;
    if (below(4) == 0)
        json_space(&content);
    if (below(8) == 0)
        put(&content, below(2) ? "],}:x"[below(5)] : (int)below(256));   /* what follows the value: not read */
    if (below(6) == 0)
        spoil(&content, LONGEST - 4);
    switch (below(12)) {
    case 0: case 1: case 2: room = allocations; break;
    case 3: case 4: room = allocations ? allocations - 1 : 0; break;
    case 5: room = allocations + 1 + below(2); break;
    case 6: case 7: room = below(allocations + 1); break;
    case 8: room = 0; break;
    default: room = 65535; break;
    }
    offset = below(10) ? prefix : some_offset(content.length);
    put_offset(t, room);
    put_offset(t, offset);
    for (i = 0; i < content.length; i++)
        put(t, content.bytes[i]);
}

/* compare_double's fixed pairs, a then b, as patterns. The original says equal when |a - b| <= max(|a|, |b|)·DBL_EPSILON:
   around 1 and 2, the pairs a unit in the last place inside that bound, outside it, and on it (<=, not <); the
   infinities (an infinity is equal to every finite double and to the other infinity, not to itself: inf - inf is NaN);
   NaN, equal to nothing; the subnormals (below 2^-1023 the bound is 0: only the same one is equal; at 2^-1023 it is
   2^-1075, rounded to the even 0; above it, up to the least normal, it rounds to the least subnormal: one unit apart is
   equal, two are not), in both orders and of both signs; a difference that overflows */
static const unsigned long long FIXED_COMPARE[][2] = {
    {0, 0}, {0, SIGN}, {ONE, ONE}, {ONE, ONE + 1}, {ONE, ONE + 2}, {ONE, ONE - 1}, {ONE, ONE - 2}, {ONE, ONE - 3},
    {TWO, TWO + 1}, {TWO, TWO + 2}, {TWO - 1, TWO}, {TWO - 2, TWO}, {TWO - 3, TWO}, {ONE | SIGN, (ONE - 2) | SIGN},
    {INFINITE, INFINITE}, {INFINITE | SIGN, INFINITE | SIGN}, {INFINITE, INFINITE | SIGN}, {INFINITE | SIGN, INFINITE},
    {INFINITE, ONE}, {ONE, INFINITE | SIGN}, {INFINITE, GREATEST}, {INFINITE, 0}, {QUIET_NAN, QUIET_NAN}, {QUIET_NAN, ONE},
    {0, QUIET_NAN}, {QUIET_NAN, INFINITE}, {INFINITE + 1, INFINITE + 1}, {1, 0}, {1, 1}, {1, 2}, {1, SIGN | 1}, {3, 2},
    {0x000FFFFFFFFFFFFFULL, 0x000FFFFFFFFFFFFEULL}, {0x000FFFFFFFFFFFFEULL, 0x000FFFFFFFFFFFFFULL},
    {SIGN | 0x000FFFFFFFFFFFFEULL, SIGN | 0x000FFFFFFFFFFFFFULL}, {0x000FFFFFFFFFFFFDULL, 0x000FFFFFFFFFFFFFULL},
    {0x0008000000000001ULL, 0x0008000000000000ULL}, {0x0008000000000000ULL, 0x0008000000000001ULL},
    {0x0008000000000002ULL, 0x0008000000000000ULL}, {0x0008000000000000ULL, 0x0007FFFFFFFFFFFFULL},
    {SIGN | 0x0007FFFFFFFFFFFFULL, SIGN | 0x0008000000000000ULL}, {0x0007FFFFFFFFFFFFULL, 0x0007FFFFFFFFFFFEULL},
    {GREATEST, GREATEST | SIGN}, {GREATEST, GREATEST - 1}, {0x0010000000000000ULL, 0x000FFFFFFFFFFFFFULL}};

/* print_number's fixed cases beside the edges: a double and the valueint beside it. The original writes valueint alone
   when the double equals it (so -0 with 0 is « 0 », and -0 with 1 is « -0 »); else 15 digits when they give back a
   double its comparison says is equal (the greatest double: 15 digits give back infinity, equal to it), else 17 */
static const struct { double value; int valueint; } FIXED_PRINT_NUMBER[] = {
    {0, 0}, {-0.0, 0}, {-0.0, 1}, {0, 1}, {1, 1}, {1, 0}, {-1, -1}, {1.5, 1}, {-1.5, -1}, {100, 100}, {100, 0}, {123456, 123456},
    {2147483647.0, 2147483647}, {2147483647.0, 0}, {2147483648.0, 2147483647}, {-2147483648.0, -2147483647 - 1},
    {-2147483649.0, -2147483647 - 1}, {1e15, 2147483647}, {1e16, 2147483647}, {1.7976931348623157e308, 2147483647},
    {-1.7976931348623157e308, -2147483647 - 1}, {9007199254740993.0, 2147483647}, {0.1 + 0.2, 0}, {1.0 / 3, 0}, {0x1p-25, 0}};

/* The fixed cases a list of C strings cannot hold: the longest input each leaf is given (its unit's « case_bytes »
   in the manifest), with what the leaf must work on to the end; inputs with a zero byte; for the comparison, texts
   that differ only at their last byte. Returns 0 when the leaf has no case of that number. */
static int special(text *t, const char *leaf, size_t number)
{
    size_t i;
    if (!strcmp(leaf, "hex4")) {
        if (number == 0) { for (i = 0; i < 16; i++) put(t, "0123456789abcdef"[i]); return 1; }
        if (number == 1) { put(t, '1'); put(t, 0); put(t, '2'); put(t, '3'); return 1; }      /* a zero byte is not a digit */
    } else if (!strcmp(leaf, "utf16")) {
        if (number == 0) { puts_in(t, "\\udbff\\udfff"); for (i = 0; i < 20; i++) put(t, 'z'); return 1; }
        if (number == 1) { puts_in(t, "\\u00"); put(t, 0); put(t, '1'); return 1; }
    } else if (!strcmp(leaf, "minify")) {
        if (number == 0) {
            for (i = 0; t->length < 256; i++) puts_in(t, i % 3 == 0 ? "{ \"k\" : " : i % 3 == 1 ? "/* c */ 1 , " : "// l\n\t[ ] } ");
            return 1;
        }
        if (number == 1) { put(t, 'a'); put(t, 0); put(t, 'b'); return 1; }                    /* a zero byte: refused */
        if (number == 2) { put(t, 0); return 1; }
        if (number == 3) { for (i = 0; i < 256; i++) put(t, '0' + (int)(i % 10)); return 1; }   /* kept whole: the longest answer */
        if (number == 4) { put(t, '"'); for (i = 1; i < 256; i++) put(t, i % 2 ? ' ' : '/'); return 1; }   /* an open string, kept whole */
        if (number < 5 + 257) { for (i = 0; i < number - 5; i++) put(t, kept()); return 1; }   /* every length up to the bound, kept whole */
    } else if (!strcmp(leaf, "print_string")) {
        if (number == 0) {
            for (i = 0; i < 256; i++) put(t, i % 4 == 0 ? 1 + (int)(i % 31) : i % 4 == 1 ? '"' : i % 4 == 2 ? '\\' : 'a' + (int)(i % 26));
            return 1;
        }
        if (number == 1) { put(t, 'a'); put(t, 0); put(t, 'b'); return 1; }                    /* a zero byte: refused */
        if (number == 2) { put(t, 0); return 1; }
        if (number == 3) { for (i = 0; i < 256; i++) put(t, 1); return 1; }                    /* the longest answer there is */
        if (number == 4) { for (i = 0; i < 256; i++) put(t, 'a' + (int)(i % 26)); return 1; }  /* the longest text with nothing to escape */
        if (number == 5) { for (i = 0; i < 255; i++) put(t, 'a' + (int)(i % 26)); return 1; }
        /* every length up to the bound, twice: with nothing to escape, then with at least one byte to escape */
        if (number < 6 + 257) { for (i = 0; i < number - 6; i++) put(t, plain_byte()); return 1; }
        if (number < 6 + 2 * 257) {
            size_t n = number - 6 - 257, at = n ? below(n) : 0;
            for (i = 0; i < n; i++)
                put(t, i == at || below(6) == 0 ? "\001\037\"\\\b\f\n\r\t\013"[below(10)] : plain_byte());
            return 1;
        }
    } else if (!strcmp(leaf, "parse_number")) {
        /* the longest input (256 bytes): digits to the end; a one, 250 zeros and the exponent that brings it back to 1;
           a point and 254 digits; 254 nines and an exponent cut short; 250 zeros after the point and a small exponent;
           then a zero byte, which ends the number as any byte the original does not copy */
        if (number == 0) { for (i = 0; i < 256; i++) put(t, '1' + (int)(i % 9)); return 1; }
        if (number == 1) { put(t, '1'); for (i = 0; i < 250; i++) put(t, '0'); puts_in(t, "e-250"); return 1; }
        if (number == 2) { puts_in(t, "0."); for (i = 0; i < 254; i++) put(t, '0' + (int)(i * 7 % 10)); return 1; }
        if (number == 3) { for (i = 0; i < 254; i++) put(t, '9'); puts_in(t, "e+"); return 1; }
        if (number == 4) { puts_in(t, "0."); for (i = 0; i < 250; i++) put(t, '0'); puts_in(t, "1e-9"); return 1; }
        if (number == 5) { put(t, '1'); put(t, 0); put(t, '2'); return 1; }
        if (number == 6) { puts_in(t, "1e"); put(t, 0); put(t, '5'); return 1; }
        if (number == 7) { put(t, 0); put(t, '1'); return 1; }                                 /* nothing before it: refused */
        if (number == 8) { puts_in(t, "-."); put(t, 0); return 1; }
    } else if (!strcmp(leaf, "compare_double")) {
        /* the pairs above; then lengths that are not sixteen (the longest the manifest allows too): refused */
        static const size_t lengths[] = {0, 8, 15, 17, 32};
        size_t pairs = sizeof FIXED_COMPARE / sizeof *FIXED_COMPARE;
        if (number < pairs) { put_bits(t, FIXED_COMPARE[number][0]); put_bits(t, FIXED_COMPARE[number][1]); return 1; }
        if (number - pairs < sizeof lengths / sizeof *lengths) { for (i = 0; i < lengths[number - pairs]; i++) put(t, 0x3f); return 1; }
    } else if (!strcmp(leaf, "print_number")) {
        /* the pairs above; every edge pattern, with 0 then INT_MAX beside it; every edge value, with 0; then lengths that
           are not twelve (the longest the manifest allows too): refused */
        static const size_t lengths[] = {0, 8, 11, 13, 16};
        size_t pairs = sizeof FIXED_PRINT_NUMBER / sizeof *FIXED_PRINT_NUMBER, edges = sizeof EDGE_BITS / sizeof *EDGE_BITS;
        size_t values = sizeof EDGE_VALUES / sizeof *EDGE_VALUES;
        if (number < pairs) { put_bits(t, bits_of(FIXED_PRINT_NUMBER[number].value)); put_int(t, (unsigned)FIXED_PRINT_NUMBER[number].valueint); return 1; }
        number -= pairs;
        if (number < 2 * edges) { put_bits(t, EDGE_BITS[number / 2]); put_int(t, number % 2 ? 2147483647u : 0u); return 1; }
        number -= 2 * edges;
        if (number < values) { put_bits(t, bits_of(EDGE_VALUES[number])); put_int(t, 0); return 1; }
        number -= values;
        if (number < sizeof lengths / sizeof *lengths) { for (i = 0; i < lengths[number]; i++) put(t, 0x3f); return 1; }
    } else if (!strcmp(leaf, "buffer_skip_whitespace")) {
        /* the longest input, skipped to its end (the original steps back one), from the start and from its last byte,
           and skipped to its last byte; every byte value at the offset, a byte kept after it; offsets at the end, beyond
           it, far beyond (the high byte alone too); the empty content; no offset: refused */
        static const size_t offsets[] = {2, 3, 255, 256, 65535};
        if (number == 0) { put_offset(t, 0); for (i = 0; i < 254; i++) put(t, " \t\n\r"[i % 4]); return 1; }
        if (number == 1) { put_offset(t, 0); for (i = 0; i < 253; i++) put(t, (int)(i % 33)); put(t, '!'); return 1; }
        if (number == 2) { put_offset(t, 253); for (i = 0; i < 254; i++) put(t, ' '); return 1; }
        if (number == 3) { put_offset(t, 256); for (i = 0; i < 254; i++) put(t, ' '); return 1; }
        if (number < 4 + 256) { put_offset(t, 0); put(t, (int)(number - 4)); put(t, 'x'); return 1; }
        number -= 4 + 256;
        if (number < 5) { put_offset(t, offsets[number]); puts_in(t, "  "); return 1; }
        number -= 5;
        if (number == 0) { put_offset(t, 0); return 1; }
        if (number == 1) { put_offset(t, 1); return 1; }
        if (number == 2) { put_offset(t, 0); put(t, ' '); return 1; }
        if (number == 3) { put_offset(t, 0); put(t, 'a'); return 1; }
        if (number == 4) { put_offset(t, 1); puts_in(t, " \t"); return 1; }
        if (number == 5) { put_offset(t, 1); puts_in(t, "a b"); return 1; }
        if (number == 6) return 1;
        if (number == 7) { put(t, ' '); return 1; }
    } else if (!strcmp(leaf, "skip_utf8_bom")) {
        /* the mark followed by 0 to 11 bytes (the original skips it only when two bytes at least follow it), the longest
           input last; the mark with one byte off by one, at each place, either way; the mark and an offset that is not 0
           (refused: the original fails), the high byte alone too; the mark elsewhere, turned round, after a zero byte;
           the empty content; no offset: refused */
        static const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
        static const size_t offsets[] = {1, 2, 3, 255, 256, 0x8000, 65535};
        if (number < 12) { put_offset(t, 0); for (i = 0; i < 3; i++) put(t, bom[i]); for (i = 0; i < number; i++) put(t, 'a' + (int)i); return 1; }
        number -= 12;
        if (number < 6) {
            put_offset(t, 0);
            for (i = 0; i < 3; i++) put(t, bom[i] + (i == number / 2 ? (number % 2 ? 1 : -1) : 0));
            puts_in(t, "ab");
            return 1;
        }
        number -= 6;
        if (number < 7) { put_offset(t, offsets[number]); for (i = 0; i < 3; i++) put(t, bom[i]); puts_in(t, "abc"); return 1; }
        number -= 7;
        if (number == 0) { put_offset(t, 0); return 1; }
        if (number == 1) { put_offset(t, 0); puts_in(t, "ab"); for (i = 0; i < 3; i++) put(t, bom[i]); return 1; }
        if (number == 2) { put_offset(t, 0); for (i = 0; i < 3; i++) put(t, bom[(i + 1) % 3]); puts_in(t, "ab"); return 1; }
        if (number == 3) { put_offset(t, 0); put(t, 0); for (i = 1; i < 3; i++) put(t, bom[i]); puts_in(t, "ab"); return 1; }
        if (number == 4) { put_offset(t, 1); return 1; }
        if (number == 5) return 1;
        if (number == 6) { put(t, 0); return 1; }
        number -= 7;
        /* the mark and zero bytes after it (it is skipped); every other mark, followed by enough to be skipped (it is
           not; but UTF-8's twice is skipped once) */
        if (number < 3) { put_offset(t, 0); for (i = 0; i < 3; i++) put(t, bom[i]); put(t, number == 2 ? 'a' : 0); put(t, number == 1 ? 'a' : 0); return 1; }
        number -= 3;
        if (number < sizeof OTHER_MARKS / sizeof *OTHER_MARKS) {
            put_offset(t, 0);
            for (i = 0; i < OTHER_MARKS[number][0]; i++) put(t, OTHER_MARKS[number][1 + i]);
            puts_in(t, "abcde");
            return 1;
        }
    } else if (!strcmp(leaf, "update_offset")) {
        /* the longest input, its only zero byte last, and without one (refused); a zero byte at the offset (nothing to
           add), the last byte too; zero bytes before and after the offset, or only before it (refused); an offset at the
           end, beyond it, far beyond (the high byte alone too): refused; bytes 128 to 255 are not zero; the empty content
           (refused); no offset: refused */
        if (number == 0) { put_offset(t, 0); for (i = 0; i < 253; i++) put(t, 'a' + (int)(i % 26)); put(t, 0); return 1; }
        if (number == 1) { put_offset(t, 0); for (i = 0; i < 254; i++) put(t, 'a'); return 1; }
        if (number == 2) { put_offset(t, 253); for (i = 0; i < 254; i++) put(t, 0); return 1; }
        if (number == 3) { put_offset(t, 256); for (i = 0; i < 254; i++) put(t, 0); return 1; }
        if (number == 4) { put_offset(t, 65535); for (i = 0; i < 254; i++) put(t, 0); return 1; }
        if (number == 5) { put_offset(t, 0); put(t, 0); return 1; }
        if (number == 6) { put_offset(t, 1); put(t, 'a'); put(t, 0); return 1; }
        if (number == 7) { put_offset(t, 2); put(t, 'a'); put(t, 0); return 1; }
        if (number == 8) { put_offset(t, 1); put(t, 0); put(t, 'b'); put(t, 0); return 1; }
        if (number == 9) { put_offset(t, 0); puts_in(t, "ab"); put(t, 0); puts_in(t, "cd"); put(t, 0); return 1; }
        if (number == 10) { put_offset(t, 3); puts_in(t, "ab"); put(t, 0); puts_in(t, "cd"); put(t, 0); return 1; }
        if (number == 11) { put_offset(t, 3); puts_in(t, "ab"); put(t, 0); puts_in(t, "cd"); return 1; }
        if (number == 12) { put_offset(t, 0); put(t, 0x80); put(t, 0xFF); put(t, 0); return 1; }
        if (number == 13) { put_offset(t, 0); return 1; }
        if (number == 14) return 1;
        if (number == 15) { put(t, 0); return 1; }
    } else if (!strcmp(leaf, "parse_string")) {
        /* strings with every escape it knows, \u of every length of UTF-8 and their bounds, pairs whole and broken, a zero
           written, digits that are not hexadecimal, cut short, unclosed, a backslash last, not a string; every byte value
           between the quotes, and after a backslash; rooms at the size asked, one short, none, the high byte alone; offsets
           after a prefix, on a byte that is not a quote, at the end and beyond (refused); the longest inputs; too short */
        static const char *strings[] = {
            "\"\"", "\"a\"", "\"abc\" rest", "\"\\\"\"", "\"\\\\\"", "\"\\/\"", "\"\\b\\f\\n\\r\\t\"", "\"a\\\"b\" c\"",
            "\"\\u0041\"", "\"\\u007F\"", "\"\\u0080\"", "\"\\u00e9\"", "\"\\u07FF\"", "\"\\u0800\"", "\"\\u20ac\"", "\"\\uD7FF\"",
            "\"\\uE000\"", "\"\\uFFFF\"", "\"\\ud83d\\ude00\"", "\"\\uD800\\uDC00\"", "\"\\uDBFF\\uDFFF\"", "\"\\u0000\"",
            "\"a\\u0000b\"", "\"\\uzzzz\"", "\"\\u00G1\"", "\"\\ud800\"", "\"\\udc00\"", "\"\\udfff\"", "\"\\ud800\\u0041\"",
            "\"\\ud800\\udbff\"", "\"\\ud800\\ue000\"", "\"\\ud800x\\udc00\"", "\"\\ud800\\Udc00\"", "\"\\ud800\\udc0\"", "\"\\u12\"",
            "\"\\u\"", "\"\\x\"", "\"\\U0041\"", "\"\\'\"", "\"abc", "\"abc\\", "\"abc\\\"", "abc\"", "\"", "\"\\", " \"a\"",
            "\"\\u0041\\", "\"\\\\\\\"\"", "\"\\\\\"\"", "\"caf\xc3\xa9\"", "\"\x01\x1f\x7f\x80\xff\""};
        static const size_t rooms[] = {0, 1, 2, 3, 4, 5, 6, 256};
        size_t count = sizeof strings / sizeof *strings, at;
        if (number < count) { put_offset(t, 65535); put_offset(t, 0); puts_in(t, strings[number]); return 1; }
        number -= count;
        if (number < 256) { put_offset(t, 65535); put_offset(t, 0); put(t, '"'); put(t, (int)number); put(t, '"'); return 1; }
        number -= 256;
        if (number < 256) { put_offset(t, 65535); put_offset(t, 0); put(t, '"'); put(t, '\\'); put(t, (int)number); put(t, '"'); return 1; }
        number -= 256;
        /* "\u20ac" asks for 7 bytes and writes 3 and its zero: every room up to it; then "ab" (it asks for 4) */
        if (number < 9) { put_offset(t, number); put_offset(t, 0); puts_in(t, "\"\\u20ac\""); return 1; }
        number -= 9;
        if (number < sizeof rooms / sizeof *rooms) { put_offset(t, rooms[number]); put_offset(t, 0); puts_in(t, "\"ab\""); return 1; }
        number -= sizeof rooms / sizeof *rooms;
        if (number == 0) { put_offset(t, 65535); put_offset(t, 3); puts_in(t, "ab \"x\""); return 1; }
        if (number == 1) { put_offset(t, 65535); put_offset(t, 2); puts_in(t, "ab \"x\""); return 1; }
        if (number == 2) { put_offset(t, 65535); put_offset(t, 6); puts_in(t, "ab \"x\""); return 1; }   /* at the end: refused */
        if (number == 3) { put_offset(t, 65535); put_offset(t, 65535); puts_in(t, "\"x\""); return 1; }
        if (number == 4) { put_offset(t, 65535); put_offset(t, 0); return 1; }                       /* no content: refused */
        if (number == 5) { put_offset(t, 65535); put_offset(t, 0x0100); for (i = 0; i < 252; i++) put(t, '"'); return 1; }
        if (number == 6) { put_offset(t, 65535); put(t, 0); return 1; }
        if (number == 7) return 1;
        if (number == 8) { put_offset(t, 65535); put_offset(t, 0); put(t, '"'); for (i = 0; i < 250; i++) put(t, 'a' + (int)(i % 26)); put(t, '"'); return 1; }
        if (number == 9) { put_offset(t, 65535); put_offset(t, 0); put(t, '"'); for (i = 0; i < 125; i++) puts_in(t, "\\n"); put(t, '"'); return 1; }
        if (number == 10) { put_offset(t, 65535); put_offset(t, 0); put(t, '"'); for (i = 0; i < 41; i++) puts_in(t, "\\u20AC"); puts_in(t, "abcd\""); return 1; }
        if (number == 11) { put_offset(t, 65535); put_offset(t, 0); put(t, '"'); for (i = 0; i < 20; i++) puts_in(t, "\\uD83D\\uDE00"); puts_in(t, "abcdefghi\""); return 1; }
        if (number == 12) { put_offset(t, 65535); put_offset(t, 0); put(t, '"'); for (i = 0; i < 251; i++) put(t, 'z'); return 1; }   /* unclosed */
        if (number == 13) { put_offset(t, 252); put_offset(t, 0); put(t, '"'); for (i = 0; i < 250; i++) put(t, 'q'); put(t, '"'); return 1; }   /* it asks for 252 */
        if (number == 14) { put_offset(t, 251); put_offset(t, 0); put(t, '"'); for (i = 0; i < 250; i++) put(t, 'q'); put(t, '"'); return 1; }
        at = number - 15;
        if (at < 4) {                                                /* a failure after its allocation: the block is given back */
            static const char *late[] = {"\"abc\\x\"", "\"abc\\ud800\"", "\"abc\\u12\"", "\"abc\\udc00 \""};
            put_offset(t, 65535); put_offset(t, 0); puts_in(t, late[at]); return 1;
        }
    } else if (!strcmp(leaf, "strdup")) {
        /* the empty text, with room for its zero and without; the longest text, with the room it needs, one short, the high
           byte alone; a zero byte in the text (refused); too short (refused); every byte value but zero */
        if (number == 0) { put_offset(t, 1); return 1; }
        if (number == 1) { put_offset(t, 0); return 1; }
        if (number < 2 + 4) {
            static const size_t rooms[] = {65535, 255, 254, 256};
            put_offset(t, rooms[number - 2]); for (i = 0; i < 254; i++) put(t, 'a' + (int)(i % 26)); return 1;
        }
        if (number == 6) { put_offset(t, 65535); puts_in(t, "ab"); put(t, 0); puts_in(t, "cd"); return 1; }
        if (number == 7) { put_offset(t, 65535); put(t, 0); return 1; }
        if (number == 8) return 1;
        if (number == 9) { put(t, 5); return 1; }
        if (number == 10) { put_offset(t, 4); puts_in(t, "abc"); return 1; }
        if (number == 11) { put_offset(t, 3); puts_in(t, "abc"); return 1; }
        if (number == 12) { put_offset(t, 65535); for (i = 1; i < 255; i++) put(t, (int)i); return 1; }
        if (number == 13) { put_offset(t, 65535); put(t, 255); return 1; }
    } else if (!strcmp(leaf, "print_value")) {
        /* every type byte, with what that type holds (the seven; the others, none: the original fails), both formats; small
           trees of every kind of item, both formats, the buffer's length from two below the least the original takes to one
           above; format bytes other than 0 and 1; the nesting limit, at it and past it, by arrays and by an object; forms
           refused; the longest inputs and the longest answer; numbers at their edges, alone */
        static const char *trees[] = {
            "n", "f", "t", "i", "d", "s", "S", "E", "c", "r", "z", "R", "x", "y", "[]", "{}", "[n]", "[nft]", "[is]", "{kn}",
            "{knKt}", "{Ki}", "{e[]}", "{qc}", "{k{k{k[i]}}}", "[{kn}{Kf}]", "[z]", "[zi]", "[iz]", "{kz}", "[[[]]]", "{k{}}",
            "[{}]", "{k[{kS}]}", "[[n][]]", "{k[]K{}}", "[rRn]", "[nx]", "{knKy}", "[[[[R]]]]"};
        static const struct { size_t arrays; const char *inner; int format; long delta; } deep[] = {
            {1000, "n", 0, 0}, {1000, "n", 0, -1}, {1000, "n", 1, 0}, {1001, "n", 0, 0}, {999, "{kn}", 0, 0}, {999, "{kn}", 1, 0},
            {999, "{k[]}", 0, 0}, {1000, "{}", 0, 0}, {999, "{}", 1, 0}, {1001, "", 0, 0}, {1000, "", 0, 0}};
        static char nested[2 * 1001 + 8];
        size_t k = number, n;
        if (k < 512) {
            int type = (int)(k / 2);
            put_two(t, 0), put(t, (int)(k % 2)), put(t, type);
            switch (type) {
            case 8: put_bits(t, bits_of(1.5)); put_int(t, 1); break;
            case 16: case 128: put_two(t, 2); puts_in(t, "a\n"); break;
            case 32: put_two(t, 2); put(t, 4); put(t, 1); break;
            case 64: put_two(t, 2); put(t, 4); put_two(t, 1); put(t, 'k'); put(t, 2); put_two(t, NOT_A_TEXT); break;
            default: break;
            }
            set_two(t, 0, 6 * t->length + 16);
            return 1;
        }
        k -= 512;
        if (k < 8 * (sizeof trees / sizeof *trees)) {
            put_fixed_tree(t, trees[k / 8], (int)(k / 4 % 2), (long)(k % 4) - 2, 0);
            return 1;
        }
        k -= 8 * (sizeof trees / sizeof *trees);
        if (k < 3) {
            static const int formats[] = {2, 255, 128};
            put_fixed_tree(t, k < 2 ? "{k[n]}" : "[{kn}]", formats[k], k < 2 ? 0 : -1, 0);
            return 1;
        }
        k -= 3;
        if (k < sizeof deep / sizeof *deep) {
            for (n = 0, number = 0; number < deep[k].arrays; number++)
                nested[n++] = '[';
            strcpy(nested + n, deep[k].inner), n += strlen(deep[k].inner);
            for (number = 0; number < deep[k].arrays; number++)
                nested[n++] = ']';
            nested[n] = 0;
            put_fixed_tree(t, nested, deep[k].format, deep[k].delta, 0);
            return 1;
        }
        k -= sizeof deep / sizeof *deep;
        switch (k) {                                                 /* refused, but for 12 */
        case 0: return 1;
        case 1: put(t, 64); return 1;
        case 2: put_two(t, 64); return 1;
        case 3: put_two(t, 64); put(t, 0); return 1;                 /* no item */
        case 4: put_two(t, 64); put(t, 0); put(t, 32); put_two(t, 2); put(t, 4); return 1;   /* two children said, one given */
        case 5: put_two(t, 64); put(t, 0); put(t, 4); put(t, 4); return 1;                   /* a byte after the item */
        case 6: put_two(t, 64); put(t, 0); put(t, 16); put_two(t, 5); puts_in(t, "abc"); return 1;
        case 7: put_two(t, 64); put(t, 0); put(t, 16); put_two(t, 3); put(t, 'a'); put(t, 0); put(t, 'b'); return 1;
        case 8: put_two(t, 64); put(t, 1); put(t, 64); put_two(t, 1); put(t, 4); put_two(t, 2); put(t, 'k'); put(t, 0); return 1;
        case 9: put_two(t, 64); put(t, 0); put(t, 128); put_two(t, 1); put(t, 0); return 1;
        case 10: put_two(t, 64); put(t, 0); put(t, 8); put_bits(t, bits_of(1.5)); put(t, 1); put(t, 0); put(t, 0); return 1;
        case 11: put_two(t, 41); put(t, 0); put(t, 4); return 1;                            /* a length past the answer's buffer */
        case 12: put_two(t, 40); put(t, 0); put(t, 4); return 1;                            /* the largest length: « null » */
        case 13: put_two(t, 0); put(t, 1); put(t, 4); return 1;                             /* no room at all: the original fails */
        case 14: put_two(t, 64); put(t, 0); put(t, 16); return 1;
        case 15: put_two(t, 64); put(t, 0); put(t, 64); put_two(t, 1); put(t, 4); return 1;   /* a child without its name */
        default: break;
        }
        k -= 16;
        if (k < 5) {
            /* the longest inputs: an array of one string of 3063 bytes, plain or every byte written as six (the longest
               answer), at the least length and one below; an array of 3066 « false », unformatted, and formatted (its text
               longer than the answer's buffer: the original fails at every length allowed) */
            static const unsigned char six[] = {1, 2, 3, 4, 5, 6, 7, 11, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
            put_two(t, 0), put(t, k == 4), put(t, 32);
            if (k < 3) {
                put_two(t, 1), put(t, 16), put_two(t, 3063);
                for (n = 0; n < 3063; n++)
                    put(t, k ? six[n % sizeof six] : 'a' + (int)(n % 26));
                set_two(t, 0, (k ? 6 * 3063 : 3063) + 4 + 2 - (k == 2));
            } else {
                put_two(t, 3066);
                for (n = 0; n < 3066; n++)
                    put(t, 1);
                set_two(t, 0, k == 3 ? 1 + 5 * 3066 + 3065 + 1 + 2 : 6 * t->length + 16);
            }
            return 1;
        }
        k -= 5;
        if (k < 80) {
            double d;
            int valueint = 0;
            if (k < 25)
                d = FIXED_PRINT_NUMBER[k].value, valueint = FIXED_PRINT_NUMBER[k].valueint;
            else if (k < 44)
                d = double_of(EDGE_BITS[k - 25]);
            else {
                d = EDGE_VALUES[k - 44];
                valueint = d >= 2147483647.0 ? 2147483647 : d <= -2147483648.0 ? -2147483647 - 1 : (int)d;
            }
            put_two(t, (size_t)printed_number(d, valueint) + 2), put(t, 0), put(t, 8);
            put_bits(t, bits_of(d)), put_int(t, (unsigned)valueint);
            return 1;
        }
        k -= 80;
        if (k < 4) {
            /* an object whose child's name is long: 255, 256, 3000 bytes unformatted, 256 formatted (at the least length:
               the text, its zero and one more) */
            static const size_t names[] = {255, 256, 3000, 256};
            size_t length = names[k];
            put_two(t, length + (k == 3 ? 13 : 9) + 2), put(t, k == 3), put(t, 64), put_two(t, 1), put(t, 4), put_two(t, length);
            for (n = 0; n < length; n++)
                put(t, 'a' + (int)(n % 26));
            return 1;
        }
        k -= 4;
        if (k < 4) {
            /* objects of 256 and of 300 children, each « "k": null », both formats, at the least length: their count needs
               its high byte */
            size_t count = k < 2 ? 256 : 300;
            int formatted = (int)(k % 2);
            put_two(t, (formatted ? 12 * count + 2 : 9 * count + 1) + 2), put(t, formatted), put(t, 64), put_two(t, count);
            for (n = 0; n < count; n++)
                put(t, 4), put_two(t, 1), put(t, 'k');
            return 1;
        }
    } else if (!strcmp(leaf, "parse_value")) {
        /* the literals and their near misses, numbers at their edges, strings with a zero inside (the answer: the text up
           to it), every way an array or an object is left open or broken, what follows a value; the room at every count
           of allocations up to those two texts need and one more; zero bytes as whitespace; the nesting limit, at it and
           past it, by arrays and by an object; offsets inside, at the end, beyond; an answer that just fits the answer's
           buffer and one a byte too long; the longest inputs */
        static const char *texts[] = {
            "null", "false", "true", "nul", "fals", "tru", "NULL", "True", "nullx", "truex", "t", "-", "+1", ".5", "1", "-0",
            "0", "01", "-01", "1.5e3", "1e999", "-1e999", "1e-999", "0x10", "1.", "1.e2", "2147483648", "-2147483649", "1-2",
            "\"\"", "\"a\\u0000b\"", "\"\\ud83d\\ude00\"", "\"abc", "\"\\x\"", "[]", "{}", "[ ]", "{ }", "[1,]", "[,1]",
            "[1 2]", "[1", "[", "{", "[ ", "{ ", "{\"a\":1,}", "{\"a\" 1}", "{\"a\":}", "{1:2}", "{\"a\":1", "{\"", "{\"a\"",
            "{\"a\":", "{\"a\":1,", "{\"\\u0000x\":1}", " [1]", "[1]]", "{\"a\":[{\"b\":null}]}",
            "[true,false,null,1,\"s\",[],{}]", "{\"\":\"\",\"\":1}", "[[[]]]", "[1,2,3]x", "{\"a\":1}x", "[\"\\u00e9\"]", "[-]",
            "[1e]", "{\"a\"::1}", "[\"a\" ,\"b\"]", "{,}", "[\"a\":1]", "{\"a\":1 \"b\":2}"};
        static const struct { size_t arrays; const char *inner; } deep[] = {
            {1000, ""}, {1001, ""}, {999, "{\"\":[]}"}, {999, "{\"\":1}"}, {1000, "{}"}, {999, "{}"}, {1000, "1"}};
        static const struct { const char *text; size_t offset; } offsets[] = {
            {"null", 1}, {"null", 4}, {"null", 5}, {"null", 65535}, {"  null", 2}, {"xx[1]", 2}, {"[1,2]", 3}, {"[1,2]", 4}};
        size_t k = number, n;
        if (k < sizeof texts / sizeof *texts) {
            put_offset(t, 65535), put_offset(t, 0), puts_in(t, texts[k]);
            return 1;
        }
        k -= sizeof texts / sizeof *texts;
        if (k < 12 + 9) {                                            /* (the first needs 10 allocations, the second 7) */
            put_offset(t, k < 12 ? k : k - 12), put_offset(t, 0);
            puts_in(t, k < 12 ? "{\"a\":[1,\"x\",{\"b\":2}]}" : "[\"ab\",-1.5,{\"\":[]}]");
            return 1;
        }
        k -= 12 + 9;
        if (k == 0) {                                                /* zero bytes, and every byte up to 32, as whitespace */
            put_offset(t, 65535), put_offset(t, 0);
            put(t, '['), put(t, 0), put(t, ' '), put(t, '1'), put(t, 0), put(t, ','), put(t, 31), put(t, '"'), put(t, 'a');
            put(t, '"'), put(t, 1), put(t, ']');
            return 1;
        }
        k -= 1;
        if (k < sizeof deep / sizeof *deep) {
            put_offset(t, 65535), put_offset(t, 0);
            for (n = 0; n < deep[k].arrays; n++)
                put(t, '[');
            puts_in(t, deep[k].inner);
            for (n = 0; n < deep[k].arrays; n++)
                put(t, ']');
            return 1;
        }
        k -= sizeof deep / sizeof *deep;
        if (k < sizeof offsets / sizeof *offsets) {
            put_offset(t, 65535), put_offset(t, offsets[k].offset), puts_in(t, offsets[k].text);
            return 1;
        }
        k -= sizeof offsets / sizeof *offsets;
        if (k < 5) {                                                 /* too short: refused; then the empty content */
            for (n = 0; n < k; n++)
                put(t, 0);
            return 1;
        }
        k -= 5;
        if (k < 2) {                                                 /* forty ones: the answer fills the answer's buffer; forty-one: refused */
            put_offset(t, 65535), put_offset(t, 0), put(t, '[');
            for (n = 0; n < 40 + k; n++)
                puts_in(t, n ? ",1" : "1");
            put(t, ']');
            return 1;
        }
        k -= 2;
        if (k < 2) {                                                 /* 1001 arrays, then objects, side by side: the depth goes back down after each */
            put_offset(t, 65535), put_offset(t, 0), put(t, '[');
            for (n = 0; n < 1001; n++)
                puts_in(t, n ? (k ? ",{}" : ",[]") : (k ? "{}" : "[]"));
            put(t, ']');
            return 1;
        }
        k -= 2;
        if (k < 3) {                                                 /* the longest: one long string; 1533 ones (refused); 1022 strings */
            put_offset(t, 65535), put_offset(t, 0);
            if (k == 0) {
                put(t, '"');
                for (n = 0; n < 3066; n++)
                    put(t, 'a' + (int)(n % 26));
                put(t, '"');
            } else {
                put(t, '[');
                for (n = 0; n < (k == 1 ? 1533u : 1022u); n++)
                    puts_in(t, k == 1 ? (n ? ",1" : "1") : (n ? ",\"\"" : "\"\""));
                put(t, ']');
            }
            return 1;
        }
    } else {
        /* two texts of 31 bytes: equal but for their case (0); then differing at their last byte only, both ways;
           then one a prefix of the other; then the shapes that are refused */
        static const char last[][2] = {{0, 0}, {'b', 'c'}, {'c', 'b'}, {'B', 'c'}, {'[', 'a'}};
        if (number < 5) {
            for (i = 0; i < 30; i++) put(t, 'a' + (int)(i % 26));
            put(t, last[number][0] ? last[number][0] : 'e');
            put(t, 0);
            for (i = 0; i < 30; i++) put(t, (number ? 'a' : 'A') + (int)(i % 26));
            put(t, last[number][1] ? last[number][1] : 'E');
            put(t, 0);
            return 1;
        }
        if (number == 5) { for (i = 0; i < 31; i++) put(t, 'a'); put(t, 0); for (i = 0; i < 30; i++) put(t, 'a'); put(t, 0); return 1; }
        if (number == 6) { for (i = 0; i < 30; i++) put(t, 'a'); put(t, 0); for (i = 0; i < 31; i++) put(t, 'a'); put(t, 0); return 1; }
        if (number == 7) return 1;                                                             /* the empty input: refused */
        if (number == 8) { puts_in(t, "abc"); return 1; }                                      /* no zero byte: refused */
        if (number == 9) { puts_in(t, "abc"); put(t, 0); return 1; }                           /* one text only: refused */
        if (number == 10) { put(t, 0); put(t, 0); return 1; }                                  /* two empty texts: equal */
        if (number == 11) { put(t, 0); put(t, 'a'); put(t, 0); return 1; }                     /* an empty text and another */
        if (number == 12) { put(t, 'a'); put(t, 0); put(t, 0); return 1; }
        if (number == 13) { put(t, 'a'); put(t, 0); put(t, 'a'); put(t, 0); put(t, 0); return 1; }   /* three zero bytes: refused */
        if (number == 14) { puts_in(t, "ab"); put(t, 0); put(t, 'c'); put(t, 0); put(t, 'd'); put(t, 0); return 1; }   /* a zero byte inside the second text: refused */
        if (number == 15) { put(t, 'a'); put(t, 0); put(t, 0); put(t, 'b'); put(t, 0); return 1; }
        /* a long text and a short one, either way round, up to the bound */
        if (number == 16) { for (i = 0; i < 40; i++) put(t, 'a'); put(t, 0); put(t, 'b'); put(t, 0); return 1; }
        if (number == 17) { put(t, 'b'); put(t, 0); for (i = 0; i < 40; i++) put(t, 'a'); put(t, 0); return 1; }
        if (number == 18) { for (i = 0; i < 62; i++) put(t, 'A' + (int)(i % 26)); put(t, 0); put(t, 0); return 1; }
        if (number == 19) { put(t, 0); for (i = 0; i < 62; i++) put(t, 'A' + (int)(i % 26)); put(t, 0); return 1; }
        if (number == 20) { for (i = 0; i < 33; i++) put(t, 'a'); put(t, 0); for (i = 0; i < 29; i++) put(t, 'A'); put(t, 0); return 1; }
    }
    return 0;
}

static const char *FIXED_HEX4[] = {"0000", "ffff", "FFFF", "12aB", "00g0", "D83d", "", "123", "12345", "0123456789abcdef",
                                   "09af", "09AF", "/000", ":000", "@000", "G000", "`000", "g000", "0/00", "0:00", "0@00", "0G00", "0`00", "0g00",
                                   "00/0", "00:0", "00@0", "00G0", "00`0", "00g0", "000/", "000:", "000@", "000G", "000`", "000g"};
static const char *FIXED_UTF16[] = {
    "\\u0041", "\\u00e9", "\\u20AC", "\\uD83D\\uDE00", "\\uD83D", "\\uDE00", "\\uD83D\\u0041", "\\u00", "", "\\u0000rest",
    /* every constant of the original and its neighbours, as a first code */
    "\\u007f", "\\u0080", "\\u07ff", "\\u0800", "\\uffff", "\\ud7ff", "\\ue000", "\\udc00", "\\udbff", "\\udfff", "\\ud800",
    /* and as the second half of a pair */
    "\\ud800\\udc00", "\\ud800\\udfff", "\\udbff\\udc00", "\\udbff\\udfff", "\\ud800\\udbff", "\\ud800\\ue000", "\\ud800\\ud7ff",
    "\\ud800\\u0041", "\\ud800/udc00", "\\ud800\\Udc00", "\\ud800\\udc0", "\\ud800\\udc0g",
    /* the longest input the leaf is given, and a pair with as much after it as fits */
    "\\u0041aaaaaaaaaaaaaaaaaaaaaaaaaa", "\\ud800\\udc00aaaaaaaaaaaaaaaaaaaa"};
static const char *FIXED_MINIFY[] = {"", "{ \"a\" : 1 }", "[1, 2, 3] // end", "/* open", "\"un\\\"closed", "{\"a\":\"b // not a comment\"}", " \t\r\n", "/", "a/b",
                                     "a\fb", "a\vb", "a\001b", "a\037b", "a\177b", "a\200b", "a\377b", "\"a\fb\tc\"", "// a\rb\nc", "/*/ x */y", "a\\b",
                                     /* a backslash outside a string protects nothing */
                                     "\\\"a b\"", "a\\/b", "\\ a\\\tb", "\\// c\nd", "\\/* c */d"};
static const char *FIXED_PRINT[] = {"", "plain", "quote \" and \\ slash", "tab\there", "\x01\x1f", "caf\xc3\xa9", "\x7f\x80\xff", "\b\f\n\r\t\v",
    /* more bytes to escape than a small counter holds */
    "\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001"
    "\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001"};
static const char *FIXED_PARSE_NUMBER[] = {
    /* the run the original copies (digits, signs, points, e and E) and the prefix of it strtod reads; nothing: refused */
    "", "0", "-0", "+0", "1", "-1", "+1", "12", "007", "1.5", "-1.5", ".5", "-.5", "+.5", "5.", "-5.", ".", "-", "+", "-.", "+-1",
    "-+1", "--1", "++1", "e", "E", "e5", ".e5", "-e5", "1e", "1E", "1e+", "1e-", "1e5", "1E5", "1e+5", "1E-5", "1.e5", "1.5e5",
    "1e05", "1e5e5", "1e5.5", "1e+-5", "1e5+", "1.2.3", "1..2", "1-2", "1+2", "12e3x", "1x", "1 2", " 1", "\t1", "x1", "0x10",
    "0X1p3", "inf", "-inf", "nan", "Infinity", "1,5", "1;5",
    /* valueint: saturated at an int's bounds, and cut towards zero between them */
    "2147483646.9", "2147483647", "2147483647.4", "2147483648", "-2147483647.9", "-2147483648", "-2147483648.4", "-2147483649",
    "0.9", "-0.9", "1e10", "-1e10",
    /* the bounds of the doubles: the greatest and the middle above it, infinity; the least normal, the least subnormal and
       half of it; exponents too large for any integer type */
    "1.7976931348623157e308", "1.7976931348623158e308", "1.7976931348623159e308", "1e309", "-1e309", "1e999999999999999999999",
    "2.2250738585072011e-308", "2.2250738585072014e-308", "4.9406564584124654e-324", "2.4703282292062327e-324",
    "2.4703282292062328e-324", "1e-324", "1e-400", "-1e-400", "1e-99999999999999999999", "0e999999999999999999",
    "1e0000000000000000000000000000000000005",
    /* rounding: the middle of two doubles goes to the even one, a little more or less to the nearer */
    "9007199254740993", "9007199254740995", "9007199254740993.000000000000001", "9007199254740992.999999999999999",
    "1.00000000000000011102230246251565404236316680908203125", "1.00000000000000011102230246251565404236316680908203126",
    "1.00000000000000011102230246251565404236316680908203124", "0.1", "0.30000000000000004", "123456789012345678901234567890"};

int main(int argc, char **argv)
{
    char seed[72] = {0};
    void (*draw)(text *) = NULL;
    const char **fixed = NULL;
    size_t known = 0, count, i, bound;
    text *cases;
    FILE *out;
    if (argc < 3 || argc > 4 || (count = strtoul(argv[2], NULL, 10)) < 1)
        return 2;
    /* (each leaf's bound is its unit's « case_bytes » in bee/reference/project.json: no case is ever longer) */
    if (!strcmp(argv[1], "hex4")) draw = hex4, fixed = FIXED_HEX4, known = sizeof FIXED_HEX4 / sizeof *FIXED_HEX4, bound = 16;
    else if (!strcmp(argv[1], "utf16")) draw = utf16, fixed = FIXED_UTF16, known = sizeof FIXED_UTF16 / sizeof *FIXED_UTF16, bound = 32;
    else if (!strcmp(argv[1], "minify")) draw = minify, fixed = FIXED_MINIFY, known = sizeof FIXED_MINIFY / sizeof *FIXED_MINIFY, bound = 256;
    else if (!strcmp(argv[1], "print_string")) draw = print_string, fixed = FIXED_PRINT, known = sizeof FIXED_PRINT / sizeof *FIXED_PRINT, bound = 256;
    else if (!strcmp(argv[1], "strcasecmp")) draw = strcasecmp_case, bound = 64;
    else if (!strcmp(argv[1], "parse_number")) draw = parse_number_case, fixed = FIXED_PARSE_NUMBER, known = sizeof FIXED_PARSE_NUMBER / sizeof *FIXED_PARSE_NUMBER, bound = 256;
    else if (!strcmp(argv[1], "compare_double")) draw = compare_double_case, bound = 32;
    else if (!strcmp(argv[1], "print_number")) draw = print_number_case, bound = 16;
    else if (!strcmp(argv[1], "buffer_skip_whitespace")) draw = buffer_skip_whitespace_case, bound = 256;
    else if (!strcmp(argv[1], "skip_utf8_bom")) draw = skip_utf8_bom_case, bound = 16;
    else if (!strcmp(argv[1], "update_offset")) draw = update_offset_case, bound = 256;
    else if (!strcmp(argv[1], "parse_string")) draw = parse_string_case, bound = 256;
    else if (!strcmp(argv[1], "strdup")) draw = strdup_case, bound = 256;
    else if (!strcmp(argv[1], "print_value")) draw = print_value_case, bound = 3072;
    else if (!strcmp(argv[1], "parse_value")) draw = parse_value_case, bound = 3072;
    else return 2;
    if (!fgets(seed, sizeof seed, stdin) || strlen(seed) < 32)
        return 2;
    for (i = 0; i < 32; i++)
        state[i / 16] = state[i / 16] * 16 + (unsigned long long)(seed[i] <= '9' ? seed[i] - '0' : (seed[i] | 32) - 'a' + 10);
    state[0] |= 1;
    cases = calloc(count, sizeof *cases);
    if (!cases)
        return 3;
    for (i = 0; i < count; i++) {
        if (i < known)
            puts_in(&cases[i], fixed[i]);
        else if (!special(&cases[i], argv[1], i - known))
            draw(&cases[i]);
        if (cases[i].length > bound)
            cases[i].length = bound;                                 /* (a case cut at the bound is a case too) */
    }
    for (i = count - 1; i > 0; i--) {
        size_t other = below(i + 1);
        text kept = cases[i];
        cases[i] = cases[other];
        cases[other] = kept;
    }
    out = fopen(argc > 3 ? argv[3] : "/cases/cases", "wb");
    if (!out)
        return 3;
    for (i = 0; i < count; i++) {
        unsigned char b[4];
        b[0] = (unsigned char)cases[i].length, b[1] = (unsigned char)(cases[i].length >> 8), b[2] = 0, b[3] = 0;
        fwrite(b, 1, 4, out);
        fwrite(cases[i].bytes, 1, cases[i].length, out);
    }
    return fclose(out) == 0 ? 0 : 3;
}
