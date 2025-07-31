// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Video FMC driver
 *
 * Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 */
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
#include "x_vfmc.h"

struct x_vfmc_dev {
	struct device *dev;
	int val;
	u8 board_type;
};

const char *board_type_str(u8 board_type)
{
	switch (board_type) {
	case BASE_BOARD_VERSAL_EDGE:
	/* VERSAL_EDGE: VEK280 and VEK385 */
		return "Xilinx Versal Edge Board";
	case BASE_BOARD_ZCU102:
		return "Xilinx ZCU102";
	case BASE_BOARD_VCK190:
		return "Xilinx Versal VCK190";
	case BASE_BOARD_ZCU106:
		return "Xilinx ZCU106";
	default:
		return "UNKNOWN";
	}
}

struct fmc_drv_data {
	const struct clk_config *clk;
};

struct clk_config {
	int (*sel_mux)(int direction, int clk_sel);
	int (*set_linerate)(u8 direction, u8 is_frl, u64 linerate, u8 lanes);
};

struct x_vfmc_dev *xfmcdev;

u8 xfmcdev_board_type(void)
{
	if (!xfmcdev) {
		pr_err("Error: xfmcdev is NULL!\n");
		return -EINVAL;
	}
	return xfmcdev->board_type;
}
EXPORT_SYMBOL_GPL(xfmcdev_board_type);

static bool is_versal_edge_board(void)
{
	if (xfmcdev->board_type == BASE_BOARD_VERSAL_EDGE)
		return true;
	else
		return false;
}

/*
 * sel_mux - Select reference clock multiplexer based on direction
 * @direction: Direction flag (0 for RX, 1 for TX)
 * @clk_sel: Clock selection value
 *
 * This function configures the reference clock multiplexer for FMC devices
 * based on the specified direction and clock selection.
 *
 * Return: 0 on success
 */
static int sel_mux(int direction, int clk_sel)
{
	if (is_versal_edge_board())
		return 0;

	if (direction) {
		pr_debug("%s:direction is tx: clk_sel: %d\n", __func__, clk_sel);
		fmc65_tx_refclk_sel(clk_sel);
		fmc64_tx_refclk_sel(clk_sel);
	} else {
		pr_debug("%s:direction is rx: clk_sel: %d\n", __func__, clk_sel);
		fmc64_rx_refclk_sel(clk_sel);
	}

	return 0;
}

/*
 * set_linerate - Configure line rate for TX/RX devices
 * @direction: Direction flag (0 for RX, 1 for TX)
 * @is_frl: FRL mode flag (0 for TMDS, 1 for FRL)
 * @linerate: Line rate in bits per second
 * @lanes: Number of lanes
 *
 * This function configures the line rate for appropriate TX/RX devices
 * based on board type and transmission parameters.
 *
 * Return: 0 on success
 */
static int set_linerate(u8 direction, u8 is_frl, u64 linerate, u8 lanes)
{
	pr_debug("%s:direction is tx isfrl: %d linerate %llu lanes %d\n",
		 __func__, is_frl, linerate, lanes);
	if (direction) {
		pr_debug("%s:direction is tx isfrl: %d linerate %llu lanes %d\n",
			 __func__, is_frl, linerate, lanes);
		if (is_versal_edge_board())
			ti_tmds1204tx_linerate_conf(is_frl, linerate, direction, lanes);
		else
			onsemitx_linerate_conf(is_frl, linerate, direction);

	} else {
		pr_debug("%s:direction is rx isfrl: %d linerate %llu lanes %d\n",
			 __func__, is_frl, linerate, lanes);
		if (is_versal_edge_board())
			ti_tmds1204rx_linerate_conf(is_frl, linerate, direction, lanes);
		else
			onsemirx_linerate_conf(is_frl, linerate, direction);
	}
	return 0;
}

static inline void msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base * 1000, delay_base * 1000 + 500);
}

/*
 * xfmcdev_parse_of - parse device tree properties for xfmc device
 * @xfmcdev: Pointer to x_vfmc_dev structure
 *
 * This function parses the device tree node to extract board type information
 * and stores it in the device structure.
 *
 * Return: 0 for success and error value on failure
 */
static int xfmcdev_parse_of(struct x_vfmc_dev *xfmcdev)
{
	struct device_node *node = xfmcdev->dev->of_node;
	struct device *dev = xfmcdev->dev;

	int ret;

	pr_debug("Node name:%s\n", of_node_full_name(node));
	ret = of_property_read_u8(node, "xlnx,board-type",
				  &xfmcdev->board_type);
	if (ret) {
		dev_err(dev, "xlnx,board-type property not found in DT\n");
		return ret;
	}

	dev_info(dev, "Board Type: %s\n",
		 board_type_str(xfmcdev->board_type));
	return 0;
}

/*
 * xvfmc_probe - The device probe function for driver initialization
 * @pdev: Pointer to the platform device structure
 *
 * This function initializes the XVFMC device, parses device tree properties,
 * allocates memory for device structures, and registers all required drivers
 * based on the board type.
 *
 * Return: 0 for success and error value on failure
 */
static int xvfmc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct clk_config *priv_data;

	xfmcdev = devm_kzalloc(&pdev->dev, sizeof(*xfmcdev), GFP_KERNEL);
	if (!xfmcdev)
		return -ENOMEM;

	priv_data = devm_kzalloc(&pdev->dev, sizeof(*priv_data), GFP_KERNEL);
	if (!priv_data)
		return -ENOMEM;
	xfmcdev->dev = &pdev->dev;
	xfmcdev->val = 5;
	ret = xfmcdev_parse_of(xfmcdev);
	if (ret < 0)
		dev_dbg(xfmcdev->dev, "xlnx,board-type property not found in DT\n");
	priv_data->sel_mux = &sel_mux;
	priv_data->set_linerate = &set_linerate;

	/* Platform Initialization */
	if (!is_versal_edge_board()) {
		struct {
			const char *name;
			int (*entry)(void);
		} non_versal_entries[] = {
			{ "fmc", fmc_entry },
			{ "fmc65", fmc65_entry },
			{ "fmc64", fmc64_entry },
			{ "tipower", tipower_entry },
			{ "si5344", si5344_entry },
		};

		for (int i = 0; i < ARRAY_SIZE(non_versal_entries); i++) {
			ret = non_versal_entries[i].entry();
			if (ret) {
				dev_err(xfmcdev->dev, "Failed to register %s\n",
					non_versal_entries[i].name);
				return ret;
			}
		}
	}

	ret = fmc74_entry();
	if (ret) {
		dev_err(xfmcdev->dev, "fmc74 registration failed\n");
		return ret;
	}

	msleep_range(300);

	ret = idt_entry();
	if (ret) {
		dev_err(xfmcdev->dev, "Failed to register IDT\n");
		return ret;
	}
	msleep_range(300);

	if (is_versal_edge_board()) {
		ret = ti_tmds1204tx_entry();
		if (ret) {
			dev_err(xfmcdev->dev, "Failed to register TI TX\n");
			return ret;
		}
		msleep_range(500);
		ret = ti_tmds1204rx_entry();
		if (ret) {
			dev_err(xfmcdev->dev, "Failed to register TI RX\n");
			return ret;
		}

	} else {
		ret = onsemitx_entry();
		if (ret) {
			dev_err(xfmcdev->dev, "Failed to register Onsemi TX\n");
			return ret;
		}

		msleep_range(300);

		ret = onsemirx_entry();
		if (ret) {
			dev_err(xfmcdev->dev, "Failed to register Onsemi RX\n");
			return ret;
		}
	}

	platform_set_drvdata(pdev, priv_data);

	dev_info(&pdev->dev, "Xilinx XVFMC Probed!!");

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
