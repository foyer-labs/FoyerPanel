/* ------------------------------------------------------------------------
 * Composizione dell'interfaccia e navigazione.
 *
 * Il telaio si costruisce una volta sola; cambiare sezione ricostruisce il
 * solo contenuto. Non ci sono transizioni di pagina a schermo intero: e
 * una scelta, perche i due orientamenti devono sembrare lo stesso
 * apparecchio (02-design-tokens.md).
 *
 * Ogni misura viene da PRF o da COM.
 * --------------------------------------------------------------------- */
#include "ui.h"

#include "comuni.h"
#include "config.h"
#include "i18n.h"
#include "translations.h"
#include "nav.h"
#include "orologio.h"
#include "schermate.h"
#include "stati.h"
#include "tempi.h"

static lv_obj_t *radice;
static lv_obj_t *principale;    /* testata + contenuto, accanto alla nav */
static lv_obj_t *intestazione;
static lv_obj_t *titolo;
static lv_obj_t *sottotitolo;
static lv_obj_t *destra;
static lv_obj_t *contenuto;
static lv_obj_t *corrente;      /* cio che la schermata ha costruito */
static sezione_t dove = SEZ_HOME;
static int       vista;

/* --- testata di sezione — 01-specifica-ui.md §1 ------------------------- */

static void costruisci_testata(lv_obj_t *padre)
{
    intestazione = ui_pannello(padre, C_BG);
    lv_obj_set_size(intestazione, LV_PCT(100), PRF->geo.head_h);
    lv_obj_set_style_pad_hor(intestazione, PRF->geo.pad, 0);
    lv_obj_set_flex_flow(intestazione, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(intestazione, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(intestazione, PRF->geo.gap, 0);

    titolo = ui_testo(intestazione, "", C_TXT, FT_L);
    sottotitolo = ui_testo(intestazione, "", C_DIM, FT_S);
    ui_spazio(intestazione);

    /* A destra ci sta l'orologio, o il paginatore nelle schermate paginate:
       il contenitore e lo stesso, cambia chi lo riempie. */
    destra = ui_pannello(intestazione, C_BG);
    lv_obj_set_style_bg_opa(destra, LV_OPA_TRANSP, 0);
    lv_obj_set_size(destra, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(destra, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(destra, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
}

/* --- the clocks that are not the home's ----------------------------------
 *
 * Every section header showed "18:42", written into ui_testata() when the
 * headers were drawn from the mockup, whose clock says 18:42. Nobody
 * noticed on the PC — captures freeze the time at 18:41, one minute away —
 * and on the wall it was simply a clock that never moved.
 *
 * A clock is a label that dies with its screen, so it has to say when it
 * goes: the same rule as the home clock (schermata_home.c), for the same
 * reason. Four slots are plenty — a header and a full-screen state at most.
 */
#define OROLOGI_MAX 4
static lv_obj_t *orologi[OROLOGI_MAX];

static void scorda_orologio(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    for (int n = 0; n < OROLOGI_MAX; n++)
        if (orologi[n] == o) orologi[n] = NULL;
}

lv_obj_t *ui_orologio(lv_obj_t *padre, font_ruolo_t font)
{
    char ora[8];
    orologio_ora(ora, sizeof ora);
    lv_obj_t *l = ui_testo(padre, ora, C_DIM, font);
    for (int n = 0; n < OROLOGI_MAX; n++) {
        if (!orologi[n]) {
            orologi[n] = l;
            lv_obj_add_event_cb(l, scorda_orologio, LV_EVENT_DELETE, NULL);
            break;
        }
    }
    return l;
}

void ui_orologi_aggiorna(void)
{
    char ora[8];
    orologio_ora(ora, sizeof ora);
    for (int n = 0; n < OROLOGI_MAX; n++)
        if (orologi[n]) ui_scrivi(orologi[n], ora);
    home_orologio_aggiorna();
}

void ui_testata(const char *t, const char *s)
{
    lv_label_set_text(titolo, t ? t : "");
    lv_label_set_text(sottotitolo, s ? s : "");
    lv_obj_set_style_opa(sottotitolo, s ? LV_OPA_COVER : LV_OPA_TRANSP, 0);

    lv_obj_clean(destra);
    ui_orologio(destra, FT_M);
}

lv_obj_t *ui_testata_destra(void)
{
    lv_obj_clean(destra);
    return destra;
}

/* --- navigazione -------------------------------------------------------- */

lv_obj_t *ui_contenuto(void) { return corrente; }

sezione_t ui_dove(void) { return dove; }

int ui_vista(void) { return vista; }

/* Vedi ui_vai_a(): chi tiene un puntatore a un oggetto che qualcun altro
   puo distruggere deve farselo dire, non sperarlo. */
static void scorda_corrente(lv_event_t *e) { LV_UNUSED(e); corrente = NULL; }

void ui_vai(sezione_t s) { ui_vai_a(s, 0); }

void ui_vai_a(sezione_t s, int v)
{
    /* Arrivare da qualche parte chiude cio che stava sopra: un modale o una
       schermata di errore non sopravvivono alla navigazione. */
    stati_chiudi();

    dove = s;
    vista = v;

    /* Il contenuto si butta e si rifa. Ricostruire e piu semplice che tenere
       in vita undici schermate, e finche non si alloca a ogni ridisegno
       l'heap resta dov'era (11-collaudo.md §1).

       `corrente` si azzera da solo quando l'oggetto muore, ed e la terza
       volta che questa regola serve in questo file — dopo l'orologio della
       home e la fascia di riconnessione. Senza, bastava questa catena:

           ui_avvia -> lv_obj_clean(radice)   distrugge il contenuto
           ui_avvia -> ui_vai -> ui_vai_a     lo cancella una seconda volta

       cioe una doppia cancellazione a ogni salvataggio della configurazione,
       che sul pannello finiva in Load access fault dentro lv_obj_delete. Non
       succedeva sempre — dipendeva da cosa ci fosse finito dentro quel blocco
       di memoria — e per questo si presentava come un pannello che «ogni
       tanto» si pianta. */
    if (corrente && lv_obj_is_valid(corrente)) lv_obj_delete(corrente);
    corrente = ui_pannello(contenuto, C_BG);
    lv_obj_add_event_cb(corrente, scorda_corrente, LV_EVENT_DELETE, NULL);
    lv_obj_set_size(corrente, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(corrente, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(corrente, PRF->geo.gap, 0);

    /* La home ha una testata sua, piu alta, con orologio grande e meteo:
       quella di sezione si toglie di mezzo. */
    const bool casa = s == SEZ_HOME;
    if (casa) lv_obj_add_flag(intestazione, LV_OBJ_FLAG_HIDDEN);
    else      lv_obj_clear_flag(intestazione, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_pad_all(contenuto, casa ? 0 : PRF->geo.pad, 0);

    schermata_costruisci(s, corrente);

    nav_evidenzia(s);
    /* Niente lv_display_trigger_activity qui: LVGL registra da solo i tocchi
       veri, e azzerare il conto anche quando e il ritorno automatico a
       cambiare schermata rimanderebbe lo standby all'infinito — bastava
       lasciare una sezione aperta e il pannello non si sarebbe mai spento. */
}

/* --- costruzione -------------------------------------------------------- */

void ui_avvia(void)
{
    /* La palette prima di qualunque cosa la usi. ui_avvia() si richiama a
       ogni salvataggio della configurazione — e per questo che cambiare un
       colore dalla pagina si vede subito, senza una riga in piu da nessuna
       parte: i colori si applicano mentre si disegna, e qui si ridisegna
       tutto. */
    tema_applica();

    /* The language too, for the same reason: every label is created after
       this line, so a language changed from the page shows at the next
       save without anything else knowing about it. */
    i18n_set(cfg_testo("system/language", "en"));
    /* And the texts edited from the page, on top of the built-in ones. */
    translations_apply();

    radice = lv_screen_active();

    /* --- si puo chiamare due volte -----------------------------------
     *
     * Senza questa pulizia, richiamarla impilerebbe un secondo rail e un
     * secondo contenuto sopra i primi. Con, diventa "rifai tutto da capo" —
     * ed e quello che serve quando la configurazione cambia: le **sezioni**
     * decidono cosa c'e nel rail, e il rail si costruisce una volta sola
     * all'accensione. Senza ricostruirlo, togliere una sezione dalla pagina
     * di configurazione la lasciava nel rail fino al riavvio.
     *
     * Quello che rende sicura questa riga sono due correzioni recenti:
     * l'orologio della home e la fascia di riconnessione adesso si azzerano
     * da soli quando i loro oggetti muoiono. Prima, pulire la radice avrebbe
     * lasciato due puntatori penzolanti — e la prima pulizia sarebbe stata
     * l'ultima cosa fatta dal pannello. */
    lv_obj_clean(radice);

    lv_obj_remove_style_all(radice);
    lv_obj_set_style_bg_color(radice, C_BG, 0);
    lv_obj_set_style_bg_opa(radice, LV_OPA_COVER, 0);
    lv_obj_clear_flag(radice, LV_OBJ_FLAG_SCROLLABLE);

    /* 09-profili.md §1-ter: in orizzontale il rail sta accanto al contenuto,
       in verticale la barra sta sotto. Due direzioni di flex, stesso codice. */
    const bool vert = PRF->orientamento == VERTICALE;
    lv_obj_set_flex_flow(radice, vert ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);

    if (!vert) nav_costruisci(radice);

    principale = ui_pannello(radice, C_BG);
    lv_obj_set_flex_grow(principale, 1);
    lv_obj_set_flex_flow(principale, LV_FLEX_FLOW_COLUMN);
    if (vert) lv_obj_set_width(principale, LV_PCT(100));
    else      lv_obj_set_height(principale, LV_PCT(100));

    costruisci_testata(principale);

    contenuto = ui_pannello(principale, C_BG);
    lv_obj_set_width(contenuto, LV_PCT(100));
    lv_obj_set_flex_grow(contenuto, 1);

    if (vert) nav_costruisci(radice);

    ui_vai(SEZ_HOME);
    tempi_avvia();
}
