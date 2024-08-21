// SPDX-License-Identifier: GPL-2.0
/*
 * ONSEMI NB7NQ621M cable redriver driver
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

#define DRIVER_NAME "onsemi-rx"

void onsemirx_exit(void);
int onsemirx_entry(void);
int onsemirx_linerate_conf(u8 is_frl, u64 LineRate, u8 is_tx);

#define to_onsemirx(_hw)	container_of(_hw, struct onsemirx, hw)
struct onsemirx *os_rxdata;

typedef enum {
	TX_R0_TMDS = 0,
	TX_R0_TMDS_14_L = 21,
	TX_R0_TMDS_14_H = 33,
	TX_R0_TMDS_20 = 45,
	TX_R0_FRL = 57,
	RX_R0 = 69,
	TX_R1_TMDS_14_LL = 90,
	TX_R1_TMDS_14_L = 99,
	TX_R1_TMDS_14 = 108,	/* HDMI 1.4 */
	TX_R1_TMDS_20 = 117,	/* HDMI 2.0 */
	TX_R1_FRL = 126,
	TX_R1_FRL_10G = 135,
	TX_R1_FRL_12G = 144,
	RX_R1_TMDS_14 = 153,
	RX_R1_TMDS_20 = 162,
	RX_R1_FRL = 171,
	TX_R2_TMDS_14_L = 180,
	TX_R2_TMDS_14_H = 196,
	TX_R2_TMDS_20 = 208,
	TX_R2_FRL = 220,
	RX_R2_TMDS_14 = 232,
	RX_R2_TMDS_20 = 241,
	RX_R2_FRL = 250,
	/* Above these were all early versions of
	 * OnSemi re-driver
	 * All the 21 write registers are added for flexibility
	 */
	TX_R3_TMDS_14_L = 271,
	TX_R3_TMDS_14_H = TX_R3_TMDS_14_L + 21,
	TX_R3_TMDS_20 = TX_R3_TMDS_14_H + 21,
	TX_R3_FRL = TX_R3_TMDS_20 + 21,
	RX_R3_TMDS_14 = TX_R3_FRL + 21,
	RX_R3_TMDS_20 = RX_R3_TMDS_14 + 21,
	RX_R3_FRL = RX_R3_TMDS_20 + 21,
} Onsemi_DeviceType;

typedef struct {
	u16 dev_type;
	u8 Address;
	u8 Values;
} Onsemi_RegisterField;

/*
 * This table contains the values to be programmed to ONSEMI device.
 * Each entry is of the format:
 * 1) Device Type
 * 2) Register Address
 * 3) Values
 */
const Onsemi_RegisterField onsemirx_regs[] = {
	{TX_R0_TMDS, 0x04, 0x18},
	{TX_R0_TMDS, 0x05, 0x0B},
	{TX_R0_TMDS, 0x06, 0x00},
	{TX_R0_TMDS, 0x07, 0x00},
	{TX_R0_TMDS, 0x08, 0x03},
	{TX_R0_TMDS, 0x09, 0x20},
	{TX_R0_TMDS, 0x0A, 0x05},
	{TX_R0_TMDS, 0x0B, 0x0F},
	{TX_R0_TMDS, 0x0C, 0xAA},
	{TX_R0_TMDS, 0x0D, 0x00},
	{TX_R0_TMDS, 0x0E, 0x03},
	{TX_R0_TMDS, 0x0F, 0x00},
	{TX_R0_TMDS, 0x10, 0x00},
	{TX_R0_TMDS, 0x11, 0x03},
	{TX_R0_TMDS, 0x12, 0x00},
	{TX_R0_TMDS, 0x13, 0x00},
	{TX_R0_TMDS, 0x14, 0x03},
	{TX_R0_TMDS, 0x15, 0x00},
	{TX_R0_TMDS, 0x16, 0x00},
	{TX_R0_TMDS, 0x17, 0x03},
	{TX_R0_TMDS, 0x18, 0x00},

	{TX_R0_TMDS_14_L, 0x04, 0xB0},
	{TX_R0_TMDS_14_L, 0x09, 0x00},
	{TX_R0_TMDS_14_L, 0x0A, 0x03},
	{TX_R0_TMDS_14_L, 0x0D, 0x02},
	{TX_R0_TMDS_14_L, 0x0E, 0x0F},
	{TX_R0_TMDS_14_L, 0x10, 0x02},
	{TX_R0_TMDS_14_L, 0x11, 0x0F},
	{TX_R0_TMDS_14_L, 0x13, 0x02},
	{TX_R0_TMDS_14_L, 0x14, 0x0F},
	{TX_R0_TMDS_14_L, 0x16, 0x02},
	{TX_R0_TMDS_14_L, 0x17, 0x63},
	{TX_R0_TMDS_14_L, 0x18, 0x0B},

	{TX_R0_TMDS_14_H, 0x04, 0xA0},
	{TX_R0_TMDS_14_H, 0x09, 0x00},
	{TX_R0_TMDS_14_H, 0x0A, 0x03},
	{TX_R0_TMDS_14_H, 0x0D, 0x30},
	{TX_R0_TMDS_14_H, 0x0E, 0x0F},
	{TX_R0_TMDS_14_H, 0x10, 0x30},
	{TX_R0_TMDS_14_H, 0x11, 0x0F},
	{TX_R0_TMDS_14_H, 0x13, 0x30},
	{TX_R0_TMDS_14_H, 0x14, 0x0F},
	{TX_R0_TMDS_14_H, 0x16, 0x02},
	{TX_R0_TMDS_14_H, 0x17, 0x63},
	{TX_R0_TMDS_14_H, 0x18, 0x0B},

	{TX_R0_TMDS_20, 0x04, 0xA0},
	{TX_R0_TMDS_20, 0x09, 0x00},
	{TX_R0_TMDS_20, 0x0A, 0x03},
	{TX_R0_TMDS_20, 0x0D, 0x31},
	{TX_R0_TMDS_20, 0x0E, 0x0F},
	{TX_R0_TMDS_20, 0x10, 0x31},
	{TX_R0_TMDS_20, 0x11, 0x0F},
	{TX_R0_TMDS_20, 0x13, 0x31},
	{TX_R0_TMDS_20, 0x14, 0x0F},
	{TX_R0_TMDS_20, 0x16, 0x02},
	{TX_R0_TMDS_20, 0x17, 0x63},
	{TX_R0_TMDS_20, 0x18, 0x0B},

	{TX_R0_FRL, 0x04, 0x18},
	{TX_R0_FRL, 0x09, 0x20},
	{TX_R0_FRL, 0x0A, 0x05},
	{TX_R0_FRL, 0x0D, 0x00},
	{TX_R0_FRL, 0x0E, 0x03},
	{TX_R0_FRL, 0x10, 0x00},
	{TX_R0_FRL, 0x11, 0x03},
	{TX_R0_FRL, 0x13, 0x00},
	{TX_R0_FRL, 0x14, 0x03},
	{TX_R0_FRL, 0x16, 0x00},
	{TX_R0_FRL, 0x17, 0x03},
	{TX_R0_FRL, 0x18, 0x00},

	{RX_R0, 0x04, 0xB0},
	{RX_R0, 0x05, 0x0D},
	{RX_R0, 0x06, 0x00},
	{RX_R0, 0x07, 0x32},
	{RX_R0, 0x08, 0x0B},
	{RX_R0, 0x09, 0x32},
	{RX_R0, 0x0A, 0x0B},
	{RX_R0, 0x0B, 0x0F},
	{RX_R0, 0x0C, 0xAA},
	{RX_R0, 0x0D, 0x00},
	{RX_R0, 0x0E, 0x03},
	{RX_R0, 0x0F, 0x00},
	{RX_R0, 0x10, 0x00},
	{RX_R0, 0x11, 0x03},
	{RX_R0, 0x12, 0x00},
	{RX_R0, 0x13, 0x00},
	{RX_R0, 0x14, 0x03},
	{RX_R0, 0x15, 0x00},
	{RX_R0, 0x16, 0x00},
	{RX_R0, 0x17, 0x03},
	{RX_R0, 0x18, 0x00},

	/* <= 74.25Mbps */
	{TX_R1_TMDS_14_LL, 0x0A, 0x18},
	{TX_R1_TMDS_14_LL, 0x0B, 0x1F},
	{TX_R1_TMDS_14_LL, 0x0C, 0x00},
	{TX_R1_TMDS_14_LL, 0x0D, 0x30},
	{TX_R1_TMDS_14_LL, 0x0E, 0x05},
	{TX_R1_TMDS_14_LL, 0x0F, 0x20},
	{TX_R1_TMDS_14_LL, 0x10, 0x43},
	{TX_R1_TMDS_14_LL, 0x11, 0x0F},
	{TX_R1_TMDS_14_LL, 0x12, 0xAA},

	/* <= 99Mbps */
	{TX_R1_TMDS_14_L, 0x0A, 0x00},
	{TX_R1_TMDS_14_L, 0x0B, 0x1F},
	{TX_R1_TMDS_14_L, 0x0C, 0x00},
	{TX_R1_TMDS_14_L, 0x0D, 0x10},
	{TX_R1_TMDS_14_L, 0x0E, 0x2A},
	{TX_R1_TMDS_14_L, 0x0F, 0x11},
	{TX_R1_TMDS_14_L, 0x10, 0x43},
	{TX_R1_TMDS_14_L, 0x11, 0x0F},
	{TX_R1_TMDS_14_L, 0x12, 0xAA},

	/* <= 1.48Gbps */
	{TX_R1_TMDS_14, 0x0A, 0x18},
	{TX_R1_TMDS_14, 0x0B, 0x1F},
	{TX_R1_TMDS_14, 0x0C, 0x0D},
	{TX_R1_TMDS_14, 0x0D, 0x10},
	{TX_R1_TMDS_14, 0x0E, 0x2A},
	{TX_R1_TMDS_14, 0x0F, 0x11},
	{TX_R1_TMDS_14, 0x10, 0x43},
	{TX_R1_TMDS_14, 0x11, 0x0F},
	{TX_R1_TMDS_14, 0x12, 0xAA},

	/* <= 5.94 */
	{TX_R1_TMDS_20, 0x0A, 0x18},
	{TX_R1_TMDS_20, 0x0B, 0x0F},
	{TX_R1_TMDS_20, 0x0C, 0x00},
	{TX_R1_TMDS_20, 0x0D, 0x10},
	{TX_R1_TMDS_20, 0x0E, 0x2A},
	{TX_R1_TMDS_20, 0x0F, 0x33},
	{TX_R1_TMDS_20, 0x10, 0x0A},
	{TX_R1_TMDS_20, 0x11, 0x0F},
	{TX_R1_TMDS_20, 0x12, 0xAA},

	{TX_R1_FRL, 0x0A, 0x20},
	{TX_R1_FRL, 0x0B, 0x0F},
	{TX_R1_FRL, 0x0C, 0x00},
	{TX_R1_FRL, 0x0D, 0x10},
	{TX_R1_FRL, 0x0E, 0x2A},
	{TX_R1_FRL, 0x0F, 0x11},
	{TX_R1_FRL, 0x10, 0x0A},
	{TX_R1_FRL, 0x11, 0x0F},
	{TX_R1_FRL, 0x12, 0xAA},

	{TX_R1_FRL_10G, 0x0A, 0x20},
	{TX_R1_FRL_10G, 0x0B, 0x0F},
	{TX_R1_FRL_10G, 0x0C, 0x00},
	{TX_R1_FRL_10G, 0x0D, 0x00},
	{TX_R1_FRL_10G, 0x0E, 0x03},
	{TX_R1_FRL_10G, 0x0F, 0x21},
	{TX_R1_FRL_10G, 0x10, 0x0A},
	{TX_R1_FRL_10G, 0x11, 0x0F},
	{TX_R1_FRL_10G, 0x12, 0xAA},

	{TX_R1_FRL_12G, 0x0A, 0x20},
	{TX_R1_FRL_12G, 0x0B, 0x0F},
	{TX_R1_FRL_12G, 0x0C, 0x00},
	{TX_R1_FRL_12G, 0x0D, 0x00},
	{TX_R1_FRL_12G, 0x0E, 0x03},
#ifdef BASE_BOARD_ZCU106
	{TX_R1_FRL_12G, 0x0F, 0x21},
#else
	{TX_R1_FRL_12G, 0x0F, 0x31},
#endif
	{TX_R1_FRL_12G, 0x10, 0x0A},
	{TX_R1_FRL_12G, 0x11, 0x0F},
	{TX_R1_FRL_12G, 0x12, 0xAA},

	{RX_R1_TMDS_14, 0x0A, 0x20},
	{RX_R1_TMDS_14, 0x0B, 0x0F},
	{RX_R1_TMDS_14, 0x0C, 0x00},
	{RX_R1_TMDS_14, 0x0D, 0x00},
	{RX_R1_TMDS_14, 0x0E, 0x03},
	{RX_R1_TMDS_14, 0x0F, 0x21},
	{RX_R1_TMDS_14, 0x10, 0x2A},
	{RX_R1_TMDS_14, 0x11, 0x0F},
	{RX_R1_TMDS_14, 0x12, 0xAA},

	{RX_R1_TMDS_20, 0x0A, 0x20},
	{RX_R1_TMDS_20, 0x0B, 0x0F},
	{RX_R1_TMDS_20, 0x0C, 0x00},
	{RX_R1_TMDS_20, 0x0D, 0x00},
	{RX_R1_TMDS_20, 0x0E, 0x03},
	{RX_R1_TMDS_20, 0x0F, 0x00},
	{RX_R1_TMDS_20, 0x10, 0x00},
	{RX_R1_TMDS_20, 0x11, 0x0F},
	{RX_R1_TMDS_20, 0x12, 0xAA},

	{RX_R1_FRL, 0x0A, 0x20},
	{RX_R1_FRL, 0x0B, 0x0F},
	{RX_R1_FRL, 0x0C, 0x00},
	{RX_R1_FRL, 0x0D, 0x00},
	{RX_R1_FRL, 0x0E, 0x07},
	{RX_R1_FRL, 0x0F, 0x20},
	{RX_R1_FRL, 0x10, 0x01},
	{RX_R1_FRL, 0x11, 0x0F},
	{RX_R1_FRL, 0x12, 0xAA},

	{TX_R2_TMDS_14_L, 0x09, 0x7C},
	{TX_R2_TMDS_14_L, 0x0A, 0x00},
	{TX_R2_TMDS_14_L, 0x0B, 0x0F},
	{TX_R2_TMDS_14_L, 0x0C, 0x00},
	{TX_R2_TMDS_14_L, 0x0D, 0x20},
	{TX_R2_TMDS_14_L, 0x0E, 0x43},
	{TX_R2_TMDS_14_L, 0x0F, 0x20},
	{TX_R2_TMDS_14_L, 0x10, 0x43},
	{TX_R2_TMDS_14_L, 0x11, 0x0F},
	{TX_R2_TMDS_14_L, 0x12, 0xAA},
	{TX_R2_TMDS_14_L, 0x13, 0x02},
	{TX_R2_TMDS_14_L, 0x14, 0x0F},
	{TX_R2_TMDS_14_L, 0x15, 0x00},
	{TX_R2_TMDS_14_L, 0x16, 0x02},
	{TX_R2_TMDS_14_L, 0x17, 0x63},
	{TX_R2_TMDS_14_L, 0x18, 0x0B},

	{TX_R2_TMDS_14_H, 0x09, 0x7C},
	{TX_R2_TMDS_14_H, 0x0A, 0x18},
	{TX_R2_TMDS_14_H, 0x0B, 0x0F},
	{TX_R2_TMDS_14_H, 0x0D, 0x00},
	{TX_R2_TMDS_14_H, 0x0E, 0x43},
	{TX_R2_TMDS_14_H, 0x0F, 0x00},
	{TX_R2_TMDS_14_H, 0x10, 0x47},
	{TX_R2_TMDS_14_H, 0x13, 0x30},
	{TX_R2_TMDS_14_H, 0x14, 0x0F},
	{TX_R2_TMDS_14_H, 0x16, 0x02},
	{TX_R2_TMDS_14_H, 0x17, 0x63},
	{TX_R2_TMDS_14_H, 0x18, 0x0B},

	{TX_R2_TMDS_20, 0x09, 0x7C},
	{TX_R2_TMDS_20, 0x0A, 0x18},
	{TX_R2_TMDS_20, 0x0B, 0x0F},
	{TX_R2_TMDS_20, 0x0D, 0x00},
	{TX_R2_TMDS_20, 0x0E, 0x43},
	{TX_R2_TMDS_20, 0x0F, 0x11},
	{TX_R2_TMDS_20, 0x10, 0x28},
	{TX_R2_TMDS_20, 0x13, 0x30},
	{TX_R2_TMDS_20, 0x14, 0x0F},
	{TX_R2_TMDS_20, 0x16, 0x02},
	{TX_R2_TMDS_20, 0x17, 0x63},
	{TX_R2_TMDS_20, 0x18, 0x0B},

	{TX_R2_FRL, 0x09, 0x7C},
	{TX_R2_FRL, 0x0A, 0x20},
	{TX_R2_FRL, 0x0B, 0x0F},
#ifdef BASE_BOARD_ZCU106
	{TX_R2_FRL, 0x0D, 0x00}, /* Onsemi 0x10}, */
	{TX_R2_FRL, 0x0E, 0x0A}, /* Onsemi 0x2A}, */
	{TX_R2_FRL, 0x0F, 0x31}, /* Onsemi 0x02}, */
	{TX_R2_FRL, 0x10, 0x05},
#elif defined BASE_BOARD_VCK190
	{TX_R2_FRL, 0x0D, 0x00},
	{TX_R2_FRL, 0x0E, 0x0A},
	{TX_R2_FRL, 0x0F, 0x31},
	{TX_R2_FRL, 0x10, 0x00},
#else
	{TX_R2_FRL, 0x0D, 0x33},
	{TX_R2_FRL, 0x0E, 0x0A},
	{TX_R2_FRL, 0x0F, 0x33},
	{TX_R2_FRL, 0x10, 0x05},
#endif
	{TX_R2_FRL, 0x13, 0x00},
	{TX_R2_FRL, 0x14, 0x03},
	{TX_R2_FRL, 0x16, 0x00},
	{TX_R2_FRL, 0x17, 0x03},
	{TX_R2_FRL, 0x18, 0x00},

	{RX_R2_TMDS_14, 0x0A, 0x20},
	{RX_R2_TMDS_14, 0x0B, 0x0F},
	{RX_R2_TMDS_14, 0x0C, 0x00},
	{RX_R2_TMDS_14, 0x0D, 0x00},
	{RX_R2_TMDS_14, 0x0E, 0x03},
	{RX_R2_TMDS_14, 0x0F, 0x21},
	{RX_R2_TMDS_14, 0x10, 0x2A},
	{RX_R2_TMDS_14, 0x11, 0x0F},
	{RX_R2_TMDS_14, 0x12, 0xAA},

	{RX_R2_TMDS_20, 0x0A, 0x20},
	{RX_R2_TMDS_20, 0x0B, 0x0F},
	{RX_R2_TMDS_20, 0x0C, 0x00},
	{RX_R2_TMDS_20, 0x0D, 0x00},
	{RX_R2_TMDS_20, 0x0E, 0x03},
	{RX_R2_TMDS_20, 0x0F, 0x00},
	{RX_R2_TMDS_20, 0x10, 0x00},
	{RX_R2_TMDS_20, 0x11, 0x0F},
	{RX_R2_TMDS_20, 0x12, 0xAA},

	{RX_R2_FRL, 0x0A, 0xA0},
	{RX_R2_FRL, 0x0B, 0x0F},
	{RX_R2_FRL, 0x0C, 0x00},
	{RX_R2_FRL, 0x0D, 0x20},
	{RX_R2_FRL, 0x0E, 0x07},
	{RX_R2_FRL, 0x0F, 0x20},
	{RX_R2_FRL, 0x10, 0x00},
	{RX_R2_FRL, 0x11, 0x0F},
	{RX_R2_FRL, 0x12, 0xAA},
	{RX_R2_FRL, 0x13, 0x20},
	{RX_R2_FRL, 0x14, 0x00},
	{RX_R2_FRL, 0x15, 0x00},
	{RX_R2_FRL, 0x16, 0x21},
	{RX_R2_FRL, 0x17, 0x00},
	{RX_R2_FRL, 0x18, 0x00},
	{RX_R2_FRL, 0x19, 0x20},
	{RX_R2_FRL, 0x1A, 0x00},
	{RX_R2_FRL, 0x1B, 0x00},
#ifdef BASE_BOARD_ZCU106
	{RX_R2_FRL, 0x1C, 0x03},
	{RX_R2_FRL, 0x1D, 0x00},
#else
	{RX_R2_FRL, 0x1C, 0x20},
	{RX_R2_FRL, 0x1D, 0x07},
#endif
	{RX_R2_FRL, 0x1E, 0x00},

	{TX_R3_TMDS_14_L, 0x0A, 0x1C},
	{TX_R3_TMDS_14_L, 0x0B, 0x0F},
	{TX_R3_TMDS_14_L, 0x0C, 0x0B},
	{TX_R3_TMDS_14_L, 0x0D, 0x30},
	{TX_R3_TMDS_14_L, 0x0E, 0x4A},
	{TX_R3_TMDS_14_L, 0x0F, 0x30},
	{TX_R3_TMDS_14_L, 0x10, 0x4A},
	{TX_R3_TMDS_14_L, 0x11, 0x0F},
	{TX_R3_TMDS_14_L, 0x12, 0xAA},
	{TX_R3_TMDS_14_L, 0x13, 0x30},
	{TX_R3_TMDS_14_L, 0x14, 0x0F},
	{TX_R3_TMDS_14_L, 0x15, 0x00},
	{TX_R3_TMDS_14_L, 0x16, 0x02},
	{TX_R3_TMDS_14_L, 0x17, 0x63},
	{TX_R3_TMDS_14_L, 0x18, 0x0B},
	{TX_R3_TMDS_14_L, 0x19, 0x00},
	{TX_R3_TMDS_14_L, 0x1A, 0x03},
	{TX_R3_TMDS_14_L, 0x1B, 0x00},
	{TX_R3_TMDS_14_L, 0x1C, 0x00},
	{TX_R3_TMDS_14_L, 0x1D, 0x03},
	{TX_R3_TMDS_14_L, 0x1E, 0x00},

	{TX_R3_TMDS_14_H, 0x0A, 0x1C},
	{TX_R3_TMDS_14_H, 0x0B, 0x0F},
	{TX_R3_TMDS_14_H, 0x0C, 0x0B},
	{TX_R3_TMDS_14_H, 0x0D, 0x30},
	{TX_R3_TMDS_14_H, 0x0E, 0x4A},
	{TX_R3_TMDS_14_H, 0x0F, 0x30},
	{TX_R3_TMDS_14_H, 0x10, 0x4A},
	{TX_R3_TMDS_14_H, 0x11, 0x0F},
	{TX_R3_TMDS_14_H, 0x12, 0xAA},
	{TX_R3_TMDS_14_H, 0x13, 0x30},
	{TX_R3_TMDS_14_H, 0x14, 0x0F},
	{TX_R3_TMDS_14_H, 0x15, 0x00},
	{TX_R3_TMDS_14_H, 0x16, 0x02},
	{TX_R3_TMDS_14_H, 0x17, 0x63},
	{TX_R3_TMDS_14_H, 0x18, 0x0B},
	{TX_R3_TMDS_14_H, 0x19, 0x00},
	{TX_R3_TMDS_14_H, 0x1A, 0x03},
	{TX_R3_TMDS_14_H, 0x1B, 0x00},
	{TX_R3_TMDS_14_H, 0x1C, 0x00},
	{TX_R3_TMDS_14_H, 0x1D, 0x03},
	{TX_R3_TMDS_14_H, 0x1E, 0x00},

	{TX_R3_TMDS_20, 0x0A, 0x1C},
	{TX_R3_TMDS_20, 0x0B, 0x0F},
	{TX_R3_TMDS_20, 0x0C, 0x00},
	{TX_R3_TMDS_20, 0x0D, 0x30},
	{TX_R3_TMDS_20, 0x0E, 0x4A},
	{TX_R3_TMDS_20, 0x0F, 0x30},
	{TX_R3_TMDS_20, 0x10, 0x4A},
	{TX_R3_TMDS_20, 0x11, 0x0F},
	{TX_R3_TMDS_20, 0x12, 0xAA},
	{TX_R3_TMDS_20, 0x13, 0x02},
	{TX_R3_TMDS_20, 0x14, 0x0F},
	{TX_R3_TMDS_20, 0x15, 0x00},
	{TX_R3_TMDS_20, 0x16, 0x02},
	{TX_R3_TMDS_20, 0x17, 0x63},
	{TX_R3_TMDS_20, 0x18, 0x0B},
	{TX_R3_TMDS_20, 0x19, 0x00},
	{TX_R3_TMDS_20, 0x1A, 0x03},
	{TX_R3_TMDS_20, 0x1B, 0x00},
	{TX_R3_TMDS_20, 0x1C, 0x00},
	{TX_R3_TMDS_20, 0x1D, 0x03},
	{TX_R3_TMDS_20, 0x1E, 0x00},

	{TX_R3_FRL, 0x0A, 0x24},
	{TX_R3_FRL, 0x0B, 0x0D},
	{TX_R3_FRL, 0x0C, 0x00},
#ifdef BASE_BOARD_ZCU106
	{TX_R3_FRL, 0x0D, 0x31},
	{TX_R3_FRL, 0x0E, 0x0A},
	{TX_R3_FRL, 0x0F, 0x31},
	{TX_R3_FRL, 0x10, 0x05},
#elif defined BASE_BOARD_ZCU102
	{TX_R3_FRL, 0x0D, 0x10},
	{TX_R3_FRL, 0x0E, 0x2A},
	{TX_R3_FRL, 0x0F, 0x31},
	{TX_R3_FRL, 0x10, 0x05},
#elif defined BASE_BOARD_VCU118
	{TX_R3_FRL, 0x0D, 0x30},
	{TX_R3_FRL, 0x0E, 0x00},
	{TX_R3_FRL, 0x0F, 0x30},
	{TX_R3_FRL, 0x10, 0x00},
#elif defined BASE_BOARD_VCK190
	{TX_R3_FRL, 0x0D, 0x31},
	{TX_R3_FRL, 0x0E, 0x0A},
	{TX_R3_FRL, 0x0F, 0x31},
	{TX_R3_FRL, 0x10, 0x00},
#endif
	{TX_R3_FRL, 0x11, 0x0F},
	{TX_R3_FRL, 0x12, 0xAA},
	{TX_R3_FRL, 0x13, 0x00},
	{TX_R3_FRL, 0x14, 0x03},
	{TX_R3_FRL, 0x15, 0x00},
	{TX_R3_FRL, 0x16, 0x00},
	{TX_R3_FRL, 0x17, 0x03},
	{TX_R3_FRL, 0x18, 0x00},
	{TX_R3_FRL, 0x19, 0x00},
	{TX_R3_FRL, 0x1A, 0x03},
	{TX_R3_FRL, 0x1B, 0x00},
	{TX_R3_FRL, 0x1C, 0x00},
	{TX_R3_FRL, 0x1D, 0x03},
	{TX_R3_FRL, 0x1E, 0x00},

	{RX_R3_TMDS_14, 0x0A, 0x34},
	{RX_R3_TMDS_14, 0x0B, 0x0D},
	{RX_R3_TMDS_14, 0x0C, 0x00},
	{RX_R3_TMDS_14, 0x0D, 0x00},
	{RX_R3_TMDS_14, 0x0E, 0x03},
	{RX_R3_TMDS_14, 0x0F, 0x21},
	{RX_R3_TMDS_14, 0x10, 0x2A},
	{RX_R3_TMDS_14, 0x11, 0x0F},
	{RX_R3_TMDS_14, 0x12, 0x00},
	{RX_R3_TMDS_14, 0x13, 0x00},
	{RX_R3_TMDS_14, 0x14, 0x03},
	{RX_R3_TMDS_14, 0x15, 0x00},
	{RX_R3_TMDS_14, 0x16, 0x00},
	{RX_R3_TMDS_14, 0x17, 0x03},
	{RX_R3_TMDS_14, 0x18, 0x00},
	{RX_R3_TMDS_14, 0x19, 0x00},
	{RX_R3_TMDS_14, 0x1A, 0x03},
	{RX_R3_TMDS_14, 0x1B, 0x00},
	{RX_R3_TMDS_14, 0x1C, 0x00},
	{RX_R3_TMDS_14, 0x1D, 0x03},
	{RX_R3_TMDS_14, 0x1E, 0x00},

	{RX_R3_TMDS_20, 0x0A, 0x34},
	{RX_R3_TMDS_20, 0x0B, 0x0D},
	{RX_R3_TMDS_20, 0x0C, 0x00},
	{RX_R3_TMDS_20, 0x0D, 0x00},
	{RX_R3_TMDS_20, 0x0E, 0x03},
	{RX_R3_TMDS_20, 0x0F, 0x00},
	{RX_R3_TMDS_20, 0x10, 0x00},
	{RX_R3_TMDS_20, 0x11, 0x0F},
	{RX_R3_TMDS_20, 0x12, 0x00},
	{RX_R3_TMDS_20, 0x13, 0x00},
	{RX_R3_TMDS_20, 0x14, 0x03},
	{RX_R3_TMDS_20, 0x15, 0x00},
	{RX_R3_TMDS_20, 0x16, 0x00},
	{RX_R3_TMDS_20, 0x17, 0x03},
	{RX_R3_TMDS_20, 0x18, 0x00},
	{RX_R3_TMDS_20, 0x19, 0x00},
	{RX_R3_TMDS_20, 0x1A, 0x03},
	{RX_R3_TMDS_20, 0x1B, 0x00},
	{RX_R3_TMDS_20, 0x1C, 0x00},
	{RX_R3_TMDS_20, 0x1D, 0x03},
	{RX_R3_TMDS_20, 0x1E, 0x00},

#ifdef BASE_BOARD_VCU118
	{RX_R3_FRL, 0x0A, 0xA4},
#else
	{RX_R3_FRL, 0x0A, 0x24},
#endif
	{RX_R3_FRL, 0x0B, 0x0D},
	{RX_R3_FRL, 0x0C, 0x00},
	{RX_R3_FRL, 0x0D, 0x20},
	{RX_R3_FRL, 0x0E, 0x07},
#ifdef BASE_BOARD_VCU118
	{RX_R3_FRL, 0x0F, 0x21},
	{RX_R3_FRL, 0x10, 0x00},
#else
	{RX_R3_FRL, 0x0F, 0x20},
	{RX_R3_FRL, 0x10, 0x00},
#endif
	{RX_R3_FRL, 0x11, 0x0F},
	{RX_R3_FRL, 0x12, 0xAA},
#ifdef BASE_BOARD_VCU118
	{RX_R3_FRL, 0x13, 0x00},
#else
	{RX_R3_FRL, 0x13, 0x21},
#endif
	{RX_R3_FRL, 0x14, 0x00},
	{RX_R3_FRL, 0x15, 0x00},
	{RX_R3_FRL, 0x16, 0x21},
	{RX_R3_FRL, 0x17, 0x00},
	{RX_R3_FRL, 0x18, 0x00},
	{RX_R3_FRL, 0x19, 0x21},
	{RX_R3_FRL, 0x1A, 0x00},
	{RX_R3_FRL, 0x1B, 0x00},

	{RX_R3_FRL, 0x1C, 0x20},
	{RX_R3_FRL, 0x1D, 0x07},

	{RX_R3_FRL, 0x1E, 0x00},

};

static const struct regmap_config onsemirx_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

/*
 * struct onsemirx - onsemi device structure
 * @client: Pointer to I2C client
 * @ctrls: onsemi control structure
 * @regmap: Pointer to regmap structure
 * @lock: Mutex structure
 * @mode_index: Resolution mode index
 */
struct onsemirx {
	struct clk_hw hw;
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex lock; /* mutex lock for operations */
	u32 mode_index;
};

static inline int onsemirx_read_reg(struct onsemirx *priv, u8 addr, u8 *val)
{
	int err = 0;

	err = regmap_read(priv->regmap, addr, (unsigned int *)val);
	if (err)
		dev_dbg(&priv->client->dev,
			"i2c read failed, addr = %x\n", addr);
	return err;
}

static inline int onsemirx_write_reg(struct onsemirx *priv, u8 addr, u8 val)
{
	int err = 0;

	err = regmap_write(priv->regmap, addr, val);
	if (err) {
		dev_dbg(&priv->client->dev,
			"i2c write failed, addr = %x\n", addr);
	}

	return err;
}

int onsemirx_linerate_conf(u8 is_frl, u64 LineRate, u8 is_tx)
{
	u32 linerate_mbps;
	u32 i = 0;
	u16 dev_type = 0xffff;
	int ret = 1;
	u8 revision = 3; //onsemi tx-mezz- R3

	linerate_mbps = (u32)((u64) LineRate / 100000); //remove one zero
	printk("linerate %llu lineratembps %u \n\r",LineRate,linerate_mbps);
	/* TX */
	if (is_tx == 1) {
		switch (revision) {
		case 0:
			if (is_frl == 1) {
				dev_type = TX_R0_FRL;
			} else {
				/* HDMI 2.0 */
				if ((linerate_mbps > 3400) &&
						(linerate_mbps <= 6000)) {
					dev_type = TX_R0_TMDS_20;
				}
				/* HDMI 1.4 1.65-3.4 Gbps */
				else if ((linerate_mbps > 1650) &&
						(linerate_mbps <= 3400)) {
					dev_type = TX_R0_TMDS_14_H;
				}
				/* HDMI 1.4 0.25-1.65 Gbps */
				else {
					dev_type = TX_R0_TMDS_14_L;
				}
			}
			break;
		case 1:
			if (is_frl == 1) {
				if (linerate_mbps >= 12000) {
					dev_type = TX_R1_FRL_12G;
				} else if (linerate_mbps >= 10000) {
					dev_type = TX_R1_FRL_10G;
				} else {
					dev_type = TX_R1_FRL;
				}
			} else {
				/* HDMI 2.0 */
				if (linerate_mbps > 3400) {
					dev_type = TX_R1_TMDS_20;
				}
				/* HDMI 1.4 */
				else if ((linerate_mbps > 99) &&
						(linerate_mbps <= 3400)) {
					dev_type = TX_R1_TMDS_14;
				}
				else if ((linerate_mbps > 74.25) &&
						(linerate_mbps <= 99)) {
					dev_type = TX_R1_TMDS_14_L;
				} else {
					dev_type = TX_R1_TMDS_14_LL;
				}
			}
			break;
		case 2:
			if (is_frl == 1) {
				dev_type = TX_R2_FRL;
			} else {
				/* HDMI 2.0 */
				if ((linerate_mbps > 3400) &&
						(linerate_mbps <= 6000)) {
					dev_type = TX_R2_TMDS_20;
				}
				/* HDMI 1.4 1.65-3.4 Gbps */
				else if ((linerate_mbps > 1650) &&
						(linerate_mbps <= 3400)) {
					dev_type = TX_R2_TMDS_14_H;
				}
				/* HDMI 1.4 0.25-1.65 Gbps */
				else {
					dev_type = TX_R2_TMDS_14_L;
				}
			}
			break;
		case 3:
			if (is_frl == 1) {
				dev_type = TX_R3_FRL;
			} else {
				/* HDMI 2.0 */
				if ((linerate_mbps > 3400) &&
						(linerate_mbps <= 6000)) {
					dev_type = TX_R3_TMDS_20;
				}
				/* HDMI 1.4 1.65-3.4 Gbps */
				else if ((linerate_mbps > 1650) &&
						(linerate_mbps <= 3400)) {
					dev_type = TX_R3_TMDS_14_H;
				}
				/* HDMI 1.4 0.25-1.65 Gbps */
				else {
					dev_type = TX_R3_TMDS_14_L;
				}
			}
			break;

		default:
			break;
		}
	}
	/* RX */
	else {
		switch (revision) {
		case 0:
			/* dev_type = RX_R0; */
			break;
		case 1:
			if (is_frl == 1) {
				dev_type = RX_R1_FRL;
			} else {
				if (linerate_mbps > 3400) {
					dev_type = RX_R1_TMDS_20;
				} else {
					dev_type = RX_R1_TMDS_14;
				}
			}
			break;
		case 2:
			if (is_frl == 1) {
				dev_type = RX_R2_FRL;
			} else {

				if (linerate_mbps > 3400) {
					dev_type = RX_R2_TMDS_20;
				} else {
					dev_type = RX_R2_TMDS_14;
				}
			}
			break;
		case 3:
			if (is_frl == 1) {
				dev_type = RX_R3_FRL;
			} else {

				if (linerate_mbps > 3400) {
					dev_type = RX_R3_TMDS_20;
				} else {
					dev_type = RX_R3_TMDS_14;
				}
			}
			break;

		default:

			break;
		}
	}

	i = dev_type;
	while (dev_type == onsemirx_regs[i].dev_type) {
		ret = onsemirx_write_reg(os_rxdata, onsemirx_regs[i].Address,
				       onsemirx_regs[i].Values);
		if (ret) {
			return ret;
		}

		i++;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(onsemirx_linerate_conf);

static int onsemirx_init(struct onsemirx *priv, u8 revision, u8 is_tx)
{
	int ret = 1;
	u16 dev_type = 0xffff;
	u32 i = 0;

	if (is_tx == 1) {
		switch (revision) {
		case 0:
			dev_type = TX_R0_TMDS;
			break;
		case 1:
			dev_type = TX_R1_TMDS_14;
			break;
		case 2:
			dev_type = TX_R2_TMDS_14_L;
			break;
		case 3:
			dev_type = TX_R3_TMDS_14_L;
			break;

		default:
			break;
		}
	} else {
		switch (revision) {
		case 0:
			dev_type = RX_R0;
			break;
		case 1:
			dev_type = RX_R1_TMDS_14;
			break;
		case 2:
			dev_type = RX_R2_TMDS_14;
			break;
		case 3:
			dev_type = RX_R3_TMDS_14;
			break;

		default:
			break;
		}
	}

	i = dev_type;

	while (dev_type == onsemirx_regs[i].dev_type) {
		ret = onsemirx_write_reg(priv, onsemirx_regs[i].Address,
				       onsemirx_regs[i].Values);

		if (ret) {
			return ret;
		}

		i++;
	}

	return ret;
}

static int onsemirx_probe(struct i2c_client *client)
{
	struct clk_init_data init;
	int ret, err;
	u32 initial_fout;

	/* initialize onsemi */
	os_rxdata = devm_kzalloc(&client->dev, sizeof(*os_rxdata), GFP_KERNEL);
	if (!os_rxdata)
		return -ENOMEM;

	mutex_init(&os_rxdata->lock);

	if (of_property_read_string(client->dev.of_node, "clock-output-names",
			&init.name))
		init.name = client->dev.of_node->name;

	/* initialize regmap */
	os_rxdata->regmap = devm_regmap_init_i2c(client, &onsemirx_regmap_config);
	if (IS_ERR(os_rxdata->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(os_rxdata->regmap));
		ret = -ENODEV;
		goto err_regmap;
	}

	i2c_set_clientdata(client, os_rxdata);
	dev_dbg(&client->dev, "init onsemi-rx with default values \n");
	/* revision Pass4 Silicon, VFMC Active HDMI TX Mezz (R2) */
	ret = onsemirx_init(os_rxdata, 3, false);
	if (ret) {
		dev_err(&client->dev, "failed to init onsemi-rx \n");
		return ret;
	}

	/* Read the requested initial output frequency from device tree */
	if (!of_property_read_u32(client->dev.of_node, "clock-frequency",
				&initial_fout)) {
		err = clk_set_rate(os_rxdata->hw.clk, initial_fout);
		if (err) {
			of_clk_del_provider(client->dev.of_node);
			return err;
		}
	}

	return 0;

err_regmap:
	mutex_destroy(&os_rxdata->lock);
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int onsemirx_remove(struct i2c_client *client)
{
	return 0;
}
#else
static void onsemirx_remove(struct i2c_client *client)
{
	return;
}
#endif

static const struct of_device_id onsemirx_of_id_table[] = {
	{ .compatible = "onsemi,onsemi-rx" },
	{ }
};
MODULE_DEVICE_TABLE(of, onsemirx_of_id_table);

static const struct i2c_device_id onsemirx_id[] = {
	{ "onsemirx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, onsemirx_id);


static struct i2c_driver onsemirx_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table	= onsemirx_of_id_table,
	},
	.probe		= onsemirx_probe,
	.remove		= onsemirx_remove,
	.id_table	= onsemirx_id,
};

void onsemirx_exit(void)
{
	i2c_del_driver(&onsemirx_i2c_driver);
}
EXPORT_SYMBOL_GPL(onsemirx_exit);

int onsemirx_entry(void)
{
	return i2c_add_driver(&onsemirx_i2c_driver);
}
EXPORT_SYMBOL_GPL(onsemirx_entry);

MODULE_DESCRIPTION("ONSEMI NB7NQ621M cable redriver driver");
MODULE_LICENSE("GPL v2");
