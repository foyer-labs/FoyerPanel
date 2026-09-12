/* Riga di comando del simulatore. */
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dati.h"
#include "profile.h"

/* L'elenco era rimasto a quattro opzioni mentre il simulatore ne aveva
   quindici: chi lancia --aiuto deve trovarci quello che c'e, altrimenti
   tanto vale non averlo. */
static void aiuto(const char *nome)
{
    printf(
        "Foyer Panel — simulator\n"
        "\n"
        "  %s [opzioni]\n"
        "\n"
        "Profilo\n"
        "  --profilo CHIAVE  avvia su un profilo diverso dal predefinito\n"
        "  --scegli          apre il selettore dei profili\n"
        "  --elenca          stampa i profili disponibili ed esce\n"
        "\n"
        "Dati e configurazione\n"
        "  --dati CARTELLA   dove stanno config.json e la sua copia (dati/)\n"
        "  --vuoto           parte senza configurazione, cioe al primo avvio\n"
        "  --segreto N=V     imposta un segreto (wifi_pw, ha_token, ...)\n"
        "  --ha              si collega a Home Assistant come farebbe il\n"
        "                    pannello; senza, i dati restano inventati\n"
        "  --casi NOME       mette i dati in un caso limite di 11-collaudo.md\n"
        "  --lingua CODICE   language: en, it, ... (overrides sistema.lingua)\n"
        "\n"
        "Dove aprire\n"
        "  --sezione CHIAVE  apre una sezione invece della home\n"
        "  --vista N         apre una vista dentro la sezione\n"
        "  --pagina N        apre una schermata paginata sulla pagina N\n"
        "  --stato NOME      mostra uno stato: avvio, primo-avvio,\n"
        "                    riconnessione, ha-giu, senza-rete, cambia-rete,\n"
        "                    standby, ripristino\n"
        "\n"
        "Catture e prove\n"
        "  --cattura FILE    disegna un fotogramma in un BMP ed esce\n"
        "  --prova-tempi     fa scorrere il tempo e verifica le temporizzazioni\n"
        "  --prova-heap [N]  naviga tutte le schermate e controlla la memoria\n"
        "  --prova-tocco     preme i passaggi dell'interfaccia e verifica\n"
        "                    che il comando arrivi dove deve\n"
        "  --prova-struttura stampa cosa ha ricavato da config.json ed esce\n"
        "  --prova-sorveglianza  le tre soglie del collegamento\n"
        "  --web [PORTA]     serve la pagina di configurazione (predefinita 8080)\n"
        "  --sblocca         parte con la configurazione gia sbloccata\n"
        "  --fps             mostra il contatore di prestazioni di LVGL\n"
        "  --aiuto           questo testo\n"
        "\n"
        "Profili:\n", nome);
    for (int k = 0; k < PRF_QUANTI; k++) printf("  %s\n", profilo_chiave(k));
}

bool sim_opzioni_leggi(int argc, char **argv, sim_opzioni_t *o, int *esito)
{
    *o = (sim_opzioni_t){0};
    *esito = 0;

    for (int n = 1; n < argc; n++) {
        const char *a = argv[n];

        if (!strcmp(a, "--aiuto") || !strcmp(a, "-h") || !strcmp(a, "--help")) {
            aiuto(argv[0]);
            return false;
        }
        if (!strcmp(a, "--elenca")) {
            for (int k = 0; k < PRF_QUANTI; k++) printf("%s\n", profilo_chiave(k));
            return false;
        }
        if (!strcmp(a, "--fps")) { o->fps = true; continue; }
        if (!strcmp(a, "--scegli")) { o->scegli = true; continue; }
        if (!strcmp(a, "--vuoto")) { o->vuoto = true; continue; }
        if (!strcmp(a, "--sblocca")) { o->sblocca = true; continue; }
        if (!strcmp(a, "--ha")) { o->ha = true; continue; }
        if (!strcmp(a, "--prova-sorveglianza")) {
            o->prova_sorveglianza = true;
            continue;
        }
        if (!strcmp(a, "--web")) {
            /* Porta alta: sotto la 1024 servirebbe essere root, e
               sul pannello la 80 la prende esp_http_server. */
            const bool numero = n + 1 < argc &&
                argv[n + 1][0] >= '0' && argv[n + 1][0] <= '9';
            o->web = numero ? atoi(argv[++n]) : 8080;
            continue;
        }
        if (!strcmp(a, "--pagina") && n + 1 < argc) {
            o->pagina = atoi(argv[++n]);
            continue;
        }
        if (!strcmp(a, "--segreto")) {
            if (n + 1 >= argc) {
                fprintf(stderr, "--segreto vuole NOME=VALORE\n");
                *esito = 2;
                return false;
            }
            if (o->quanti_segreti < (int)(sizeof o->segreti / sizeof o->segreti[0]))
                o->segreti[o->quanti_segreti++] = argv[n + 1];
            n++;
            continue;
        }
        if (!strcmp(a, "--prova-struttura")) {
            o->prova_struttura = true;
            continue;
        }
        if (!strcmp(a, "--dati")) {
            if (n + 1 >= argc) {
                fprintf(stderr, "--dati vuole una cartella\n");
                *esito = 2;
                return false;
            }
            o->dati = argv[++n];
            continue;
        }
        if (!strcmp(a, "--prova-tempi")) { o->prova_tempi = true; continue; }
        if (!strcmp(a, "--sezione")) {
            if (n + 1 >= argc) {
                fprintf(stderr, "--sezione vuole una chiave\n");
                *esito = 2;
                return false;
            }
            o->sezione = argv[++n];
            continue;
        }
        if (!strcmp(a, "--vista")) {
            if (n + 1 >= argc) {
                fprintf(stderr, "--vista vuole un numero\n");
                *esito = 2;
                return false;
            }
            o->vista = atoi(argv[++n]);
            continue;
        }
        if (!strcmp(a, "--prova-tocco")) { o->prova_tocco = true; continue; }
        if (!strcmp(a, "--prova-heap")) {
            /* Un numero negativo chiede il dettaglio giro per giro. */
            const bool numero = n + 1 < argc &&
                ((argv[n + 1][0] >= '0' && argv[n + 1][0] <= '9') ||
                 (argv[n + 1][0] == '-' && argv[n + 1][1] >= '0'
                                        && argv[n + 1][1] <= '9'));
            o->prova_heap = numero ? atoi(argv[++n]) : 20;
            continue;
        }
        if (!strcmp(a, "--stato")) {
            if (n + 1 >= argc) {
                fprintf(stderr, "--stato vuole un nome\n");
                *esito = 2;
                return false;
            }
            o->stato = argv[++n];
            continue;
        }
        if (!strcmp(a, "--lingua")) {
            if (n + 1 >= argc) {
                fprintf(stderr, "--lingua wants a language code\n");
                *esito = 2;
                return false;
            }
            o->lingua = argv[++n];
            continue;
        }
        if (!strcmp(a, "--casi")) {
            if (n + 1 >= argc) {
                fprintf(stderr, "--casi vuole un nome\n");
                *esito = 2;
                return false;
            }
            /* Si ricorda soltanto: applicarlo qui vorrebbe dire metterlo
               **prima** della configurazione, e i fornitori contano le unita
               con cfg_quanti(). Applicato durante la lettura degli argomenti
               ne trovava zero e ci restava: ogni cattura di un caso limite
               usciva con le schede vuote. Lo applica main_sim.c dopo
               sim_configura(). */
            o->caso = argv[++n];
            continue;
        }
        if (!strcmp(a, "--cattura")) {
            if (n + 1 >= argc) {
                fprintf(stderr, "--cattura vuole un percorso\n");
                *esito = 2;
                return false;
            }
            o->cattura = argv[++n];
            continue;
        }
        if (!strcmp(a, "--profilo")) {
            if (n + 1 >= argc) {
                fprintf(stderr, "--profilo vuole una chiave\n");
                *esito = 2;
                return false;
            }
            o->profilo = argv[++n];
            continue;
        }
        fprintf(stderr, "opzione sconosciuta: %s\n", a);
        *esito = 2;
        return false;
    }

    if (o->profilo && !profilo_scegli(o->profilo)) {
        fprintf(stderr, "profilo sconosciuto: %s\nprofili:\n", o->profilo);
        for (int k = 0; k < PRF_QUANTI; k++) fprintf(stderr, "  %s\n", profilo_chiave(k));
        *esito = 2;
        return false;
    }
    return true;
}
