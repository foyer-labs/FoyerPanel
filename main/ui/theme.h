/* ------------------------------------------------------------------------
 * Palette e stati composti — 02-design-tokens.md.
 *
 * Qui c'e solo il colore, che e identico sui due profili. Le misure
 * arrivano dal profilo: se stai per scrivere un numero di layout in questo
 * file, il posto giusto e docs/profili.json.
 *
 * --- la palette non e piu costante, ed e cambiato poco -------------------
 *
 * I nomi qui sotto erano `#define` che si espandevano in un colore scritto
 * a mano. Adesso sono letture da una tabella che `tema_applica()` riempie
 * leggendo la configurazione. **Nessuno dei quasi settecento punti d'uso e
 * cambiato**, ed e il motivo per cui questa modifica e stata piccola: i
 * colori stavano tutti qui, e fuori da questo file non c'era un solo
 * esadecimale.
 *
 * --- otto si scelgono, il resto si ricava -------------------------------
 *
 * In configurazione si scelgono **otto** colori: sfondo, schede, bordi,
 * testo, testo debole, accento, positivo, negativo. Tutti gli altri si
 * ricavano, e non per pigrizia: gli stati composti sono ventuno, e
 * chiederli uno per uno vorrebbe dire ventun scelte da tenere coerenti fra
 * loro — basta sbagliarne una perche' una scritta diventi illeggibile.
 *
 * Come si ricavano sta in tema.c. La cosa da sapere qui e una sola:
 * **con i valori predefiniti la tabella contiene esattamente i colori che
 * questo file conteneva prima**, byte per byte. Non per approssimazione:
 * per costruzione. C'e una prova che lo verifica.
 *
 * La palette predefinita e stata scelta e non va "migliorata". Cambiarla
 * adesso si puo — dalla pagina, per il proprio pannello — ed e diverso dal
 * cambiarla per tutti.
 * --------------------------------------------------------------------- */
#ifndef THEME_H
#define THEME_H

#include "lvgl.h"

#include "tema.h"

/* --- palette ------------------------------------------------------------ */
#define C_BG     lv_color_hex(TEMA[TEMA_BG])
#define C_NERO   lv_color_hex(TEMA[TEMA_NERO])
#define C_CARD   lv_color_hex(TEMA[TEMA_CARD])
#define C_CARD2  lv_color_hex(TEMA[TEMA_CARD2])
#define C_LINE   lv_color_hex(TEMA[TEMA_LINE])
#define C_TXT    lv_color_hex(TEMA[TEMA_TXT])
#define C_DIM    lv_color_hex(TEMA[TEMA_DIM])
#define C_ACC    lv_color_hex(TEMA[TEMA_ACC])
#define C_OK     lv_color_hex(TEMA[TEMA_OK])
#define C_WARN   lv_color_hex(TEMA[TEMA_WARN])
#define C_COOL   lv_color_hex(TEMA[TEMA_COOL])
#define C_CALDO  lv_color_hex(TEMA[TEMA_CALDO])
#define C_LUCE   lv_color_hex(TEMA[TEMA_LUCE])
#define C_OFF    lv_color_hex(TEMA[TEMA_OFF])
#define C_INK    lv_color_hex(TEMA[TEMA_INK])

/* --- stati composti: sfondo, bordo, testo -------------------------------
 *
 * Dove due stati avevano lo stesso colore, uno e l'alias dell'altro invece
 * di una seconda voce con lo stesso valore: due voci uguali si allontanano
 * al primo che ne cambia una sola. */
#define C_SEL_BG      lv_color_hex(TEMA[TEMA_SEL_BG])
#define C_SEL_LINE    lv_color_hex(TEMA[TEMA_SEL_LINE])
#define C_SEL_TXT     C_ACC

#define C_INVIO_BG    lv_color_hex(TEMA[TEMA_INVIO_BG])
#define C_INVIO_LINE  C_SEL_LINE
#define C_INVIO_TXT   lv_color_hex(TEMA[TEMA_INVIO_TXT])

#define C_FATTO_BG    lv_color_hex(TEMA[TEMA_FATTO_BG])
#define C_FATTO_LINE  lv_color_hex(TEMA[TEMA_FATTO_LINE])
#define C_FATTO_TXT   lv_color_hex(TEMA[TEMA_FATTO_TXT])

#define C_ERR_BG      lv_color_hex(TEMA[TEMA_ERR_BG])
#define C_ERR_LINE    lv_color_hex(TEMA[TEMA_ERR_LINE])
#define C_ERR_TXT     lv_color_hex(TEMA[TEMA_ERR_TXT])

#define C_RICON_BG    lv_color_hex(TEMA[TEMA_RICON_BG])
#define C_RICON_LINE  C_SEL_LINE
#define C_RICON_TXT   C_INVIO_TXT

#define C_AVV_OK_BG   lv_color_hex(TEMA[TEMA_AVV_OK_BG])
#define C_AVV_OK_LINE C_FATTO_LINE
#define C_AVV_OK_TXT  lv_color_hex(TEMA[TEMA_AVV_OK_TXT])

#define C_AVV_KO_BG   lv_color_hex(TEMA[TEMA_AVV_KO_BG])
#define C_AVV_KO_LINE C_ERR_LINE
#define C_AVV_KO_TXT  lv_color_hex(TEMA[TEMA_AVV_KO_TXT])

/* --- il meteo ------------------------------------------------------------
 *
 * Non si scelgono e non si ricavano: dicono che tempo fa. Chi disegna non
 * li nomina uno per uno — chiede ui_meteo_colore(), che sceglie quello
 * giusto per la condizione, accanto a ui_meteo_icona() che sceglie il
 * glifo. Le due domande sono la stessa, e stanno una accanto all'altra. */
#define C_METEO_SOLE    lv_color_hex(TEMA[TEMA_METEO_SOLE])
#define C_MARCHIO       lv_color_hex(TEMA[TEMA_MARCHIO])
#define C_METEO_NUVOLA  lv_color_hex(TEMA[TEMA_METEO_NUVOLA])
#define C_METEO_PIOGGIA lv_color_hex(TEMA[TEMA_METEO_PIOGGIA])
#define C_METEO_NEVE    lv_color_hex(TEMA[TEMA_METEO_NEVE])

/* --- colori dei calendari, nell'ordine in cui si assegnano --------------- */
#define C_CAL_1  lv_color_hex(TEMA[TEMA_CAL_1])
#define C_CAL_2  lv_color_hex(TEMA[TEMA_CAL_2])
#define C_CAL_3  lv_color_hex(TEMA[TEMA_CAL_3])
#define C_CAL_4  lv_color_hex(TEMA[TEMA_CAL_4])

#endif /* THEME_H */
