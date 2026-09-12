/* ------------------------------------------------------------------------
 * Riassunto della home — presenza, aperture, meteo.
 *
 * Tre sensori aggregati, uno per scheda, con tutto negli attributi. Il
 * pannello non deduce: se il sensore non c'e, la scheda dice che non lo sa.
 * --------------------------------------------------------------------- */
#include "dati_finti.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "config.h"
#include "entita.h"
#include "ha.h"
#include "i18n.h"

#include <ctype.h>

#define PERSONE_MAX  8
#define APERTURE_MAX 8

static persona_t  persone[PERSONE_MAX];
static int        n_persone;
static bool       presenza_nota;

static apertura_t aperture[APERTURE_MAX];
static int        n_aperture;
static bool       aperture_note;

static meteo_t    meteo;

/* The edge case of 11-collaudo.md the simulator is in: it decides what is
   made up when no house is connected. */
static caso_t     caso_riassunto;

/* Made-up data only in the simulator, and not in the case where what is
   being looked at is precisely the data that does not answer. */
static bool inventa(void)
{
    return !dati_dal_vero() && caso_riassunto != CASO_NON_DISPONIBILE;
}

/* Gli attributi arrivano come JSON e le stringhe che ne escono muoiono col
   prossimo aggiornamento: qui se ne tiene una copia, perche l'interfaccia
   ci punta dentro finche la schermata vive. */
static char       testi[PERSONE_MAX + APERTURE_MAX][2][48];
static int        quanti_testi;

static const char *conserva(const char *s)
{
    if (!s) return NULL;
    if (quanti_testi >= (int)(sizeof testi / sizeof testi[0])) return "";
    char *dove = testi[quanti_testi][0];
    quanti_testi++;
    snprintf(dove, sizeof testi[0][0], "%s", s);
    return dove;
}

/* Un elenco dentro un attributo: `persone` o `aperture`. Il template in
   Home Assistant lo compone come array di oggetti, perche e la forma in
   cui una lista di cose con dei campi si scrive senza inventare
   separatori da riparsare. */
static cJSON *albero_corrente;   /* si tiene finche non se ne legge un altro */

static const cJSON *elenco(const char *entita, const char *attributo)
{
    if (albero_corrente) { cJSON_Delete(albero_corrente); albero_corrente = NULL; }

    char *json = ent_attributo_json(entita, attributo);
    if (!json) return NULL;
    albero_corrente = cJSON_Parse(json);
    ent_libera_testo(json);
    return cJSON_IsArray(albero_corrente) ? albero_corrente : NULL;
}

/* The first of two names an object field may have: the English one the
   package uses since schema 11, then the Italian one of the package a
   house may still run. Both are read so that nobody has to edit their Home
   Assistant because the panel changed language. */
static const char *campo_di(const cJSON *v, const char *nuovo, const char *vecchio)
{
    const char *s = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)v, nuovo));
    if (!s && vecchio)
        s = cJSON_GetStringValue(cJSON_GetObjectItem((cJSON *)v, vecchio));
    return s;
}

/* "17:40" is a time, and a time is shown with the language's words around
   it — "since 17:40", "dalle 17:40". Any other text is shown as it is:
   the template may say "back around 19:15", and that is its choice. */
static const char *da_quando(const char *s)
{
    if (!s) return NULL;
    const size_t n = strlen(s);
    const bool ora = (n == 4 || n == 5) && s[n - 3] == ':'
                     && isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[n - 1]);
    if (!ora) return s;
    static char buf[4][24];
    static int giro;
    char *b = buf[giro++ % 4];
    snprintf(b, sizeof buf[0], tr(TX_HOME_SINCE), s);
    return b;
}

/* Without a house connected — the simulator — three people, as for the
   weather below: the captures are for looking at a screen before hanging
   it on the wall, and "Home Assistant is not connected" in the home's
   first card shows nothing of how it is drawn. Bare times, so they read
   in the panel's language. On a real panel this branch is never taken. */
static void presenza_finta(void)
{
    static const struct { const char *nome, *ora; bool in_casa; } P[] = {
        { "Person 1", "17:40", true },
        { "Person 2", "16:05", true },
        { "Person 3", "18:10", false },
    };
    presenza_nota = true;
    for (unsigned n = 0; n < sizeof P / sizeof P[0] && n_persone < PERSONE_MAX; n++) {
        persona_t *p = &persone[n_persone++];
        p->nome = P[n].nome;
        p->in_casa = P[n].in_casa;
        p->quando = da_quando(P[n].ora);
    }
}

static void leggi_presenza(void)
{
    n_persone = 0;
    presenza_nota = false;

    const char *e = cfg_testo("presence/sensor", "");
    if (!ent_vista(e) || !ent_disponibile(e)) {
        if (inventa()) presenza_finta();
        return;
    }

    presenza_nota = true;

    /* L'attributo `persone` e un array di oggetti; se manca, il sensore
       dice almeno **quante** persone ci sono, e con quello si scrive una
       riga sola invece di un elenco. Meglio un numero vero che tre nomi
       inventati. */
    const cJSON *a = elenco(e, "people");
    if (!a) a = elenco(e, "persone");

    for (const cJSON *v = a ? a->child : NULL;
         v && n_persone < PERSONE_MAX; v = v->next) {
        persona_t *p = &persone[n_persone];
        p->nome = conserva(campo_di(v, "name", "nome"));
        const cJSON *casa = cJSON_GetObjectItem((cJSON *)v, "home");
        if (!casa) casa = cJSON_GetObjectItem((cJSON *)v, "in_casa");
        p->in_casa = cJSON_IsTrue(casa);
        /* Three names for the time. The Italian package wrote `da`, and
           this code read `quando`: on a real panel the time under each
           name never appeared. `since` is the name from now on. */
        const char *q = campo_di(v, "since", "quando");
        if (!q) q = campo_di(v, "da", NULL);
        p->quando = conserva(da_quando(q));
        if (p->nome && *p->nome) n_persone++;
    }
}

/* --- quante aperture, e quali ------------------------------------------
 *
 * Due domande diverse, e il sensore risponde in due posti diversi: il
 * **numero** sta nel suo stato, i **nomi** in un attributo. Questo codice
 * leggeva solo l'attributo, e quando non lo trovava diceva "0 aperture" —
 * che non e "non lo so", e una risposta precisa e falsa. Sul pannello vero
 * si e vista cosi: una finestra aperta e uno zero sullo schermo.
 *
 * L'attributo poi non si chiamava nemmeno come credeva. 06-ha-package.yaml
 * — il file che si copia dentro Home Assistant — lo chiama `elenco` e ci
 * mette una lista di nomi; qui si cercava `aperture` con dentro oggetti che
 * dicono anche da quanto. Due documenti dello stesso progetto che non si
 * parlavano, e nessuno se ne accorgeva perche il risultato era uno zero
 * plausibile.
 *
 * Ora si accettano tutte e due le forme. Non e indecisione: chi ha gia
 * copiato il template non deve rifarlo, e chi vuole anche il "da quanto"
 * puo arricchirlo senza che il pannello smetta di capirlo. Quello che non
 * si accetta piu e inventare uno zero.
 */
static void leggi_aperture(void)
{
    n_aperture = 0;
    aperture_note = false;

    const char *e = cfg_testo("house_state/openings_sensor", "");
    if (!ent_vista(e) || !ent_disponibile(e)) {
        /* Le aperture non si sono mai inventate, e per la home andava
           bene: senza sensore, "non lo so" e la risposta giusta. Lo
           standby pero mostra una pastiglia che c'e solo quando qualcosa
           e aperto, e una cosa che non si puo far comparire non si puo
           nemmeno guardare prima di appendere il pannello al muro. */
        if (caso_riassunto == CASO_APERTURA) {
            aperture_note = true;
            n_aperture = 1;
            aperture[0].nome = "Kitchen";
            aperture[0].da   = da_quando("18:29");
        } else if (inventa()) {
            /* Everything closed, and known: the home's card says so
               instead of "not connected". Only the simulator gets here. */
            aperture_note = true;
        }
        return;
    }

    aperture_note = true;

    /* Il conteggio, che e la sola cosa sempre presente. */
    const int dallo_stato = (int)ent_numero(e, 0);

    const cJSON *a = elenco(e, "list");
    if (!a) a = elenco(e, "elenco");
    if (!a) a = elenco(e, "openings");
    if (!a) a = elenco(e, "aperture");

    for (const cJSON *v = a ? a->child : NULL;
         v && n_aperture < APERTURE_MAX; v = v->next) {
        apertura_t *p = &aperture[n_aperture];

        if (cJSON_IsString(v)) {
            /* La forma del template: solo il nome. */
            p->nome = conserva(cJSON_GetStringValue((cJSON *)v));
            p->da = "";
        } else {
            p->nome = conserva(campo_di(v, "name", "nome"));
            p->da = conserva(campo_di(v, "since", "da"));
        }
        if (p->nome && *p->nome) n_aperture++;
    }

    /* Se i nomi non sono arrivati ma lo stato dice che qualcosa e aperto,
       vince lo stato: meglio "1 apertura" senza sapere quale che uno zero
       che contraddice la finestra aperta. Le voci in piu restano senza
       nome, e chi disegna mostra il numero. */
    if (dallo_stato > n_aperture) {
        n_aperture = dallo_stato < APERTURE_MAX ? dallo_stato : APERTURE_MAX;
        for (int k = 0; k < n_aperture; k++)
            if (!aperture[k].nome) { aperture[k].nome = ""; aperture[k].da = ""; }
    }
}

/* --- la temperatura di fuori, per tutto il pannello ---------------------
 *
 * **Un fatto, un posto.** Quanti gradi fa fuori e una proprieta della casa,
 * non di una schermata: la home, lo standby e la sezione Clima devono
 * mostrare lo stesso numero, e l'unico modo perche lo facciano e' che lo
 * chiedano qui.
 *
 * Non e' sempre stato cosi. La sezione Clima aveva una chiave sua,
 * `clima.sensore_esterno`, con una descrizione vuota nello schema e un
 * valore di ripiego di **30,4 gradi**: un numero che sembra una temperatura
 * di fuori e che compariva sul vetro se lo stato di quel sensore non era un
 * numero. Due chiavi per lo stesso fatto sono due numeri diversi sullo
 * stesso vetro che aspettano l'occasione — e l'occasione e' il giorno che
 * qualcuno ne cambia una sola.
 *
 * L'ordine e questo, e vale per tutti:
 *
 *   1. il sensore locale di `meteo`, dallo stato o dall'attributo dichiarato
 *   2. la temperatura del servizio meteo
 *   3. **niente**: si scrive "—", non un numero inventato
 *
 * Il sensore locale vince perche un sensore sul balcone sa che tempo fa
 * qui, il servizio sa che tempo fa in paese. */
static void leggi_meteo(void)
{
    meteo = (meteo_t){ .stato = "", .temperatura = TEMP_IGNOTA,
                       .disponibile = false };

    /* Il sensore locale si legge **anche senza entita weather**: prima
       stava dentro il ramo che richiedeva il servizio meteo, e chi avesse
       avuto solo un sensore sul balcone non avrebbe visto niente. */
    const char *locale = cfg_testo("weather/local_temperature_sensor", "");
    const char *attr = cfg_testo("weather/local_temperature_attribute", "");
    double t = -999;
    if (*locale && ent_vista(locale) && ent_disponibile(locale))
        t = *attr ? ent_attributo_numero(locale, attr, -999)
                  : ent_numero(locale, -999);

    const char *e = cfg_testo("weather/entity", "");
    if (*e && ent_vista(e) && ent_disponibile(e)) {
        meteo.stato = ent_stato(e);
        meteo.disponibile = true;
        if (t <= -999) t = ent_attributo_numero(e, "temperature", -999);
    }

    if (t > -999) meteo.temperatura = (int16_t)(t * 10 + (t < 0 ? -0.5 : 0.5));

    /* Senza casa collegata, un valore verosimile: le catture servono a
       guardare una schermata prima di appenderla al muro, e un trattino al
       posto della temperatura non fa vedere se il disegno funziona. Sul
       pannello vero questo ramo non si prende mai. */
    if (!dati_dal_vero() && meteo.temperatura == TEMP_IGNOTA) {
        meteo.temperatura = 226;
        meteo.stato = "sunny";
        meteo.disponibile = true;
    }
}

void finto_riassunto_applica(caso_t c)
{
    caso_riassunto = c;
    quanti_testi = 0;
    leggi_presenza();
    leggi_aperture();
    leggi_meteo();
}

int              dati_persone(void)       { return n_persone; }
bool             dati_presenza_nota(void) { return presenza_nota; }
const persona_t *dati_persona(int n)
{
    return (n >= 0 && n < n_persone) ? &persone[n] : NULL;
}

int dati_persone_in_casa(void)
{
    /* Se l'elenco non c'e ma il sensore si', il suo stato e il conteggio:
       e comunque un dato vero, e vale piu di un elenco vuoto. */
    if (!n_persone && presenza_nota)
        return (int)ent_numero(cfg_testo("presence/sensor", ""), 0);

    int quante = 0;
    for (int n = 0; n < n_persone; n++) if (persone[n].in_casa) quante++;
    return quante;
}

int               dati_aperture(void)      { return n_aperture; }
bool              dati_aperture_note(void) { return aperture_note; }
const apertura_t *dati_apertura(int n)
{
    return (n >= 0 && n < n_aperture) ? &aperture[n] : NULL;
}

meteo_t dati_meteo(void) { return meteo; }
