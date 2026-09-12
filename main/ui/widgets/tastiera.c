#include "tastiera.h"

#include "comuni.h"

#include <string.h>

/* Tre modi, come chiede la riga funzioni: lettere, cifre, simboli. */
typedef enum { MODO_LETTERE = 0, MODO_NUMERI, MODO_SIMBOLI } modo_t;

/* Le tre righe di ogni modo. Le lettere sono minuscole; MAIUSC le alza al
   volo, che e piu semplice e piu leggero di tenere due tabelle.

   The letters are the language's: keyboard.letters in i18n/<code>.json,
   three rows separated by '|' — QWERTY, AZERTY, QWERTZ. Whoever types a
   Wi-Fi password in French has AZERTY in their fingers. gen_texts.py
   --check makes sure every layout has all twenty-six letters: one missing
   would make some passwords impossible to type, on one language only. */
static const char *const RIGHE[3][3] = {
    { NULL, NULL, NULL },       /* from tr(TX_KEYBOARD_LETTERS) */
    { "1234567890", "-/:;()€&@", ".,?!'\"" },
    { "[]{}#%^*+=", "_\\|~<>$·", ".,?!'\"" },
};

/* Row r of the letters, copied out of "qwertyuiop|asdfghjkl|zxcvbnm". */
static const char *riga_lettere(int r, char *buf, size_t n)
{
    const char *s = tr(TX_KEYBOARD_LETTERS);
    for (int k = 0; k < r && s; k++) {
        s = strchr(s, '|');
        if (s) s++;
    }
    if (!s) { buf[0] = 0; return buf; }
    size_t i = 0;
    while (s[i] && s[i] != '|' && i + 1 < n) { buf[i] = s[i]; i++; }
    buf[i] = 0;
    return buf;
}

typedef struct {
    lv_obj_t      *griglia;
    tastiera_cb_t  richiamo;
    const char    *azione;
    modo_t         modo;
    bool           maiuscole;
} stato_t;

static void ricostruisci(lv_obj_t *t);

static void libera(lv_event_t *e)
{
    lv_free(lv_obj_get_user_data(lv_event_get_target(e)));
}

/* --- eventi ------------------------------------------------------------- */

/* Il testo del tasto vive nel tasto stesso: l'etichetta e gia li, e leggerla
   evita di tenere in vita una tabella parallela che puo disallinearsi. */
static void su_carattere(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    lv_obj_t *t = lv_obj_get_user_data(b);
    stato_t *s = lv_obj_get_user_data(t);
    if (s->richiamo)
        s->richiamo(TASTO_CARATTERE,
                    lv_label_get_text(lv_obj_get_child(b, 0)));
}

/* --- costruzione dei tasti ---------------------------------------------- */

static lv_obj_t *tasto(lv_obj_t *riga, lv_obj_t *radice, const char *testo,
                       lv_color_t sfondo, lv_color_t colore, font_ruolo_t corpo)
{
    lv_obj_t *b = ui_pannello(riga, sfondo);
    lv_obj_set_height(b, PRF->tocco.tastiera.h);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_min_width(b, 0, 0);
    lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
    ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(b, radice);
    ui_testo(b, testo, colore, corpo);
    return b;
}

static lv_obj_t *riga_nuova(lv_obj_t *padre)
{
    lv_obj_t *r = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, COM.pastiglia_gap, 0);
    return r;
}

/* --- funzioni della quarta riga ----------------------------------------- */

static void su_maiusc(lv_event_t *e)
{
    lv_obj_t *t = lv_obj_get_user_data(lv_event_get_target(e));
    stato_t *s = lv_obj_get_user_data(t);
    s->maiuscole = !s->maiuscole;
    ricostruisci(t);
}

static void su_modo(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    lv_obj_t *t = lv_obj_get_user_data(b);
    stato_t *s = lv_obj_get_user_data(t);
    const modo_t voluto = (modo_t)(intptr_t)lv_event_get_user_data(e);
    /* Premere di nuovo lo stesso tasto riporta alle lettere: e come si
       aspetta chiunque abbia usato una tastiera di un telefono. */
    s->modo = s->modo == voluto ? MODO_LETTERE : voluto;
    ricostruisci(t);
}

static void su_cancella(lv_event_t *e)
{
    lv_obj_t *t = lv_obj_get_user_data(lv_event_get_target(e));
    stato_t *s = lv_obj_get_user_data(t);
    if (s->richiamo) s->richiamo(TASTO_CANCELLA, NULL);
}

static void su_azione(lv_event_t *e)
{
    lv_obj_t *t = lv_obj_get_user_data(lv_event_get_target(e));
    stato_t *s = lv_obj_get_user_data(t);
    if (s->richiamo) s->richiamo(TASTO_AZIONE, NULL);
}

static void su_spazio(lv_event_t *e)
{
    lv_obj_t *t = lv_obj_get_user_data(lv_event_get_target(e));
    stato_t *s = lv_obj_get_user_data(t);
    if (s->richiamo) s->richiamo(TASTO_CARATTERE, " ");
}

/* --- disegno ------------------------------------------------------------ */

static void ricostruisci(lv_obj_t *t)
{
    stato_t *s = lv_obj_get_user_data(t);
    lv_obj_clean(t);

    /* Le tre righe di caratteri. */
    for (int r = 0; r < 3; r++) {
        lv_obj_t *riga = riga_nuova(t);
        char lettere[16];
        const char *chiavi = s->modo == MODO_LETTERE
                           ? riga_lettere(r, lettere, sizeof lettere)
                           : RIGHE[s->modo][r];
        for (const char *c = chiavi; *c; ) {
            /* I caratteri fuori ASCII occupano piu byte: si copiano interi,
               altrimenti l'euro diventa tre tasti rotti. */
            char testo[5] = {0};
            int len = 1;
            const unsigned char b0 = (unsigned char)*c;
            if (b0 >= 0xF0) len = 4;
            else if (b0 >= 0xE0) len = 3;
            else if (b0 >= 0xC0) len = 2;
            for (int k = 0; k < len; k++) testo[k] = c[k];
            c += len;

            if (s->modo == MODO_LETTERE && s->maiuscole
                && testo[0] >= 'a' && testo[0] <= 'z')
                testo[0] = (char)(testo[0] - 'a' + 'A');

            lv_obj_t *b = tasto(riga, t, testo, C_CARD2, C_TXT, FT_L);
            lv_obj_add_event_cb(b, su_carattere, LV_EVENT_CLICKED, NULL);
        }
    }

    /* Riga delle funzioni. */
    lv_obj_t *f = riga_nuova(t);

    lv_obj_t *m = tasto(f, t, tr(TX_KEYBOARD_SHIFT),
                        s->maiuscole ? C_SEL_BG : C_CARD2,
                        s->maiuscole ? C_ACC : C_DIM, FT_XS);
    lv_obj_add_event_cb(m, su_maiusc, LV_EVENT_CLICKED, NULL);

    lv_obj_t *n = tasto(f, t, "123", s->modo == MODO_NUMERI ? C_SEL_BG : C_CARD2,
                        s->modo == MODO_NUMERI ? C_ACC : C_DIM, FT_XS);
    lv_obj_add_event_cb(n, su_modo, LV_EVENT_CLICKED, (void *)(intptr_t)MODO_NUMERI);

    lv_obj_t *sim = tasto(f, t, "@#€", s->modo == MODO_SIMBOLI ? C_SEL_BG : C_CARD2,
                          s->modo == MODO_SIMBOLI ? C_ACC : C_DIM, FT_XS);
    lv_obj_add_event_cb(sim, su_modo, LV_EVENT_CLICKED, (void *)(intptr_t)MODO_SIMBOLI);

    /* Lo spazio e largo: e il tasto che si preme senza guardare. */
    lv_obj_t *sp = tasto(f, t, tr(TX_KEYBOARD_SPACE), C_CARD2, C_DIM, FT_S);
    lv_obj_set_flex_grow(sp, 4);
    lv_obj_add_event_cb(sp, su_spazio, LV_EVENT_CLICKED, NULL);

    lv_obj_t *canc = tasto(f, t, tr(TX_KEYBOARD_DELETE), C_CARD2, C_DIM, FT_XS);
    lv_obj_add_event_cb(canc, su_cancella, LV_EVENT_CLICKED, NULL);

    lv_obj_t *az = tasto(f, t, s->azione, C_ACC, C_INK, FT_M);
    lv_obj_set_flex_grow(az, 2);
    lv_obj_set_style_border_color(az, C_ACC, 0);
    lv_obj_add_event_cb(az, su_azione, LV_EVENT_CLICKED, NULL);
}

lv_obj_t *tastiera(lv_obj_t *padre, const char *azione, tastiera_cb_t su_tasto)
{
    stato_t *s = lv_malloc_zeroed(sizeof *s);
    if (!s) return ui_pannello(padre, C_BG);
    s->richiamo = su_tasto;
    s->azione = azione ? azione : tr(TX_KEYBOARD_NEXT);

    lv_obj_t *t = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
    lv_obj_set_width(t, LV_PCT(100));
    lv_obj_set_height(t, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(t, COM.pastiglia_gap, 0);
    lv_obj_set_user_data(t, s);
    lv_obj_add_event_cb(t, libera, LV_EVENT_DELETE, NULL);

    ricostruisci(t);
    return t;
}
