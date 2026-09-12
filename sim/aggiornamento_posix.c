/* ------------------------------------------------------------------------
 * Aggiornamento del firmware, sul PC — vedi aggiornamento.h.
 *
 * Non c'e una partizione da scrivere: c'e un file. Ma tutto il resto e vero,
 * ed e il motivo per cui questo porto esiste invece di lasciare l'OTA senza
 * prova. Quello che si vuole verificare non e che esp_ota_write funzioni —
 * quello lo garantisce Espressif — ma la **catena**: il caricamento che
 * arriva a pezzi, il conto dei byte, il rifiuto di un'immagine che non
 * convince, l'interruzione a meta che non lascia niente di installato, e il
 * giro di prova che si conferma solo se il pannello funziona davvero.
 *
 * Quella catena e nostra, e sbagliarla si paga con un pannello a muro che
 * non riparte.
 *
 * L'immagine non si verifica con una firma, che sul PC non avrebbe niente
 * da verificare: si controlla che cominci come un'immagine ESP-IDF. Basta a
 * far passare il caso "un file qualsiasi caricato per sbaglio", che e quello
 * che la prova deve poter distinguere da un aggiornamento buono.
 * --------------------------------------------------------------------- */
#include "aggiornamento.h"
#include "archivio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Il primo byte di un'immagine ESP-IDF. Non e una firma e non pretende di
   esserlo: e il minimo che distingue un firmware da un file di testo. */
#define MAGIA 0xE9

static FILE  *uscita;
static bool   in_corso;
static size_t scritti;
static size_t attesi;
static bool   magia_vista;
static char   motivo[96];

/* Dove finisce l'immagine sul PC: un file accanto ai dati, cosi si puo
   guardare cosa e arrivato. Accanto davvero — nella cartella dei dati, non
   in quella da cui si lancia il simulatore, che di solito e la radice del
   progetto. */
static char PERCORSO[300];

bool agg_apri(size_t totale)
{
    if (in_corso) {
        snprintf(motivo, sizeof motivo, "un aggiornamento e gia in corso");
        return false;
    }
    motivo[0] = 0;
    scritti = 0;
    attesi = totale;
    magia_vista = false;

    snprintf(PERCORSO, sizeof PERCORSO, "%s/aggiornamento.bin", archivio_dove());
    uscita = fopen(PERCORSO, "wb");
    if (!uscita) {
        snprintf(motivo, sizeof motivo, "non posso scrivere %.70s", PERCORSO);
        return false;
    }
    in_corso = true;
    return true;
}

bool agg_scrivi(const void *dati, size_t n)
{
    if (!in_corso) return false;
    if (!n) return true;

    /* Il primo byte si guarda al volo: e l'unica cosa che si puo dire di
       un'immagine mentre arriva ancora. */
    if (!scritti && n) magia_vista = ((const unsigned char *)dati)[0] == MAGIA;

    if (fwrite(dati, 1, n, uscita) != n) {
        snprintf(motivo, sizeof motivo, "scrittura fallita a %u byte",
                 (unsigned)scritti);
        fclose(uscita); uscita = NULL;
        in_corso = false;
        return false;
    }
    scritti += n;
    return true;
}

bool agg_chiudi(bool completo)
{
    if (!in_corso) return false;
    in_corso = false;
    if (uscita) { fclose(uscita); uscita = NULL; }

    if (!completo) {
        snprintf(motivo, sizeof motivo, "trasferimento interrotto a %u byte",
                 (unsigned)scritti);
        remove(PERCORSO);
        return false;
    }
    if (!magia_vista) {
        /* Il gemello sul pannello dice la stessa cosa quando la firma non
           torna, e la dice nello stesso momento: alla chiusura, prima che
           la partizione di avvio cambi. */
        snprintf(motivo, sizeof motivo,
                 "immagine rifiutata: non comincia come un firmware");
        remove(PERCORSO);
        return false;
    }
    if (attesi && scritti != attesi) {
        snprintf(motivo, sizeof motivo,
                 "arrivati %u byte su %u dichiarati",
                 (unsigned)scritti, (unsigned)attesi);
        remove(PERCORSO);
        return false;
    }
    return true;
}

const char *agg_motivo(void)  { return motivo; }
size_t      agg_scritti(void) { return scritti; }

/* Sul PC non si riavvia niente, quindi non si e mai in prova: il giro di
   prova si verifica con la prova dedicata, che chiama direttamente le due
   funzioni qui sotto. */
bool agg_in_prova(void)   { return false; }
void agg_conferma(void)   { }
void agg_rifiuta(void)    { }

const char *agg_versione(void)
{
#ifdef PANNELLO_VERSIONE
    return PANNELLO_VERSIONE;
#else
    return "simulatore";
#endif
}

