/* ------------------------------------------------------------------------
 * L'ora, e da dove viene.
 *
 * Sul PC la da il sistema operativo e c'e sempre. Sul pannello no: all'
 * accensione l'orologio parte dal 1970 e resta li finche la rete non lo
 * corregge, e mostrare "01:00" con la faccia di chi sa che ore sono e
 * peggio che non mostrare niente. Di qui orologio_valido(), che distingue
 * "non lo so ancora" da "sono le sei e mezza".
 *
 * Il fuso arriva da config.json come nome IANA — "Europe/Rome" — e passa da
 * fusi.h per diventare quello che il sistema capisce. Vedi li perche non si
 * puo darglielo com'e.
 * --------------------------------------------------------------------- */
#ifndef OROLOGIO_H
#define OROLOGIO_H

#include <stdbool.h>
#include <stddef.h>

/* Applica il fuso di `sistema/fuso_orario`. Falso se quel nome non si sa
   tradurre: chi chiama lo dica, invece di lasciare il pannello su UTC senza
   che nessuno lo sappia. */
bool orologio_fuso_da_configurazione(void);

/* Vero quando l'ora e credibile. La soglia e il 2025: un orologio non
   sincronizzato parte dal 1970, e qualunque anno prima di quando questo
   firmware e stato scritto vuol dire che nessuno l'ha ancora corretto. */
bool orologio_valido(void);

/* "18:41". Se l'ora non e valida scrive "--:--", che e la stessa cosa che
   si mostrava prima ma decisa in un posto solo. */
void orologio_ora(char *buf, size_t n);

/* "lunedi 25 agosto", in minuscolo perche va sotto un orario grande. Vuoto
   se l'ora non e valida. */
void orologio_data(char *buf, size_t n);

/* Minuti dalla mezzanotte, 0..1439, oppure **-1** se l'ora non e credibile.
   Serve a chi confronta l'adesso con un orario scritto in configurazione, e
   il -1 non e pigrizia: un pannello appena acceso crede che siano le 01:00
   del 1970, e una regola notturna che gli credesse spegnerebbe lo schermo a
   ogni riavvio diurno. */
int orologio_minuti(void);

/* La **mezzanotte locale di oggi**, scritta in UTC come la vuole Home
   Assistant: "2026-08-30T22:00:00+00:00". Falso se l'ora non e credibile.

   Sta qui e non in chi la usa perche il fuso e uno solo e la conversione va
   fatta in un posto solo: due punti che calcolano «da quando e oggi»
   finiscono per non essere d'accordo il giorno del cambio d'ora. */
bool orologio_mezzanotte_iso(char *buf, size_t n);

/* L'ora locale (0..23) di un istante dato in **millisecondi UTC**, che e
   come li scrive Home Assistant nelle statistiche. -1 se non si sa. */
int orologio_ora_di(long long ms_utc);

/* --- gli istanti che arrivano da Home Assistant -------------------------
 *
 * Un timer di Home Assistant dice quando finisce, non quanto manca:
 * `finishes_at` e un istante assoluto in ISO 8601, e l'attributo
 * `remaining` **resta fermo** al valore d'inizio finche il timer corre.
 * Chi vuole un conto alla rovescia se lo calcola, e per calcolarlo servono
 * due cose che qui stanno vicine: leggere quell'istante e sapere che ore
 * sono adesso.
 *
 * Stanno qui e non in chi le usa perche sono conversioni di tempo, e le
 * conversioni di tempo sparse sono il modo in cui due punti del programma
 * finiscono per non essere d'accordo il giorno del cambio d'ora. */

/* Adesso, in secondi dall'epoca. **0** se l'ora non e credibile — la stessa
   soglia di orologio_valido(), perche un conto alla rovescia calcolato su
   un orologio fermo al 1970 direbbe numeri con la faccia seria. */
long long orologio_adesso_utc(void);

/* Da "2026-09-06T18:12:00+00:00" ai secondi dall'epoca. Accetta lo stesso
   formato con "Z" al posto del fuso e quello senza fuso, che si legge come
   UTC. **0** se la stringa non si sa leggere: non e un istante valido, ed e
   piu onesto di una data inventata.

   Non passa da mktime(): quella vuole l'ora **locale** e leggerebbe come
   ora di casa un istante che e gia scritto in UTC — un'ora di scarto per
   sei mesi l'anno, che e esattamente il genere di errore che nessuno
   attribuisce a una conversione. */
long long orologio_da_iso(const char *iso);

/* "20:12": l'ora **locale** di un istante dato in secondi dall'epoca.
   Falso se l'istante non e valido o il buffer e troppo corto. */
bool orologio_ora_locale(long long s_utc, char *buf, size_t n);

#endif /* OROLOGIO_H */
