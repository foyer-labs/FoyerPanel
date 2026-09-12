/* ------------------------------------------------------------------------
 * Quali colori esistono, e quali si scelgono — 02-design-tokens.md.
 *
 * Sta separato da `theme.h` per una ragione sola, e non è di ordine: qui
 * dentro **non c'è LVGL**. I colori sono numeri, il modo di ricavarli è
 * aritmetica, e nessuna delle due cose ha bisogno di una libreria grafica.
 * Così la palette si può provare — e la prova che tiene ferma la promessa
 * dei valori predefiniti è quella che conta — senza collegare LVGL con i
 * suoi font.
 *
 * `theme.h` prende questa tabella e le mette sopra i nomi con cui
 * l'interfaccia la usa.
 * --------------------------------------------------------------------- */
#ifndef TEMA_H
#define TEMA_H

#include <stdint.h>

typedef enum {
    /* --- gli otto che si scelgono --- */
    TEMA_BG = 0,      /* sfondo generale                                  */
    TEMA_CARD,        /* schede                                           */
    TEMA_LINE,        /* bordi 1 px                                       */
    TEMA_TXT,         /* testo principale                                 */
    TEMA_DIM,         /* testo secondario                                 */
    TEMA_ACC,         /* accento, selezione, azione                       */
    TEMA_OK,          /* positivo                                         */
    TEMA_WARN,        /* negativo                                         */

    /* Una luce accesa. Ha un colore suo e non segue l'accento perche' non
       e' uno stato dell'interfaccia: e' una cosa che si accende in casa. Un
       lampadario verde e' strano da guardare anche quando il pannello e
       verde — e chi lo guarda non pensa «che bel tema», pensa che la luce
       sia di quel colore. */
    TEMA_LUCE,

    TEMA_SCELTI,      /* quanti sono, e da qui in poi si ricavano tutti   */

    /* --- ricavati dai neutri --- */
    TEMA_CARD2 = TEMA_SCELTI,  /* sfondi secondari, rail, barre           */
    TEMA_OFF,         /* tracce spente                                    */
    TEMA_INK,         /* testo sopra l'accento                            */

    /* --- ruotati con l'accento --- */
    TEMA_SEL_BG,      /* selezione nel rail                               */
    TEMA_SEL_LINE,
    TEMA_INVIO_BG,    /* pulsante "in corso"                              */
    TEMA_INVIO_TXT,
    TEMA_RICON_BG,    /* fascia di riconnessione                          */

    /* --- ruotati col positivo --- */
    TEMA_FATTO_BG,    /* pulsante "riuscito"                              */
    TEMA_FATTO_LINE,
    TEMA_FATTO_TXT,
    TEMA_AVV_OK_BG,   /* avviso positivo                                  */
    TEMA_AVV_OK_TXT,

    /* --- ruotati col negativo --- */
    TEMA_ERR_BG,      /* pulsante "fallito"                               */
    TEMA_ERR_LINE,
    TEMA_ERR_TXT,
    TEMA_AVV_KO_BG,   /* avviso negativo                                  */
    TEMA_AVV_KO_TXT,

    /* --- costanti, e con una ragione ciascuna ---
     *
     * `cool` e `caldo` sono una coppia: azzurro in raffrescamento, ambra in
     * riscaldamento. Dicono **cosa sta facendo l'impianto**, non che aspetto
     * ha il pannello.
     *
     * L'azzurro era già fisso; l'ambra era l'accento, e sembrava giusta
     * perché l'accento era ambra — la stessa coincidenza del sole, e si è
     * vista appena l'accento è diventato verde. `caldo` tiene esattamente
     * quell'ambra, così con la palette predefinita non cambia niente.
     *
     * `nero` è nero vero, e serve in un posto solo: la schermata di riposo.
     * Lo sfondo generale è un grigio molto scuro con dentro un filo di blu,
     * e su un pannello a muro di notte quel filo si vede — un rettangolo
     * debolmente illuminato in un corridoio buio.
     *
     * I quattro dei calendari devono distinguersi **fra loro**: seguire
     * l'accento porterebbe il primo a coincidere con qualcos'altro. */
    TEMA_COOL,
    TEMA_CALDO,
    TEMA_NERO,

    /* --- il meteo, che non segue niente --------------------------------
     *
     * L'icona del tempo dice **che tempo fa**, e un sole verde non lo dice.
     * Prima era tutta del colore dell'accento, e con l'ambra sembrava
     * giusta perche' l'ambra sembra un sole: era una coincidenza, e si e
     * vista appena l'accento e diventato un altro.
     *
     * Quattro colori fissi, uno per quello che l'icona rappresenta. Il sole
     * tiene l'ambra di sempre, cosi con la palette predefinita quella parte
     * dello schermo non si muove di un pixel; nuvola, pioggia e neve
     * smettono di essere ambra e diventano quello che sono. */
    TEMA_METEO_SOLE,
    TEMA_METEO_NUVOLA,
    TEMA_METEO_PIOGGIA,
    TEMA_METEO_NEVE,
    TEMA_CAL_1,
    TEMA_CAL_2,
    TEMA_CAL_3,
    TEMA_CAL_4,

    /* The amber of Foyer's logo. Fixed like the weather's sun, and for a
       stronger reason: a logo that changes colour with the theme is no
       longer the same logo. */
    TEMA_MARCHIO,

    TEMA_QUANTI,
} tema_voce_t;

/* La tabella, in 0xRRGGBB. La riempie tema_applica(). */
extern uint32_t TEMA[TEMA_QUANTI];

/* Legge la configurazione e ricava tutto il resto. Da chiamare prima di
   costruire l'interfaccia e a ogni salvataggio: ui_avvia() lo fa. */
void tema_applica(void);

/* Il valore predefinito di una voce. Serve alla prova che i tre posti — la
   tabella qui, il documento e lo schema — dicano la stessa cosa. */
uint32_t tema_predefinito(tema_voce_t v);

/* Il nome con cui una delle otto voci scelte sta in configurazione, o NULL
   se quella voce non si sceglie. */
const char *tema_nome(tema_voce_t v);

#endif /* TEMA_H */
