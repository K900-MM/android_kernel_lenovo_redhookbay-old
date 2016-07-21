/*
 * platform_smb347.c: smb347 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <asm/intel-mid.h>
#include <linux/lnw_gpio.h>
#include <linux/power/smb347-charger.h>
#include <asm/intel-mid.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>
#include "platform_smb347.h"
#include <linux/power_supply.h>
#include <linux/power/battery_id.h>

/* Redridge DV2.1 */
static struct smb347_charger_platform_data smb347_rr_pdata = {
	.battery_info	= {
		.name			= "UP110005",
		.technology		= POWER_SUPPLY_TECHNOLOGY_LIPO,
		.voltage_max_design	= 3700000,
		.voltage_min_design	= 3000000,
		.charge_full_design	= 6894000,
	},
	.use_mains			= true,
	.enable_control			= SMB347_CHG_ENABLE_PIN_ACTIVE_LOW,
	.otg_control			= SMB347_OTG_CONTROL_SW,
	.irq_gpio			= SMB347_IRQ_GPIO,
	.char_config_regs		= {
						/* Reg  Value */
						0x00, 0xFC,
						0x01, 0x95,
						0x02, 0x83,
						0x03, 0xE3,
						0x04, 0x3A,
						0x05, 0x1A,
						0x06, 0x65,
						0x07, 0xEF,
						0x08, 0x09,
						0x09, 0xDF,
						0x0A, 0xAB,
						0x0B, 0x5A,
						0x0C, 0xC1,
						0x0D, 0x46,
						0x00, 0x00,
					},
};

/* Salitpa EV 0.5 */
static struct smb347_charger_platform_data smb347_ev05_pdata = {
	.battery_info	= {
		.name			= "UP110005",
		.technology		= POWER_SUPPLY_TECHNOLOGY_LIPO,
		.voltage_max_design	= 3700000,
		.voltage_min_design	= 3000000,
		.charge_full_design	= 6894000,
	},
	.use_mains			= true,
	.enable_control			= SMB347_CHG_ENABLE_PIN_ACTIVE_LOW,
	.otg_control			= SMB347_OTG_CONTROL_DISABLED,
	.irq_gpio			= SMB347_IRQ_GPIO,
	.char_config_regs		= {
						/* Reg  Value */
						0x00, 0xA1,
						0x01, 0x6C,
						0x02, 0x93,
						0x03, 0xE5,
						0x04, 0x3E,
						0x05, 0x16,
						0x06, 0x0C,
						0x07, 0x8c,
						0x08, 0x08,
						0x09, 0x0c,
						0x0A, 0xA4,
						0x0B, 0x13,
						0x0C, 0x81,
						0x0D, 0x02,
						0x0E, 0x20,
						0x10, 0x7F
					},
};

/* Salitpa EV 1.0 */
static struct smb347_charger_platform_data smb347_ev10_pdata = {
	.battery_info	= {
		.name			= "UP110005",
		.technology		= POWER_SUPPLY_TECHNOLOGY_LIPO,
		.voltage_max_design	= 3700000,
		.voltage_min_design	= 3000000,
		.charge_full_design	= 6894000,
	},
	.use_mains			= true,
	.enable_control			= SMB347_CHG_ENABLE_PIN_ACTIVE_LOW,
	.otg_control			= SMB347_OTG_CONTROL_DISABLED,
	.irq_gpio			= SMB347_IRQ_GPIO,
	.char_config_regs		= {
						/* Reg  Value */
						0x00, 0xAA,
						0x01, 0x6C,
						0x02, 0x93,
						0x03, 0xE5,
						0x04, 0x3E,
						0x05, 0x16,
						0x06, 0x74,
						0x07, 0xCC,
						0x09, 0x0C,
						0x0A, 0xA4,
						0x0B, 0x13,
						0x0C, 0x8D,
						0x0D, 0x00,
						0x0E, 0x20,
						0x10, 0x7F
					},
};

/* baytrail ffrd8 */
static struct smb347_charger_platform_data byt_t_ffrd8_pdata = {
	.battery_info	= {
		.name			= "UP110005",
		.technology		= POWER_SUPPLY_TECHNOLOGY_LIPO,
		.voltage_max_design	= 3700000,
		.voltage_min_design	= 3000000,
		.charge_full_design	= 6894000,
	},
	.use_mains			= false,
	.use_usb			= true,
	.enable_control			= SMB347_CHG_ENABLE_PIN_ACTIVE_LOW,
	.otg_control			= SMB347_OTG_CONTROL_DISABLED,
	.char_config_regs		= {
						/* Reg  Value */
						0x00, 0x46,
						0x01, 0x65,
						0x02, 0x93,
						0x03, 0xED,
						0x04, 0x38,
						0x05, 0x05,
						0x06, 0x74,
						0x07, 0x95,
						0x09, 0x00,
						0x0A, 0xC7,
						0x0B, 0x95,
						0x0C, 0x8F,
						0x0D, 0xB4,
						0x0E, 0x60,
						0x10, 0x40,
			/* disable suspend as charging didnot start*/
						0x30, 0x42,  /*orig:0x46*/
						0x31, 0x80
					},
};
#ifdef CONFIG_POWER_SUPPLY_CHARGER
#define SMB347_CHRG_CUR_NOLIMIT		1800
#define SMB347_CHRG_CUR_HIGH		1400
#define	SMB347_CHRG_CUR_MEDIUM		1200
#define SMB347_CHRG_CUR_LOW		1000

static struct ps_batt_chg_prof ps_batt_chrg_prof;
static struct ps_pse_mod_prof batt_chg_profile;
static struct power_supply_throttle smb347_throttle_states[] = {
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = SMB347_CHRG_CUR_NOLIMIT,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = SMB347_CHRG_CUR_HIGH,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = SMB347_CHRG_CUR_MEDIUM,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = SMB347_CHRG_CUR_LOW,
	},
	{
		.throttle_action = PSY_THROTTLE_DISABLE_CHARGING,
	},
	{
		.throttle_action = PSY_THROTTLE_DISABLE_CHARGER,
	},
};

static char *smb347_supplied_to[] = {
	"max170xx_battery",
	"max17042_battery",
	"max17047_battery",
};

static void *platform_get_batt_charge_profile()
{
	struct ps_temp_chg_table temp_mon_range[BATT_TEMP_NR_RNG];

	char batt_str[] = "INTN0001";

	memcpy(batt_chg_profile.batt_id, batt_str, strlen(batt_str));

	batt_chg_profile.battery_type = 0x2;
	batt_chg_profile.capacity = 0x2C52;
	batt_chg_profile.voltage_max = 4350;
	batt_chg_profile.chrg_term_mA = 300;
	batt_chg_profile.low_batt_mV = 3400;
	batt_chg_profile.disch_tmp_ul = 55;
	batt_chg_profile.disch_tmp_ll = 0;
	batt_chg_profile.temp_mon_ranges = 5;

	temp_mon_range[0].temp_up_lim = 55;
	temp_mon_range[0].full_chrg_vol = 4100;
	temp_mon_range[0].full_chrg_cur = 1500;
	temp_mon_range[0].maint_chrg_vol_ll = 4050;
	temp_mon_range[0].maint_chrg_vol_ul = 4100;
	temp_mon_range[0].maint_chrg_cur = 1000;

	temp_mon_range[1].temp_up_lim = 45;
	temp_mon_range[1].full_chrg_vol = 4350;
	temp_mon_range[1].full_chrg_cur = 1800;
	temp_mon_range[1].maint_chrg_vol_ll = 4230;
	temp_mon_range[1].maint_chrg_vol_ul = 4350;
	temp_mon_range[1].maint_chrg_cur = 1000;

	temp_mon_range[2].temp_up_lim = 23;
	temp_mon_range[2].full_chrg_vol = 4350;
	temp_mon_range[2].full_chrg_cur = 1500;
	temp_mon_range[2].maint_chrg_vol_ll = 4230;
	temp_mon_range[2].maint_chrg_vol_ul = 4350;
	temp_mon_range[2].maint_chrg_cur = 1000;

	temp_mon_range[3].temp_up_lim = 10;
	temp_mon_range[3].full_chrg_vol = 4340;
	temp_mon_range[3].full_chrg_cur = 1500;
	temp_mon_range[3].maint_chrg_vol_ll = 4230;
	temp_mon_range[3].maint_chrg_vol_ul = 4350;
	temp_mon_range[3].maint_chrg_cur = 1000;

	temp_mon_range[4].temp_up_lim = 0;
	temp_mon_range[4].full_chrg_vol = 0;
	temp_mon_range[4].full_chrg_cur = 0;
	temp_mon_range[4].maint_chrg_vol_ll = 0;
	temp_mon_range[4].maint_chrg_vol_ul = 0;
	temp_mon_range[4].maint_chrg_vol_ul = 0;
	temp_mon_range[4].maint_chrg_cur = 0;

	memcpy(batt_chg_profile.temp_mon_range,
		temp_mon_range,
		BATT_TEMP_NR_RNG * sizeof(struct ps_temp_chg_table));

	batt_chg_profile.temp_low_lim = 0;

	ps_batt_chrg_prof.chrg_prof_type = PSE_MOD_CHRG_PROF;
	ps_batt_chrg_prof.batt_prof = &batt_chg_profile;
	battery_prop_changed(POWER_SUPPLY_BATTERY_INSERTED, &ps_batt_chrg_prof);
	return &ps_batt_chrg_prof;
}

static void platform_init_chrg_params(
	struct smb347_charger_platform_data *pdata)
{
	pdata->throttle_states = smb347_throttle_states;
	pdata->supplied_to = smb347_supplied_to;
	pdata->num_throttle_states = ARRAY_SIZE(smb347_throttle_states);
	pdata->num_supplicants = ARRAY_SIZE(smb347_supplied_to);
	pdata->supported_cables = POWER_SUPPLY_CHARGER_TYPE_USB;
	pdata->chg_profile = (struct ps_batt_chg_prof *)
			platform_get_batt_charge_profile();
}
#endif

static void *get_platform_data(void)
{
	/* Redridge all */
	if (INTEL_MID_BOARD(2, TABLET, MFLD, RR, ENG) ||
		INTEL_MID_BOARD(2, TABLET, MFLD, RR, PRO))
		return &smb347_rr_pdata;
	else if (INTEL_MID_BOARD(2, TABLET, MFLD, SLP, ENG) ||
		INTEL_MID_BOARD(2, TABLET, MFLD, SLP, PRO)) {
		/* Salitpa */
		/* EV 0.5 */
		if (SPID_HARDWARE_ID(MFLD, TABLET, SLP, EV05))
			return &smb347_ev05_pdata;
		/* EV 1.0 and later */
		else
			return &smb347_ev10_pdata;
	} else if (intel_mid_identify_cpu() ==
				INTEL_MID_CPU_CHIP_VALLEYVIEW2) {
		byt_t_ffrd8_pdata.irq_gpio = acpi_get_gpio("\\_SB_GPO2", 2);
		byt_t_ffrd8_pdata.irq_gpio = 132;  /* sus0_2 */
#ifdef CONFIG_POWER_SUPPLY_CHARGER
		byt_t_ffrd8_pdata.enable_control = SMB347_CHG_ENABLE_SW;
		platform_init_chrg_params(&byt_t_ffrd8_pdata);
#endif
		return &byt_t_ffrd8_pdata; /* WA till the config data */
	}
	return NULL;
}

void *smb347_platform_data(void *info)
{
	return get_platform_data();
}
