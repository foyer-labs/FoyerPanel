/* ------------------------------------------------------------------------
 * Configurazione all'avvio del simulatore.
 *
 * Sul pannello la cartella e la partizione LittleFS e chi la riempie e la
 * pagina web al primo avvio. Qui la cartella e `dati/` e la prima volta ci
 * si mette dentro la configurazione d'esempio, quella della casa vera: il
 * simulatore deve mostrare qualcosa appena si lancia, senza chiedere niente.
 *
 * Copiarla e non leggerla dal suo posto in docs/ non e un dettaglio: da quel
 * momento e un file dell'archivio come lo sarebbe sul pannello, si puo
 * modificare, si riscrive col salvataggio atomico, e le prove che seguono
 * partono dalla stessa situazione di un apparecchio gia configurato.
 *
 * Con --vuoto l'archivio si svuota e si vede quello che vede un pannello
 * appena tolto dalla scatola: il primo avvio.
 * --------------------------------------------------------------------- */
#include "sim.h"

#include <stdio.h>
#include <string.h>

#include "archivio.h"
#include "config.h"
#include "segreti.h"

/* Il percorso della configurazione d'esempio arriva da CMake, cosi il
   simulatore si lancia da qualunque cartella. */
#ifndef SIM_CONFIG_ESEMPIO
#define SIM_CONFIG_ESEMPIO "docs/04-config.example.json"
#endif

static bool semina(void)
{
    FILE *f = fopen(SIM_CONFIG_ESEMPIO, "rb");
    if (!f) {
        fprintf(stderr, "configurazione d'esempio non trovata: %s\n",
                SIM_CONFIG_ESEMPIO);
        return false;
    }

    static char buf[ARCHIVIO_MAX];
    const size_t n = fread(buf, 1, sizeof buf, f);
    const bool troppo = !feof(f);
    fclose(f);
    if (troppo) {
        fprintf(stderr, "la configurazione d'esempio supera %d byte\n",
                ARCHIVIO_MAX);
        return false;
    }

    return archivio_scrivi(CFG_FILE, buf, n);
}

esito_cfg_t sim_configura(const sim_opzioni_t *o)
{
    archivio_radice(o->dati ? o->dati : "dati");
    segreti_avvia();

    /* --segreto NOME=VALORE, per le prove e le catture. Non c'e niente di
       simile sul pannello: li i segreti arrivano dal primo avvio o dalla
       pagina web, e non esiste un modo per scriverli senza passare da
       quelle due strade. */
    for (int n = 0; n < o->quanti_segreti; n++) {
        const char *uguale = strchr(o->segreti[n], '=');
        if (!uguale) {
            fprintf(stderr, "--segreto vuole NOME=VALORE\n");
            continue;
        }
        char nome[32];
        const size_t l = (size_t)(uguale - o->segreti[n]);
        if (l >= sizeof nome) continue;
        memcpy(nome, o->segreti[n], l);
        nome[l] = 0;

        const segreto_t s = segreto_da_nome(nome);
        if (s == SEG_QUANTI) {
            fprintf(stderr, "segreto sconosciuto: %s\n", nome);
            continue;
        }
        segreti_scrivi(s, uguale + 1);
    }

    if (o->vuoto) {
        archivio_cancella(CFG_FILE);
        archivio_cancella(CFG_COPIA);
        segreti_azzera();
    } else if (!archivio_esiste(CFG_FILE)) {
        semina();
    }

    const esito_cfg_t esito = cfg_carica();

    /* Il perche di quello che si vedra: una configurazione ripescata dalla
       copia o rifiutata cambia mezza interfaccia, e senza una riga qui si
       passerebbe il tempo a chiedersi perche mancano le telecamere. */
    switch (esito) {
    case CFG_LETTA:
        break;
    case CFG_DA_COPIA:
        printf("config.json non andava: si e ripartiti dalla copia\n");
        break;
    case CFG_PREDEFINITA:
        printf("nessuna configurazione in %s/: primo avvio\n",
               archivio_dove());
        break;
    case CFG_TROPPO_NUOVA:
        printf("config.json ha uno schema piu nuovo del firmware: "
               "sola lettura\n");
        break;
    }

    for (int n = 0; n < cfg_errori(); n++) {
        const errore_cfg_t *e = cfg_errore(n);
        printf("  %s %s: %s\n", e->gravita == CFG_ERRORE ? "errore" : "avviso",
               e->campo, e->motivo);
    }

    return esito;
}
