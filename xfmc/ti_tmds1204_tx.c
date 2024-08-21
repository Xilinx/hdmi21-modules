// SPDX-License-Identifier: GPL-2.0
/*
 * TI TMDS1204 retimer driver
 *
 * Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/version.h>

int ti_tmds1204tx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx, u8 lanes);
void ti_tmds1204tx_exit(void);
int ti_tmds1204tx_entry(void);

#define DRIVER_NAME "ti_tmds1204-tx"

#define to_ti_tmds1204tx(_hw)	container_of(_hw, struct ti_tmds1204tx, hw)
struct ti_tmds1204tx *txdata;

struct reg_fields {
	u16 dev_type;
	u8 addr;
	u8 val;
};

enum {
	TX_TI_R1_INIT = 0, // program 6 registers
	TX_TI_TMDS_14_L_R1 = TX_TI_R1_INIT + 7, // 13 registers are programmed
	TX_TI_TMDS_14_H_R1 = TX_TI_TMDS_14_L_R1 + 13,
	TX_TI_TMDS_20_R1 = TX_TI_TMDS_14_H_R1 + 13,
	TX_TI_FRL_3G_R1 = TX_TI_TMDS_20_R1 + 13,
	TX_TI_FRL_6G_3_R1 = TX_TI_FRL_3G_R1 + 13,
	TX_TI_FRL_6G_4_R1 = TX_TI_FRL_6G_3_R1 + 13,
	TX_TI_FRL_8G_R1 = TX_TI_FRL_6G_4_R1 + 13,
	TX_TI_FRL_10G_R1 = TX_TI_FRL_8G_R1 + 13,
	TX_TI_FRL_12G_R1 = TX_TI_FRL_10G_R1 + 13,

	RX_TI_R1_INIT = TX_TI_FRL_12G_R1 + 13,
	RX_TI_TMDS_14_L_R1 = RX_TI_R1_INIT + 9,
	RX_TI_TMDS_14_H_R1 = RX_TI_TMDS_14_L_R1 + 14-2,
	RX_TI_TMDS_20_R1 = RX_TI_TMDS_14_H_R1 + 14-2,
	RX_TI_FRL_3G_R1 = RX_TI_TMDS_20_R1 + 14-2,
	RX_TI_FRL_6G_3_R1 = RX_TI_FRL_3G_R1 + 14-2,
	RX_TI_FRL_6G_4_R1 = RX_TI_FRL_6G_3_R1 + 14-2,
	RX_TI_FRL_8G_R1 = RX_TI_FRL_6G_4_R1 + 14-2,
	RX_TI_FRL_10G_R1 = RX_TI_FRL_8G_R1 + 14-2,
	RX_TI_FRL_12G_R1 = RX_TI_FRL_10G_R1 + 14-2,
};

/*
 * This table contains the values to be programmed to TI_TMDS1204 device.
 * Each entry is of the format:
 * 1) Device Type
 * 2) Register addr
 * 3) val
 */
const struct reg_fields ti_tmds1204tx_regs[] = {

	{TX_TI_R1_INIT, 0x0A, 0x8E},
	{TX_TI_R1_INIT, 0x0B, 0x43},
	{TX_TI_R1_INIT, 0x0C, 0x70},
	{TX_TI_R1_INIT, 0x0D, 0x22},
	{TX_TI_R1_INIT, 0x0E, 0x97},
	{TX_TI_R1_INIT, 0x11, 0x00},
	{TX_TI_R1_INIT, 0x09, 0x00},

	{TX_TI_TMDS_14_L_R1, 0x11, 0x00},
	{TX_TI_TMDS_14_L_R1, 0x0D, 0x22},
	{TX_TI_TMDS_14_L_R1, 0x12, 0x03},
	{TX_TI_TMDS_14_L_R1, 0x13, 0x00},
	{TX_TI_TMDS_14_L_R1, 0x14, 0x03},
	{TX_TI_TMDS_14_L_R1, 0x15, 0x05},
	{TX_TI_TMDS_14_L_R1, 0x16, 0x03},
	{TX_TI_TMDS_14_L_R1, 0x17, 0x05},
	{TX_TI_TMDS_14_L_R1, 0x18, 0x03},
	{TX_TI_TMDS_14_L_R1, 0x19, 0x05},
	{TX_TI_TMDS_14_L_R1, 0x20, 0x00},
	{TX_TI_TMDS_14_L_R1, 0x31, 0x00},
	{TX_TI_TMDS_14_L_R1, 0x11, 0x0F},

	{TX_TI_TMDS_14_H_R1, 0x11, 0x00},
	{TX_TI_TMDS_14_H_R1, 0x0D, 0x22},
	{TX_TI_TMDS_14_H_R1, 0x12, 0x03},
	{TX_TI_TMDS_14_H_R1, 0x13, 0x00},
	{TX_TI_TMDS_14_H_R1, 0x14, 0x03},
	{TX_TI_TMDS_14_H_R1, 0x15, 0x05},
	{TX_TI_TMDS_14_H_R1, 0x16, 0x03},
	{TX_TI_TMDS_14_H_R1, 0x17, 0x05},
	{TX_TI_TMDS_14_H_R1, 0x18, 0x03},
	{TX_TI_TMDS_14_H_R1, 0x19, 0x05},
	{TX_TI_TMDS_14_H_R1, 0x20, 0x00},
	{TX_TI_TMDS_14_H_R1, 0x31, 0x00},
	{TX_TI_TMDS_14_H_R1, 0x11, 0x0F},

	{TX_TI_TMDS_20_R1, 0x11, 0x00},
	{TX_TI_TMDS_20_R1, 0x0D, 0x22},
	{TX_TI_TMDS_20_R1, 0x12, 0x03},
	{TX_TI_TMDS_20_R1, 0x13, 0x00},
	{TX_TI_TMDS_20_R1, 0x14, 0x03},
	{TX_TI_TMDS_20_R1, 0x15, 0x05},
	{TX_TI_TMDS_20_R1, 0x16, 0x03},
	{TX_TI_TMDS_20_R1, 0x17, 0x05},
	{TX_TI_TMDS_20_R1, 0x18, 0x03},
	{TX_TI_TMDS_20_R1, 0x19, 0x05},
	{TX_TI_TMDS_20_R1, 0x20, 0x02},
	{TX_TI_TMDS_20_R1, 0x31, 0x00},
	{TX_TI_TMDS_20_R1, 0x11, 0x0F},

	{TX_TI_FRL_3G_R1, 0x11, 0x00},
	{TX_TI_FRL_3G_R1, 0x0D, 0x22},
	{TX_TI_FRL_3G_R1, 0x12, 0x03},
	{TX_TI_FRL_3G_R1, 0x13, 0x00},
	{TX_TI_FRL_3G_R1, 0x14, 0x03},
	{TX_TI_FRL_3G_R1, 0x15, 0x05},
	{TX_TI_FRL_3G_R1, 0x16, 0x03},
	{TX_TI_FRL_3G_R1, 0x17, 0x05},
	{TX_TI_FRL_3G_R1, 0x18, 0x03},
	{TX_TI_FRL_3G_R1, 0x19, 0x05},
	{TX_TI_FRL_3G_R1, 0x20, 0x00},
	{TX_TI_FRL_3G_R1, 0x31, 0x01},
	{TX_TI_FRL_3G_R1, 0x11, 0x0F},

	{TX_TI_FRL_6G_3_R1, 0x11, 0x00},
	{TX_TI_FRL_6G_3_R1, 0x0D, 0x22},
	{TX_TI_FRL_6G_3_R1, 0x12, 0x03},
	{TX_TI_FRL_6G_3_R1, 0x13, 0x00},
	{TX_TI_FRL_6G_3_R1, 0x14, 0x03},
	{TX_TI_FRL_6G_3_R1, 0x15, 0x05},
	{TX_TI_FRL_6G_3_R1, 0x16, 0x03},
	{TX_TI_FRL_6G_3_R1, 0x17, 0x05},
	{TX_TI_FRL_6G_3_R1, 0x18, 0x03},
	{TX_TI_FRL_6G_3_R1, 0x19, 0x05},
	{TX_TI_FRL_6G_3_R1, 0x20, 0x00},
	{TX_TI_FRL_6G_3_R1, 0x31, 0x02},
	{TX_TI_FRL_6G_3_R1, 0x11, 0x0F},

	{TX_TI_FRL_6G_4_R1, 0x11, 0x00},
	{TX_TI_FRL_6G_4_R1, 0x0D, 0x22},
	{TX_TI_FRL_6G_4_R1, 0x12, 0x03},
	{TX_TI_FRL_6G_4_R1, 0x13, 0x05},
	{TX_TI_FRL_6G_4_R1, 0x14, 0x03},
	{TX_TI_FRL_6G_4_R1, 0x15, 0x05},
	{TX_TI_FRL_6G_4_R1, 0x16, 0x03},
	{TX_TI_FRL_6G_4_R1, 0x17, 0x05},
	{TX_TI_FRL_6G_4_R1, 0x18, 0x03},
	{TX_TI_FRL_6G_4_R1, 0x19, 0x05},
	{TX_TI_FRL_6G_4_R1, 0x20, 0x00},
	{TX_TI_FRL_6G_4_R1, 0x31, 0x03},
	{TX_TI_FRL_6G_4_R1, 0x11, 0x0F},

	{TX_TI_FRL_8G_R1, 0x11, 0x00},
	{TX_TI_FRL_8G_R1, 0x0D, 0x22},
	{TX_TI_FRL_8G_R1, 0x12, 0x03},
	{TX_TI_FRL_8G_R1, 0x13, 0x05},
	{TX_TI_FRL_8G_R1, 0x14, 0x03},
	{TX_TI_FRL_8G_R1, 0x15, 0x05},
	{TX_TI_FRL_8G_R1, 0x16, 0x03},
	{TX_TI_FRL_8G_R1, 0x17, 0x05},
	{TX_TI_FRL_8G_R1, 0x18, 0x03},
	{TX_TI_FRL_8G_R1, 0x19, 0x05},
	{TX_TI_FRL_8G_R1, 0x20, 0x00},
	{TX_TI_FRL_8G_R1, 0x31, 0x04},
	{TX_TI_FRL_8G_R1, 0x11, 0x0F},

	{TX_TI_FRL_10G_R1, 0x11, 0x00},
	{TX_TI_FRL_10G_R1, 0x0D, 0x22},
	{TX_TI_FRL_10G_R1, 0x12, 0x03},
	{TX_TI_FRL_10G_R1, 0x13, 0x05},
	{TX_TI_FRL_10G_R1, 0x14, 0x03},
	{TX_TI_FRL_10G_R1, 0x15, 0x05},
	{TX_TI_FRL_10G_R1, 0x16, 0x03},
	{TX_TI_FRL_10G_R1, 0x17, 0x05},
	{TX_TI_FRL_10G_R1, 0x18, 0x03},
	{TX_TI_FRL_10G_R1, 0x19, 0x05},
	{TX_TI_FRL_10G_R1, 0x20, 0x00},
	{TX_TI_FRL_10G_R1, 0x31, 0x05},
	{TX_TI_FRL_10G_R1, 0x11, 0x0F},

	{TX_TI_FRL_12G_R1, 0x11, 0x00},
	{TX_TI_FRL_12G_R1, 0x0D, 0x22},
#if defined (XPS_BOARD_ZCU102)
	{TX_TI_FRL_12G_R1, 0x12, 0x02},
#elif defined (XPS_BOARD_ZCU106)
	{TX_TI_FRL_12G_R1, 0x12, 0x02},
#else
	{TX_TI_FRL_12G_R1, 0x12, 0x03},
#endif
	{TX_TI_FRL_12G_R1, 0x13, 0x05},
#if defined (XPS_BOARD_ZCU102)
	{TX_TI_FRL_12G_R1, 0x14, 0x02},
#elif defined (XPS_BOARD_ZCU106)
	{TX_TI_FRL_12G_R1, 0x14, 0x02},
#else
	{TX_TI_FRL_12G_R1, 0x14, 0x03},
#endif
	{TX_TI_FRL_12G_R1, 0x15, 0x05},
#if defined (XPS_BOARD_ZCU102)
	{TX_TI_FRL_12G_R1, 0x16, 0x02},
#elif defined (XPS_BOARD_ZCU106)
	{TX_TI_FRL_12G_R1, 0x16, 0x02},
#else
	{TX_TI_FRL_12G_R1, 0x16, 0x03},
#endif
	{TX_TI_FRL_12G_R1, 0x17, 0x05},
#if defined (XPS_BOARD_ZCU102)
	{TX_TI_FRL_12G_R1, 0x18, 0x02},
#elif defined (XPS_BOARD_ZCU106)
	{TX_TI_FRL_12G_R1, 0x18, 0x02},
#else
	{TX_TI_FRL_12G_R1, 0x18, 0x03},
#endif
	{TX_TI_FRL_12G_R1, 0x19, 0x05},
	{TX_TI_FRL_12G_R1, 0x20, 0x00},
	{TX_TI_FRL_12G_R1, 0x31, 0x06},
	{TX_TI_FRL_12G_R1, 0x11, 0x0F},

	{RX_TI_R1_INIT, 0x0A, 0x4E},
	{RX_TI_R1_INIT, 0x0B, 0x43},
	{RX_TI_R1_INIT, 0x0C, 0x70},
	{RX_TI_R1_INIT, 0x0D, 0xE3},
	{RX_TI_R1_INIT, 0x0E, 0x97},
	{RX_TI_R1_INIT, 0x1E, 0x00},
	{RX_TI_R1_INIT, 0x11, 0x0F},
	{RX_TI_R1_INIT, 0x09, 0x00},
	{RX_TI_R1_INIT, 0xF8, 0x03},

	{RX_TI_TMDS_14_L_R1, 0x0A, 0x4E},
	{RX_TI_TMDS_14_L_R1, 0x0D, 0xE3},
	{RX_TI_TMDS_14_L_R1, 0x12, 0x03},
	{RX_TI_TMDS_14_L_R1, 0x13, 0x00},
	{RX_TI_TMDS_14_L_R1, 0x14, 0x03},
	{RX_TI_TMDS_14_L_R1, 0x15, 0x05},
	{RX_TI_TMDS_14_L_R1, 0x16, 0x03},
	{RX_TI_TMDS_14_L_R1, 0x17, 0x05},
	{RX_TI_TMDS_14_L_R1, 0x18, 0x03},
	{RX_TI_TMDS_14_L_R1, 0x19, 0x05},
	{RX_TI_TMDS_14_L_R1, 0x20, 0x00},
	{RX_TI_TMDS_14_L_R1, 0x31, 0x00},

	{RX_TI_TMDS_14_H_R1, 0x0A, 0x4E},
	{RX_TI_TMDS_14_H_R1, 0x0D, 0xE3},
	{RX_TI_TMDS_14_H_R1, 0x12, 0x03},
	{RX_TI_TMDS_14_H_R1, 0x13, 0x00},
	{RX_TI_TMDS_14_H_R1, 0x14, 0x03},
	{RX_TI_TMDS_14_H_R1, 0x15, 0x05},
	{RX_TI_TMDS_14_H_R1, 0x16, 0x03},
	{RX_TI_TMDS_14_H_R1, 0x17, 0x05},
	{RX_TI_TMDS_14_H_R1, 0x18, 0x03},
	{RX_TI_TMDS_14_H_R1, 0x19, 0x05},
	{RX_TI_TMDS_14_H_R1, 0x20, 0x00},
	{RX_TI_TMDS_14_H_R1, 0x31, 0x00},

	{RX_TI_TMDS_20_R1, 0x0A, 0x4E},
	{RX_TI_TMDS_20_R1, 0x0D, 0xE3},
	{RX_TI_TMDS_20_R1, 0x12, 0x03},
	{RX_TI_TMDS_20_R1, 0x13, 0x00},
	{RX_TI_TMDS_20_R1, 0x14, 0x03},
	{RX_TI_TMDS_20_R1, 0x15, 0x05},
	{RX_TI_TMDS_20_R1, 0x16, 0x03},
	{RX_TI_TMDS_20_R1, 0x17, 0x05},
	{RX_TI_TMDS_20_R1, 0x18, 0x03},
	{RX_TI_TMDS_20_R1, 0x19, 0x05},
	{RX_TI_TMDS_20_R1, 0x20, 0x02},
	{RX_TI_TMDS_20_R1, 0x31, 0x00},
	{RX_TI_FRL_3G_R1, 0x0A, 0x0E},
	{RX_TI_FRL_3G_R1, 0x0D, 0xE3},
	{RX_TI_FRL_3G_R1, 0x12, 0x03},
	{RX_TI_FRL_3G_R1, 0x13, 0x00},
	{RX_TI_FRL_3G_R1, 0x14, 0x03},
	{RX_TI_FRL_3G_R1, 0x15, 0x05},
	{RX_TI_FRL_3G_R1, 0x16, 0x03},
	{RX_TI_FRL_3G_R1, 0x17, 0x05},
	{RX_TI_FRL_3G_R1, 0x18, 0x03},
	{RX_TI_FRL_3G_R1, 0x19, 0x05},
	{RX_TI_FRL_3G_R1, 0x20, 0x00},
	{RX_TI_FRL_3G_R1, 0x31, 0x01},
	{RX_TI_FRL_6G_3_R1, 0x0A, 0x0E},
	{RX_TI_FRL_6G_3_R1, 0x0D, 0xE3},
	{RX_TI_FRL_6G_3_R1, 0x12, 0x03},
	{RX_TI_FRL_6G_3_R1, 0x13, 0x00},
	{RX_TI_FRL_6G_3_R1, 0x14, 0x03},
	{RX_TI_FRL_6G_3_R1, 0x15, 0x05},
	{RX_TI_FRL_6G_3_R1, 0x16, 0x03},
	{RX_TI_FRL_6G_3_R1, 0x17, 0x05},
	{RX_TI_FRL_6G_3_R1, 0x18, 0x03},
	{RX_TI_FRL_6G_3_R1, 0x19, 0x05},
	{RX_TI_FRL_6G_3_R1, 0x20, 0x00},
	{RX_TI_FRL_6G_3_R1, 0x31, 0x02},
	{RX_TI_FRL_6G_4_R1, 0x0A, 0x0E},
	{RX_TI_FRL_6G_4_R1, 0x0D, 0xE3},
	{RX_TI_FRL_6G_4_R1, 0x12, 0x03},
	{RX_TI_FRL_6G_4_R1, 0x13, 0x05},
	{RX_TI_FRL_6G_4_R1, 0x14, 0x03},
	{RX_TI_FRL_6G_4_R1, 0x15, 0x05},
	{RX_TI_FRL_6G_4_R1, 0x16, 0x03},
	{RX_TI_FRL_6G_4_R1, 0x17, 0x05},
	{RX_TI_FRL_6G_4_R1, 0x18, 0x03},
	{RX_TI_FRL_6G_4_R1, 0x19, 0x05},
	{RX_TI_FRL_6G_4_R1, 0x20, 0x00},
	{RX_TI_FRL_6G_4_R1, 0x31, 0x03},

	{RX_TI_FRL_8G_R1, 0x0A, 0x0E},
	{RX_TI_FRL_8G_R1, 0x0D, 0xF3},
	{RX_TI_FRL_8G_R1, 0x12, 0x01},
	{RX_TI_FRL_8G_R1, 0x13, 0x00},
	{RX_TI_FRL_8G_R1, 0x14, 0x03},
	{RX_TI_FRL_8G_R1, 0x15, 0x05},
	{RX_TI_FRL_8G_R1, 0x16, 0x01},
	{RX_TI_FRL_8G_R1, 0x17, 0x00},
	{RX_TI_FRL_8G_R1, 0x18, 0x01},
	{RX_TI_FRL_8G_R1, 0x19, 0x00},
	{RX_TI_FRL_8G_R1, 0x20, 0x00},
	{RX_TI_FRL_8G_R1, 0x31, 0x04},

	{RX_TI_FRL_10G_R1, 0x0A, 0x0E},
	{RX_TI_FRL_10G_R1, 0x0D, 0xF3},
	{RX_TI_FRL_10G_R1, 0x12, 0x02},
	{RX_TI_FRL_10G_R1, 0x13, 0x00},
	{RX_TI_FRL_10G_R1, 0x14, 0x01},
	{RX_TI_FRL_10G_R1, 0x15, 0x00},
	{RX_TI_FRL_10G_R1, 0x16, 0x00},
	{RX_TI_FRL_10G_R1, 0x17, 0x01},
	{RX_TI_FRL_10G_R1, 0x18, 0x02},
	{RX_TI_FRL_10G_R1, 0x19, 0x00},
	{RX_TI_FRL_10G_R1, 0x20, 0x00},
	{RX_TI_FRL_10G_R1, 0x31, 0x05},
	{RX_TI_FRL_12G_R1, 0x0A, 0x0E},
#if defined (XPS_BOARD_ZCU102)
	{RX_TI_FRL_12G_R1, 0x0D, 0xF3},
#elif defined (XPS_BOARD_ZCU106)
	{RX_TI_FRL_12G_R1, 0x0D, 0xF3},
#else
	{RX_TI_FRL_12G_R1, 0x0D, 0xF3},
#endif
	{RX_TI_FRL_12G_R1, 0x12, 0x01},
#if defined (XPS_BOARD_ZCU102)
	{RX_TI_FRL_12G_R1, 0x13, 0x00},
#elif defined (XPS_BOARD_ZCU106)
	{RX_TI_FRL_12G_R1, 0x13, 0x05},
#else
	{RX_TI_FRL_12G_R1, 0x13, 0x01},
#endif
	{RX_TI_FRL_12G_R1, 0x14, 0x01},
	{RX_TI_FRL_12G_R1, 0x15, 0x01},
#if defined (XPS_BOARD_ZCU106)
	{RX_TI_FRL_12G_R1, 0x16, 0x00},
	{RX_TI_FRL_12G_R1, 0x17, 0x03},
#else
	{RX_TI_FRL_12G_R1, 0x16, 0x01},
	{RX_TI_FRL_12G_R1, 0x17, 0x01},
#endif

#if defined (XPS_BOARD_ZCU102)
	{RX_TI_FRL_12G_R1, 0x18, 0x02},
#elif defined (XPS_BOARD_ZCU106)
	{RX_TI_FRL_12G_R1, 0x18, 0x02},
#else
	{RX_TI_FRL_12G_R1, 0x18, 0x01},
#endif
	{RX_TI_FRL_12G_R1, 0x19, 0x01},
	{RX_TI_FRL_12G_R1, 0x20, 0x00},
	{RX_TI_FRL_12G_R1, 0x31, 0x06},
};

static const struct regmap_config ti_tmds1204tx_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

/*
 * struct ti_tmds1204 - ti_tmds1204 device structure
 * @client: Pointer to I2C client
 * @regmap: Pointer to regmap structure
 * @lock: Mutex structure
 * @mode_index: Resolution mode index
 */
struct ti_tmds1204tx {
	struct clk_hw hw;
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex lock; /* mutex lock for operations */
	u32 mode_index;
};

static inline int ti_tmds1204tx_read_reg(struct ti_tmds1204tx *priv, u8 addr, u8 *val)
{
	int err = 0;

	err = regmap_read(priv->regmap, addr, (unsigned int *)val);
	if (err)
		dev_dbg(&priv->client->dev,
			"i2c read failed, addr = %x\n", addr);
	return err;
}

static inline int ti_tmds1204tx_write_reg(struct ti_tmds1204tx *priv, u8 addr, u8 val)
{
	int err = 0;

	err = regmap_write(priv->regmap, addr, val);
	if (err) {
		dev_dbg(&priv->client->dev,
			"i2c write failed, addr = %x\n", addr);
	}

	return err;
}

int ti_tmds1204tx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx, u8 lanes)
{
	u32 linerate_mbps;
	u32 i = 0;
	u16 dev_type = 0xffff;
	int ret = 1;
	u8 revision = 1;

	linerate_mbps = (u32)((u64)linerate / 1000000);
	dev_info(&txdata->client->dev, "linerate %llu lineratembps %u lanes %d\n\r",
		 linerate, linerate_mbps, lanes);
	/* TX */
	if (is_tx == 1) {
		switch (revision) {
		case 1:
			if (is_frl == 1) {
				if (linerate_mbps == 12000)
					dev_type = TX_TI_FRL_12G_R1;
				else if (linerate_mbps == 10000)
					dev_type = TX_TI_FRL_10G_R1;
				else if (linerate_mbps == 8000)
					dev_type = TX_TI_FRL_8G_R1;
				else if (linerate_mbps == 6000) {
					if (lanes == 4)
						dev_type = TX_TI_FRL_6G_4_R1;
					else
						dev_type = TX_TI_FRL_6G_3_R1;
				} else if (linerate_mbps == 3000)
					dev_type = TX_TI_FRL_3G_R1;
			} else {
				if (linerate_mbps <= 1650)
					dev_type = TX_TI_TMDS_14_L_R1;
				else if (linerate_mbps > 1650 &&
					 linerate_mbps <= 3400)
					dev_type = TX_TI_TMDS_14_H_R1;
				else
					dev_type = TX_TI_TMDS_20_R1;
			}
			break;
		default:
			break;
		}
	} else { /* RX */
		switch (revision) {
		case 1:
			if (is_frl == 1) {
				if (linerate_mbps == 12000)
					dev_type = RX_TI_FRL_12G_R1;
				else if (linerate_mbps == 10000)
					dev_type = RX_TI_FRL_10G_R1;
				else if (linerate_mbps == 8000)
					dev_type = RX_TI_FRL_8G_R1;
				else if (linerate_mbps == 6000)
					dev_type = RX_TI_FRL_6G_4_R1;
				else if (linerate_mbps == 3000)
					dev_type = RX_TI_FRL_3G_R1;;
			} else {
				dev_type = RX_TI_TMDS_20_R1;
			}
			break;
		default:

			break;
		}
	}

	i = dev_type;
	while (dev_type == ti_tmds1204tx_regs[i].dev_type) {
		ret = ti_tmds1204tx_write_reg(txdata, ti_tmds1204tx_regs[i].addr,
				       ti_tmds1204tx_regs[i].val);
		if (ret)
			return ret;

		i++;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ti_tmds1204tx_linerate_conf);

static int ti_tmds1204tx_init(struct ti_tmds1204tx *priv, u8 revision, u8 is_tx)
{
	int ret = 1;
	u16 dev_type = 0xffff;
	u32 i = 0;

	if (is_tx == 1) {
		switch (revision) {
		case 1:
			dev_type = TX_TI_R1_INIT;
			break;

		default:
			break;
		}
	} else {
		switch (revision) {
		case 1:
			dev_type = RX_TI_R1_INIT;
			break;
		default:
			break;
		}
	}

	i = dev_type;

	while (dev_type == ti_tmds1204tx_regs[i].dev_type) {
		ret = ti_tmds1204tx_write_reg(priv, ti_tmds1204tx_regs[i].addr,
				       ti_tmds1204tx_regs[i].val);

		if (ret)
			return ret;

		i++;
	}

	return ret;
}

static int ti_tmds1204tx_probe(struct i2c_client *client)
{
	int ret;

	/* initialize ti_tmds1204 */
	txdata = devm_kzalloc(&client->dev, sizeof(*txdata), GFP_KERNEL);
	if (!txdata)
		return -ENOMEM;

	txdata->client = client;
	mutex_init(&txdata->lock);

	/* initialize regmap */
	txdata->regmap = devm_regmap_init_i2c(client, &ti_tmds1204tx_regmap_config);
	if (IS_ERR(txdata->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(txdata->regmap));
		ret = -ENODEV;
		goto err_regmap;
	}

	i2c_set_clientdata(client, txdata);

	dev_dbg(&client->dev, "init ti_tmds1204-tx\n");
	ret = ti_tmds1204tx_init(txdata, 1, true);
	if (ret) {
		dev_err(&client->dev, "failed to init ti_tmds1204-tx\n");
		return ret;
	}
	return 0;

err_regmap:
	mutex_destroy(&txdata->lock);
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int ti_tmds1204tx_remove(struct i2c_client *client)
{
	return 0;
}
#else
static void ti_tmds1204tx_remove(struct i2c_client *client)
{
	return;
}
#endif

static const struct of_device_id ti_tmds1204tx_of_id_table[] = {
	{ .compatible = "ti_tmds1204,ti_tmds1204-tx" },
	{ }
};
MODULE_DEVICE_TABLE(of, ti_tmds1204tx_of_id_table);

static const struct i2c_device_id ti_tmds1204tx_id[] = {
	{ "ti_tmds1204tx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ti_tmds1204tx_id);

static struct i2c_driver ti_tmds1204tx_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table	= ti_tmds1204tx_of_id_table,
	},
	.probe		= ti_tmds1204tx_probe,
	.remove		= ti_tmds1204tx_remove,
	.id_table	= ti_tmds1204tx_id,
};

void ti_tmds1204tx_exit(void)
{
	i2c_del_driver(&ti_tmds1204tx_i2c_driver);
}
EXPORT_SYMBOL_GPL(ti_tmds1204tx_exit);

int ti_tmds1204tx_entry(void)
{
	return i2c_add_driver(&ti_tmds1204tx_i2c_driver);
}
EXPORT_SYMBOL_GPL(ti_tmds1204tx_entry);

MODULE_DESCRIPTION("TI TMDS1204 retimer chip driver");
MODULE_LICENSE("GPL v2");
