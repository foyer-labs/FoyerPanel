/* ------------------------------------------------------------------------
 * Prova della conversione in ore — vedi main/app/durata.h.
 *
 * Nasce da una cosa vista sul pannello vero: i consumabili di un robot
 * arrivavano alcuni in ore e alcuni in secondi, e comparivano cosi com'erano
 * — «214 h» accanto a «770400 s». A un metro e mezzo da un vetro non si
 * convertono a mente.
 *
 * I casi che contano sono tre, e nessuno dei tre e ovvio:
 *
 *   - **un tempo si porta in ore**, qualunque unita usi;
 *   - **quello che non e un tempo non si tocca**: inventare le ore da una
 *     percentuale darebbe un numero plausibile e falso, cioe quello che
 *     nessuno va a controllare;
 *   - **uno stato che non e un numero non vale zero.** «unknown» letto come
 *     zero farebbe scattare qualunque soglia, e il pannello annuncerebbe una
 *     spazzola finita perche' il sensore taceva.
 * --------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>

#include "durata.h"

static int falliti;

static void prova(const char *cosa, bool esito)
{
    printf("%-64s %s\n", cosa, esito ? "ok" : "FALLITA");
    if (!esito) falliti++;
}

static void testo(const char *stato, const char *unita, const char *atteso)
{
    char b[32];
    durata_testo(stato, unita, b, sizeof b);
    if (strcmp(b, atteso) == 0) {
        printf("  %-12s %-6s -> %-10s ok\n", stato, unita, b);
    } else {
        printf("  %-12s %-6s -> %-10s atteso \"%s\"   DIVERSO\n",
               stato, unita, b, atteso);
        falliti++;
    }
}

int main(void)
{
    double ore = -1;

    printf("--- un tempo si porta in ore ---\n");
    prova("i secondi diventano ore",
          durata_in_ore("770400", "s", &ore) && ore > 213.9 && ore < 214.1);
    prova("i minuti anche",
          durata_in_ore("120", "min", &ore) && ore > 1.99 && ore < 2.01);
    prova("le ore restano ore",
          durata_in_ore("214", "h", &ore) && ore > 213.9 && ore < 214.1);
    prova("i giorni diventano ore",
          durata_in_ore("2", "d", &ore) && ore > 47.9 && ore < 48.1);
    prova("e le forme per esteso valgono uguale",
          durata_in_ore("3600", "seconds", &ore) && ore > 0.99 && ore < 1.01);

    printf("\n--- quello che non e un tempo resta com'e ---\n");
    prova("una percentuale non e un tempo",
          !durata_in_ore("80", "%", &ore));
    prova("senza unita non si indovina",
          !durata_in_ore("80", "", &ore));
    /* `min` comincia per `m` come `mese`: se il confronto guardasse il
       primo carattere, i minuti diventerebbero qualcos'altro. */
    prova("un'unita sconosciuta non passa per un'altra",
          !durata_in_ore("5", "mesi", &ore));

    printf("\n--- uno stato che non e un numero non vale zero ---\n");
    prova("«unknown» non e zero", !durata_in_ore("unknown", "s", &ore));
    prova("«unavailable» nemmeno", !durata_in_ore("unavailable", "h", &ore));
    prova("e la stringa vuota nemmeno", !durata_in_ore("", "h", &ore));

    printf("\n--- il testo che si vede ---\n");
    testo("770400", "s", "214 h");
    testo("214", "h", "214 h");
    testo("7200", "s", "2 h");
    /* Sotto l'ora «0 h» direbbe una cosa sbagliata: sembra finito, e finito
       non e ancora. */
    testo("1800", "s", "<1 h");
    testo("0", "h", "<1 h");
    /* Non e un tempo: si mostra com'e, unita compresa. */
    testo("80", "%", "80 %");
    testo("unknown", "s", "unknown s");

    printf("\n%s\n", falliti ? "PROVE FALLITE" : "tutto a posto");
    return falliti ? 1 : 0;
}
