/* The generator of cases: « generate <leaf> <count> [cases] », the seed (hexadecimal) on its input; writes /cases/cases
   (the judge's framing: « <u32 length><bytes> »…). For each leaf:
   - FIXED cases, at every draw: every constant the original compares with, and its neighbours (the bounds of a
     UTF-8 length, of the surrogates, of a digit); the longest input the manifest allows (LONGEST below is the
     manifest's largest « case_bytes », and each leaf's own bound is the one its unit has there); the empty input;
   - DRAWN cases, from what the leaf's inputs are made of: every byte value, every length up to the leaf's bound,
     the hard paths on purpose (escapes, surrogate pairs whole and broken, texts cut short, comments left open,
     control characters, zero bytes where a leaf refuses them), and the longest answers on purpose too (a text kept
     whole, a text whose every byte is written as six, two texts of unequal lengths up to the bound).
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
