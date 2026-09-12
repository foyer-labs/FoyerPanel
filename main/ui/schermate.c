#include "schermate.h"

#include "comuni.h"
#include "i18n.h"
#include "ui.h"

/* Segnaposto per le sezioni non ancora implementate. Dice cosa manca invece
   di mostrare una pagina vuota, che sembrerebbe un difetto. */
static void segnaposto(lv_obj_t *c, sezione_t s)
{
    const sezione_info_t *info = sezione(s);
    ui_testata(tr(info->nome), NULL);

    lv_obj_t *k = ui_scheda(c);
    lv_obj_set_size(k, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    ui_icona(k, info->icona, C_LINE, IC_L);
    ui_testo(k, tr(info->nome), C_DIM, FT_L);
    ui_testo(k, tr(TX_COMMON_SECTION_NOT_READY), C_DIM, FT_S);
}

void schermata_costruisci(sezione_t s, lv_obj_t *c)
{
    switch (s) {
    case SEZ_HOME: schermata_home(c); break;
    case SEZ_LUCI: schermata_luci(c); break;
    case SEZ_CLIMA: schermata_clima(c); break;
    case SEZ_ENERGIA: schermata_energia(c); break;
    case SEZ_ACCESSI: schermata_accessi(c); break;
    case SEZ_PROGRAMMAZIONI: schermata_programmazioni(c); break;
    case SEZ_AGENDA: schermata_agenda(c); break;
    case SEZ_WIFI: schermata_wifi(c); break;
    case SEZ_INTERRUTTORI: schermata_interruttori(c); break;
    case SEZ_ROBOT: schermata_robot(c); break;
    case SEZ_IMPOSTAZIONI: schermata_impostazioni(c); break;
    case SEZ_DIAGNOSTICA: schermata_diagnostica(c); break;
    default:       segnaposto(c, s);  break;
    }
}
