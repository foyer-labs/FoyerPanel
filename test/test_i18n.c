/* ------------------------------------------------------------------------
 * The C side of the languages — see main/i18n/i18n.h.
 *
 * tools/gen_texts.py --check already proves the tables are complete and
 * consistent. What it cannot prove is that the panel *picks* the right
 * entry: the plural form for a count, English for a code it does not
 * know, the decimal separator of the language. Those are decisions made
 * at run time, and a wrong one shows a correct word in the wrong place.
 *
 * Keys are real ones on purpose: a test with keys of its own would pass
 * on tables that do not exist on the panel.
 * --------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>

#include "i18n.h"

static int failed;

static void check(const char *what, int ok)
{
    printf("%-60s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) failed++;
}

static void same(const char *what, const char *got, const char *want)
{
    const int ok = strcmp(got, want) == 0;
    printf("%-60s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) {
        printf("    got  \"%s\"\n    want \"%s\"\n", got, want);
        failed++;
    }
}

int main(void)
{
    printf("--- choosing a language ---\n");
    check("English is the language before anyone chooses",
          i18n_lang() == LANG_EN);
    check("a known code is accepted", i18n_set("it"));
    check("...and is the language now", i18n_lang() == LANG_IT);
    same("...and the texts follow", tr(TX_ROBOT_CLEAN_ALL), "Pulisci tutto");
    check("an unknown code is refused", !i18n_set("xx"));
    check("...and falls back to English, not to the last language",
          i18n_lang() == LANG_EN);
    check("NULL is refused the same way", !i18n_set(NULL) && i18n_lang() == LANG_EN);

    printf("\n--- plural forms ---\n");
    i18n_set("en");
    same("1 is singular in English", trn(TXN_ROBOT_ALERTS, 1), "%d alert");
    same("2 is plural", trn(TXN_ROBOT_ALERTS, 2), "%d alerts");
    same("0 is plural in English", trn(TXN_ROBOT_ALERTS, 0), "%d alerts");
    i18n_set("it");
    same("1 is singular in Italian", trn(TXN_ROBOT_ALERTS, 1), "%d avviso");
    same("0 is plural in Italian", trn(TXN_ROBOT_ALERTS, 0), "%d avvisi");
    /* French says "0 alerte" — zero is singular. The rule is already in
       i18n.c; its line here comes with i18n/fr.json. */

    printf("\n--- pinning a language ---\n");
    check("a pinned language is accepted", i18n_force("it"));
    i18n_set("en");
    check("...and survives a later i18n_set()", i18n_lang() == LANG_IT);
    check("pinning an unknown code says so", !i18n_force("xx"));
    check("...and pins English", i18n_lang() == LANG_EN);
    i18n_force("en");

    printf("\n--- decimal separators ---\n");
    char b[32];
    i18n_force("it");
    strcpy(b, "21.5");       i18n_decimals(b); same("Italian writes 21,5", b, "21,5");
    strcpy(b, "3.42 kW");    i18n_decimals(b); same("with a unit after it", b, "3,42 kW");
    strcpy(b, "Wait...");    i18n_decimals(b); same("an ellipsis is not a number", b, "Wait...");
    strcpy(b, "1.5 / 2.0");  i18n_decimals(b); same("every number, not only the first", b, "1,5 / 2,0");
    i18n_force("en");
    strcpy(b, "21.5");       i18n_decimals(b); same("English keeps 21.5", b, "21.5");

    printf("\n--- the checks a translation edited on the page passes ---\n");
    /* The same rules as tools/gen_texts.py: a text the build accepts must
       be accepted here, and one it refuses refused. */
    check("same conversions, same order", i18n_same_printf("%s: %d", "%s — %d"));
    check("swapped conversions are refused", !i18n_same_printf("%s %d", "%d %s"));
    check("a missing one is refused", !i18n_same_printf("%d open", "open"));
    check("the whole spec counts: %.1f is not %.2f",
          !i18n_same_printf("%.1f", "%.2f"));
    check("%% is a conversion too", !i18n_same_printf("100%%", "100%"));
    check("a '%' that starts no conversion is text", i18n_same_printf("100%", "100"));
    check("...even next to one", i18n_same_printf("%d%", "%d"));
    check("but \"% o\" is one — space flag, octal — as PRINTF_RE says",
          !i18n_same_printf("50% off", "50 off"));
    check("{names} in any order", i18n_same_named("{a} of {b}", "{b}: {a}"));
    check("a missing {name} is refused", !i18n_same_named("{a} of {b}", "{a}"));
    check("a renamed one too", !i18n_same_named("{count}", "{n}"));
    check("{{ is not a name, {a} after it is",
          i18n_same_named("{{a}", "{a}") && i18n_same_named("{A}", "x"));
    check("Latin-1, euro and dashes are drawable",
          i18n_drawable("è à ç ß — € “ok” …"));
    check("a CJK character is not", !i18n_drawable("\xe6\xbc\xa2"));
    check("broken UTF-8 is not", !i18n_drawable("\xc3"));
    check("a tab is not (the fonts have no glyph)", !i18n_drawable("a\tb"));
    check("a newline is", i18n_drawable("a\nb"));
    check("QWERTZ is a keyboard", i18n_keyboard_ok("qwertzuiop|asdfghjkl|yxcvbnm"));
    check("AZERTY too", i18n_keyboard_ok("azertyuiop|qsdfghjklm|wxcvbn"));
    check("a missing letter is not", !i18n_keyboard_ok("qwertyuiop|asdfghjkl|zxcvbn"));
    check("two rows are not", !i18n_keyboard_ok("qwertyuiopasdfghjkl|zxcvbnm"));
    check("an empty row is not", !i18n_keyboard_ok("qwertyuiopasdfghjklzxcvbnm||"));
    check("capitals are not", !i18n_keyboard_ok("QWERTYUIOP|asdfghjkl|zxcvbnm"));

    printf("\n--- edits on top of the tables ---\n");
    i18n_force("it");
    i18n_override(TX_ROBOT_CLEAN_ALL, "Pulisci ogni stanza");
    same("an edit wins over the table", tr(TX_ROBOT_CLEAN_ALL), "Pulisci ogni stanza");
    i18n_override_n(TXN_ROBOT_ALERTS, 0, "un solo avviso");
    same("a plural form too", trn(TXN_ROBOT_ALERTS, 1), "un solo avviso");
    same("...and only that form", trn(TXN_ROBOT_ALERTS, 3), "%d avvisi");
    i18n_override(TX_ROBOT_CLEAN_ALL, NULL);
    same("NULL gives the table back", tr(TX_ROBOT_CLEAN_ALL), "Pulisci tutto");
    i18n_clear_overrides();
    same("clearing gives the plural back", trn(TXN_ROBOT_ALERTS, 1), "%d avviso");
    check("keys by name", i18n_key("robot.clean_all") == TX_ROBOT_CLEAN_ALL
          && i18n_key_n("robot.alerts") == TXN_ROBOT_ALERTS);
    check("...and not across kinds", i18n_key("robot.alerts") < 0
          && i18n_key_n("robot.clean_all") < 0 && i18n_key("nope") < 0);
    i18n_force("en");

    printf("\n--- ids out of range ---\n");
    same("an id past the table is an empty text, not a crash",
         tr((tx_t)TX_COUNT), "");
    same("the same for plural ids", trn((txn_t)TXN_COUNT, 3), "");

    printf("\n%s\n", failed ? "TESTS FAILED" : "all good");
    return failed ? 1 : 0;
}
