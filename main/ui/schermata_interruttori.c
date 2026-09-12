/* ------------------------------------------------------------------------
 * Sezione Interruttori — un elenco di righe, con l'interruttore a destra.
 *
 *   ┌───────────────────────────────────────────────────────┐
 *   │ ⏻  Presa TV                        118 W   ( ●───)    │
 *   │ ≈  Deumidificatore                   0 W   (───● )    │
 *   │ ≈  Pompa irrigazione               640 W   ( ●───)    │
 *   │ ❄  Congelatore                              (───● )   │
 *   └───────────────────────────────────────────────────────┘
 *
 * Righe e non piastrelle, ed e' la proposta B del mockup. Un interruttore ha
 * due stati e nient'altro: dargli l'area di una scheda delle Luci — che ha
 * anche la percentuale — vorrebbe dire pagare in paginazione uno spazio che
 * non serve a dire niente. Dodici righe stanno in una pagina; le stesse
 * dodici piastrelle sarebbero due pagine e mezzo.
 *
 * **L'interruttore a destra dice cosa succede al tocco prima che lo tocchi.**
 * Una piastrella va capita: si preme? apre un dettaglio? Questo no. Ed e' lo
 * stesso oggetto del dettaglio del clima e delle impostazioni, quindi non
 * c'e' niente di nuovo da imparare.
 *
 * Il bersaglio pero e' **tutta la riga**, non il solo interruttore: 76 px di
 * altezza contro 34, e un disegno che sembra premibile ma non e' l'unico
 * punto premibile e' meglio di un bersaglio piccolo. L'interruttore resta li
 * a dire lo stato e la direzione.
 * --------------------------------------------------------------------- */
#include "schermate.h"

#include <stdio.h>

#include "comuni.h"
#include "dati.h"
#include "nav.h"
#include "widgets/paginatore.h"
#include "profile.h"
#include "theme.h"
#include "ui.h"
#include "widgets/interruttore.h"

static int pagina;

/* Si ricostruisce la schermata invece di aggiornare la riga: e cosi che fa
   il resto del pannello, e una sola strada e' meglio di due che devono
   restare d'accordo. Il valore vero arriva col prossimo evento da Home
   Assistant; questo giro serve a far muovere l'interruttore subito, che se
   non si muove sembra che il tocco non sia arrivato. */
static void rifai(void)
{
    dati_ricarica();
    ui_vai_a(ui_dove(), ui_vista());
}

static void su_pagina(int nuova) { pagina = nuova; rifai(); }

static void su_riga(lv_event_t *e)
{
    const int n = (int)(intptr_t)lv_event_get_user_data(e);
    const interruttore_t *i = dati_interruttore(n);
    if (!i) return;
    dati_interruttore_premi(n, !i->acceso);
    rifai();
}

static void riga(lv_obj_t *padre, const interruttore_t *i, int indice)
{
    lv_obj_t *r = ui_scheda(padre);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, PRF->interruttori.riga_h);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, PRF->geo.gap, 0);

    /* Non disponibile: tutta la riga si attenua in un colpo solo, e lo dice
       anche a parole. Un interruttore che non risponde **non si finge
       spento**: spento e' uno stato, non lo sappiamo e' un altro, e
       disegnare il primo al posto del secondo e' il modo di far premere
       qualcosa che non succedera. */
    if (!i->disponibile)
        lv_obj_set_style_opa(r, ui_opa(COM.opacita_dato_vecchio_pct), 0);

    /* Il nome dell'icona diventa un glifo qui, che e' l'unico posto che sa
       cosa sia un glifo. Il ripiego e' una presa e non niente: una riga
       senza icona si legge come una riga a cui manca qualcosa, e a chi non
       l'ha scelta non manca niente. */
    ui_icona(r, icona_da_nome(i->icona, ICO_OUTLET),
             i->acceso ? C_ACC : C_DIM, IC_M);

    lv_obj_t *col = ui_pannello(r, C_CARD);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, COM.gap_stretto, 0);

    lv_obj_t *nm = ui_testo(col, i->nome && i->nome[0] ? i->nome : "—",
                            C_TXT, FT_M);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nm, LV_PCT(100));

    ui_testo(col, !i->disponibile ? tr(TX_COMMON_NOT_RESPONDING)
                 : i->acceso      ? tr(TX_SWITCHES_ON)
                                  : tr(TX_SWITCHES_OFF),
             i->acceso ? C_ACC : C_DIM, FT_XS);

    /* L'assorbimento, se questo interruttore ne ha uno. Senza, la riga non
       lascia il posto vuoto: il nome cresce e basta. Un incolonnamento che
       tiene la colonna anche quando e' vuota fa sembrare rotto cio' che e'
       soltanto assente. */
    if (i->potenza_c_e)
        ui_testo(r, ui_potenza(i->watt), i->acceso ? C_TXT : C_DIM, FT_M);

    lv_obj_t *sw = interruttore(r, i->acceso, i->disponibile);

    /* Il gestore sta sulla **riga**, non sull'interruttore: cosi il
       bersaglio e' alto settantasei pixel invece di trentaquattro. */
    if (i->disponibile) {
        lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(r, su_riga, LV_EVENT_CLICKED,
                            (void *)(intptr_t)indice);
        /* L'interruttore non se lo prende lui, il tocco: se lo prendesse,
           premere proprio sopra di lui non farebbe niente — che e'
           esattamente il punto in cui tutti premono. */
        lv_obj_remove_flag(sw, LV_OBJ_FLAG_CLICKABLE);
    }
}

void schermata_interruttori(lv_obj_t *c)
{
    const int quanti = dati_interruttori();

    int accesi = 0;
    for (int n = 0; n < quanti; n++) {
        const interruttore_t *i = dati_interruttore(n);
        if (i && i->acceso) accesi++;
    }

    static char sotto[32];
    lv_snprintf(sotto, sizeof sotto, trn(TXN_SWITCHES_ON_COUNT, accesi), accesi);
    ui_testata(tr(TX_SWITCHES_TITLE), quanti ? sotto : NULL);

    if (!quanti) {
        lv_obj_t *a = ui_testo(c, tr(TX_SWITCHES_NONE), C_DIM, FT_S);
        lv_label_set_long_mode(a, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(a, LV_PCT(100));
        return;
    }

    const int per_pagina = PRF->interruttori.per_pagina;
    const int pagine = paginatore_pagine(quanti, per_pagina);
    if (pagina >= pagine) pagina = 0;

    if (pagine > 1)
        paginatore_frecce(ui_testata_destra(), pagina, pagine, su_pagina);

    lv_obj_t *elenco = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(elenco, LV_OPA_TRANSP, 0);
    lv_obj_set_width(elenco, LV_PCT(100));
    lv_obj_set_flex_grow(elenco, 1);
    lv_obj_set_flex_flow(elenco, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(elenco, COM.gap_stretto, 0);

    for (int n = 0; n < per_pagina; n++) {
        const int k = pagina * per_pagina + n;
        if (k >= quanti) break;
        const interruttore_t *i = dati_interruttore(k);
        if (i) riga(elenco, i, k);
    }

    paginatore_pallini(c, pagina, pagine);
}
