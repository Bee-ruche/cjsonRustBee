/* The generator of cases: « generate <leaf> <count> [cases] », the seed (hexadecimal) on its input; writes /cases/cases
   (the judge's framing: « <u32 length><bytes> »…). For each leaf:
   - FIXED cases, at every draw: every constant the original compares with, and its neighbours (the bounds of a
     UTF-8 length, of the surrogates, of a digit, of an int, of the doubles); the longest input the manifest allows (LONGEST below is the
     manifest's largest « case_bytes », and each leaf's own bound is the one its unit has there); the empty input;
   - DRAWN cases, from what the leaf's inputs are made of: every byte value, every length up to the leaf's bound,
     the hard paths on purpose (escapes, surrogate pairs whole and broken, texts cut short, comments left open,
     control characters, zero bytes where a leaf refuses them).
   The order is drawn too. A wrong leaf that is accepted names a path these cases do not reach: add the case here. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LONGEST = 256 };

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

static void minify(text *t)
{
    static const char *marks = "{}[]:,";
    size_t parts = below(4) ? below(24) : below(120), most = 256;    /* (the leaf's bound: 256 bytes) */
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

static void print_string(text *t)
{
    size_t n = below(4) ? below(40) : below(257);                    /* (the leaf's bound: 256 bytes) */
    int dense = below(5) == 0;                                       /* nearly every byte needs an escape */
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
    unsigned char first[32];
    size_t n = below(32), i, shape = below(16);                      /* (the leaf's bound: 64 bytes, two texts and their zeros) */
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
    for (i = 0; i < n; i++) {
        int c = first[i];
        if (below(3) == 0 && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
            c ^= 32;                                                 /* the other case */
        put(t, c);
    }
    if (below(3) == 0 && n)
        t->bytes[t->length - 1 - below(n)] = (unsigned char)(1 + below(255));   /* one byte differs */
    if (below(4) == 0)
        t->length -= below(n + 1);                                   /* a prefix */
    if (below(4) == 0) { size_t more = 1 + below(3); while (more--) put(t, 'a' + (int)below(26)); }
    if (shape == 1)
        return;                                                      /* no zero byte at the end: refused */
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
    } else if (!strcmp(leaf, "print_string")) {
        if (number == 0) {
            for (i = 0; i < 256; i++) put(t, i % 4 == 0 ? 1 + (int)(i % 31) : i % 4 == 1 ? '"' : i % 4 == 2 ? '\\' : 'a' + (int)(i % 26));
            return 1;
        }
        if (number == 1) { put(t, 'a'); put(t, 0); put(t, 'b'); return 1; }                    /* a zero byte: refused */
        if (number == 2) { put(t, 0); return 1; }
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
                                     "a\fb", "a\vb", "a\001b", "a\037b", "a\177b", "a\200b", "a\377b", "\"a\fb\tc\"", "// a\rb\nc", "/*/ x */y", "a\\b"};
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
