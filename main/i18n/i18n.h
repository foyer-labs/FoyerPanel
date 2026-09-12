/* ------------------------------------------------------------------------
 * What the panel says, in the language it was asked to speak.
 *
 * Every text on screen comes from here: `tr(TX_...)` for a plain text,
 * `trn(TXN_..., n)` for one that depends on a count. The texts themselves
 * live in i18n/<code>.json and are compiled into tables by
 * tools/gen_texts.py — see that script for what the build refuses.
 *
 * No dependency on LVGL or on the configuration, on purpose: the language
 * is *given* to this module (ui_avvia() reads it from `system.language`),
 * so the tables and the plural rules can be tested on their own.
 * --------------------------------------------------------------------- */
#ifndef I18N_H
#define I18N_H

#include <stdbool.h>
#include <stddef.h>

#include "texts_gen.h"

/* The text for `id` in the current language. Never NULL: a text missing
   in a translation falls back to English, which is always complete —
   tools/gen_texts.py --check fails the build before that can ship, but
   a wrong word is still better than an empty label. */
const char *tr(tx_t id);

/* The form of `id` that goes with `n`: "1 alert", "3 alerts". The text
   usually contains a %d or %ld, and the caller still passes `n` to printf:
   choosing the form and writing the number are separate steps, because
   some forms ("no alerts") do not show the number at all. */
const char *trn(txn_t id, long n);

/* Choose the language by its code ("en", "it", ...). Unknown or NULL
   codes choose English and return false, so a configuration written for
   a firmware with more languages still shows something readable. */
bool i18n_set(const char *code);

/* Pin a language regardless of what i18n_set() is told later. Only the
   simulator uses it, for `--lingua`: the configuration is re-read at every
   save, and captures must not change language halfway through. False,
   and English pinned, for a code that does not exist. */
bool i18n_force(const char *code);

lang_t i18n_lang(void);
const char *i18n_code(void);

/* The decimal separator of the current language: ',' or '.'. */
char i18n_decimal(void);

/* Replace '.' with the decimal separator in numbers just written by
   printf — "21.5" becomes "21,5" in Italian. Only a '.' between two digits
   changes, so an ellipsis or the end of a sentence is left alone; an IP
   address is not, so call it on formatted numbers, never on data. */
void i18n_decimals(char *s);

/* --- translations edited on the panel ------------------------------------
 *
 * The configuration page can change any text, in any language, and the
 * change is kept on the panel (main/cfg/translations.c). For the current
 * language those edits sit on top of the built-in tables: tr() and trn()
 * return them first.
 *
 * The string is not copied. Whoever sets it keeps it alive until the next
 * i18n_clear_overrides() — translations.c keeps the parsed file for that
 * long. NULL restores the built-in text. */
void i18n_override(tx_t id, const char *s);
void i18n_override_n(txn_t id, int form, const char *s);   /* form 0 one, 1 other */
void i18n_clear_overrides(void);

/* A key by its name in i18n/<code>.json: "robot.clean_all". -1 if the
   panel has no such plain (or plural) text. */
int i18n_key(const char *key);
int i18n_key_n(const char *key);

/* --- the checks a translation must pass -----------------------------------
 *
 * The same ones tools/gen_texts.py runs at build time, here because a
 * translation edited on the page never goes through the build. */

/* The printf conversions of `a` and `b` are the same, in the same order:
   "%s %d" translated as "%d %s" would be a string read as a number. */
bool i18n_same_printf(const char *a, const char *b);

/* The same {named} placeholders, in any order: the page's texts. */
bool i18n_same_named(const char *a, const char *b);

/* Valid UTF-8, and every character one the text fonts can draw. */
bool i18n_drawable(const char *s);

/* keyboard.letters: three rows of a-z separated by '|', all 26 letters. */
bool i18n_keyboard_ok(const char *s);

#endif /* I18N_H */
