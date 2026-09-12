/* ------------------------------------------------------------------------
 * Interfaccia: composizione, navigazione, macchina a stati.
 *
 * Vale sia per il simulatore sia per il pannello: qui dentro non ci sono
 * chiamate all'hardware. Chi chiama ha gia scelto il profilo (PRF) e ha gia
 * un display LVGL attivo.
 * --------------------------------------------------------------------- */
#ifndef UI_H
#define UI_H

#include "fonts/fonts.h"
#include "lvgl.h"
#include "sezioni.h"

/* Costruisce l'interfaccia sullo schermo attivo e avvia le temporizzazioni. */
void ui_avvia(void);

/* Porta alla sezione indicata. SEZ_HOME e il ritorno unico: non esiste un
   tasto "indietro" gerarchico (01-specifica-ui.md §1). */
void ui_vai(sezione_t s);

/* Come sopra, ma scegliendo una vista dentro la sezione: il gruppo del
   clima, il dettaglio di un condizionatore, la rete privata del Wi-Fi.
   La vista 0 e sempre quella predefinita. Non e una gerarchia di
   navigazione — HOME resta il ritorno unico — ma il modo per aprire una
   sezione dove serve, anche dal simulatore. */
void ui_vai_a(sezione_t s, int vista);

/* La sezione e la vista mostrate adesso. */
sezione_t ui_dove(void);
int       ui_vista(void);

/* Il contenitore in cui una schermata disegna il proprio contenuto: dentro
   la testata, sopra il dock, con il padding gia applicato. */
lv_obj_t *ui_contenuto(void);

/* Rimette il titolo e il sottotitolo della testata di sezione, e riporta
   l'orologio al suo posto a destra. */
void ui_testata(const char *titolo, const char *sottotitolo);

/* Il posto a destra nella testata, svuotato: chiamandolo l'orologio se ne
   va, perche nelle schermate paginate lo sostituisce il paginatore
   (01-specifica-ui.md §1). */
lv_obj_t *ui_testata_destra(void);

/* A label with the time that keeps itself current: ui_orologi_aggiorna()
   rewrites every one still alive, and each forgets itself when its screen
   is destroyed. */
lv_obj_t *ui_orologio(lv_obj_t *padre, font_ruolo_t font);

/* Once a second from the main loop: header clocks, full-screen states and
   the home clock. Cheap when the minute has not changed — ui_scrivi()
   compares before writing. */
void ui_orologi_aggiorna(void);

#endif /* UI_H */
