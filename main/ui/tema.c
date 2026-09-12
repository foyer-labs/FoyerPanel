/* ------------------------------------------------------------------------
 * Come si ricavano i colori che non si scelgono — vedi theme.h.
 *
 * Otto voci arrivano dalla configurazione. Le altre ventisei si ricavano da
 * quelle, e ognuna con una regola sua che vale la pena scrivere per esteso,
 * perche' e stata scelta misurando e non a occhio.
 *
 * --- gli stati composti: si ruota la tinta ------------------------------
 *
 * «Selezione nel rail», «pulsante riuscito», «avviso negativo» sono ambre,
 * verdi e rossi molto scuri o molto chiari, **aggiustati a mano** quando la
 * palette e stata scelta. Provato: non sono mescolanze. Cercando la quota
 * di tinta che meglio li riproduce, lo scarto arriva a undici unita e il
 * pixel a sedici bit viene diverso in tredici casi su sedici. Quindi una
 * formula del tipo «fondo piu un decimo di accento» non li ridarebbe: li
 * cambierebbe.
 *
 * Quello che invece li ridà esatti e li fa anche seguire un accento nuovo e
 * **ruotare la tinta**: si prende il colore di oggi e gli si gira la tinta
 * di quanto e girata quella della sua famiglia, lasciando saturazione e
 * luminosita dove erano state messe a mano. Con l'accento blu, l'ambra
 * scurissima della selezione (#221C0E) diventa un blu scurissimo (#0E1422)
 * con la stessa saturazione e la stessa luminosita.
 *
 * **E con i colori di oggi la rotazione e zero**, e per una rotazione di
 * zero questa funzione restituisce l'ingresso senza toccarlo. L'identita
 * non dipende dall'arrotondamento della conversione fra spazi di colore:
 * e un ramo scritto apposta, ed e il motivo per cui la palette predefinita
 * e ancora quella, byte per byte.
 *
 * --- i neutri: una quota per canale -------------------------------------
 *
 * `card2` sta fra lo sfondo e le schede, `off` fra lo sfondo e il testo
 * debole. Qui le mescolanze funzionano — sono neutri, non hanno tinta da
 * conservare — e la quota si tiene **per canale**, misurata dai valori di
 * oggi: cosi con quelli si riottiene l'identico, e con altri due estremi il
 * risultato resta in mezzo.
 * --------------------------------------------------------------------- */
#include "tema.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

uint32_t TEMA[TEMA_QUANTI];

/* --- i valori predefiniti, cioe la palette scelta ----------------------- */

static const uint32_t PREDEFINITI[TEMA_QUANTI] = {
    [TEMA_BG]         = 0x0D1014,
    [TEMA_CARD]       = 0x151A21,
    [TEMA_LINE]       = 0x1E242D,
    [TEMA_TXT]        = 0xE8ECF2,
    [TEMA_DIM]        = 0x8A95A6,
    [TEMA_ACC]        = 0xF0A835,
    [TEMA_OK]         = 0x4FC39A,
    [TEMA_WARN]       = 0xE2664F,
    [TEMA_LUCE]       = 0xF0A835,

    [TEMA_CARD2]      = 0x11161C,
    [TEMA_OFF]        = 0x232B35,
    [TEMA_INK]        = 0x0D1014,

    [TEMA_SEL_BG]     = 0x221C0E,
    [TEMA_SEL_LINE]   = 0x4A3C1C,
    [TEMA_INVIO_BG]   = 0x2A2313,
    [TEMA_INVIO_TXT]  = 0xF0C987,
    [TEMA_RICON_BG]   = 0x251D0E,

    [TEMA_FATTO_BG]   = 0x16301F,
    [TEMA_FATTO_LINE] = 0x2C5A3D,
    [TEMA_FATTO_TXT]  = 0x7DDBA6,
    [TEMA_AVV_OK_BG]  = 0x13251C,
    [TEMA_AVV_OK_TXT] = 0xA9E3C3,

    [TEMA_ERR_BG]     = 0x2E1A17,
    [TEMA_ERR_LINE]   = 0x5A2F28,
    [TEMA_ERR_TXT]    = 0xF09287,
    [TEMA_AVV_KO_BG]  = 0x271815,
    [TEMA_AVV_KO_TXT] = 0xF3B5AC,

    [TEMA_COOL]       = 0x57B6E0,
    [TEMA_CALDO]      = 0xF0A835,
    [TEMA_NERO]       = 0x000000,

    /* Il sole tiene l'ambra che aveva prima, quando il colore glielo dava
       l'accento: con la palette predefinita quell'angolo dello schermo non
       cambia. Gli altri tre sono quello che rappresentano. */
    [TEMA_METEO_SOLE]    = 0xF0A835,
    [TEMA_METEO_NUVOLA]  = 0x9AA7B5,
    [TEMA_METEO_PIOGGIA] = 0x57B6E0,
    [TEMA_METEO_NEVE]    = 0xBBD9E8,

    [TEMA_CAL_1]      = 0xF0A835,
    [TEMA_CAL_2]      = 0x4FC39A,
    [TEMA_CAL_3]      = 0x6F9FE0,
    [TEMA_CAL_4]      = 0xB98BD9,

    [TEMA_MARCHIO]    = 0xF0A835,
};

/* Il nome in configurazione delle sole voci che si scelgono. */
static const char *const NOMI[TEMA_SCELTI] = {
    [TEMA_BG]   = "background",
    [TEMA_CARD] = "cards",
    [TEMA_LINE] = "borders",
    [TEMA_TXT]  = "text",
    [TEMA_DIM]  = "text_dim",
    [TEMA_ACC]  = "accent",
    [TEMA_OK]   = "positive",
    [TEMA_WARN] = "negative",
    [TEMA_LUCE] = "lights_on",
};

uint32_t tema_predefinito(tema_voce_t v)
{
    return (v >= 0 && v < TEMA_QUANTI) ? PREDEFINITI[v] : 0;
}

const char *tema_nome(tema_voce_t v)
{
    return (v >= 0 && v < TEMA_SCELTI) ? NOMI[v] : NULL;
}

/* --- colore ------------------------------------------------------------- */

typedef struct { uint8_t r, g, b; } rgb_t;

static rgb_t da_intero(uint32_t x)
{
    rgb_t c = { (uint8_t)(x >> 16), (uint8_t)(x >> 8), (uint8_t)x };
    return c;
}

static uint32_t a_intero(rgb_t c)
{
    return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

/* Tinta in gradi e saturazione in millesimi. La luminosita non serve a
   nessuno qui e non si calcola. */
static void tinta_e_saturazione(rgb_t c, float *tinta, float *sat)
{
    const float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    const float max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const float min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const float d = max - min;

    if (d <= 0.0f) { *tinta = 0.0f; *sat = 0.0f; return; }

    const float l = (max + min) / 2.0f;
    *sat = d / (1.0f - fabsf(2.0f * l - 1.0f));

    float t;
    if (max == r)      t = fmodf((g - b) / d, 6.0f);
    else if (max == g) t = (b - r) / d + 2.0f;
    else               t = (r - g) / d + 4.0f;
    t *= 60.0f;
    if (t < 0.0f) t += 360.0f;
    *tinta = t;
}

static float canale(float p, float q, float t)
{
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

/* Gira la tinta di `gradi` e moltiplica la saturazione per `fattore`.
 *
 * **Il ramo che conta e il primo.** Con i colori predefiniti la rotazione e
 * zero e il fattore uno, e allora il colore torna indietro identico senza
 * passare da nessuna conversione: e cosi che la palette predefinita resta
 * esattamente quella scelta, invece di essere quella che sopravvive a un
 * giro in HSL e ritorno. */
static rgb_t ruota(rgb_t c, float gradi, float fattore)
{
    if (gradi == 0.0f && fattore == 1.0f) return c;

    const float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    const float max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const float min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const float l = (max + min) / 2.0f;
    const float d = max - min;

    if (d <= 0.0f) return c;          /* un grigio non ha tinta da girare */

    float s = d / (1.0f - fabsf(2.0f * l - 1.0f));
    float t;
    if (max == r)      t = fmodf((g - b) / d, 6.0f);
    else if (max == g) t = (b - r) / d + 2.0f;
    else               t = (r - g) / d + 4.0f;
    t = t * 60.0f + gradi;
    t = fmodf(t, 360.0f);
    if (t < 0.0f) t += 360.0f;

    s *= fattore;
    if (s > 1.0f) s = 1.0f;
    if (s < 0.0f) s = 0.0f;

    const float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    const float p = 2.0f * l - q;
    const float tt = t / 360.0f;

    rgb_t fuori;
    fuori.r = (uint8_t)(canale(p, q, tt + 1.0f / 3.0f) * 255.0f + 0.5f);
    fuori.g = (uint8_t)(canale(p, q, tt) * 255.0f + 0.5f);
    fuori.b = (uint8_t)(canale(p, q, tt - 1.0f / 3.0f) * 255.0f + 0.5f);
    return fuori;
}

/* La quota per canale che porta da `da` a `vero` andando verso `a`, e la
   sua applicazione a due estremi diversi. Torna l'identico quando gli
   estremi sono quelli da cui la quota e stata misurata. */
static rgb_t fra(rgb_t da0, rgb_t a0, rgb_t vero, rgb_t da, rgb_t a)
{
    const int d0[3] = { a0.r - da0.r, a0.g - da0.g, a0.b - da0.b };
    const int v0[3] = { vero.r - da0.r, vero.g - da0.g, vero.b - da0.b };
    const int d[3]  = { a.r - da.r,   a.g - da.g,   a.b - da.b };
    const int base[3] = { da.r, da.g, da.b };

    rgb_t fuori;
    uint8_t *out[3] = { &fuori.r, &fuori.g, &fuori.b };
    for (int i = 0; i < 3; i++) {
        float q = d0[i] ? (float)v0[i] / (float)d0[i] : 0.0f;
        int x = base[i] + (int)(q * (float)d[i] + (d[i] < 0 ? -0.5f : 0.5f));
        if (x < 0) x = 0;
        if (x > 255) x = 255;
        *out[i] = (uint8_t)x;
    }
    return fuori;
}

/* Luminanza relativa. La formula e quella delle WCAG, ed e la stessa che
   la pagina di configurazione usa per dire il contrasto: due posti che
   devono dare la stessa risposta, ed e il motivo per cui e scritta
   uguale. */
static float luminanza(rgb_t c)
{
    float v[3] = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f };
    for (int i = 0; i < 3; i++)
        v[i] = v[i] <= 0.03928f ? v[i] / 12.92f
                                : powf((v[i] + 0.055f) / 1.055f, 2.4f);
    return 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
}

/* Quanto si stacca un colore da un altro, nella scala delle WCAG: da 1 (lo
   stesso colore) a 21 (nero su bianco). */
static float stacco(rgb_t a, rgb_t b)
{
    const float x = luminanza(a), y = luminanza(b);
    return (x > y ? x + 0.05f : y + 0.05f) / (x > y ? y + 0.05f : x + 0.05f);
}

/* --- da configurazione -------------------------------------------------- */

/* "#RRGGBB" -> 0xRRGGBB, oppure il ripiego se non si legge. La validazione
   ha gia rifiutato tutto il resto; questo e il controllo di chi legge, non
   di chi valida. */
static uint32_t colore_da_testo(const char *s, uint32_t ripiego)
{
    if (!s || s[0] != '#') return ripiego;
    uint32_t x = 0;
    for (int n = 1; n <= 6; n++) {
        const char c = s[n];
        int v;
        if (c >= '0' && c <= '9')      v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return ripiego;
        x = (x << 4) | (uint32_t)v;
    }
    return s[7] == 0 ? x : ripiego;
}

/* Le famiglie: quale voce scelta comanda la rotazione di quale stato. */
static const struct { tema_voce_t stato, famiglia; } RUOTATI[] = {
    { TEMA_SEL_BG,     TEMA_ACC },  { TEMA_SEL_LINE,   TEMA_ACC },
    { TEMA_INVIO_BG,   TEMA_ACC },  { TEMA_INVIO_TXT,  TEMA_ACC },
    { TEMA_RICON_BG,   TEMA_ACC },
    { TEMA_FATTO_BG,   TEMA_OK },   { TEMA_FATTO_LINE, TEMA_OK },
    { TEMA_FATTO_TXT,  TEMA_OK },   { TEMA_AVV_OK_BG,  TEMA_OK },
    { TEMA_AVV_OK_TXT, TEMA_OK },
    { TEMA_ERR_BG,     TEMA_WARN }, { TEMA_ERR_LINE,   TEMA_WARN },
    { TEMA_ERR_TXT,    TEMA_WARN }, { TEMA_AVV_KO_BG,  TEMA_WARN },
    { TEMA_AVV_KO_TXT, TEMA_WARN },
};

void tema_applica(void)
{
    rgb_t scelti[TEMA_SCELTI];

    for (int v = 0; v < TEMA_SCELTI; v++) {
        char percorso[32];
        snprintf(percorso, sizeof percorso, "appearance/%s", NOMI[v]);
        scelti[v] = da_intero(colore_da_testo(cfg_testo(percorso, NULL),
                                              PREDEFINITI[v]));
        TEMA[v] = (a_intero(scelti[v]));
    }

    /* Costanti: tinte che vogliono dire qualcosa per conto loro. */
    for (int v = TEMA_COOL; v < TEMA_QUANTI; v++)
        TEMA[v] = PREDEFINITI[v];

    /* Neutri, per quota misurata. */
    TEMA[TEMA_CARD2] = (a_intero(fra(
        da_intero(PREDEFINITI[TEMA_BG]), da_intero(PREDEFINITI[TEMA_CARD]),
        da_intero(PREDEFINITI[TEMA_CARD2]),
        scelti[TEMA_BG], scelti[TEMA_CARD])));

    TEMA[TEMA_OFF] = (a_intero(fra(
        da_intero(PREDEFINITI[TEMA_BG]), da_intero(PREDEFINITI[TEMA_DIM]),
        da_intero(PREDEFINITI[TEMA_OFF]),
        scelti[TEMA_BG], scelti[TEMA_DIM])));

    /* --- il testo sopra l'accento: vince chi si stacca di piu ----------
     *
     * Prima era fisso e uguale allo sfondo. Con l'ambra andava bene, ma un
     * accento scuro scelto da chi configura lo renderebbe nero su nero.
     *
     * La prima versione sceglieva con una soglia sulla luminanza
     * dell'accento, e **sceglieva peggio di quanto si possa**: misurato sul
     * verde #5D875B, che e' il primo accento scelto davvero su questo
     * pannello. La soglia diceva «chiaro», e chiaro su quel verde da 3,5:1
     * mentre scuro da 4,6:1. Non un disastro: solo la scelta peggiore delle
     * due, presa da una regola che le due non le confronta.
     *
     * Confrontarle costa due sottrazioni. E con l'ambra predefinita la
     * risposta resta lo sfondo — 9,4:1 contro 1,7:1 — cioe esattamente il
     * colore di prima. */
    TEMA[TEMA_INK] = stacco(scelti[TEMA_ACC], scelti[TEMA_BG])
                   >= stacco(scelti[TEMA_ACC], scelti[TEMA_TXT])
                   ? TEMA[TEMA_BG] : TEMA[TEMA_TXT];

    /* Stati composti: la tinta gira con la famiglia. */
    for (unsigned n = 0; n < sizeof RUOTATI / sizeof RUOTATI[0]; n++) {
        const tema_voce_t stato = RUOTATI[n].stato;
        const tema_voce_t fam = RUOTATI[n].famiglia;

        float t0, s0, t1, s1;
        tinta_e_saturazione(da_intero(PREDEFINITI[fam]), &t0, &s0);
        tinta_e_saturazione(scelti[fam], &t1, &s1);

        /* Se la famiglia e diventata un grigio la tinta non c'e piu: si
           porta a zero anche la saturazione dello stato, invece di girare
           una tinta che non vuol dire niente. */
        const float fattore = s0 > 0.0f ? s1 / s0 : 1.0f;

        TEMA[stato] = (a_intero(
            ruota(da_intero(PREDEFINITI[stato]), t1 - t0, fattore)));
    }
}
