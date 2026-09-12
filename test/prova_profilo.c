/* ------------------------------------------------------------------------
 * Prova del profilo, senza LVGL e senza hardware.
 *
 * Verifica cio che 11-collaudo.md §1 chiede sul profilo: le quattro tabelle
 * esistono, sono coerenti fra loro, e il passaggio da un profilo all'altro
 * non tocca nient'altro che profile.h.
 *
 *     ./prova_profilo            esegue i controlli
 *     ./prova_profilo --elenca   stampa le quattro tabelle
 * --------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>

#include "profile.h"

static int errori = 0;

#define ESIGE(cond, ...)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("  NO  ");                                                 \
            printf(__VA_ARGS__);                                              \
            printf("\n");                                                     \
            errori++;                                                         \
        }                                                                     \
    } while (0)

static void controlla(const profilo_t *p)
{
    const bool vert = p->orientamento == VERTICALE;

    /* 09-profili.md §1-ter: la navigazione ha una forma sola per orientamento */
    ESIGE(vert ? p->geo.rail_w == 0 : p->geo.rail_w > 0,
          "%s: rail_w %u non coerente con l'orientamento", p->chiave, p->geo.rail_w);
    ESIGE(vert ? p->geo.navbar_h > 0 : p->geo.navbar_h == 0,
          "%s: navbar_h %u non coerente con l'orientamento", p->chiave, p->geo.navbar_h);

    /* 09-profili.md §6: in verticale la barra sostituisce il dock */
    ESIGE(vert ? !p->home.dock : p->home.dock, "%s: dock", p->chiave);
    ESIGE(p->home.dock == (p->home.dock_max > 0),
          "%s: dock_max %u incoerente con dock", p->chiave, p->home.dock_max);

    const bool p4 = strncmp(p->chiave, "p4", 2) == 0;

    /* --- il limite di otto riquadri, che non c'era ------------------------
     *
     * Qui c'era una regola: al massimo otto riquadri per pagina, perche le
     * schermate a griglia scrivono i testi in un buffer di otto a giro
     * (`buf[indice % 8]`) e — diceva la regola — l'etichetta di LVGL **punta**
     * dentro quel buffer invece di copiarlo, quindi al nono riquadro due
     * schede mostrerebbero lo stesso numero.
     *
     * La premessa e' falsa. `lv_label_set_text()` copia dentro l'etichetta;
     * quella che punta e' `lv_label_set_text_static()`, che in questo
     * programma non compare da nessuna parte (`grep set_text_static main/`
     * non trova niente). La prova sta sul vetro, ed e' piu vecchia della
     * regola: la scheda di una zona di clima scrive la temperatura misurata
     * in **un solo** `static char misurata[12]`, non indicizzato, riusato da
     * tutte le schede della pagina — e le schede mostrano numeri diversi. Se
     * l'etichetta puntasse, mostrerebbero tutte l'ultimo.
     *
     * La regola e' stata scritta quando le griglie erano 4x2 e non costava
     * niente: e' rimasta finche' non e' servito il contrario, e allora ha
     * impedito di far stare dodici zone in una pagina — che e' il modo
     * tipico in cui una precauzione senza prova diventa un vincolo. I buffer
     * a giro restano dove sono: non servono, non fanno danno, e toglierli e'
     * un lavoro suo.
     *
     * Resta il solo controllo che serve davvero: una griglia deve avere
     * almeno una colonna e una riga, se no la pagina e' vuota e la divisione
     * che calcola le pagine e' una divisione per zero. */
    ESIGE(p->griglia.clima_col >= 1 && p->griglia.clima_rig >= 1,
          "%s: griglia clima %dx%d",
          p->chiave, p->griglia.clima_col, p->griglia.clima_rig);
    ESIGE(p->griglia.cond_col >= 1 && p->griglia.cond_rig >= 1,
          "%s: griglia condizionatori %dx%d",
          p->chiave, p->griglia.cond_col, p->griglia.cond_rig);
    /* --- la fascia dello standby ----------------------------------------
     *
     * Quattro dati esatti: fuori, sole, casa, batteria. Non e un massimo
     * ma un conto, e il profilo dice come disporli — quattro in riga in
     * orizzontale, due per due in verticale.
     *
     * Se il prodotto non facesse quattro, l'ultimo andrebbe a capo da solo
     * lasciando una riga con un dato e tre buchi: brutto, e soprattutto
     * silenzioso, perche' succederebbe su un profilo solo e nessuno
     * guarderebbe proprio quello. */
    ESIGE(p->griglia.standby_col * p->griglia.standby_rig == 4,
          "%s: la fascia dello standby e %dx%d, e i dati sono quattro",
          p->chiave, p->griglia.standby_col, p->griglia.standby_rig);

    /* La larghezza percentuale di ogni voce viene da qui: con zero colonne
       sarebbe una divisione per zero, e con una sola i quattro si
       impilerebbero uno per riga. */
    ESIGE(p->griglia.standby_col >= 2,
          "%s: la fascia dello standby vuole almeno due colonne, ne ha %d",
          p->chiave, p->griglia.standby_col);

    ESIGE(p->griglia.luci_col >= 1 && p->griglia.luci_rig >= 1,
          "%s: griglia luci %dx%d",
          p->chiave, p->griglia.luci_col, p->griglia.luci_rig);

    /* §9: il co-processore C6 e un passo di avvio in piu, e una partizione */
    ESIGE(p->comp.partizione_c6 == p4, "%s: partizione c6", p->chiave);
    ESIGE(p->comp.passi_avvio == (p4 ? 6 : 5), "%s: passi di avvio", p->chiave);

    /* §1-bis: ruota chi non e nel proprio orientamento nativo */
    const bool nativo_orizzontale = !p4;
    ESIGE(p->comp.rotazione_software == (vert == nativo_orizzontale),
          "%s: rotazione software", p->chiave);

    /* la scala tipografica e monotona e i corpi fuori scala stanno sopra */
    ESIGE(p->font.f_xxl > p->font.f_xl && p->font.f_xl > p->font.f_l &&
          p->font.f_l > p->font.f_m && p->font.f_m > p->font.f_s &&
          p->font.f_s >= p->font.f_xs, "%s: scala tipografica", p->chiave);
    ESIGE(p->font.standby > p->font.f_xxl && p->font.clima_dettaglio > p->font.f_xxl,
          "%s: corpi fuori scala", p->chiave);

    /* §5: le griglie hanno almeno una colonna e una riga */
    ESIGE(p->griglia.luci_col && p->griglia.luci_rig, "%s: griglia luci", p->chiave);
    ESIGE(p->griglia.clima_col && p->griglia.clima_rig, "%s: griglia clima", p->chiave);
    ESIGE(p->griglia.agenda_col >= 1, "%s: colonne agenda", p->chiave);

    /* in verticale le griglie sono a due colonne, in orizzontale tre o quattro */
    ESIGE(vert ? p->griglia.luci_col == 2 : p->griglia.luci_col >= 3,
          "%s: colonne luci %u", p->chiave, p->griglia.luci_col);

    /* §8: dove il disegno scende sotto il minimo, ci pensa l'area estesa;
       nessuna misura di tocco puo pero essere zero */
    ESIGE(p->tocco.apertura.w && p->tocco.apertura.h, "%s: pulsante apertura", p->chiave);
    ESIGE(p->tocco.clima_pm.w && p->tocco.clima_pm.h, "%s: tasti clima", p->chiave);
    ESIGE(p->tocco.interruttore.w && p->tocco.interruttore.h, "%s: interruttore", p->chiave);

    /* lo schermo e quello che dice la chiave */
    char atteso[32];
    snprintf(atteso, sizeof atteso, "%ux%u", p->schermo.larghezza, p->schermo.altezza);
    ESIGE(strstr(p->chiave, atteso) != NULL,
          "%s: la chiave non corrisponde a %s", p->chiave, atteso);

    /* la testata della home e piu alta di quella di sezione, sempre */
    ESIGE(p->geo.home_head_h > p->geo.head_h, "%s: testate", p->chiave);
}

static void elenca(const profilo_t *p)
{
    printf("%-14s %s\n", p->chiave, p->nome);
    printf("  schermo   %ux%u  %u ppi  %s  %s\n", p->schermo.larghezza,
           p->schermo.altezza, p->schermo.ppi, p->schermo.interfaccia,
           p->orientamento == VERTICALE ? "verticale" : "orizzontale");
    printf("  telaio    rail %u  barra %u  testata %u/%u  pad %u  gap %u  raggi %u/%u/%u\n",
           p->geo.rail_w, p->geo.navbar_h, p->geo.head_h, p->geo.home_head_h,
           p->geo.pad, p->geo.gap, p->geo.radius, p->geo.radius_tile, p->geo.radius_btn);
    printf("  corpi     %u %u %u %u %u %u   fuori scala %u %u %u %u\n",
           p->font.f_xxl, p->font.f_xl, p->font.f_l, p->font.f_m, p->font.f_s,
           p->font.f_xs, p->font.standby, p->font.energia_home,
           p->font.clima_dettaglio, p->font.password_mono);
    printf("  griglie   luci %ux%u  clima %ux%u  cond %ux%u  agenda %u col\n",
           p->griglia.luci_col, p->griglia.luci_rig, p->griglia.clima_col,
           p->griglia.clima_rig, p->griglia.cond_col, p->griglia.cond_rig,
           p->griglia.agenda_col);
    printf("  home      dock %s (max %u)\n",
           p->home.dock ? "si" : "no", p->home.dock_max);
    printf("  avvio     %u passi  rollback %us  c6 %s  rotazione sw %s\n\n",
           p->comp.passi_avvio, p->comp.ota_rollback_s,
           p->comp.partizione_c6 ? "si" : "no",
           p->comp.rotazione_software ? "si" : "no");
}

int main(int argc, char **argv)
{
    const bool stampa = argc > 1 && strcmp(argv[1], "--elenca") == 0;

#ifdef PROFILO_TUTTI
    ESIGE(profilo_scegli("non-esiste") == false, "una chiave inventata e stata accettata");
    for (int n = 0; n < PRF_QUANTI; n++) {
        const char *k = profilo_chiave(n);
        ESIGE(k != NULL, "profilo_chiave(%d) e nullo", n);
        ESIGE(profilo_scegli(k), "profilo_scegli(\"%s\") ha fallito", k);
        ESIGE(strcmp(PRF->chiave, k) == 0, "PRF non e passato a %s", k);
        if (stampa) elenca(PRF);
        controlla(PRF);
    }
    ESIGE(profilo_chiave(PRF_QUANTI) == NULL, "profilo_chiave fuori range");
    printf("%d profili controllati, %d errori\n", PRF_QUANTI, errori);
#else
    if (stampa) elenca(PRF);
    controlla(PRF);
    printf("profilo %s controllato, %d errori\n", PRF->chiave, errori);
#endif

    /* le misure comuni ci sono e hanno senso */
    ESIGE(COM.tocco_min >= 44, "il minimo di tocco e sceso sotto 44");
    ESIGE(COM.opacita_standby_pct > 0 && COM.opacita_standby_pct <= 100,
          "opacita di standby fuori scala");

    return errori == 0 ? 0 : 1;
}
