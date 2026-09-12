/* ------------------------------------------------------------------------
 * Translations edited on the panel.
 *
 * Every text of the panel and of the configuration page can be changed
 * from the page, in any of the languages the firmware carries. The change
 * is kept on the panel, one file per language — texts-<code>.json — and
 * holds only what differs from the built-in text: a firmware update that
 * improves a translation still reaches every text nobody touched.
 *
 * The file has the shape of i18n/<code>.json, minus `_meta`:
 *
 *     { "panel": { "robot.clean_all": "Pulisci tutta la casa",
 *                  "robot.alerts": { "one": "...", "other": "..." } },
 *       "web":   { "section.home.title": "Casa" } }
 *
 * and translations_export() writes the whole language — built-in texts
 * with the edits on top — in exactly the format of i18n/<code>.json, so
 * that what was fixed on a panel goes back into the project as a file.
 *
 * Every text passes the checks of tools/gen_texts.py before it is saved
 * (i18n.h), and again before it is used: a firmware update can change the
 * placeholders of an English text, and an edit made before it would then
 * pass a string where a number is expected.
 * --------------------------------------------------------------------- */
#ifndef TRANSLATIONS_H
#define TRANSLATIONS_H

#include <stdbool.h>
#include <stddef.h>

/* The longest text accepted, in bytes. The page's longest help texts are
   a few hundred; this is a limit against mistakes, not a style guide. */
#define TRANSLATION_TEXT_MAX 1024

/* Load the current language's edits into tr(). Call it after i18n_set(),
   in the task that draws: tr() reads what this sets without a lock. A
   missing or unreadable file leaves the built-in texts. */
void translations_apply(void);

/* The texts the page shows itself in: {"lang","languages":[{code,name}],
   "web":{...}}, built-in with the edits on top. NULL for an unknown code.
   Free with translations_free(). */
char *translations_page(const char *code);

/* What the page's translation editor needs, for both sections: every key
   with its English text, the language's built-in one and the edit if
   there is one — {"panel":{"key":{"en":...,"base":...,"custom":...}}}.
   "en" is left out when the language is English. A "stale" edit is one
   that no longer passes the checks, and is not in use. "glyphs" are the
   ranges the fonts can draw, so the page can say so while typing. */
char *translations_editor(const char *code);

/* Merge `json` into the language's edits: the same shape as the file, a
   null removes an edit, and a text equal to the built-in one removes it
   too. For a plural key, the forms left out stay as they are. All or
   nothing: on any error the file is not touched, and translations_errors()
   says which texts and why. */
bool translations_save(const char *code, const char *json, size_t n);

/* Forget every edit of a language. */
bool translations_reset(const char *code);

/* The whole language as i18n/<code>.json. */
char *translations_export(const char *code);

void translations_free(char *s);

/* --- errors of the last save ---------------------------------------------- */

#define TRANSLATIONS_ERRORS_MAX 16

typedef struct {
    char field[80];      /* "panel.robot.alerts.one" */
    char reason[96];
} translation_error_t;

int                        translations_errors(void);
const translation_error_t *translations_error(int n);

#endif /* TRANSLATIONS_H */
