/* ------------------------------------------------------------------------
 * Prova dell'heap — 11-collaudo.md §1.
 *
 * "Nessuna schermata alloca memoria a ogni ridisegno: l'heap dopo dieci
 * minuti di navigazione e pari a quello iniziale entro il 2%."
 *
 * Dieci minuti di navigazione a mano non li fa nessuno, e infatti quel
 * criterio in un progetto normale non si verifica mai. Qui si naviga tutte
 * le schermate in tutte le viste, tante volte, e si guarda quanto e
 * cresciuto l'heap: le stesse ricostruzioni che farebbe una persona, in
 * qualche secondo.
 *
 * Serve davvero, perche diversi widget allocano — i punti del tratteggio,
 * i descrittori delle griglie, lo stato dei pulsanti di comando — e ognuno
 * di quelli e un'occasione per dimenticare una free.
 * --------------------------------------------------------------------- */
#include "sim.h"

#include <malloc.h>
#include <stdio.h>

#include "lvgl.h"
#include "profile.h"
#include "sezioni.h"
#include "stati.h"
#include "ui.h"

/* Tutte le destinazioni che l'interfaccia sa costruire, viste comprese. */
static const struct { sezione_t sez; int vista; } GIRO[] = {
    { SEZ_HOME, 0 },
    { SEZ_LUCI, 0 },
    { SEZ_CLIMA, 0 }, { SEZ_CLIMA, 1 },
    { SEZ_CLIMA, 10 }, { SEZ_CLIMA, 11 }, { SEZ_CLIMA, 12 }, { SEZ_CLIMA, 13 },
    { SEZ_ENERGIA, 0 },
    { SEZ_ACCESSI, 0 },
    { SEZ_AGENDA, 0 },
    { SEZ_WIFI, 0 }, { SEZ_WIFI, 1 }, { SEZ_WIFI, 2 },
    { SEZ_IMPOSTAZIONI, 0 }, { SEZ_IMPOSTAZIONI, 1 },
    { SEZ_IMPOSTAZIONI, 2 }, { SEZ_IMPOSTAZIONI, 3 },
    { SEZ_IMPOSTAZIONI, 4 },
};
#define N_GIRO ((int)(sizeof GIRO / sizeof GIRO[0]))

/* La soglia del collaudo. */
#define TOLLERANZA_PCT 2

/* Giri di riscaldamento prima di misurare.
 *
 * Serve, e la prima versione di questa prova non ce l'aveva e falliva: le
 * cache di LVGL — glifi disegnati, buffer di tracciamento — si riempiono
 * nei primi giri e poi si fermano. Una perdita vera invece non si assesta
 * mai, ed e proprio quello che questa prova continua a distinguere.
 *
 * Erano venticinque, misurati sul profilo di allora quando le schermate
 * erano quelle della Fase 1. Con la striscia delle telecamere in home il
 * numero non bastava piu e la prova falliva sul p4-800x1280: +11888 byte
 * su venti giri. Ma raddoppiando i giri misurati la crescita saliva solo a
 * +13712, cioe rallentava invece di raddoppiare — una cache che si riempie,
 * non una perdita. Con sessanta giri di riscaldamento scende a +1152.
 *
 * La striscia adesso non c'e piu, e sessanta giri restano: sono una
 * pazienza, non una taratura su quella schermata, e abbassarli per
 * guadagnare qualche secondo vorrebbe dire rimettere in gioco proprio la
 * distinzione che questa prova esiste per fare.
 *
 * La lezione e la stessa della prima volta, e vale la pena riscriverla: la
 * differenza fra una cache e una perdita non si vede da un numero solo, si
 * vede da come cambia raddoppiando i giri. Alzare la soglia sarebbe stato
 * il modo veloce per non vedere piu la domanda. */
#define RISCALDAMENTO 60

/* Nel simulatore LVGL alloca dalla libc (LV_USE_STDLIB_MALLOC = CLIB),
   quindi il contatore che conta e quello della libc, non lv_mem_monitor:
   quest'ultimo misura un pool che qui non esiste. */
static size_t in_uso(void)
{
    return (size_t)mallinfo2().uordblks;
}

static void butta(lv_display_t *d, const lv_area_t *area, uint8_t *px)
{
    LV_UNUSED(area);
    LV_UNUSED(px);
    lv_display_flush_ready(d);
}

/* Un giro completo: tutte le destinazioni e tutti gli stati. Riscaldamento
   e misura devono fare **esattamente le stesse cose**, altrimenti le cache
   di cio che manca al riscaldamento si riempiono durante la misura e
   sembrano una perdita. E successo, ed e la ragione per cui questa e una
   funzione sola invece di due cicli scritti a mano. */
static void un_giro(lv_display_t *d, int n_giro)
{
    for (int n = 0; n < N_GIRO; n++) {
        ui_vai_a(GIRO[n].sez, GIRO[n].vista);
        lv_refr_now(d);
    }
    stato_riconnessione(n_giro + 1, "18:39");
    lv_refr_now(d);
    stato_riconnessione(0, NULL);
    stato_ha_giu(true, "18:31");
    lv_refr_now(d);
    stato_ha_giu(false, NULL);
    conferma_azione("Ripristinare il pannello?",
                    "Si cancellano configurazione, password e token.",
                    "Ripristina", true, NULL);
    lv_refr_now(d);
    stato_avvio(true);
    lv_refr_now(d);
    stato_avvio(false);
}

int sim_prova_heap(int giri)
{
    const bool dettaglio = giri < 0;
    if (dettaglio) giri = -giri;
    if (giri < 1) giri = 20;

    lv_display_t *d = lv_display_create((int32_t)PRF->schermo.larghezza,
                                        (int32_t)PRF->schermo.altezza);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    static uint8_t *buf;
    const size_t byte = (size_t)PRF->schermo.larghezza * 64 * sizeof(uint16_t);
    buf = lv_malloc(byte);
    lv_display_set_buffers(d, buf, NULL, (uint32_t)byte,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, butta);
    lv_sysmon_hide_performance(d);
    lv_sysmon_hide_memory(d);

    ui_avvia();

    /* Riscaldamento: si fa tutto quello che si fara anche dopo, e solo
       quando le cache si sono assestate si comincia a misurare. */
    for (int g = 0; g < RISCALDAMENTO; g++) un_giro(d, g);
    ui_vai(SEZ_HOME);
    lv_refr_now(d);

    const size_t prima = in_uso();

    for (int g = 0; g < giri; g++) {
        un_giro(d, g);
        if (dettaglio)
            printf("  giro %2d  heap %lu byte\n", g + 1,
                   (unsigned long)in_uso());
    }

    /* Dove sta la perdita: si costruisce la **stessa** destinazione due volte
       di fila e si guarda la differenza. Confrontare due schermate diverse
       non direbbe niente, perche una schermata piu ricca occupa di piu ed e
       giusto cosi; ricostruire la stessa deve costare zero. */
    printf("\nperdita per destinazione, ricostruendo due volte la stessa:\n");
    long peggiore = 0;
    for (int n = 0; n < N_GIRO; n++) {
        ui_vai_a(GIRO[n].sez, GIRO[n].vista);
        lv_refr_now(d);
        const size_t a = in_uso();
        ui_vai_a(GIRO[n].sez, GIRO[n].vista);
        lv_refr_now(d);
        const long delta = (long)in_uso() - (long)a;
        if (delta > peggiore) peggiore = delta;
        if (delta != 0)
            printf("  %-16s vista %-2d  %+ld byte\n",
                   sezione(GIRO[n].sez)->chiave, GIRO[n].vista, delta);
    }
    if (peggiore == 0) printf("  nessuna\n");

    /* Gli stati, con la stessa regola: due volte di fila la stessa cosa. */
    ui_vai(SEZ_HOME);
    lv_refr_now(d);
    long r = 0, h = 0, v = 0, f = 0;
    for (int passo = 0; passo < 2; passo++) {
        size_t a = in_uso();
        stato_riconnessione(1, "18:39");
        lv_refr_now(d);
        stato_riconnessione(0, NULL);
        r = (long)in_uso() - (long)a;

        a = in_uso();
        stato_ha_giu(true, "18:31");
        lv_refr_now(d);
        stato_ha_giu(false, NULL);
        h = (long)in_uso() - (long)a;

        a = in_uso();
        stato_avvio(true);
        lv_refr_now(d);
        stato_avvio(false);
        v = (long)in_uso() - (long)a;

        a = in_uso();
        conferma_azione("Ripristinare il pannello?",
                    "Si cancellano configurazione, password e token.",
                    "Ripristina", true, NULL);
        lv_refr_now(d);
        f = (long)in_uso() - (long)a;
    }
    printf("\nperdita per stato:\n"
           "  riconnessione %+ld · ha-giu %+ld · avvio %+ld · conferma %+ld byte\n",
           r, h, v, f);

    ui_vai(SEZ_HOME);
    lv_refr_now(d);

    const size_t dopo = in_uso();

    const long cresciuto = (long)dopo - (long)prima;
    const long soglia = (long)prima * TOLLERANZA_PCT / 100;

    printf("\nprofilo %s · %d giri di riscaldamento, %d misurati, "
           "%d destinazioni\n", PRF->chiave, RISCALDAMENTO, giri, N_GIRO);
    printf("  heap in uso prima  %lu byte\n", (unsigned long)prima);
    printf("  heap in uso dopo   %lu byte\n", (unsigned long)dopo);
    printf("  differenza         %+ld byte (soglia ±%ld, cioe il %d%%)\n",
           cresciuto, soglia, TOLLERANZA_PCT);

    if (cresciuto > soglia) {
        printf("  ESITO: l'heap e cresciuto oltre la soglia\n");
        return 1;
    }
    printf("  ESITO: l'heap resta dov'era\n");
    return 0;
}
