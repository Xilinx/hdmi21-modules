// SPDX-License-Identifier: GPL-2.0
/*
 * FMC Expander driver
 *
 * Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 */
#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/version.h>

/* function prototypes */
int fmc64_rx_refclk_sel(unsigned int clk_sel);
int fmc64_tx_refclk_sel(unsigned int clk_sel);
int fmc64_entry(void);
void fmc64_exit(void);

static const struct i2c_device_id fmc64_id[] = {
	{ "expander-fmc64", 8 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fmc64_id);

static const struct of_device_id fmc64_of_table[] = {
	{ .compatible = "expander-fmc64" },
	{ }
};
MODULE_DEVICE_TABLE(of, fmc64_of_table);

struct fmc64 {
	struct gpio_chip	chip;
	struct i2c_client	*client;
	struct mutex	lock;		/* protect 'out' */
	unsigned int status;	/* current status */
	unsigned int out;	/* software latch */
	int (*write)(struct i2c_client *client, unsigned data);
	int (*read)(struct i2c_client *client);
};

struct p_data {
	unsigned int	gpio_base;
	unsigned int	n_latch;
};

enum fmc64_refclk_sel {
	tx_refclk_from_idt = 0,
	tx_refclk_from_si5344,
	rx_refclk_from_cable = 0,
	rx_refclk_from_si5344,
};

struct fmc64			*gpio64;

/* Talk to 8-bit I/O expander */

static int i2c_write_le8(struct i2c_client *client, unsigned int data)
{
	return i2c_smbus_write_byte(client, data);
}

static int i2c_read_le8(struct i2c_client *client)
{
	return (int)i2c_smbus_read_byte(client);
}

static int fmc64_modify_reg(struct fmc64 *gpio, u8 val, u8 mask)
{
	unsigned int data;
	int ret = 0;

	/* Read data */
	data = gpio->read(gpio->client);
	/* Clear masked bits */
	data &= ~mask;
	/* Update */
	data |= (val & mask);
	/* Write data */
	gpio->write(gpio->client, data);

	return ret;
}

int fmc64_rx_refclk_sel(unsigned int clk_sel)
{
	int ret;

	if (clk_sel == rx_refclk_from_si5344) {
		dev_info(&gpio64->client->dev, "rx refclock from si5344\n");
		ret |= fmc64_modify_reg(gpio64, 0x41, 0x18);

	} else if (clk_sel == rx_refclk_from_cable) {
		dev_info(&gpio64->client->dev, "rx refclock from cable\n");
		ret |= fmc64_modify_reg(gpio64, 0x51, 0x18);
	} else {
		dev_info(&gpio64->client->dev,
			 "invalid rx ref clock selection\n");
		return 0;
	}

	if (ret == 1)
		dev_err(&gpio64->client->dev,
			"failed to select rx ref clock\n");

	return ret;
}
EXPORT_SYMBOL_GPL(fmc64_rx_refclk_sel);

int fmc64_tx_refclk_sel(unsigned int clk_sel)
{
	int ret;

	if (clk_sel == tx_refclk_from_idt) {
		dev_info(&gpio64->client->dev, "tx refclock from idt\n");
		ret |= fmc64_modify_reg(gpio64, 0x41, 0x60);

	} else if (clk_sel == tx_refclk_from_si5344) {
		dev_info(&gpio64->client->dev, "tx refclock from si5344\n");
		ret |= fmc64_modify_reg(gpio64, 0x01, 0x60);
	} else {
		dev_info(&gpio64->client->dev,
			 "invalid tx refclock selection\n");
		return 0;
	}

	if (ret == 1)
		dev_err(&gpio64->client->dev,
			"Failed to select TX Ref clock\r\n");

	return ret;
}
EXPORT_SYMBOL_GPL(fmc64_tx_refclk_sel);

static int fmc64_probe(struct i2c_client *client)
{
	struct p_data	*pdata = dev_get_platdata(&client->dev);
	struct device_node		*np = client->dev.of_node;
	unsigned int			n_latch = 0;
	int				status;
	const struct i2c_device_id *id = i2c_match_id(fmc64_id, client);

	if (IS_ENABLED(CONFIG_OF) && np)
		of_property_read_u32(np, "lines-initial-states", &n_latch);
	else if (pdata)
		n_latch = pdata->n_latch;
	else
		dev_dbg(&client->dev, "no platform data\n");

	/* Allocate, initialize, and register this gpio_chip. */
	gpio64 = devm_kzalloc(&client->dev, sizeof(*gpio64), GFP_KERNEL);
	if (!gpio64)
		return -ENOMEM;

	mutex_init(&gpio64->lock);

	gpio64->chip.base			= pdata ? pdata->gpio_base : -1;
	gpio64->chip.parent		= &client->dev;
	gpio64->chip.owner		= THIS_MODULE;
	gpio64->chip.ngpio		= id->driver_data;

	if (gpio64->chip.ngpio == 8) {
		gpio64->write	= i2c_write_le8;
		gpio64->read	= i2c_read_le8;

		if (!i2c_check_functionality(client->adapter,
					     I2C_FUNC_SMBUS_BYTE))
			status = -EIO;
		/* fail if there's no chip present */
		else
			status = i2c_smbus_read_byte(client);
	} else {
		dev_dbg(&client->dev, "unsupported number of gpio64s\n");
		status = -EINVAL;
	}

	if (status < 0)
		goto fail;

	gpio64->chip.label = client->name;
	gpio64->client = client;
	i2c_set_clientdata(client, gpio64);
	gpio64->out = ~n_latch;
	gpio64->status = gpio64->out;

	status = devm_gpiochip_add_data(&client->dev, &gpio64->chip, gpio64);
	if (status < 0)
		goto fail;

	/* init fmc64 */
	gpio64->write(gpio64->client, 0x41);

	return 0;

fail:
	dev_dbg(&client->dev, "probe error %d for '%s'\n", status,
		client->name);

	return status;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int fmc64_remove(struct i2c_client *client)
{
	return 0;
}
#else
static void fmc64_remove(struct i2c_client *client)
{
	return;
}
#endif

static struct i2c_driver fmc64_driver = {
	.driver = {
		.name	= "fmc64",
		.of_match_table = of_match_ptr(fmc64_of_table),
	},
	.probe	= fmc64_probe,
	.remove	= fmc64_remove,
	.id_table = fmc64_id,
};

void fmc64_exit(void)
{
	i2c_del_driver(&fmc64_driver);
}
EXPORT_SYMBOL_GPL(fmc64_exit);

int fmc64_entry(void)
{
	return i2c_add_driver(&fmc64_driver);
}
EXPORT_SYMBOL_GPL(fmc64_entry);

MODULE_DESCRIPTION("FMC64 Expander driver");
MODULE_LICENSE("GPL v2");
