/* ------------------------------------------------------------------------
 * Translations edited on the panel — see translations.h.
 * --------------------------------------------------------------------- */
#include "translations.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archivio.h"
#include "cJSON.h"
#include "i18n.h"

static const char *const FORMS[2] = { "one", "other" };

/* --- errors --------------------------------------------------------------- */

static translation_error_t errors[TRANSLATIONS_ERRORS_MAX];
static int n_errors;

int translations_errors(void) { return n_errors; }

const translation_error_t *translations_error(int n)
{
    return n >= 0 && n < n_errors ? &errors[n] : NULL;
}

/* One more error, as "panel.robot.alerts.one". Past the sixteenth they are
   not kept: the first ones say enough, and the save fails all the same. */
static void fail(const char *section, const char *key, int form,
                 const char *reason)
{
    if (n_errors >= TRANSLATIONS_ERRORS_MAX) return;
    translation_error_t *e = &errors[n_errors++];
    snprintf(e->field, sizeof e->field, "%s%s%s%s%s", section,
             key ? "." : "", key ? key : "",
             form >= 0 ? "." : "", form >= 0 ? FORMS[form] : "");
    snprintf(e->reason, sizeof e->reason, "%s", reason);
}

/* --- languages and files -------------------------------------------------- */

static bool find_lang(const char *code, lang_t *out)
{
    if (!code) return false;
    for (int n = 0; n < LANG_COUNT; n++)
        if (strcmp(LANG_INFO[n].code, code) == 0) { *out = (lang_t)n; return true; }
    return false;
}

static void file_name(lang_t l, char *buf, size_t n)
{
    snprintf(buf, n, "texts-%s.json", LANG_INFO[l].code);
}

/* The language's edits, or NULL when there are none. The buffer comes
   from cJSON's allocator, which on the panel is PSRAM: a whole language
   is more than the internal RAM should lend for a moment. */
static cJSON *read_file(lang_t l)
{
    char name[32];
    file_name(l, name, sizeof name);
    if (!archivio_esiste(name)) return NULL;
    const size_t max = ARCHIVIO_FILE_MAX + 2;
    char *buf = cJSON_malloc(max);
    if (!buf) return NULL;
    size_t n = 0;
    cJSON *doc = NULL;
    if (archivio_leggi(name, buf, max, &n)) doc = cJSON_ParseWithLength(buf, n);
    cJSON_free(buf);
    if (!cJSON_IsObject(doc)) { cJSON_Delete(doc); return NULL; }
    return doc;
}

/* The section `name` of `doc`, made an empty object if it is not one. */
static cJSON *section_of(cJSON *doc, const char *name)
{
    cJSON *s = cJSON_GetObjectItemCaseSensitive(doc, name);
    if (cJSON_IsObject(s)) return s;
    cJSON_DeleteItemFromObjectCaseSensitive(doc, name);
    return cJSON_AddObjectToObject(doc, name);
}

/* The page's built-in texts of a language. They are in the firmware as
   JSON text — the page is the one that reads them — so every use parses
   them: rare, and cheaper than keeping a tree in memory for it. */
static cJSON *builtin_web(lang_t l)
{
    cJSON *w = cJSON_Parse(WEB_TEXTS[l]);
    if (!cJSON_IsObject(w)) { cJSON_Delete(w); w = cJSON_CreateObject(); }
    return w;
}

/* --- what a key is -------------------------------------------------------- */

typedef struct {
    lang_t lang;
    cJSON *web_en;      /* NULL where only the panel's texts are used */
    cJSON *web_base;
} ctx_t;

/* A key of either section: whether it has plural forms, its English text
   and the language's built-in one. Plain texts use index 0 only. */
typedef struct {
    bool        plural;
    int         id;          /* index in the panel's tables; -1 for web */
    const char *en[2];
    const char *base[2];
} ref_t;

static const char *form_of(const cJSON *o, int f)
{
    const cJSON *s = cJSON_GetObjectItemCaseSensitive(o, FORMS[f]);
    return cJSON_IsString(s) ? s->valuestring : NULL;
}

static bool resolve(const ctx_t *c, bool panel, const char *key, ref_t *r)
{
    memset(r, 0, sizeof *r);
    r->id = -1;
    if (!key) return false;
    if (panel) {
        int id = i18n_key(key);
        if (id >= 0) {
            r->id = id;
            r->en[0] = TEXTS[LANG_EN][id];
            r->base[0] = TEXTS[c->lang][id] ? TEXTS[c->lang][id] : r->en[0];
            return r->en[0] != NULL;
        }
        id = i18n_key_n(key);
        if (id < 0) return false;
        r->id = id;
        r->plural = true;
        for (int f = 0; f < 2; f++) {
            r->en[f] = TEXTS_N[LANG_EN][id][f];
            r->base[f] = TEXTS_N[c->lang][id][f] ? TEXTS_N[c->lang][id][f] : r->en[f];
            if (!r->en[f]) return false;
        }
        return true;
    }

    const cJSON *e = cJSON_GetObjectItemCaseSensitive(c->web_en, key);
    const cJSON *b = cJSON_GetObjectItemCaseSensitive(c->web_base, key);
    if (cJSON_IsString(e)) {
        r->en[0] = e->valuestring;
        r->base[0] = cJSON_IsString(b) ? b->valuestring : r->en[0];
        return true;
    }
    if (!cJSON_IsObject(e)) return false;
    r->plural = true;
    for (int f = 0; f < 2; f++) {
        r->en[f] = form_of(e, f);
        if (!r->en[f]) return false;
        const char *x = cJSON_IsObject(b) ? form_of(b, f) : NULL;
        r->base[f] = x ? x : r->en[f];
    }
    return true;
}

/* --- checks --------------------------------------------------------------- */

static bool valid_utf8(const char *s)
{
    for (const unsigned char *p = (const unsigned char *)s; *p; ) {
        int more = *p < 0x80 ? 0 : (*p & 0xE0) == 0xC0 ? 1
                 : (*p & 0xF0) == 0xE0 ? 2 : (*p & 0xF8) == 0xF0 ? 3 : -1;
        if (more < 0) return false;
        p++;
        for (int i = 0; i < more; i++, p++)
            if ((*p & 0xC0) != 0x80) return false;
    }
    return true;
}

/* NULL if `s` may stand for `en`; otherwise why not. The reasons are for
   whoever reads the 422 — in English, like every technical message. */
static const char *check(bool panel, const char *key, const char *en,
                         const char *s)
{
    if (strlen(s) > TRANSLATION_TEXT_MAX) return "too long";
    if (!panel) {
        if (!valid_utf8(s)) return "not valid UTF-8";
        if (!i18n_same_named(en, s)) return "the {placeholders} differ from English";
        return NULL;
    }
    if (!i18n_drawable(s)) return "a character the panel's fonts cannot draw";
    if (!i18n_same_printf(en, s)) return "the % placeholders differ from English";
    if (strcmp(key, "keyboard.letters") == 0 && !i18n_keyboard_ok(s))
        return "three rows of a-z separated by '|', all 26 letters";
    return NULL;
}

/* The edit of form `f` in `item` — an entry of the file — as it is. */
static const char *raw_form(const ref_t *r, const cJSON *item, int f)
{
    if (!r->plural) return f == 0 && cJSON_IsString(item) ? item->valuestring : NULL;
    return cJSON_IsObject(item) ? form_of(item, f) : NULL;
}

/* The same, only if it still passes the checks. */
static const char *custom_form(bool panel, const char *key, const ref_t *r,
                               const cJSON *item, int f)
{
    const char *s = raw_form(r, item, f);
    return s && !check(panel, key, r->en[f], s) ? s : NULL;
}

/* What each form shows: the edit if it is good, the built-in text if not. */
static void merged(bool panel, const char *key, const ref_t *r,
                   const cJSON *item, const char *v[2])
{
    for (int f = 0; f < 2; f++) {
        const char *s = f == 0 || r->plural
            ? custom_form(panel, key, r, item, f) : NULL;
        v[f] = s ? s : r->base[f];
    }
}

/* --- into tr() ------------------------------------------------------------ */

/* The file in use. tr() returns pointers into it, so it lives until the
   next translations_apply(), which clears them before letting it go. */
static cJSON *applied;

void translations_apply(void)
{
    i18n_clear_overrides();
    cJSON_Delete(applied);
    applied = NULL;

    const ctx_t c = { .lang = i18n_lang() };
    cJSON *doc = read_file(c.lang);
    if (!doc) return;
    /* The page's texts are the page's: the panel keeps only its own. */
    cJSON_DeleteItemFromObjectCaseSensitive(doc, "web");

    const cJSON *item;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(doc, "panel")) {
        ref_t r;
        if (!resolve(&c, true, item->string, &r)) continue;
        for (int f = 0; f < (r.plural ? 2 : 1); f++) {
            const char *s = custom_form(true, item->string, &r, item, f);
            if (!s) continue;
            if (r.plural) i18n_override_n((txn_t)r.id, f, s);
            else          i18n_override((tx_t)r.id, s);
        }
    }
    applied = doc;
}

/* --- for the page --------------------------------------------------------- */

void translations_free(char *s) { cJSON_free(s); }

static void add_value(cJSON *o, const char *name, const ref_t *r,
                      const char *const v[2])
{
    if (!r->plural) { cJSON_AddStringToObject(o, name, v[0]); return; }
    cJSON *p = cJSON_AddObjectToObject(o, name);
    for (int f = 0; f < 2; f++)
        if (v[f]) cJSON_AddStringToObject(p, FORMS[f], v[f]);
}

static void add_languages(cJSON *o)
{
    cJSON *langs = cJSON_AddArrayToObject(o, "languages");
    for (int n = 0; n < LANG_COUNT && langs; n++) {
        cJSON *x = cJSON_CreateObject();
        if (!x) break;
        cJSON_AddStringToObject(x, "code", LANG_INFO[n].code);
        cJSON_AddStringToObject(x, "name", LANG_INFO[n].name);
        cJSON_AddItemToArray(langs, x);
    }
}

char *translations_page(const char *code)
{
    ctx_t c;
    if (!find_lang(code, &c.lang)) return NULL;
    c.web_en = builtin_web(LANG_EN);
    c.web_base = builtin_web(c.lang);
    cJSON *doc = read_file(c.lang);
    const cJSON *custom = cJSON_GetObjectItemCaseSensitive(doc, "web");

    char *out = NULL;
    cJSON *o = cJSON_CreateObject();
    if (o) {
        cJSON_AddStringToObject(o, "lang", LANG_INFO[c.lang].code);
        add_languages(o);
        cJSON *web = cJSON_AddObjectToObject(o, "web");
        const cJSON *e;
        cJSON_ArrayForEach(e, c.web_en) {
            ref_t r;
            if (!resolve(&c, false, e->string, &r)) continue;
            const char *v[2];
            merged(false, e->string, &r,
                   cJSON_GetObjectItemCaseSensitive(custom, e->string), v);
            add_value(web, e->string, &r, v);
        }
        out = cJSON_PrintUnformatted(o);
    }
    cJSON_Delete(o);
    cJSON_Delete(doc);
    cJSON_Delete(c.web_en);
    cJSON_Delete(c.web_base);
    return out;
}

/* One key for the editor: its English text, the built-in one, the edit. */
static void add_entry(cJSON *out, const ctx_t *c, bool panel, const char *key,
                      const cJSON *custom)
{
    ref_t r;
    if (!resolve(c, panel, key, &r)) return;
    cJSON *e = cJSON_AddObjectToObject(out, key);
    if (!e) return;
    if (c->lang != LANG_EN) add_value(e, "en", &r, r.en);
    add_value(e, "base", &r, r.base);

    const cJSON *item = cJSON_GetObjectItemCaseSensitive(custom, key);
    const char *v[2] = { NULL, NULL };
    bool stale = false;
    for (int f = 0; f < (r.plural ? 2 : 1); f++) {
        v[f] = raw_form(&r, item, f);
        if (v[f] && check(panel, key, r.en[f], v[f])) stale = true;
    }
    if (!v[0] && !v[1]) return;
    add_value(e, "custom", &r, v);
    if (stale) cJSON_AddTrueToObject(e, "stale");
}

char *translations_editor(const char *code)
{
    ctx_t c;
    if (!find_lang(code, &c.lang)) return NULL;
    c.web_en = builtin_web(LANG_EN);
    c.web_base = builtin_web(c.lang);
    cJSON *doc = read_file(c.lang);
    const cJSON *panel_custom = cJSON_GetObjectItemCaseSensitive(doc, "panel");
    const cJSON *web_custom = cJSON_GetObjectItemCaseSensitive(doc, "web");

    char *out = NULL;
    cJSON *o = cJSON_CreateObject();
    if (o) {
        cJSON_AddStringToObject(o, "lang", LANG_INFO[c.lang].code);
        /* The characters the fonts can draw, so that the page says "the
           panel cannot draw this" while the text is typed, not at save. */
        cJSON *glyphs = cJSON_AddArrayToObject(o, "glyphs");
        for (int n = 0; n < I18N_GLYPHS_N && glyphs; n++) {
            const int r[2] = { (int)I18N_GLYPHS[n][0], (int)I18N_GLYPHS[n][1] };
            cJSON_AddItemToArray(glyphs, cJSON_CreateIntArray(r, 2));
        }
        cJSON *panel = cJSON_AddObjectToObject(o, "panel");
        for (int n = 0; n < TX_COUNT && panel; n++)
            add_entry(panel, &c, true, TX_KEYS[n], panel_custom);
        for (int n = 0; n < TXN_COUNT && panel; n++)
            add_entry(panel, &c, true, TXN_KEYS[n], panel_custom);
        cJSON *web = cJSON_AddObjectToObject(o, "web");
        const cJSON *e;
        cJSON_ArrayForEach(e, c.web_en)
            if (web) add_entry(web, &c, false, e->string, web_custom);
        out = cJSON_PrintUnformatted(o);
    }
    cJSON_Delete(o);
    cJSON_Delete(doc);
    cJSON_Delete(c.web_en);
    cJSON_Delete(c.web_base);
    return out;
}

/* --- saving --------------------------------------------------------------- */

/* One form of one key into `dst`, the key's entry in the file: NULL
   removes it, a text equal to the built-in one removes it too — an edit
   that changes nothing would only hide a later fix of the built-in text. */
static void merge_form(cJSON *dst, const char *name, const char *s,
                       const char *base)
{
    cJSON_DeleteItemFromObjectCaseSensitive(dst, name);
    if (s && strcmp(s, base) != 0) cJSON_AddStringToObject(dst, name, s);
}

static void merge_entry(const ctx_t *c, bool panel, const char *sec,
                        cJSON *dst, const cJSON *val)
{
    const char *key = val->string;
    ref_t r;
    if (!resolve(c, panel, key, &r)) { fail(sec, key, -1, "no such text"); return; }

    if (cJSON_IsNull(val)) {
        cJSON_DeleteItemFromObjectCaseSensitive(dst, key);
        return;
    }
    if (!r.plural) {
        if (!cJSON_IsString(val)) { fail(sec, key, -1, "must be a text"); return; }
        const char *why = check(panel, key, r.en[0], val->valuestring);
        if (why) { fail(sec, key, -1, why); return; }
        merge_form(dst, key, val->valuestring, r.base[0]);
        return;
    }

    if (!cJSON_IsObject(val)) { fail(sec, key, -1, "must be {\"one\", \"other\"}"); return; }
    const cJSON *x;
    cJSON_ArrayForEach(x, val) {
        const int f = x->string && strcmp(x->string, "one") == 0 ? 0
                    : x->string && strcmp(x->string, "other") == 0 ? 1 : -1;
        if (f < 0) { fail(sec, key, -1, "only \"one\" and \"other\""); return; }
        if (!cJSON_IsNull(x) && !cJSON_IsString(x)) { fail(sec, key, f, "must be a text"); return; }
        const char *why = cJSON_IsString(x) ? check(panel, key, r.en[f], x->valuestring) : NULL;
        if (why) { fail(sec, key, f, why); return; }
    }
    cJSON *forms = cJSON_GetObjectItemCaseSensitive(dst, key);
    if (!cJSON_IsObject(forms)) {
        cJSON_DeleteItemFromObjectCaseSensitive(dst, key);
        forms = cJSON_AddObjectToObject(dst, key);
        if (!forms) { fail(sec, key, -1, "out of memory"); return; }
    }
    cJSON_ArrayForEach(x, val) {
        const int f = strcmp(x->string, "one") == 0 ? 0 : 1;
        merge_form(forms, FORMS[f], cJSON_IsString(x) ? x->valuestring : NULL, r.base[f]);
    }
    if (!forms->child) cJSON_DeleteItemFromObjectCaseSensitive(dst, key);
}

/* Entries the firmware no longer has — a key renamed by an update — or
   of the wrong shape. Saving is the moment to forget them. */
static void drop_unknown(const ctx_t *c, bool panel, cJSON *sec)
{
    cJSON *x = sec->child;
    while (x) {
        cJSON *next = x->next;
        ref_t r;
        const bool keep = resolve(c, panel, x->string, &r)
            && (r.plural ? cJSON_IsObject(x) : cJSON_IsString(x));
        if (!keep) cJSON_Delete(cJSON_DetachItemViaPointer(sec, x));
        x = next;
    }
}

bool translations_save(const char *code, const char *json, size_t n)
{
    n_errors = 0;
    ctx_t c = { 0 };
    if (!find_lang(code, &c.lang)) { fail("lang", NULL, -1, "no such language"); return false; }

    cJSON *in = json ? cJSON_ParseWithLength(json, n) : NULL;
    if (!cJSON_IsObject(in)) {
        cJSON_Delete(in);
        fail("body", NULL, -1, "not a JSON object");
        return false;
    }
    c.web_en = builtin_web(LANG_EN);
    c.web_base = builtin_web(c.lang);
    cJSON *doc = read_file(c.lang);
    if (!doc) doc = cJSON_CreateObject();
    cJSON *panel = section_of(doc, "panel");
    cJSON *web = section_of(doc, "web");
    if (!panel || !web) fail("body", NULL, -1, "out of memory");

    const cJSON *s;
    cJSON_ArrayForEach(s, in) {
        const bool is_panel = s->string && strcmp(s->string, "panel") == 0;
        const bool is_web = s->string && strcmp(s->string, "web") == 0;
        if ((!is_panel && !is_web) || !cJSON_IsObject(s)) {
            fail(s->string ? s->string : "body", NULL, -1, "not a section");
            continue;
        }
        const cJSON *val;
        cJSON_ArrayForEach(val, s)
            if (panel && web)
                merge_entry(&c, is_panel, s->string, is_panel ? panel : web, val);
    }

    bool ok = n_errors == 0;
    if (ok) {
        drop_unknown(&c, true, panel);
        drop_unknown(&c, false, web);
        char name[32];
        file_name(c.lang, name, sizeof name);
        if (!panel->child && !web->child) {
            if (archivio_esiste(name) && !archivio_cancella(name)) ok = false;
        } else {
            char *text = cJSON_PrintUnformatted(doc);
            ok = text && archivio_scrivi(name, text, strlen(text));
            cJSON_free(text);
        }
        if (!ok) fail("body", NULL, -1, "could not be stored: too large, or no space");
    }
    cJSON_Delete(in);
    cJSON_Delete(doc);
    cJSON_Delete(c.web_en);
    cJSON_Delete(c.web_base);
    return ok;
}

bool translations_reset(const char *code)
{
    lang_t l;
    if (!find_lang(code, &l)) return false;
    char name[32];
    file_name(l, name, sizeof name);
    return !archivio_esiste(name) || archivio_cancella(name);
}

/* --- export --------------------------------------------------------------- */

/* A growing text. Allocated by cJSON's allocator, like everything else
   this module returns, so that one free fits all. */
typedef struct {
    char  *p;
    size_t n, cap;
    bool   bad;
} out_t;

static void put_n(out_t *o, const char *s, size_t n)
{
    if (o->bad) return;
    if (o->n + n + 1 > o->cap) {
        size_t cap = o->cap ? o->cap : 16384;
        while (o->n + n + 1 > cap) cap *= 2;
        char *p = cJSON_malloc(cap);
        if (!p) { o->bad = true; return; }
        if (o->p) { memcpy(p, o->p, o->n); cJSON_free(o->p); }
        o->p = p;
        o->cap = cap;
    }
    memcpy(o->p + o->n, s, n);
    o->n += n;
    o->p[o->n] = 0;
}

static void put(out_t *o, const char *s) { put_n(o, s, strlen(s)); }

/* A JSON string as Python's json.dumps(ensure_ascii=False) writes it,
   which is how i18n/<code>.json is written: an exported file then differs
   from the project's only where a text does. */
static void put_string(out_t *o, const char *s)
{
    put(o, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  put(o, "\\\""); break;
        case '\\': put(o, "\\\\"); break;
        case '\n': put(o, "\\n"); break;
        case '\r': put(o, "\\r"); break;
        case '\t': put(o, "\\t"); break;
        case '\b': put(o, "\\b"); break;
        case '\f': put(o, "\\f"); break;
        default:
            if (*p < 0x20) {
                char esc[8];
                snprintf(esc, sizeof esc, "\\u%04x", *p);
                put(o, esc);
            } else {
                put_n(o, (const char *)p, 1);
            }
        }
    }
    put(o, "\"");
}

static int by_name(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* One section, keys sorted as json.dumps(sort_keys=True) sorts them. */
static void put_section(out_t *o, const ctx_t *c, bool panel, const char *name,
                        const char **keys, int n, const cJSON *custom, bool last)
{
    qsort(keys, (size_t)n, sizeof *keys, by_name);
    put(o, "  ");
    put_string(o, name);
    put(o, n ? ": {\n" : ": {}");
    for (int i = 0; i < n; i++) {
        ref_t r;
        if (!resolve(c, panel, keys[i], &r)) continue;
        const char *v[2];
        merged(panel, keys[i], &r, cJSON_GetObjectItemCaseSensitive(custom, keys[i]), v);
        put(o, "    ");
        put_string(o, keys[i]);
        put(o, ": ");
        if (!r.plural) {
            put_string(o, v[0]);
        } else {
            put(o, "{\n      \"one\": ");
            put_string(o, v[0]);
            put(o, ",\n      \"other\": ");
            put_string(o, v[1]);
            put(o, "\n    }");
        }
        put(o, i + 1 < n ? ",\n" : "\n");
    }
    if (n) put(o, "  }");
    put(o, last ? "\n" : ",\n");
}

char *translations_export(const char *code)
{
    ctx_t c;
    if (!find_lang(code, &c.lang)) return NULL;
    c.web_en = builtin_web(LANG_EN);
    c.web_base = builtin_web(c.lang);
    cJSON *doc = read_file(c.lang);

    const int n_web = cJSON_GetArraySize(c.web_en);
    const char **keys = cJSON_malloc(sizeof *keys *
                                     (size_t)(TX_COUNT + TXN_COUNT + n_web + 1));
    out_t o = { 0 };
    if (!keys) o.bad = true;
    else {
        const lang_info_t *li = &LANG_INFO[c.lang];
        const char decimal[2] = { li->decimal, 0 };
        put(&o, "{\n  \"_meta\": {\n    \"code\": ");
        put_string(&o, li->code);
        put(&o, ",\n    \"decimal\": ");
        put_string(&o, decimal);
        put(&o, ",\n    \"name\": ");
        put_string(&o, li->name);
        put(&o, "\n  },\n");

        int n = 0;
        for (int i = 0; i < TX_COUNT; i++) keys[n++] = TX_KEYS[i];
        for (int i = 0; i < TXN_COUNT; i++) keys[n++] = TXN_KEYS[i];
        /* A firmware whose page has no texts of its own has no "web" in
           its files either, and the export does not invent one. */
        put_section(&o, &c, true, "panel", keys, n,
                    cJSON_GetObjectItemCaseSensitive(doc, "panel"), n_web == 0);

        n = 0;
        const cJSON *e;
        cJSON_ArrayForEach(e, c.web_en) if (e->string) keys[n++] = e->string;
        if (n)
            put_section(&o, &c, false, "web", keys, n,
                        cJSON_GetObjectItemCaseSensitive(doc, "web"), true);
        put(&o, "}\n");
    }
    cJSON_free(keys);
    cJSON_Delete(doc);
    cJSON_Delete(c.web_en);
    cJSON_Delete(c.web_base);
    if (o.bad) { cJSON_free(o.p); return NULL; }
    return o.p;
}
