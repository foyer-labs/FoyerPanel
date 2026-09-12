# ----------------------------------------------------------------------------
# Quale config.json finisce nell'immagine di `storage` — vedi CMakeLists.txt.
#
# Si chiama con -DDATI=… -DESEMPIO=… -DDEST=…, a ogni compilazione.
#
# `dati/config.json` esiste solo dopo che il simulatore e partito almeno una
# volta: e lui che ce lo copia. Su un repository appena clonato non c'e, e
# pretenderlo voleva dire che `idf.py build` si fermava su un file che
# nessuno aveva detto di creare. Allora si prende la configurazione d'esempio,
# che e valida per costruzione — la prova `configurazione` la valida a ogni
# ctest — e lo si dice, perche' un pannello scritto con l'esempio mostra una
# casa che non e la tua.
#
# La scelta sta qui e non nel CMakeLists: fatta alla configurazione, il giorno
# che il simulatore crea `dati/` la compilazione continuerebbe a usare
# l'esempio finche' qualcuno non riconfigura.
# ----------------------------------------------------------------------------
if(EXISTS "${DATI}")
    set(sorgente "${DATI}")
else()
    set(sorgente "${ESEMPIO}")
    message(STATUS "storage: dati/config.json non c'e, uso "
                   "docs/04-config.example.json")
endif()
file(COPY_FILE "${sorgente}" "${DEST}" ONLY_IF_DIFFERENT)
