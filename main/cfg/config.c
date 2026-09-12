#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archivio.h"
#include "cJSON.h"
#include "validazione.h"

static cJSON *doc;
static errore_cfg_t errori[CFG_ERRORI_MAX];
static int n_errori;

/* --- errori ------------------------------------------------------------- */

void cfg_azzera_errori(void) { n_errori = 0; }

void cfg_aggiungi_errore(gravita_t g, const char *campo, const char *motivo)
{
    if (n_errori >= CFG_ERRORI_MAX) return;
    errore_cfg_t *e = &errori[n_errori++];
    e->gravita = g;
    snprintf(e->campo, sizeof e->campo, "%s", campo ? campo : "");
    snprintf(e->motivo, sizeof e->motivo, "%s", motivo ? motivo : "");
}

int cfg_errori(void) { return n_errori; }

const errore_cfg_t *cfg_errore(int n)
{
    return (n >= 0 && n < n_errori) ? &errori[n] : NULL;
}

/* --- navigazione dell'albero -------------------------------------------- */

/* Percorsi con la barra: "display/brightness_active". Piu corti da scrivere
   di una catena di cJSON_GetObjectItem, e soprattutto uguali ai nomi che la
   validazione mette negli errori e che la pagina web rimanda indietro. */
/* Percorso a barre a partire da un nodo qualunque. Tenerlo separato da
   trova() serve agli accessori sugli array, dove la parte dentro l'elemento
   puo essere annidata a sua volta: "extras/quiet". */
static cJSON *trova_da(cJSON *nodo, const char *percorso)
{
    if (!nodo || !percorso) return NULL;

    const char *p = percorso;
    char pezzo[64];

    while (*p && nodo) {
        size_t n = 0;
        while (p[n] && p[n] != '/' && n < sizeof pezzo - 1) n++;
        memcpy(pezzo, p, n);
        pezzo[n] = 0;
        p += n;
        if (*p == '/') p++;

        if (pezzo[0] >= '0' && pezzo[0] <= '9' && cJSON_IsArray(nodo))
            nodo = cJSON_GetArrayItem(nodo, atoi(pezzo));
        else
            nodo = cJSON_GetObjectItemCaseSensitive(nodo, pezzo);
    }
    return nodo;
}

static cJSON *trova(const char *percorso)
{
    return doc ? trova_da(doc, percorso) : NULL;
}

/* La camminata lungo il percorso e la stessa per testo e vero/falso: la
   differenza e cosa si posa in fondo. Tenerle separate costerebbe due copie
   della parte che sbaglia piu facilmente — quella che crea gli oggetti
   intermedi — quindi qui c'e una funzione sola e due modi di chiuderla. */
typedef enum { POSA_TESTO, POSA_VERO } posa_t;

static bool posa(const char *percorso, posa_t come,
                 const char *testo, bool vero)
{
    if (!doc || !percorso) return false;

    cJSON *nodo = doc;
    const char *p = percorso;
    char pezzo[64];

    while (*p) {
        size_t n = 0;
        while (p[n] && p[n] != '/' && n < sizeof pezzo - 1) n++;
        memcpy(pezzo, p, n);
        pezzo[n] = 0;
        p += n;
        const bool ultimo = (*p != '/');
        if (*p == '/') p++;

        if (ultimo) {
            /* Si toglie e si rimette invece di modificare sul posto:
               cJSON_SetValuestring fallisce se quello che c'e non e una
               stringa — un null, per dire — e quello e proprio il caso in
               cui il valore va sostituito, non lasciato stare. */
            cJSON_DeleteItemFromObjectCaseSensitive(nodo, pezzo);
            return come == POSA_TESTO
                ? cJSON_AddStringToObject(nodo, pezzo, testo) != NULL
                : cJSON_AddBoolToObject(nodo, pezzo, vero) != NULL;
        }

        cJSON *giu = cJSON_GetObjectItemCaseSensitive(nodo, pezzo);
        if (!giu) giu = cJSON_AddObjectToObject(nodo, pezzo);
        /* Se lungo la strada c'e qualcosa che non e un oggetto ci si ferma:
           trasformarlo in oggetto vorrebbe dire buttare via un dato che
           qualcuno aveva messo li apposta. */
        if (!cJSON_IsObject(giu)) return false;
        nodo = giu;
    }
    return false;
}

bool cfg_imposta_vero(const char *percorso, bool valore)
{
    return posa(percorso, POSA_VERO, NULL, valore);
}

bool cfg_imposta_testo(const char *percorso, const char *valore)
{
    return valore ? posa(percorso, POSA_TESTO, valore, false) : false;
}

const char *cfg_testo(const char *percorso, const char *ripiego)
{
    const cJSON *v = trova(percorso);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : ripiego;
}

/* --- un numero scritto fra virgolette --------------------------------------
 *
 * Lo schema dice `integer` e il documento dovrebbe avere un numero. A volte
 * ha `"1000"`, ed e successo davvero: la pagina di configurazione non
 * riconosceva come numerici i campi facoltativi — quelli scritti
 * `anyOf: [{integer}, {null}]`, perche' cosi il tipo non sta piu' al primo
 * livello — e li salvava come testo.
 *
 * Rifiutarlo e' formalmente giusto e praticamente pessimo: cfg_intero()
 * ripiegava sul valore di riposo, e per `potenza_massima` quel valore e'
 * zero, e con zero la barretta del consumo **non si disegna**. Nessun
 * errore, nessun avviso: una barra che non compare e chi guarda che pensa
 * al disegno.
 *
 * Quindi si legge anche la stringa, purche' sia tutta cifre. Non e'
 * indulgenza verso i dati sbagliati: e' che questo documento lo scrivono
 * versioni diverse della stessa pagina, e una configurazione salvata ieri
 * deve continuare a valere domani. Chi scrive resta severo — la pagina
 * adesso salva numeri — e chi legge resta tollerante.
 *
 * Quello che **non** si accetta: "1000 W", "  12", "" e "12.5" (per un
 * intero). Una stringa che non e' un numero intero e basta vale il ripiego,
 * come prima. */
static bool intero_da_testo(const cJSON *v, int32_t *fuori)
{
    if (!cJSON_IsString(v) || !v->valuestring || !v->valuestring[0])
        return false;

    const char *s = v->valuestring;
    const bool meno = *s == '-';
    if (meno || *s == '+') s++;
    if (!*s) return false;

    long long n = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return false;
        n = n * 10 + (*s - '0');
        if (n > 2147483647LL) return false;   /* non ci sta: vale il ripiego */
    }
    *fuori = (int32_t)(meno ? -n : n);
    return true;
}

const char *cfg_nome_host(char *buf, size_t n)
{
    if (!buf || n == 0) return buf;

    const char *nome = cfg_testo("system/panel_name", "pannello");
    size_t j = 0;
    for (size_t i = 0; nome[i] && j < n - 1; i++) {
        const char c = nome[i];
        if (c >= 'A' && c <= 'Z')      buf[j++] = (char)(c - 'A' + 'a');
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            buf[j++] = c;
        else if (c == ' ' || c == '-') buf[j++] = '-';
    }
    buf[j] = 0;

    /* Un nome fatto di soli caratteri che cadono lascerebbe una stringa
       vuota, e `.local` da solo non e' un indirizzo. */
    if (!buf[0]) {
        const char *r = "pannello";
        size_t k = 0;
        while (r[k] && k < n - 1) { buf[k] = r[k]; k++; }
        buf[k] = 0;
    }
    return buf;
}

int32_t cfg_intero(const char *percorso, int32_t ripiego)
{
    const cJSON *v = trova(percorso);
    if (cJSON_IsNumber(v)) return (int32_t)v->valuedouble;
    int32_t n;
    return intero_da_testo(v, &n) ? n : ripiego;
}

double cfg_decimale(const char *percorso, double ripiego)
{
    const cJSON *v = trova(percorso);
    return cJSON_IsNumber(v) ? v->valuedouble : ripiego;
}

bool cfg_vero(const char *percorso, bool ripiego)
{
    const cJSON *v = trova(percorso);
    return cJSON_IsBool(v) ? cJSON_IsTrue(v) : ripiego;
}

int cfg_quanti(const char *percorso)
{
    const cJSON *v = trova(percorso);
    return cJSON_IsArray(v) ? cJSON_GetArraySize(v) : 0;
}

static cJSON *elemento(const char *array, int n, const char *campo)
{
    cJSON *a = trova(array);
    if (!cJSON_IsArray(a)) return NULL;
    cJSON *e = cJSON_GetArrayItem(a, n);
    if (!e) return NULL;
    return campo ? trova_da(e, campo) : e;
}

const char *cfg_testo_in(const char *array, int n, const char *campo,
                         const char *ripiego)
{
    const cJSON *v = elemento(array, n, campo);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : ripiego;
}

int32_t cfg_intero_in(const char *array, int n, const char *campo,
                      int32_t ripiego)
{
    const cJSON *v = elemento(array, n, campo);
    if (cJSON_IsNumber(v)) return (int32_t)v->valuedouble;
    int32_t k;
    return intero_da_testo(v, &k) ? k : ripiego;
}

bool cfg_vero_in(const char *array, int n, const char *campo, bool ripiego)
{
    const cJSON *v = elemento(array, n, campo);
    return cJSON_IsBool(v) ? cJSON_IsTrue(v) : ripiego;
}

bool cfg_ha_in(const char *array, int n, const char *campo)
{
    const cJSON *v = elemento(array, n, campo);
    return v && !cJSON_IsNull(v);
}

cJSON *cfg_albero(void) { return doc; }

/* --- migrazioni --------------------------------------------------------- */

/* Ogni campo nuovo ha un valore predefinito sensato, cosi una migrazione non
   chiede mai niente all'utente (§3). Sono funzioni separate e in sequenza:
   chi arriva da uno schema molto vecchio le attraversa tutte. */
static void migra_1_2(cJSON *c)
{
    /* Lo schema 1 non aveva il blocco `home`: si crea vuoto. Ci metteva
       dentro `striscia_telecamere` acceso — la striscia di anteprime in
       home — e quel campo lo toglie la migrazione 4→5, insieme al resto del
       video. Crearlo qui per cancellarlo dopo sarebbe una strada che gira su
       se stessa; il blocco pero serve, perche gli schemi successivi ci
       scrivono dentro. */
    if (!cJSON_GetObjectItem(c, "home")) cJSON_AddObjectToObject(c, "home");
}

/* Lo schema 2 aveva **una** rete privata. Le case ne hanno piu di una, e
   sceglierne una sarebbe stato decidere al posto di chi configura: quella
   che c'era diventa il primo elemento dell'elenco.

   Quello che questa migrazione **non** porta con se e la password: lo
   schema 2 condivideva quella della rete a cui il pannello e collegato, e
   oggi ogni rete privata ha il suo segreto in NVS. I segreti non passano da
   qui — config.c non conosce NVS, ed e giusto cosi — quindi la password
   della prima rete privata va reinserita una volta dalla pagina di
   configurazione. Il difetto e visibile subito (il QR non funziona), non
   silenzioso. */
static void migra_2_3(cJSON *c)
{
    cJSON *cw = cJSON_GetObjectItem(c, "condivisione_wifi");
    if (!cw) return;

    cJSON *vecchia = cJSON_DetachItemFromObject(cw, "privata");
    if (!vecchia) return;
    if (cJSON_GetObjectItem(cw, "private")) { cJSON_Delete(vecchia); return; }

    cJSON *elenco = cJSON_AddArrayToObject(cw, "private");
    if (elenco) cJSON_AddItemToArray(elenco, vecchia);
    else        cJSON_Delete(vecchia);
}

/* Via il PIN, e con lui i campi che lo governavano. Vanno **tolti** e non
   solo ignorati: lo schema non li ammette piu (`additionalProperties`
   falso), quindi una configurazione che se li portasse dietro verrebbe
   rifiutata al primo salvataggio — e chi configura si vedrebbe un errore su
   un campo che non ha mai scritto e che la pagina non gli mostra.

   E l'eccezione alla regola di §8 sui campi sconosciuti da conservare: li
   si conserva perche potrebbero servire a un altro pannello, ma questi non
   servono piu a nessuno e in piu fanno danno. */
static const char *const CAMPI_DEL_PIN[] = {
    "richiedi_pin", "tentativi_pin", "minuti_blocco", "secondi_visibilita",
};

/* Li cerca **dovunque possano essere**: allo schema 2 stavano dentro le
   reti, allo schema 3 quelli del blocco erano saliti accanto alle reti. Chi
   arriva da lontano ha attraversato tutte e due le disposizioni, e una
   pulizia che ne conoscesse una sola lascerebbe indietro l'altra.

   cJSON_DeleteItemFromObject regge un oggetto nullo e regge un nodo che
   oggetto non e: nessuna delle due cose va cercata prima. */
static void via_il_pin(cJSON *o)
{
    for (unsigned n = 0; n < sizeof CAMPI_DEL_PIN / sizeof CAMPI_DEL_PIN[0]; n++)
        cJSON_DeleteItemFromObject(o, CAMPI_DEL_PIN[n]);
}

static void migra_3_4(cJSON *c)
{
    cJSON *cw = cJSON_GetObjectItem(c, "condivisione_wifi");
    if (!cw) return;

    via_il_pin(cw);
    via_il_pin(cJSON_GetObjectItem(cw, "ospiti"));
    cJSON *r = NULL;
    cJSON_ArrayForEach(r, cJSON_GetObjectItem(cw, "private")) via_il_pin(r);
}

/* --- via le telecamere -------------------------------------------------
 *
 * Non basta smettere di leggerle: lo schema non ammette campi che non
 * conosce (`additionalProperties` falso), quindi una configurazione che si
 * portasse dietro il blocco `telecamere` verrebbe **rifiutata al primo
 * salvataggio** — e chi configura si vedrebbe un errore su un campo che non
 * ha mai scritto e che la pagina non gli mostra piu. E' successo la stessa
 * cosa col PIN, ed e la ragione per cui quella migrazione esiste.
 *
 * Se ne va anche la voce "telecamere" dall'elenco delle sezioni, che ora
 * non e piu fra quelle ammesse, e `home.striscia_telecamere`, che comandava
 * le anteprime nella home.
 *
 * `accessi[].telecamera` invece resta dov'e: e' un campo che lo schema
 * nuovo non ammette, quindi va tolto anche lui — un accesso che nomina una
 * telecamera che non esiste piu non ha niente da mostrare. */
static void migra_4_5(cJSON *c)
{
    cJSON_DeleteItemFromObject(c, "telecamere");
    cJSON_DeleteItemFromObject(cJSON_GetObjectItem(c, "home"),
                               "striscia_telecamere");

    cJSON *a = NULL;
    cJSON_ArrayForEach(a, cJSON_GetObjectItem(c, "accessi"))
        cJSON_DeleteItemFromObject(a, "telecamera");

    /* L'elenco delle sezioni e un array di stringhe: si ricostruisce senza
       quella, invece di cancellare mentre lo si percorre. */
    cJSON *sez = cJSON_GetObjectItem(c, "sezioni");
    if (!cJSON_IsArray(sez)) return;

    cJSON *tenute = cJSON_CreateArray();
    if (!tenute) return;

    cJSON *s = NULL;
    cJSON_ArrayForEach(s, sez) {
        if (cJSON_IsString(s) && strcmp(s->valuestring, "telecamere") == 0)
            continue;
        cJSON *copia = cJSON_Duplicate(s, true);
        if (copia) cJSON_AddItemToArray(tenute, copia);
    }
    cJSON_ReplaceItemInObject(c, "sezioni", tenute);
}

/* --- 5 → 6 → 7: le telecamere, andate e tornate e andate ----------------
 *
 * Lo schema 6 le riammetteva in RTSP e lo schema 7 le toglie di nuovo,
 * nella stessa giornata. Chi arriva da 5 attraversa il 6 senza vederlo, e
 * va benissimo: le due migrazioni restano scritte separate perche'
 * raccontano due decisioni diverse, e una configurazione salvata allo
 * schema 6 esiste davvero — c'e' stata sul pannello di casa per un'ora.
 *
 * Il perche' della rinuncia sta in 05-architettura-firmware.md §5, con i
 * numeri. In breve: questo silicio l'H.264 lo decodifica in software, e i
 * flussi di casa sono sopra il profilo baseline, che quel decodificatore
 * e' il solo che accetti. */
static void migra_5_6(cJSON *c)
{
    (void)c;
}

static void migra_6_7(cJSON *c)
{
    cJSON_DeleteItemFromObject(c, "telecamere");

    /* L'elenco delle sezioni si ricostruisce senza quella, invece di
       cancellare mentre lo si percorre. */
    cJSON *sez = cJSON_GetObjectItem(c, "sezioni");
    if (!cJSON_IsArray(sez)) return;

    cJSON *tenute = cJSON_CreateArray();
    if (!tenute) return;

    cJSON *s = NULL;
    cJSON_ArrayForEach(s, sez) {
        if (cJSON_IsString(s) && strcmp(s->valuestring, "telecamere") == 0)
            continue;
        cJSON *copia = cJSON_Duplicate(s, true);
        if (copia) cJSON_AddItemToArray(tenute, copia);
    }
    cJSON_ReplaceItemInObject(c, "sezioni", tenute);
}

/* --- 7 → 8: i colori si possono scegliere -------------------------------
 *
 * Niente da spostare: `aspetto` e facoltativo, e una configurazione che non
 * ce l'ha usa i colori con cui il pannello e stato disegnato — che sono gli
 * stessi di prima, byte per byte.
 *
 * Il numero sale lo stesso, e per la ragione di sempre:
 * `additionalProperties` e falso, quindi una configurazione con dentro
 * `aspetto`, aperta da un firmware allo schema 7, verrebbe rifiutata con un
 * errore su un campo che chi configura non ha mai scritto a mano. Col
 * numero a 8 quel firmware dice invece la cosa giusta — «schema piu nuovo
 * del mio» — che e un messaggio da cui si capisce cosa fare. */
static void migra_7_8(cJSON *c)
{
    (void)c;
}

/* --- 8 → 9: gli avvisi del robot ----------------------------------------
 *
 * Due elenchi nuovi dentro `robot`, tutti e due facoltativi: niente da
 * spostare. Il numero sale per la ragione di sempre — `additionalProperties`
 * e falso, e una configurazione che li contiene, aperta da un firmware allo
 * schema 8, verrebbe rifiutata con un errore su un campo che chi configura
 * non ha mai scritto a mano. */
static void migra_8_9(cJSON *c)
{
    (void)c;
}

/* --- 9 → 10: the panel speaks more than one language ----------------
 *
 * Nothing to move: `sistema.lingua` is optional, and a configuration
 * without it gets English. The number goes up for the usual reason —
 * `additionalProperties` is false, and a configuration that names a
 * language, opened by a schema-9 firmware, would be refused over a field
 * the user never typed by hand.
 *
 * A configuration migrated from 9 does **not** get `"lingua": "it"`, even
 * though every schema-9 panel spoke Italian: the migration cannot know who
 * reads the panel, and English is the documented default. The first-boot
 * screens and the settings are where the language is chosen. */
static void migra_9_10(cJSON *c)
{
    (void)c;
}

/* --- 10 → 11: the configuration speaks English ------------------------
 *
 * Every key and every enumerated value was Italian — `sistema`,
 * `nome_pannello`, `tutta_la_casa` — because the panel was written for one
 * house in Italy. A project anyone can install needs a configuration anyone
 * can read, and it had to happen before the first release: after it, every
 * rename breaks someone's file.
 *
 * The keys are renamed without context: the same Italian name becomes the
 * same English name wherever it appears. That was checked on the whole
 * schema — no Italian name meant two different things — and it is what
 * makes this a flat table instead of a map of paths. `attivo` and `attiva`
 * both become `enabled`: no object had both.
 *
 * Values are renamed by path, because a word like "acceso" is a value in
 * one place and just text in another.
 *
 * This is also the door from the original Italian project to this one: a
 * schema-10 config.json from either loads here and comes out in English. */
static const struct { const char *da, *a; } CHIAVI_11[] = {
    { "a", "to" }, { "accensione", "on_time" },
    { "accento", "accent" }, { "accessi", "access" },
    { "agenda", "calendar" }, { "altezza", "height" },
    { "aria_fresca", "fresh_air" }, { "aspetto", "appearance" },
    { "attiva", "enabled" }, { "attivo", "enabled" },
    { "attributi", "attributes" }, { "attributo", "attribute" },
    { "attributo_temperatura_locale", "local_temperature_attribute" }, { "automazione_timer", "timer_automation" },
    { "automazioni", "automations" }, { "avvisi", "alerts" },
    { "batteria", "battery" }, { "batteria_pct", "battery_pct" },
    { "batteria_positiva", "battery_positive" }, { "batteria_w", "battery_w" },
    { "bordi", "borders" }, { "calendari", "calendars" },
    { "casa_w", "house_w" }, { "ciclo", "cycle" },
    { "clima", "climate" }, { "colore", "color" },
    { "comando_stanze", "rooms_command" }, { "condivisione_wifi", "wifi_sharing" },
    { "condizionatori", "air_conditioners" }, { "conferma", "confirm" },
    { "configurazione_sempre_aperta", "config_always_open" }, { "consumato", "consumed" },
    { "corrente", "current" }, { "da", "from" },
    { "diagnostica", "diagnostics" }, { "dimmerabile", "dimmable" },
    { "dispositivi", "devices" }, { "durata_predefinita", "default_duration" },
    { "elettrodomestici", "appliances" }, { "endpoint_stato", "status_endpoint" },
    { "energia", "energy" }, { "entita", "entity" },
    { "etichetta", "label" }, { "extra", "extras" },
    { "finestre", "windows" }, { "forma_stanze", "rooms_format" },
    { "fuso_orario", "timezone" }, { "giornata", "today" },
    { "giorni", "days" }, { "icona", "icon" },
    { "immesso", "exported" }, { "impulso_ms", "pulse_ms" },
    { "intermittente", "intermittent" }, { "interruttore", "switch" },
    { "interruttori", "switches" }, { "larghezza", "width" },
    { "limiti_condizionatori", "ac_limits" }, { "limiti_riscaldamento", "heating_limits" },
    { "lingua", "language" }, { "luce_pannello", "panel_light" },
    { "luci", "lights" }, { "luci_accese", "lights_on" },
    { "luminosita_attiva", "brightness_active" }, { "luminosita_standby", "brightness_standby" },
    { "manutenzioni", "maintenance" }, { "max_eventi_home", "home_max_events" },
    { "meteo", "weather" }, { "minuti_acceso", "minutes_on" },
    { "minuti_spento", "minutes_off" }, { "mostra_password", "show_password" },
    { "ms_pressione_prolungata", "long_press_ms" }, { "nascosta", "hidden" },
    { "negativo", "negative" }, { "nome", "name" },
    { "nome_mostrato", "display_name" }, { "nome_pannello", "panel_name" },
    { "numero", "number" }, { "oggi_kwh", "today_kwh" },
    { "orologio", "clock" }, { "passo", "step" },
    { "percorso_ws", "ws_path" }, { "piani", "floors" },
    { "piano", "floor" }, { "piano_pannello", "panel_floor" },
    { "porta", "port" }, { "positivo", "positive" },
    { "potenza", "power" }, { "potenza_massima", "max_power" },
    { "prelevato", "imported" }, { "presenza", "presence" },
    { "prodotto", "produced" }, { "profilo", "profile" },
    { "programmazioni", "schedules" }, { "quando", "when" },
    { "rete", "network" }, { "rete_positiva", "grid_positive" },
    { "rete_w", "grid_w" }, { "reti", "networks" },
    { "riga", "row" }, { "ripetizioni", "repeats" },
    { "riscaldamento", "heating" }, { "scene", "scenes" },
    { "schede", "cards" }, { "schermo", "display" },
    { "secondi_a_spegnimento", "off_after_s" }, { "secondi_a_standby", "standby_after_s" },
    { "secondi_riprova", "retry_s" }, { "secondi_ritorno_home", "return_home_after_s" },
    { "segni", "signs" }, { "sensore", "sensor" },
    { "sensore_aggregato", "aggregate_sensor" }, { "sensore_aperture", "openings_sensor" },
    { "sensore_stato", "state_sensor" }, { "sensore_temperatura_locale", "local_temperature_sensor" },
    { "senza_scelta", "no_selection" }, { "sezioni", "sections" },
    { "sfondo", "background" }, { "sicurezza", "security" },
    { "silenzioso", "quiet" }, { "sistema", "system" },
    { "soglia", "threshold" }, { "spegni_a_casa_vuota", "off_when_nobody_home" },
    { "spegnimento", "off_time" }, { "spegnimento_notturno", "night_off" },
    { "stanze", "rooms" }, { "stato_casa", "house_state" },
    { "storico", "history" }, { "stringhe", "strings" },
    { "temperatura", "temperature" }, { "temperatura_interna", "indoor_temperature" },
    { "tensione", "voltage" }, { "testo", "text" },
    { "testo_debole", "text_dim" }, { "tipo", "type" },
    { "umidita", "humidity" }, { "validita", "validity" },
    { "zone", "zones" },
};

static const struct { const char *percorso, *da, *a; } VALORI_11[] = {
    /* The attribute names of the Home Assistant package, which the panel
       also reads under their Italian names (dati_finti_energia.c). */
    { "energy/attributes/house_w", "casa", "house_w" },
    { "energy/attributes/grid_w", "rete_w", "grid_w" },
    { "energy/attributes/battery_w", "batteria_w", "battery_w" },
    { "energy/attributes/battery_pct", "batteria_pct", "battery_pct" },
    { "energy/attributes/today_kwh", "oggi_kwh", "today_kwh" },
    { "access/*/type", "impulso", "pulse" },
    { "access/*/type", "interruttore", "switch" },
    { "energy/signs/grid_positive", "prelievo", "import" },
    { "energy/signs/grid_positive", "immissione", "export" },
    { "energy/signs/battery_positive", "carica", "charge" },
    { "energy/signs/battery_positive", "scarica", "discharge" },
    { "robot/no_selection", "tutta_la_casa", "whole_house" },
    { "robot/no_selection", "niente", "nothing" },
    { "robot/rooms_format", "segmenti", "segments" },
    { "robot/rooms_format", "elenco", "list" },
    { "robot/alerts/*/when", "acceso", "on" },
    { "robot/alerts/*/when", "spento", "off" },
    { "wifi_sharing/networks/*/security", "nessuna", "none" },
    { "sections/*", "luci", "lights" },
    { "sections/*", "clima", "climate" },
    { "sections/*", "energia", "energy" },
    { "sections/*", "accessi", "access" },
    { "sections/*", "agenda", "calendar" },
    { "sections/*", "interruttori", "switches" },
    { "sections/*", "programmazioni", "schedules" },
};

static void chiavi_11(cJSON *nodo)
{
    if (cJSON_IsArray(nodo)) {
        cJSON *x = NULL;
        cJSON_ArrayForEach(x, nodo) chiavi_11(x);
        return;
    }
    if (!cJSON_IsObject(nodo)) return;
    for (cJSON *x = nodo->child; x; x = x->next) {
        if (x->string)
            for (unsigned n = 0; n < sizeof CHIAVI_11 / sizeof *CHIAVI_11; n++)
                if (strcmp(x->string, CHIAVI_11[n].da) == 0) {
                    /* cJSON owns the key: replaced in place, so the order of
                       the keys — what the user sees in an exported file —
                       stays the same. */
                    char *nuova = (char *)cJSON_malloc(strlen(CHIAVI_11[n].a) + 1);
                    if (nuova) {
                        strcpy(nuova, CHIAVI_11[n].a);
                        if (!(x->type & cJSON_StringIsConst)) cJSON_free(x->string);
                        x->string = nuova;
                        x->type &= ~cJSON_StringIsConst;
                    }
                    break;
                }
        chiavi_11(x);
    }
}

/* The values at `percorso` — robot, alerts, every element, when: with a
   star for the elements of an array — renamed where they equal `da`. */
static void valori_11(cJSON *nodo, const char *percorso, const char *da,
                      const char *a)
{
    if (!nodo) return;
    if (!*percorso) {
        if (cJSON_IsString(nodo) && strcmp(nodo->valuestring, da) == 0)
            cJSON_SetValuestring(nodo, a);
        return;
    }
    const char *barra = strchr(percorso, '/');
    const size_t n = barra ? (size_t)(barra - percorso) : strlen(percorso);
    const char *resto = barra ? barra + 1 : "";
    if (n == 1 && percorso[0] == '*') {
        cJSON *x = NULL;
        cJSON_ArrayForEach(x, nodo) valori_11(x, resto, da, a);
        return;
    }
    char chiave[40];
    if (n >= sizeof chiave) return;
    memcpy(chiave, percorso, n);
    chiave[n] = 0;
    valori_11(cJSON_GetObjectItem(nodo, chiave), resto, da, a);
}

static void migra_10_11(cJSON *c)
{
    chiavi_11(c);
    for (unsigned n = 0; n < sizeof VALORI_11 / sizeof *VALORI_11; n++)
        valori_11(c, VALORI_11[n].percorso, VALORI_11[n].da, VALORI_11[n].a);
}

static bool migra(cJSON *c, int da)
{
    if (da < 2) { migra_1_2(c); da = 2; }
    if (da < 3) { migra_2_3(c); da = 3; }
    if (da < 4) { migra_3_4(c); da = 4; }
    if (da < 5) { migra_4_5(c); da = 5; }
    if (da < 6) { migra_5_6(c); da = 6; }
    if (da < 7) { migra_6_7(c); da = 7; }
    if (da < 8) { migra_7_8(c); da = 8; }
    if (da < 9) { migra_8_9(c); da = 9; }
    if (da < 10) { migra_9_10(c); da = 10; }
    if (da < 11) { migra_10_11(c); da = 11; }
    cJSON *s = cJSON_GetObjectItem(c, "schema");
    if (s) cJSON_SetNumberValue(s, da);
    return da == CFG_SCHEMA;
}

/* --- caricamento -------------------------------------------------------- */

static cJSON *analizza(const char *nome)
{
    /* Dal mucchio e non statico. Ventiquattro kilobyte dichiarati statici
       vivono in RAM **interna**, che su questo chip e la risorsa scarsa —
       512 kB in tutto, contese da Wi-Fi, TLS e LVGL — mentre questo e solo
       il foglio su cui si legge un file prima di analizzarlo: niente in
       esso chiede di stare li. Sopra i 16 kB il mucchio lo mette in PSRAM
       da solo.

       Il conto si e visto sul pannello: con Wi-Fi, TLS e telecamente accesi
       restavano dodici kilobyte interni, e l'acceleratore AES falliva perche
       non trovava un buffer per il DMA. Ventiquattro kilobyte tolti di li
       sono un quarto di quello che serviva.

       Allocato una volta e tenuto: la configurazione si rilegge a ogni
       salvataggio, e restituirlo per riprenderlo subito dopo frammenta senza
       guadagnare niente. */
    static char *buf;
    if (!buf) {
        buf = malloc(ARCHIVIO_MAX + 1);
        if (!buf) return NULL;
    }

    size_t n = 0;
    if (!archivio_leggi(nome, buf, ARCHIVIO_MAX + 1, &n)) return NULL;
    return cJSON_ParseWithLength(buf, n);
}

esito_cfg_t cfg_carica(void)
{
    cfg_azzera_errori();

    cJSON *nuovo = analizza(CFG_FILE);
    esito_cfg_t esito = CFG_LETTA;

    /* §2.4: se il principale manca o non si analizza si usa la copia; se
       manca anche quella si va al primo avvio. */
    if (!nuovo) {
        nuovo = analizza(CFG_COPIA);
        esito = nuovo ? CFG_DA_COPIA : CFG_PREDEFINITA;
    }
    if (!nuovo) {
        /* Nessun file: si parte comunque con un documento **in memoria**,
           non con il vuoto. La differenza si vede solo quando qualcuno
           prova a scrivere — e su un pannello appena montato al muro
           scrivere il nome della rete e la prima cosa che si fa. Prima
           `doc` restava nullo: tutti gli accessori funzionavano lo stesso,
           perche ognuno ha il suo ripiego, e l'interfaccia si vedeva; ma
           cfg_imposta_testo() non aveva dove mettere il valore e falliva
           senza che nulla, a schermo, lasciasse sospettare il perche.

           Dentro c'e solo il numero di schema. Basta: il caricamento non
           valida, analizza soltanto, e ogni campo assente cade sul proprio
           ripiego. Un file con dentro poco e un file valido — quello che
           non deve esistere e il **niente**.

           In memoria e non su disco: si salva quando qualcuno cambia
           qualcosa, non per il fatto di essersi acceso. Un pannello che
           scrive in flash a ogni avvio consuma la flash per niente. */
        if (doc) cJSON_Delete(doc);
        doc = cJSON_CreateObject();
        if (doc) cJSON_AddNumberToObject(doc, "schema", CFG_SCHEMA);
        return CFG_PREDEFINITA;
    }

    const cJSON *s = cJSON_GetObjectItem(nuovo, "schema");
    const int versione = cJSON_IsNumber(s) ? (int)s->valuedouble : 0;

    /* Uno schema piu nuovo del nostro vuol dire che il pannello e stato
       riportato a un firmware piu vecchio: si rifiuta e si parte in sola
       lettura, invece di scrivere sopra qualcosa che non si capisce. */
    if (versione > CFG_SCHEMA) {
        cJSON_Delete(nuovo);
        return CFG_TROPPO_NUOVA;
    }

    const bool migrata = versione < CFG_SCHEMA && migra(nuovo, versione);

    if (doc) cJSON_Delete(doc);
    doc = nuovo;

    if (migrata) cfg_salva();
    return esito;
}

/* --- sostituzione e salvataggio ----------------------------------------- */

bool cfg_sostituisci(const char *json, size_t n)
{
    cfg_azzera_errori();

    if (n > ARCHIVIO_MAX) {
        cfg_aggiungi_errore(CFG_ERRORE, "(radice)",
                            "piu grande del massimo consentito");
        return false;
    }

    cJSON *nuovo = cJSON_ParseWithLength(json, n);
    if (!nuovo) {
        cfg_aggiungi_errore(CFG_ERRORE, "(radice)", "JSON non valido");
        return false;
    }

    /* An older document is brought forward before it is judged, exactly
       as when it is read from flash. Without this, a file exported from an
       older panel — or from the Italian project this one comes from — was
       refused by the validator over names it had every right to use. */
    const cJSON *ver = cJSON_GetObjectItem(nuovo, "schema");
    const int versione = cJSON_IsNumber(ver) ? (int)ver->valuedouble : CFG_SCHEMA;
    if (versione > 0 && versione < CFG_SCHEMA) migra(nuovo, versione);

    /* Si valida **prima** di scrivere: se qualcosa non va, il file resta
       quello di prima e chi ha inviato riceve l'elenco dei campi rifiutati. */
    if (!validazione_esegui(nuovo)) {
        cJSON_Delete(nuovo);
        return false;
    }

    if (doc) cJSON_Delete(doc);
    doc = nuovo;
    return cfg_salva();
}

bool cfg_salva(void)
{
    if (!doc) return false;
    char *testo = cJSON_Print(doc);
    if (!testo) return false;
    /* The 24 KB of §2.5 are checked here: the store now accepts larger
       files (the translations), and a config.json over the limit would be
       written and then never read back whole. */
    const size_t n = strlen(testo);
    const bool ok = n <= ARCHIVIO_MAX && archivio_scrivi(CFG_FILE, testo, n);
    cJSON_free(testo);
    return ok;
}

/* I segreti non stanno nel documento: vivono in NVS e non passano mai di
   qui. Non c'e niente da oscurare, ed e esattamente il punto — l'argomento
   resta perche chi legge la firma se lo chiede, e la risposta e questa. */
/* Un segreto in config.json non ci dovrebbe essere: sta in NVS, e lo schema
   non ha nessun campo per metterlo. Ma §8 dice che i campi sconosciuti si
   **conservano**, non si cancellano — cosi un config.json esportato da un
   pannello e importato in un altro torna indietro intatto — e questo vuol
   dire che un campo scritto a mano sopravvive al giro. Se qualcuno ci
   infila un token, da quel momento e nel file che si copia da un pannello
   all'altro, e nessuno se ne accorge.

   Quindi in esportazione si oscura per **nome del campo**, non per valore:
   il valore di un token non ha una forma riconoscibile, il nome si. */
static bool nome_da_segreto(const char *nome)
{
    static const char *const SPIE[] = {
        "token", "password", "passwd", "segreto", "secret", "chiave_api",
        "api_key", "psk",
    };
    if (!nome) return false;
    for (unsigned n = 0; n < sizeof SPIE / sizeof SPIE[0]; n++)
        if (strstr(nome, SPIE[n])) return true;
    return false;
}

static void oscura(cJSON *nodo)
{
    for (cJSON *c = nodo ? nodo->child : NULL; c; c = c->next) {
        if (c->string && nome_da_segreto(c->string) && cJSON_IsString(c)) {
            cJSON_SetValuestring(c, "***");
            continue;
        }
        if (cJSON_IsObject(c) || cJSON_IsArray(c)) oscura(c);
    }
}

char *cfg_esporta(bool oscura_segreti)
{
    if (!doc) return NULL;
    if (!oscura_segreti) return cJSON_Print(doc);

    /* Su una copia: oscurare l'albero vero significherebbe riscrivere il
       file con gli asterischi al primo salvataggio. */
    cJSON *copia = cJSON_Duplicate(doc, true);
    if (!copia) return NULL;
    oscura(copia);
    char *s = cJSON_Print(copia);
    cJSON_Delete(copia);
    return s;
}

void cfg_libera_testo(char *s) { if (s) cJSON_free(s); }
