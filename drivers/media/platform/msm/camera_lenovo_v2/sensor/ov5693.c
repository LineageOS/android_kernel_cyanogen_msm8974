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
#define OV5693_SENSOR_NAME "ov5693"
DEFINE_MSM_MUTEX(ov5693_mut);


#define OV5693_INFO				2
#define OV5693_DEBUG				1


static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "ov5693 debug output");

#define ov5693_debug(level, fmt, arg...) \
  do{ \
	  if (level > debug) \
		printk( fmt , ## arg); \
	}while(0)

static struct msm_sensor_ctrl_t ov5693_s_ctrl;
struct otp_struct {
	int module_integrator_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int rg_ratio;
	int bg_ratio;
	int light_rg;
	int light_bg;
	int user_data[5];
	int lenc[62];
    int checksum_lsc_ok; // 1, it is already calculated; 0 , not
    int checksum_wb_ok; // 1, lenc is already read out; 0, not 	int R_gain;
	int R_gain;
	int G_gain;
	int B_gain;

};
static struct otp_struct ov5693_otp;

static struct msm_sensor_power_setting ov5693_power_setting[] = {
/**===================================================================**/
//GPIO --->LOW
#if 0
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_IMG_EN,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},



	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_IMG_EN_1,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_IMG_EN_2,
		.config_val = GPIO_OUT_LOW,
		.delay = 30,
	},
#endif
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 0,
	},
#if 0
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},


	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
#endif

/**===================================================================**/
//PREPARE VOL

	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 1,
	},


/**======================================================================**/
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
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
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 20,
	},


	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info ov5693_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id ov5693_i2c_id[] = {
	{OV5693_SENSOR_NAME, (kernel_ulong_t)&ov5693_s_ctrl},
	{ }
};
static int32_t msm_ov5693_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov5693_s_ctrl);
}
static struct i2c_driver ov5693_i2c_driver = {
	.id_table = ov5693_i2c_id,
	.probe  = msm_ov5693_i2c_probe,
	.driver = {
		.name = OV5693_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov5693_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov5693_dt_match[] = {
	{.compatible = "qcom,ov5693", .data = &ov5693_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ov5693_dt_match);

static struct platform_driver ov5693_platform_driver = {
	.driver = {
		.name = "qcom,ov5693",
		.owner = THIS_MODULE,
		.of_match_table = ov5693_dt_match,
	},
};
// index: index of otp group. (1, 2, 3)
// return:0, group index is empty
//1, group index has invalid data
//2, group index has valid data

#define OV5693_RG_Ratio_Typical 279//343
#define OV5693_BG_Ratio_Typical 275//328


uint16_t ov5693_read_i2c(struct msm_sensor_ctrl_t *s_ctrl,uint32_t address)
{
	int rc=0;
	uint16_t ret_val=0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			address, &ret_val,MSM_CAMERA_I2C_BYTE_DATA);
	if(rc<0)
		printk("read otp register addrees[0x%x] fail, rc=%d\n", address, rc);
	return ret_val;


}
int ov5693_write_i2c(struct msm_sensor_ctrl_t *s_ctrl,int address, int val)
{
	int ret=0;
	ret = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, address, val, MSM_CAMERA_I2C_BYTE_DATA);
	return ret;


}

int ov5693_check_otp_wb(struct msm_sensor_ctrl_t *s_ctrl,int index)
{
	uint16_t flag;
	int i;
	int bank, address;
	// select bank index
	bank = 0xc0 | index;
	ov5693_write_i2c(s_ctrl,0x3d84, bank);
	// read otp into buffer
	ov5693_write_i2c(s_ctrl,0x3d81, 0x01);
	msleep(5);
	// read flag
	address = 0x3d00;
	flag = ov5693_read_i2c(s_ctrl,address);
	ov5693_debug(OV5693_INFO,"ov5693_check_otp_wb flag=0x%x\n", flag);
	flag = flag & 0xc0;
	// clear otp buffer
	for (i=0;i<16;i++) {
		ov5693_write_i2c(s_ctrl,0x3d00 + i, 0x00);
	}
	if (flag == 0x00) {
		return 0;
	}
	else if (flag & 0x80) {
		return 1;
	}
	else {
		return 2;
	}
}
// index: index of otp group. (1, 2, 3)
// return:0, group index is empty
//1, group index has invalid data
//2, group index has valid data

int ov5693_check_otp_lenc(struct msm_sensor_ctrl_t *s_ctrl,int index)
{
	int flag, i, bank;
	int address;
	// select bank: 4, 8, 12
	bank = 0xc0 | (index * 4);
	ov5693_write_i2c(s_ctrl,0x3d84, bank);
	// read otp into buffer
	ov5693_write_i2c(s_ctrl,0x3d81, 0x01);
	msleep(10);
	// read flag
	address = 0x3d00;
	flag = ov5693_read_i2c(s_ctrl,address);
	flag = flag & 0xc0;
	// clear otp buffer
	for (i=0;i<16;i++) {
		ov5693_write_i2c(s_ctrl,0x3d00 + i, 0x00);
	}
	if (flag == 0x00) {
		return 0;
	}
	else if (flag & 0x80) {
		return 1;
	}
	else {
		return 2;
	}
}
// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of otp_struct
// return:0,
int ov5693_read_otp_wb(struct msm_sensor_ctrl_t *s_ctrl,int index, struct otp_struct *otp_ptr)
{
	int i, bank;
	int address;
	int temp;
	// select bank index
	bank = 0xc0 | index;
	ov5693_write_i2c(s_ctrl,0x3d84, bank);
	// read otp into buffer
	ov5693_write_i2c(s_ctrl,0x3d81, 0x01);
	msleep(5);
	address = 0x3d00;
	(*otp_ptr).module_integrator_id = ov5693_read_i2c(s_ctrl,address + 1);
	(*otp_ptr).lens_id = ov5693_read_i2c(s_ctrl,address + 2);
	(*otp_ptr).production_year = ov5693_read_i2c(s_ctrl,address + 3);
	(*otp_ptr).production_month = ov5693_read_i2c(s_ctrl,address + 4);
	(*otp_ptr).production_day = ov5693_read_i2c(s_ctrl,address + 5);
	temp = ov5693_read_i2c(s_ctrl,address + 10);
	(*otp_ptr).rg_ratio = ((ov5693_read_i2c(s_ctrl,address + 6)<<2)&0x3FC) + ((temp>>6) & 0x03);
	(*otp_ptr).bg_ratio = ((ov5693_read_i2c(s_ctrl,address + 7)<<2)&0x3FC) + ((temp>>4) & 0x03);
	(*otp_ptr).light_rg = (ov5693_read_i2c(s_ctrl,address + 8) <<2) + ((temp>>2) & 0x03);
	(*otp_ptr).light_bg = (ov5693_read_i2c(s_ctrl,address + 9)<<2) + (temp & 0x03);
	(*otp_ptr).user_data[0] = ov5693_read_i2c(s_ctrl,address + 11);
	(*otp_ptr).user_data[1] = ov5693_read_i2c(s_ctrl,address + 12);
	(*otp_ptr).user_data[2] = ov5693_read_i2c(s_ctrl,address + 13);
	(*otp_ptr).user_data[3] = ov5693_read_i2c(s_ctrl,address + 14);
	(*otp_ptr).user_data[4] = ov5693_read_i2c(s_ctrl,address + 15);
#if 1
{
	ov5693_debug(OV5693_INFO,"bank[%d]\n", index);
	ov5693_debug(OV5693_INFO,"module_integrator_id=%d\n",(*otp_ptr).module_integrator_id );
	ov5693_debug(OV5693_INFO,"lens_id=%d\n",(*otp_ptr).lens_id );
	ov5693_debug(OV5693_INFO,"production_year=%d\n",(*otp_ptr).production_year );
	ov5693_debug(OV5693_INFO,"production_month=%d\n",(*otp_ptr).production_month );
	ov5693_debug(OV5693_INFO,"production_day=%d\n",(*otp_ptr).production_day );
	ov5693_debug(OV5693_INFO,"rg_ratio=%d\n",(*otp_ptr).rg_ratio);
	ov5693_debug(OV5693_INFO,"bg_ratio=%d\n",(*otp_ptr).bg_ratio);
	ov5693_debug(OV5693_INFO,"golden rg_ratio=%d\n",OV5693_RG_Ratio_Typical);
	ov5693_debug(OV5693_INFO,"golden bg_ratio=%d\n",OV5693_BG_Ratio_Typical );
	ov5693_debug(OV5693_INFO,"light_rg=%d\n",(*otp_ptr).light_rg );
	ov5693_debug(OV5693_INFO,"light_bg=%d\n",(*otp_ptr).light_bg );
	ov5693_debug(OV5693_INFO,"user_data[0]=%d\n",(*otp_ptr).user_data[0]);
	ov5693_debug(OV5693_INFO,"user_data[1]=%d\n",(*otp_ptr).user_data[1] );
	ov5693_debug(OV5693_INFO,"user_data[2]=%d\n",(*otp_ptr).user_data[2]);
	ov5693_debug(OV5693_INFO,"user_data[3]=%d\n",(*otp_ptr).user_data[3] );
	ov5693_debug(OV5693_INFO,"user_data[4]=%d\n",(*otp_ptr).user_data[4]);

}
#endif
	// clear otp buffer
	for (i=0;i<16;i++) {
		ov5693_write_i2c(s_ctrl,0x3d00 + i, 0x00);
	}
	return 0;
}
// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of otp_struct
// return:0,
int ov5693_read_otp_lenc(struct msm_sensor_ctrl_t *s_ctrl,int index, struct otp_struct *otp_ptr)
{
	int bank, i;
	int address;
	// select bank: 4, 8, 12
	bank = 0xc0 | (index * 4);
	ov5693_write_i2c(s_ctrl,0x3d84, bank);
	// read otp into buffer
	ov5693_write_i2c(s_ctrl,0x3d81, 0x01);
	msleep(5);
	address = 0x3d01;
	for(i=0;i<15;i++) {
		(* otp_ptr).lenc[i]=ov5693_read_i2c(s_ctrl,address);
		address++;
	}
	// clear otp buffer
	for (i=0;i<16;i++) {
		ov5693_write_i2c(s_ctrl,0x3d00 + i, 0x00);
	}
	// select 2nd bank
	bank++;
	ov5693_write_i2c(s_ctrl,0x3d84, bank);
	// read otp
	ov5693_write_i2c(s_ctrl,0x3d81, 0x01);
	msleep(5);
	address = 0x3d00;
	for(i=15;i<31;i++) {
		(* otp_ptr).lenc[i]=ov5693_read_i2c(s_ctrl,address);
		address++;
	}
	// clear otp buffer
	for (i=0;i<16;i++) {
		ov5693_write_i2c(s_ctrl,0x3d00 + i, 0x00);
	}
	// select 3rd bank
	bank++;
	ov5693_write_i2c(s_ctrl,0x3d84, bank);
	// read otp
	ov5693_write_i2c(s_ctrl,0x3d81, 0x01);
	msleep(5);
	address = 0x3d00;
	for(i=31;i<47;i++) {
		(* otp_ptr).lenc[i]=ov5693_read_i2c(s_ctrl,address);
		address++;
	}
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov5693_write_i2c(s_ctrl,0x3d00 + i, 0x00);
	}
	// select 4th bank
	bank++;
	ov5693_write_i2c(s_ctrl,0x3d84, bank);
	// read otp
	ov5693_write_i2c(s_ctrl,0x3d81, 0x01);
	msleep(5);
	address = 0x3d00;
	for(i=47;i<62;i++) {
		(* otp_ptr).lenc[i]=ov5693_read_i2c(s_ctrl,address);
		address++;
	}
	// clear otp buffer
	for (i=0;i<16;i++) {
		ov5693_write_i2c(s_ctrl,0x3d00 + i, 0x00);
	}
#if 1
{

	int j=0;
	for(j=0;j<62;j++)
		ov5693_debug(debug, "lenc[%d]=0x%x\n",j, (* otp_ptr).lenc[j]);

}
#endif
	return 0;
}
// R_gain, sensor red gain of AWB, 0x400 =1
// G_gain, sensor green gain of AWB, 0x400 =1
// B_gain, sensor blue gain of AWB, 0x400 =1
// return 0;
int ov5693_update_awb_gain(struct msm_sensor_ctrl_t *s_ctrl,int R_gain, int G_gain, int B_gain)
{
	ov5693_debug(OV5693_INFO,"R_gain=0x%x, G_gain=0x%x, B_gain=0x%x\n", R_gain,G_gain,B_gain);

	if (R_gain>0x400) {
		ov5693_write_i2c(s_ctrl,0x3400, R_gain>>8);
		ov5693_write_i2c(s_ctrl,0x3401, R_gain & 0x00ff);
	}
	if (G_gain>0x400) {
		ov5693_write_i2c(s_ctrl,0x3402, G_gain>>8);
		ov5693_write_i2c(s_ctrl,0x3403, G_gain & 0x00ff);
	}
	if (B_gain>0x400) {
		ov5693_write_i2c(s_ctrl,0x3404, B_gain>>8);
		ov5693_write_i2c(s_ctrl,0x3405, B_gain & 0x00ff);
	}
	return 0;
}
// otp_ptr: pointer of otp_struct
int ov5693_update_lenc(struct msm_sensor_ctrl_t *s_ctrl,struct otp_struct * otp_ptr)
{
	int i, temp;
	temp = ov5693_read_i2c(s_ctrl,0x5000);
	temp = 0x80 | temp;
	ov5693_write_i2c(s_ctrl,0x5000, temp);
	for(i=0;i<62;i++) {
		ov5693_write_i2c(s_ctrl,0x5800 + i, (*otp_ptr).lenc[i]);
	}
	return 0;
}
int ov5693_update_otp_wb(struct msm_sensor_ctrl_t *s_ctrl)
{
    int rc = -1;
	rc = ov5693_update_awb_gain(s_ctrl,ov5693_otp.R_gain, ov5693_otp.G_gain, ov5693_otp.B_gain);
    return rc;
}
// call this function after OV5693 initialization
// return value: 0 update success
//1, no OTP
int ov5693_store_otp_wb(struct msm_sensor_ctrl_t *s_ctrl)
{

	int i;
	int otp_index;
	int temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;
	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	if(ov5693_otp.checksum_wb_ok == 0)
	{
		for(i=1;i<=3;i++) {
			temp = ov5693_check_otp_wb(s_ctrl,i);
			if (temp == 2) {
				otp_index = i;
				break;
			}
		}
		if (i>3) {
			// no valid wb OTP data
			return 1;
		}
		ov5693_read_otp_wb(s_ctrl,otp_index, &ov5693_otp);
		if(ov5693_otp.light_rg==0) {
			// no light source information in OTP, light factor = 1
			rg = ov5693_otp.rg_ratio;
		}
		else {
			rg = ov5693_otp.rg_ratio * ((ov5693_otp.light_rg +512) / 1024);
		}
		if(ov5693_otp.light_bg==0) {
			// not light source information in OTP, light factor = 1
			bg = ov5693_otp.bg_ratio;
		}
		else {
			bg = ov5693_otp.bg_ratio * ((ov5693_otp.light_bg +512) / 1024);
		}
		//calculate G gain
		//0x400 = 1x gain
		if(bg < OV5693_BG_Ratio_Typical) {
			if (rg< OV5693_RG_Ratio_Typical) {
				// ov5693_otp.bg_ratio < BG_Ratio_typical &&
				// ov5693_otp.rg_ratio < RG_Ratio_typical
				G_gain = 0x400;
				B_gain = 0x400 * OV5693_BG_Ratio_Typical / bg;
				R_gain = 0x400 * OV5693_RG_Ratio_Typical / rg;
			}
			else {
				// ov5693_otp.bg_ratio < BG_Ratio_typical &&
				// ov5693_otp.rg_ratio >= RG_Ratio_typical
				R_gain = 0x400;
				G_gain = 0x400 * rg / OV5693_RG_Ratio_Typical;
				B_gain = G_gain * OV5693_BG_Ratio_Typical /bg;
			}
		}
		else {
			if (rg < OV5693_RG_Ratio_Typical) {
			// ov5693_otp.bg_ratio >= BG_Ratio_typical &&
			// ov5693_otp.rg_ratio < RG_Ratio_typical
			B_gain = 0x400;
			G_gain = 0x400 * bg / OV5693_BG_Ratio_Typical;
			R_gain = G_gain * OV5693_RG_Ratio_Typical / rg;
			}
			else {
				// ov5693_otp.bg_ratio >= BG_Ratio_typical &&
				// ov5693_otp.rg_ratio >= RG_Ratio_typical
				G_gain_B = 0x400 * bg / OV5693_BG_Ratio_Typical;
				G_gain_R = 0x400 * rg / OV5693_RG_Ratio_Typical;
				if(G_gain_B > G_gain_R ) {
					B_gain = 0x400;
					G_gain = G_gain_B;
					R_gain = G_gain * OV5693_RG_Ratio_Typical /rg;
				}
				else {
					R_gain = 0x400;
					G_gain = G_gain_R;
					B_gain = G_gain * OV5693_BG_Ratio_Typical / bg;
				}
			}
		}
		ov5693_otp.R_gain = R_gain;
		ov5693_otp.G_gain = G_gain;
		ov5693_otp.B_gain = B_gain;
		ov5693_otp.checksum_wb_ok = 1;
	}
	return 0;
}
int ov5693_update_otp_lenc(struct msm_sensor_ctrl_t *s_ctrl)
{
    int rc = -1;
	rc = ov5693_update_lenc(s_ctrl,&ov5693_otp);
    return rc;
}
// call this function after OV5693 initialization
// return value: 0 update success
//1, no OTP
int ov5693_store_otp_lenc(struct msm_sensor_ctrl_t *s_ctrl)
{

	int i;
	int otp_index;
	int temp;
		// check first lens correction OTP with valid data
	if(ov5693_otp.checksum_lsc_ok == 0)
	{
		for(i=1;i<=3;i++) {
			temp = ov5693_check_otp_lenc(s_ctrl,i);
			if (temp == 2) {
			otp_index = i;
			break;
			}
		}
		if (i>3) {
			// no valid WB OTP data
			return 1;
		}
		ov5693_read_otp_lenc(s_ctrl,otp_index, &ov5693_otp);
		ov5693_otp.checksum_lsc_ok =1;
	}
	// success
	return 0;
}



int32_t ov5693_sensor_otp_proc(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc=0;
	printk("ov5693_sensor_otp_proc start\n");

	if(!s_ctrl->sensor_i2c_client)
	{
		printk("sensor_i2c_client is null fail\n");
		return 0;

	}
	if(!s_ctrl->sensor_i2c_client->i2c_func_tbl)
	{
		printk("i2c_func_tbl is null fail\n");
		return 0;

	}
	if(!s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read)
	{
		printk("i2c_read is null fail\n");
		return 0;

	}
	if(!s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write)
	{
		printk("i2c_write is null fail\n");
		return 0;

	}

	rc=ov5693_store_otp_wb(s_ctrl);
	if(rc!=0)
		printk("no valid wb otp wb data rc=%d\n", rc);
    rc=ov5693_store_otp_lenc(s_ctrl);
	if(rc!=0)
		printk("no valid wb otp lenc data rc=%d\n", rc);
	rc=ov5693_update_otp_wb(s_ctrl);
	if(rc!=0)
		printk("no valid wb otp wb data rc=%d\n", rc);


	rc=ov5693_update_otp_lenc(s_ctrl);
	if(rc!=0)
		printk("no valid len otp data rc=%d\n", rc);
#if 0
{
int temp=0;
temp=ov5693_read_i2c(s_ctrl,0x3098);
printk("0x3098=0x%x\n", temp);
temp=ov5693_read_i2c(s_ctrl,0x3099);
printk("0x3099=0x%x\n", temp);
temp=ov5693_read_i2c(s_ctrl,0x309a);
printk("0x309a=0x%x\n", temp);
temp=ov5693_read_i2c(s_ctrl,0x309b);
printk("0x309b=0x%x\n", temp);
temp=ov5693_read_i2c(s_ctrl,0x309c);
printk("0x309c=0x%x\n", temp);

temp=ov5693_read_i2c(s_ctrl,0x30b2);
printk("0x30b2=0x%x\n", temp);

temp=ov5693_read_i2c(s_ctrl,0x30b3);
printk("0x30b3=0x%x\n", temp);
temp=ov5693_read_i2c(s_ctrl,0x30b4);
printk("0x30b4=0x%x\n", temp);
temp=ov5693_read_i2c(s_ctrl,0x30b5);
printk("0x30b5=0x%x\n", temp);
temp=ov5693_read_i2c(s_ctrl,0x30b6);
printk("0x30b6=0x%x\n", temp);

temp=ov5693_read_i2c(s_ctrl,0x4837);
printk("0x4937=0x%x\n", temp);


}
#endif
	return rc;

}
int32_t ov5693_sensor_read_mid(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc=-1;

// start stream
	ov5693_write_i2c(s_ctrl, 0x0100, 0x01 );
    msleep(5);

	rc=ov5693_store_otp_wb(s_ctrl);
	if(rc!=0)
		printk("no valid wb otp wb data rc=%d\n", rc);


	rc=ov5693_store_otp_lenc(s_ctrl);
	if(rc!=0)
		printk("no valid wb otp lenc data rc=%d\n", rc);

//stop stream
	ov5693_write_i2c(s_ctrl, 0x0100, 0x00 );

   return rc;
}

int32_t ov5693_sensor_get_otp(struct msm_sensor_ctrl_t *s_ctrl,struct msm_sensor_otp_params *sensor_otp_params_ptr)
{
    (* sensor_otp_params_ptr).module_integrator_id=ov5693_otp.module_integrator_id;
    pr_err("%s  ljk ov5693_sensor_get_otp MID=%d \n",__func__,(* sensor_otp_params_ptr).module_integrator_id);
    return 0;
}

static int32_t ov5693_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(ov5693_dt_match, &pdev->dev);
	pr_err("%s:%d match %pK\n", __func__, __LINE__, match);
	rc = msm_sensor_platform_probe(pdev, match->data);
	//ov5693_otp.checksum_wb_ok = 0;
	//ov5693_otp.checksum_lsc_ok =0;
	return rc;
}

static int __init ov5693_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d  ljk\n", __func__, __LINE__);
	rc = platform_driver_probe(&ov5693_platform_driver,
		ov5693_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&ov5693_i2c_driver);
}

static void __exit ov5693_exit_module(void)
{
	pr_info("%s:%d \n", __func__, __LINE__);
	if (ov5693_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov5693_s_ctrl);
		platform_driver_unregister(&ov5693_platform_driver);
	} else
		i2c_del_driver(&ov5693_i2c_driver);
	return;
}

static struct msm_sensor_ctrl_t ov5693_s_ctrl = {
	.sensor_i2c_client = &ov5693_sensor_i2c_client,
	.power_setting_array.power_setting = ov5693_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov5693_power_setting),
	.msm_sensor_mutex = &ov5693_mut,
	.sensor_v4l2_subdev_info = ov5693_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov5693_subdev_info),
    .sensor_read_mid = ov5693_sensor_read_mid,
	.sensor_otp_proc = ov5693_sensor_otp_proc,
	.sensor_get_otp = ov5693_sensor_get_otp,
};

module_init(ov5693_init_module);
module_exit(ov5693_exit_module);
MODULE_DESCRIPTION("ov5693");
MODULE_LICENSE("GPL v2");
