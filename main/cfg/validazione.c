#include "validazione.h"

#include <stdio.h>
#include <string.h>

#include "profile.h"
#include "texts_gen.h"

/* --- mattoni ------------------------------------------------------------ */

static bool solo(const char *s, const char *ammessi)
{
    if (!s || !*s) return false;
    for (const char *c = s; *c; c++)
        if (!strchr(ammessi, *c)) return false;
    return true;
}

/* Come sopra ma fermandosi prima di `fine`: serve per il dominio di un
   entity_id, che sta prima del punto. */
static bool solo_fino(const char *s, const char *fine, const char *ammessi)
{
    if (!s || s >= fine) return false;
    for (const char *c = s; c < fine; c++)
        if (!strchr(ammessi, *c)) return false;
    return true;
}

#define MINUSCOLE "abcdefghijklmnopqrstuvwxyz"
#define CIFRE     "0123456789"

/* ^[a-z0-9_]+$ */
static bool slug(const char *s) { return solo(s, MINUSCOLE CIFRE "_"); }

/* ^dominio\.[a-z0-9_]+$ — l'entity_id di Home Assistant. Il dominio si passa
   con il punto: "light." per una luce, NULL per uno qualsiasi.

   Piu domini si passano separati da una barra verticale: "light.|switch."
   accetta tutti e due. Serve alle luci, che in una casa vera non sono sempre
   `light.` — un rele o un'integrazione distratta le espone come `switch.`, e
   il pannello sa comandarle lo stesso perche' il dominio se lo legge
   dall'identificativo. */
static bool entita(const char *s, const char *dominio)
{
    if (!s) return false;
    const char *punto = strchr(s, '.');
    if (!punto || punto == s || !punto[1]) return false;
    if (dominio) {
        bool va = false;
        for (const char *d = dominio; d && *d && !va; ) {
            const char *fine = strchr(d, '|');
            const size_t n = fine ? (size_t)(fine - d) : strlen(d);
            if (n && strncmp(s, d, n) == 0) va = true;
            d = fine ? fine + 1 : NULL;
        }
        if (!va) return false;
    }
    return solo(punto + 1, MINUSCOLE CIFRE "_") &&
           solo_fino(s, punto, MINUSCOLE "_");
}

/* ^([01][0-9]|2[0-3]):[0-5][0-9]$ */
static bool ora(const char *s)
{
    if (!s || strlen(s) != 5 || s[2] != ':') return false;
    const int h = (s[0] - '0') * 10 + (s[1] - '0');
    const int m = (s[3] - '0') * 10 + (s[4] - '0');
    return s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9' &&
           s[3] >= '0' && s[3] <= '9' && s[4] >= '0' && s[4] <= '9' &&
           h <= 23 && m <= 59;
}

/* --- lettura con controllo ---------------------------------------------- */

static const cJSON *campo(const cJSON *o, const char *nome)
{
    return cJSON_GetObjectItemCaseSensitive(o, nome);
}

static void err(const char *dove, const char *nome, const char *motivo)
{
    char c[64];
    if (nome) snprintf(c, sizeof c, "%s/%s", dove, nome);
    else      snprintf(c, sizeof c, "%s", dove);
    cfg_aggiungi_errore(CFG_ERRORE, c, motivo);
}

static void avv(const char *dove, const char *nome, const char *motivo)
{
    char c[64];
    if (nome) snprintf(c, sizeof c, "%s/%s", dove, nome);
    else      snprintf(c, sizeof c, "%s", dove);
    cfg_aggiungi_errore(CFG_AVVISO, c, motivo);
}

/* Intero dentro un intervallo. `obbligatorio` a falso lascia passare
   l'assenza, che e diversa da un valore fuori scala. */
static bool intero(const cJSON *o, const char *dove, const char *nome,
                   int32_t min, int32_t max, bool obbligatorio)
{
    const cJSON *v = campo(o, nome);
    if (!v || cJSON_IsNull(v)) {
        if (obbligatorio) err(dove, nome, "manca");
        return !obbligatorio;
    }
    if (!cJSON_IsNumber(v)) { err(dove, nome, "non e un numero"); return false; }
    const int32_t x = (int32_t)v->valuedouble;
    if (x < min || x > max) {
        char m[96];
        snprintf(m, sizeof m, "fuori dall'intervallo %d..%d", (int)min, (int)max);
        err(dove, nome, m);
        return false;
    }
    return true;
}

static bool testo(const cJSON *o, const char *dove, const char *nome,
                  size_t min, size_t max, bool obbligatorio)
{
    const cJSON *v = campo(o, nome);
    if (!v || cJSON_IsNull(v)) {
        if (obbligatorio) err(dove, nome, "manca");
        return !obbligatorio;
    }
    if (!cJSON_IsString(v)) { err(dove, nome, "non e una stringa"); return false; }
    const size_t n = strlen(v->valuestring);
    if (n < min || n > max) {
        char m[96];
        snprintf(m, sizeof m, "lunghezza fuori da %u..%u",
                 (unsigned)min, (unsigned)max);
        err(dove, nome, m);
        return false;
    }
    return true;
}

static bool uno_di(const cJSON *o, const char *dove, const char *nome,
                   const char *const *valori, int quanti, bool obbligatorio)
{
    const cJSON *v = campo(o, nome);
    if (!v || cJSON_IsNull(v)) {
        if (obbligatorio) err(dove, nome, "manca");
        return !obbligatorio;
    }
    if (cJSON_IsString(v))
        for (int n = 0; n < quanti; n++)
            if (strcmp(v->valuestring, valori[n]) == 0) return true;
    err(dove, nome, "valore non ammesso");
    return false;
}

static bool ent(const cJSON *o, const char *dove, const char *nome,
                const char *dominio, bool obbligatorio)
{
    const cJSON *v = campo(o, nome);
    if (!v || cJSON_IsNull(v)) {
        if (obbligatorio) err(dove, nome, "manca");
        return !obbligatorio;
    }
    if (!cJSON_IsString(v) || !entita(v->valuestring, dominio)) {
        char m[96];
        snprintf(m, sizeof m, "non e un'entita %s", dominio ? dominio : "valida");
        err(dove, nome, m);
        return false;
    }
    return true;
}

static int quanti(const cJSON *o, const char *nome)
{
    const cJSON *v = campo(o, nome);
    return cJSON_IsArray(v) ? cJSON_GetArraySize(v) : -1;
}

/* --- livello 1: quello che lo schema gia dice --------------------------- */

static void v_sistema(const cJSON *c)
{
    const cJSON *o = campo(c, "system");
    if (!cJSON_IsObject(o)) { err("system", NULL, "manca"); return; }

    testo(o, "system", "panel_name", 1, 32, true);
    /* Scritti a mano e non presi da profile.h, che pure li conosce: qui la
       fonte autorevole e config.schema.json, e confronta_validazione.py
       verifica a ogni prova che queste quattro righe dicano quello che
       dice lo schema. Il nome e CHIAVI_PROFILO e non PROFILI perche
       PROFILI, in profile.h, e la tabella vera dei profili: due cose
       diverse con lo stesso nome nello stesso file di traduzione sono un
       equivoco che aspetta. */
    static const char *const CHIAVI_PROFILO[] = {
        "p4-1280x800", "p4-800x1280" };
    uno_di(o, "system", "profile", CHIAVI_PROFILO,
           (int)(sizeof CHIAVI_PROFILO / sizeof CHIAVI_PROFILO[0]), true);

    /* ...e deve essere **quello di questo pannello**. Fino a poco fa bastava
       che fosse uno dei profili conosciuti, e il documento poteva dire
       orizzontale su un pannello verticale senza che nessuno lo notasse.

       Non e teorico: 03-config-contratto.md incoraggia a importare il
       config.json di un pannello nell'altro — e la cosa giusta, quasi tutto
       si riusa — e chi lo fa si porta dietro questo campo. Il profilo vero
       lo decide la compilazione, quindi il pannello continua a disegnare
       giusto; ma da quel momento il documento **descrive un altro
       apparecchio**, e i controlli che dipendono dal profilo guardano il
       pannello sbagliato.

       Avviso e non errore: la configurazione e utilizzabile, il campo si
       corregge con un clic, e rifiutare l'importazione per una riga
       vorrebbe dire scoraggiare proprio la cosa che il contratto consiglia. */
    const cJSON *pf = campo(o, "profile");
    if (cJSON_IsString(pf) && strcmp(pf->valuestring, PRF->chiave) != 0) {
        char m[120];
        snprintf(m, sizeof m, "questo pannello e %s: il documento viene da "
                 "un altro, correggilo", PRF->chiave);
        avv("system", "profile", m);
    }
    testo(o, "system", "timezone", 3, 64, true);

    /* The languages come from the generated tables, not from a list
       written here: a language is accepted exactly when the panel has its
       texts. The schema lists them by hand, and gen_texts.py --check
       fails if the two disagree. */
    static const char *const LINGUE[] = { I18N_CODES };
    uno_di(o, "system", "language", LINGUE,
           (int)(sizeof LINGUE / sizeof LINGUE[0]), false);

    const int n = quanti(o, "ntp");
    if (n > 3) err("system", "ntp", "al massimo tre server");

    /* La rete del pannello. Qui c'e **solo il nome**: la password sta in
       NVS e non passa mai da config.json, che si esporta e si copia. Un
       SSID invece l'access point lo grida a chiunque passi, e serve poterlo
       mostrare in diagnostica — un pannello che non sa dire a quale rete
       sta provando ad attaccarsi non aiuta nessuno.

       Facoltativa: al primo avvio non c'e, ed e giusto che la
       configurazione resti valida lo stesso. Senza, il pannello non si
       connette e lo dice. */
    const cJSON *r = campo(o, "network");
    if (r && !cJSON_IsNull(r)) {
        if (!cJSON_IsObject(r)) {
            err("system", "network", "non e un oggetto");
        } else {
            testo(r, "system/network", "ssid", 1, 32, true);
            const cJSON *nasc = campo(r, "hidden");
            if (nasc && !cJSON_IsNull(nasc) && !cJSON_IsBool(nasc))
                err("system/network", "hidden", "non e vero o falso");
        }
    }
}

static void v_schermo(const cJSON *c)
{
    const cJSON *o = campo(c, "display");
    if (!cJSON_IsObject(o)) { err("display", NULL, "manca"); return; }

    intero(o, "display", "brightness_active", 0, 100, true);
    intero(o, "display", "brightness_standby", 0, 100, true);
    intero(o, "display", "standby_after_s", 15, 3600, true);
    intero(o, "display", "off_after_s", 0, 86400, true);
    intero(o, "display", "return_home_after_s", 10, 600, false);
    intero(o, "display", "long_press_ms", 500, 3000, false);

    const cJSON *sn = campo(o, "night_off");
    if (cJSON_IsObject(sn)) {
        for (const char *k = "da"; k; k = (k[0] == 'd') ? "a" : NULL) {
            const cJSON *v = campo(sn, k);
            if (cJSON_IsString(v) && !ora(v->valuestring))
                err("display/night_off", k, "non e un'ora hh:mm");
        }
    }
}

static void v_home_assistant(const cJSON *c)
{
    const cJSON *o = campo(c, "home_assistant");
    if (!cJSON_IsObject(o)) { err("home_assistant", NULL, "manca"); return; }

    const cJSON *h = campo(o, "host");
    if (!cJSON_IsString(h) || !solo(h->valuestring, MINUSCOLE "ABCDEFGHIJKLMNOPQRSTUVWXYZ" CIFRE "._-"))
        err("home_assistant", "host",
            "un indirizzo o un nome host, senza schema ne percorso");

    intero(o, "home_assistant", "port", 1, 65535, true);
    intero(o, "home_assistant", "retry_s", 5, 300, false);

    const cJSON *p = campo(o, "ws_path");
    if (cJSON_IsString(p) && p->valuestring[0] != '/')
        err("home_assistant", "ws_path", "deve cominciare con /");
}

static void v_accessi(const cJSON *c)
{
    const int n = quanti(c, "access");
    if (n < 0) { err("access", NULL, "manca"); return; }
    if (n > 6) err("access", NULL, "al massimo sei accessi");

    const cJSON *el = campo(c, "access"), *a = NULL;
    int i = 0;
    cJSON_ArrayForEach(a, el) {
        char dove[48];
        snprintf(dove, sizeof dove, "access/%d", i++);
        const cJSON *id = campo(a, "id");
        if (!cJSON_IsString(id) || !slug(id->valuestring))
            err(dove, "id", "solo minuscole, cifre e trattino basso");
        testo(a, dove, "name", 1, 32, true);
        static const char *const TIPI[] = { "pulse", "switch" };
        uno_di(a, dove, "type", TIPI, 2, true);
        ent(a, dove, "entity", NULL, true);
        intero(a, dove, "pulse_ms", 200, 5000, false);
        ent(a, dove, "state_sensor", "binary_sensor.", false);
    }
}

static void v_luci(const cJSON *c)
{
    const cJSON *o = campo(c, "lights");
    if (!cJSON_IsObject(o)) { err("lights", NULL, "manca"); return; }

    if (quanti(o, "scenes") > 5) err("lights", "scenes", "al massimo cinque scene");

    const cJSON *sc = campo(o, "scenes"), *s = NULL;
    int i = 0;
    cJSON_ArrayForEach(s, sc) {
        char dove[48];
        snprintf(dove, sizeof dove, "lights/scenes/%d", i++);
        testo(s, dove, "name", 1, 24, true);
        ent(s, dove, "entity", "scene.", true);
    }

    const int n = quanti(o, "zones");
    if (n < 0) { err("lights", "zones", "manca"); return; }

    const cJSON *zo = campo(o, "zones"), *z = NULL;
    i = 0;
    cJSON_ArrayForEach(z, zo) {
        char dove[48];
        snprintf(dove, sizeof dove, "lights/zones/%d", i++);
        testo(z, dove, "name", 1, 32, true);
        ent(z, dove, "entity", "light.|switch.", true);
    }
}

static void v_limiti(const cJSON *o, const char *dove)
{
    if (!cJSON_IsObject(o)) return;
    const cJSON *p = campo(o, "step");
    if (cJSON_IsNumber(p)) {
        const double v = p->valuedouble;
        if (v != 0.1 && v != 0.5 && v != 1.0)
            err(dove, "step", "solo 0.1, 0.5 o 1.0");
    }
    const cJSON *mn = campo(o, "min"), *mx = campo(o, "max");
    if (cJSON_IsNumber(mn) && mn->valuedouble < 5)
        err(dove, "min", "non sotto 5 gradi");
    if (cJSON_IsNumber(mx) && mx->valuedouble > 35)
        err(dove, "max", "non sopra 35 gradi");
}

static void v_clima(const cJSON *c)
{
    const cJSON *o = campo(c, "climate");
    if (!cJSON_IsObject(o)) { err("climate", NULL, "manca"); return; }

    /* `clima.sensore_esterno` diceva la stessa cosa di
       `meteo.sensore_temperatura_locale`, e la sezione Clima la leggeva da
       li invece che dalla fonte comune: due chiavi per un fatto solo sono
       due numeri diversi sullo stesso vetro il giorno che qualcuno ne
       cambia una. Ritirata; il documento la conserva, come ogni chiave che
       lo schema non conosce piu (03-config-contratto.md §12), ma chi ce
       l'ha deve sapere che non fa piu niente. */
    if (cJSON_IsString(campo(o, "sensore_esterno")))
        avv("climate", "sensore_esterno",
            "non si legge piu: la temperatura di fuori sta in "
            "meteo/sensore_temperatura_locale, e vale per tutto il pannello");

    /* --- i piani -------------------------------------------------------
     *
     * Un id ripetuto e' il difetto che non si vede: due piani con lo stesso
     * id, e tutte le zone dell'uno finiscono sotto l'altro — con
     * l'interruttore sbagliato accanto. Meglio dirlo qui che scoprirlo
     * accendendo il riscaldamento del piano che non serve. */
    const cJSON *pia = campo(o, "floors"), *pi = NULL;
    int np = 0;
    cJSON_ArrayForEach(pi, pia) {
        char dove[32];
        snprintf(dove, sizeof dove, "climate/floors/%d", np);
        const cJSON *id = campo(pi, "id");
        if (!cJSON_IsString(id) || !slug(id->valuestring))
            err(dove, "id", "solo minuscole, cifre e trattino basso");
        else {
            const cJSON *altro = NULL;
            int k = 0;
            cJSON_ArrayForEach(altro, pia) {
                if (k++ >= np) break;
                const cJSON *a = campo(altro, "id");
                if (cJSON_IsString(a) && strcmp(a->valuestring, id->valuestring) == 0)
                    err(dove, "id", "gia usato da un altro piano");
            }
        }
        testo(pi, dove, "name", 1, 20, true);
        const cJSON *sw = campo(pi, "switch");
        if (!cJSON_IsString(sw)
            || !(entita(sw->valuestring, "switch.")
                 || entita(sw->valuestring, "input_boolean.")))
            err(dove, "switch", "serve switch. o input_boolean.");
        np++;
    }
    if (np > 4) err("climate", "floors", "al massimo quattro piani");

    v_limiti(campo(o, "heating_limits"), "climate/heating_limits");
    v_limiti(campo(o, "ac_limits"), "climate/ac_limits");

    /* Il piano di questo pannello: senza, l'icona del riscaldamento non
       compare ne nello standby ne nella testata. Non e' un errore — un
       pannello puo non governare nessun piano — ma un id che non esiste si'. */
    const cJSON *mio = campo(o, "panel_floor");
    if (cJSON_IsString(mio)) {
        bool c_e = false;
        const cJSON *q = NULL;
        cJSON_ArrayForEach(q, pia) {
            const cJSON *a = campo(q, "id");
            if (cJSON_IsString(a) && strcmp(a->valuestring, mio->valuestring) == 0)
                c_e = true;
        }
        if (!c_e) err("climate", "panel_floor", "nessun piano ha questo id");
    }

    const cJSON *ri = campo(o, "heating"), *z = NULL;
    if (!cJSON_IsArray(ri)) err("climate", "heating", "manca");
    int i = 0;
    cJSON_ArrayForEach(z, ri) {
        char dove[48];
        snprintf(dove, sizeof dove, "climate/heating/%d", i++);
        testo(z, dove, "name", 1, 32, true);
        ent(z, dove, "climate", "climate.", true);
        /* Un id che non corrisponde a nessun piano non fa sparire la zona —
           finisce nel gruppo «senza piano», che si vede — ma e' quasi sempre
           un errore di battitura, e chi lo ha scritto crede di averla
           assegnata. */
        const cJSON *zp = campo(z, "floor");
        if (cJSON_IsString(zp)) {
            bool c_e = false;
            const cJSON *q = NULL;
            cJSON_ArrayForEach(q, pia) {
                const cJSON *a = campo(q, "id");
                if (cJSON_IsString(a) && strcmp(a->valuestring, zp->valuestring) == 0)
                    c_e = true;
            }
            if (!c_e) avv(dove, "floor", "nessun piano ha questo id");
        }
    }

    if (quanti(o, "air_conditioners") > 8)
        err("climate", "air_conditioners", "al massimo otto condizionatori");

    const cJSON *co = campo(o, "air_conditioners"), *u = NULL;
    if (!cJSON_IsArray(co)) err("climate", "air_conditioners", "manca");
    i = 0;
    cJSON_ArrayForEach(u, co) {
        char dove[48];
        snprintf(dove, sizeof dove, "climate/air_conditioners/%d", i++);
        const cJSON *id = campo(u, "id");
        if (!cJSON_IsString(id) || !slug(id->valuestring))
            err(dove, "id", "solo minuscole, cifre e trattino basso");
        testo(u, dove, "name", 1, 32, true);
        ent(u, dove, "climate", "climate.", true);
        ent(u, dove, "timer", "timer.", false);
        ent(u, dove, "timer_automation", "automation.", false);
        const cJSON *d = campo(u, "default_duration");
        if (cJSON_IsString(d) && !strchr(d->valuestring, ':'))
            err(dove, "default_duration", "formato h:mm");
    }
}

static void v_energia(const cJSON *c)
{
    const cJSON *o = campo(c, "energy");
    if (!cJSON_IsObject(o)) { err("energy", NULL, "manca"); return; }

    ent(o, "energy", "aggregate_sensor", "sensor.", true);

    const cJSON *sg = campo(o, "signs");
    if (cJSON_IsObject(sg)) {
        static const char *const RETE[] = { "import", "export" };
        static const char *const BATT[] = { "charge", "discharge" };
        uno_di(sg, "energy/signs", "grid_positive", RETE, 2, false);
        uno_di(sg, "energy/signs", "battery_positive", BATT, 2, false);
    }

    if (quanti(o, "strings") > 4)
        err("energy", "strings", "al massimo quattro stringhe");

    const cJSON *st = campo(o, "strings"), *s = NULL;
    int i = 0;
    cJSON_ArrayForEach(s, st) {
        char dove[48];
        snprintf(dove, sizeof dove, "energy/strings/%d", i++);
        testo(s, dove, "name", 1, 24, true);
    }
}

/* Una rete condivisibile: tutte le voci dell'elenco hanno
   la stessa forma, quindi la stessa verifica. */
static void v_rete(const cJSON *r, const char *dove)
{
    if (!cJSON_IsObject(r)) { err(dove, NULL, "non e un oggetto"); return; }

    testo(r, dove, "display_name", 1, 32, false);
    testo(r, dove, "ssid", 1, 32, false);

    static const char *const SIC[] = { "WPA", "WEP", "none" };
    uno_di(r, dove, "security", SIC, 3, false);

    /* Una rete accesa senza SSID e un riquadro che si apre su un QR vuoto:
       lo schema non lo vieta — un campo mancante e legittimo — ma qui si
       sa a cosa serve, e si puo dirlo a chi configura. */
    const cJSON *attiva = campo(r, "enabled");
    const cJSON *ssid   = campo(r, "ssid");
    if ((!attiva || cJSON_IsTrue(attiva)) &&
        (!ssid || !cJSON_IsString(ssid) || !ssid->valuestring[0]))
        avv(dove, "ssid", "rete attiva senza SSID: non sara condivisibile");
}

static void v_condivisione(const cJSON *wf)
{
    if (!wf) return;                  /* assente e legittimo: niente da condividere */
    /* Presente ma non un oggetto, invece, va detto. Sembra pignoleria e non
       lo e: la pagina di configurazione scende dentro questa sezione
       fidandosi dello schema, e trovarci un valore semplice le faceva
       saltare il disegno di tutta la sezione. Un documento che il pannello
       accetta in silenzio e che poi rompe la pagina e il peggio dei due
       mondi. */
    if (!cJSON_IsObject(wf)) {
        err("wifi_sharing", NULL, "non e un oggetto");
        return;
    }

    intero(wf, "wifi_sharing", "return_home_after_s", 30, 600, false);

    /* Un elenco solo: la rete ospiti non e piu un caso a parte. Il limite e
       cinque perche cinque sono le voci della tabella dei segreti, non
       perche cinque stiano bene sullo schermo. */
    const cJSON *el = campo(wf, "networks");
    if (el && !cJSON_IsArray(el)) {
        err("wifi_sharing", "networks", "non e un elenco");
        return;
    }
    if (quanti(wf, "networks") > 5)
        err("wifi_sharing", "networks", "al massimo cinque reti");

    const cJSON *r = NULL;
    int i = 0;
    cJSON_ArrayForEach(r, el) {
        char dove[48];
        snprintf(dove, sizeof dove, "wifi_sharing/networks/%d", i++);
        v_rete(r, dove);
    }
}

/* --- lo standby --------------------------------------------------------
 *
 * Il pannello sta al posto di un termostato, e questa e la schermata che
 * si vede quasi tutto il tempo in cui e acceso. Tutto qui dentro e
 * facoltativo: quello che manca non compare, invece di comparire vuoto.
 *
 * L'unica cosa che si dice a voce alta e la mancanza della temperatura
 * della stanza, e con un avviso e non un errore: un pannello senza quel
 * numero funziona benissimo, ma non e il pannello che si voleva.
 */
static void v_misura(const cJSON *o, const char *dove)
{
    if (!cJSON_IsObject(o)) { err(dove, NULL, "non e un oggetto"); return; }

    ent(o, dove, "entity", NULL, true);
    /* L'attributo e un nome di campo di Home Assistant, non un'entita:
       minuscole, cifre e trattini bassi. Serve ai sensori che tengono il
       valore in un attributo invece che nello stato — e quale dei due sia
       cambia da sensore a sensore, per questo si sceglie qui. */
    if (testo(o, dove, "attribute", 1, 40, false)) {
        const cJSON *a = campo(o, "attribute");
        if (cJSON_IsString(a))
            for (const char *p = a->valuestring; *p; p++)
                if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')
                      || *p == '_')) {
                    err(dove, "attribute",
                        "solo minuscole, cifre e trattini bassi");
                    break;
                }
    }
}

static void v_standby(const cJSON *c)
{
    const cJSON *sb = campo(c, "standby");
    if (!cJSON_IsObject(sb)) {
        if (sb && !cJSON_IsNull(sb)) err("standby", NULL, "non e un oggetto");
        else cfg_aggiungi_errore(CFG_AVVISO, "standby",
                 "senza temperatura della stanza lo standby mostra l'ora "
                 "sola: su un pannello che sostituisce un termostato e "
                 "quasi sempre una dimenticanza");
        return;
    }

    const cJSON *ti = campo(sb, "indoor_temperature");
    if (!ti || cJSON_IsNull(ti))
        cfg_aggiungi_errore(CFG_AVVISO, "standby/indoor_temperature",
            "senza, lo standby mostra l'ora sola: su un pannello che "
            "sostituisce un termostato e quasi sempre una dimenticanza");
    else {
        v_misura(ti, "standby/indoor_temperature");
        testo(ti, "standby/indoor_temperature", "label", 1, 16, false);
    }

    /* Il clima da le due pastiglie: "chiesti 21,0" e "sta scaldando".
       Senza, restano solo i gradi misurati — che e onesto e completo. */
    ent(sb, "standby", "climate", "climate.", false);
}

static void v_resto(const cJSON *c)
{
    const cJSON *ag = campo(c, "calendar");
    if (cJSON_IsObject(ag)) {
        intero(ag, "calendar", "days", 1, 14, false);
        intero(ag, "calendar", "home_max_events", 1, 5, false);
        if (quanti(ag, "calendars") > 4)
            err("calendar", "calendars", "al massimo quattro calendari");
    }

    const cJSON *pr = campo(c, "presence");
    if (!cJSON_IsObject(pr)) err("presence", NULL, "manca");
    else {
        ent(pr, "presence", "sensor", "sensor.", true);
    }

    const cJSON *me = campo(c, "weather");
    if (!cJSON_IsObject(me)) err("weather", NULL, "manca");
    else {
        ent(me, "weather", "entity", "weather.", true);
        ent(me, "weather", "local_temperature_sensor", "sensor.", false);
        testo(me, "weather", "local_temperature_attribute", 1, 40, false);
    }

    v_condivisione(campo(c, "wifi_sharing"));

}

/* --- interruttori ------------------------------------------------------
 *
 * Tre controlli, e il terzo e quello che conta. L'entita deve essere di un
 * dominio che si accende e si spegne: `switch.`, `light.`, `input_boolean.`.
 * Un `sensor.` o un `button.` passerebbero la forma — hanno un punto e un
 * nome — e sul vetro darebbero un interruttore che non comanda niente,
 * perche il servizio si ricava dal dominio e `sensor.turn_on` non esiste.
 *
 * L'icona si controlla contro l'elenco chiuso: un nome fuori elenco non
 * darebbe errore sul pannello, darebbe un rettangolo vuoto — ed e' proprio
 * il genere di guasto che dal muro si scambia per un difetto del disegno. */
/* --- le programmazioni --------------------------------------------------
 *
 * Un gruppo per ogni cosa che va a orario. Quello che qui si controlla e
 * quello che sul vetro **non si vedrebbe**: un orario che punta a un
 * `input_datetime` inesistente non da errore, da una riga che non cambia
 * mai; un'automazione che non esiste da un interruttore che non fa niente.
 *
 * I domini si pretendono uno per uno invece di accettare qualunque entita:
 * un `input_number` al posto di un `input_datetime` e' un errore facile da
 * fare — sono vicini nel menu — e il pannello chiamerebbe `set_datetime` su
 * una cosa che non sa cosa farsene. */
static void v_programmazioni(const cJSON *c)
{
    const cJSON *el = campo(c, "schedules");
    if (!el) return;                       /* facoltativa: non c'e, va bene */
    if (!cJSON_IsArray(el)) {
        err("schedules", NULL, "deve essere un elenco");
        return;
    }
    if (cJSON_GetArraySize(el) > 12)
        err("schedules", NULL, "al massimo dodici gruppi");

    const cJSON *g = NULL;
    int n = 0;
    cJSON_ArrayForEach(g, el) {
        char dove[32];
        snprintf(dove, sizeof dove, "schedules/%d", n++);

        testo(g, dove, "name", 1, 24, true);

        /* L'entita comandata e facoltativa: un gruppo puo avere solo una
           finestra di validita, senza niente da accendere. */
        const cJSON *e = campo(g, "entity");
        if (cJSON_IsString(e) && !entita(e->valuestring, NULL))
            err(dove, "entity", "forma dominio.oggetto");

        const cJSON *pw = campo(g, "power");
        if (cJSON_IsString(pw) && !entita(pw->valuestring, "sensor."))
            err(dove, "power", "serve un sensor.");

        const cJSON *fin = campo(g, "windows");
        if (fin && !cJSON_IsArray(fin))
            err(dove, "windows", "deve essere un elenco");
        else if (cJSON_GetArraySize(fin) > 4)
            err(dove, "windows", "al massimo quattro finestre");

        const cJSON *f = NULL;
        int k = 0;
        cJSON_ArrayForEach(f, fin) {
            char qui[64];
            snprintf(qui, sizeof qui, "%s/windows/%d", dove, k++);
            const cJSON *a = campo(f, "on_time");
            const cJSON *s = campo(f, "off_time");
            if (!cJSON_IsString(a) || !entita(a->valuestring, "input_datetime."))
                err(qui, "on_time", "serve un input_datetime.");
            if (!cJSON_IsString(s) || !entita(s->valuestring, "input_datetime."))
                err(qui, "off_time", "serve un input_datetime.");
            /* Lo stesso orario ai due capi: la finestra dura zero, e non e
               una cosa che qualcuno voglia. Succede copiando una riga. */
            if (cJSON_IsString(a) && cJSON_IsString(s)
                && strcmp(a->valuestring, s->valuestring) == 0)
                err(qui, "off_time", "e lo stesso orario dell'accensione");
        }

        const cJSON *aut = campo(g, "automations");
        if (aut && !cJSON_IsArray(aut))
            err(dove, "automations", "deve essere un elenco");
        else if (cJSON_GetArraySize(aut) > 12)
            err(dove, "automations", "al massimo dodici automazioni");

        const cJSON *au = NULL;
        k = 0;
        cJSON_ArrayForEach(au, aut) {
            char qui[64];
            snprintf(qui, sizeof qui, "%s/automations/%d", dove, k++);
            const cJSON *ae = campo(au, "entity");
            if (!cJSON_IsString(ae) || !entita(ae->valuestring, "automation."))
                err(qui, "entity", "serve un automation.");
            testo(au, qui, "name", 0, 32, false);
        }

        /* Un gruppo che non ha niente dentro compare sul pannello come una
           riga vuota: non e un guasto, ma non e nemmeno quello che voleva
           chi l'ha creato. */
        if (cJSON_GetArraySize(fin) == 0 && cJSON_GetArraySize(aut) == 0)
            avv(dove, NULL, "gruppo senza finestre ne automazioni");
    }
}

static void v_interruttori(const cJSON *c)
{
    const cJSON *el = campo(c, "switches");
    if (!el) return;                       /* facoltativa: non c'e, va bene */
    if (!cJSON_IsArray(el)) { err("switches", NULL, "deve essere un elenco"); return; }
    if (cJSON_GetArraySize(el) > 24)
        err("switches", NULL, "al massimo ventiquattro interruttori");

    static const char *const ICONE[] = {
        "outlet",
        "power_settings_new",
        "lightbulb",
        "bolt",
        "cable",
        "mode_fan",
        "wind_power",
        "ac_unit",
        "local_fire_department",
        "heat_pump",
        "water_drop",
        "valve",
        "pool",
        "shower",
        "grass",
        "sunny",
        "tv",
        "speaker",
        "coffee",
        "kitchen",
        "videocam",
        "router",
        "storage",
        "garage",
        "door_front",
        "speed",
        "timer",
        "sensors",
    };
    const int QUANTE_ICONE = (int)(sizeof ICONE / sizeof *ICONE);

    const cJSON *i = NULL;
    int n = 0;
    cJSON_ArrayForEach(i, el) {
        char dove[40];
        snprintf(dove, sizeof dove, "switches/%d", n++);

        testo(i, dove, "name", 1, 24, true);

        const cJSON *e = campo(i, "entity");
        if (!cJSON_IsString(e)
            || !(entita(e->valuestring, "switch.")
                 || entita(e->valuestring, "light.")
                 || entita(e->valuestring, "input_boolean."))
            ) err(dove, "entity", "serve switch., light. o input_boolean.");

        const cJSON *ic = campo(i, "icon");
        if (cJSON_IsString(ic)) {
            bool nota = false;
            for (int k = 0; k < QUANTE_ICONE; k++)
                if (strcmp(ic->valuestring, ICONE[k]) == 0) { nota = true; break; }
            if (!nota)
                err(dove, "icon", "non e fra le icone compilate nel firmware");
        }

        ent(i, dove, "power", "sensor.", false);
    }
}

/* --- aspetto ------------------------------------------------------------
 *
 * Otto colori, tutti facoltativi: quello che manca vale il predefinito. Il
 * controllo e sulla forma — `#RRGGBB` e nient'altro — e **non** sul
 * contrasto.
 *
 * Il contrasto lo calcola e lo mostra la pagina di configurazione, ma non
 * lo impone qui, e la differenza e voluta: un pannello e di chi ce l'ha in
 * casa, e rifiutare una combinazione perche' a un algoritmo non piace
 * vorrebbe dire decidere al posto suo. La pagina avvisa; il pannello
 * ubbidisce. E la pagina di configurazione non si colora, quindi da li si
 * torna sempre indietro. */
static void v_aspetto(const cJSON *c)
{
    const cJSON *a = campo(c, "appearance");
    if (!a) return;
    if (!cJSON_IsObject(a)) { err("appearance", NULL, "deve essere un blocco"); return; }

    static const char *const COLORI[] = {
        "background", "cards", "borders", "text",
        "text_dim", "accent", "positive", "negative",
        "lights_on",
    };

    for (unsigned n = 0; n < sizeof COLORI / sizeof COLORI[0]; n++) {
        const cJSON *v = campo(a, COLORI[n]);
        if (!v || cJSON_IsNull(v)) continue;

        bool va = cJSON_IsString(v) && strlen(v->valuestring) == 7
               && v->valuestring[0] == '#';
        for (int k = 1; va && k <= 6; k++) {
            const char x = v->valuestring[k];
            va = (x >= '0' && x <= '9') || (x >= 'a' && x <= 'f')
              || (x >= 'A' && x <= 'F');
        }
        if (!va) err("appearance", COLORI[n], "serve un colore come #RRGGBB");
    }
}

static void v_sezioni(const cJSON *c)
{
    const int n = quanti(c, "sections");
    if (n < 1) { err("sections", NULL, "almeno una sezione"); return; }
    if (n > 10) err("sections", NULL, "al massimo dieci sezioni");

    static const char *const AMMESSE[] = {
        "lights", "switches", "climate", "energy", "access",
        "calendar", "wifi", "robot", "schedules" };
    const int QUANTE_AMMESSE = (int)(sizeof AMMESSE / sizeof *AMMESSE);

    const cJSON *el = campo(c, "sections"), *s = NULL;
    int i = 0;
    cJSON_ArrayForEach(s, el) {
        bool ok = false;
        if (cJSON_IsString(s))
            for (int k = 0; k < QUANTE_AMMESSE; k++)
                if (strcmp(s->valuestring, AMMESSE[k]) == 0) { ok = true; break; }
        if (!ok) {
            char dove[32];
            snprintf(dove, sizeof dove, "sections/%d", i);
            err(dove, NULL, "sezione sconosciuta");
        }
        i++;
    }
}

/* --- livello 2: le regole fra campi (§10) ------------------------------- */

static void v_incroci(const cJSON *c)
{
    const cJSON *a = NULL;
    int i = 0;
    cJSON_ArrayForEach(a, campo(c, "access")) {
        char dove[48];
        snprintf(dove, sizeof dove, "access/%d", i++);

        const cJSON *tipo = campo(a, "type");
        const cJSON *ims = campo(a, "pulse_ms");
        if (cJSON_IsString(tipo) && strcmp(tipo->valuestring, "pulse") == 0 &&
            (!cJSON_IsNumber(ims) || ims->valuedouble <= 0))
            err(dove, "pulse_ms", "un accesso a impulso deve averlo");
        if (cJSON_IsString(tipo) && strcmp(tipo->valuestring, "switch") == 0 &&
            cJSON_IsNumber(ims))
            avv(dove, "pulse_ms", "ignorato su un interruttore");
    }

    /* stessa entita usata da due accessi */
    const cJSON *x = NULL;
    i = 0;
    cJSON_ArrayForEach(x, campo(c, "access")) {
        const cJSON *ex = campo(x, "entity");
        const cJSON *y = NULL;
        int k = 0;
        cJSON_ArrayForEach(y, campo(c, "access")) {
            if (k++ <= i) continue;
            const cJSON *ey = campo(y, "entity");
            if (cJSON_IsString(ex) && cJSON_IsString(ey) &&
                strcmp(ex->valuestring, ey->valuestring) == 0) {
                char dove[48];
                snprintf(dove, sizeof dove, "access/%d", i);
                avv(dove, "entity", "gia usata da un altro accesso");
            }
        }
        i++;
    }

    /* limiti: min minore di max */
    const cJSON *cl = campo(c, "climate");
    static const char *const GRUPPI[] = {
        "heating_limits", "ac_limits" };
    for (int g = 0; g < 2; g++) {
        const cJSON *l = campo(cl, GRUPPI[g]);
        const cJSON *mn = campo(l, "min"), *mx = campo(l, "max");
        if (cJSON_IsNumber(mn) && cJSON_IsNumber(mx) &&
            mn->valuedouble >= mx->valuedouble) {
            char dove[48];
            snprintf(dove, sizeof dove, "climate/%s", GRUPPI[g]);
            err(dove, NULL, "min deve essere minore di max");
        }
    }

    /* timer e automazione vanno valorizzati insieme: con uno solo dei due
       l'interruttore non compare, e meta funzione e peggio di nessuna */
    const cJSON *u = NULL;
    i = 0;
    cJSON_ArrayForEach(u, campo(cl, "air_conditioners")) {
        char dove[48];
        snprintf(dove, sizeof dove, "climate/air_conditioners/%d", i++);
        const bool t = cJSON_IsString(campo(u, "timer"));
        const bool au = cJSON_IsString(campo(u, "timer_automation"));
        if (t != au)
            avv(dove, NULL, "timer e automazione_timer vanno insieme");
    }

    /* schermo: le soglie in ordine */
    const cJSON *sc = campo(c, "display");
    const cJSON *la = campo(sc, "brightness_active");
    const cJSON *ls = campo(sc, "brightness_standby");
    if (cJSON_IsNumber(la) && cJSON_IsNumber(ls) &&
        ls->valuedouble > la->valuedouble)
        err("display", "brightness_standby", "non puo superare quella attiva");

    const cJSON *st = campo(sc, "standby_after_s");
    const cJSON *sp = campo(sc, "off_after_s");
    if (cJSON_IsNumber(st) && cJSON_IsNumber(sp) &&
        sp->valuedouble != 0 && sp->valuedouble < st->valuedouble)
        err("display", "off_after_s",
            "non puo precedere lo standby");

    /* una sezione deve avere la propria sorgente dati */
    const cJSON *s = NULL;
    cJSON_ArrayForEach(s, campo(c, "sections")) {
        if (!cJSON_IsString(s)) continue;
        if (strcmp(s->valuestring, "calendar") == 0) {
            const cJSON *ag = campo(c, "calendar");
            if (!cJSON_IsTrue(campo(ag, "enabled")))
                err("sections", "calendar", "elencata ma agenda.attiva e falso");
            else if (!cJSON_IsString(campo(ag, "sensor")))
                err("calendar", "sensor", "attiva senza sensore");
        }
        if (strcmp(s->valuestring, "energy") == 0 &&
            !cJSON_IsString(campo(campo(c, "energy"), "aggregate_sensor")))
            err("sections", "energy", "elencata senza sensore_aggregato");
    }

    /* Qui c'erano tre controlli sulle telecamere: l'accesso che nominava una
       telecamera inesistente, la sezione elencata con l'elenco vuoto, e piu
       di quattro in evidenza. Sono usciti col video.

       `videocam` resta invece fra le icone che un interruttore puo scegliere:
       e un glifo, non una funzione, e chi ha una presa che alimenta una
       telecamera continua a poterla riconoscere. */
}

/* --- ingresso ----------------------------------------------------------- */

bool validazione_esegui(const cJSON *c)
{
    cfg_azzera_errori();

    if (!cJSON_IsObject(c)) {
        cfg_aggiungi_errore(CFG_ERRORE, "(radice)", "non e un oggetto");
        return false;
    }

    const cJSON *s = campo(c, "schema");
    if (!cJSON_IsNumber(s)) err("schema", NULL, "manca");

    v_sistema(c);
    v_schermo(c);
    v_home_assistant(c);
    v_accessi(c);
    v_luci(c);
    v_clima(c);
    v_interruttori(c);
    v_programmazioni(c);
    v_aspetto(c);
    v_energia(c);
    v_standby(c);
    v_resto(c);
    v_sezioni(c);
    v_incroci(c);

    for (int n = 0; n < cfg_errori(); n++)
        if (cfg_errore(n)->gravita == CFG_ERRORE) return false;
    return true;
}
