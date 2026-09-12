/* ------------------------------------------------------------------------
 * Prova del tocco — solo PC.
 *
 * Quattro volte in questo progetto un comando non e arrivato dove doveva, e
 * quattro volte se n'e accorto un dito sul vetro invece di una prova. Le
 * cause erano diverse — un contenitore decorativo cliccabile per difetto, un
 * ui_tocco_su_tutto() chiamato prima di costruire i figli, un widget
 * sovrapposto — ma il sintomo era sempre lo stesso: si preme e non succede
 * niente, oppure succede solo premendo in un punto preciso.
 *
 * Qui il tocco si inietta davvero. Non si guarda l'albero degli oggetti per
 * dedurre chi prenderebbe il tocco: si registra un dispositivo di ingresso
 * finto, si preme al **centro** del bersaglio, si rilascia, e si guarda se
 * la schermata e andata dove doveva. E la stessa strada che percorre un
 * dito, compresa la ricerca dell'oggetto piu in alto sotto il punto — che e
 * proprio il pezzo in cui il difetto si nasconde.
 *
 * Il bersaglio si indica col **testo che porta**, non con le coordinate: le
 * coordinate cambiano con il profilo, il testo no, e una prova che si rompe
 * cambiando pannello smette di essere letta.
 * --------------------------------------------------------------------- */
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"

#include "dati.h"
#include "i18n.h"
#include "stati.h"
#include "sistema.h"
#include "segreti.h"
#include "config.h"
#include "profile.h"
#include "schermate.h"
#include "sezioni.h"
#include "ui.h"

/* Il travaso che butta via i pixel: qui non si guarda niente, si tocca. */
static void scarta(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    LV_UNUSED(a); LV_UNUSED(px);
    lv_display_flush_ready(d);
}

static lv_point_t dove_premo;
static bool       premuto;

static void leggi_finto(lv_indev_t *indev, lv_indev_data_t *dati)
{
    LV_UNUSED(indev);
    dati->point = dove_premo;
    dati->state = premuto ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* L'etichetta con quel testo, ovunque sia nell'albero. */
static lv_obj_t *cerca_testo(lv_obj_t *o, const char *testo)
{
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *t = lv_label_get_text(o);
        if (t && strcmp(t, testo) == 0) return o;
    }
    for (uint32_t n = 0; n < lv_obj_get_child_count(o); n++) {
        lv_obj_t *t = cerca_testo(lv_obj_get_child(o, n), testo);
        if (t) return t;
    }
    return NULL;
}

/* Il pezzo di interfaccia che quel testo rappresenta: l'etichetta stessa se
   e lei a essere premibile, altrimenti il primo antenato che lo e. E il
   riquadro che l'utente vede e crede di poter toccare. */
static lv_obj_t *bersaglio(lv_obj_t *etichetta)
{
    for (lv_obj_t *o = etichetta; o; o = lv_obj_get_parent(o))
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE)) return o;
    return etichetta;
}

static void gira(lv_display_t *d, int volte)
{
    for (int n = 0; n < volte; n++) {
        lv_tick_inc(20);
        lv_timer_handler();
    }
    lv_refr_now(d);
}

/* Preme al centro del bersaglio e rilascia, come farebbe un dito. */
static void tocca(lv_display_t *d, lv_obj_t *o)
{
    lv_obj_update_layout(o);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    dove_premo.x = (a.x1 + a.x2) / 2;
    dove_premo.y = (a.y1 + a.y2) / 2;

    premuto = true;  gira(d, 3);
    premuto = false; gira(d, 3);
}

/* passo_t e non caso_t: `caso_t` esiste gia in dati.h e sono i casi limite
   di collaudo. Qui un passo e un movimento del dito. */
typedef struct {
    const char *sezione;
    int         vista;
    const char *testo;      /* quello che c'e scritto sul bersaglio */
    int         vista_dopo; /* dove deve portare; negativo = apre un modale */
    /* Cosa deve comparire nel modale. Vuoto vuol dire «lo stesso testo che
       si e toccato», che e il caso della conferma: il pulsante «Ripristina»
       apre un modale che dice «Ripristina». Il tastierino di un orario no —
       si tocca «08:00» e dentro ci sono due cifre separate — e senza questo
       campo l'unico modo di provarlo sarebbe non provarlo. */
    const char *dentro;
} passo_t;

/* Quante reti si condividono davvero, e come si chiama la prima.
   Ricalcolarlo qui e una ripetizione voluta: e **la regola** a essere sotto
   prova, non il codice che la applica. */
static int reti_accese(void)
{
    int q = 0;
    for (int n = 0; n < dati_reti(); n++)
        if (dati_rete(n).attiva) q++;
    return q;
}

/* Il primo gruppo di programmazioni, e il primo orario che si puo toccare.
   Ricalcolati qui apposta: e la regola a essere sotto prova, non il codice
   che la applica. Senza programmazioni configurate i due passi si saltano,
   come si saltano quelli delle reti quando non ce ne sono. */
static const char *primo_gruppo(void)
{
    const programmazione_t *p = dati_programmazione(0);
    return (p && p->nome && p->nome[0] && p->finestre > 0) ? p->nome : NULL;
}

static const char *primo_orario(void)
{
    const finestra_t *f = dati_finestra(0, 0);
    if (!f || f->da_min == ORARIO_IGNOTO) return NULL;
    static char b[16];
    snprintf(b, sizeof b, "%02d:%02d", f->da_min / 60, f->da_min % 60);
    return b;
}

static const char *prima_rete(void)
{
    for (int n = 0; n < dati_reti(); n++) {
        const rete_t r = dati_rete(n);
        if (r.attiva) return r.nome_mostrato;
    }
    return NULL;
}

/* I passaggi che un dito deve poter fare. Cresce ogni volta che se ne
   scopre uno che non funzionava: e li che questa prova guadagna il suo
   costo. */
static int riempi_casi(passo_t *c)
{
    const int quante = reti_accese();
    const char *prima = prima_rete();
    int n = 0;

    /* Il ripristino di fabbrica, e **prima** del ritorno qui sotto: non ha
       niente a che vedere con le reti condivise, e legarlo a quelle vorrebbe
       dire smettere di provarlo su una configurazione che non ne ha.

       Il pulsante deve aprire la conferma, e la conferma deve avere sia il
       si che il no. Il si non si preme: cancellerebbe davvero e
       riavvierebbe. Si verifica che ci sia, perche un modale senza uscita e
       la trappola peggiore di tutte. */
    c[n++] = (passo_t){ "settings", 0, tr(TX_SETTINGS_RESET_BUTTON), -1, NULL };

    /* Le programmazioni: si apre un gruppo toccandone l'intestazione, e da
       li si tocca un orario e deve aprirsi il tastierino. Il secondo e il
       passaggio che vale la prova — una pastiglia con dentro un'etichetta e
       il caso in cui il tocco finisce sul testo invece che sul riquadro, ed
       e successo altrove in questo progetto. */
    if (dati_programmazioni() > 0 && primo_gruppo() && primo_orario()) {
        c[n++] = (passo_t){ "schedules", 0, primo_gruppo(), 1, NULL };
        c[n++] = (passo_t){ "schedules", 1, primo_orario(), -1, tr(TX_COMMON_SAVE) };
    }

    /* Il robot: il triangolo accanto al nome deve portare agli avvisi, e da
       li si deve poter tornare alle stanze.
     *
     * Il primo passo e' quello che vale: la pastiglia sta **dentro** una
     * riga che e gia un bersaglio suo, e in questo progetto un contenitore
     * che si mangia il tocco dei figli e gia successo — nelle
     * programmazioni, dove la colonna del nome ingoiava il tocco del
     * gruppo. Qui il rischio e' l'opposto e altrettanto silenzioso: il
     * tocco che arriva alla riga invece che alla pastiglia porterebbe alle
     * stanze, cioe da qualche parte, e nessuno lo chiamerebbe un difetto. */
    if (dati_robot_avvisi_accesi() + dati_robot_manutenzioni_agli_sgoccioli()) {
        static char pastiglia[16];
        const int q = dati_robot_avvisi_accesi()
                    + dati_robot_manutenzioni_agli_sgoccioli();
        snprintf(pastiglia, sizeof pastiglia, trn(TXN_ROBOT_ALERTS, q), q);
        c[n++] = (passo_t){ "robot", 0, pastiglia, 1, NULL };
        c[n++] = (passo_t){ "robot", 1, tr(TX_ROBOT_BACK_TO_ROOMS), 0, NULL };
        /* E lo stesso dalla home, che e' il caso vero: li la pastiglia sta
           dentro una riga **gia cliccabile**, e se il tocco finisse sulla
           riga si arriverebbe alle stanze — vista 0 — invece che agli
           avvisi. Il passo lo distingue proprio perche' guarda quale vista
           si e aperta, e non se e successo qualcosa.

           Solo in verticale: la riga del robot sta nella fascia che la home
           disegna li e non in orizzontale, dove le sezioni si raggiungono
           dal dock. Non e una mancanza di questa prova — e il layout, e
           chiederlo dove non c'e vorrebbe dire una prova che fallisce per
           una schermata fatta apposta cosi. */
        if (PRF->orientamento == VERTICALE)
            c[n++] = (passo_t){ "home", 0, pastiglia, 1, NULL };
    }

    if (!quante || !prima) return n;   /* niente da condividere, niente da toccare */

    /* Una forma sola, adesso: si entra nell'elenco, si tocca una rete, si
       torna. Prima ce n'erano due — con una rete sola si saltava l'elenco —
       e quel salto era proprio la cosa che rendeva la sezione difficile da
       descrivere. */
    c[n++] = (passo_t){ "wifi", 0, prima, 1, NULL };
    c[n++] = (passo_t){ "wifi", 1, tr(TX_WIFI_ALL_NETWORKS), 0, NULL };
    return n;
}

/* --- il primo avvio scrive davvero? -------------------------------------
 *
 * E la domanda che conta su quella schermata, e non si risponde
 * guardandola: per mesi ha raccolto indirizzo e token in due buffer e li ha
 * buttati passando al passo dopo. Sembrava funzionare — i campi si
 * riempivano, i pallini avanzavano — e non salvava niente.
 *
 * Qui si tocca la prima rete dell'elenco, si preme il tasto d'azione della
 * tastiera, e poi si guarda **la configurazione e NVS**, non lo schermo. Lo
 * schermo aveva gia detto di si.
 */
static int prova_primo_avvio(lv_display_t *d)
{
    int falliti = 0;

    /* Si parte come un pannello appena acceso. */
    segreti_scrivi(SEG_WIFI_PASSWORD, "");
    cfg_imposta_testo("system/network/ssid", "");
    cfg_salva();

    stato_primo_avvio(true, 0);
    /* Due attese, e sono due cose diverse: prima che la scansione finisca,
       poi che la schermata se ne accorga — se ne accorge un tempo di LVGL,
       e finche non scatta l'elenco resta quello di prima. Aspettare solo la
       prima lasciava la schermata ferma su "cerco le reti". */
    for (int k = 0; k < 30 && sistema_reti_quante() == 0; k++) gira(d, 1);
    gira(d, 30);

    const rete_trovata_t prima = sistema_rete_trovata(0);
    lv_obj_t *riga = cerca_testo(lv_layer_top(), prima.ssid);
    if (!riga) {
        printf("  NO   primo avvio: l'elenco delle reti e vuoto\n");
        stato_primo_avvio(false, 0);
        return 1;
    }
    tocca(d, bersaglio(riga));

    /* Toccare la riga **sceglie**, non prosegue: il passo si chiude con
       "Avanti", che e la stessa cosa che fa un dito. Una prova che salta
       questo passaggio verifica una schermata che non esiste. */
    lv_obj_t *av = cerca_testo(lv_layer_top(), tr(TX_COMMON_NEXT));
    if (!av) {
        printf("  NO   primo avvio: scelta la rete, non si puo proseguire\n");
        stato_primo_avvio(false, 0);
        return 1;
    }
    tocca(d, bersaglio(av));

    /* Il tasto d'azione della tastiera del passo 2: porta avanti **e**
       salva. Senza password, che su questa prova non serve — quello che si
       verifica e che l'SSID arrivi in configurazione. */
    lv_obj_t *az = cerca_testo(lv_layer_top(), tr(TX_SETUP_CONNECT));
    if (!az) {
        printf("  NO   primo avvio: il passo della password non si apre\n");
        stato_primo_avvio(false, 0);
        return 1;
    }
    tocca(d, bersaglio(az));

    const char *scritto = cfg_testo("system/network/ssid", "");
    if (strcmp(scritto, prima.ssid) == 0) {
        printf("  ok   primo avvio: la rete scelta finisce in configurazione\n");
    } else {
        printf("  NO   primo avvio: in configurazione c'e \"%s\" invece di "
               "\"%s\"\n", scritto, prima.ssid);
        falliti++;
    }

    stato_primo_avvio(false, 0);
    return falliti;
}

int sim_prova_tocco(void)
{
    lv_display_t *d = lv_display_create((int32_t)PRF->schermo.larghezza,
                                        (int32_t)PRF->schermo.altezza);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    static uint8_t *buf;
    const size_t byte = (size_t)PRF->schermo.larghezza * 64 * sizeof(uint16_t);
    buf = malloc(byte);
    lv_display_set_buffers(d, buf, NULL, (uint32_t)byte,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, scarta);
    lv_sysmon_hide_performance(d);
    lv_sysmon_hide_memory(d);

    lv_indev_t *dito = lv_indev_create();
    lv_indev_set_type(dito, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(dito, leggi_finto);
    lv_indev_set_display(dito, d);

    ui_avvia();

    passo_t casi[10];
    const int quanti = riempi_casi(casi);
    if (!quanti) {
        printf("nessuna rete privata accesa: niente da toccare\n");
        free(buf);
        return 0;
    }

    int falliti = 0;
    for (int n = 0; n < quanti; n++) {
        const passo_t *c = &casi[n];
        ui_vai_a(sezione_da_chiave(c->sezione), c->vista);
        gira(d, 2);

        lv_obj_t *et = cerca_testo(lv_display_get_screen_active(d), c->testo);
        if (!et) {
            printf("  NO   \"%s\" non c'e nella vista %d\n", c->testo, c->vista);
            falliti++;
            continue;
        }

        lv_obj_t *b = bersaglio(et);
        if (b == et) {
            /* Un'etichetta premibile e quasi sempre un difetto: vuol dire
               che il riquadro attorno non prende il tocco, e chi guarda si
               trova a dover centrare le lettere. */
            printf("  NO   \"%s\": tocca solo il testo, non il riquadro\n",
                   c->testo);
            falliti++;
            continue;
        }

        tocca(d, b);

        /* vista_dopo negativa: non si va da nessuna parte, si apre un
           modale. Quello che si verifica e che sia comparso e che offra
           tutte e due le uscite. */
        if (c->vista_dopo < 0) {
            lv_obj_t *cima = lv_layer_top();
            const bool si = cerca_testo(cima, c->dentro ? c->dentro
                                                        : c->testo) != NULL;
            const bool no = cerca_testo(cima, tr(TX_COMMON_CANCEL)) != NULL;
            if (si && no) {
                printf("  ok   \"%s\" apre la conferma, con Annulla\n",
                       c->testo);
            } else {
                printf("  NO   \"%s\": conferma %s\n", c->testo,
                       !si ? "non aperta" : "senza Annulla");
                falliti++;
            }
            /* Si esce dal modale come farebbe chi ci ripensa. */
            lv_obj_t *ann = cerca_testo(cima, tr(TX_COMMON_CANCEL));
            if (ann) tocca(d, bersaglio(ann));
            continue;
        }

        if (ui_vista() == c->vista_dopo) {
            printf("  ok   \"%s\" porta alla vista %d\n", c->testo,
                   c->vista_dopo);
        } else {
            printf("  NO   \"%s\" doveva portare alla vista %d, siamo alla %d\n",
                   c->testo, c->vista_dopo, ui_vista());
            falliti++;
        }
    }

    falliti += prova_primo_avvio(d);

    printf("\n%s\n", falliti ? "PROVE FALLITE" : "tutti i tocchi arrivano");
    free(buf);
    return falliti ? 1 : 0;
}
