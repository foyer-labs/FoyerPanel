/* GENERATO da tools/genera_icone.py — non modificare a mano. */
#include "icons.h"

#include "profile.h"

LV_FONT_DECLARE(icone_19);
LV_FONT_DECLARE(icone_24);
LV_FONT_DECLARE(icone_30);
LV_FONT_DECLARE(icone_64);

static const lv_font_t *const TABELLA[FAM_QUANTE][IC_QUANTI] = {
    [FAM_P4] = {
        [IC_S] = &icone_19,
        [IC_M] = &icone_24,
        [IC_L] = &icone_30,
        [IC_XL] = &icone_64,
    },
};

const lv_font_t *icona(icona_corpo_t corpo)
{
    if ((unsigned)corpo >= (unsigned)IC_QUANTI) corpo = IC_M;
    const lv_font_t *f = TABELLA[PRF->famiglia][corpo];
    return f ? f : LV_FONT_DEFAULT;
}

/* --- l'icona scelta in configurazione --- */
static const struct { const char *nome, *glifo; } SCELTE[] = {
    { "outlet", ICO_OUTLET },
    { "power_settings_new", ICO_POWER_SETTINGS_NEW },
    { "lightbulb", ICO_LIGHTBULB },
    { "bolt", ICO_BOLT },
    { "cable", ICO_CABLE },
    { "mode_fan", ICO_MODE_FAN },
    { "wind_power", ICO_WIND_POWER },
    { "ac_unit", ICO_AC_UNIT },
    { "local_fire_department", ICO_LOCAL_FIRE_DEPARTMENT },
    { "heat_pump", ICO_HEAT_PUMP },
    { "water_drop", ICO_WATER_DROP },
    { "valve", ICO_VALVE },
    { "pool", ICO_POOL },
    { "shower", ICO_SHOWER },
    { "grass", ICO_GRASS },
    { "sunny", ICO_SUNNY },
    { "tv", ICO_TV },
    { "speaker", ICO_SPEAKER },
    { "coffee", ICO_COFFEE },
    { "kitchen", ICO_KITCHEN },
    { "videocam", ICO_VIDEOCAM },
    { "router", ICO_ROUTER },
    { "storage", ICO_STORAGE },
    { "garage", ICO_GARAGE },
    { "door_front", ICO_DOOR_FRONT },
    { "speed", ICO_SPEED },
    { "timer", ICO_TIMER },
    { "sensors", ICO_SENSORS },
};
#define SCELTE_QUANTE 28

const char *icona_da_nome(const char *nome, const char *ripiego)
{
    if (!nome || !*nome) return ripiego;
    for (int n = 0; n < SCELTE_QUANTE; n++) {
        const char *a = SCELTE[n].nome, *b = nome;
        while (*a && *a == *b) { a++; b++; }
        if (*a == 0 && *b == 0) return SCELTE[n].glifo;
    }
    return ripiego;
}

int icone_scegliibili(void) { return SCELTE_QUANTE; }

const char *icona_scegliibile(int n)
{
    return (n >= 0 && n < SCELTE_QUANTE) ? SCELTE[n].nome : NULL;
}
