# ---------------------------------------------------------------------------
# Elenco condiviso dei sorgenti.
#
# Lo leggono sia la compilazione per il simulatore (CMakeLists.txt alla radice)
# sia quella per il pannello (main/CMakeLists.txt, ESP-IDF). Un file aggiunto
# qui compare in tutte e due: e il modo per accorgersi subito se qualcosa in
# ui/ ha smesso di compilare per uno dei due bersagli.
#
# In ui/ non ci vanno chiamate all'hardware: quel codice deve girare sul PC.
# ---------------------------------------------------------------------------

set(PANNELLO_MAIN_DIR ${CMAKE_CURRENT_LIST_DIR}/main)

# CONFIGURE_DEPENDS fa rileggere gli elenchi quando compare un file nuovo,
# cosi non serve riconfigurare a mano. ESP-IDF pero legge questo file anche
# in **modalita script** — durante la scansione delle dipendenze fra
# componenti, prima ancora che esista un progetto — e li quell'opzione non
# e ammessa e la configurazione si ferma.
#
# Quindi si aggiunge solo quando c'e una compilazione vera dietro. In
# modalita script gli elenchi vengono calcolati una volta e va benissimo:
# quella passata serve a sapere **quali** componenti servono, non a
# compilarli.
if(CMAKE_SCRIPT_MODE_FILE)
    set(_RILEGGI "")
else()
    set(_RILEGGI CONFIGURE_DEPENDS)
endif()

# Il profilo. profile_def.c e l'unica unita che definisce la tabella.
set(PANNELLO_SRC_PROFILO
    ${PANNELLO_MAIN_DIR}/profile_def.c
)

# Interfaccia: tema, navigazione, schermate, sovrapposizioni, widget.
file(GLOB PANNELLO_SRC_UI ${_RILEGGI}
    ${PANNELLO_MAIN_DIR}/ui/*.c
    ${PANNELLO_MAIN_DIR}/ui/widgets/*.c
    ${PANNELLO_MAIN_DIR}/ui/fonts/*.c
    ${PANNELLO_MAIN_DIR}/ui/icons/*.c
)

# Stato applicativo e fornitori di dati indipendenti dall'hardware.
file(GLOB PANNELLO_SRC_APP ${_RILEGGI}
    ${PANNELLO_MAIN_DIR}/app/*.c
)

# I soli fornitori di dati, senza sezioni.c e sorveglianza.c che disegnano
# e vogliono LVGL. Le prove che non aprono una finestra usano questo, e non
# un elenco scritto a mano: aggiungendo un fornitore, quell'elenco si
# dimenticava — ed e successo due volte.
# The language tables: i18n.c and the texts_gen.c that tools/gen_texts.py
# writes from i18n/<code>.json. Both are committed, so the glob finds them
# on a fresh clone before any generator has run. Defined before the data
# providers because they speak too: "yesterday 07:00", "3 min ago".
file(GLOB PANNELLO_SRC_I18N ${_RILEGGI}
    ${PANNELLO_MAIN_DIR}/i18n/*.c
)

file(GLOB PANNELLO_SRC_DATI ${_RILEGGI}
    ${PANNELLO_MAIN_DIR}/app/dati*.c
)
# Quello da cui i fornitori dipendono, e che non si chiama dati*.c: il
# registro diagnostico — che dati_contatori() interroga per gli errori e
# dati_log() per le righe — e l'orologio, che gli da l'ora di ogni voce.
# Scritti qui e non nel glob perche il nome non li tradisce: chi legge
# quell'asterisco deve poter vedere anche cosa ci sta attaccato.
list(APPEND PANNELLO_SRC_DATI
    ${PANNELLO_MAIN_DIR}/app/registro.c
    ${PANNELLO_MAIN_DIR}/app/orologio.c
    ${PANNELLO_MAIN_DIR}/app/fusi.c      # l'orologio traduce i fusi
    ${PANNELLO_MAIN_DIR}/app/durata.c    # il robot dice le ore dei consumabili
    ${PANNELLO_SRC_I18N}                 # and the providers speak
)
list(REMOVE_DUPLICATES PANNELLO_SRC_DATI)

# Configurazione: lettura, validazione, migrazioni. Indipendente
# dall'archivio, che cambia fra PC e pannello.
file(GLOB PANNELLO_SRC_CFG ${_RILEGGI}
    ${PANNELLO_MAIN_DIR}/cfg/*.c
)

# Server di configurazione: smistamento e regole di accesso. Il trasporto
# no: quello cambia fra PC e pannello, come l'archivio.
file(GLOB PANNELLO_SRC_WEB ${_RILEGGI}
    ${PANNELLO_MAIN_DIR}/web/*.c
)

# Home Assistant: protocollo e stato delle entita. Il trasporto no.
file(GLOB PANNELLO_SRC_HA ${_RILEGGI}
    ${PANNELLO_MAIN_DIR}/ha/*.c
)

# I trasporti a socket. Stanno qui e non in sim/ perche lwIP da i socket BSD
# anche sull'ESP32: lo stesso file serve tutti e due i bersagli, e il
# protocollo che gira sul muro e quello che le prove hanno gia percorso.
file(GLOB PANNELLO_SRC_RETE ${_RILEGGI}
    ${PANNELLO_MAIN_DIR}/rete/*.c
)

set(PANNELLO_SRC_COMUNI
    ${PANNELLO_SRC_PROFILO}
    ${PANNELLO_SRC_UI}
    ${PANNELLO_SRC_APP}
    ${PANNELLO_SRC_I18N}
    ${PANNELLO_SRC_CFG}
    ${PANNELLO_SRC_WEB}
    ${PANNELLO_SRC_HA}
    ${PANNELLO_SRC_RETE}
)

set(PANNELLO_INCLUDE_COMUNI
    ${PANNELLO_MAIN_DIR}
    ${PANNELLO_MAIN_DIR}/ui
    ${PANNELLO_MAIN_DIR}/app
    ${PANNELLO_MAIN_DIR}/i18n
    ${PANNELLO_MAIN_DIR}/cfg
    ${PANNELLO_MAIN_DIR}/web
    ${PANNELLO_MAIN_DIR}/ha
    # Il porto del canale sta qui: lo includono sia ws_socket.c, che e
    # comune, sia le due attuazioni, che comuni non sono.
    ${PANNELLO_MAIN_DIR}/rete
)
