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

int fmc65_tx_refclk_sel(unsigned int clk_sel);
int fmc65_rx_refclk_sel(unsigned int clk_sel);
int fmc65_entry(void);
void fmc65_exit(void);

static const struct i2c_device_id fmc65_id[] = {
	{ "expander-fmc65", 8 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fmc65_id);

static const struct of_device_id fmc65_of_table[] = {
	{ .compatible = "expander-fmc65" },
	{ }
};
MODULE_DEVICE_TABLE(of, fmc65_of_table);

struct fmc65 {
	struct gpio_chip	chip;
	struct i2c_client	*client;
	struct mutex	lock;	/* protect 'out' */
	unsigned int	status;	/* current status */
	unsigned int	out;	/* software latch */
	int (*write)(struct i2c_client *client, unsigned data);
	int (*read)(struct i2c_client *client);
};

struct p_data {
	unsigned int	gpio_base;
	unsigned int	n_latch;
};

struct fmc65	*gpio;

enum fmc_tx_refclk {
	tx_refclk_from_idt = 0,
	tx_refclk_from_si5344,
};

/* Talk to 8-bit I/O expander */

static int i2c_write_le8(struct i2c_client *client, unsigned data)
{
	return i2c_smbus_write_byte(client, data);
}

static int i2c_read_le8(struct i2c_client *client)
{
	return (int)i2c_smbus_read_byte(client);
}

static int fmc65_modify_reg(struct fmc65 *gpio, u8 val, u8 mask)
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

int fmc65_tx_refclk_sel(unsigned int clk_sel)
{
	int ret;

	if (clk_sel == tx_refclk_from_idt) {
		dev_info(&gpio->client->dev, "tx refclock from IDT\n");
		ret |= fmc65_modify_reg(gpio, 0x1A, 0x08);

	} else if (clk_sel == tx_refclk_from_si5344) {
		dev_info(&gpio->client->dev, "tx refclock from si5344\n");
		ret |= fmc65_modify_reg(gpio, 0x12, 0x08);
	} else {
		dev_info(&gpio->client->dev, "invalid tx refclock selection\n");
		return 0;
	}

	if (ret == 1)
		dev_info(&gpio->client->dev, "failed to select tx refclock\n");

	return ret;
}
EXPORT_SYMBOL_GPL(fmc65_tx_refclk_sel);

static int fmc65_probe(struct i2c_client *client)
{
	struct p_data	*pdata = dev_get_platdata(&client->dev);
	struct device_node		*np = client->dev.of_node;
	unsigned int			n_latch = 0;
	int				status;
	const struct i2c_device_id *id = i2c_match_id(fmc65_id, client);

	if (IS_ENABLED(CONFIG_OF) && np)
		of_property_read_u32(np, "lines-initial-states", &n_latch);
	else if (pdata)
		n_latch = pdata->n_latch;
	else
		dev_dbg(&client->dev, "no platform data\n");

	/* Allocate, initialize, and register this gpio_chip. */
	gpio = devm_kzalloc(&client->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	mutex_init(&gpio->lock);

	gpio->chip.base			= pdata ? pdata->gpio_base : -1;
	gpio->chip.parent		= &client->dev;
	gpio->chip.owner		= THIS_MODULE;
	gpio->chip.ngpio		= id->driver_data;

	if (gpio->chip.ngpio == 8) {
		gpio->write	= i2c_write_le8;
		gpio->read	= i2c_read_le8;

		if (!i2c_check_functionality(client->adapter,
					     I2C_FUNC_SMBUS_BYTE))
			status = -EIO;

		/* fail if there's no chip present */
		else
			status = i2c_smbus_read_byte(client);
	} else {
		dev_dbg(&client->dev, "unsupported number of gpios\n");
		status = -EINVAL;
	}

	if (status < 0)
		goto fail;

	gpio->chip.label = client->name;

	gpio->client = client;
	i2c_set_clientdata(client, gpio);

	gpio->out = ~n_latch;
	gpio->status = gpio->out;

	status = devm_gpiochip_add_data(&client->dev, &gpio->chip, gpio);
	if (status < 0)
		goto fail;
	/* init fmc65 */
	gpio->write(gpio->client, 0x1A);

	return 0;

fail:
	dev_dbg(&client->dev, "probe error %d for '%s'\n", status,
		client->name);

	return status;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int fmc65_remove(struct i2c_client *client)
{
	return 0;
}
#else
static void fmc65_remove(struct i2c_client *client)
{
	return;
}
#endif

static struct i2c_driver fmc65_driver = {
	.driver = {
		.name	= "fmc65",
		.of_match_table = of_match_ptr(fmc65_of_table),
	},
	.probe	= fmc65_probe,
	.remove	= fmc65_remove,
	.id_table = fmc65_id,
};

void fmc65_exit(void)
{
	i2c_del_driver(&fmc65_driver);
}
EXPORT_SYMBOL_GPL(fmc65_exit);

int fmc65_entry(void)
{
	return i2c_add_driver(&fmc65_driver);
}
EXPORT_SYMBOL_GPL(fmc65_entry);

MODULE_DESCRIPTION("FMC65 Expander driver");
MODULE_LICENSE("GPL v2");
