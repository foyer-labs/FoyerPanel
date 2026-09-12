/* ------------------------------------------------------------------------
 * Language tables and plural rules — see i18n.h.
 * --------------------------------------------------------------------- */
#include "i18n.h"

#include <ctype.h>
#include <string.h>

static lang_t current = LANG_EN;
static bool forced;

/* The edits of the current language (see i18n.h). They belong to one
   language: changing language clears them, and whoever loads them loads
   the new language's. */
static const char *over[TX_COUNT + 1];
static const char *over_n[TXN_COUNT + 1][2];

static bool find(const char *code, lang_t *out)
{
    if (!code) return false;
    for (int n = 0; n < LANG_COUNT; n++) {
        if (strcmp(LANG_INFO[n].code, code) == 0) {
            *out = (lang_t)n;
            return true;
        }
    }
    return false;
}

bool i18n_set(const char *code)
{
    if (forced) return true;
    lang_t l = LANG_EN;
    const bool ok = find(code, &l);
    if (l != current) i18n_clear_overrides();
    current = l;
    return ok;
}

bool i18n_force(const char *code)
{
    forced = false;
    const bool ok = i18n_set(code);
    forced = true;
    return ok;
}

lang_t i18n_lang(void) { return current; }
const char *i18n_code(void) { return LANG_INFO[current].code; }
char i18n_decimal(void) { return LANG_INFO[current].decimal; }

const char *tr(tx_t id)
{
    if ((unsigned)id >= TX_COUNT) return "";
    if (over[id]) return over[id];
    const char *s = TEXTS[current][id];
    if (!s) s = TEXTS[LANG_EN][id];
    return s ? s : "";
}

/* Which of the two forms goes with n.
 *
 * Five languages, two rules. English, Italian, German and Spanish say
 * "1 thing, 0 things"; French says "0 chose, 1 chose, 2 choses" — zero is
 * singular. Languages with more than two forms (Polish, Russian, Arabic)
 * would need a third column in the tables, and this function is where
 * they would get it. */
static int form(long n)
{
    if (n < 0) n = -n;
    if (strcmp(LANG_INFO[current].code, "fr") == 0) return n <= 1 ? 0 : 1;
    return n == 1 ? 0 : 1;
}

const char *trn(txn_t id, long n)
{
    if ((unsigned)id >= TXN_COUNT) return "";
    const int f = form(n);
    if (over_n[id][f]) return over_n[id][f];
    const char *s = TEXTS_N[current][id][f];
    if (!s) s = TEXTS_N[LANG_EN][id][f];
    return s ? s : "";
}

void i18n_override(tx_t id, const char *s)
{
    if ((unsigned)id < TX_COUNT) over[id] = s;
}

void i18n_override_n(txn_t id, int f, const char *s)
{
    if ((unsigned)id < TXN_COUNT && (f == 0 || f == 1)) over_n[id][f] = s;
}

void i18n_clear_overrides(void)
{
    memset(over, 0, sizeof over);
    memset(over_n, 0, sizeof over_n);
}

int i18n_key(const char *key)
{
    if (!key) return -1;
    for (int n = 0; n < TX_COUNT; n++)
        if (strcmp(TX_KEYS[n], key) == 0) return n;
    return -1;
}

int i18n_key_n(const char *key)
{
    if (!key) return -1;
    for (int n = 0; n < TXN_COUNT; n++)
        if (strcmp(TXN_KEYS[n], key) == 0) return n;
    return -1;
}

/* --- checks --------------------------------------------------------------- */

/* The printf conversion starting at `s` (a '%'): where it ends, or NULL if
   this '%' does not start one. */
static const char *spec_end(const char *s)
{
    const char *e = s + 1;
    if (*e == '%') return e + 1;
    while (*e && strchr("-+ #0", *e)) e++;
    if (*e == '*') e++; else while (isdigit((unsigned char)*e)) e++;
    if (*e == '.') {
        const char *d = e + 1;
        if (*d == '*') d++; else while (isdigit((unsigned char)*d)) d++;
        if (d == e + 1) return NULL;     /* "." needs digits or '*' */
        e = d;
    }
    if ((e[0] == 'h' && e[1] == 'h') || (e[0] == 'l' && e[1] == 'l')) e += 2;
    else if (*e && strchr("hlzjtL", *e)) e++;
    return *e && strchr("diouxXeEfFgGaAcsp", *e) ? e + 1 : NULL;
}

/* The next printf conversion from *p, copied into `spec`; false at the end.
   Exactly gen_texts.py's PRINTF_RE, so that the page accepts what the build
   accepts: flags, width, precision, length, conversion, and "%%" — which
   has to match like the others, or "100%%" translated as "100%" would print
   whatever follows. A '%' that starts none of them is text. */
static bool next_spec(const char **p, char *spec, size_t max)
{
    for (const char *s = *p; (s = strchr(s, '%')) != NULL; s++) {
        const char *e = spec_end(s);
        if (!e) continue;
        size_t n = (size_t)(e - s);
        if (n >= max) n = max - 1;
        memcpy(spec, s, n);
        spec[n] = 0;
        *p = e;
        return true;
    }
    return false;
}

bool i18n_same_printf(const char *a, const char *b)
{
    if (!a || !b) return false;
    char x[24], y[24];
    for (;;) {
        const bool ha = next_spec(&a, x, sizeof x);
        const bool hb = next_spec(&b, y, sizeof y);
        if (ha != hb) return false;
        if (!ha) return true;
        if (strcmp(x, y) != 0) return false;
    }
}

/* The {names} of `s`, sorted, joined by '\x01' — two texts have the same
   placeholders when their lists are equal. */
static void named_list(const char *s, char *out, size_t max)
{
    char names[32][32];
    int n = 0;
    for (const char *p = s; (p = strchr(p, '{')) != NULL && n < 32; p++) {
        const char *e = p + 1;
        while (*e == '_' || (*e >= 'a' && *e <= 'z') || isdigit((unsigned char)*e)) e++;
        if (*e != '}' || e == p + 1 || isdigit((unsigned char)p[1])) continue;
        size_t l = (size_t)(e - p - 1);
        if (l >= sizeof names[0]) l = sizeof names[0] - 1;
        memcpy(names[n], p + 1, l);
        names[n][l] = 0;
        n++;
    }
    /* insertion sort: a text has a handful of placeholders */
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
            char t[32];
            memcpy(t, names[j], sizeof t);
            memcpy(names[j], names[j - 1], sizeof t);
            memcpy(names[j - 1], t, sizeof t);
        }
    out[0] = 0;
    for (int i = 0; i < n; i++) {
        strncat(out, names[i], max - strlen(out) - 1);
        strncat(out, "\x01", max - strlen(out) - 1);
    }
}

bool i18n_same_named(const char *a, const char *b)
{
    if (!a || !b) return false;
    char x[512], y[512];
    named_list(a, x, sizeof x);
    named_list(b, y, sizeof y);
    return strcmp(x, y) == 0;
}

bool i18n_drawable(const char *s)
{
    if (!s) return false;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned int cp;
        int more;
        if (*p < 0x80)                { cp = *p; more = 0; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; more = 1; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; more = 2; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; more = 3; }
        else return false;
        p++;
        for (int i = 0; i < more; i++, p++) {
            if ((*p & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (*p & 0x3F);
        }
        bool ok = false;
        for (int r = 0; r < I18N_GLYPHS_N && !ok; r++)
            ok = cp >= I18N_GLYPHS[r][0] && cp <= I18N_GLYPHS[r][1];
        if (!ok) return false;
    }
    return true;
}

bool i18n_keyboard_ok(const char *s)
{
    if (!s) return false;
    int rows = 1, in_row = 0;
    unsigned int seen = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '|') {
            if (!in_row) return false;
            rows++;
            in_row = 0;
        } else if (*p >= 'a' && *p <= 'z') {
            seen |= 1u << (*p - 'a');
            if (++in_row > 12) return false;
        } else {
            return false;
        }
    }
    return rows == 3 && in_row > 0 && seen == (1u << 26) - 1;
}

void i18n_decimals(char *s)
{
    const char sep = i18n_decimal();
    if (!s || sep == '.') return;
    for (char *p = s; *p; p++)
        if (*p == '.' && p > s && isdigit((unsigned char)p[-1])
                && isdigit((unsigned char)p[1]))
            *p = sep;
}
