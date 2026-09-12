# ---------------------------------------------------------------------------
# Compilazione per il pannello — ESP-IDF.
#
#     cd firmware
#     idf.py -B build-p4 set-target esp32p4
#     idf.py -B build-p4 -D SDKCONFIG=sdkconfig.p4 build flash monitor
#
# Sta in una cartella sua e non alla radice perche alla radice c'e gia il
# CMakeLists del simulatore, e ESP-IDF vuole il proprio in cima al progetto.
# Sono due bersagli dello stesso codice: `main/` e condivisa, e la differenza
# la fanno i porti — archivio, magazzino, trasporti — che qui sono quelli
# dell'apparecchio e li quelli del PC.
#
# Il profilo si sceglie **alla compilazione** e ce n'e uno solo: il pannello
# non ha un selettore all'avvio, e PANNELLO_PROFILO fissa quale tabella di
# profile.h finisce nel binario. Le altre non ci entrano nemmeno.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# --- quale orientamento ---------------------------------------------------
#
# Il profilo si sceglie alla compilazione e ce n'e uno solo nel binario: il
# pannello non ha un selettore all'avvio, e PANNELLO_PROFILO fissa quale
# tabella di profile.h ci entra. L'altra non ci arriva nemmeno.
#
# Il predefinito e il verticale, che e quello appeso al muro. Resta una
# scelta e non un automatismo: quale dei due orientamenti sia montato non
# c'e modo di dedurlo dal silicio, che e lo stesso. Si passa da riga di
# comando quando serve l'altro:
#
#     idf.py -B build-p4 -D PANNELLO_PROFILO_SCELTA=PRF_P4_1280X800 build
if(NOT DEFINED PANNELLO_PROFILO_SCELTA)
    set(PANNELLO_PROFILO_PREDEFINITO "PRF_P4_800X1280")
    set(PANNELLO_PROFILO_SCELTA "${PANNELLO_PROFILO_PREDEFINITO}"
        CACHE STRING "profilo del pannello")
endif()

# Una tabella che non appartiene al bersaglio non da errore: da uno schermo
# che disegna alla misura sbagliata. Meglio fermarsi qui. Oggi il controllo
# puo scattare solo se qualcuno passa un PRF_ inventato, e va tenuto lo
# stesso: e la rete sotto la riga di comando, ed e gratis.
if(IDF_TARGET STREQUAL "esp32p4" AND NOT PANNELLO_PROFILO_SCELTA MATCHES "^PRF_P4_")
    message(FATAL_ERROR
        "profilo ${PANNELLO_PROFILO_SCELTA} su bersaglio esp32p4: "
        "servono PRF_P4_1280X800 o PRF_P4_800X1280")
endif()
message(STATUS "pannello: ${PANNELLO_PROFILO_SCELTA} su ${IDF_TARGET}")

project(pannello)

# --- l'immagine di `storage`, da scrivere a mano quando serve --------------
#
# Costruisce una LittleFS con dentro `dati/` — cioe config.json e la sua
# copia di sicurezza — e la lascia in build/storage.bin.
#
# **Non** si flasha con l'applicazione, e non e una dimenticanza: manca
# apposta FLASH_IN_PROJECT. Scriverla a ogni flash cancellerebbe la
# configurazione del pannello a ogni aggiornamento, che e precisamente cio
# che l'OTA promette di non fare.
#
# Si scrive quando serve, e serve due volte: la prima, su un pannello nuovo
# che non ha ancora rete e quindi non puo ricevere il documento dalla pagina
# web; e quando si vuole rimettere una configurazione nota dopo averne
# provata una storta.
#
#     idf.py -B build-p4 -D SDKCONFIG=sdkconfig.p4 -p COMx storage-flash
# La cartella `dati/` non ci va **intera**, e questa e la ragione. Sul PC
# quella e la radice dell'archivio del simulatore, e il simulatore ci tiene
# dentro anche i segreti: `dati/segreti/` e uno spazio del magazzino, non
# della configurazione. Prendendola tutta, una password del Wi-Fi scritta
# durante una prova finiva in chiaro dentro la partizione del pannello —
# dove il pannello non la legge nemmeno, perche' i segreti li tiene in NVS,
# ma dove non deve stare: 03-config-contratto.md §1 dice che un segreto non
# entra in cio che si esporta o si copia, e un'immagine flashabile e
# esattamente cio che si copia.
#
# Quindi si allestisce una cartella con dentro il solo documento. La copia
# rifa il giro a ogni compilazione del bersaglio, cosi una modifica a
# config.json non richiede di riconfigurare. Se `dati/` non c'e ancora —
# un repository appena clonato — si prende l'esempio: copia_config.cmake.
set(STORAGE_STAGE "${CMAKE_BINARY_DIR}/storage_img")
add_custom_target(storage_solo_config
    COMMAND ${CMAKE_COMMAND} -E make_directory "${STORAGE_STAGE}"
    COMMAND ${CMAKE_COMMAND}
            "-DDATI=${CMAKE_CURRENT_LIST_DIR}/../dati/config.json"
            "-DESEMPIO=${CMAKE_CURRENT_LIST_DIR}/../docs/04-config.example.json"
            "-DDEST=${STORAGE_STAGE}/config.json"
            -P "${CMAKE_CURRENT_LIST_DIR}/copia_config.cmake"
    VERBATIM)

littlefs_create_partition_image(storage "${STORAGE_STAGE}"
                               DEPENDS storage_solo_config)
