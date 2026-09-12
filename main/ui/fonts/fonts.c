/* GENERATO da tools/genera_font.py — non modificare a mano. */
#include "fonts.h"

#include "profile.h"


LV_FONT_DECLARE(font_md_17);
LV_FONT_DECLARE(font_mono_34);
LV_FONT_DECLARE(font_rg_14);
LV_FONT_DECLARE(font_sb_104);
LV_FONT_DECLARE(font_sb_12);
LV_FONT_DECLARE(font_sb_126);
LV_FONT_DECLARE(font_sb_150);
LV_FONT_DECLARE(font_sb_26);
LV_FONT_DECLARE(font_sb_30);
LV_FONT_DECLARE(font_sb_38);
LV_FONT_DECLARE(font_sb_52);
LV_FONT_DECLARE(font_sb_56);
LV_FONT_DECLARE(font_sb_60);
LV_FONT_DECLARE(font_sb_84);
LV_FONT_DECLARE(font_sb_96);

static const lv_font_t *const TABELLA[FAM_QUANTE][FT_QUANTI] = {
    [FAM_P4] = {
        [FT_XXL] = &font_sb_56,
        [FT_XL] = &font_sb_38,
        [FT_L] = &font_sb_26,
        [FT_M] = &font_md_17,
        [FT_S] = &font_rg_14,
        [FT_XS] = &font_sb_12,
        [FT_STANDBY] = &font_sb_126,
        [FT_STANDBY_TEMP] = &font_sb_84,
        [FT_STANDBY_1] = &font_sb_60,
        [FT_STANDBY_2] = &font_sb_84,
        [FT_STANDBY_3] = &font_sb_104,
        [FT_STANDBY_4] = &font_sb_126,
        [FT_STANDBY_5] = &font_sb_150,
        [FT_ENERGIA] = &font_sb_52,
        [FT_CLIMA] = &font_sb_96,
        [FT_MONO] = &font_mono_34,
        [FT_DECIMI_XL] = &font_sb_30,
    },
};

const lv_font_t *font(font_ruolo_t ruolo)
{
    if ((unsigned)ruolo >= (unsigned)FT_QUANTI) ruolo = FT_M;
    const lv_font_t *f = TABELLA[PRF->famiglia][ruolo];
    return f ? f : LV_FONT_DEFAULT;
}
