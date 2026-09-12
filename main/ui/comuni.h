/* ------------------------------------------------------------------------
 * Mattoni di base condivisi da tutte le schermate.
 *
 * Non e una libreria di widget — quelli stanno in widgets/ — ma le tre o
 * quattro cose che servono ovunque: un contenitore senza gli stili
 * predefiniti di LVGL, un'etichetta con un ruolo tipografico, un bordo.
 *
 * Nessuna misura qui dentro: arrivano da PRF e da COM.
 * --------------------------------------------------------------------- */
#ifndef UI_COMUNI_H
#define UI_COMUNI_H

#include "fonts/fonts.h"
#include "i18n.h"   /* tr(), trn(): every screen speaks */
#include "icons/icons.h"
#include "lvgl.h"
#include "dati.h"
#include "profile.h"
#include "theme.h"

/* Contenitore nudo: niente padding, niente bordo, niente scorrimento.
   LVGL ne mette di suoi e sono misure che non abbiamo scelto noi. */
lv_obj_t *ui_pannello(lv_obj_t *padre, lv_color_t sfondo);

/* Scheda: contenitore con sfondo, bordo e raggio del profilo. */
lv_obj_t *ui_scheda(lv_obj_t *padre);

/* Etichetta con un ruolo tipografico. Il codice non nomina mai un corpo. */
lv_obj_t *ui_testo(lv_obj_t *padre, const char *testo, lv_color_t colore,
                   font_ruolo_t ruolo);

/* Etichetta in maiuscoletto spaziato, come le intestazioni delle schede
   ("IN CASA", "ENERGIA ADESSO"): 01-specifica-ui.md §2. */
lv_obj_t *ui_occhiello(lv_obj_t *padre, const char *testo);

/* Icona come etichetta: e una stringa UTF-8 in un font-icona. */
lv_obj_t *ui_icona(lv_obj_t *padre, const char *ico, lv_color_t colore,
                   icona_corpo_t corpo);

/* Bordo di un lato, o LV_BORDER_SIDE_FULL. */
void ui_bordo(lv_obj_t *o, lv_border_side_t lato, lv_color_t colore);

/* --- fare di un oggetto intero un bersaglio ------------------------------
 *
 * Serve a una trappola di LVGL che costa mezz'ora a chiunque la incontri:
 * **lv_obj nasce cliccabile**. Un contenitore decorativo — un riquadro, una
 * riga, una traccia di barra — si prende quindi il tocco e non ne fa
 * niente, e il gestore messo sul genitore non scatta mai. Le etichette no,
 * quelle nascono trasparenti al tocco: cosi capita di avere una scheda in
 * cui si accende la luce toccando **solo il nome**, che e esattamente il
 * difetto che si stava cercando di togliere.
 *
 * Questa funzione toglie la cliccabilita a tutto quello che sta dentro `o`
 * e la lascia solo a `o`: da quel momento il bersaglio e l'oggetto intero,
 * cornice compresa. Da chiamare **dopo** aver costruito il contenuto. */
void ui_tocco_su_tutto(lv_obj_t *o);

/* --- cambiare un testo senza ridisegnare per niente ---------------------
 *
 * lv_label_set_text() invalida l'etichetta **anche quando il testo e lo
 * stesso**, e su questo pannello un'invalidazione non e gratis: dentro un
 * contenitore semitrasparente costringe a ridisegnare anche cio che sta
 * sotto, e il ridisegno si vede mentre avviene.
 *
 * Lo standby lo faceva sei volte al secondo per un orologio che cambia una
 * volta al minuto. Da qui la regola: si scrive solo se e cambiato. */
void ui_scrivi(lv_obj_t *etichetta, const char *testo);

/* Riempitivo che spinge a fondo riga cio che viene dopo. */
lv_obj_t *ui_spazio(lv_obj_t *padre);

/* Una griglia di celle tutte uguali.
   LVGL tiene un **puntatore** ai descrittori delle tracce, non una copia:
   un array statico condiviso fra due griglie fa si che la seconda riscriva
   la prima, e la prima perde delle colonne senza dire niente. Qui i
   descrittori vivono quanto la griglia. */
lv_obj_t *ui_griglia(lv_obj_t *padre, int colonne, int righe);

/* --- il meteo, in due pezzi --------------------------------------------
 *
 * Home Assistant da uno stato in inglese e minuscolo: "partlycloudy",
 * "clear-night". Queste due lo traducono nelle due cose che il pannello
 * mostra, l'icona e la parola.
 *
 * Stanno qui e non nella schermata che le usa perche' le schermate sono
 * due — la home e lo standby — e uno stato tradotto in due posti prima o
 * poi si traduce in due modi.
 *
 * Uno stato sconosciuto da il sole per l'icona e stringa vuota per il
 * nome: un'icona ci vuole comunque, una parola sbagliata no. */
const char *ui_meteo_icona(const char *stato);

/* Il colore di quell'icona. Sta accanto a chi sceglie il glifo perche' e la
   stessa domanda fatta due volte, e separarle vorrebbe dire due tabelle di
   condizioni meteo che si allontanano. Non segue la palette: un sole del
   colore dell'accento e un sole solo finche' l'accento e ambra. */
lv_color_t ui_meteo_colore(const char *stato);
const char *ui_meteo_nome(const char *stato);

/* The Home Assistant connection state in words, for the screen; the
   argument is an ha_stato_t. */
const char *ui_ha_stato(int s);

/* Una temperatura in decimi di grado come la scrive un italiano: 245 -> "24,5".
   Con TEMP_IGNOTA restituisce "—", che e il segnaposto di 11-collaudo.md §2.
   Il risultato vive fino alla ottava chiamata successiva: basta per comporre
   una riga, non per tenerlo da parte. */
const char *ui_temp(int16_t decimi);

/* As above, but the decimal only when there is one: 180 -> "18",
   187 -> "18.7" ("18,7" in Italian). For lines of text; where the number
   is big there is ui_temperatura(). */
const char *ui_temp_compatta(int16_t decimi);

/* A big temperature, with the degree sign: the whole part in the `ruolo`
   size and the tenths — only when there are any — a step below, on the
   same baseline. "18°" stays as it was; "18.7°" puts ".7°" smaller, so the
   separator does not widen the number more than needed. With TEMP_IGNOTA
   it writes "—°". */
lv_obj_t *ui_temperatura(lv_obj_t *padre, int16_t decimi, lv_color_t colore,
                         font_ruolo_t ruolo);

/* Una potenza in watt scritta come la legge una persona: "3,42 kW" sopra il
   chilowatt, "640 W" sotto. Sempre in valore assoluto: la direzione la dice
   la parola accanto, mai un segno meno (01-specifica-ui.md §3.3). */
const char *ui_potenza(int32_t watt);

/* Solo il numero, e l'unita a parte. Serve dove la cifra e grande e
   l'unita piccola accanto: il font delle cifre grandi ha in tabella le
   cifre e la virgola e nient'altro, quindi una "kW" dentro quel testo
   diventerebbe due rettangoli vuoti — ed e successo. */
const char *ui_potenza_numero(int32_t watt);
const char *ui_potenza_unita(int32_t watt);

/* Energia in wattora: "24,8 kWh". */
const char *ui_energia(int32_t wattora);

/* Da percentuale a opacita LVGL: le percentuali stanno in COM. */
lv_opa_t ui_opa(uint8_t percento);

/* Allarga l'area sensibile fino al minimo di COM.tocco_min senza toccare il
   disegno: 09-profili.md §8 elenca tasti piu piccoli del minimo, e si
   risolve cosi, non ingrandendo il tasto. */
void ui_tocco_minimo(lv_obj_t *o, uint16_t w, uint16_t h);

#endif /* UI_COMUNI_H */
