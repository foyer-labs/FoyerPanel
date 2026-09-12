/* ------------------------------------------------------------------------
 * Cattura fuori schermo: disegna un fotogramma e lo scrive su file.
 *
 * Serve a confrontare quello che il codice disegna con il mockup 1:1, che e
 * il modo economico di verificare "indistinguibile dai mockup" senza stare a
 * guardare quattro finestre. Non usa SDL: crea un display con un buffer in
 * memoria, quindi funziona anche senza schermo, in una macchina di
 * compilazione o dentro uno script.
 *
 *     ./pannello --profilo p4-800x1280 --cattura fuori.bmp
 * --------------------------------------------------------------------- */
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "dati.h"
#include "lvgl.h"
#include "profile.h"
#include "sezioni.h"
#include "ui.h"

/* Il fotogramma intero, in RGB565 come sul pannello. */
static uint16_t *fotogramma;
static uint32_t  fg_w, fg_h;

static void raccogli(lv_display_t *d, const lv_area_t *area, uint8_t *px)
{
    const uint16_t *src = (const uint16_t *)px;
    const int32_t w = lv_area_get_width(area);

    for (int32_t y = area->y1; y <= area->y2; y++) {
        if (y < 0 || (uint32_t)y >= fg_h) continue;
        for (int32_t x = area->x1; x <= area->x2; x++) {
            if (x < 0 || (uint32_t)x >= fg_w) continue;
            fotogramma[(uint32_t)y * fg_w + (uint32_t)x] =
                src[(y - area->y1) * w + (x - area->x1)];
        }
    }
    lv_display_flush_ready(d);
}

/* BMP a 24 bit: nessuna libreria, lo apre qualunque cosa, e il confronto con
   il mockup si fa a occhio o con un diff di immagini. */
static bool scrivi_bmp(const char *percorso)
{
    const uint32_t righe = fg_h, colonne = fg_w;
    const uint32_t riga_byte = (colonne * 3 + 3) & ~3u;   /* padding a 4 */
    const uint32_t dati = riga_byte * righe;
    const uint32_t inizio = 14 + 40;

    FILE *f = fopen(percorso, "wb");
    if (!f) { perror(percorso); return false; }

    uint8_t testa[54] = {0};
    testa[0] = 'B'; testa[1] = 'M';
    uint32_t totale = inizio + dati;
    memcpy(testa + 2, &totale, 4);
    memcpy(testa + 10, &inizio, 4);
    uint32_t q = 40; memcpy(testa + 14, &q, 4);
    int32_t  l = (int32_t)colonne; memcpy(testa + 18, &l, 4);
    int32_t  a = -(int32_t)righe;  memcpy(testa + 22, &a, 4);  /* dall'alto */
    uint16_t piani = 1, bit = 24;
    memcpy(testa + 26, &piani, 2);
    memcpy(testa + 28, &bit, 2);
    memcpy(testa + 34, &dati, 4);
    fwrite(testa, 1, sizeof testa, f);

    uint8_t *riga = calloc(riga_byte, 1);
    for (uint32_t y = 0; y < righe; y++) {
        for (uint32_t x = 0; x < colonne; x++) {
            const uint16_t c = fotogramma[y * colonne + x];
            /* RGB565 -> 8 bit per canale, replicando i bit alti nei bassi
               invece di lasciarli a zero: cosi il bianco resta bianco. */
            const uint8_t r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
            const uint8_t g = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
            const uint8_t b = (uint8_t)((c & 0x1F) * 255 / 31);
            riga[x * 3 + 0] = b;   /* il BMP e in BGR */
            riga[x * 3 + 1] = g;
            riga[x * 3 + 2] = r;
        }
        fwrite(riga, 1, riga_byte, f);
    }
    free(riga);
    fclose(f);
    return true;
}


int sim_cattura(const char *percorso, const char *sez, int vista,
                const char *stato)
{
    fg_w = PRF->schermo.larghezza;
    fg_h = PRF->schermo.altezza;
    fotogramma = calloc((size_t)fg_w * fg_h, sizeof *fotogramma);
    if (!fotogramma) { fprintf(stderr, "memoria insufficiente\n"); return 1; }

    lv_display_t *d = lv_display_create((int32_t)fg_w, (int32_t)fg_h);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);

    /* Una striscia alla volta, come sul pannello: se un widget disegnasse
       fuori dalla propria area, qui si vedrebbe. */
    static uint8_t *buf;
    const size_t buf_byte = (size_t)fg_w * 64 * sizeof(uint16_t);
    buf = malloc(buf_byte);
    lv_display_set_buffers(d, buf, NULL, (uint32_t)buf_byte,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, raccogli);

    /* I contatori di LVGL si accendono da soli e finirebbero nell'immagine,
       che deve contenere solo l'interfaccia. */
    lv_sysmon_hide_performance(d);
    lv_sysmon_hide_memory(d);

    ui_avvia();
    if (sez) ui_vai_a(sezione_da_chiave(sez), vista);

    /* Se le telecamere sono collegate si aspetta il primo fotogramma di
       ognuna prima di scattare. Non e una comodita: i riquadri dichiarano
       di esserci solo mentre vengono disegnati, quindi il motore non apre
       niente prima di questo punto, e dall'altra parte ffmpeg ci mette
       qualche secondo a partire. Fotografare subito darebbe riquadri vuoti,
       che e esattamente quello che si voleva smettere di fare. */
    /* Si ridisegna **sempre**, non solo quando e stata chiesta una sezione:
       i riquadri sono stati costruiti quando i fotogrammi non c'erano
       ancora, e senza ricostruirli si fotograferebbe l'attesa. Ci era
       cascata la striscia in home, che non passa da --sezione. */
    ui_vai_a(ui_dove(), ui_vista());

    /* Lo stato si applica **dopo** l'ultimo ridisegno, e non prima.
       ui_vai_a() chiama stati_chiudi(): un modale creato prima veniva
       distrutto proprio dalla riga che doveva mostrarlo, e le catture di
       --stato ripristino uscivano senza la conferma. Il difetto era muto —
       l'immagine c'era, era solo quella sotto. */
    sim_applica_stato(stato, vista);

    lv_refr_now(d);

    const bool ok = scrivi_bmp(percorso);
    if (ok) printf("catturato %s (%ux%u, profilo %s)\n", percorso, fg_w, fg_h,
                   PRF->chiave);

    free(fotogramma);
    free(buf);
    return ok ? 0 : 1;
}
