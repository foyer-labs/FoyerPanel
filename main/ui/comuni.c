#include <string.h>

#include "comuni.h"
#include "ha.h"

lv_obj_t *ui_pannello(lv_obj_t *padre, lv_color_t sfondo)
{
    lv_obj_t *o = lv_obj_create(padre);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_color(o, sfondo, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

lv_obj_t *ui_scheda(lv_obj_t *padre)
{
    lv_obj_t *o = ui_pannello(padre, C_CARD);
    lv_obj_set_style_radius(o, PRF->geo.radius, 0);
    ui_bordo(o, LV_BORDER_SIDE_FULL, C_LINE);
    /* Il padding interno di una scheda non e quello di schermo: nei mockup
       e piu stretto, e piu stretto in verticale che ai lati. Finora si
       riusava geo.pad per entrambi gli assi, e quei pixel in piu erano
       esattamente quelli che mancavano al dettaglio del condizionatore. */
    lv_obj_set_style_pad_ver(o, PRF->geo.pad_scheda.h, 0);
    lv_obj_set_style_pad_hor(o, PRF->geo.pad_scheda.w, 0);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    return o;
}

lv_obj_t *ui_testo(lv_obj_t *padre, const char *testo, lv_color_t colore,
                   font_ruolo_t ruolo)
{
    lv_obj_t *l = lv_label_create(padre);
    lv_label_set_text(l, testo);
    lv_obj_set_style_text_color(l, colore, 0);
    lv_obj_set_style_text_font(l, font(ruolo), 0);
    return l;
}

lv_obj_t *ui_occhiello(lv_obj_t *padre, const char *testo)
{
    lv_obj_t *l = ui_testo(padre, testo, C_DIM, FT_XS);
    /* La spaziatura e in millesimi di em perche profili.json non puo tenere
       un numero con la virgola senza che qualcuno lo arrotondi per sbaglio. */
    const int32_t sp = (int32_t)PRF->font.f_xs
                     * (int32_t)COM.spaziatura_maiuscoletto_millesimi / 1000;
    lv_obj_set_style_text_letter_space(l, sp, 0);
    return l;
}

lv_obj_t *ui_icona(lv_obj_t *padre, const char *ico, lv_color_t colore,
                   icona_corpo_t corpo)
{
    lv_obj_t *l = lv_label_create(padre);
    lv_label_set_text(l, ico);
    lv_obj_set_style_text_color(l, colore, 0);
    lv_obj_set_style_text_font(l, icona(corpo), 0);
    return l;
}

void ui_bordo(lv_obj_t *o, lv_border_side_t lato, lv_color_t colore)
{
    lv_obj_set_style_border_color(o, colore, 0);
    lv_obj_set_style_border_width(o, COM.bordo, 0);
    lv_obj_set_style_border_side(o, lato, 0);
}

static void togli_tocco(lv_obj_t *o)
{
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    for (uint32_t n = 0; n < lv_obj_get_child_count(o); n++)
        togli_tocco(lv_obj_get_child(o, n));
}

void ui_scrivi(lv_obj_t *etichetta, const char *testo)
{
    if (!etichetta || !testo) return;
    const char *ora = lv_label_get_text(etichetta);
    if (ora && strcmp(ora, testo) == 0) return;
    lv_label_set_text(etichetta, testo);
}

void ui_tocco_su_tutto(lv_obj_t *o)
{
    if (!o) return;

    /* Su un oggetto senza figli questa funzione non ha niente da togliere:
       fa solo quello che farebbe lv_obj_add_flag, e chi l'ha chiamata voleva
       altro. Vuol dire che e stata chiamata **prima** di costruire il
       contenuto, e il difetto che ne segue e muto — la scheda si accende
       solo dove capita, e sembra un tocco capriccioso.

       L'intestazione lo dice gia a parole, e non e bastato: e successo tre
       volte, l'ultima nell'elenco delle reti Wi-Fi. Una riga di registro non
       lo impedisce, ma lo fa vedere alla prima prova invece che al primo
       dito. */
    if (lv_obj_get_child_count(o) == 0)
        LV_LOG_WARN("ui_tocco_su_tutto su un oggetto senza figli: "
                    "chiamata prima di costruire il contenuto?");

    togli_tocco(o);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t *ui_spazio(lv_obj_t *padre)
{
    lv_obj_t *o = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    /* Dimensione propria zero: cresce solo lungo l'asse del flex. Senza,
       si porta dietro la dimensione predefinita di LVGL e dentro una riga
       a contenuto la fa diventare alta cento pixel. */
    lv_obj_set_size(o, 0, 0);
    lv_obj_set_flex_grow(o, 1);
    return o;
}

static void libera_tracce(lv_event_t *e)
{
    lv_free(lv_obj_get_user_data(lv_event_get_target(e)));
}

lv_obj_t *ui_griglia(lv_obj_t *padre, int colonne, int righe)
{
    if (colonne < 1) colonne = 1;
    if (righe < 1) righe = 1;

    /* Un blocco solo per colonne e righe, terminatore compreso. */
    const size_t quante = (size_t)colonne + righe + 2;
    int32_t *tracce = lv_malloc(sizeof *tracce * quante);
    if (!tracce) return ui_pannello(padre, C_BG);

    int32_t *col = tracce;
    int32_t *rig = tracce + colonne + 1;
    for (int n = 0; n < colonne; n++) col[n] = LV_GRID_FR(1);
    col[colonne] = LV_GRID_TEMPLATE_LAST;
    for (int n = 0; n < righe; n++) rig[n] = LV_GRID_FR(1);
    rig[righe] = LV_GRID_TEMPLATE_LAST;

    lv_obj_t *g = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(g, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(g, tracce);
    lv_obj_add_event_cb(g, libera_tracce, LV_EVENT_DELETE, NULL);
    lv_obj_set_grid_dsc_array(g, col, rig);
    lv_obj_set_layout(g, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_column(g, PRF->geo.gap, 0);
    lv_obj_set_style_pad_row(g, PRF->geo.gap, 0);
    return g;
}

/* Otto buffer a giro: una riga puo comporre piu temperature insieme, e
   restituire sempre lo stesso buffer le farebbe diventare tutte uguali. */
static char giro[8][12];
static uint8_t prossimo;

static char *prendi(void)
{
    char *b = giro[prossimo];
    prossimo = (uint8_t)((prossimo + 1) % (sizeof giro / sizeof giro[0]));
    return b;
}

const char *ui_temp(int16_t decimi)
{
    if (decimi == TEMP_IGNOTA) return "—";
    char *b = prendi();
    /* The sign on its own, then the absolute value. Split into whole and
       tenths as signed numbers, -0.5 °C became 0 and 5: the whole part of
       -5 tenths is zero, and zero has no minus. The panel showed "0,5°" on
       a frosty balcony — the one reading where the sign is the news. */
    const bool meno = decimi < 0;
    const int d = meno ? -(int)decimi : (int)decimi;
    lv_snprintf(b, sizeof giro[0], "%s%d%c%d", meno ? "-" : "", d / 10,
                i18n_decimal(), d % 10);
    return b;
}

/* --- degrees without fake decimals ----------------------------------------
 *
 * ui_temp_intera() was here, and it **truncated**: 18.7 °C outside became
 * "18°" on the home screen, and -0.7 °C became "0°". Now the decimal shows
 * when there is one, and a round value stays round — "18°", not "18.0°".
 * The separator is the language's, as in ui_temp(). */
const char *ui_temp_compatta(int16_t decimi)
{
    if (decimi == TEMP_IGNOTA) return "—";
    char *b = prendi();
    const bool meno = decimi < 0;
    const int d = meno ? -(int)decimi : (int)decimi;
    if (d % 10)
        lv_snprintf(b, sizeof giro[0], "%s%d%c%d", meno ? "-" : "", d / 10,
                    i18n_decimal(), d % 10);
    else
        lv_snprintf(b, sizeof giro[0], "%s%d", meno ? "-" : "", d / 10);
    return b;
}

/* The size of the tenths, a step below the number's. For xl it is not the
   scale's step — 38 to 26 is a jump, and the tenths would look like a
   footnote — but a size of its own, 30 (09-profili.md §4). */
static font_ruolo_t corpo_decimi(font_ruolo_t ruolo)
{
    switch (ruolo) {
    case FT_XL: return FT_DECIMI_XL;
    case FT_M:  return FT_S;
    default:    return ruolo;
    }
}

lv_obj_t *ui_temperatura(lv_obj_t *padre, int16_t decimi, lv_color_t colore,
                         font_ruolo_t ruolo)
{
    lv_obj_t *r = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END);

    char t[16];
    const bool meno = decimi < 0;
    const int d = meno ? -(int)decimi : (int)decimi;

    /* Round or unknown: a single label, as before. */
    if (decimi == TEMP_IGNOTA || d % 10 == 0) {
        lv_snprintf(t, sizeof t, "%s°", ui_temp_compatta(decimi));
        ui_testo(r, t, colore, ruolo);
        return r;
    }

    lv_snprintf(t, sizeof t, "%s%d", meno ? "-" : "", d / 10);
    ui_testo(r, t, colore, ruolo);

    const font_ruolo_t piccolo = corpo_decimi(ruolo);
    lv_snprintf(t, sizeof t, "%c%d°", i18n_decimal(), d % 10);
    lv_obj_t *p = ui_testo(r, t, colore, piccolo);

    /* The two labels are aligned at the bottom, and the bottom of a label
       is not its baseline: below it is the descenders' room, which grows
       with the size. Without this correction the small digit would sit a
       few pixels lower than the large one, and the number would look
       crooked. */
    const int32_t scarto = (int32_t)font(piccolo)->base_line
                         - (int32_t)font(ruolo)->base_line;
    lv_obj_set_style_translate_y(p, scarto, 0);
    return r;
}

const char *ui_potenza(int32_t watt)
{
    if (watt < 0) watt = -watt;      /* mai un meno davanti a un numero */
    char *b = prendi();
    if (watt >= 1000)
        lv_snprintf(b, sizeof giro[0], "%d%c%02d kW", (int)(watt / 1000),
                    i18n_decimal(), (int)((watt % 1000) / 10));
    else
        lv_snprintf(b, sizeof giro[0], "%d W", (int)watt);
    return b;
}

const char *ui_potenza_numero(int32_t watt)
{
    if (watt < 0) watt = -watt;
    char *b = prendi();
    if (watt >= 1000)
        lv_snprintf(b, sizeof giro[0], "%d%c%02d", (int)(watt / 1000),
                    i18n_decimal(), (int)((watt % 1000) / 10));
    else
        lv_snprintf(b, sizeof giro[0], "%d", (int)watt);
    return b;
}

const char *ui_potenza_unita(int32_t watt)
{
    if (watt < 0) watt = -watt;
    return watt >= 1000 ? "kW" : "W";
}

const char *ui_energia(int32_t wattora)
{
    if (wattora < 0) wattora = -wattora;
    char *b = prendi();
    lv_snprintf(b, sizeof giro[0], "%d%c%d kWh", (int)(wattora / 1000),
                i18n_decimal(), (int)((wattora % 1000) / 100));
    return b;
}

lv_opa_t ui_opa(uint8_t percento)
{
    if (percento > 100) percento = 100;
    return (lv_opa_t)((uint16_t)percento * LV_OPA_COVER / 100);
}

void ui_tocco_minimo(lv_obj_t *o, uint16_t w, uint16_t h)
{
    const uint16_t min = COM.tocco_min;
    const int32_t dx = w < min ? (min - w + 1) / 2 : 0;
    const int32_t dy = h < min ? (min - h + 1) / 2 : 0;
    if (dx || dy) lv_obj_set_ext_click_area(o, dx > dy ? dx : dy);
}

/* --- il meteo ----------------------------------------------------------- */

const char *ui_meteo_icona(const char *stato)
{
    if (!stato || !*stato) return ICO_SUNNY;
    if (strstr(stato, "rain") || strstr(stato, "pour")) return ICO_RAINY;
    if (strstr(stato, "snow") || strstr(stato, "hail")) return ICO_AC_UNIT;
    if (strstr(stato, "cloud") || strstr(stato, "fog")) return ICO_CLOUD;
    return ICO_SUNNY;
}

lv_color_t ui_meteo_colore(const char *stato)
{
    if (!stato || !*stato) return C_METEO_SOLE;
    if (strstr(stato, "rain") || strstr(stato, "pour")) return C_METEO_PIOGGIA;
    if (strstr(stato, "snow") || strstr(stato, "hail")) return C_METEO_NEVE;
    if (strstr(stato, "cloud") || strstr(stato, "fog")) return C_METEO_NUVOLA;
    return C_METEO_SOLE;
}

/* I nomi di Home Assistant per intero, e non un `strstr` come per l'icona.
   L'icona puo permettersi di essere approssimativa — nuvoloso e poco
   nuvoloso hanno la stessa nuvola — mentre la parola no: "poco nuvoloso"
   contiene "nuvoloso", e cercare pezzi darebbe la risposta piu grossolana
   proprio dove serve quella precisa. */
/* The Home Assistant connection state in words, for the screen.
   ha_stato_nome() is the machine label that /api/status and the log
   use, and stays the same whatever language the panel speaks. */
const char *ui_ha_stato(int s)
{
    switch ((ha_stato_t)s) {
    case HA_SPENTO:          return tr(TX_HA_STATE_OFF);
    case HA_CONNETTO:        return tr(TX_HA_STATE_CONNECTING);
    case HA_AUTENTICO:       return tr(TX_HA_STATE_AUTHENTICATING);
    case HA_ALLINEO:         return tr(TX_HA_STATE_SYNCING);
    case HA_PRONTO:          return tr(TX_HA_STATE_CONNECTED);
    case HA_CADUTO:          return tr(TX_HA_STATE_DROPPED);
    case HA_TOKEN_RIFIUTATO: return tr(TX_HA_STATE_TOKEN_REFUSED);
    }
    return "";
}

const char *ui_meteo_nome(const char *stato)
{
    if (!stato || !*stato) return "";

    static const struct { const char *ha; tx_t nome; } N[] = {
        { "clear-night",      TX_WEATHER_CLEAR_NIGHT },
        { "cloudy",           TX_WEATHER_CLOUDY },
        { "fog",              TX_WEATHER_FOG },
        { "hail",             TX_WEATHER_HAIL },
        { "lightning",        TX_WEATHER_THUNDERSTORM },
        { "lightning-rainy",  TX_WEATHER_THUNDERSTORM },
        { "partlycloudy",     TX_WEATHER_PARTLY_CLOUDY },
        { "pouring",          TX_WEATHER_POURING },
        { "rainy",            TX_WEATHER_RAINY },
        { "snowy",            TX_WEATHER_SNOWY },
        { "snowy-rainy",      TX_WEATHER_SLEET },
        { "sunny",            TX_WEATHER_SUNNY },
        { "windy",            TX_WEATHER_WINDY },
        { "windy-variant",    TX_WEATHER_WINDY },
    };
    for (unsigned n = 0; n < sizeof N / sizeof N[0]; n++)
        if (strcmp(stato, N[n].ha) == 0) return tr(N[n].nome);

    /* "exceptional" e tutto quello che verra dopo: meglio niente che una
       parola inventata accanto a un numero vero. */
    return "";
}
