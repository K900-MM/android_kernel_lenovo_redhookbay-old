/*
 * wm5102.c  --  WM5102 ALSA SoC Audio driver
 *
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/gpio.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/registers.h>

#include "arizona.h"
#include "wm5102.h"
#include "wm_adsp.h"

#define SPEAKER_PA_GPIO 112 //GP_CORE_016

struct wm5102_priv *wm5102_p;

/* add call state update code */
static int call_state_flag = 0;
static int call_state_flag_parm_set(const char *val, struct kernel_param *kp)
{
    param_set_int(val, kp);

    return 0;
}
module_param_call(call_state_flag, call_state_flag_parm_set, param_get_int,
        &call_state_flag, S_IWUSR | S_IRUGO);
/*--------------------------------end--------------------------------*/

static int micbias_ev(struct snd_soc_dapm_widget *w,
                     struct snd_kcontrol *kcontrol, int event)
{
//       struct snd_soc_codec *codec = w->codec;
//       struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
//       struct arizona *arizona = priv->arizona;
//       struct device *dev = arizona->dev;

       if (!IS_ERR(micvdd_swit)) {
           switch (event) {
               case SND_SOC_DAPM_PRE_PMU:
                   if (!micvdd_enable_flag) {
                       regulator_enable(micvdd_swit);
                       micvdd_enable_flag = true;
                   }
                    mic_ev_enabled = true;
                   break;
               case SND_SOC_DAPM_POST_PMD:
                   if (mic_ev_enabled && (!micvdd_enable_flag)) {
                       regulator_disable(micvdd_swit);
                       micvdd_enable_flag = false;
                   }
                   mic_ev_enabled = false;
                   break;
           }
       }

       return 0;
}

static DECLARE_TLV_DB_SCALE(ana_tlv, 0, 100, 0);
static DECLARE_TLV_DB_SCALE(eq_tlv, -1200, 100, 0);
static DECLARE_TLV_DB_SCALE(digital_tlv, -6400, 50, 0);
static DECLARE_TLV_DB_SCALE(noise_tlv, 0, 600, 0);
static DECLARE_TLV_DB_LINEAR(linear_tlv, 0, 65535);

static const struct wm_adsp_region wm5102_dsp1_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x100000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x180000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x190000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x1a8000 },
};

static const struct reg_default wm5102_sysclk_reva_patch[] = {
	{ 0x3000, 0x2225 },
	{ 0x3001, 0x3a03 },
	{ 0x3002, 0x0225 },
	{ 0x3003, 0x0801 },
	{ 0x3004, 0x6249 },
	{ 0x3005, 0x0c04 },
	{ 0x3006, 0x0225 },
	{ 0x3007, 0x5901 },
	{ 0x3008, 0xe249 },
	{ 0x3009, 0x030d },
	{ 0x300a, 0x0249 },
	{ 0x300b, 0x2c01 },
	{ 0x300c, 0xe249 },
	{ 0x300d, 0x4342 },
	{ 0x300e, 0xe249 },
	{ 0x300f, 0x73c0 },
	{ 0x3010, 0x4249 },
	{ 0x3011, 0x0c00 },
	{ 0x3012, 0x0225 },
	{ 0x3013, 0x1f01 },
	{ 0x3014, 0x0225 },
	{ 0x3015, 0x1e01 },
	{ 0x3016, 0x0225 },
	{ 0x3017, 0xfa00 },
	{ 0x3018, 0x0000 },
	{ 0x3019, 0xf000 },
	{ 0x301a, 0x0000 },
	{ 0x301b, 0xf000 },
	{ 0x301c, 0x0000 },
	{ 0x301d, 0xf000 },
	{ 0x301e, 0x0000 },
	{ 0x301f, 0xf000 },
	{ 0x3020, 0x0000 },
	{ 0x3021, 0xf000 },
	{ 0x3022, 0x0000 },
	{ 0x3023, 0xf000 },
	{ 0x3024, 0x0000 },
	{ 0x3025, 0xf000 },
	{ 0x3026, 0x0000 },
	{ 0x3027, 0xf000 },
	{ 0x3028, 0x0000 },
	{ 0x3029, 0xf000 },
	{ 0x302a, 0x0000 },
	{ 0x302b, 0xf000 },
	{ 0x302c, 0x0000 },
	{ 0x302d, 0xf000 },
	{ 0x302e, 0x0000 },
	{ 0x302f, 0xf000 },
	{ 0x3030, 0x0225 },
	{ 0x3031, 0x1a01 },
	{ 0x3032, 0x0225 },
	{ 0x3033, 0x1e00 },
	{ 0x3034, 0x0225 },
	{ 0x3035, 0x1f00 },
	{ 0x3036, 0x6225 },
	{ 0x3037, 0xf800 },
	{ 0x3038, 0x0000 },
	{ 0x3039, 0xf000 },
	{ 0x303a, 0x0000 },
	{ 0x303b, 0xf000 },
	{ 0x303c, 0x0000 },
	{ 0x303d, 0xf000 },
	{ 0x303e, 0x0000 },
	{ 0x303f, 0xf000 },
	{ 0x3040, 0x2226 },
	{ 0x3041, 0x3a03 },
	{ 0x3042, 0x0226 },
	{ 0x3043, 0x0801 },
	{ 0x3044, 0x6249 },
	{ 0x3045, 0x0c06 },
	{ 0x3046, 0x0226 },
	{ 0x3047, 0x5901 },
	{ 0x3048, 0xe249 },
	{ 0x3049, 0x030d },
	{ 0x304a, 0x0249 },
	{ 0x304b, 0x2c01 },
	{ 0x304c, 0xe249 },
	{ 0x304d, 0x4342 },
	{ 0x304e, 0xe249 },
	{ 0x304f, 0x73c0 },
	{ 0x3050, 0x4249 },
	{ 0x3051, 0x0c00 },
	{ 0x3052, 0x0226 },
	{ 0x3053, 0x1f01 },
	{ 0x3054, 0x0226 },
	{ 0x3055, 0x1e01 },
	{ 0x3056, 0x0226 },
	{ 0x3057, 0xfa00 },
	{ 0x3058, 0x0000 },
	{ 0x3059, 0xf000 },
	{ 0x305a, 0x0000 },
	{ 0x305b, 0xf000 },
	{ 0x305c, 0x0000 },
	{ 0x305d, 0xf000 },
	{ 0x305e, 0x0000 },
	{ 0x305f, 0xf000 },
	{ 0x3060, 0x0000 },
	{ 0x3061, 0xf000 },
	{ 0x3062, 0x0000 },
	{ 0x3063, 0xf000 },
	{ 0x3064, 0x0000 },
	{ 0x3065, 0xf000 },
	{ 0x3066, 0x0000 },
	{ 0x3067, 0xf000 },
	{ 0x3068, 0x0000 },
	{ 0x3069, 0xf000 },
	{ 0x306a, 0x0000 },
	{ 0x306b, 0xf000 },
	{ 0x306c, 0x0000 },
	{ 0x306d, 0xf000 },
	{ 0x306e, 0x0000 },
	{ 0x306f, 0xf000 },
	{ 0x3070, 0x0226 },
	{ 0x3071, 0x1a01 },
	{ 0x3072, 0x0226 },
	{ 0x3073, 0x1e00 },
	{ 0x3074, 0x0226 },
	{ 0x3075, 0x1f00 },
	{ 0x3076, 0x6226 },
	{ 0x3077, 0xf800 },
	{ 0x3078, 0x0000 },
	{ 0x3079, 0xf000 },
	{ 0x307a, 0x0000 },
	{ 0x307b, 0xf000 },
	{ 0x307c, 0x0000 },
	{ 0x307d, 0xf000 },
	{ 0x307e, 0x0000 },
	{ 0x307f, 0xf000 },
	{ 0x3080, 0x2227 },
	{ 0x3081, 0x3a03 },
	{ 0x3082, 0x0227 },
	{ 0x3083, 0x0801 },
	{ 0x3084, 0x6255 },
	{ 0x3085, 0x0c04 },
	{ 0x3086, 0x0227 },
	{ 0x3087, 0x5901 },
	{ 0x3088, 0xe255 },
	{ 0x3089, 0x030d },
	{ 0x308a, 0x0255 },
	{ 0x308b, 0x2c01 },
	{ 0x308c, 0xe255 },
	{ 0x308d, 0x4342 },
	{ 0x308e, 0xe255 },
	{ 0x308f, 0x73c0 },
	{ 0x3090, 0x4255 },
	{ 0x3091, 0x0c00 },
	{ 0x3092, 0x0227 },
	{ 0x3093, 0x1f01 },
	{ 0x3094, 0x0227 },
	{ 0x3095, 0x1e01 },
	{ 0x3096, 0x0227 },
	{ 0x3097, 0xfa00 },
	{ 0x3098, 0x0000 },
	{ 0x3099, 0xf000 },
	{ 0x309a, 0x0000 },
	{ 0x309b, 0xf000 },
	{ 0x309c, 0x0000 },
	{ 0x309d, 0xf000 },
	{ 0x309e, 0x0000 },
	{ 0x309f, 0xf000 },
	{ 0x30a0, 0x0000 },
	{ 0x30a1, 0xf000 },
	{ 0x30a2, 0x0000 },
	{ 0x30a3, 0xf000 },
	{ 0x30a4, 0x0000 },
	{ 0x30a5, 0xf000 },
	{ 0x30a6, 0x0000 },
	{ 0x30a7, 0xf000 },
	{ 0x30a8, 0x0000 },
	{ 0x30a9, 0xf000 },
	{ 0x30aa, 0x0000 },
	{ 0x30ab, 0xf000 },
	{ 0x30ac, 0x0000 },
	{ 0x30ad, 0xf000 },
	{ 0x30ae, 0x0000 },
	{ 0x30af, 0xf000 },
	{ 0x30b0, 0x0227 },
	{ 0x30b1, 0x1a01 },
	{ 0x30b2, 0x0227 },
	{ 0x30b3, 0x1e00 },
	{ 0x30b4, 0x0227 },
	{ 0x30b5, 0x1f00 },
	{ 0x30b6, 0x6227 },
	{ 0x30b7, 0xf800 },
	{ 0x30b8, 0x0000 },
	{ 0x30b9, 0xf000 },
	{ 0x30ba, 0x0000 },
	{ 0x30bb, 0xf000 },
	{ 0x30bc, 0x0000 },
	{ 0x30bd, 0xf000 },
	{ 0x30be, 0x0000 },
	{ 0x30bf, 0xf000 },
	{ 0x30c0, 0x2228 },
	{ 0x30c1, 0x3a03 },
	{ 0x30c2, 0x0228 },
	{ 0x30c3, 0x0801 },
	{ 0x30c4, 0x6255 },
	{ 0x30c5, 0x0c06 },
	{ 0x30c6, 0x0228 },
	{ 0x30c7, 0x5901 },
	{ 0x30c8, 0xe255 },
	{ 0x30c9, 0x030d },
	{ 0x30ca, 0x0255 },
	{ 0x30cb, 0x2c01 },
	{ 0x30cc, 0xe255 },
	{ 0x30cd, 0x4342 },
	{ 0x30ce, 0xe255 },
	{ 0x30cf, 0x73c0 },
	{ 0x30d0, 0x4255 },
	{ 0x30d1, 0x0c00 },
	{ 0x30d2, 0x0228 },
	{ 0x30d3, 0x1f01 },
	{ 0x30d4, 0x0228 },
	{ 0x30d5, 0x1e01 },
	{ 0x30d6, 0x0228 },
	{ 0x30d7, 0xfa00 },
	{ 0x30d8, 0x0000 },
	{ 0x30d9, 0xf000 },
	{ 0x30da, 0x0000 },
	{ 0x30db, 0xf000 },
	{ 0x30dc, 0x0000 },
	{ 0x30dd, 0xf000 },
	{ 0x30de, 0x0000 },
	{ 0x30df, 0xf000 },
	{ 0x30e0, 0x0000 },
	{ 0x30e1, 0xf000 },
	{ 0x30e2, 0x0000 },
	{ 0x30e3, 0xf000 },
	{ 0x30e4, 0x0000 },
	{ 0x30e5, 0xf000 },
	{ 0x30e6, 0x0000 },
	{ 0x30e7, 0xf000 },
	{ 0x30e8, 0x0000 },
	{ 0x30e9, 0xf000 },
	{ 0x30ea, 0x0000 },
	{ 0x30eb, 0xf000 },
	{ 0x30ec, 0x0000 },
	{ 0x30ed, 0xf000 },
	{ 0x30ee, 0x0000 },
	{ 0x30ef, 0xf000 },
	{ 0x30f0, 0x0228 },
	{ 0x30f1, 0x1a01 },
	{ 0x30f2, 0x0228 },
	{ 0x30f3, 0x1e00 },
	{ 0x30f4, 0x0228 },
	{ 0x30f5, 0x1f00 },
	{ 0x30f6, 0x6228 },
	{ 0x30f7, 0xf800 },
	{ 0x30f8, 0x0000 },
	{ 0x30f9, 0xf000 },
	{ 0x30fa, 0x0000 },
	{ 0x30fb, 0xf000 },
	{ 0x30fc, 0x0000 },
	{ 0x30fd, 0xf000 },
	{ 0x30fe, 0x0000 },
	{ 0x30ff, 0xf000 },
	{ 0x3100, 0x222b },
	{ 0x3101, 0x3a03 },
	{ 0x3102, 0x222b },
	{ 0x3103, 0x5803 },
	{ 0x3104, 0xe26f },
	{ 0x3105, 0x030d },
	{ 0x3106, 0x626f },
	{ 0x3107, 0x2c01 },
	{ 0x3108, 0xe26f },
	{ 0x3109, 0x4342 },
	{ 0x310a, 0xe26f },
	{ 0x310b, 0x73c0 },
	{ 0x310c, 0x026f },
	{ 0x310d, 0x0c00 },
	{ 0x310e, 0x022b },
	{ 0x310f, 0x1f01 },
	{ 0x3110, 0x022b },
	{ 0x3111, 0x1e01 },
	{ 0x3112, 0x022b },
	{ 0x3113, 0xfa00 },
	{ 0x3114, 0x0000 },
	{ 0x3115, 0xf000 },
	{ 0x3116, 0x0000 },
	{ 0x3117, 0xf000 },
	{ 0x3118, 0x0000 },
	{ 0x3119, 0xf000 },
	{ 0x311a, 0x0000 },
	{ 0x311b, 0xf000 },
	{ 0x311c, 0x0000 },
	{ 0x311d, 0xf000 },
	{ 0x311e, 0x0000 },
	{ 0x311f, 0xf000 },
	{ 0x3120, 0x022b },
	{ 0x3121, 0x0a01 },
	{ 0x3122, 0x022b },
	{ 0x3123, 0x1e00 },
	{ 0x3124, 0x022b },
	{ 0x3125, 0x1f00 },
	{ 0x3126, 0x622b },
	{ 0x3127, 0xf800 },
	{ 0x3128, 0x0000 },
	{ 0x3129, 0xf000 },
	{ 0x312a, 0x0000 },
	{ 0x312b, 0xf000 },
	{ 0x312c, 0x0000 },
	{ 0x312d, 0xf000 },
	{ 0x312e, 0x0000 },
	{ 0x312f, 0xf000 },
	{ 0x3130, 0x0000 },
	{ 0x3131, 0xf000 },
	{ 0x3132, 0x0000 },
	{ 0x3133, 0xf000 },
	{ 0x3134, 0x0000 },
	{ 0x3135, 0xf000 },
	{ 0x3136, 0x0000 },
	{ 0x3137, 0xf000 },
	{ 0x3138, 0x0000 },
	{ 0x3139, 0xf000 },
	{ 0x313a, 0x0000 },
	{ 0x313b, 0xf000 },
	{ 0x313c, 0x0000 },
	{ 0x313d, 0xf000 },
	{ 0x313e, 0x0000 },
	{ 0x313f, 0xf000 },
	{ 0x3140, 0x0000 },
	{ 0x3141, 0xf000 },
	{ 0x3142, 0x0000 },
	{ 0x3143, 0xf000 },
	{ 0x3144, 0x0000 },
	{ 0x3145, 0xf000 },
	{ 0x3146, 0x0000 },
	{ 0x3147, 0xf000 },
	{ 0x3148, 0x0000 },
	{ 0x3149, 0xf000 },
	{ 0x314a, 0x0000 },
	{ 0x314b, 0xf000 },
	{ 0x314c, 0x0000 },
	{ 0x314d, 0xf000 },
	{ 0x314e, 0x0000 },
	{ 0x314f, 0xf000 },
	{ 0x3150, 0x0000 },
	{ 0x3151, 0xf000 },
	{ 0x3152, 0x0000 },
	{ 0x3153, 0xf000 },
	{ 0x3154, 0x0000 },
	{ 0x3155, 0xf000 },
	{ 0x3156, 0x0000 },
	{ 0x3157, 0xf000 },
	{ 0x3158, 0x0000 },
	{ 0x3159, 0xf000 },
	{ 0x315a, 0x0000 },
	{ 0x315b, 0xf000 },
	{ 0x315c, 0x0000 },
	{ 0x315d, 0xf000 },
	{ 0x315e, 0x0000 },
	{ 0x315f, 0xf000 },
	{ 0x3160, 0x0000 },
	{ 0x3161, 0xf000 },
	{ 0x3162, 0x0000 },
	{ 0x3163, 0xf000 },
	{ 0x3164, 0x0000 },
	{ 0x3165, 0xf000 },
	{ 0x3166, 0x0000 },
	{ 0x3167, 0xf000 },
	{ 0x3168, 0x0000 },
	{ 0x3169, 0xf000 },
	{ 0x316a, 0x0000 },
	{ 0x316b, 0xf000 },
	{ 0x316c, 0x0000 },
	{ 0x316d, 0xf000 },
	{ 0x316e, 0x0000 },
	{ 0x316f, 0xf000 },
	{ 0x3170, 0x0000 },
	{ 0x3171, 0xf000 },
	{ 0x3172, 0x0000 },
	{ 0x3173, 0xf000 },
	{ 0x3174, 0x0000 },
	{ 0x3175, 0xf000 },
	{ 0x3176, 0x0000 },
	{ 0x3177, 0xf000 },
	{ 0x3178, 0x0000 },
	{ 0x3179, 0xf000 },
	{ 0x317a, 0x0000 },
	{ 0x317b, 0xf000 },
	{ 0x317c, 0x0000 },
	{ 0x317d, 0xf000 },
	{ 0x317e, 0x0000 },
	{ 0x317f, 0xf000 },
	{ 0x3180, 0x2001 },
	{ 0x3181, 0xf101 },
	{ 0x3182, 0x0000 },
	{ 0x3183, 0xf000 },
	{ 0x3184, 0x0000 },
	{ 0x3185, 0xf000 },
	{ 0x3186, 0x0000 },
	{ 0x3187, 0xf000 },
	{ 0x3188, 0x0000 },
	{ 0x3189, 0xf000 },
	{ 0x318a, 0x0000 },
	{ 0x318b, 0xf000 },
	{ 0x318c, 0x0000 },
	{ 0x318d, 0xf000 },
	{ 0x318e, 0x0000 },
	{ 0x318f, 0xf000 },
	{ 0x3190, 0x0000 },
	{ 0x3191, 0xf000 },
	{ 0x3192, 0x0000 },
	{ 0x3193, 0xf000 },
	{ 0x3194, 0x0000 },
	{ 0x3195, 0xf000 },
	{ 0x3196, 0x0000 },
	{ 0x3197, 0xf000 },
	{ 0x3198, 0x0000 },
	{ 0x3199, 0xf000 },
	{ 0x319a, 0x0000 },
	{ 0x319b, 0xf000 },
	{ 0x319c, 0x0000 },
	{ 0x319d, 0xf000 },
	{ 0x319e, 0x0000 },
	{ 0x319f, 0xf000 },
	{ 0x31a0, 0x0000 },
	{ 0x31a1, 0xf000 },
	{ 0x31a2, 0x0000 },
	{ 0x31a3, 0xf000 },
	{ 0x31a4, 0x0000 },
	{ 0x31a5, 0xf000 },
	{ 0x31a6, 0x0000 },
	{ 0x31a7, 0xf000 },
	{ 0x31a8, 0x0000 },
	{ 0x31a9, 0xf000 },
	{ 0x31aa, 0x0000 },
	{ 0x31ab, 0xf000 },
	{ 0x31ac, 0x0000 },
	{ 0x31ad, 0xf000 },
	{ 0x31ae, 0x0000 },
	{ 0x31af, 0xf000 },
	{ 0x31b0, 0x0000 },
	{ 0x31b1, 0xf000 },
	{ 0x31b2, 0x0000 },
	{ 0x31b3, 0xf000 },
	{ 0x31b4, 0x0000 },
	{ 0x31b5, 0xf000 },
	{ 0x31b6, 0x0000 },
	{ 0x31b7, 0xf000 },
	{ 0x31b8, 0x0000 },
	{ 0x31b9, 0xf000 },
	{ 0x31ba, 0x0000 },
	{ 0x31bb, 0xf000 },
	{ 0x31bc, 0x0000 },
	{ 0x31bd, 0xf000 },
	{ 0x31be, 0x0000 },
	{ 0x31bf, 0xf000 },
	{ 0x31c0, 0x0000 },
	{ 0x31c1, 0xf000 },
	{ 0x31c2, 0x0000 },
	{ 0x31c3, 0xf000 },
	{ 0x31c4, 0x0000 },
	{ 0x31c5, 0xf000 },
	{ 0x31c6, 0x0000 },
	{ 0x31c7, 0xf000 },
	{ 0x31c8, 0x0000 },
	{ 0x31c9, 0xf000 },
	{ 0x31ca, 0x0000 },
	{ 0x31cb, 0xf000 },
	{ 0x31cc, 0x0000 },
	{ 0x31cd, 0xf000 },
	{ 0x31ce, 0x0000 },
	{ 0x31cf, 0xf000 },
	{ 0x31d0, 0x0000 },
	{ 0x31d1, 0xf000 },
	{ 0x31d2, 0x0000 },
	{ 0x31d3, 0xf000 },
	{ 0x31d4, 0x0000 },
	{ 0x31d5, 0xf000 },
	{ 0x31d6, 0x0000 },
	{ 0x31d7, 0xf000 },
	{ 0x31d8, 0x0000 },
	{ 0x31d9, 0xf000 },
	{ 0x31da, 0x0000 },
	{ 0x31db, 0xf000 },
	{ 0x31dc, 0x0000 },
	{ 0x31dd, 0xf000 },
	{ 0x31de, 0x0000 },
	{ 0x31df, 0xf000 },
	{ 0x31e0, 0x0000 },
	{ 0x31e1, 0xf000 },
	{ 0x31e2, 0x0000 },
	{ 0x31e3, 0xf000 },
	{ 0x31e4, 0x0000 },
	{ 0x31e5, 0xf000 },
	{ 0x31e6, 0x0000 },
	{ 0x31e7, 0xf000 },
	{ 0x31e8, 0x0000 },
	{ 0x31e9, 0xf000 },
	{ 0x31ea, 0x0000 },
	{ 0x31eb, 0xf000 },
	{ 0x31ec, 0x0000 },
	{ 0x31ed, 0xf000 },
	{ 0x31ee, 0x0000 },
	{ 0x31ef, 0xf000 },
	{ 0x31f0, 0x0000 },
	{ 0x31f1, 0xf000 },
	{ 0x31f2, 0x0000 },
	{ 0x31f3, 0xf000 },
	{ 0x31f4, 0x0000 },
	{ 0x31f5, 0xf000 },
	{ 0x31f6, 0x0000 },
	{ 0x31f7, 0xf000 },
	{ 0x31f8, 0x0000 },
	{ 0x31f9, 0xf000 },
	{ 0x31fa, 0x0000 },
	{ 0x31fb, 0xf000 },
	{ 0x31fc, 0x0000 },
	{ 0x31fd, 0xf000 },
	{ 0x31fe, 0x0000 },
	{ 0x31ff, 0xf000 },
	{ 0x024d, 0xff50 },
	{ 0x0252, 0xff50 },
	{ 0x0259, 0x0112 },
	{ 0x025e, 0x0112 },
};

static const char *wm5102_aec_loopback_texts[] = {
	"HPOUT1L", "HPOUT1R", "HPOUT2L", "HPOUT2R", "EPOUT",
	"SPKOUTL", "SPKOUTR", "SPKDAT1L", "SPKDAT1R",
};

static const unsigned int wm5102_aec_loopback_values[] = {
	0, 1, 2, 3, 4, 6, 7, 8, 9,
};

static const struct soc_enum wm5102_aec_loopback =
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DAC_AEC_CONTROL_1,
			      ARIZONA_AEC_LOOPBACK_SRC_SHIFT,
			      0xf,
			      ARRAY_SIZE(wm5102_aec_loopback_texts),
			      wm5102_aec_loopback_texts,
			      wm5102_aec_loopback_values);


static int wm5102_sysclk_ev(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona *arizona = dev_get_drvdata(codec->dev);
	struct regmap *regmap = codec->control_data;
	const struct reg_default *patch = NULL;
	int i, patch_size;

	switch (arizona->rev) {
	case 0:
		patch = wm5102_sysclk_reva_patch;
		patch_size = ARRAY_SIZE(wm5102_sysclk_reva_patch);
		break;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (patch)
			for (i = 0; i < patch_size; i++)
				regmap_write(regmap, patch[i].reg,
					     patch[i].def);
		break;

	default:
		break;
	}

	return 0;
}

int wm5102_dsp_set_mode( struct snd_soc_codec *codec)
{
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);
	struct regmap *regmap = codec->control_data;
	unsigned int val;

	printk("WM5102 DSP Set mode (MBC %d, VSS %d, AEC %d ,wm5102->txnr_ena %d)\n",
		wm5102->mbc_ena, wm5102->vss_ena, wm5102->aec_ena,wm5102->txnr_ena);

	/* Only for VSS/MBC */
	if (wm5102->core.adsp[0].fw == 0) {
        /* Tune down MBC ramp time */
        regmap_write(regmap, 0x18003e, 0);
        regmap_write(regmap, 0x18003f, 6990 * 20);
        regmap_write(regmap, 0x18003c, 0x0010);
        regmap_write(regmap, 0x18003d, 0x0000);
		regmap_read( regmap, 0x190059, &val);
		if (wm5102->mbc_ena)
		{

			if (!(val & 0x0001))
			{
				regmap_update_bits( regmap, 0x190058, 0x0001, 0x0000);
				regmap_update_bits( regmap, 0x190059, 0x0001, 0x0001);
			}
		}else	{
			if ((val & 0x0001))
			{
				regmap_update_bits( regmap, 0x190058, 0x0001, 0x0000);
				regmap_update_bits( regmap, 0x190059, 0x0001, 0x0000);
			}
		}
		regmap_read( regmap, 0x19005d, &val);
		if (wm5102->vss_ena )
		{
			if (!(val & 0x0001))
			{
				regmap_update_bits( regmap, 0x19005c, 0x0001, 0x0000);
				regmap_update_bits( regmap, 0x19005d, 0x0001, 0x0001);
			}
		}else	{
			if ((val & 0x0001))
			{
				regmap_update_bits( regmap, 0x19005c, 0x0001, 0x0000);
				regmap_update_bits( regmap, 0x19005d, 0x0001, 0x0000);
			}
		}
	}

	if (wm5102->core.adsp[0].fw != 0) {
		if (wm5102->aec_ena) {
			regmap_update_bits(regmap, 0x190211, 1, 1);
			regmap_update_bits(regmap, 0x190597, 1, 1);	
		} else {
			regmap_update_bits(regmap, 0x190211, 1, 0);
			regmap_update_bits(regmap, 0x190597, 1, 0);	
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(wm5102_dsp_set_mode);


int wm5102_dsp_set_txnr_mode( struct snd_soc_codec *codec)
{
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);
	struct regmap *regmap = codec->control_data;
	unsigned int val;

	printk("WM5102 DSP Set mode (wm5102->txnr_ena %d)\n",wm5102->txnr_ena);

	/* Only for AEC TXNR*/
	if (wm5102->core.adsp[0].fw != 0) {
        if (wm5102->txnr_ena) {
            regmap_write(regmap, 0x19024C, 0x00FE); // TXNR_NR_NOISE_RDCTN_MINGAIN_1(19024CH): 00FF  TXNR_NR_NOISE_RDCTN_MINGAIN=1111_1111
            regmap_write(regmap, 0x19024D, 0x7000); // TXNR_NR_NOISE_RDCTN_MINGAIN_2(19024DH): 6000  TXNR_NR_NOISE_RDCTN_MINGAIN=0110_0000_0000_0000
            regmap_write(regmap, 0x190362, 0x0010); // TXNR_NR_ECHO_MIN_GAIN_BOOST_1(190362H): 0028  TXNR_NR_ECHO_MIN_GAIN_BOOST=0010_1000
            regmap_write(regmap, 0x190363, 0x0000); // TXNR_NR_ECHO_MIN_GAIN_BOOST_2(190363H): 0000  TXNR_NR_ECHO_MIN_GAIN_BOOST=0000_0000_0000_0000
        } else {
            regmap_write(regmap, 0x19024C, 0x00FF); // TXNR_NR_NOISE_RDCTN_MINGAIN_1(19024CH): 00FF  TXNR_NR_NOISE_RDCTN_MINGAIN=1111_1111
            regmap_write(regmap, 0x19024D, 0x6000); // TXNR_NR_NOISE_RDCTN_MINGAIN_2(19024DH): 6000  TXNR_NR_NOISE_RDCTN_MINGAIN=0110_0000_0000_0000
            regmap_write(regmap, 0x190362, 0x0028); // TXNR_NR_ECHO_MIN_GAIN_BOOST_1(190362H): 0028  TXNR_NR_ECHO_MIN_GAIN_BOOST=0010_1000
            regmap_write(regmap, 0x190363, 0x0000); // TXNR_NR_ECHO_MIN_GAIN_BOOST_2(190363H): 0000  TXNR_NR_ECHO_MIN_GAIN_BOOST=0000_0000_0000_0000
        }
	}

	return 0;
}
EXPORT_SYMBOL_GPL(wm5102_dsp_set_txnr_mode);

static int wm5102_mbc_mode_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);

	ucontrol->value.enumerated.item[0] = wm5102->mbc_ena;

	return 0;
}

static int wm5102_mbc_mode_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);
	int val = ucontrol->value.enumerated.item[0];

	if (val > 1)
		return -EINVAL;

	wm5102->mbc_ena = val;
	return wm5102_dsp_set_mode( codec);
}

static int wm5102_vss_mode_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);

	ucontrol->value.enumerated.item[0] = wm5102->vss_ena;

	return 0;
}

static int wm5102_vss_mode_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);
	int val = ucontrol->value.enumerated.item[0];

	if (val > 1)
		return -EINVAL;

	wm5102->vss_ena = val;

	return wm5102_dsp_set_mode( codec);
}

static int wm5102_aec_mode_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);

	printk("wm5102_aec_get\n");
	ucontrol->value.enumerated.item[0] = wm5102->aec_ena;

	return 0;
}

static int wm5102_aec_mode_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);
	int val = ucontrol->value.enumerated.item[0];

	if (val > 1)
		return -EINVAL;

	wm5102->aec_ena = val;
	return wm5102_dsp_set_mode( codec);
}

static int wm5102_txnr_mode_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);

	ucontrol->value.enumerated.item[0] = wm5102->txnr_ena;

	return 0;
}

static int wm5102_txnr_mode_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);
	int val = ucontrol->value.enumerated.item[0];

	if (val > 1)
		return -EINVAL;

	wm5102->txnr_ena = val;

    printk("wm5102_txnr_mode_put\n");
    return wm5102_dsp_set_txnr_mode( codec);
}

static int wm5102_txnr_init_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);

	ucontrol->value.enumerated.item[0] = wm5102->txnr_ena;

	return 0;
}

static int wm5102_txnr_init_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm5102_priv *wm5102 = dev_get_drvdata(codec->dev);
	int val = wm5102->txnr_ena;

	if (val > 1)
		return -EINVAL;

    printk("wm5102_txnr_init_put\n");
    return wm5102_dsp_set_txnr_mode( codec);
}
static const struct snd_kcontrol_new wm5102_snd_controls[] = {
SOC_SINGLE_BOOL_EXT("MBC Switch", 0, wm5102_mbc_mode_get, wm5102_mbc_mode_put),
SOC_SINGLE_BOOL_EXT("VSS Switch", 0, wm5102_vss_mode_get, wm5102_vss_mode_put),
SOC_SINGLE_BOOL_EXT("AEC Switch", 0, wm5102_aec_mode_get, wm5102_aec_mode_put),
SOC_SINGLE_BOOL_EXT("TXNR Switch", 0, wm5102_txnr_mode_get, wm5102_txnr_mode_put),
SOC_SINGLE_BOOL_EXT("TXNR Init Setting", 0, wm5102_txnr_init_get, wm5102_txnr_init_put),

SOC_SINGLE("IN1 High Performance Switch", ARIZONA_IN1L_CONTROL,
	   ARIZONA_IN1_OSR_SHIFT, 1, 0),
SOC_SINGLE("IN2 High Performance Switch", ARIZONA_IN2L_CONTROL,
	   ARIZONA_IN2_OSR_SHIFT, 1, 0),
SOC_SINGLE("IN3 High Performance Switch", ARIZONA_IN3L_CONTROL,
	   ARIZONA_IN3_OSR_SHIFT, 1, 0),

SOC_DOUBLE_R_RANGE_TLV("IN1 Volume", ARIZONA_IN1L_CONTROL,
		       ARIZONA_IN1R_CONTROL,
		       ARIZONA_IN1L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_DOUBLE_R_RANGE_TLV("IN2 Volume", ARIZONA_IN2L_CONTROL,
		       ARIZONA_IN2R_CONTROL,
		       ARIZONA_IN2L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN3L Volume", ARIZONA_IN3L_CONTROL,
		       ARIZONA_IN3L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN3R Volume", ARIZONA_IN3R_CONTROL,
		       ARIZONA_IN3R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),

SOC_DOUBLE_R_TLV("IN1 Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_1L,
		 ARIZONA_ADC_DIGITAL_VOLUME_1R, ARIZONA_IN1L_DIG_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("IN2 Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_2L,
		 ARIZONA_ADC_DIGITAL_VOLUME_2R, ARIZONA_IN2L_DIG_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("IN3 Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_3L,
		 ARIZONA_ADC_DIGITAL_VOLUME_3R, ARIZONA_IN3L_DIG_VOL_SHIFT,
		 0xbf, 0, digital_tlv),

SOC_ENUM("Input Ramp Up", arizona_in_vi_ramp),
SOC_ENUM("Input Ramp Down", arizona_in_vd_ramp),

ARIZONA_MIXER_CONTROLS("EQ1", ARIZONA_EQ1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EQ2", ARIZONA_EQ2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EQ3", ARIZONA_EQ3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EQ4", ARIZONA_EQ4MIX_INPUT_1_SOURCE),

SND_SOC_BYTES_MASK("EQ1 Coefficeints", ARIZONA_EQ1_1, 21,
		   ARIZONA_EQ1_ENA_MASK),
SND_SOC_BYTES_MASK("EQ2 Coefficeints", ARIZONA_EQ2_1, 21,
		   ARIZONA_EQ2_ENA_MASK),
SND_SOC_BYTES_MASK("EQ3 Coefficeints", ARIZONA_EQ3_1, 21,
		   ARIZONA_EQ3_ENA_MASK),
SND_SOC_BYTES_MASK("EQ4 Coefficeints", ARIZONA_EQ4_1, 21,
		   ARIZONA_EQ4_ENA_MASK),

SOC_SINGLE_TLV("EQ1 B1 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B2 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B3 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B4 Volume", ARIZONA_EQ1_2, ARIZONA_EQ1_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B5 Volume", ARIZONA_EQ1_2, ARIZONA_EQ1_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

SOC_SINGLE_TLV("EQ2 B1 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B2 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B3 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B4 Volume", ARIZONA_EQ2_2, ARIZONA_EQ2_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B5 Volume", ARIZONA_EQ2_2, ARIZONA_EQ2_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

SOC_SINGLE_TLV("EQ3 B1 Volume", ARIZONA_EQ3_1, ARIZONA_EQ3_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B2 Volume", ARIZONA_EQ3_1, ARIZONA_EQ3_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B3 Volume", ARIZONA_EQ3_1, ARIZONA_EQ3_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B4 Volume", ARIZONA_EQ3_2, ARIZONA_EQ3_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B5 Volume", ARIZONA_EQ3_2, ARIZONA_EQ3_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

SOC_SINGLE_TLV("EQ4 B1 Volume", ARIZONA_EQ4_1, ARIZONA_EQ4_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B2 Volume", ARIZONA_EQ4_1, ARIZONA_EQ4_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B3 Volume", ARIZONA_EQ4_1, ARIZONA_EQ4_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B4 Volume", ARIZONA_EQ4_2, ARIZONA_EQ4_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B5 Volume", ARIZONA_EQ4_2, ARIZONA_EQ4_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

SOC_SINGLE_TLV("EQ1 Ctrl_1", ARIZONA_EQ1_1, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_2", ARIZONA_EQ1_2, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_3", ARIZONA_EQ1_3, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_4", ARIZONA_EQ1_4, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_5", ARIZONA_EQ1_5, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_6", ARIZONA_EQ1_6, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_7", ARIZONA_EQ1_7, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_8", ARIZONA_EQ1_8, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_9", ARIZONA_EQ1_9, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_10", ARIZONA_EQ1_10, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_11", ARIZONA_EQ1_11, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_12", ARIZONA_EQ1_12, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_13", ARIZONA_EQ1_13, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_14", ARIZONA_EQ1_14, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_15", ARIZONA_EQ1_15, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_16", ARIZONA_EQ1_16, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_17", ARIZONA_EQ1_17, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_18", ARIZONA_EQ1_18, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_19", ARIZONA_EQ1_19, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_20", ARIZONA_EQ1_20, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ1 Ctrl_21", ARIZONA_EQ1_21, 0, 65535, 0, linear_tlv),

SOC_SINGLE_TLV("EQ2 Ctrl_1", ARIZONA_EQ2_1, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_2", ARIZONA_EQ2_2, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_3", ARIZONA_EQ2_3, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_4", ARIZONA_EQ2_4, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_5", ARIZONA_EQ2_5, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_6", ARIZONA_EQ2_6, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_7", ARIZONA_EQ2_7, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_8", ARIZONA_EQ2_8, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_9", ARIZONA_EQ2_9, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_10", ARIZONA_EQ2_10, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_11", ARIZONA_EQ2_11, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_12", ARIZONA_EQ2_12, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_13", ARIZONA_EQ2_13, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_14", ARIZONA_EQ2_14, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_15", ARIZONA_EQ2_15, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_16", ARIZONA_EQ2_16, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_17", ARIZONA_EQ2_17, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_18", ARIZONA_EQ2_18, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_19", ARIZONA_EQ2_19, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_20", ARIZONA_EQ2_20, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ2 Ctrl_21", ARIZONA_EQ2_21, 0, 65535, 0, linear_tlv),

SOC_SINGLE_TLV("EQ3 Ctrl_1", ARIZONA_EQ3_1, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_2", ARIZONA_EQ3_2, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_3", ARIZONA_EQ3_3, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_4", ARIZONA_EQ3_4, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_5", ARIZONA_EQ3_5, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_6", ARIZONA_EQ3_6, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_7", ARIZONA_EQ3_7, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_8", ARIZONA_EQ3_8, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_9", ARIZONA_EQ3_9, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_10", ARIZONA_EQ3_10, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_11", ARIZONA_EQ3_11, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_12", ARIZONA_EQ3_12, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_13", ARIZONA_EQ3_13, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_14", ARIZONA_EQ3_14, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_15", ARIZONA_EQ3_15, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_16", ARIZONA_EQ3_16, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_17", ARIZONA_EQ3_17, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_18", ARIZONA_EQ3_18, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_19", ARIZONA_EQ3_19, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_20", ARIZONA_EQ3_20, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ3 Ctrl_21", ARIZONA_EQ3_21, 0, 65535, 0, linear_tlv),

SOC_SINGLE_TLV("EQ4 Ctrl_1", ARIZONA_EQ4_1, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_2", ARIZONA_EQ4_2, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_3", ARIZONA_EQ4_3, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_4", ARIZONA_EQ4_4, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_5", ARIZONA_EQ4_5, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_6", ARIZONA_EQ4_6, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_7", ARIZONA_EQ4_7, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_8", ARIZONA_EQ4_8, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_9", ARIZONA_EQ4_9, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_10", ARIZONA_EQ4_10, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_11", ARIZONA_EQ4_11, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_12", ARIZONA_EQ4_12, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_13", ARIZONA_EQ4_13, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_14", ARIZONA_EQ4_14, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_15", ARIZONA_EQ4_15, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_16", ARIZONA_EQ4_16, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_17", ARIZONA_EQ4_17, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_18", ARIZONA_EQ4_18, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_19", ARIZONA_EQ4_19, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_20", ARIZONA_EQ4_20, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("EQ4 Ctrl_21", ARIZONA_EQ4_21, 0, 65535, 0, linear_tlv),

ARIZONA_MIXER_CONTROLS("DRC1L", ARIZONA_DRC1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DRC1R", ARIZONA_DRC1RMIX_INPUT_1_SOURCE),

SND_SOC_BYTES_MASK("DRC1", ARIZONA_DRC1_CTRL1, 5,
		   ARIZONA_DRC1R_ENA | ARIZONA_DRC1L_ENA),

ARIZONA_MIXER_CONTROLS("DRC2L", ARIZONA_DRC2LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DRC2R", ARIZONA_DRC2RMIX_INPUT_1_SOURCE),

SND_SOC_BYTES_MASK("DRC2", ARIZONA_DRC2_CTRL1, 5,
		   ARIZONA_DRC2R_ENA | ARIZONA_DRC2L_ENA),

SOC_SINGLE_TLV("DRC1 Ctrl_1", ARIZONA_DRC1_CTRL1, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("DRC1 Ctrl_2", ARIZONA_DRC1_CTRL2, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("DRC1 Ctrl_3", ARIZONA_DRC1_CTRL3, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("DRC1 Ctrl_4", ARIZONA_DRC1_CTRL4, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("DRC1 Ctrl_5", ARIZONA_DRC1_CTRL5, 0, 65535, 0, linear_tlv),

SOC_SINGLE_TLV("DRC2 Ctrl_1", ARIZONA_DRC2_CTRL1, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("DRC2 Ctrl_2", ARIZONA_DRC2_CTRL2, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("DRC2 Ctrl_3", ARIZONA_DRC2_CTRL3, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("DRC2 Ctrl_4", ARIZONA_DRC2_CTRL4, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("DRC2 Ctrl_5", ARIZONA_DRC2_CTRL5, 0, 65535, 0, linear_tlv),

ARIZONA_MIXER_CONTROLS("LHPF1", ARIZONA_HPLP1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("LHPF2", ARIZONA_HPLP2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("LHPF3", ARIZONA_HPLP3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("LHPF4", ARIZONA_HPLP4MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("ISRC1FSH", ARIZONA_ISRC_1_CTRL_1),
ARIZONA_MIXER_CONTROLS("ISRC1FSL", ARIZONA_ISRC_1_CTRL_2),
ARIZONA_MIXER_CONTROLS("ISRC2FSH", ARIZONA_ISRC_2_CTRL_1),
ARIZONA_MIXER_CONTROLS("ISRC2FSL", ARIZONA_ISRC_2_CTRL_2),

ARIZONA_MIXER_CONTROLS("DSP1L", ARIZONA_DSP1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP1R", ARIZONA_DSP1RMIX_INPUT_1_SOURCE),

SOC_ENUM("LHPF1 Mode", arizona_lhpf1_mode),
SOC_ENUM("LHPF2 Mode", arizona_lhpf2_mode),
SOC_ENUM("LHPF3 Mode", arizona_lhpf3_mode),
SOC_ENUM("LHPF4 Mode", arizona_lhpf4_mode),

SOC_ENUM("ISRC1FSH Mode", arizona_isrc1fsh_mode),
SOC_ENUM("ISRC1FSL Mode", arizona_isrc1fsl_mode),
SOC_ENUM("ISRC2FSH Mode", arizona_isrc2fsh_mode),
SOC_ENUM("ISRC2FSL Mode", arizona_isrc2fsl_mode),

SOC_ENUM("DSP1 Mode", arizona_dsp1_mode),
SOC_ENUM("OUT Mode", arizona_out_mode),
SOC_ENUM("FX Mode", arizona_fx_mode),

SOC_SINGLE_TLV("LHPF1 Coeff", ARIZONA_HPLPF1_2, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("LHPF2 Coeff", ARIZONA_HPLPF2_2, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("LHPF3 Coeff", ARIZONA_HPLPF3_2, 0, 65535, 0, linear_tlv),
SOC_SINGLE_TLV("LHPF4 Coeff", ARIZONA_HPLPF4_2, 0, 65535, 0, linear_tlv),

ARIZONA_MIXER_CONTROLS("Mic", ARIZONA_MICMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("Noise", ARIZONA_NOISEMIX_INPUT_1_SOURCE),

SOC_SINGLE_TLV("Noise Generator Volume", ARIZONA_COMFORT_NOISE_GENERATOR,
	       ARIZONA_NOISE_GEN_GAIN_SHIFT, 0x16, 0, noise_tlv),

ARIZONA_MIXER_CONTROLS("HPOUT1L", ARIZONA_OUT1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT1R", ARIZONA_OUT1RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT2L", ARIZONA_OUT2LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT2R", ARIZONA_OUT2RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EPOUT", ARIZONA_OUT3LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKOUTL", ARIZONA_OUT4LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKOUTR", ARIZONA_OUT4RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKDAT1L", ARIZONA_OUT5LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKDAT1R", ARIZONA_OUT5RMIX_INPUT_1_SOURCE),

SOC_SINGLE("SPKDAT1 High Performance Switch", ARIZONA_OUTPUT_PATH_CONFIG_5L,
	   ARIZONA_OUT5_OSR_SHIFT, 1, 0),

SOC_SINGLE("HPOUT1 Mono Switch", ARIZONA_OUTPUT_PATH_CONFIG_1L,
	   ARIZONA_OUT1_MONO_SHIFT, 1, 0),

SOC_SINGLE("HPOUT2 Mono Switch", ARIZONA_OUTPUT_PATH_CONFIG_2L,
	   ARIZONA_OUT2_MONO_SHIFT, 1, 0),

SOC_DOUBLE_R_RANGE_TLV("HPOUT1 Volume", ARIZONA_OUTPUT_PATH_CONFIG_1L,
		       ARIZONA_OUTPUT_PATH_CONFIG_1R,
		       ARIZONA_OUT1L_PGA_VOL_SHIFT,
		       0x34, 0x40, 0, ana_tlv),
SOC_DOUBLE_R_RANGE_TLV("HPOUT2 Volume", ARIZONA_OUTPUT_PATH_CONFIG_2L,
		       ARIZONA_OUTPUT_PATH_CONFIG_2R,
		       ARIZONA_OUT2L_PGA_VOL_SHIFT,
		       0x34, 0x40, 0, ana_tlv),

SOC_DOUBLE_R("HPOUT1 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_1L,
	     ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("HPOUT2 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_2L,
	     ARIZONA_DAC_DIGITAL_VOLUME_2R, ARIZONA_OUT2L_MUTE_SHIFT, 1, 1),
SOC_SINGLE("EPOUT Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_3L,
	   ARIZONA_OUT3L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("Speaker Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_4L,
	     ARIZONA_DAC_DIGITAL_VOLUME_4R, ARIZONA_OUT4L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("SPKDAT1 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_5L,
	     ARIZONA_DAC_DIGITAL_VOLUME_5R, ARIZONA_OUT5L_MUTE_SHIFT, 1, 1),

SOC_DOUBLE_R_TLV("HPOUT1 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_1L,
		 ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("HPOUT2 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_2L,
		 ARIZONA_DAC_DIGITAL_VOLUME_2R, ARIZONA_OUT2L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("EPOUT Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_3L,
	       ARIZONA_OUT3L_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("Speaker Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_4L,
		 ARIZONA_DAC_DIGITAL_VOLUME_4R, ARIZONA_OUT4L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("SPKDAT1 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_5L,
		 ARIZONA_DAC_DIGITAL_VOLUME_5R, ARIZONA_OUT5L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),

SOC_SINGLE_RANGE_TLV("EPOUT Volume", ARIZONA_OUTPUT_PATH_CONFIG_3L,
		     ARIZONA_OUT3L_PGA_VOL_SHIFT, 0x34, 0x40, 0, ana_tlv),
SOC_ENUM("Output Ramp Up", arizona_out_vi_ramp),
SOC_ENUM("Output Ramp Down", arizona_out_vd_ramp),

SOC_DOUBLE("SPKDAT1 Switch", ARIZONA_PDM_SPK1_CTRL_1, ARIZONA_SPK1L_MUTE_SHIFT,
	   ARIZONA_SPK1R_MUTE_SHIFT, 1, 1),

SOC_ENUM("AEC Loopback", wm5102_aec_loopback),

ARIZONA_MIXER_CONTROLS("AIF1TX1", ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX2", ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX3", ARIZONA_AIF1TX3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX4", ARIZONA_AIF1TX4MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX5", ARIZONA_AIF1TX5MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX6", ARIZONA_AIF1TX6MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX7", ARIZONA_AIF1TX7MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX8", ARIZONA_AIF1TX8MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF2TX1", ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX2", ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF3TX1", ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF3TX2", ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE),
};

static int wm5102_spk_ev(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol,
			 int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	struct wm5102_priv *wm5102 = snd_soc_codec_get_drvdata(codec);

	if (arizona->rev < 1)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!wm5102->spk_ena) {
			snd_soc_write(codec, 0x4f5, 0x25a);
			wm5102->spk_ena_pending = true;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (wm5102->spk_ena_pending) {
			msleep(75);
			snd_soc_write(codec, 0x4f5, 0xda);
			wm5102->spk_ena_pending = false;
			wm5102->spk_ena++;
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		wm5102->spk_ena--;
		if (!wm5102->spk_ena)
			snd_soc_write(codec, 0x4f5, 0x25a);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!wm5102->spk_ena)
			snd_soc_write(codec, 0x4f5, 0x0da);
		break;
	}

	return 0;
}


ARIZONA_MIXER_ENUMS(EQ1, ARIZONA_EQ1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(EQ2, ARIZONA_EQ2MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(EQ3, ARIZONA_EQ3MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(EQ4, ARIZONA_EQ4MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(DRC1L, ARIZONA_DRC1LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(DRC1R, ARIZONA_DRC1RMIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(DRC2L, ARIZONA_DRC2LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(DRC2R, ARIZONA_DRC2RMIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(LHPF1, ARIZONA_HPLP1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(LHPF2, ARIZONA_HPLP2MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(LHPF3, ARIZONA_HPLP3MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(LHPF4, ARIZONA_HPLP4MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(Mic, ARIZONA_MICMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(Noise, ARIZONA_NOISEMIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(PWM1, ARIZONA_PWM1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(PWM2, ARIZONA_PWM2MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(OUT1L, ARIZONA_OUT1LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(OUT1R, ARIZONA_OUT1RMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(OUT2L, ARIZONA_OUT2LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(OUT2R, ARIZONA_OUT2RMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(OUT3, ARIZONA_OUT3LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SPKOUTL, ARIZONA_OUT4LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SPKOUTR, ARIZONA_OUT4RMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SPKDAT1L, ARIZONA_OUT5LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SPKDAT1R, ARIZONA_OUT5RMIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(AIF1TX1, ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX2, ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX3, ARIZONA_AIF1TX3MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX4, ARIZONA_AIF1TX4MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX5, ARIZONA_AIF1TX5MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX6, ARIZONA_AIF1TX6MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX7, ARIZONA_AIF1TX7MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX8, ARIZONA_AIF1TX8MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(AIF2TX1, ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF2TX2, ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(AIF3TX1, ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF3TX2, ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ASRC1L, ARIZONA_ASRC1LMIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ASRC1R, ARIZONA_ASRC1RMIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ASRC2L, ARIZONA_ASRC2LMIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ASRC2R, ARIZONA_ASRC2RMIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC1INT1, ARIZONA_ISRC1INT1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1INT2, ARIZONA_ISRC1INT2MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1INT3, ARIZONA_ISRC1INT3MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1INT4, ARIZONA_ISRC1INT4MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC1DEC1, ARIZONA_ISRC1DEC1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1DEC2, ARIZONA_ISRC1DEC2MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1DEC3, ARIZONA_ISRC1DEC3MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1DEC4, ARIZONA_ISRC1DEC4MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC1FSH, ARIZONA_ISRC_1_CTRL_1);
ARIZONA_MUX_ENUMS(ISRC1FSL, ARIZONA_ISRC_1_CTRL_2);
ARIZONA_MUX_ENUMS(ISRC2FSH, ARIZONA_ISRC_2_CTRL_1);
ARIZONA_MUX_ENUMS(ISRC2FSL, ARIZONA_ISRC_2_CTRL_2);

ARIZONA_MUX_ENUMS(ISRC2INT1, ARIZONA_ISRC2INT1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC2INT2, ARIZONA_ISRC2INT2MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC2INT3, ARIZONA_ISRC2INT3MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC2INT4, ARIZONA_ISRC2INT4MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC2DEC1, ARIZONA_ISRC2DEC1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC2DEC2, ARIZONA_ISRC2DEC2MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC2DEC3, ARIZONA_ISRC2DEC3MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC2DEC4, ARIZONA_ISRC2DEC4MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(DSP1L, ARIZONA_DSP1LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(DSP1R, ARIZONA_DSP1RMIX_INPUT_1_SOURCE);

ARIZONA_DSP_AUX_ENUMS(DSP1, ARIZONA_DSP1AUX1MIX_INPUT_1_SOURCE);

static void speaker_pa_gpio_request(void)
{
    int ret;

    gpio_free(SPEAKER_PA_GPIO);
    ret = gpio_request(SPEAKER_PA_GPIO, "speaker pa gpio");
    if (ret) {
        printk("enter %s, in line %d\n", __func__, __LINE__);
    }
}

static int speaker_pa_power_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{
//    struct snd_soc_codec *codec = w->codec;

	if (SND_SOC_DAPM_EVENT_ON(event)) {

        gpio_direction_output(SPEAKER_PA_GPIO, 1);
        msleep(5);
        printk( "enter: %s, enable speaker pa\n", __func__);
    }

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
        gpio_direction_output(SPEAKER_PA_GPIO, 0);
        msleep(50);
        printk( "enter: %s, disable speaker pa\n", __func__);
    }
    return 0;
}

static const struct snd_soc_dapm_widget wm5102_dapm_widgets[] = {
SND_SOC_DAPM_SUPPLY("SYSCLK", ARIZONA_SYSTEM_CLOCK_1, ARIZONA_SYSCLK_ENA_SHIFT,
		    0, wm5102_sysclk_ev, SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_SUPPLY("ASYNCCLK", ARIZONA_ASYNC_CLOCK_1,
		    ARIZONA_ASYNC_CLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("OPCLK", ARIZONA_OUTPUT_SYSTEM_CLOCK,
		    ARIZONA_OPCLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("ASYNCOPCLK", ARIZONA_OUTPUT_ASYNC_CLOCK,
		    ARIZONA_OPCLK_ASYNC_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_SIGGEN("TONE"),
SND_SOC_DAPM_SIGGEN("NOISE"),
SND_SOC_DAPM_SIGGEN("HAPTICS"),

SND_SOC_DAPM_INPUT("IN1L"),
SND_SOC_DAPM_INPUT("IN1R"),
SND_SOC_DAPM_INPUT("IN2L"),
SND_SOC_DAPM_INPUT("IN2R"),
SND_SOC_DAPM_INPUT("IN3L"),
SND_SOC_DAPM_INPUT("IN3R"),

SND_SOC_DAPM_PGA_E("IN1L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN1L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN1R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN1R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN2L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN2L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN2R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN2R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN3L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN3L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN3R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN3R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_SUPPLY("MICBIAS1", SND_SOC_NOPM,
		    ARIZONA_MICB1_ENA_SHIFT, 0, micbias_ev,
		    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
SND_SOC_DAPM_SUPPLY("MICBIAS2", ARIZONA_MIC_BIAS_CTRL_2,
		    ARIZONA_MICB2_ENA_SHIFT, 0, micbias_ev,
		    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
SND_SOC_DAPM_SUPPLY("MICBIAS3", ARIZONA_MIC_BIAS_CTRL_3,
		    ARIZONA_MICB3_ENA_SHIFT, 0, micbias_ev,
		    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

SND_SOC_DAPM_PGA("Noise Generator", ARIZONA_COMFORT_NOISE_GENERATOR,
		 ARIZONA_NOISE_GEN_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("Tone Generator 1", ARIZONA_TONE_GENERATOR_1,
		 ARIZONA_TONE1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("Tone Generator 2", ARIZONA_TONE_GENERATOR_1,
		 ARIZONA_TONE2_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("Mic Mute Mixer", ARIZONA_MIC_NOISE_MIX_CONTROL_1,
		 ARIZONA_MICMUTE_MIX_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("EQ1", ARIZONA_EQ1_1, ARIZONA_EQ1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ2", ARIZONA_EQ2_1, ARIZONA_EQ2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ3", ARIZONA_EQ3_1, ARIZONA_EQ3_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ4", ARIZONA_EQ4_1, ARIZONA_EQ4_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("DRC1L", ARIZONA_DRC1_CTRL1, ARIZONA_DRC1L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("DRC1R", ARIZONA_DRC1_CTRL1, ARIZONA_DRC1R_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("DRC2L", ARIZONA_DRC2_CTRL1, ARIZONA_DRC2L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("DRC2R", ARIZONA_DRC2_CTRL1, ARIZONA_DRC2R_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("LHPF1", ARIZONA_HPLPF1_1, ARIZONA_LHPF1_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF2", ARIZONA_HPLPF2_1, ARIZONA_LHPF2_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF3", ARIZONA_HPLPF3_1, ARIZONA_LHPF3_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF4", ARIZONA_HPLPF4_1, ARIZONA_LHPF4_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("PWM1 Driver", ARIZONA_PWM_DRIVE_1, ARIZONA_PWM1_ENA_SHIFT,
		 0, NULL, 0),
SND_SOC_DAPM_PGA("PWM2 Driver", ARIZONA_PWM_DRIVE_1, ARIZONA_PWM2_ENA_SHIFT,
		 0, NULL, 0),

SND_SOC_DAPM_PGA("ASRC1L", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC1L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("ASRC1R", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC1R_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("ASRC2L", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC2L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("ASRC2R", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC2R_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("ISRC1INT1", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT2", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT3", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT4", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC1DEC1", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC2", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC3", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC4", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC2INT1", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT2", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT3", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT4", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC2DEC1", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC2", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC3", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC4", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_AIF_OUT("AIF1TX1", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX2", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX3", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX4", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX5", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX6", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX7", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX8", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF1RX1", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX2", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX3", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX4", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX5", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX6", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX7", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX8", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF2TX1", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX2", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF2RX1", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX2", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF3TX1", NULL, 0,
		     ARIZONA_AIF3_TX_ENABLES, ARIZONA_AIF3TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF3TX2", NULL, 0,
		     ARIZONA_AIF3_TX_ENABLES, ARIZONA_AIF3TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF3RX1", NULL, 0,
		    ARIZONA_AIF3_RX_ENABLES, ARIZONA_AIF3RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF3RX2", NULL, 0,
		    ARIZONA_AIF3_RX_ENABLES, ARIZONA_AIF3RX2_ENA_SHIFT, 0),

ARIZONA_DSP_WIDGETS(DSP1, "DSP1"),

/* BODGE: v3.4 DAPM is giving us trouble */
SND_SOC_DAPM_PGA("AEC Loopback", SND_SOC_NOPM,
		 ARIZONA_AEC_LOOPBACK_ENA, 0, NULL, 0),

SND_SOC_DAPM_PGA_E("OUT1L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT1L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT1R", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT1R_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT2L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT2L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT2R", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT2R_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT3L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT3L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT4L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT4L_ENA_SHIFT, 0, NULL, 0, wm5102_spk_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT4R", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT4R_ENA_SHIFT, 0, NULL, 0, wm5102_spk_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT5L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT5L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT5R", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT5R_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

ARIZONA_MIXER_WIDGETS(EQ1, "EQ1"),
ARIZONA_MIXER_WIDGETS(EQ2, "EQ2"),
ARIZONA_MIXER_WIDGETS(EQ3, "EQ3"),
ARIZONA_MIXER_WIDGETS(EQ4, "EQ4"),

ARIZONA_MIXER_WIDGETS(DRC1L, "DRC1L"),
ARIZONA_MIXER_WIDGETS(DRC1R, "DRC1R"),

ARIZONA_MIXER_WIDGETS(DRC2L, "DRC2L"),
ARIZONA_MIXER_WIDGETS(DRC2R, "DRC2R"),

ARIZONA_MIXER_WIDGETS(LHPF1, "LHPF1"),
ARIZONA_MIXER_WIDGETS(LHPF2, "LHPF2"),
ARIZONA_MIXER_WIDGETS(LHPF3, "LHPF3"),
ARIZONA_MIXER_WIDGETS(LHPF4, "LHPF4"),

ARIZONA_MIXER_WIDGETS(Mic, "Mic"),
ARIZONA_MIXER_WIDGETS(Noise, "Noise"),

ARIZONA_MIXER_WIDGETS(PWM1, "PWM1"),
ARIZONA_MIXER_WIDGETS(PWM2, "PWM2"),

ARIZONA_MIXER_WIDGETS(OUT1L, "HPOUT1L"),
ARIZONA_MIXER_WIDGETS(OUT1R, "HPOUT1R"),
ARIZONA_MIXER_WIDGETS(OUT2L, "HPOUT2L"),
ARIZONA_MIXER_WIDGETS(OUT2R, "HPOUT2R"),
ARIZONA_MIXER_WIDGETS(OUT3, "EPOUT"),
ARIZONA_MIXER_WIDGETS(SPKOUTL, "SPKOUTL"),
ARIZONA_MIXER_WIDGETS(SPKOUTR, "SPKOUTR"),
ARIZONA_MIXER_WIDGETS(SPKDAT1L, "SPKDAT1L"),
ARIZONA_MIXER_WIDGETS(SPKDAT1R, "SPKDAT1R"),

ARIZONA_MIXER_WIDGETS(AIF1TX1, "AIF1TX1"),
ARIZONA_MIXER_WIDGETS(AIF1TX2, "AIF1TX2"),
ARIZONA_MIXER_WIDGETS(AIF1TX3, "AIF1TX3"),
ARIZONA_MIXER_WIDGETS(AIF1TX4, "AIF1TX4"),
ARIZONA_MIXER_WIDGETS(AIF1TX5, "AIF1TX5"),
ARIZONA_MIXER_WIDGETS(AIF1TX6, "AIF1TX6"),
ARIZONA_MIXER_WIDGETS(AIF1TX7, "AIF1TX7"),
ARIZONA_MIXER_WIDGETS(AIF1TX8, "AIF1TX8"),

ARIZONA_MIXER_WIDGETS(AIF2TX1, "AIF2TX1"),
ARIZONA_MIXER_WIDGETS(AIF2TX2, "AIF2TX2"),

ARIZONA_MIXER_WIDGETS(AIF3TX1, "AIF3TX1"),
ARIZONA_MIXER_WIDGETS(AIF3TX2, "AIF3TX2"),

ARIZONA_MUX_WIDGETS(ASRC1L, "ASRC1L"),
ARIZONA_MUX_WIDGETS(ASRC1R, "ASRC1R"),
ARIZONA_MUX_WIDGETS(ASRC2L, "ASRC2L"),
ARIZONA_MUX_WIDGETS(ASRC2R, "ASRC2R"),

ARIZONA_MUX_WIDGETS(ISRC1DEC1, "ISRC1DEC1"),
ARIZONA_MUX_WIDGETS(ISRC1DEC2, "ISRC1DEC2"),
ARIZONA_MUX_WIDGETS(ISRC1DEC3, "ISRC1DEC3"),
ARIZONA_MUX_WIDGETS(ISRC1DEC4, "ISRC1DEC4"),

ARIZONA_MUX_WIDGETS(ISRC1INT1, "ISRC1INT1"),
ARIZONA_MUX_WIDGETS(ISRC1INT2, "ISRC1INT2"),
ARIZONA_MUX_WIDGETS(ISRC1INT3, "ISRC1INT3"),
ARIZONA_MUX_WIDGETS(ISRC1INT4, "ISRC1INT4"),

ARIZONA_MUX_WIDGETS(ISRC2DEC1, "ISRC2DEC1"),
ARIZONA_MUX_WIDGETS(ISRC2DEC2, "ISRC2DEC2"),
ARIZONA_MUX_WIDGETS(ISRC2DEC3, "ISRC2DEC3"),
ARIZONA_MUX_WIDGETS(ISRC2DEC4, "ISRC2DEC4"),

ARIZONA_MUX_WIDGETS(ISRC2INT1, "ISRC2INT1"),
ARIZONA_MUX_WIDGETS(ISRC2INT2, "ISRC2INT2"),
ARIZONA_MUX_WIDGETS(ISRC2INT3, "ISRC2INT3"),
ARIZONA_MUX_WIDGETS(ISRC2INT4, "ISRC2INT4"),

WM_ADSP2("DSP1", 0),

SND_SOC_DAPM_OUTPUT("HPOUT1L"),
SND_SOC_DAPM_OUTPUT("HPOUT1R"),
SND_SOC_DAPM_OUTPUT("HPOUT2L"),
SND_SOC_DAPM_OUTPUT("HPOUT2R"),
SND_SOC_DAPM_OUTPUT("EPOUTN"),
SND_SOC_DAPM_OUTPUT("EPOUTP"),
SND_SOC_DAPM_OUTPUT("SPKOUTLN"),
SND_SOC_DAPM_OUTPUT("SPKOUTLP"),
SND_SOC_DAPM_OUTPUT("SPKOUTRN"),
SND_SOC_DAPM_OUTPUT("SPKOUTRP"),
SND_SOC_DAPM_OUTPUT("SPKDAT1L"),
SND_SOC_DAPM_OUTPUT("SPKDAT1R"),

//add for speaker PA
SND_SOC_DAPM_INPUT("PA_SPKINL"),
SND_SOC_DAPM_INPUT("PA_SPKINR"),
SND_SOC_DAPM_OUTPUT("PA_SPKOUTN"),
SND_SOC_DAPM_OUTPUT("PA_SPKOUTP"),

SND_SOC_DAPM_PGA_E("PA_Engine", SND_SOC_NOPM, 0, 0, NULL, 0,
	   speaker_pa_power_event,
	   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
//add end
};

#define ARIZONA_MIXER_INPUT_ROUTES(name)	\
	{ name, "Noise Generator", "Noise Generator" }, \
	{ name, "Tone Generator 1", "Tone Generator 1" }, \
	{ name, "Tone Generator 2", "Tone Generator 2" }, \
	{ name, "Haptics", "HAPTICS" }, \
	{ name, "AEC", "AEC Loopback" }, \
	{ name, "IN1L", "IN1L PGA" }, \
	{ name, "IN1R", "IN1R PGA" }, \
	{ name, "IN2L", "IN2L PGA" }, \
	{ name, "IN2R", "IN2R PGA" }, \
	{ name, "IN3L", "IN3L PGA" }, \
	{ name, "IN3R", "IN3R PGA" }, \
	{ name, "Mic Mute Mixer", "Mic Mute Mixer" }, \
	{ name, "AIF1RX1", "AIF1RX1" }, \
	{ name, "AIF1RX2", "AIF1RX2" }, \
	{ name, "AIF1RX3", "AIF1RX3" }, \
	{ name, "AIF1RX4", "AIF1RX4" }, \
	{ name, "AIF1RX5", "AIF1RX5" }, \
	{ name, "AIF1RX6", "AIF1RX6" }, \
	{ name, "AIF1RX7", "AIF1RX7" }, \
	{ name, "AIF1RX8", "AIF1RX8" }, \
	{ name, "AIF2RX1", "AIF2RX1" }, \
	{ name, "AIF2RX2", "AIF2RX2" }, \
	{ name, "AIF3RX1", "AIF3RX1" }, \
	{ name, "AIF3RX2", "AIF3RX2" }, \
	{ name, "EQ1", "EQ1" }, \
	{ name, "EQ2", "EQ2" }, \
	{ name, "EQ3", "EQ3" }, \
	{ name, "EQ4", "EQ4" }, \
	{ name, "DRC1L", "DRC1L" }, \
	{ name, "DRC1R", "DRC1R" }, \
	{ name, "DRC2L", "DRC2L" }, \
	{ name, "DRC2R", "DRC2R" }, \
	{ name, "LHPF1", "LHPF1" }, \
	{ name, "LHPF2", "LHPF2" }, \
	{ name, "LHPF3", "LHPF3" }, \
	{ name, "LHPF4", "LHPF4" }, \
	{ name, "ASRC1L", "ASRC1L" }, \
	{ name, "ASRC1R", "ASRC1R" }, \
	{ name, "ASRC2L", "ASRC2L" }, \
	{ name, "ASRC2R", "ASRC2R" }, \
	{ name, "ISRC1DEC1", "ISRC1DEC1" }, \
	{ name, "ISRC1DEC2", "ISRC1DEC2" }, \
	{ name, "ISRC1DEC3", "ISRC1DEC3" }, \
	{ name, "ISRC1DEC4", "ISRC1DEC4" }, \
	{ name, "ISRC1INT1", "ISRC1INT1" }, \
	{ name, "ISRC1INT2", "ISRC1INT2" }, \
	{ name, "ISRC1INT3", "ISRC1INT3" }, \
	{ name, "ISRC1INT4", "ISRC1INT4" }, \
	{ name, "ISRC2DEC1", "ISRC2DEC1" }, \
	{ name, "ISRC2DEC2", "ISRC2DEC2" }, \
	{ name, "ISRC2DEC3", "ISRC2DEC3" }, \
	{ name, "ISRC2DEC4", "ISRC2DEC4" }, \
	{ name, "ISRC2INT1", "ISRC2INT1" }, \
	{ name, "ISRC2INT2", "ISRC2INT2" }, \
	{ name, "ISRC2INT3", "ISRC2INT3" }, \
	{ name, "ISRC2INT4", "ISRC2INT4" }, \
	{ name, "DSP1.1", "DSP1" }, \
	{ name, "DSP1.2", "DSP1" }, \
	{ name, "DSP1.3", "DSP1" }, \
	{ name, "DSP1.4", "DSP1" }, \
	{ name, "DSP1.5", "DSP1" }, \
	{ name, "DSP1.6", "DSP1" }

static const struct snd_soc_dapm_route wm5102_dapm_routes[] = {
	{ "OUT1L", NULL, "SYSCLK" },
	{ "OUT1R", NULL, "SYSCLK" },
	{ "OUT2L", NULL, "SYSCLK" },
	{ "OUT2R", NULL, "SYSCLK" },
	{ "OUT3L", NULL, "SYSCLK" },
	{ "OUT4L", NULL, "SYSCLK" },
	{ "OUT4R", NULL, "SYSCLK" },
	{ "OUT5L", NULL, "SYSCLK" },
	{ "OUT5R", NULL, "SYSCLK" },

	{ "MICBIAS1", NULL, "SYSCLK" },
	{ "MICBIAS2", NULL, "SYSCLK" },
	{ "MICBIAS3", NULL, "SYSCLK" },

	{ "Noise Generator", NULL, "SYSCLK" },
	{ "Tone Generator 1", NULL, "SYSCLK" },
	{ "Tone Generator 2", NULL, "SYSCLK" },

	{ "Noise Generator", NULL, "NOISE" },
	{ "Tone Generator 1", NULL, "TONE" },
	{ "Tone Generator 2", NULL, "TONE" },

	{ "Mic Mute Mixer", NULL, "Noise Mixer" },
	{ "Mic Mute Mixer", NULL, "Mic Mixer" },

	{ "AIF1 Capture", NULL, "AIF1TX1" },
	{ "AIF1 Capture", NULL, "AIF1TX2" },
	{ "AIF1 Capture", NULL, "AIF1TX3" },
	{ "AIF1 Capture", NULL, "AIF1TX4" },
	{ "AIF1 Capture", NULL, "AIF1TX5" },
	{ "AIF1 Capture", NULL, "AIF1TX6" },
	{ "AIF1 Capture", NULL, "AIF1TX7" },
	{ "AIF1 Capture", NULL, "AIF1TX8" },

	{ "AIF1RX1", NULL, "AIF1 Playback" },
	{ "AIF1RX2", NULL, "AIF1 Playback" },
	{ "AIF1RX3", NULL, "AIF1 Playback" },
	{ "AIF1RX4", NULL, "AIF1 Playback" },
	{ "AIF1RX5", NULL, "AIF1 Playback" },
	{ "AIF1RX6", NULL, "AIF1 Playback" },
	{ "AIF1RX7", NULL, "AIF1 Playback" },
	{ "AIF1RX8", NULL, "AIF1 Playback" },

	{ "AIF2 Capture", NULL, "AIF2TX1" },
	{ "AIF2 Capture", NULL, "AIF2TX2" },

	{ "AIF2RX1", NULL, "AIF2 Playback" },
	{ "AIF2RX2", NULL, "AIF2 Playback" },

	{ "AIF3 Capture", NULL, "AIF3TX1" },
	{ "AIF3 Capture", NULL, "AIF3TX2" },

	{ "AIF3RX1", NULL, "AIF3 Playback" },
	{ "AIF3RX2", NULL, "AIF3 Playback" },

	{ "AIF1 Playback", NULL, "SYSCLK" },
	{ "AIF2 Playback", NULL, "SYSCLK" },
	{ "AIF3 Playback", NULL, "SYSCLK" },

	{ "AIF1 Capture", NULL, "SYSCLK" },
	{ "AIF2 Capture", NULL, "SYSCLK" },
	{ "AIF3 Capture", NULL, "SYSCLK" },

	{ "IN1L PGA", NULL, "IN1L" },
	{ "IN1R PGA", NULL, "IN1R" },

	{ "IN2L PGA", NULL, "IN2L" },
	{ "IN2R PGA", NULL, "IN2R" },

	{ "IN3L PGA", NULL, "IN3L" },
	{ "IN3R PGA", NULL, "IN3R" },

	{ "ASRC1L", NULL, "ASRC1L Input" },
	{ "ASRC1R", NULL, "ASRC1R Input" },
	{ "ASRC2L", NULL, "ASRC2L Input" },
	{ "ASRC2R", NULL, "ASRC2R Input" },

	{ "ISRC1DEC1", NULL, "ISRC1DEC1 Input" },
	{ "ISRC1DEC2", NULL, "ISRC1DEC2 Input" },
	{ "ISRC1DEC3", NULL, "ISRC1DEC3 Input" },
	{ "ISRC1DEC4", NULL, "ISRC1DEC4 Input" },

	{ "ISRC1INT1", NULL, "ISRC1INT1 Input" },
	{ "ISRC1INT2", NULL, "ISRC1INT2 Input" },
	{ "ISRC1INT3", NULL, "ISRC1INT3 Input" },
	{ "ISRC1INT4", NULL, "ISRC1INT4 Input" },

	{ "ISRC2DEC1", NULL, "ISRC2DEC1 Input" },
	{ "ISRC2DEC2", NULL, "ISRC2DEC2 Input" },
	{ "ISRC2DEC3", NULL, "ISRC2DEC3 Input" },
	{ "ISRC2DEC4", NULL, "ISRC2DEC4 Input" },

	{ "ISRC2INT1", NULL, "ISRC2INT1 Input" },
	{ "ISRC2INT2", NULL, "ISRC2INT2 Input" },
	{ "ISRC2INT3", NULL, "ISRC2INT3 Input" },
	{ "ISRC2INT4", NULL, "ISRC2INT4 Input" },

	ARIZONA_MIXER_ROUTES("OUT1L", "HPOUT1L"),
	ARIZONA_MIXER_ROUTES("OUT1R", "HPOUT1R"),
	ARIZONA_MIXER_ROUTES("OUT2L", "HPOUT2L"),
	ARIZONA_MIXER_ROUTES("OUT2R", "HPOUT2R"),
	ARIZONA_MIXER_ROUTES("OUT3L", "EPOUT"),

	ARIZONA_MIXER_ROUTES("OUT4L", "SPKOUTL"),
	ARIZONA_MIXER_ROUTES("OUT4R", "SPKOUTR"),
	ARIZONA_MIXER_ROUTES("OUT5L", "SPKDAT1L"),
	ARIZONA_MIXER_ROUTES("OUT5R", "SPKDAT1R"),

	ARIZONA_MIXER_ROUTES("PWM1 Driver", "PWM1"),
	ARIZONA_MIXER_ROUTES("PWM2 Driver", "PWM2"),

	ARIZONA_MIXER_ROUTES("AIF1TX1", "AIF1TX1"),
	ARIZONA_MIXER_ROUTES("AIF1TX2", "AIF1TX2"),
	ARIZONA_MIXER_ROUTES("AIF1TX3", "AIF1TX3"),
	ARIZONA_MIXER_ROUTES("AIF1TX4", "AIF1TX4"),
	ARIZONA_MIXER_ROUTES("AIF1TX5", "AIF1TX5"),
	ARIZONA_MIXER_ROUTES("AIF1TX6", "AIF1TX6"),
	ARIZONA_MIXER_ROUTES("AIF1TX7", "AIF1TX7"),
	ARIZONA_MIXER_ROUTES("AIF1TX8", "AIF1TX8"),

	ARIZONA_MIXER_ROUTES("AIF2TX1", "AIF2TX1"),
	ARIZONA_MIXER_ROUTES("AIF2TX2", "AIF2TX2"),

	ARIZONA_MIXER_ROUTES("AIF3TX1", "AIF3TX1"),
	ARIZONA_MIXER_ROUTES("AIF3TX2", "AIF3TX2"),

	ARIZONA_MIXER_ROUTES("EQ1", "EQ1"),
	ARIZONA_MIXER_ROUTES("EQ2", "EQ2"),
	ARIZONA_MIXER_ROUTES("EQ3", "EQ3"),
	ARIZONA_MIXER_ROUTES("EQ4", "EQ4"),

	ARIZONA_MIXER_ROUTES("DRC1L", "DRC1L"),
	ARIZONA_MIXER_ROUTES("DRC1R", "DRC1R"),

	ARIZONA_MIXER_ROUTES("DRC2L", "DRC2L"),
	ARIZONA_MIXER_ROUTES("DRC2R", "DRC2R"),

	ARIZONA_MIXER_ROUTES("LHPF1", "LHPF1"),
	ARIZONA_MIXER_ROUTES("LHPF2", "LHPF2"),
	ARIZONA_MIXER_ROUTES("LHPF3", "LHPF3"),
	ARIZONA_MIXER_ROUTES("LHPF4", "LHPF4"),

	ARIZONA_MUX_ROUTES("ASRC1L"),
	ARIZONA_MUX_ROUTES("ASRC1R"),
	ARIZONA_MUX_ROUTES("ASRC2L"),
	ARIZONA_MUX_ROUTES("ASRC2R"),

	ARIZONA_MUX_ROUTES("ISRC1INT1"),
	ARIZONA_MUX_ROUTES("ISRC1INT2"),
	ARIZONA_MUX_ROUTES("ISRC1INT3"),
	ARIZONA_MUX_ROUTES("ISRC1INT4"),

	ARIZONA_MUX_ROUTES("ISRC1DEC1"),
	ARIZONA_MUX_ROUTES("ISRC1DEC2"),
	ARIZONA_MUX_ROUTES("ISRC1DEC3"),
	ARIZONA_MUX_ROUTES("ISRC1DEC4"),

	ARIZONA_MUX_ROUTES("ISRC2INT1"),
	ARIZONA_MUX_ROUTES("ISRC2INT2"),
	ARIZONA_MUX_ROUTES("ISRC2INT3"),
	ARIZONA_MUX_ROUTES("ISRC2INT4"),

	ARIZONA_MUX_ROUTES("ISRC2DEC1"),
	ARIZONA_MUX_ROUTES("ISRC2DEC2"),
	ARIZONA_MUX_ROUTES("ISRC2DEC3"),
	ARIZONA_MUX_ROUTES("ISRC2DEC4"),

	ARIZONA_DSP_ROUTES("DSP1"),

	//{ "AEC Loopback", "HPOUT1L", "OUT1L" },
	//{ "AEC Loopback", "HPOUT1R", "OUT1R" },
	{ "HPOUT1L", NULL, "OUT1L" },
	{ "HPOUT1R", NULL, "OUT1R" },

	//{ "AEC Loopback", "HPOUT2L", "OUT2L" },
	//{ "AEC Loopback", "HPOUT2R", "OUT2R" },
	{ "HPOUT2L", NULL, "OUT2L" },
	{ "HPOUT2R", NULL, "OUT2R" },

	//{ "AEC Loopback", "EPOUT", "OUT3L" },
	{ "EPOUTN", NULL, "OUT3L" },
	{ "EPOUTP", NULL, "OUT3L" },

	//{ "AEC Loopback", "SPKOUTL", "OUT4L" },
	{ "SPKOUTLN", NULL, "OUT4L" },
	{ "SPKOUTLP", NULL, "OUT4L" },

	//{ "AEC Loopback", "SPKOUTR", "OUT4R" },
	{ "SPKOUTRN", NULL, "OUT4R" },
	{ "SPKOUTRP", NULL, "OUT4R" },

	//{ "AEC Loopback", "SPKDAT1L", "OUT5L" },
	//{ "AEC Loopback", "SPKDAT1R", "OUT5R" },
	{ "SPKDAT1L", NULL, "OUT5L" },
	{ "SPKDAT1R", NULL, "OUT5R" },

//add for speaker PA
	{"PA_Engine", NULL, "PA_SPKINR"},
	{"PA_Engine", NULL, "PA_SPKINL"},
	{"PA_SPKOUTN", NULL, "PA_Engine"},
	{"PA_SPKOUTP", NULL, "PA_Engine"},
	{"PA_SPKINR", NULL, "HPOUT2R"},
	{"PA_SPKINL", NULL, "HPOUT2L"},
//add end
};

static int wm5102_set_fll(struct snd_soc_codec *codec, int fll_id, int source,
			  unsigned int Fref, unsigned int Fout)
{
	struct wm5102_priv *wm5102 = snd_soc_codec_get_drvdata(codec);

	switch (fll_id) {
	case WM5102_FLL1:
		return arizona_set_fll(&wm5102->fll[0], source, Fref, Fout);
	case WM5102_FLL2:
		return arizona_set_fll(&wm5102->fll[1], source, Fref, Fout);
	case WM5102_FLL1_REFCLK:
		return arizona_set_fll_refclk(&wm5102->fll[0], source, Fref,
					      Fout);
	case WM5102_FLL2_REFCLK:
		return arizona_set_fll_refclk(&wm5102->fll[1], source, Fref,
					      Fout);
	default:
		return -EINVAL;
	}
}

#define WM5102_RATES SNDRV_PCM_RATE_8000_192000

#define WM5102_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver wm5102_dai[] = {
	{
		.name = "wm5102-aif1",
		.id = 1,
		.base = ARIZONA_AIF1_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = WM5102_RATES,
			.formats = WM5102_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF1 Capture",
			 .channels_min = 1,
			 .channels_max = 8,
			 .rates = WM5102_RATES,
			 .formats = WM5102_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "wm5102-aif2",
		.id = 2,
		.base = ARIZONA_AIF2_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM5102_RATES,
			.formats = WM5102_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF2 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = WM5102_RATES,
			 .formats = WM5102_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "wm5102-aif3",
		.id = 3,
		.base = ARIZONA_AIF3_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM5102_RATES,
			.formats = WM5102_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF3 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = WM5102_RATES,
			 .formats = WM5102_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
};

static int wm5102_codec_probe(struct snd_soc_codec *codec)
{
	struct wm5102_priv *priv = snd_soc_codec_get_drvdata(codec);
	int ret;

	codec->control_data = priv->core.arizona->regmap;

	ret = snd_soc_codec_set_cache_io(codec, 32, 16, SND_SOC_REGMAP);
	if (ret != 0)
		return ret;

	ret = snd_soc_add_codec_controls(codec, wm_adsp_fw_controls, 1);
	if (ret != 0)
		return ret;

//	snd_soc_dapm_disable_pin(&codec->dapm, "HAPTICS");

//	priv->core.arizona->dapm = &codec->dapm;

	return 0;
}

static int wm5102_codec_remove(struct snd_soc_codec *codec)
{
//	struct wm5102_priv *priv = snd_soc_codec_get_drvdata(codec);

//	priv->core.arizona->dapm = NULL;

	return 0;
}

#define WM5102_DIG_VU 0x0200

static unsigned int wm5102_digital_vu[] = {
	ARIZONA_DAC_DIGITAL_VOLUME_1L,
	ARIZONA_DAC_DIGITAL_VOLUME_1R,
	ARIZONA_DAC_DIGITAL_VOLUME_2L,
	ARIZONA_DAC_DIGITAL_VOLUME_2R,
	ARIZONA_DAC_DIGITAL_VOLUME_3L,
	ARIZONA_DAC_DIGITAL_VOLUME_3R,
	ARIZONA_DAC_DIGITAL_VOLUME_4L,
	ARIZONA_DAC_DIGITAL_VOLUME_4R,
	ARIZONA_DAC_DIGITAL_VOLUME_5L,
	ARIZONA_DAC_DIGITAL_VOLUME_5R,
};

static struct snd_soc_codec_driver soc_codec_dev_wm5102 = {
	.probe = wm5102_codec_probe,
	.remove = wm5102_codec_remove,

	.idle_bias_off = true,

	.set_sysclk = arizona_set_sysclk,
	.set_pll = wm5102_set_fll,

	.controls = wm5102_snd_controls,
	.num_controls = ARRAY_SIZE(wm5102_snd_controls),
	.dapm_widgets = wm5102_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(wm5102_dapm_widgets),
	.dapm_routes = wm5102_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(wm5102_dapm_routes),
};

void wm5102_fll_interrupt_regist(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(wm5102_p->fll); i++)
		wm5102_p->fll[i].vco_mult = 1;

	arizona_init_fll(arizona_global, 1, ARIZONA_FLL1_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL1_LOCK, ARIZONA_IRQ_FLL1_CLOCK_OK,
			 &wm5102_p->fll[0]);
	arizona_init_fll(arizona_global, 2, ARIZONA_FLL2_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL2_LOCK, ARIZONA_IRQ_FLL2_CLOCK_OK,
			 &wm5102_p->fll[1]);

	for (i = 0; i < ARRAY_SIZE(wm5102_dai); i++)
		arizona_init_dai(&wm5102_p->core, i);

	/* Latch volume update bits */
	for (i = 0; i < ARRAY_SIZE(wm5102_digital_vu); i++)
		regmap_update_bits(arizona_global->regmap, wm5102_digital_vu[i],
				   WM5102_DIG_VU, WM5102_DIG_VU);

    return;
}
EXPORT_SYMBOL_GPL(wm5102_fll_interrupt_regist);

static int __devinit wm5102_probe(struct platform_device *pdev)
{
	struct arizona *arizona = dev_get_drvdata(pdev->dev.parent);
	struct wm5102_priv *wm5102;
	int ret;

	wm5102 = devm_kzalloc(&pdev->dev, sizeof(struct wm5102_priv),
			      GFP_KERNEL);
	if (wm5102 == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, wm5102);

	wm5102->core.arizona = arizona;
	wm5102->core.num_inputs = 6;

	wm5102->core.adsp[0].part = "wm5102";
	wm5102->core.adsp[0].num = 1;
	wm5102->core.adsp[0].type = WMFW_ADSP2;
	wm5102->core.adsp[0].base = ARIZONA_DSP1_CONTROL_1;
	wm5102->core.adsp[0].dev = arizona->dev;
	wm5102->core.adsp[0].regmap = arizona->regmap;
	wm5102->core.adsp[0].mem = wm5102_dsp1_regions;
	wm5102->core.adsp[0].num_mems = ARRAY_SIZE(wm5102_dsp1_regions);

	ret = wm_adsp2_init(&wm5102->core.adsp[0], true);
	if (ret != 0)
		return ret;

/* the register of headset/headphone detection irq moved to k5_machine, 
 * because the k5_machine loaded later than this module, so there is a 
 * bug when the system powered up with headset/headphone plugin.*/
#if 0
	for (i = 0; i < ARRAY_SIZE(wm5102->fll); i++)
		wm5102->fll[i].vco_mult = 1;

	arizona_init_fll(arizona, 1, ARIZONA_FLL1_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL1_LOCK, ARIZONA_IRQ_FLL1_CLOCK_OK,
			 &wm5102->fll[0]);
	arizona_init_fll(arizona, 2, ARIZONA_FLL2_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL2_LOCK, ARIZONA_IRQ_FLL2_CLOCK_OK,
			 &wm5102->fll[1]);

	for (i = 0; i < ARRAY_SIZE(wm5102_dai); i++)
		arizona_init_dai(&wm5102->core, i);

	/* Latch volume update bits */
	for (i = 0; i < ARRAY_SIZE(wm5102_digital_vu); i++)
		regmap_update_bits(arizona->regmap, wm5102_digital_vu[i],
				   WM5102_DIG_VU, WM5102_DIG_VU);
#endif

    wm5102_p = wm5102;
    speaker_pa_gpio_request();

	pm_runtime_enable(&pdev->dev);
	pm_runtime_idle(&pdev->dev);

	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_wm5102,
				      wm5102_dai, ARRAY_SIZE(wm5102_dai));
}

static int __devexit wm5102_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
    gpio_free(SPEAKER_PA_GPIO);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static struct platform_driver wm5102_codec_driver = {
	.driver = {
		.name = "wm5102-codec",
		.owner = THIS_MODULE,
	},
	.probe = wm5102_probe,
	.remove = __devexit_p(wm5102_remove),
};

static __init int wm5102_codec_init(void)
{
	return platform_driver_register(&wm5102_codec_driver);
}
module_init(wm5102_codec_init);

static __exit void wm5102_codec_exit(void)
{
	return platform_driver_unregister(&wm5102_codec_driver);
}
module_exit(wm5102_codec_exit);

MODULE_DESCRIPTION("ASoC WM5102 driver");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wm5102-codec");
