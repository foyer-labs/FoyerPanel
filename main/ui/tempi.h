/* ------------------------------------------------------------------------
 * Temporizzazioni e macchina a stati — 01-specifica-ui.md §5,
 * 05-architettura-firmware.md §8.
 *
 *   AVVIO ─→ HOME ⇄ SEZIONE ─→ MODALE
 *              │
 *              ├─ 120 s ─→ STANDBY ─ 600 s ─→ SPENTO
 *              └─ 60 s inattiva ─→ HOME        (schermata Wi-Fi: 120 s)
 *
 * Il timer di inattivita si azzera a ogni tocco, **anche su un'area inattiva
 * dello schermo**: per questo si legge da LVGL, che conta l'ultimo evento di
 * ingresso, invece di appendere un callback a ogni widget e dimenticarne uno.
 *
 * Standby e spegnimento sono sospesi con un modale aperto o con la
 * schermata Wi-Fi in primo piano.
 *
 * I valori qui sono i **predefiniti** della specifica. Quelli veri arrivano
 * da config.json (`schermo.*`, `condivisione_wifi.secondi_ritorno_home`) e
 * si rileggono a ogni battito: cambiarli dalla pagina ha effetto subito,
 * senza riavvio, perche' nessuno li tiene in una variabile.
 *
 * Sono rimasti dei `#define` e non sono diventati variabili apposta: sono il
 * ripiego quando la configurazione non dice niente, e un ripiego scritto
 * accanto alla soglia che governa si legge insieme a quella.
 * --------------------------------------------------------------------- */
#ifndef TEMPI_H
#define TEMPI_H

#include <stdbool.h>
#include <stdint.h>

/* 01-specifica-ui.md §5 — millisecondi. */
#define T_PRESSIONE_PROLUNGATA   1500
#define T_TIMEOUT_COMANDO        8000
#define T_CONFERMA_ANNULLO       8000
#define T_RISCONTRO_RIUSCITO     3000
#define T_RITORNO_HOME          60000
#define T_RITORNO_HOME_WIFI    120000
#define T_STANDBY              120000
#define T_SPOSTAMENTO_PIXEL    180000
#define T_SPEGNIMENTO          600000
#define T_RICONNESSIONE         15000
/* The boot screen: how often it looks, how long it stays once everything
   is up (long enough to read the last tick), and the most it waits. After
   that the panel starts anyway — the reconnection band and the "Home
   Assistant is not responding" screen say what is missing better than a
   list that stopped moving. */
#define T_AVVIO_CONTROLLO         250
#define T_AVVIO_FINITO            800
#define T_AVVIO_MASSIMO         20000

typedef enum {
    ST_AVVIO,
    ST_ATTIVO,     /* home o sezione, indistinguibili per le temporizzazioni */
    ST_STANDBY,
    ST_SPENTO,
} stato_t;

/* Avvia il conteggio. Da chiamare quando l'interfaccia e costruita. */
void tempi_avvia(void);

stato_t tempi_stato(void);

/* Da quanto nessuno tocca, in millisecondi. E il numero su cui si decide lo
   standby, e l'unico modo per sapere se non arriva perche il tempo non
   passa o perche qualcosa continua a svegliarlo. */
uint32_t tempi_fermo_ms(void);

/* Sospende standby, spegnimento e ritorno automatico: modale aperto o
   schermata Wi-Fi in primo piano. */
void tempi_sospendi(bool si);

/* Riporta allo stato attivo e azzera i conteggi. */
void tempi_risveglia(void);

/* Le soglie in vigore **adesso**, configurazione compresa. Le chiede la
   schermata delle impostazioni, che finora mostrava i `#define` e quindi
   diceva "120 s" anche a chi ne aveva scritti trenta. Spegnimento a 0 vuol
   dire "mai". */
uint32_t tempi_standby_ms(void);
uint32_t tempi_spegnimento_ms(void);

/* Vero mentre la regola notturna e in vigore. La schermata delle
   impostazioni la mostra: uno schermo che si spegne da solo alle undici
   deve poter dire perche', o sembra un guasto. */
bool tempi_e_notte(void);

/* Apre lo standby adesso, senza aspettare i due minuti di immobilita.

   Esiste per il simulatore: aspettare due minuti veri per guardare la
   schermata non e provarla, e catturarla sarebbe impossibile. Sul pannello
   non la chiama nessuno — li lo standby arriva quando deve. */
void tempi_forza_standby(void);

/* Butta la vista di riposo e la ricostruisce, se e aperta. Serve al
   salvataggio della configurazione: quella vista sta sul livello superiore,
   che ui_avvia() non ripulisce, quindi senza questa resterebbe quella di
   prima — e chi ha appena cambiato la grandezza dei caratteri vedrebbe non
   succedere niente. */
void tempi_rifai_standby(void);

#endif /* TEMPI_H */
