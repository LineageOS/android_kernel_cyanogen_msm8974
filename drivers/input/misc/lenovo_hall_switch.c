/*
 *  Hall switch sensor driver
 *
 * Copyright (c) 2016, Michael Sky <electrydev@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/input.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define LID_OPEN	0
#define LID_CLOSED	1

struct hall_switch_data {
	struct input_dev *input_dev;
	struct delayed_work hall_work;
	struct workqueue_struct *hall_workqueue;
	int hall_gpio;
	int hall_irq;
	char state;
};
struct hall_switch_data *hall_data = NULL;

static ssize_t hall_show_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hall_data->state);
}

static struct device_attribute attrs[] = {
	__ATTR(state, S_IRUGO, hall_show_state, NULL),
};

static irqreturn_t misc_hall_irq(int irq, void *data)
{
	int gpio_value;

	if (hall_data == NULL)
		return 0;

	gpio_value = gpio_get_value(hall_data->hall_gpio);
	hall_data->state = gpio_value ? LID_OPEN : LID_CLOSED;

	input_report_switch(hall_data->input_dev, SW_LID,
		hall_data->state);
	input_sync(hall_data->input_dev);

	return IRQ_HANDLED;
}

static int __devinit hall_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct device_node *np = pdev->dev.of_node;
	unsigned char attr_count;

	hall_data = kzalloc(sizeof(struct hall_switch_data), GFP_KERNEL);
	if (!hall_data) {
		dev_err(&pdev->dev, "Failed allocate memory for hall_data\n");
		retval = -ENOMEM;
		goto exit;
	}

	retval = of_get_named_gpio(np, "irq-gpio", 0);
	if (retval < 0) {
		dev_err(&pdev->dev, "Failed to get irq gpio, ret=%d\n",
			retval);
		goto exit_kfree;
	}
	hall_data->hall_gpio = retval;

	hall_data->input_dev = input_allocate_device();
	if (hall_data->input_dev == NULL) {
		dev_err(&pdev->dev, "Failed to allocate input device\n");
		retval = -ENOMEM;
		goto exit_kfree;
	}

	hall_data->input_dev->name = "hall-switch";
	hall_data->input_dev->phys = "/dev/input/hall-switch";

	set_bit(EV_SW, hall_data->input_dev->evbit);
	set_bit(SW_LID, hall_data->input_dev->swbit);

	input_set_capability(hall_data->input_dev, EV_SW, SW_LID);

	retval = input_register_device(hall_data->input_dev);
	if (retval) {
		dev_err(&pdev->dev, "Failed to register input device\n");
		goto exit_register_input;
	}

	retval = gpio_request(hall_data->hall_gpio, "hall_gpio");
	if (retval) {
		dev_err(&pdev->dev, "irq gpio [%d], request failed\n",
			hall_data->hall_gpio);
		goto exit_enable_irq;
	}
	retval = gpio_direction_input(hall_data->hall_gpio);
	if (retval) {
		dev_err(&pdev->dev, "irq gpio [%d], direction set failed\n",
			hall_data->hall_gpio);
		goto exit_free_gpio;
	}

	hall_data->hall_irq = gpio_to_irq(hall_data->hall_gpio);

	retval = request_threaded_irq(hall_data->hall_irq, NULL,
			misc_hall_irq, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"misc_hall_irq", hall_data);
	if (retval < 0) {
		dev_err(&pdev->dev,
			"Failed to create hall irq thread for gpio [%d]\n",
			hall_data->hall_gpio);
		goto exit_free_gpio;
	}

	enable_irq_wake(hall_data->hall_irq);

	hall_data->state = LID_OPEN;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&pdev->dev.kobj,
						&attrs[attr_count].attr);
		if (retval < 0) {
			pr_err("%s: Failed to create sysfs attributes, ret=%d\n",
				__func__, retval);
			goto exit_sysfs;
		}
	}

	return retval;

exit_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&pdev->dev.kobj,
					&attrs[attr_count].attr);
	}

exit_free_gpio:
	gpio_free(hall_data->hall_gpio);

exit_enable_irq:
	input_unregister_device(hall_data->input_dev);

exit_register_input:
	input_free_device(hall_data->input_dev);
	hall_data->input_dev = NULL;

exit_kfree:
	kfree(hall_data);

exit:
	return retval;
}


#ifdef CONFIG_OF
static struct of_device_id hall_match_table[] = {
	{ .compatible = "hall-switch",},
	{ },
};
#else
#define hall_match_table NULL
#endif

static struct platform_driver hall_driver = {
	.probe = hall_probe,
	.driver = {
		.name = "hall_switch",
		.owner = THIS_MODULE,
		.of_match_table = hall_match_table,
	},
};

static int __init hall_init(void)
{
	return platform_driver_register(&hall_driver);
}

module_init(hall_init);
MODULE_DESCRIPTION("Hall switch sensor driver");
MODULE_AUTHOR("Michael Sky <electrydev@gmail.com>");
MODULE_LICENSE("GPL");
