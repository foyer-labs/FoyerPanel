/* ------------------------------------------------------------------------
 * Prova della coda dei tocchi.
 *
 * Quello che si vuole dimostrare e una cosa sola, e non e "funziona": e che
 * **nessun tocco si perde**, per quanto tardi arrivi chi lo consuma. Sul
 * vetro un tocco perso si vede come "ha risposto al secondo", e da li non
 * si risale a niente — non si sa se il chip non l'ha visto, se e stato letto
 * in ritardo, o se e stato letto e buttato. Qui invece si sa: si mette in
 * fila una successione precisa e si guarda cosa esce.
 *
 * I casi che contano sono quelli in cui i due ritmi non coincidono, che sono
 * anche quelli che nessuno riesce a tenere in testa guardando il codice: due
 * tocchi dentro lo stesso buco, un dito che si alza mentre chi disegna e
 * ancora indietro di un evento, una lettura vuota in mezzo a un tocco.
 * --------------------------------------------------------------------- */
#include <stdio.h>

#include "tocco_eventi.h"

static int falliti;

static void prova(const char *cosa, bool esito)
{
    printf("%-62s %s\n", cosa, esito ? "ok" : "FALLITA");
    if (!esito) falliti++;
}

/* Consuma un evento e dice se e quello che ci si aspettava. */
static bool esce(bool giu, int16_t x, int16_t y)
{
    const tocco_evento_t e = tocco_prossimo();
    return e.giu == giu && e.x == x && e.y == y;
}

int main(void)
{
    /* --- un tocco normale, letto con calma --------------------------- */
    tocco_azzera();
    tocco_letto(0, 0, false);          /* niente sotto il dito */
    prova("da fermo non si accoda niente", tocco_in_attesa() == 0);

    tocco_letto(100, 50, true);
    prova("il dito che scende e una transizione", tocco_in_attesa() == 1);
    prova("e viene fuori dov'era", esce(true, 100, 50));

    tocco_letto(100, 50, true);
    tocco_letto(100, 50, true);
    prova("tenuto giu non accoda altro", tocco_in_attesa() == 0);
    prova("ma continua a risultare giu", esce(true, 100, 50));

    tocco_letto(0, 0, false);
    prova("il dito che si alza e una transizione", tocco_in_attesa() == 1);
    prova("e il rilascio porta l'ultimo punto valido, non lo zero del chip",
          esce(false, 100, 50));

    /* --- il caso per cui questo modulo esiste ------------------------- */
    /* Un tocco intero fra due letture di chi disegna. Con uno stato solo
       sarebbe sparito: al momento della lettura il dito e gia su. */
    tocco_azzera();
    tocco_letto(10, 20, true);
    tocco_letto(0, 0, false);
    prova("un tocco intero fra due letture lascia due transizioni",
          tocco_in_attesa() == 2);
    prova("  e la prima e la pressione", esce(true, 10, 20));
    prova("  e la seconda il rilascio, nello stesso punto", esce(false, 10, 20));

    /* --- due tocchi dentro lo stesso buco ---------------------------- */
    tocco_azzera();
    tocco_letto(1, 1, true);   tocco_letto(0, 0, false);
    tocco_letto(2, 2, true);   tocco_letto(0, 0, false);
    prova("due tocchi nello stesso buco sono quattro transizioni",
          tocco_in_attesa() == 4);
    prova("  giu sul primo",  esce(true, 1, 1));
    prova("  su sul primo",   esce(false, 1, 1));
    prova("  giu sul secondo", esce(true, 2, 2));
    prova("  su sul secondo",  esce(false, 2, 2));
    prova("e dopo si resta fermi sull'ultimo", esce(false, 2, 2));

    /* --- il dito che si muove mentre e giu --------------------------- */
    tocco_azzera();
    tocco_letto(5, 5, true);
    esce(true, 5, 5);                  /* consumato */
    tocco_letto(9, 9, true);
    prova("muovendosi non accoda", tocco_in_attesa() == 0);
    prova("ma la posizione segue il dito", esce(true, 9, 9));
    tocco_letto(0, 0, false);
    prova("e il rilascio e dove si e staccato", esce(false, 9, 9));

    /* --- la coda piena ------------------------------------------------ */
    /* Non deve corrompersi ne bloccarsi: butta e lo dice. Se questo numero
       fosse diverso da zero sul pannello, il difetto non sarebbe qui —
       sarebbe in quanto tempo passa fra due letture. */
    tocco_azzera();
    for (int n = 0; n < 20; n++) tocco_letto((int16_t)n, 0, n % 2 == 0);
    prova("la coda piena si ferma al suo tetto", tocco_in_attesa() == 8);
    prova("e conta quelle buttate", tocco_perse() == 12);
    bool ordinati = true;
    for (int n = 0; n < 8; n++) {
        const tocco_evento_t e = tocco_prossimo();
        if (e.giu != (n % 2 == 0)) ordinati = false;
    }
    prova("quelle rimaste sono in ordine, non mescolate", ordinati);

    /* --- azzerare azzera davvero ------------------------------------- */
    tocco_letto(3, 3, true);
    tocco_azzera();
    prova("azzerando non resta niente in coda", tocco_in_attesa() == 0);
    prova("ne il conto delle perse", tocco_perse() == 0);
    prova("e si riparte da rilasciato", esce(false, 0, 0));

    printf("\n%s\n", falliti ? "PROVE FALLITE" : "nessun tocco si perde");
    return falliti ? 1 : 0;
}
