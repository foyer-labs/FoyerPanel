<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="logo/foyer-dark.svg">
    <img src="logo/foyer-light.svg" alt="Foyer Panel" width="360">
  </picture>
</p>

# Foyer Panel — la specifica

Pannelli di controllo domotici da parete collegati a Home Assistant,
**stessa interfaccia e una sola base di codice**:

| Profilo | Pannello | Risoluzione | Orientamento |
|---|---|---|---|
| `p4-1280x800` | 10,1" ESP32-P4 | 1280×800 | orizzontale |
| `p4-800x1280` | 10,1" ESP32-P4 | 800×1280 | verticale |

Questa cartella è la specifica da cui il codice è stato scritto, ed è
tenuta allineata al codice: una modifica al comportamento cambia il
documento che la descrive nello stesso commit. Come si compila e come si
prova sta in [`sviluppo.md`](sviluppo.md); la presentazione del progetto
nel [README](../README.it.md) in cima al repository.

## La regola della specifica

**`09-profili.md` è l'unico documento che contiene numeri e comportamenti
che cambiano fra i pannelli.** Tutti gli altri descrivono struttura e
gerarchia. Se trovi una misura dipendente dal pannello altrove, è un
errore di quel documento.

Nel codice la controparte è `profile.h`, unico file con misure di layout,
generato da `profili.json`.

## Contenuto

| File | Cosa contiene |
|---|---|
| `sviluppo.md` | ambiente, compilazione, simulatore, prove, aggiornamento, licenze |
| `01-specifica-ui.md` | ogni schermata, stati, comportamenti, temporizzazioni |
| `02-design-tokens.md` | palette predefinita, icone, animazioni |
| `03-config-contratto.md` | configurazione, API, migrazioni, validazione |
| `04-config.example.json` | una configurazione d'esempio completa, con entità inventate |
| `05-architettura-firmware.md` | task, memoria, partizioni, OTA, macchina a stati |
| `06-ha-package.yaml` | pacchetto Home Assistant con i sensori aggregati che il pannello legge |
| `09-profili.md` | **tutte le divergenze fra i pannelli** |
| `profili.json` | le stesse tabelle, leggibili da programma |
| `10-diagnostica.md` | registro, contatori, endpoint di stato, sensore HA |
| `11-collaudo.md` | criteri di accettazione per fase, casi limite, prove dal vivo |
| `12-sicurezza.md` | cosa protegge il pannello, e da chi |
| `13-primo-avvio-p4.md` | la scheda P4 nuova: compilare, scrivere, cosa guardare se non parte |
| `config.schema.json` | schema formale della configurazione |
| `screenshots/` | le schermate del README, rifatte da `tools/screenshots.py`, e il supporto da parete da `tools/render_stl.py` |
| `mockup/verticale/` | rendering 1:1 del profilo verticale |
| `mockup/10-pollici/` | rendering 1:1 a 1280×800 |
| `mockup/clima-condizionatori.html` | sezione Clima |
| `mockup/pagina-configurazione.html` | pagina web, non dipende dal pannello — la pagina vera è `web/index.html` |

## Sintesi delle decisioni

**Navigazione**: home a tutta larghezza con i riquadri di sezione; dentro
le sezioni un rail compatto di sole icone, con HOME come ritorno unico e
l'ingranaggio delle impostazioni in fondo. Una sezione senza entità da
mostrare non compare.

**Home**: presenza, energia in sintesi, aperture e luci accese, agenda del
giorno, meteo con le prossime tre ore.

**Accessi**: comandi a impulso e interruttori. Dove un accesso ha un
sensore di stato l'interfaccia lo usa; dove non ce l'ha lo dichiara: dove
non sa, non finge di sapere.

**Clima**: zone di riscaldamento con il solo setpoint, condizionatori con
modalità, ventilazione, deflettore e timer.

**Configurazione su due livelli**: sul pannello solo ciò che serve quando
la rete non funziona; tutto il resto da una pagina web servita dal
pannello stesso, accesa solo dal pannello e per quindici minuti.

**Aggiornamento**: OTA con caricamento del file dalla pagina web, verifica
della firma, rollback automatico e partizione di recupero.
