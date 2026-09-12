/* ------------------------------------------------------------------------
 * Translations edited on the panel — see main/cfg/translations.h.
 *
 * What is proved here is what the page relies on: an edit reaches tr(),
 * a bad one is refused whole with the reason, an edit equal to the
 * built-in text is not kept, and the export is the project's file with
 * the edits in it. Plus the case that only happens months later: an edit
 * the checks no longer accept, after an update changed the English text,
 * is left out instead of passed to printf.
 *
 * Real keys, like test_i18n.c: robot.clean_all (plain, no placeholder),
 * access.named_open ("%s open") and robot.alerts (plural).
 * --------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>

#include "archivio.h"
#include "cJSON.h"
#include "i18n.h"
#include "translations.h"

/* Le cartelle di appoggio stanno nella cartella di compilazione, che
   CMake passa in PROVE_DIR: sono prodotti come i binari, e nella radice
   del progetto facevano solo disordine. */
#ifndef PROVE_DIR
#define PROVE_DIR "."
#endif

static int failed;

static void check(const char *what, int ok)
{
    printf("%-64s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) failed++;
}

static void same(const char *what, const char *got, const char *want)
{
    const int ok = got && strcmp(got, want) == 0;
    printf("%-64s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) {
        printf("    got  \"%s\"\n    want \"%s\"\n", got ? got : "(null)", want);
        failed++;
    }
}

static bool save(const char *code, const char *json)
{
    return translations_save(code, json, strlen(json));
}

static bool has_error(const char *field, const char *reason_part)
{
    for (int n = 0; n < translations_errors(); n++) {
        const translation_error_t *e = translations_error(n);
        if (strcmp(e->field, field) == 0
                && (!reason_part || strstr(e->reason, reason_part)))
            return true;
    }
    return false;
}

/* The language the panel shows, as ui_avvia() sets it. */
static void show(const char *code)
{
    i18n_set(code);
    translations_apply();
}

/* A value in the editor's answer: panel.<key>.<field>. */
static const cJSON *editor_field(const cJSON *doc, const char *sec,
                                 const char *key, const char *field)
{
    const cJSON *s = cJSON_GetObjectItemCaseSensitive(doc, sec);
    const cJSON *k = cJSON_GetObjectItemCaseSensitive(s, key);
    return cJSON_GetObjectItemCaseSensitive(k, field);
}

static cJSON *editor(const char *code)
{
    char *s = translations_editor(code);
    cJSON *doc = s ? cJSON_Parse(s) : NULL;
    translations_free(s);
    return doc;
}

int main(void)
{
    archivio_radice(PROVE_DIR "/prova_dati_testi");
    translations_reset("en");
    translations_reset("it");

    printf("--- an edit reaches the panel ---\n");
    show("it");
    same("before any edit, the built-in text", tr(TX_ROBOT_CLEAN_ALL), "Pulisci tutto");
    check("an edit is saved",
          save("it", "{\"panel\":{\"robot.clean_all\":\"Pulisci ogni stanza\"}}"));
    same("...and does nothing until applied", tr(TX_ROBOT_CLEAN_ALL), "Pulisci tutto");
    show("it");
    same("...and is the text once applied", tr(TX_ROBOT_CLEAN_ALL), "Pulisci ogni stanza");
    show("en");
    same("another language keeps its own", tr(TX_ROBOT_CLEAN_ALL), "Clean everything");
    show("it");
    same("and coming back finds the edit again", tr(TX_ROBOT_CLEAN_ALL),
         "Pulisci ogni stanza");
    check("a second save keeps the first edit",
          save("it", "{\"panel\":{\"access.named_open\":\"%s spalancato\"}}"));
    show("it");
    same("...the new one", tr(TX_ACCESS_NAMED_OPEN), "%s spalancato");
    same("...and the old one", tr(TX_ROBOT_CLEAN_ALL), "Pulisci ogni stanza");

    printf("\n--- plural forms ---\n");
    check("one form alone is saved",
          save("it", "{\"panel\":{\"robot.alerts\":{\"one\":\"%d solo avviso\"}}}"));
    show("it");
    same("...and used for one", trn(TXN_ROBOT_ALERTS, 1), "%d solo avviso");
    same("...the other stays built-in", trn(TXN_ROBOT_ALERTS, 3), "%d avvisi");
    check("the other form, later",
          save("it", "{\"panel\":{\"robot.alerts\":{\"other\":\"%d avvisi aperti\"}}}"));
    show("it");
    same("...joins the first instead of replacing it", trn(TXN_ROBOT_ALERTS, 1),
         "%d solo avviso");
    same("...and is used", trn(TXN_ROBOT_ALERTS, 3), "%d avvisi aperti");

    printf("\n--- refused, whole ---\n");
    check("a swapped placeholder is refused",
          !save("it", "{\"panel\":{\"robot.clean_all\":\"ok\","
                      "\"access.named_open\":\"%d aperto\"}}"));
    check("...naming the text and the reason",
          has_error("panel.access.named_open", "placeholders"));
    show("it");
    same("...and nothing of it is kept, not even the good text",
         tr(TX_ROBOT_CLEAN_ALL), "Pulisci ogni stanza");
    check("a plural form, by form",
          !save("it", "{\"panel\":{\"robot.alerts\":{\"one\":\"un avviso\"}}}")
          && has_error("panel.robot.alerts.one", NULL));
    check("a character the fonts do not have",
          !save("it", "{\"panel\":{\"robot.clean_all\":\"\xe6\xbc\xa2\"}}")
          && has_error("panel.robot.clean_all", "fonts"));
    check("a key the panel does not have",
          !save("it", "{\"panel\":{\"robot.nope\":\"x\"}}")
          && has_error("panel.robot.nope", NULL));
    check("a text where plural forms are expected",
          !save("it", "{\"panel\":{\"robot.alerts\":\"%d avvisi\"}}")
          && has_error("panel.robot.alerts", NULL));
    check("a form that does not exist",
          !save("it", "{\"panel\":{\"robot.alerts\":{\"few\":\"%d\"}}}"));
    check("a keyboard without a letter",
          !save("it", "{\"panel\":{\"keyboard.letters\":\"qwertyuiop|asdfghjkl|zxcvbn\"}}")
          && has_error("panel.keyboard.letters", NULL));
    check("a section that does not exist",
          !save("it", "{\"screen\":{}}") && has_error("screen", NULL));
    check("not JSON", !save("it", "{\"panel\":") && has_error("body", NULL));
    check("a language the firmware does not have",
          !save("xx", "{\"panel\":{}}") && has_error("lang", NULL));
    char long_text[TRANSLATION_TEXT_MAX + 64];
    snprintf(long_text, sizeof long_text, "{\"panel\":{\"robot.clean_all\":\"");
    size_t l = strlen(long_text);
    memset(long_text + l, 'a', TRANSLATION_TEXT_MAX + 1);
    strcpy(long_text + l + TRANSLATION_TEXT_MAX + 1, "\"}}");
    check("a text longer than the limit", !save("it", long_text));
    check("the keyboard of another language is fine",
          save("it", "{\"panel\":{\"keyboard.letters\":\"qwertzuiop|asdfghjkl|yxcvbnm\"}}"));

    printf("\n--- what is not kept ---\n");
    check("null removes an edit",
          save("it", "{\"panel\":{\"access.named_open\":null,\"keyboard.letters\":null}}"));
    show("it");
    same("...and the built-in text is back", tr(TX_ACCESS_NAMED_OPEN), "%s aperto");
    check("an edit equal to the built-in text is saved",
          save("it", "{\"panel\":{\"robot.clean_all\":\"Pulisci tutto\"}}"));
    cJSON *ed = editor("it");
    check("...but not kept: nothing hides a later fix of the built-in",
          ed && !editor_field(ed, "panel", "robot.clean_all", "custom"));
    check("the other edits are still there",
          ed && editor_field(ed, "panel", "robot.alerts", "custom"));
    cJSON_Delete(ed);
    check("removing the last edits",
          save("it", "{\"panel\":{\"robot.alerts\":{\"one\":null,\"other\":null}}}"));
    check("...leaves no file", !archivio_esiste("texts-it.json"));

    printf("\n--- the editor ---\n");
    save("it", "{\"panel\":{\"robot.clean_all\":\"Pulisci ogni stanza\"}}");
    ed = editor("it");
    const cJSON *x = editor_field(ed, "panel", "robot.clean_all", "en");
    check("English as reference", cJSON_IsString(x) && strcmp(x->valuestring, "Clean everything") == 0);
    x = editor_field(ed, "panel", "robot.clean_all", "base");
    check("the built-in text", cJSON_IsString(x) && strcmp(x->valuestring, "Pulisci tutto") == 0);
    x = editor_field(ed, "panel", "robot.clean_all", "custom");
    check("the edit", cJSON_IsString(x) && strcmp(x->valuestring, "Pulisci ogni stanza") == 0);
    x = editor_field(ed, "panel", "robot.alerts", "base");
    check("plural keys as {one, other}", cJSON_IsObject(x)
          && cJSON_GetObjectItemCaseSensitive(x, "other"));
    check("the characters the fonts draw, for the page's own check",
          cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(ed, "glyphs")) == I18N_GLYPHS_N);
    check("every key of the panel is there",
          cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(ed, "panel"))
          == TX_COUNT + TXN_COUNT);
    cJSON_Delete(ed);
    ed = editor("en");
    check("English has no English reference, only its own text",
          ed && !editor_field(ed, "panel", "robot.clean_all", "en")
          && editor_field(ed, "panel", "robot.clean_all", "base"));
    cJSON_Delete(ed);
    check("an unknown language has no editor", translations_editor("xx") == NULL);

    printf("\n--- an edit the checks no longer accept ---\n");
    /* As a firmware update would leave it: the English text changed its
       placeholder after the edit was saved. Written by hand, since saving
       it is exactly what the checks prevent. */
    const char *old = "{\"panel\":{\"access.named_open\":\"%d aperto\","
                      "\"robot.clean_all\":\"Pulisci ogni stanza\","
                      "\"robot.gone\":\"x\"}}";
    archivio_scrivi("texts-it.json", old, strlen(old));
    show("it");
    same("it is not used", tr(TX_ACCESS_NAMED_OPEN), "%s aperto");
    same("the good ones next to it are", tr(TX_ROBOT_CLEAN_ALL), "Pulisci ogni stanza");
    ed = editor("it");
    check("the editor shows it, marked stale",
          ed && cJSON_IsTrue(editor_field(ed, "panel", "access.named_open", "stale")));
    check("...and only it",
          ed && !editor_field(ed, "panel", "robot.clean_all", "stale"));
    cJSON_Delete(ed);
    check("the next save forgets keys the firmware no longer has",
          save("it", "{\"panel\":{\"access.named_open\":null}}"));
    char buf[4096];
    size_t n = 0;
    check("...robot.gone is gone",
          archivio_leggi("texts-it.json", buf, sizeof buf, &n)
          && !strstr(buf, "robot.gone") && strstr(buf, "robot.clean_all"));

    printf("\n--- export ---\n");
    char *exp = translations_export("it");
    cJSON *doc = exp ? cJSON_Parse(exp) : NULL;
    check("the export is JSON", doc != NULL);
    check("in the project's format",
          exp && strncmp(exp, "{\n  \"_meta\": {\n    \"code\": \"it\",\n", 31) == 0
          && exp[strlen(exp) - 1] == '\n');
    const cJSON *panel = cJSON_GetObjectItemCaseSensitive(doc, "panel");
    check("every key", cJSON_GetArraySize(panel) == TX_COUNT + TXN_COUNT);
    x = cJSON_GetObjectItemCaseSensitive(panel, "robot.clean_all");
    check("with the edit", cJSON_IsString(x) && strcmp(x->valuestring, "Pulisci ogni stanza") == 0);
    x = cJSON_GetObjectItemCaseSensitive(panel, "access.named_open");
    check("and the built-in text elsewhere", cJSON_IsString(x) && strcmp(x->valuestring, "%s aperto") == 0);
    check("keys sorted, as json.dumps writes them",
          panel && panel->child && panel->child->next
          && strcmp(panel->child->string, panel->child->next->string) < 0);
    cJSON_Delete(doc);
    translations_free(exp);

    /* The file untouched is the file in the project: exporting English
       with no edits must give i18n/en.json back, byte for byte. */
    FILE *f = fopen("i18n/en.json", "rb");
    static char repo[256 * 1024];
    size_t repo_n = f ? fread(repo, 1, sizeof repo - 1, f) : 0;
    if (f) fclose(f);
    repo[repo_n] = 0;
    exp = translations_export("en");
    check("English without edits is i18n/en.json, byte for byte",
          exp && repo_n && strcmp(exp, repo) == 0);
    translations_free(exp);

    printf("\n--- the page's own texts ---\n");
    char *page = translations_page("it");
    doc = page ? cJSON_Parse(page) : NULL;
    x = cJSON_GetObjectItemCaseSensitive(doc, "lang");
    check("the language asked for", cJSON_IsString(x) && strcmp(x->valuestring, "it") == 0);
    check("the languages to choose from",
          cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(doc, "languages")) == LANG_COUNT);
    check("and a web section", cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(doc, "web")));
    cJSON_Delete(doc);
    translations_free(page);
    check("an unknown language has none", translations_page("xx") == NULL);

    printf("\n--- reset ---\n");
    check("a language's edits can be forgotten", translations_reset("it"));
    show("it");
    same("...and the built-in text is back", tr(TX_ROBOT_CLEAN_ALL), "Pulisci tutto");
    check("forgetting nothing is fine too", translations_reset("it"));
    check("but not for a language that does not exist", !translations_reset("xx"));

    printf("\n%s\n", failed ? "TESTS FAILED" : "all good");
    return failed ? 1 : 0;
}
