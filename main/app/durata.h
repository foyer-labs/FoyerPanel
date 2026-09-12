/* ------------------------------------------------------------------------
 * Quanto manca, detto in ore.
 *
 * Nasce da una cosa vista sul pannello: i sensori dei consumabili di un
 * robot **non hanno tutti la stessa unità**. Alcuni dicono le ore, altri i
 * secondi, e il pannello mostrava tutti e due come arrivavano — «214 h»
 * accanto a «770400 s». Sono lo stesso dato detto in due lingue, e a un
 * metro e mezzo da un vetro non si convertono a mente.
 *
 * Sta in un file suo, senza nessuna dipendenza, per una ragione sola: così
 * si può provare. Dentro `dati_robot.c` sarebbe stata una funzione statica
 * in mezzo a cose che vogliono la configurazione e Home Assistant, e
 * l'unico modo di verificarla sarebbe stato guardare il vetro.
 * --------------------------------------------------------------------- */
#ifndef DURATA_H
#define DURATA_H

#include <stdbool.h>
#include <stddef.h>

/* Il valore di un sensore, portato in ore.
 *
 * `unita` è quella che Home Assistant allega all'entità: `s`, `min`, `h`,
 * `d`, e le loro forme per esteso. Falso quando non è un tempo — per cento,
 * cicli, o niente — e allora le ore non si inventano: chi chiama mostra il
 * valore com'è. */
bool durata_in_ore(const char *stato, const char *unita, double *ore);

/* Il testo da mostrare, terminato: «214 h», «<1 h», oppure il valore con la
   sua unità quando non è un tempo.
 *
 * **Ore intere, e non decimi.** A un consumabile che dura trecento ore un
 * decimo non cambia niente, e un numero con la virgola su una riga che si
 * legge di sfuggita è rumore. Sotto l'ora però «0 h» direbbe una cosa
 * sbagliata — sembra finito, e finito non è ancora — e allora si scrive
 * «<1 h», che è la stessa informazione senza la bugia. */
void durata_testo(const char *stato, const char *unita, char *fuori, size_t max);

#endif /* DURATA_H */
