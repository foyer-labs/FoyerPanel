# Il vetro della scheda (Guition JC8012P4A1C): un JD9365 su MIPI-DSI.
#
# **Questa e la variante "Old Panel".** La stessa scheda esiste in due
# versioni con sequenze di accensione e tempi diversi, e si distinguono
# dall'etichetta dietro il guscio: con `V2` dopo il codice e la nuova, senza
# e questa. Le istruzioni di programmazione del costruttore lo dicono in una
# riga, ed e una riga che vale una serata.
#
# Il file viene da 1-Demo/IDF_5.5.4/JC8012P4A1C_I_W_Y_Old_Panel. Non si
# prende dal registro, ed e l'eccezione alla regola scritta in cima a
# firmware/main/idf_component.yml: la sequenza del registro dichiara al
# pannello quattro corsie DSI e questa scheda ne ha due, e i registri di
# accensione della variante vecchia sono diversi da quelli della nuova.

idf_component_register(
    SRCS "esp_lcd_jd9365.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "driver"
    REQUIRES "esp_lcd")

# Il CMake del costruttore chiedeva qui `cmake_utilities`, solo per ricavare
# la versione dal proprio manifesto. Quel manifesto e stato tolto — una
# copia dentro il repository non e un pacchetto del registro — e con lui il
# motivo di tirarsi dietro un altro componente per tre numeri.
#
# I numeri restano perche' il sorgente li stampa all'avvio: nel registro si
# legge quale copia del driver sta girando, ed e esattamente cio che
# servirebbe sapere se un giorno lo schermo tornasse nero.
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    ESP_LCD_JD9365_VER_MAJOR=1
    ESP_LCD_JD9365_VER_MINOR=0
    ESP_LCD_JD9365_VER_PATCH=2)
