#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

static bool lenovo_hw;
static unsigned int lenovo_hw_gpio;

bool lenovo_hw_get(void)
{
	return lenovo_hw;
}

static int __devinit lenovo_hw_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;

	ret = of_get_named_gpio(np, "hw-gpio", 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get hw gpio, ret=%d\n",
			ret);
		goto set_default;
	}

	lenovo_hw_gpio = ret;

	ret = gpio_request(lenovo_hw_gpio, "lenovo-hw-gpio");
	if (ret) {
		dev_err(&pdev->dev, "hw gpio %d request failed\n",
			lenovo_hw_gpio);
		ret = -EINVAL;
		goto set_default;
	}

	ret = gpio_tlmm_config(GPIO_CFG(lenovo_hw_gpio, 0,
				GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN,
				GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to configure %d gpio (pull_down), ret=%d\n",
			lenovo_hw_gpio, ret);
		goto gpio_free;
	}

	usleep(20);

	if (!gpio_get_value(lenovo_hw_gpio)) {
		// Try PULL_UP
		ret = gpio_tlmm_config(GPIO_CFG(lenovo_hw_gpio, 0,
					GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
					GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Unable to configure %d gpio (pull up), ret=%d\n",
				lenovo_hw_gpio, ret);
			goto gpio_free;
		}

		usleep(20);

		if (gpio_get_value(lenovo_hw_gpio)) {
			lenovo_hw = true;
		} else {
			lenovo_hw = false;
		}
	} else {
		lenovo_hw = true;
	}

	dev_err(&pdev->dev, "Detected lenovo hw %d\n", lenovo_hw);

	return ret;

gpio_free:
	gpio_free(lenovo_hw_gpio);
set_default:
	lenovo_hw = true;
	dev_err(&pdev->dev,
		"Unable to get lenovo hw, defaulting to %d, ret=%d\n",
		lenovo_hw, ret);
	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id lenovo_hw_match_table[] = {
	{ .compatible = "lenovo-hw",},
	{ },
};
#else
#define lenovo_hw_match_table NULL
#endif

static struct platform_driver lenovo_hw_driver = {
	.probe = lenovo_hw_probe,
	.driver = {
		.name = "lenovo-hw",
		.owner = THIS_MODULE,
		.of_match_table = lenovo_hw_match_table,
	},
};

static int __init lenovo_hw_init(void)
{
	return platform_driver_register(&lenovo_hw_driver);
}

module_init(lenovo_hw_init);
MODULE_DESCRIPTION("HW Detection module");
MODULE_AUTHOR("Michael Sky <electrydev@gmail.com>");
MODULE_LICENSE("GPL");
