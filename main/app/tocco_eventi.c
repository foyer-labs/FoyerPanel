/* ------------------------------------------------------------------------
 * La coda delle transizioni del dito — vedi tocco_eventi.h per il perche.
 *
 * Nessun blocco qui dentro. Chi legge il chip e chi disegna girano in due
 * compiti diversi, e la protezione la mette **chi chiama** — sul pannello e
 * uno spinlock, che intorno a queste funzioni dura pochi cicli. Metterla
 * qui vorrebbe dire portarsi dentro un tipo di FreeRTOS e rendere il modulo
 * incompilabile sul PC, cioe non provabile: e proprio la cosa che questo
 * modulo esiste per essere.
 * --------------------------------------------------------------------- */
#include "tocco_eventi.h"

/* Otto transizioni sono quattro tocchi completi. Fra due letture di chi
   disegna non ne succedono mai cosi tanti: se la coda si riempie il difetto
   e altrove, ed e per questo che si contano quelle perse invece di
   ingrandire la coda. */
#define CODA_MAX 8

static tocco_evento_t coda[CODA_MAX];
static uint8_t  primo, quanti;
static uint16_t perse;

/* Lo stato in cui si e rimasti dopo aver consumato tutto: e quello che si
   continua a rispondere finche non succede altro. */
static tocco_evento_t fermo;

/* Quello che il chip ha detto l'ultima volta: serve a riconoscere i cambi,
   ed e diverso da `fermo`, che e quello che si e gia raccontato. */
static bool giu_chip;

void tocco_azzera(void)
{
    primo = quanti = 0;
    perse = 0;
    fermo = (tocco_evento_t){ 0, 0, false };
    giu_chip = false;
}

static void metti(int16_t x, int16_t y, bool giu)
{
    if (quanti >= CODA_MAX) { perse++; return; }
    coda[(primo + quanti) % CODA_MAX] = (tocco_evento_t){ x, y, giu };
    quanti++;
}

void tocco_letto(int16_t x, int16_t y, bool giu)
{
    if (giu == giu_chip) {
        /* Nessun cambio. Se il dito e giu e si e mosso, la posizione nuova
           serve comunque a chi trascina: si aggiorna quella su cui si
           risponde, senza accodare niente. Accodare ogni lettura
           riempirebbe la coda in un decimo di secondo e farebbe buttare via
           proprio le transizioni, che sono le uniche che contano. */
        if (giu) { fermo.x = x; fermo.y = y; }
        return;
    }
    giu_chip = giu;

    /* Sul rilascio le coordinate sono quelle dell'ultimo punto valido, non
       quelle che il chip riporta adesso: da rilasciato non riporta niente di
       sensato, e chi disegna deve sapere **dove** il dito si e staccato per
       decidere se il tocco vale. */
    if (giu) metti(x, y, true);
    else     metti(fermo.x, fermo.y, false);

    if (giu) { fermo.x = x; fermo.y = y; }
}

tocco_evento_t tocco_prossimo(void)
{
    if (quanti) {
        fermo = coda[primo];
        primo = (uint8_t)((primo + 1) % CODA_MAX);
        quanti--;
    }
    return fermo;
}

int      tocco_in_attesa(void) { return quanti; }
uint16_t tocco_perse(void)     { return perse; }
