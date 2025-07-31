/* SPDX-License-Identifier: MIT */
/*
 * Copyright 2025 Advanced Micro Devices, Inc. All Rights Reserved.
 */

/*****************************************************************************/
/*
 *
 * @file x_vfmc.h
 *
 * This is the main header file for Xilinx HDMI FMC modules is used
 * for board type macros and global functions
 */
/*****************************************************************************/
#ifndef X_VFMC_H_
#define X_VFMC_H_	/*
			 * Prevent circular inclusions
			 * by using protection macros
			 */

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/

/************************** Constant Definitions *****************************/
enum base_board {
	BASE_BOARD_VERSAL_EDGE = 0, /* It represents both VEK385 and VEK280 */
	BASE_BOARD_ZCU102,
	BASE_BOARD_ZCU106,
	BASE_BOARD_VCK190,
};

const char *board_type_str(u8 board_type);

/* FMC Specific Information related functions */
int fmc64_tx_refclk_sel(unsigned int clk_sel);
int fmc64_rx_refclk_sel(unsigned int clk_sel);
int fmc65_tx_refclk_sel(unsigned int clk_sel);
int fmc64_rx_refclk_sel(unsigned int clk_sel);

int fmc_entry(void);
void fmc_exit(void);

int fmc64_entry(void);
void fmc64_exit(void);

int fmc74_entry(void);
void fmc74_exit(void);

int fmc65_entry(void);
void fmc65_exit(void);

int si5344_entry(void);
void si5344_exit(void);

int tipower_entry(void);
int tipower_init(void);
void tipower_exit(void);

int idt_entry(void);
void idt_exit(void);

int ti_tmds1204tx_entry(void);
int ti_tmds1204rx_entry(void);
u8 xfmcdev_board_type(void);

int onsemirx_linerate_conf(u8 is_frl, u64 line_rate, u8 is_tx);
int onsemirx_entry(void);
void onsemirx_exit(void);

int onsemitx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx);
int onsemitx_entry(void);
void onsemitx_exit(void);

int ti_tmds1204tx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx, u8 lanes);
int ti_tmds1204tx_entry(void);
void ti_tmds1204tx_exit(void);

int ti_tmds1204rx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx, u8 lanes);
int ti_tmds1204rx_entry(void);
void ti_tmds1204rx_exit(void);

/************************** Variable Declarations ****************************/

/************************** Variable Declarations ****************************/

#ifdef __cplusplus
}
#endif

#endif /* End of protection macro */
