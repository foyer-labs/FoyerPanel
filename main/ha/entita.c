/* ------------------------------------------------------------------------
 * Lo stato delle entita — magazzino e ricostruzione dell'elenco.
 * --------------------------------------------------------------------- */
#include "entita.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "validazione.h"   /* cfg_albero() */

static uint32_t generazione;

typedef struct {
    /* Novantasei e non sessantaquattro: gli identificatori generati da
       Home Assistant dal nome dell'automazione arrivano lunghi, e
       "automation.condizionatore_camera_padronale_fine_timer"
       ne vuole settantasei. A sessantaquattro venivano troncati in
       silenzio, e un identificatore troncato non corrisponde a niente: si
       seguiva un'entita che non poteva esistere. */
    char     id[96];
    char     stato[40];
    cJSON   *attributi;      /* NULL finche non ne arriva uno */
    uint32_t aggiornata_ms;
    /* L'impronta di stato e attributi messi insieme. Serve a distinguere
       "e arrivato un aggiornamento" da "e cambiato qualcosa": Home Assistant
       ripete lo stesso valore ogni volta che una sonda riferisce, e su una
       casa vera sono decine di messaggi al secondo che non dicono niente di
       nuovo. Confrontare il testo degli attributi vorrebbe dire riserializzarli
       a ogni messaggio; un numero di trentadue bit no. */
    uint32_t impronta;
    bool     vista;
} voce_t;

/* FNV-1a: due righe, nessuna dipendenza, e per accorgersi che due valori
   sono diversi basta e avanza. Non e una firma: se due stati diversi
   dessero lo stesso numero si perderebbe un ridisegno, non un dato. */
static uint32_t impronta_di(const char *s, uint32_t h)
{
    for (; s && *s; s++) {
        h ^= (uint8_t)*s;
        h *= 16777619u;
    }
    return h;
}

/* --- la tabella, grande quanto serve ------------------------------------
 *
 * Era un vettore fisso da ENTITA_MAX. Comodo, e caro nel posto sbagliato:
 * una voce sono centocinquantadue byte, il massimo e centonovantadue, e
 * fanno ventinove kilobyte di **RAM interna** occupati all'accensione e
 * per sempre — perche un vettore statico non sta in PSRAM, ci sta e basta
 * dove la RAM e poca.
 *
 * La configurazione di casa ne segue settantatre. Le altre
 * centodiciannove voci non erano margine: erano diciassette kilobyte
 * interni tenuti da parte per entita che non esistono, sullo stesso chip
 * dove il minimo storico di memoria libera era arrivato a zero.
 *
 * Ora si cresce a blocchi mentre si legge la configurazione. ENTITA_MAX
 * resta, ma come limite e non come prenotazione. */
#define BLOCCO 16
static voce_t *voci;
static int     quante, capaci;

/* Falso se la memoria non c'e: chi chiama smette di seguire entita invece
   di scrivere fuori dal vettore. */
static bool fai_posto(void)
{
    if (quante < capaci) return true;
    if (capaci >= ENTITA_MAX) return false;

    int nuova = capaci + BLOCCO;
    if (nuova > ENTITA_MAX) nuova = ENTITA_MAX;

    voce_t *piu = realloc(voci, (size_t)nuova * sizeof *voci);
    if (!piu) return false;

    memset(piu + capaci, 0, (size_t)(nuova - capaci) * sizeof *piu);
    voci = piu;
    capaci = nuova;
    return true;
}

/* --- elenco di quelle da seguire ---------------------------------------- */

/* Un identificatore di Home Assistant e `dominio.oggetto`: un punto, in
   mezzo, con roba a destra e a sinistra. Serve a distinguerlo dagli altri
   testi che stanno in configurazione — nomi di zone, fusi orari, URL — che
   un punto ce l'hanno anche loro. */
static bool sembra_entita(const char *s)
{
    if (!s || !*s) return false;

    /* **Un punto solo.** Un identificatore di Home Assistant e
       `dominio.oggetto` e non ne ha mai due. Senza questo controllo
       "it.pool.ntp.org", che sta nella stessa configurazione ed e fatto di
       sole minuscole e punti, verrebbe seguito come se fosse un'entita — e
       resterebbe per sempre fra quelle "configurate ma inesistenti",
       rendendo inutile proprio il conto che serve a dire se la
       configurazione parla della casa collegata. */
    const char *punto = strchr(s, '.');
    if (!punto || punto == s || !punto[1]) return false;
    if (strchr(punto + 1, '.')) return false;
    /* Niente barre e niente due punti: cosi "192.0.2.10" e
       "http://x.y/z" restano fuori. */
    if (strchr(s, '/') || strchr(s, ':')) return false;
    for (const char *p = s; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')
              || *p == '_' || *p == '.'))
            return false;
    /* Un indirizzo IP passerebbe i controlli di sopra se non fosse per le
       cifre all'inizio: un dominio comincia sempre con una lettera. */
    return s[0] >= 'a' && s[0] <= 'z';
}

static voce_t *trova(const char *id)
{
    if (!id) return NULL;
    for (int n = 0; n < quante; n++)
        if (strcmp(voci[n].id, id) == 0) return &voci[n];
    return NULL;
}

bool ent_segui(const char *id)
{
    if (!sembra_entita(id)) return false;
    if (trova(id)) return true;
    if (!fai_posto()) return false;

    /* Uno che non ci sta si rifiuta invece di troncarlo: un identificatore
       troncato non corrisponde a nessuna entita e resterebbe per sempre
       fra le "configurate ma inesistenti", che e un modo silenzioso di
       mentire. Meglio non seguirlo e dirlo. */
    if (strlen(id) >= sizeof voci[0].id) return false;

    voce_t *v = &voci[quante++];
    memset(v, 0, sizeof *v);
    snprintf(v->id, sizeof v->id, "%s", id);
    return true;
}

/* Scende nell'albero della configurazione e prende ogni stringa che
   somigli a un identificatore. Cosi non c'e un elenco di percorsi da
   tenere allineato allo schema: un campo nuovo che contiene un'entita
   viene seguito senza toccare questo file. */
static void raccogli(const cJSON *nodo)
{
    for (const cJSON *c = nodo ? nodo->child : NULL; c; c = c->next) {
        if (cJSON_IsString(c)) ent_segui(c->valuestring);
        else if (cJSON_IsObject(c) || cJSON_IsArray(c)) raccogli(c);
    }
}

void ent_ricostruisci_elenco(void)
{
    for (int n = 0; n < quante; n++)
        if (voci[n].attributi) cJSON_Delete(voci[n].attributi);
    /* Si azzera quello che c'e, non tutto il massimo possibile: `voci` ora
       e grande `capaci`, non ENTITA_MAX, e `sizeof voci` sarebbe la
       dimensione del puntatore. */
    if (voci) memset(voci, 0, (size_t)capaci * sizeof *voci);
    quante = 0;
    raccogli(cfg_albero());
}

int ent_seguite(void) { return quante; }

/* Cresce di uno a ogni cambiamento vero. Chi disegna la guarda invece
   dell'orologio: ricostruire perche e passato un secondo e lavoro fatto per
   niente, e su questo pannello quel lavoro si vede — il ridisegno pieno di
   una schermata occupa il bus della PSRAM da cui lo schermo sta leggendo. */
uint32_t ent_generazione(void) { return generazione; }

const char *ent_seguita(int n)
{
    return (n >= 0 && n < quante) ? voci[n].id : "";
}

bool ent_seguita_e(const char *id) { return trova(id) != NULL; }

/* --- quello che si sa --------------------------------------------------- */

bool ent_aggiorna(const char *id, const char *stato, const char *attributi_json,
                  uint32_t adesso_ms)
{
    voce_t *v = trova(id);
    if (!v) return false;      /* non la seguiamo: si scarta senza rumore */

    const uint32_t adesso_impronta =
        impronta_di(attributi_json, impronta_di(stato, 2166136261u));
    if (!v->vista || adesso_impronta != v->impronta) {
        v->impronta = adesso_impronta;
        generazione++;
    }

    snprintf(v->stato, sizeof v->stato, "%s", stato ? stato : "");
    v->aggiornata_ms = adesso_ms;
    v->vista = true;

    if (attributi_json) {
        cJSON *nuovi = cJSON_Parse(attributi_json);
        /* Si sostituisce solo se il nuovo si e letto: attributi illeggibili
           non devono cancellare quelli buoni di prima. */
        if (nuovi) {
            if (v->attributi) cJSON_Delete(v->attributi);
            v->attributi = nuovi;
        }
    }
    return true;
}

void ent_dimentica_valori(void)
{
    /* Dimenticare e un cambiamento come un altro, anzi il piu importante:
       se il collegamento cade e nessuno lo conta, lo schermo resta con la
       fotografia di prima e la mostra come se fosse di adesso. */
    generazione++;

    for (int n = 0; n < quante; n++) {
        if (voci[n].attributi) cJSON_Delete(voci[n].attributi);
        voci[n].attributi = NULL;
        voci[n].stato[0] = 0;
        voci[n].vista = false;
        voci[n].aggiornata_ms = 0;
    }
}

bool ent_vista(const char *id)
{
    const voce_t *v = trova(id);
    return v && v->vista;
}

bool ent_disponibile(const char *id)
{
    const voce_t *v = trova(id);
    if (!v || !v->vista) return false;
    /* `unavailable` e `unknown` sono due cose diverse per Home Assistant —
       integrazione giu contro valore non ancora noto — ma per il pannello
       sono la stessa: non si puo comandare e non si puo mostrare. */
    return strcmp(v->stato, "unavailable") != 0
        && strcmp(v->stato, "unknown") != 0;
}

const char *ent_stato(const char *id)
{
    const voce_t *v = trova(id);
    return (v && v->vista) ? v->stato : "";
}

bool ent_stato_e(const char *id, const char *valore)
{
    return valore && strcmp(ent_stato(id), valore) == 0;
}

double ent_numero(const char *id, double ripiego)
{
    const char *s = ent_stato(id);
    if (!*s) return ripiego;
    char *fine = NULL;
    const double v = strtod(s, &fine);
    /* "on", "unavailable", "" non sono numeri: torna il ripiego invece di
       zero, perche zero e un valore che qualcuno mostrerebbe. */
    return (fine && fine != s) ? v : ripiego;
}

static const cJSON *attributo(const char *id, const char *nome)
{
    const voce_t *v = trova(id);
    if (!v || !v->attributi || !nome) return NULL;
    return cJSON_GetObjectItemCaseSensitive(v->attributi, nome);
}

const char *ent_attributo(const char *id, const char *nome, const char *ripiego)
{
    const cJSON *a = attributo(id, nome);
    return cJSON_IsString(a) && a->valuestring ? a->valuestring : ripiego;
}

double ent_attributo_numero(const char *id, const char *nome, double ripiego)
{
    const cJSON *a = attributo(id, nome);
    return cJSON_IsNumber(a) ? a->valuedouble : ripiego;
}

char *ent_attributo_json(const char *id, const char *nome)
{
    const cJSON *a = attributo(id, nome);
    return a ? cJSON_PrintUnformatted(a) : NULL;
}

void ent_libera_testo(char *s) { if (s) cJSON_free(s); }

bool ent_attributo_vero(const char *id, const char *nome, bool ripiego)
{
    const cJSON *a = attributo(id, nome);
    return cJSON_IsBool(a) ? cJSON_IsTrue(a) : ripiego;
}

uint32_t ent_eta_ms(const char *id, uint32_t adesso_ms)
{
    const voce_t *v = trova(id);
    if (!v || !v->vista) return UINT32_MAX;   /* mai vista: eta infinita */
    return adesso_ms - v->aggiornata_ms;
}

int ent_mai_viste(void)
{
    int n = 0;
    for (int k = 0; k < quante; k++) if (!voci[k].vista) n++;
    return n;
}
