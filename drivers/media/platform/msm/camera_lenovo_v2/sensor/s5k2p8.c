/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "msm_sensor.h"
#define S5K2P8_SENSOR_NAME "s5k2p8"
DEFINE_MSM_MUTEX(s5k2p8_mut);

static struct msm_sensor_ctrl_t s5k2p8_s_ctrl;

static struct msm_sensor_power_setting s5k2p8_power_setting[] = {
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 3,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VDIG,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 1,
	},

	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 0,
		.delay = 1,
	},

		/*

	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 30,
	},*/

	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info s5k2p8_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id s5k2p8_i2c_id[] = {
	{S5K2P8_SENSOR_NAME, (kernel_ulong_t)&s5k2p8_s_ctrl},
	{ }
};

int s5k2p8_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint16_t chipid = 0;
	struct msm_camera_i2c_client *sensor_i2c_client;
	struct msm_camera_slave_info *slave_info;
	const char *sensor_name;

	if (!s_ctrl) {
		pr_err("%s:%d failed: %pK\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}
	sensor_i2c_client = s_ctrl->sensor_i2c_client;
	slave_info = s_ctrl->sensordata->slave_info;
	sensor_name = s_ctrl->sensordata->sensor_name;

	if (!sensor_i2c_client || !slave_info || !sensor_name) {
		pr_err("%s:%d failed: %pK %pK %pK\n",
			__func__, __LINE__, sensor_i2c_client, slave_info,
			sensor_name);
		return -EINVAL;
	}

	rc = sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, slave_info->sensor_id_reg_addr,
		&chipid, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__, sensor_name);
		return rc;
	}

	pr_err("%s: read id: %x expected id %x\n", __func__, chipid,
		slave_info->sensor_id);
	if (chipid != slave_info->sensor_id) {
/* add ljk for cam sensor id 2102 & 2108 compatiable */
	    if(chipid != 0x2102)
	    {
    		pr_err("msm_sensor_match_id chip id doesnot match  ljk\n");
    		return -ENODEV;
	    }
	    else
	    {
	       pr_err("msm_sensor_match_id chip id 2102 for compatibe to DVT2 ljk\n");
	    }
/*+end*/
	}
	return rc;
}

static struct msm_sensor_fn_t s5k2p8_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = s5k2p8_sensor_match_id,
};

static int32_t msm_s5k2p8_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &s5k2p8_s_ctrl);
}

static struct i2c_driver s5k2p8_i2c_driver = {
	.id_table = s5k2p8_i2c_id,
	.probe  = msm_s5k2p8_i2c_probe,
	.driver = {
		.name = S5K2P8_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k2p8_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id s5k2p8_dt_match[] = {
	{.compatible = "qcom,s5k2p8", .data = &s5k2p8_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, s5k2p8_dt_match);

static struct platform_driver s5k2p8_platform_driver = {
	.driver = {
		.name = "qcom,s5k2p8",
		.owner = THIS_MODULE,
		.of_match_table = s5k2p8_dt_match,
	},
};

static int32_t s5k2p8_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(s5k2p8_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init s5k2p8_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d \n", __func__, __LINE__);
	rc = platform_driver_probe(&s5k2p8_platform_driver,
		s5k2p8_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&s5k2p8_i2c_driver);
}

static void __exit s5k2p8_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (s5k2p8_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&s5k2p8_s_ctrl);
		platform_driver_unregister(&s5k2p8_platform_driver);
	} else
		i2c_del_driver(&s5k2p8_i2c_driver);
	return;
}

static struct msm_sensor_ctrl_t s5k2p8_s_ctrl = {
	.sensor_i2c_client = &s5k2p8_sensor_i2c_client,
	.power_setting_array.power_setting = s5k2p8_power_setting,
	.power_setting_array.size = ARRAY_SIZE(s5k2p8_power_setting),
	.msm_sensor_mutex = &s5k2p8_mut,
	.sensor_v4l2_subdev_info = s5k2p8_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k2p8_subdev_info),
	.func_tbl = &s5k2p8_sensor_func_tbl,
};

module_init(s5k2p8_init_module);
module_exit(s5k2p8_exit_module);
MODULE_DESCRIPTION("s5k2p8");
MODULE_LICENSE("GPL v2");
