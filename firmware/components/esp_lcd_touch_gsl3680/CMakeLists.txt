# Il vetro capacitivo della scheda (Guition JC8012P4A1C): un Silead
# GSL3680, che non ha il firmware a bordo e se lo fa caricare via I2C a ogni
# accensione. Quel blocco — gsl_point_id.c, ottantamila righe di tabella — e
# legato a questo modello di vetro e viene dal pacchetto del costruttore:
# non esiste nel registro di Espressif e non si puo dedurre.

idf_component_register(
    SRCS "esp_lcd_touch_gsl3680.c" "gsl_point_id.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_lcd" "esp_lcd_touch")
