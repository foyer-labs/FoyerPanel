/* ------------------------------------------------------------------------
 * Simulatore del pannello — LVGL 9 su SDL2.
 *
 *     ./pannello                          parte sul profilo predefinito
 *     ./pannello --profilo p4-1280x800    parte su quello scelto
 *     ./pannello --scegli                 chiede quale profilo
 *
 * Il pannello vero compila lo stesso codice di ui/ con un profilo solo,
 * scelto alla compilazione. Qui ci sono tutti e due e si sceglie
 * all'avvio: e la differenza fra un attrezzo e un apparecchio.
 * --------------------------------------------------------------------- */
#include <stdio.h>
#include <time.h>

#include "lvgl.h"
#include "dati.h"
#include "sistema.h"
#include "impronta.h"
#include "profile.h"
#include "schermate.h"
#include "ha.h"
#include "ws.h"
#include "sezioni.h"
#include "config.h"
#include "i18n.h"
#include "sorveglianza.h"
#include "sim.h"
#include "stati.h"
#include "ui.h"
#include "web.h"
#include "versione.h"

/* Cosa succede quando la pagina web salva: si rilegge quello che viene
   dalla configurazione e si ridisegna dove si era. Nessun riavvio, nessuno
   schermo nero, e chi sta guardando la sezione Luci vede comparire la zona
   che ha appena aggiunto dal telefono. */
static void configurazione_cambiata(void)
{
    /* Un indirizzo o un token nuovi vanno provati subito: e l'unico modo
       per uscire da "token rifiutato", che apposta non si sblocca da solo.
       ha_riprova() non fa niente se Home Assistant non e mai stato avviato,
       quindi il simulatore senza --ha resta com'e. */
    ha_riprova();
    dati_ricarica();
    sezioni_azzera();
    /* Le sezioni decidono cosa c'e nel rail, e il rail si costruisce
       all'accensione: rifare il solo contenuto lo lascerebbe indietro. */
    ui_avvia();
}

/* A translation was saved: the screens again, and nothing else. */
static void testi_cambiati(void)
{
    sezioni_azzera();
    ui_avvia();
}

int main(int argc, char **argv)
{
    sim_opzioni_t opz;
    int esito;

    lv_init();

    if (!sim_opzioni_leggi(argc, argv, &opz, &esito)) {
        lv_deinit();
        return esito;
    }

    /* Senza indicazioni si parte sul pannello che c'e davvero in casa:
       il verticale appeso al muro. Il selettore resta a un'opzione di
       distanza. */
    if (opz.scegli) {
        const char *chiave = sim_scegli_profilo();
        if (!chiave) return 0;
        profilo_scegli(chiave);
    } else if (!opz.profilo) {
        profilo_scegli(PANNELLO_PROFILO_PREDEFINITO);
    }

    /* Prima di tutto il resto: la struttura dell'interfaccia — quali zone,
       quali sezioni, quali accessi — viene da config.json, non dal
       codice. Anche le catture e le prove partono da qui, altrimenti
       misurerebbero un pannello diverso da quello che si vede. */
    const esito_cfg_t esito_cfg = sim_configura(&opz);

    /* Pinned, not just set: ui_avvia() reads sistema.lingua again at every
       configuration save, and a capture that switched language halfway
       through would compare two different screens. Before the case below:
       reading the data already writes words — "since 17:40" under a
       person — and a case applied first wrote them in the language of the
       moment, English in an Italian capture. */
    if (opz.lingua && !i18n_force(opz.lingua)) {
        fprintf(stderr, "--lingua %s: no such language\n", opz.lingua);
        return 2;
    }

    /* Il caso limite si applica **dopo**: dati_caso() rilegge la
       configurazione, e prima che ci sia conterebbe zero zone e zero
       condizionatori. */
    if (opz.caso) dati_caso(dati_caso_da_nome(opz.caso));

    /* Un pannello senza configurazione non mostra una home vuota: mostra il
       primo avvio, ed e l'unica cosa che sa fare finche non gli si dice
       dov'e Home Assistant. Vale anche per le catture, altrimenti --vuoto
       fotograferebbe una schermata che sul muro non si vedrebbe mai. */
    if (esito_cfg == CFG_PREDEFINITA && !opz.stato && !opz.sezione)
        opz.stato = "primo-avvio";

    if (opz.prova_sorveglianza) return sim_prova_sorveglianza();
    if (opz.prova_struttura) return sim_prova_struttura();
    if (opz.prova_tempi) return sim_prova_tempi();
    if (opz.prova_tocco) return sim_prova_tocco();
    if (opz.prova_heap) return sim_prova_heap(opz.prova_heap);

    /* Cattura: nessuna finestra, un fotogramma su file e via. */
    if (opz.pagina) schermate_pagina(opz.pagina);

    /* Il collegamento a Home Assistant: sul pannello si apre sempre, qui
       solo con --ha. Senza, dati_dal_vero() resta falso e le schermate
       mostrano i valori inventati che servono alle catture. */
    if (opz.ha) {
        ha_avvia();

        /* Con --cattura non c'e un ciclo principale che faccia avanzare il
           protocollo: si aspetta qui la fotografia iniziale, se no si
           fotograferebbe un pannello che non ha ancora sentito niente. Tre
           secondi bastano su una rete locale; se non arriva, si cattura lo
           stato di attesa, che e comunque la verita. */
        /* nanosleep e non lv_delay_ms: senza un display SDL non c'e
           nessuna sorgente di tick, e lv_delay_ms aspetterebbe un orologio
           che non parte mai. Un'attesa che non finisce dentro il percorso
           delle catture vuol dire che tools/cattura.py si pianta. */
        if (opz.cattura)
            for (int n = 0; n < 600 && ha_stato() != HA_PRONTO; n++) {
                ha_gira((uint32_t)n * 5);
                const struct timespec t = { .tv_sec = 0, .tv_nsec = 5000000 };
                nanosleep(&t, NULL);
            }

        printf("Home Assistant: %s%s%s\n", ha_stato_nome(ha_stato()),
               ha_motivo()[0] ? " — " : "", ha_motivo());
        dati_ricarica();
    }

    if (opz.cattura)
        return sim_cattura(opz.cattura, opz.sezione, opz.vista, opz.stato);

    printf("Foyer Panel %s — profile %s (%ux%u, %s)\n",
           PANNELLO_VERSIONE, PRF->chiave,
           PRF->schermo.larghezza, PRF->schermo.altezza,
           PRF->orientamento == VERTICALE ? "portrait" : "landscape");

    /* La finestra ha esattamente la risoluzione del vetro. Nessun
       adattamento, nessuna scala: quello che si vede qui e quello che si
       vedra sul muro, alla stessa densita di pixel. */
    lv_display_t *disp = lv_sdl_window_create(PRF->schermo.larghezza,
                                              PRF->schermo.altezza);
    static char titolo[96];
    lv_snprintf(titolo, sizeof titolo, "Foyer Panel %s — %s", PANNELLO_VERSIONE,
                PRF->chiave);
    lv_sdl_window_set_title(disp, titolo);
    lv_sdl_mouse_create();

    /* LV_USE_PERF_MONITOR lo accende da solo: senza --fps si toglie. */
    if (opz.fps) lv_sysmon_show_performance(disp);
    else         lv_sysmon_hide_performance(disp);

    /* Il server: ascolta subito perche /api/status e /api/log rispondono
       sempre, ma tutto il resto e 404 finche non si tocca l'interruttore in
       Impostazioni. --sblocca lo apre subito, e esiste solo qui. */
    if (opz.web) {
        web_quando_cambia(configurazione_cambiata);
        web_when_texts_change(testi_cambiati);
        if (web_avvia(opz.web)) {
            printf("configurazione su http://localhost:%d/ — %s\n", opz.web,
                   opz.sblocca ? "sbloccata"
                               : "chiusa: sbloccala da Impostazioni");
            if (opz.sblocca) web_sblocca();
        } else {
            fprintf(stderr, "porta %d occupata: server non avviato\n", opz.web);
        }
    }

    /* Il collegamento a Home Assistant: sul pannello si apre sempre, qui
       solo con --ha. Senza, dati_dal_vero() resta falso e le schermate
       mostrano i valori inventati che servono alle catture. */
    sistema_avvia();
    ui_avvia();
    if (opz.sezione) ui_vai_a(sezione_da_chiave(opz.sezione), opz.vista);
    sim_applica_stato(opz.stato, opz.vista);

    /* With --ha the simulator starts the way the panel does, boot screen
       included. Without, nothing would ever tick — no radio, no Home
       Assistant — and every run would open on twenty seconds of waiting. */
    if (opz.ha && !opz.stato && esito_cfg != CFG_PREDEFINITA) stato_avvio(true);

    for (;;) {
        uint32_t attesa = lv_timer_handler();
        if (attesa == LV_NO_TIMER_READY) attesa = LV_DEF_REFR_PERIOD;

        /* Il server avanza dal ciclo principale e non da un thread: cosi
           configurazione e segreti hanno un lettore per volta senza che
           serva un lucchetto, e sul pannello quel problema non esiste
           perche esp_http_server serve dal proprio compito. */
        const uint32_t adesso = lv_tick_get();
        web_tempo(adesso);
        http_gira();

        /* Home Assistant avanza dal ciclo principale come il server web:
           un lettore per volta sullo stato delle entita, senza lucchetti.
           Sul pannello sara un task suo che parla per code — la stessa
           cosa vista da un altro lato. */
        ha_gira(adesso);
        dati_gira(adesso);   /* i rilasci degli impulsi scadono qui */

        sorveglianza_gira(adesso);

        /* The clocks, as on the panel: once a second. The simulator never
           called this, and its clocks stood still like the panel's. */
        static uint32_t ultimo_orologio;
        if (adesso - ultimo_orologio > 1000) {
            ultimo_orologio = adesso;
            ui_orologi_aggiorna();
        }

        static uint32_t ultimo_controllo;
        static uint32_t ultima_impronta;
        /* I valori cambiano di continuo; ridisegnare a ogni evento
           costerebbe piu del ridisegno stesso. Un giro al secondo e piu
           di quanto un occhio distingua da un metro e mezzo. */
        if (ha_stato() == HA_PRONTO && adesso - ultimo_controllo > 1000) {
            /* Il tempo si segna qui, prima di guardare se e cambiato
               qualcosa: e cosi che una strozzatura strozza. Segnandolo solo
               nel ramo del ridisegno, quando **non** cambiava niente la
               condizione restava vera e si rientrava al giro dopo. */
            ultimo_controllo = adesso;
            dati_ricarica();
            /* Come sul pannello: si rifa solo se e cambiato qualcosa che
               **questa** sezione mostra. Qui non si vedrebbe la differenza —
               sul PC un ridisegno e gratis — ma tenere le due strade uguali
               e l'unico modo perche il simulatore continui a dire la verita
               su come si comporta il pannello. */
            const uint32_t imp = impronta_sezione((int)ui_dove());
            if (imp != ultima_impronta) {
                ultima_impronta = imp;
                ui_vai_a(ui_dove(), ui_vista());
            }
        }
        if (attesa > 20) attesa = 20;   /* per non far aspettare chi bussa */

        lv_delay_ms(attesa);
    }
}
