/* ------------------------------------------------------------------------
 * Simulatore — solo PC.
 *
 * Questo file e i suoi .c non finiscono mai sul pannello: qui stanno le
 * comodita di sviluppo (scelta del profilo all'avvio, scorciatoie per i casi
 * limite) che sul muro non hanno senso.
 *
 * Le misure della finestra del simulatore stanno qui e non in profile.h:
 * non sono interfaccia del pannello, sono l'attrezzo con cui la si guarda.
 * --------------------------------------------------------------------- */
#ifndef SIM_H
#define SIM_H

#include <stdbool.h>

#include "config.h"

/* --- misure della sola finestra di scelta, non del pannello -------------- */
#define SIM_SCELTA_W   560
#define SIM_SCELTA_H   380
#define SIM_SCELTA_PAD 24
#define SIM_SCELTA_GAP 12
#define SIM_VOCE_H     52

typedef struct {
    const char *profilo;   /* NULL: quello predefinito           */
    bool        scegli;    /* apre il selettore dei quattro      */
    const char *dati;      /* cartella dell'archivio             */
    bool        vuoto;     /* parte senza configurazione         */
    bool        prova_struttura;
    const char *segreti[8];  /* NOME=VALORE, solo per le prove */
    int         quanti_segreti;
    int         pagina;    /* pagina iniziale, per le catture */
    int         web;       /* porta del server, 0 = spento    */
    bool        sblocca;   /* parte gia sbloccato, per le prove */
    bool        ha;        /* si collega a Home Assistant       */
    bool        prova_sorveglianza;
    const char *cattura;   /* file BMP: disegna un fotogramma ed esce */
    const char *sezione;   /* chiave della sezione da aprire all'avvio */
    const char *caso;      /* caso limite di 11-collaudo.md §2 da simulare */
    const char *lingua;    /* language code, overrides sistema.lingua */
    int         vista;     /* vista dentro la sezione, 0 = predefinita */
    const char *stato;     /* stato da mostrare: avvio, riconnessione,
                              ha-giu, conferma */
    int         prova_heap; /* giri di navigazione; 0 = non provare */
    bool        prova_tocco;
    bool        prova_tempi;/* verifica le temporizzazioni ed esce */
    bool        elenca;    /* stampa i profili ed esce           */
    bool        fps;       /* mostra il contatore di LVGL        */
} sim_opzioni_t;

/* Legge la riga di comando. Falso se c'e da uscire subito (aiuto o errore);
   `esito` porta il codice di uscita. */
bool sim_opzioni_leggi(int argc, char **argv, sim_opzioni_t *o, int *esito);

/* Apre una finestra con i profili e restituisce la chiave scelta,
   oppure NULL se la finestra e stata chiusa. */
const char *sim_scegli_profilo(void);

/* Disegna un fotogramma fuori schermo e lo scrive in un BMP. Nessuna
   finestra: funziona anche dove non c'e uno schermo. */
int sim_cattura(const char *percorso, const char *sezione, int vista,
                const char *stato);

/* Mostra uno stato di 01-specifica-ui.md §4. */
void sim_applica_stato(const char *nome, int vista);

/* Prepara l'archivio e carica config.json. Da chiamare prima di ui_avvia():
   la struttura dell'interfaccia viene dalla configurazione. */
esito_cfg_t sim_configura(const sim_opzioni_t *o);

/* Stampa la struttura che l'interfaccia ha ricavato dalla
   configurazione. La confronta col documento tools/prova_struttura.py. */
int sim_prova_struttura(void);

/* Le tre soglie del collegamento, coi due numeri dati a mano. */
int sim_prova_sorveglianza(void);

/* Naviga tutte le schermate `giri` volte e controlla che l'heap non sia
   cresciuto oltre il 2%: e il criterio di 11-collaudo.md §1. */
int sim_prova_heap(int giri);

/* Preme davvero i passaggi dell'interfaccia e verifica che il comando
   arrivi dove deve. Zero se arrivano tutti. Vedi sim_tocco.c. */
int sim_prova_tocco(void);

/* Fa scorrere il tempo e verifica i criteri di 11-collaudo.md §1 sulle
   temporizzazioni: ritorno alla home, standby, spegnimento, risveglio. */
int sim_prova_tempi(void);

#endif /* SIM_H */
