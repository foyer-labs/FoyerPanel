/* ------------------------------------------------------------------------
 * Lo stato delle entita — 03-config-contratto.md §5.
 *
 * Home Assistant manda tutto: in una casa vera sono qualche migliaio di
 * entita, e al pannello ne interessano un centinaio. Qui si tiene **solo
 * quello che la configurazione nomina**, e non e soltanto per risparmiare
 * i 100 kB che 05-architettura-firmware.md §3 concede a configurazione e
 * stato: e anche il modo per sapere che un'entita configurata non esiste.
 * Se si tenesse tutto, "non c'e" e "non l'abbiamo guardata" sarebbero la
 * stessa cosa.
 *
 * Un'entita che non esiste **non e un errore di configurazione**: puo
 * essere un'integrazione temporaneamente giu. Il pannello mostra "non
 * disponibile" in grigio e disattiva i comandi relativi, e il resto della
 * schermata continua a funzionare.
 *
 * Tre stati diversi, e tenerli distinti e tutto il punto:
 *   - **non nominata**: la configurazione non la cita. Non la seguiamo.
 *   - **nominata ma mai vista**: configurata e assente da Home Assistant.
 *   - **vista**: c'e, e ha un valore — che puo comunque essere
 *     `unavailable`, ed e ancora un'altra cosa.
 * --------------------------------------------------------------------- */
#ifndef ENTITA_H
#define ENTITA_H

#include <stdbool.h>
#include <stdint.h>

/* Quante se ne possono seguire **al massimo**. La casa vera ne nomina
   settantatre fra luci, clima, telecamere, accessi ed energia.

   Non e piu una prenotazione: la tabella cresce a blocchi mentre legge la
   configurazione e si ferma dove serve. Quando invece era un vettore
   fisso, questo numero costava ventinove kilobyte di RAM interna
   all'accensione — diciassette dei quali per entita che non esistono, su
   un chip dove il minimo storico di memoria libera era arrivato a zero.
   Ora "il doppio lascia spazio senza pesare" e vero davvero. */
#define ENTITA_MAX 192

/* --- quali seguire ------------------------------------------------------ */

/* Svuota l'elenco e lo ricostruisce da config.json: ogni campo che contiene
   un identificatore di entita diventa una da seguire. Da richiamare quando
   la configurazione cambia. */
void ent_ricostruisci_elenco(void);

/* Aggiunge un identificatore all'elenco di quelli seguiti. Serve al
   ricostruttore e alle prove; ignora i doppioni e le stringhe che non
   somigliano a un identificatore. */
bool ent_segui(const char *id);

int         ent_seguite(void);
const char *ent_seguita(int n);

/* Cresce di uno a ogni cambiamento vero — stato o attributi diversi da
   prima, non semplicemente riferiti di nuovo. Chi disegna la guarda per
   sapere **se** vale la pena rifare qualcosa. */
uint32_t ent_generazione(void);

/* --- quello che si sa --------------------------------------------------- */

/* Registra uno stato arrivato da Home Assistant. `attributi_json` puo
   essere NULL. Vero se l'entita era fra quelle seguite: quelle che non lo
   sono si scartano senza rumore, ed e la maggioranza. */
bool ent_aggiorna(const char *id, const char *stato, const char *attributi_json,
                  uint32_t adesso_ms);

/* Dimentica tutti i valori ma non l'elenco: dopo una caduta del
   collegamento non si sa piu niente, ma si sa ancora cosa chiedere. */
void ent_dimentica_valori(void);

/* Vero se l'entita e nominata dalla configurazione. */
bool ent_seguita_e(const char *id);

/* Vero se ne e arrivato almeno un valore. Falso per un'entita configurata
   che Home Assistant non conosce. */
bool ent_vista(const char *id);

/* Vero se si puo comandare: vista, e con uno stato diverso da
   `unavailable` e `unknown`. E la condizione che disattiva i comandi. */
bool ent_disponibile(const char *id);

/* Lo stato come stringa. "" se non si sa niente: mai un valore inventato,
   perche il pannello non mostra stati che non conosce. */
const char *ent_stato(const char *id);

bool   ent_stato_e(const char *id, const char *valore);
double ent_numero(const char *id, double ripiego);

/* Un attributo. Gli attributi arrivano come oggetto JSON e restano tali:
   quali servano lo decide chi legge, non chi riceve. */
const char *ent_attributo(const char *id, const char *nome, const char *ripiego);
double      ent_attributo_numero(const char *id, const char *nome, double ripiego);

/* Un attributo che non e un valore singolo ma un elenco o un oggetto,
   restituito come testo JSON. Chi chiama lo libera con
   ent_libera_testo(). NULL se l'attributo non c'e.

   Serve ai sensori aggregati — presenza, aperture — dove il template in
   Home Assistant mette una lista di oggetti: e la forma in cui una lista
   di cose con dei campi si scrive senza inventare separatori da
   riparsare. */
char *ent_attributo_json(const char *id, const char *nome);
void  ent_libera_testo(char *s);
bool        ent_attributo_vero(const char *id, const char *nome, bool ripiego);

/* Da quanti millisecondi non arriva un valore nuovo. Serve a mostrare i
   dati sbiaditi con l'ora dell'ultimo valore valido. */
uint32_t ent_eta_ms(const char *id, uint32_t adesso_ms);

/* Quante fra quelle seguite non si sono mai viste: e il numero che dice se
   la configurazione parla di una casa diversa da quella collegata. */
int ent_mai_viste(void);

#endif /* ENTITA_H */
