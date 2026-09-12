/* ------------------------------------------------------------------------
 * Registro delle schermate.
 *
 * Ogni sezione sa costruirsi dentro un contenitore che le viene dato gia
 * dimensionato e con il padding applicato. Non conosce il rail, la barra ne
 * il dock: di quelli si occupa ui.c.
 *
 * Le schermate arrivano una per commit; quelle non ancora fatte mostrano un
 * segnaposto che dice cosa manca, invece di una pagina vuota che sembra un
 * difetto.
 * --------------------------------------------------------------------- */
#ifndef SCHERMATE_H
#define SCHERMATE_H

#include "lvgl.h"
#include "sezioni.h"

/* Costruisce la sezione dentro `contenitore` e imposta la testata. */
void schermata_costruisci(sezione_t s, lv_obj_t *contenitore);

/* --- le singole schermate ---------------------------------------------- */
void schermata_home(lv_obj_t *c);
void schermata_luci(lv_obj_t *c);
void schermata_clima(lv_obj_t *c);

/* Il dettaglio di un condizionatore, che e una vista dentro il clima. */
void schermata_clima_dettaglio(lv_obj_t *c, int indice);
int  schermata_clima_vista_dettaglio(int indice);

/* Su quale pagina aprire il dettaglio del condizionatore, che in verticale
   sono due. Serve alle catture, che non possono premere le frecce; a bordo
   la pagina la sceglie il dito. */
void schermate_pagina(int p);
void schermata_clima_pagina(int p);
void schermata_energia(lv_obj_t *c);
void schermata_accessi(lv_obj_t *c);
void schermata_programmazioni(lv_obj_t *c);
void schermata_agenda(lv_obj_t *c);
void schermata_wifi(lv_obj_t *c);
void schermata_interruttori(lv_obj_t *c);
void schermata_robot(lv_obj_t *c);

/* Il triangolo degli avvisi del robot, o NULL se non c'e niente da
   segnalare. Lo mettono in due — la testata della sezione e la riga in home
   — e sta in un posto solo perche' due disegni dello stesso segno si
   allontanano al primo che ne cambia uno. Porta alla vista degli avvisi. */
lv_obj_t *robot_triangolo(lv_obj_t *padre);
void schermata_impostazioni(lv_obj_t *c);
void schermata_diagnostica(lv_obj_t *c);

/* L'ora sulla home, aggiornata senza ricostruire niente. Non fa niente se
   la home non e a schermo. */
void home_orologio_aggiorna(void);

#endif /* SCHERMATE_H */
