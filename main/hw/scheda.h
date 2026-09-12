/* ------------------------------------------------------------------------
 * La scheda su cui si sta compilando.
 *
 * Un solo posto che sceglie, invece di un `#ifdef` in cima a ogni file che
 * ha bisogno di un piedino. Oggi la scheda e una, e il ramo unico sembra
 * un lusso: resta perche la differenza non e di stile. Con la scelta ripetuta,
 * aggiungere una seconda scheda vuol dire trovare tutti gli `#ifdef`, e
 * quello che si dimentica non da errore — compila contro i piedini
 * sbagliati.
 *
 * Il bersaglio lo decide `idf.py set-target`, e da li discende tutto: il
 * profilo predefinito (firmware/CMakeLists.txt), la configurazione
 * (sdkconfig.defaults.<bersaglio>), le partizioni, e questo file.
 * --------------------------------------------------------------------- */
#ifndef SCHEDA_H
#define SCHEDA_H

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "scheda_p4.h"
#else
#error "scheda sconosciuta: aggiungi il suo scheda_*.h qui"
#endif

#endif /* SCHEDA_H */
