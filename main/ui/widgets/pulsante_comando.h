/* ------------------------------------------------------------------------
 * Pulsante di comando con riscontro — 01-specifica-ui.md §4.4.
 *
 * "Il riscontro sta **nel pulsante toccato**." Non in una notifica altrove,
 * non in una fascia in cima: nel pulsante che il dito ha appena premuto.
 *
 *   in corso    rotellina e "Invio…", fondo #2a2313, permanente fino alla
 *               risposta
 *   riuscito    spunta e "Inviato", fondo #16301f, per 3 s
 *   fallito     croce e "Non riuscito", fondo #2e1a17, **permanente**
 *
 * Il riuscito dichiara che Home Assistant ha accettato il comando, non che
 * il cancello si sia aperto. Sono due cose diverse e il pannello sa solo la
 * prima.
 *
 * Con `conferma` il pulsante porta "TIENI PREMUTO" e vuole una pressione di
 * 1500 ms con anello di avanzamento; rilasciato a meta non manda niente e
 * l'anello rientra.
 * --------------------------------------------------------------------- */
#ifndef PULSANTE_COMANDO_H
#define PULSANTE_COMANDO_H

#include "lvgl.h"

/* Il pulsante e scattato: da qui in poi tocca a chi ascolta mandare il
   comando e raccontare com'e andata con pulsante_comando_stato().

   Un evento a se e non LV_EVENT_CLICKED, e la differenza conta: con la
   conferma il click di LVGL arriva **comunque** al rilascio, anche a meta
   anello, e un cancello che si apre perche il dito e scivolato e proprio
   quello che la pressione prolungata doveva impedire. Questo evento lo
   manda solo chi ha davvero completato il gesto. */
#define EV_COMANDO_SCATTATO ((lv_event_code_t)(LV_EVENT_LAST + 1))

typedef enum {
    CMD_PRONTO = 0,
    CMD_IN_CORSO,
    CMD_RIUSCITO,
    CMD_FALLITO,
} stato_comando_t;

/* Un pulsante di comando. `attivo` a falso lo lascia visibile ma inerte:
   e il caso dell'entita non disponibile. */
lv_obj_t *pulsante_comando(lv_obj_t *padre, const char *testo, bool conferma,
                           bool attivo);

/* Porta il pulsante in uno stato. Da CMD_RIUSCITO torna da solo a
   CMD_PRONTO dopo tre secondi; da CMD_FALLITO non torna, perche un comando
   fallito non deve sparire mentre nessuno guarda. */
void pulsante_comando_stato(lv_obj_t *b, stato_comando_t s);

/* Riscrive l'etichetta di un pulsante gia costruito. Serve dove il comando
   cambia con la scelta — «Pulisci tutto» che diventa «Pulisci la Cucina» —
   e ricostruire il pulsante vorrebbe dire farlo sparire da sotto il dito.
   Il testo non si copia: il puntatore deve vivere quanto il pulsante. */
void pulsante_comando_testo(lv_obj_t *b, const char *testo);

/* Accende o spegne un pulsante gia costruito, con lo stesso aspetto smorzato
   che ha alla nascita quando `attivo` e falso. */
void pulsante_comando_attivo(lv_obj_t *b, bool attivo);

#endif /* PULSANTE_COMANDO_H */
