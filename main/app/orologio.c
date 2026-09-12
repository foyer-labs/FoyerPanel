/* ------------------------------------------------------------------------
 * L'ora — comune al pannello e al simulatore.
 *
 * Qui non c'e niente di specifico: time(), localtime() e setenv("TZ") ci
 * sono su tutti e due. Quello che cambia e **chi corregge l'orologio** —
 * sul PC il sistema, sul pannello NTP — e quello sta altrove.
 * --------------------------------------------------------------------- */
#include "orologio.h"
#include "i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "fusi.h"

/* Il 2025 e l'anno in cui questo firmware e stato scritto: qualunque data
   prima di allora vuol dire che nessuno ha ancora corretto l'orologio, non
   che siamo tornati indietro nel tempo. Un orologio non sincronizzato parte
   dal 1970 e ci resta. */
#define ANNO_MINIMO 2025

/* Named one by one, not as TX_DATE_WEEKDAY_0 + n: the enum is sorted by
   key, and an order that happens to hold today is not a contract. */
static const tx_t GIORNI[] = {
    TX_DATE_WEEKDAY_0, TX_DATE_WEEKDAY_1, TX_DATE_WEEKDAY_2, TX_DATE_WEEKDAY_3,
    TX_DATE_WEEKDAY_4, TX_DATE_WEEKDAY_5, TX_DATE_WEEKDAY_6,
};
static const tx_t MESI[] = {
    TX_DATE_MONTH_01, TX_DATE_MONTH_02, TX_DATE_MONTH_03, TX_DATE_MONTH_04,
    TX_DATE_MONTH_05, TX_DATE_MONTH_06, TX_DATE_MONTH_07, TX_DATE_MONTH_08,
    TX_DATE_MONTH_09, TX_DATE_MONTH_10, TX_DATE_MONTH_11, TX_DATE_MONTH_12,
};

bool orologio_fuso_da_configurazione(void)
{
    const char *nome = cfg_testo("system/timezone", "");
    if (!*nome) return false;

    const char *posix = fuso_posix(nome);
    if (!posix) return false;

    setenv("TZ", posix, 1);
    tzset();
    return true;
}

/* L'istante grezzo, prima di qualunque fuso.

   Un'ora fissa, se qualcuno la impone. Serve alle catture: con l'orologio
   vero ogni immagine differirebbe dalla precedente per i minuti, e un
   confronto fra due serie di catture diventerebbe illeggibile proprio nella
   parte che si guarda per prima. Sul pannello questa variabile non esiste, e
   il ramo non costa niente. */
static time_t istante(void)
{
    const char *fissa = getenv("PANNELLO_ORA");
    return fissa ? (time_t)strtoll(fissa, NULL, 10) : time(NULL);
}

/* Una volta sola in un posto solo: chi vuole sapere che ore sono chiede
   qui, e chi non le sa ancora riceve NULL invece di una data del 1970
   travestita da ora. */
static const struct tm *adesso(void)
{
    static struct tm t;

    /* Un'ora fissa, se qualcuno la impone. Serve alle catture: con l'orologio
       vero ogni immagine differirebbe dalla precedente per i minuti, e un
       confronto fra due serie di catture diventerebbe illeggibile proprio
       nella parte che si guarda per prima. Sul pannello questa variabile non
       esiste, e il ramo non costa niente. */
    const time_t ora = istante();
    localtime_r(&ora, &t);
    return (t.tm_year + 1900) >= ANNO_MINIMO ? &t : NULL;
}

bool orologio_valido(void) { return adesso() != NULL; }

int orologio_minuti(void)
{
    const struct tm *t = adesso();
    return t ? t->tm_hour * 60 + t->tm_min : -1;
}

bool orologio_mezzanotte_iso(char *buf, size_t n)
{
    const struct tm *t = adesso();
    if (!t || !buf || n < 26) return false;

    /* Si azzera l'ora e si lascia decidere a mktime se quel giorno l'ora
       legale c'era o no: tm_isdst a -1 vuol dire «non lo so, guardalo tu»,
       ed e l'unico valore giusto: la mezzanotte del giorno del cambio d'ora
       non dista ventiquattro ore da quella di ieri. */
    struct tm m = *t;
    m.tm_hour = m.tm_min = m.tm_sec = 0;
    m.tm_isdst = -1;

    const time_t mezzanotte = mktime(&m);
    if (mezzanotte == (time_t)-1) return false;

    struct tm u;
    gmtime_r(&mezzanotte, &u);
    return strftime(buf, n, "%Y-%m-%dT%H:%M:%S+00:00", &u) > 0;
}

int orologio_ora_di(long long ms_utc)
{
    if (ms_utc <= 0) return -1;
    const time_t s = (time_t)(ms_utc / 1000);
    struct tm l;
    localtime_r(&s, &l);
    return l.tm_hour;
}

void orologio_ora(char *buf, size_t n)
{
    const struct tm *t = adesso();
    if (t) snprintf(buf, n, "%02d:%02d", t->tm_hour, t->tm_min);
    else   snprintf(buf, n, "--:--");
}

void orologio_data(char *buf, size_t n)
{
    const struct tm *t = adesso();
    if (!t) { if (n) buf[0] = 0; return; }

    /* tm_wday e tm_mon sono indici e vengono da localtime_r, quindi gia
       nell'intervallo giusto; il controllo c'e lo stesso perche un indice
       fuori posto qui sarebbe una lettura fuori dall'array, e costa una
       riga. */
    const int g = (t->tm_wday >= 0 && t->tm_wday < 7) ? t->tm_wday : 0;
    const int m = (t->tm_mon  >= 0 && t->tm_mon  < 12) ? t->tm_mon : 0;
    /* The order is the language's, not printf's: "Friday 28 August",
       "vendredi 28 août", "Freitag, 28. August", "viernes 28 de agosto".
       printf cannot reorder its arguments, so the format has names in
       braces and is filled here. An unknown name is copied as it is: a
       translation with a typo shows the typo, not a crash. */
    char giorno[4];
    snprintf(giorno, sizeof giorno, "%d", t->tm_mday);
    const char *f = tr(TX_DATE_LONG);
    size_t o = 0;
    if (n) buf[0] = 0;
    while (*f && o + 1 < n) {
        const char *metti = NULL;
        size_t salta = 0;
        if (!strncmp(f, "{weekday}", 9))    { metti = tr(GIORNI[g]); salta = 9; }
        else if (!strncmp(f, "{day}", 5))   { metti = giorno;        salta = 5; }
        else if (!strncmp(f, "{month}", 7)) { metti = tr(MESI[m]);   salta = 7; }
        if (metti) {
            while (*metti && o + 1 < n) buf[o++] = *metti++;
            f += salta;
        } else {
            buf[o++] = *f++;
        }
    }
    if (n) buf[o < n ? o : n - 1] = 0;
}

/* --- gli istanti che arrivano da Home Assistant ------------------------ */

long long orologio_adesso_utc(void)
{
    return adesso() ? (long long)istante() : 0;
}

/* Giorni dal 1970 al 1 gennaio di quell'anno, senza tabelle e senza
   mktime(): la formula di Howard Hinnant, che e esatta per qualunque anno
   del calendario gregoriano proletticamente esteso.

   Serve perche timegm() non e nello standard — c'e su glibc e su newlib, si
   chiama _mkgmtime su Windows, e non c'e detto che ci sia domani. Sei righe
   che non dipendono da nessuno costano meno di una compilazione che si
   rompe su un compilatore diverso. */
static long long giorni_dal_1970(int anno, int mese, int giorno)
{
    anno -= mese <= 2;
    const long long era = (anno >= 0 ? anno : anno - 399) / 400;
    const unsigned aoe = (unsigned)(anno - era * 400);              /* 0..399 */
    const unsigned doy = (unsigned)((153 * (mese + (mese > 2 ? -3 : 9)) + 2) / 5
                                    + giorno - 1);                  /* 0..365 */
    const unsigned doe = aoe * 365 + aoe / 4 - aoe / 100 + doy;     /* 0..146096 */
    return era * 146097 + (long long)doe - 719468;
}

long long orologio_da_iso(const char *iso)
{
    if (!iso) return 0;

    int a = 0, me = 0, g = 0, h = 0, mi = 0, s = 0;
    int letti = 0;
    if (sscanf(iso, "%4d-%2d-%2dT%2d:%2d:%2d%n",
               &a, &me, &g, &h, &mi, &s, &letti) != 6 || letti <= 0)
        return 0;

    /* Un controllo largo: non si pretende di sapere quanti giorni ha
       febbraio, si pretende che i numeri siano numeri. Una data assurda
       darebbe un istante assurdo, e chi chiama se ne accorge dal fatto che
       il conto alla rovescia non ha senso — ma "2026-13-99" non deve
       diventare un puntatore fuori posto o un anno negativo. */
    if (a < 1970 || a > 2200 || me < 1 || me > 12 || g < 1 || g > 31
        || h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 60)
        return 0;

    /* I decimi di secondo, se ci sono: Home Assistant li scrive
       ("...T18:12:00.123456+00:00") e vanno saltati, non letti. */
    const char *p = iso + letti;
    if (*p == '.') { p++; while (*p >= '0' && *p <= '9') p++; }

    /* Lo scarto dal fuso. "Z" o niente vogliono dire UTC. */
    long long scarto = 0;
    if (*p == '+' || *p == '-') {
        int oh = 0, om = 0;
        if (sscanf(p + 1, "%2d:%2d", &oh, &om) != 2
            && sscanf(p + 1, "%2d%2d", &oh, &om) != 2)
            return 0;
        scarto = (long long)(oh * 3600 + om * 60) * (*p == '-' ? -1 : 1);
    }

    const long long giorni = giorni_dal_1970(a, me, g);
    return giorni * 86400 + h * 3600LL + mi * 60LL + s - scarto;
}

bool orologio_ora_locale(long long s_utc, char *buf, size_t n)
{
    if (s_utc <= 0 || !buf || n < 6) return false;

    const time_t s = (time_t)s_utc;
    struct tm l;
    localtime_r(&s, &l);
    return snprintf(buf, n, "%02d:%02d", l.tm_hour, l.tm_min) > 0;
}
