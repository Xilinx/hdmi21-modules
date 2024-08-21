// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Video FMC driver
 *
 * Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 */
#define DEBUG
#define DEBUG_TRACE

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <dt-bindings/phy/phy.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

int onsemitx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx);
int fmc64_tx_refclk_sel(unsigned int clk_sel);
int fmc65_tx_refclk_sel(unsigned int clk_sel);
int onsemirx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx);
int fmc64_rx_refclk_sel(unsigned int clk_sel);
int ti_tmds1204tx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx, u8 lanes);
int ti_tmds1204rx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx, u8 lanes);

static int sel_mux(int direction, int clk_sel)
{
#ifndef BASE_BOARD_VEK280
	if (direction)
	{
		printk("%s:direction is tx: clk_sel: %d\n",__func__,clk_sel);
		fmc65_tx_refclk_sel(clk_sel);
		fmc64_tx_refclk_sel(clk_sel);
	} else {
		printk("%s:direction is rx: clk_sel: %d\n",__func__,clk_sel);
		fmc64_rx_refclk_sel(clk_sel);
	}

#endif
	return 0;
}

static int set_linerate(u8 direction, u8 is_frl, u64 linerate, u8 lanes)
{
	printk("%s:direction is tx: isfrl: %d linerate %llu lanes %d\n",
					__func__,is_frl,linerate,lanes);
	if (direction) {
		printk("%s:direction is tx: isfrl: %d linerate %llu lanes %d\n",
						__func__,is_frl,linerate,lanes);
#ifdef BASE_BOARD_VEK280
		ti_tmds1204tx_linerate_conf(is_frl, linerate, direction,lanes);
#else
		onsemitx_linerate_conf(is_frl, linerate, direction);
#endif
	} else {
		printk("%s:direction is rx: isfrl: %d linerate %llu lanes %d\n",
						__func__,is_frl,linerate,lanes);
#ifdef BASE_BOARD_VEK280
		ti_tmds1204rx_linerate_conf(is_frl, linerate, direction,lanes);
#else
		onsemirx_linerate_conf(is_frl, linerate, direction);
#endif

	}
	return 0;
}

struct x_vfmc_dev {
	struct device *dev;
	int val;
};

struct fmc_drv_data {
	const struct clk_config *clk;
};

struct clk_config {
	int (*sel_mux)(int, int);
	int (*set_linerate)(u8, u8, u64, u8);
};

int fmc_entry(void);
int fmc_exit(void);

int fmc64_entry(void);
int fmc64_exit(void);

int fmc74_entry(void);
int fmc74_exit(void);

int fmc65_entry(void);
int fmc65_exit(void);

int tipower_entry(void);
void tipower_exit(void);

int idt_entry(void);
int onsemitx_entry(void);
int onsemirx_entry(void);
int si5344_entry(void);
int ti_tmds1204tx_entry(void);
int ti_tmds1204rx_entry(void);

static inline void msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base * 1000, delay_base * 1000 + 500);
}

/**
 * xvfmc_probe - The device probe function for driver initialization.
 * @pdev: pointer to the platform device structure.
 *
 * Return: 0 for success and error value on failure
 */
static int xvfmc_probe(struct platform_device *pdev)
{
	struct x_vfmc_dev *xfmcdev;
	struct clk_config *priv_data;

	printk("%s %d\n",__func__,__LINE__);	

	xfmcdev = devm_kzalloc(&pdev->dev, sizeof(*xfmcdev), GFP_KERNEL);
	if (!xfmcdev)
		return -ENOMEM;	

	priv_data = devm_kzalloc(&pdev->dev, sizeof(*priv_data), GFP_KERNEL);
	if (!priv_data)
		return -ENOMEM;	
	xfmcdev->dev = &pdev->dev;
	xfmcdev->val = 5;
	priv_data->sel_mux = &sel_mux;
	priv_data->set_linerate = &set_linerate; 
	/* Platform Initialization */
	fmc74_entry();
#ifndef BASE_BOARD_VEK280
	fmc_entry();
	fmc65_entry();
	fmc64_entry();
	tipower_entry();
#endif
	msleep_range(300);
	idt_entry();
	msleep_range(300);
#ifdef BASE_BOARD_VEK280
	ti_tmds1204tx_entry();
	msleep_range(500);
	ti_tmds1204rx_entry();
#else
	onsemitx_entry();
	msleep_range(300);
	onsemirx_entry();
#endif
#ifndef BASE_BOARD_VEK280
	si5344_entry();
#endif

	platform_set_drvdata(pdev, priv_data);

	printk("%s %d\n",__func__,__LINE__);	
	return 0;
}

/* Match table for of_platform binding */
static const struct of_device_id xvfmc_of_match[] = {
	{ .compatible = "vfmc" },
	{ /* end of table */ },
};
MODULE_DEVICE_TABLE(of, xvfmc_of_match);

static struct platform_driver xvfmc_driver = {
	.probe = xvfmc_probe,
	.driver = {
		.name = "xilinx-vfmc",
		.of_match_table	= xvfmc_of_match,
	},
};
module_platform_driver(xvfmc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Xilinx Vphy driver");
