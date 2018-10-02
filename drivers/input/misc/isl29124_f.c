/******************************************************************************

	file		: isl29124.c

	Description	: Driver for ISL29124 RGB light sensor

	License		: GPLv2

	Copyright	: Intersil Corporation (c) 2013	
******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/isl29124.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/input.h>
/* add for lux and CCT compensation  */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <asm/io.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/math64.h>

MODULE_AUTHOR("Intersil Corporation");
MODULE_LICENSE("GPLv2");
MODULE_DESCRIPTION("Driver for ISL29124 RGB light sensor");
#define SENSOR_VREG 1
#define SENSOR_COM  1
#define SENSOR_INPUT 1
#define NEW_CCM
struct mutex rwlock_mutex;
unsigned char irq_num;
struct work_struct work;
struct i2c_client *isl29124_client_data;
#if SENSOR_VREG
struct regulator *vdd;
struct regulator *vcc_i2c;
#define GOODIX_VTG_MIN_UV	2600000
#define GOODIX_VTG_MAX_UV	3300000
#define GOODIX_I2C_VTG_MIN_UV	1800000
#define GOODIX_I2C_VTG_MAX_UV	1800000
#endif
#if SENSOR_INPUT
	struct input_dev *sensor_input;
	struct delayed_work    sensor_dwork; /* for ALS polling */
	bool sensor_enable;
	unsigned long POLL_DELAY= 125;
#endif
/* Devices supported by this driver and their I2C address */
struct i2c_device_id isl_sensor_device_table[] = {
	{"isl29124_f", ISL29124_I2C_ADDR},
	{}
};

#if SENSOR_COM
       int isl29124_i2c_read_word16(struct i2c_client *client, 
                                    unsigned char reg_addr, unsigned short *buf);
       int get_optical_range(int *range);
       int get_adc_resolution_bits(int *res);
#endif


#if SENSOR_COM
/* CCT parameter */
#define CCT_FACTOR      (1000000000L)   //10e9, factor
#define CCT_ALPHA       438496059L
#define CCT_BETA        1104739872L
#define CCT_GAMMA       1825683698L
#define CCT_KRG         1411916127L
#define CCT_KRB         -34054705L
#define CCT_KGR         4698210L
#define CCT_KGB         6626509L
#define CCT_KBR         -83308558L
#define CCT_KBG         -208474309L
#endif

struct isl29124_data_t {
	u8 als_pwr_status;
	struct i2c_client* client;
	u8 als_range;		/* als range, 0: low range, 1: high range */
	u8 als_resolution;	/* als adc resolution, 0: 12bit, 1: 16bit */
	int poll_delay;		/* poll delay set by hal */
	u8 show_als_raw;	/* show als raw data flag, used for debug */
	//struct timer_list als_timer;	/* als poll timer */
	//spinlock_t als_timer_lock;
	//struct work_struct als_work;
	//struct workqueue_struct *als_wq;
	//struct input_dev *als_input_dev;
	u8 als_range_using;		/* the als range using now */
        u8 adc_resolution;
	u8 als_pwr_before_suspend;
	u8 als_chg_range_delay_cnt;

	u16 last_r;
	u16 last_g;
	u16 last_b;
	u16 last_g2;
#ifdef NEW_CCM
	u16 cct;
	u16 X;
	u16 Y;
	u16 Z;
#endif
	u8 all_last_avail;
	u8 current_color;
};

#ifdef NEW_CCM
// louis add the below March, 13, 2014
enum range { RangeLo=0, RangeHi, RangeMax };
enum resolution { Bit16=0, Bit12, BitMax };
// 14bit fixed point calc
static s32 CCM_Gain[RangeMax][BitMax] = {
	{35447L, 631511L},
	{46172L,       22650L},
};

static s32 CCM_RangeLo[3][3] ={
	{	-2980L,	16389L,	-11820L},// X col
	{	-4388L,	16383L,	-10653L },// Y col
	{	-8998L,	13667L,	-3900L}, // Z col
};

static s32 CCM_RangeHi[3][3] ={
	{	-715L,	14265L,	-9230L},// X col
	{	-3267L,	16383L,	-9969L },// Y col
	{	-7420L,	7032L,	7344L}, // Z col
};
// louis end of add
#endif

#ifdef NEW_CCM
static u32 cal_cct(struct isl29124_data_t *dat)
{
        //s32 tmp;
        s32 cct;
        s64 X0, Y0, Z0, sum0;
        s64 x,y,n, xe, ye;
        u8 Range;
        u8 bits;
        u16 als_r, als_g, als_b;
	s64 tmp;

        als_r = dat->last_r;
        als_g = dat->last_g;
        als_b = dat->last_b;

        //bits = (dat->adc_resolution==0)? 1:0;
	bits = 0;
        Range=dat->als_range_using;
        if(Range == 0)
        {
                X0 = ( CCM_RangeLo[0][0]*als_r + CCM_RangeLo[0][1]*als_g + CCM_RangeLo[0][2] * als_b );
                Y0 = ( CCM_RangeLo[1][0]*als_r + CCM_RangeLo[1][1]*als_g + CCM_RangeLo[1][2] * als_b );
                Z0 = ( CCM_RangeLo[2][0]*als_r + CCM_RangeLo[2][1]*als_g + CCM_RangeLo[2][2] * als_b );
        }
        else
        {
                X0 = ( CCM_RangeHi[0][0]*als_r + CCM_RangeHi[0][1]*als_g + CCM_RangeHi[0][2] * als_b );
                Y0 = ( CCM_RangeHi[1][0]*als_r + CCM_RangeHi[1][1]*als_g + CCM_RangeHi[1][2] * als_b );
                Z0 = ( CCM_RangeHi[2][0]*als_r + CCM_RangeHi[2][1]*als_g + CCM_RangeHi[2][2] * als_b );
        }


//      X=X0/CCM_Gain[Range][bits];
//      Y=Y0/CCM_Gain[Range][bits];
//      Z=Z0/CCM_Gain[Range][bits];
//      sum = X + Y + Z;
//      x = X*1000/sum; y = Y*1000/sum;

        sum0 = X0 + Y0 + Z0;
        if (sum0 == 0)
        {
                //printk("sum0 value is 0");
                return -1;
        }
        //x = X0*1000*CCM_Gain[Range][bits]/sum0;
        x = div64_s64(X0*10000, sum0);
        //y = Y0*1000*CCM_Gain[Range][bits]/sum0;
        y = div64_s64(Y0*10000, sum0);
        xe=3320; // 0.3320
        ye=1858; // 0.1858
        if (y == 1858)
        {
                printk("y-ye value is 0");
                return -1;
        }
        // n = (x-xe)/(y-ye)
        //n = ( x - xe )*1000/( y - ye );
        n = div64_s64(( x - xe )*10000,( y - ye ));
        //cct = n * (n * ((-449 * n) / 1000 + 3525) / 1000 - 6823) / 1000 + 5520;
	tmp = div64_s64(-449*n, 10000);
	tmp = div64_s64((tmp+3525)*n, 10000);
	tmp = div64_s64((tmp-6823)*n, 10000);
	cct = tmp + 5520;
        //n = (X<<31 - 712964572L *sum ) / ( Y<<17 - 24354L * sum);
        //cct = n * (n * ((-449*n)/16384 + 3525)/16384 - 6823)/16384 + 5520;
        dat->X = div64_s64( X0, CCM_Gain[Range][bits]);
        dat->Y = div64_s64( Y0, CCM_Gain[Range][bits]);
        dat->Z = div64_s64( Z0, CCM_Gain[Range][bits]);

        //printk(KERN_ERR "CCM : X0 %lld, Y0:%lld, Z0:%lld, sum0:%lld, x:%lld, y:%lld, n:%lld, cct:%lld\n", X0, Y0, Z0, sum0, x, y, n,(long long int) cct );
        if(cct < 0) cct = 0;

        dat->cct = cct;
        return cct;
}
#else
static u32 cal_cct(u16 als_r, u16 als_g, u16 als_b)
{
        s32 tmp;
        s32 cct;
        s32 rp, gp, bp;
        s32 x,y,n;
        s32 sum;

        tmp = als_r  + als_g * CCT_KRG + als_b * CCT_KRB;
        div_s64(tmp, CCT_FACTOR);
        rp = CCT_ALPHA * tmp;

        tmp = als_r * CCT_KGR  + als_g  + als_b * CCT_KGB;
        div_s64(tmp, CCT_FACTOR);
        gp = CCT_BETA * tmp;

        tmp = als_r * CCT_KBR  + als_g * CCT_KBG + als_b;
        div_s64(tmp, CCT_FACTOR);
        bp = CCT_GAMMA * tmp;

        sum = rp + gp + bp;
        x = div_s64(rp * 10000, sum);
        y = div_s64(gp * 10000, sum);

        n = ((x - 332) * 1000) / (y - 1858);
        cct = n * (n * ((-449 * n) / 1000 + 3525) / 1000 - 6823) / 1000 + 5520;

        if(cct < 0) cct = 0;
        return cct;
}
#endif
/*********************************************
ssize_t show_cct(struct device *dev, struct device_attribute *attr, char *buf)
{
       	int ret;
       	unsigned short cct;
	unsigned short regr;
        unsigned short regg;
        unsigned short regb;
        struct i2c_client *client = to_i2c_client(dev);

        mutex_lock(&rwlock_mutex);
        ret = isl29124_i2c_read_word16(client, RED_DATA_LBYTE_REG, &regr);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
                return -1;
        }

        ret = isl29124_i2c_read_word16(client, GREEN_DATA_LBYTE_REG, &regg);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
                return -1;
        }
        ret = isl29124_i2c_read_word16(client, BLUE_DATA_LBYTE_REG, &regb);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
                return -1;
        }

	cct = cal_cct(regr,regg,regb);

       	sprintf(buf, "%d\n", cct);
       	mutex_unlock(&rwlock_mutex);
       	return strlen(buf);
}
***********************************************************/
#if SENSOR_COM

#define ISL29124_COMP           0x80

/* coeff for x1, X1000 */
#define X_C0    1138
#define X_C1    429
#define X_C2    -190

/* coeff for y1, X1000 */
#define Y_C0    733
#define Y_C1    697
#define Y_C2    -202

/* coeff for LUX */
#define LUX_C0  339 //X1000
#define LUX_C1  -2033 //X100
#define LUX_C2  2532    //X1000
#define LUX_C3  -1880   //X1000
#endif

#ifdef NEW_CCM
static u32 cal_lux(struct isl29124_data_t *dat, int *cct, u8 dbg)
{
	u32 lux;
	u8 bits;
	u16 r, g, b;

	r = dat->last_r;
	g = dat->last_g;
	b = dat->last_b;

	//bits = (dat->adc_resolution==0)? 1:0;a
	bits =0;
	if(dat->als_range_using == 0)
	{ // 375 lux
		lux = 10 * ( CCM_RangeLo[1][0]*r + CCM_RangeLo[1][1]*g + CCM_RangeLo[1][2] * b )/CCM_Gain[RangeLo][bits];
	}
	else
	{ // 10000 lux
		lux = 10 * ( CCM_RangeHi[1][0]*r + CCM_RangeHi[1][1]*g + CCM_RangeHi[1][2] * b )/CCM_Gain[RangeHi][bits];
	}

	*cct = cal_cct(dat);

	if(dbg)
	{
//		printk(KERN_ERR "r=%d, g=%d, b=%d, "
//			" lux=%d, cct=%d\n", r, g, b,
//			lux, *cct);
	}

	return lux;

}
#else
static u8 isl29124_comp_ref;
static u32 cal_lux(u16 r, u16 g1, u16 b, u16 g2, u8 range, int *cct, u8 dbg)
{
        int comp_diff;
        int delta_ir;   //delta_ir, X10000
        int tmp_s32_1, tmp_s32_2;
        int x,y;        //X1000
        int x1,y1;      //X1000
        u32 lux;
        int n;

        /* calculate the comp ref diff */
        comp_diff = (isl29124_comp_ref & 0x3f) - (ISL29124_COMP & 0x3f);

        /* calcualte the delta ir */
        tmp_s32_1 = 10000 - 10000 * g2 / g1;
        delta_ir = tmp_s32_1 / comp_diff;

        /* calcualte the x, y */
        x = 1000 * r / (r + g1 + b);
        y = 1000 * g1 / (r + g1 + b);

        /* calcualte the x1, y1, X1000 */
        x1 = (X_C0 * x + X_C1 * y + X_C2) / 1000;
        y1 = (Y_C0 * x + Y_C1 * y + Y_C2) / 1000;

        /* calculate the LUX out */
        tmp_s32_2 = 1000;

        tmp_s32_1 = LUX_C1 * delta_ir / 1000;   //X1000
        tmp_s32_2 = tmp_s32_2 + tmp_s32_1;
        tmp_s32_1 = LUX_C2 * x1 / 1000; //X1000
        tmp_s32_2 = tmp_s32_2 + tmp_s32_1;

        tmp_s32_1 = LUX_C3 * y1 / 1000; //X1000
        tmp_s32_2 = tmp_s32_2 + tmp_s32_1;

        tmp_s32_2 = tmp_s32_2 / 5; //X200

        tmp_s32_1 = LUX_C0 * g1 / 10; //X100
        tmp_s32_1 = tmp_s32_1 * tmp_s32_2; //X20000

        if(tmp_s32_1 < 0) tmp_s32_1 = 0;

        if(range == 0)
        {
                /* 330Lux full range */
                lux = 10 * tmp_s32_1 / 20000;
        }
        else
        {
                /* 4008 lux full range */
                lux = 10 * tmp_s32_1 / (20000 * 330 / 4000);
        }
        /* calculate cct */
        tmp_s32_1 = x1 - 332; //X1000;
        tmp_s32_2 = y1 - 186; //X1000;
        n = (tmp_s32_1 * 1000) / tmp_s32_2; //X1000;

        tmp_s32_1 = -449 * n / 1000; //X1
        tmp_s32_1 = tmp_s32_1 + 3525;
        tmp_s32_1 = tmp_s32_1 * n / 1000 - 6823; //X1
        *cct = n * tmp_s32_1 / 1000 + 5520;

        if(*cct < 0) *cct = 0;

        if(dbg)
        {
 //              printk(KERN_ERR "r=%d, g=%d, b=%d, g2=%d, delta_ir=%d, x=%d, y=%d, x1=%d, "
 //                       "y1=%d, n=%d, lux=%d, cct=%d\n", r, g1, b, g2, delta_ir, x, y, x1, y1,
 //                       n, lux, *cct);
        }

        return lux;

}
#endif 

ssize_t show_lux(struct device *dev, struct device_attribute *attr, char *buf)
{

        int ret;
        int range;
        int res;
        int cct;
	u8 dbg;
	u32 lux, lux1, lux2;
        unsigned short regr;
        unsigned short regg;
        unsigned short regb;
  //    unsigned short regg2;
        struct i2c_client *client = to_i2c_client(dev);
        struct isl29124_data_t *dat=dev_get_drvdata(dev);
        //dat = (struct isl29124_data_t *)dev->platform_data;

        mutex_lock(&rwlock_mutex);
        ret = isl29124_i2c_read_word16(client, RED_DATA_LBYTE_REG, &regr);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
                return -1;
        }

        ret = isl29124_i2c_read_word16(client, GREEN_DATA_LBYTE_REG, &regg);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
                return -1;
        }
        ret = isl29124_i2c_read_word16(client, BLUE_DATA_LBYTE_REG, &regb);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
                return -1;
        }
/****************
        ret = isl29124_i2c_read_word16(client, GREEN_DATA_LBYTE_REG, &regg2);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
                return -1;
        }
**********************/
        ret = get_optical_range(&range);
        ret = get_adc_resolution_bits(&res);
        dbg = 1;
        dat->last_r = regr;
        dat->last_g = regg;
        dat->last_b = regb;
        dat->adc_resolution = (res == 16? 0:1);
        dat->als_range_using = (range == 330? 0:1);
        lux1 = cal_lux(dat, &cct, dbg);
        lux2 = 3 * regr;
        if (lux1 <= 60000 && lux2 <= 60000) {
		lux = lux1;
	}
	else
		lux = lux2;
//	if(regr==65535 && regg==65535 && regb ==65535) lux = 300000;
//        if(lux > 655350) lux = 655350;
        sprintf(buf, "R=%d, G=%d, B=%d, CCT=%d, LUX=%d\n", regr, regg, regb, cct, lux);
        //sprintf(buf, "R=%d, G=%d, B=%d, CCT=%d, LUX=%d\n", regr, regg, regb, cct, lux);
        mutex_unlock(&rwlock_mutex);
        return strlen(buf);

}

static ssize_t show_cct(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
        u16 cct;
        int ret;
        int res;
	u8 dbg;
        int range;
	unsigned short regr;
        unsigned short regg;
        unsigned short regb;
        struct isl29124_data_t *isl29124=dev_get_drvdata(dev);
        struct i2c_client *client = to_i2c_client(dev);
	mutex_lock(&rwlock_mutex);
        ret = isl29124_i2c_read_word16(client, RED_DATA_LBYTE_REG, &regr);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
                return -1;
        }

        ret = isl29124_i2c_read_word16(client, GREEN_DATA_LBYTE_REG, &regg);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
                return -1;
        }
        ret = isl29124_i2c_read_word16(client, BLUE_DATA_LBYTE_REG, &regb);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
                return -1;
        }
	ret = get_optical_range(&range);
        ret = get_adc_resolution_bits(&res);
        dbg = 1;
        isl29124->last_r = regr;
        isl29124->last_g = regg;
        isl29124->last_b = regb;
        isl29124->adc_resolution = (res == 16? 0:1);
        isl29124->als_range_using = (range == 330? 0:1);
        cal_cct(isl29124);
        cct = isl29124->cct;
	mutex_unlock(&rwlock_mutex);

        return sprintf(buf,"%d\n", cct);
        //return snprintf(buf, PAGE_SIZE, "%d\n", cct);
}

/*
 * @fn          set_optical_range 
 *
 * @brief       This function sets the optical sensing range of sensor device 
 *
 * @return      Returns 0 on success otherwise returns an error (-1)
 *
 */
int set_optical_range(int *range)
{
        int ret;

        ret = i2c_smbus_read_byte_data(isl29124_client_data, CONFIG1_REG);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to get data\n", __FUNCTION__);
                return -1;
        }

        if(*range == 4000)
                ret |= RGB_SENSE_RANGE_4000_SET;
        else if (*range == 330)
                ret &= RGB_SENSE_RANGE_330_SET;
        else
                return -1;

        ret = i2c_smbus_write_byte_data(isl29124_client_data, CONFIG1_REG, ret);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
                return -1;
        }

        return 0;
}

/*
 * @fn          get_optical_range 
 *
 * @brief       This function gets the optical sensing range of sensor device 
 *
 * @return      Returns 0 on success otherwise returns an error (-1)
 *
 */

int get_optical_range(int *range)
{
        int ret;

        ret = i2c_smbus_read_byte_data(isl29124_client_data, CONFIG1_REG);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
                return -1;
        }

        *range = (ret & (1 << RGB_DATA_SENSE_RANGE_POS))?4000:330;

        return 0;

}

/*
 * @fn          get_adc_resolution_bits 
 *
 * @brief       This function gets the adc resolution of sensor device 
 *
 * @return      Returns 0 on success otherwise returns an error (-1)
 *
 */
#define ADC_RESOLUTION_BITS_POS 4
int get_adc_resolution_bits(int *res)
{
	int ret;

	ret = i2c_smbus_read_byte_data(isl29124_client_data, CONFIG1_REG); 
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}

	*res = (ret & (1 << ADC_RESOLUTION_BITS_POS ))?12:16;

	return 0;
}

/*
 * @fn          set_adc_resolution_bits 
 *
 * @brief       This function sets the adc resolution of sensor device 
 *
 * @return      Returns 0 on success otherwise returns an error (-1)
 *
 */
int set_adc_resolution_bits(int *res)
{
	int ret;
	int reg;

	reg = i2c_smbus_read_byte_data(isl29124_client_data, CONFIG1_REG);
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;
	}	

	if(*res)
		reg |= 1<<ADC_RESOLUTION_BITS_POS;
	else 
		reg &= ~(1<<ADC_RESOLUTION_BITS_POS);
	printk("%s,reg is %x\n",__func__,reg);
	ret = i2c_smbus_write_byte_data(isl29124_client_data, CONFIG1_REG, reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;
	}

	return 0;
}


/*
 * @fn          set_mode 
 *
 * @brief       This function sets the operating mode of sensor device 
 *
 * @return      Returns 0 on success otherwise returns an error (-1)
 *
 */
int set_mode(int mode)
{
	int ret;
	short int reg;

	reg = i2c_smbus_read_byte_data(isl29124_client_data, CONFIG1_REG);
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;
	}

	reg &= RGB_OP_MODE_CLEAR;
	reg |= mode;

	ret = i2c_smbus_write_byte_data(isl29124_client_data, CONFIG1_REG, reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__); 
		return -1;
	}

	return 0;
}


/*
 * @fn          autorange 
 *
 * @brief       This function processes autoranging of sensor device 
 *
 * @return      void 
 *
 */
void autorange(int green)
{
	int ret;
	unsigned int adc_resolution, optical_range;		

	ret = get_adc_resolution_bits(&adc_resolution);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to get adc resolution\n", __FUNCTION__);
		return;
	}

	ret = get_optical_range(&optical_range);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to get optical range\n", __FUNCTION__);
		return;
	}


	switch (adc_resolution) {
		case 12:
			switch(optical_range) {
				case 330:
					/* Switch to 4000 lux */
					if(green > 0xCCC) {
						optical_range = 4000;
						set_optical_range(&optical_range);
					}
					break;
				case 4000:
					/* Switch to 330 lux */
					if(green < 0xCC) {
						optical_range = 330;
						set_optical_range(&optical_range);
					}
					break;
			}
			break;
		case 16:
			switch(optical_range) {
				case 330:
					/* Switch to 4000 lux */
					if(green > 0xCCCC) {
						optical_range = 4000;
						set_optical_range(&optical_range);
					}

					break;
				case 4000:
					/* Switch to 330 lux */
					if(green < 0xCCC) {
						optical_range = 330;
						set_optical_range(&optical_range);

					}
					break;
			}
			break;
	}
}


/*
 * @fn         	isl29124_i2c_read_word16 
 *
 * @brief       This function reads a word (16-bit) from sensor device 
 *
 * @return      Returns 0 on success otherwise returns an error (-1)
 *
 */ 
int isl29124_i2c_read_word16(struct i2c_client *client, unsigned char reg_addr, unsigned short *buf)
{
	//short int reg_h;
	//short int reg_l;
	u8 dat[4];
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg_addr, 2, dat);
	if(ret != 2)
	{
		printk(KERN_ERR "%s: Failed to read block data\n", __FUNCTION__);
		return -1;
	}
	*buf = ((u16)dat[1] << 8) | (u16)dat[0];
/*
	reg_l = i2c_smbus_read_byte_data(client, reg_addr); 
	if (reg_l < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}

	reg_h = i2c_smbus_read_byte_data(client, reg_addr + 1); 
	if (reg_h < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}

	*buf = (reg_h << 8) | reg_l;	
*/
	return 0;
}  


/*
 * @fn         	isl29124_i2c_write_word16 
 *
 * @brief       This function writes a word (16-bit) to sensor device
 *
 * @return      Returns 0 on success otherwise returns an error (-1)
 *
 */
int isl29124_i2c_write_word16(struct i2c_client *client, unsigned char reg_addr, unsigned short *buf)
{
	int ret;
	unsigned char reg_h;
	unsigned char reg_l;

	/* Extract LSB and MSB bytes from data */
	reg_l = *buf & 0xFF;
	reg_h = (*buf & 0xFF00) >> 8;

	ret = i2c_smbus_write_byte_data(client, reg_addr, reg_l); 
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;	
	}

	ret = i2c_smbus_write_byte_data(client, reg_addr + 1, reg_h); 
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;	
	}

	return 0;
}


/*
 * @fn         	show_red 
 *
 * @brief       This function exports the RED Lux value in adc dec code to sysfs
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
ssize_t show_red(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned short reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	ret = isl29124_i2c_read_word16(client, RED_DATA_LBYTE_REG, &reg);	
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);	
		return -1;
	}

	sprintf(buf, "%d", reg);

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}


/*
 * @fn         	show_green 
 *
 * @brief       This function export GREEN Lux value in adc dec code to sysfs
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
ssize_t show_green(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned short reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	ret = isl29124_i2c_read_word16(client, GREEN_DATA_LBYTE_REG, &reg);	
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);	
		return -1;
	}

	/* Process autoranging of sensor */
	autorange(reg);

	sprintf(buf, "%d", reg);

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);

}


/*
 * @fn         	show_blue 
 *
 * @brief       This function export BLUE Lux value in adc dec code to sysfs 
 *
 * @return      Returns length of data buffer read on success otherwise returns an error (-1)
 *
 */
ssize_t show_blue(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned short reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	ret = isl29124_i2c_read_word16(client, BLUE_DATA_LBYTE_REG, &reg);	
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);	
		return -1;
	}

	sprintf(buf, "%d", reg); 

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);

}

ssize_t show_rgb(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned short regr;
	unsigned short regg;
	unsigned short regb;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	ret = isl29124_i2c_read_word16(client, RED_DATA_LBYTE_REG, &regr);	
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);	
		return -1;
	}

	ret = isl29124_i2c_read_word16(client, GREEN_DATA_LBYTE_REG, &regg);	
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);	
		return -1;
	}
	ret = isl29124_i2c_read_word16(client, BLUE_DATA_LBYTE_REG, &regb);	
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);	
		return -1;
	}
	sprintf(buf, "%d,%d,%d\n", regr,regg,regb);

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}
/*
 * @fn          show_mode
 *
 * @brief       This function displays the sensor operating mode 
 *
 * @return     	Returns the length of data buffer read on success otherwise returns an error (-1)
 *
 */
static ssize_t show_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG1_REG); 
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}
	printk("reg is %d\n",reg);
	switch (reg & 0x7) {
		case 0:
			sprintf(buf,"%s\n","pwdn"); 
			break;
		case 1:
			sprintf(buf,"%s\n","green"); 
			break;
		case 2:
			sprintf(buf, "%s\n","red"); 
			break;
		case 3:
			sprintf(buf, "%s\n","blue"); 
			break;
		case 4:
			sprintf(buf, "%s\n","standby"); 
			break;
		case 5:
			sprintf(buf, "%s\n","green.red.blue"); 
			break;
		case 6:
			sprintf(buf, "%s\n","green.red"); 
			break;
		case 7:
			sprintf(buf, "%s\n","green.blue"); 
			break;
	}

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}


/*
 * @fn        	store_mode 
 *
 * @brief       This function sets the sensor operating mode
 *
 * @return      Returns the length of buffer data written on success otherwise returns an error (-1)
 *
 */
static ssize_t store_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        int ret;
        int mode;
        int val;

        mutex_lock(&rwlock_mutex);
        val = simple_strtoul(buf, NULL, 10);
        if(val == 0) {
                mode = RGB_OP_PWDN_MODE_SET;
        } else if(val == 1) {
                mode = RGB_OP_GREEN_MODE_SET;
        } else if(val == 2) {
                mode = RGB_OP_RED_MODE_SET;
        } else if(val == 3) {
                mode = RGB_OP_BLUE_MODE_SET;
        } else if(val == 4) {
                mode = RGB_OP_STANDBY_MODE_SET;
        } else if(val == 5) {
                mode = RGB_OP_GRB_MODE_SET;
        } else if(val == 6) {
                mode = RGB_OP_GR_MODE_SET;
        } else if(val == 7) {
                mode = RGB_OP_GB_MODE_SET;
        } else {
                printk(KERN_ERR "%s: Invalid value\n", __FUNCTION__);
                mutex_unlock(&rwlock_mutex);
                return -1;
        }

        ret = set_mode(mode);
        if (ret < 0) {
                printk(KERN_ERR "%s: Failed to set operating mode\n", __FUNCTION__);
                mutex_unlock(&rwlock_mutex);
                return -1;
        }

        mutex_unlock(&rwlock_mutex);
        return strlen(buf);

}

/*
 * @fn        	show_optical_range 
 *
 * @brief       This function shows the current optical sensing range of sensor device
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t show_optical_range(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	int reg;

	mutex_lock(&rwlock_mutex);
	ret = get_optical_range(&reg);
	if(ret < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;
	}

	sprintf(buf, "%d", reg); 

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}

/*
 * @fn        	show_adc_resolution_bits 
 *
 * @brief       This function shows the current adc resolution of sensor device (12-bit or 16-bit)
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t show_adc_resolution_bits(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	int reg;

	mutex_lock(&rwlock_mutex);
	ret = get_adc_resolution_bits(&reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__); 
		return -1;
	}

	sprintf(buf, "%d", reg); 
	mutex_unlock(&rwlock_mutex);

	return strlen(buf);
}

/*
 * @fn        	store_adc_resolution_bits 
 *
 * @brief       This function sets the adc resolution of sensor device (12-bit or 16-bit)
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t store_adc_resolution_bits(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int reg;
	int val;
	mutex_lock(&rwlock_mutex);
	val = simple_strtoul(buf, NULL, 10);
	if(val == 16)
		reg = 0;
	else if(val == 12)
		reg = 1;
	else {
		printk(KERN_ERR "%s: Invalid value for adc resolution bits\n", __FUNCTION__);
	mutex_unlock(&rwlock_mutex);
		return count;
	}
	printk("%s,reg is %d,count is %d,buf is %d\n",__func__,reg,count,strlen(buf));
	ret = set_adc_resolution_bits(&reg);	
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to set adc resolution\n", __FUNCTION__);
	mutex_unlock(&rwlock_mutex);
		return count;

	}

	mutex_unlock(&rwlock_mutex);

	return strlen(buf);	
}


#ifdef ISL29124_INTERRUPT_MODE
/*
 * @fn         	show_intr_threshold_high
 *
 * @brief       This function shows the high interrupt threshold value in adc dec code
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t show_intr_threshold_high(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = isl29124_i2c_read_word16(client, HIGH_THRESHOLD_LBYTE_REG, &reg); 
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
		return -1;	
	}

	sprintf(buf, "%d", reg);

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}


/*
 * @fn         	store_intr_threshold_high 
 *
 * @brief       This function sets the high interrupt threshold value in adc dec code 
 *
 * @return      Returns length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t store_intr_threshold_high(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned short reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = simple_strtoul(buf, NULL, 10);
	if (reg == 0 || reg > 65535) {
		printk(KERN_ERR "%s: Invalid input value\n", __FUNCTION__);
		return -1;
	}
	ret = isl29124_i2c_write_word16(client, HIGH_THRESHOLD_LBYTE_REG, &reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write word\n", __FUNCTION__);
		return -1;
	}	

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);	

}

/*
 * @fn          show_intr_threshold_low
 *
 * @brief       This function shows the low interrupt threshold value in adc dec code
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t show_intr_threshold_low(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = isl29124_i2c_read_word16(client, LOW_THRESHOLD_LBYTE_REG, &reg); 
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
		return -1;	
	}

	sprintf(buf, "%d", reg); 

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}


/*
 * @fn          store_intr_threshold_low
 *
 * @brief       This function sets the low interrupt threshold value in adc dec code 
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t store_intr_threshold_low(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned short reg;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = simple_strtoul(buf, NULL, 10);
	if (reg == 0 || reg > 65536) {
		printk(KERN_ERR "%s: Invalid input value\n", __FUNCTION__);
		return -1;
	}

	ret = isl29124_i2c_write_word16(client, LOW_THRESHOLD_LBYTE_REG, &reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write word\n", __FUNCTION__);
		return -1;
	}	
	mutex_unlock(&rwlock_mutex);
	return strlen(buf);	
}

/*
 * @fn          show_intr_threshold_assign
 *
 * @brief       This function displays the color component for which the interrupts are generated
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t show_intr_threshold_assign(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG); 
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}

	/* Extract interrupt threshold assign value */
	reg = (reg & ((0x3) << INTR_THRESHOLD_ASSIGN_POS)) >> INTR_THRESHOLD_ASSIGN_POS;	

	switch(reg) {
		case 0:
			sprintf(buf, "%s", "none"); 
			break;
		case 1:
			sprintf(buf, "%s", "green"); 
			break;
		case 2:
			sprintf(buf, "%s", "red"); 
			break;
		case 3:
			sprintf(buf, "%s", "blue"); 
			break;
	}
	mutex_unlock(&rwlock_mutex);
	return strlen(buf);

}

/*
 * @fn          store_intr_threshold_assign
 *
 * @brief       This function displays the color component for which the interrupts are generated 
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t store_intr_threshold_assign(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	short int reg;
	short int threshold_assign;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	if(!strcmp(buf, "none")) {
		threshold_assign = INTR_THRESHOLD_ASSIGN_CLEAR;				  	
	} else if(!strcmp(buf, "green")) {
		threshold_assign = INTR_THRESHOLD_ASSIGN_GREEN;
	} else if(!strcmp(buf, "red")) {
		threshold_assign = INTR_THRESHOLD_ASSIGN_RED;
	} else if(!strcmp(buf, "blue")) {
		threshold_assign = INTR_THRESHOLD_ASSIGN_BLUE;
	} else {
		printk(KERN_ERR "%s: Invalid value\n", __FUNCTION__);
		return -1;
	}

	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG);
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;
	}

	reg &= INTR_THRESHOLD_ASSIGN_CLEAR;
	reg |= threshold_assign;	

	ret = i2c_smbus_write_byte_data(client, CONFIG3_REG, reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;
	}

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}

/*
 * @fn          show_intr_persistency
 *
 * @brief       This function displays the current interrupt persistency of sensor device
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t show_intr_persistency(struct device *dev, struct device_attribute *attr, char *buf)
{

	short int reg;
	short int intr_persist;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG); 
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}

	reg = (reg & (0x3 << INTR_PERSIST_CTRL_POS)) >> INTR_PERSIST_CTRL_POS;

	switch(reg) {
		case 0:
			intr_persist = 1;
			break;
		case 1:
			intr_persist = 2;
			break;
		case 2:
			intr_persist = 4;
			break;
		case 3:
			intr_persist = 8;
			break; 	

	}

	sprintf(buf, "%d", intr_persist); 
	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}

/*
 * @fn          store_intr_persistency
 *
 * @brief       This function sets the interrupt persistency of sensor device 
 *
 * @return      Returns size of buffer data on success otherwise returns an error (-1)
 *
 */
static ssize_t store_intr_persistency(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	short int reg;
	short int intr_persist;


	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	intr_persist = simple_strtoul(buf, NULL, 10);
	if (intr_persist == 8)
		intr_persist = INTR_PERSIST_SET_8;
	else if (intr_persist == 4)
		intr_persist = INTR_PERSIST_SET_4;
	else if (intr_persist == 2)
		intr_persist = INTR_PERSIST_SET_2;
	else if (intr_persist == 1)
		intr_persist = INTR_PERSIST_SET_1;
	else {
		printk(KERN_ERR "%s: Invalid value\n", __FUNCTION__);
		return -1;
	}

	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG);
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;
	}

	reg &= INTR_PERSIST_CTRL_CLEAR;
	reg |= intr_persist << INTR_PERSIST_CTRL_POS; 	

	ret = i2c_smbus_write_byte_data(client, CONFIG3_REG, reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;
	}

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}


/*
 * @fn         	show_rgb_conv_intr 
 *
 * @brief       This function displays the RGB conversion interrupt Enable Disable status
 *
 * @return      Returns the length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t show_rgb_conv_intr(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG); 
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}

	reg = (reg & (1 << RGB_CONV_TO_INTB_CTRL_POS)) >> RGB_CONV_TO_INTB_CTRL_POS;

	sprintf(buf, "%s", reg?"disable":"enable"); 

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}


/*
 * @fn        	store_rgb_conv_intr  
 *
 * @brief       This function Enables or Disables the RGB conversion interrupt 
 *
 * @return      Returns length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t store_rgb_conv_intr(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, rgb_conv_intr;
	short int reg;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	if(!strcmp(buf, "enable"))
		rgb_conv_intr = 0;
	else if(!strcmp(buf, "disable"))
		rgb_conv_intr = 1;
	else {
		printk(KERN_ERR "%s: Invalid value for rgb conversion interrupt\n", __FUNCTION__);
		return -1;
	}

	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG);
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;
	}	

	reg &= RGB_CONV_TO_INTB_CLEAR;
	reg |= rgb_conv_intr;

	ret = i2c_smbus_write_byte_data(client, CONFIG3_REG, reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;
	}

	mutex_unlock(&rwlock_mutex);
	return strlen(buf);	
}


/*
 * @fn          show_adc_start_sync
 *
 * @brief       This function Displays the adc start synchronization method
 *
 * @return     	Returns length of data buffer on success otherwise returns an error (-1) 
 *
 */
static ssize_t show_adc_start_sync(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG1_REG); 
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}

	reg = (reg & (1 << RGB_START_SYNC_AT_INTB_POS)) >> RGB_START_SYNC_AT_INTB_POS;

	sprintf(buf, "%s", reg?"risingIntb":"i2cwrite"); 
	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}
/*
 *
 *  @fn         store_adc_start_sync 
 *
 * @brief       This function sets the adc start synchronization method
 *
 * @return      Returns length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t store_adc_start_sync(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	short int reg, adc_start_sync;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	if(!strcmp(buf, "i2cwrite"))
		adc_start_sync = 0;
	else if(!strcmp(buf, "risingIntb"))
		adc_start_sync = 1;
	else {
		printk(KERN_ERR "%s: Invalid value for adc start sync\n", __FUNCTION__);
		return -1;
	}

	reg = i2c_smbus_read_byte_data(client, CONFIG1_REG);
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;
	}	

	if(adc_start_sync)
		reg |= ADC_START_AT_RISING_INTB;
	else 
		reg &= ADC_START_AT_I2C_WRITE;


	ret = i2c_smbus_write_byte_data(client, CONFIG1_REG, reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;
	}
	mutex_unlock(&rwlock_mutex);
	return strlen(buf);	

}
#endif


/*
 * @fn          show_ir_comp_ctrl
 *
 * @brief       This function Displays the IR compensation control status
 *
 * @return      Returns length of data buffer on success otherwise returns an error (-1) 
 *
 */
#define REG_02_IR_COM_POS 7

static ssize_t show_ir_comp_ctrl(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG2_REG); 
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}

	reg = (reg & (1 << REG_02_IR_COM_POS)) >> REG_02_IR_COM_POS;
	sprintf(buf, "%s", reg?"high":"low");
	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}

/*
 * @fn          store_ir_comp_ctrl
 *
 * @brief       This function Enables or Disables the IR compensation control 
 *
 * @return      Returns length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t store_ir_comp_ctrl(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	short int reg, ir_comp_ctrl;

	struct i2c_client *client = to_i2c_client(dev);                             


	mutex_lock(&rwlock_mutex);
	if(!strcmp(buf, "low"))
		ir_comp_ctrl = 0;
	else if(!strcmp(buf, "high"))
		ir_comp_ctrl = 1;
	else {
		printk(KERN_ERR "%s: Invalid value for rgb conversion interrupt\n", __FUNCTION__);
		mutex_unlock(&rwlock_mutex);
		return -1;
	}

	reg = i2c_smbus_read_byte_data(client, CONFIG2_REG);
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;
	}	

	reg &= ~(1<<REG_02_IR_COM_POS);
	reg |= ir_comp_ctrl<<REG_02_IR_COM_POS;

	ret = i2c_smbus_write_byte_data(client, CONFIG2_REG, (u8)reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;
	}
	mutex_unlock(&rwlock_mutex);
	return strlen(buf);	
}

/*
 * @fn          show_active_ir_comp
 *
 * @brief       This function Displays the active IR compensation value
 *
 * @return     	Returns length of data buffer on success otherwise returns an error (-1) 
 *
 */
static ssize_t show_active_ir_comp(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG2_REG); 
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}

	reg = reg & 0x3F;
	sprintf(buf, "%d", reg); 
	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}


/*
 * @fn          store_active_ir_comp
 *
 * @brief       This function sets the active IR compensation value 
 *
 * @return     	Returns length of data buffer on success otherwise returns an error (-1)
 *
 */
static ssize_t store_active_ir_comp(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned short reg;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&rwlock_mutex);
	reg = simple_strtoul(buf, NULL, 10);
	if (reg == 0 || reg > 63) {
		printk(KERN_ERR "%s: Invalid input value\n", __FUNCTION__);
		return -1;
	}
	ret = i2c_smbus_write_byte_data(client, CONFIG2_REG, (u8)reg); 
//	ret = isl29124_i2c_write_word16(client, CONFIG2_REG, &reg);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to write word\n", __FUNCTION__);
		return -1;
	}	
	mutex_unlock(&rwlock_mutex);
	return strlen(buf);	

}
#if SENSOR_INPUT
static ssize_t isl29124_show_enable_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	
	return sprintf(buf, "%d\n",sensor_enable);
}

static ssize_t isl29124_store_enable_sensor(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = simple_strtoul(buf, NULL, 10);
	
	printk(KERN_INFO "%s: isl29124 enable sensor -> %ld\n", __func__, val);

	if ((val != 0) && (val != 1))
	{
		return count;
	}
	
	if(val == 1)
	{
	sensor_enable = 1;
	mutex_lock(&rwlock_mutex);
	__cancel_delayed_work(&sensor_dwork);
	schedule_delayed_work(&sensor_dwork, msecs_to_jiffies(POLL_DELAY));	// 125ms
	mutex_unlock(&rwlock_mutex);
	}
	else
	if(sensor_enable == 1)
	{
	sensor_enable = 0;
	mutex_lock(&rwlock_mutex);
	__cancel_delayed_work(&sensor_dwork);
	mutex_unlock(&rwlock_mutex);
	}
	return count;
}
static ssize_t isl29124_show_delay(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	
	return sprintf(buf, "%ld\n",POLL_DELAY);
}

static ssize_t isl29124_store_delay(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = simple_strtoul(buf, NULL, 10);
	
	printk(KERN_INFO "%s: poll delay  -> %ld\n", __func__, val);
	mutex_lock(&rwlock_mutex);
	POLL_DELAY = val;
	mutex_unlock(&rwlock_mutex);
	return count;
}
#endif

static ssize_t show_reg_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;
	int i;
	struct i2c_client *client = to_i2c_client(dev);

	mutex_lock(&rwlock_mutex);
	*buf = 0;
	for(i=0; i<15 ; i++)
	{
		reg = i2c_smbus_read_byte_data(client, (u8)i);
		if (reg < 0) {
			printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
			return -1;
		}

		sprintf(buf, "%sreg%02x(%02x)\n", buf, i, reg);
	}
	mutex_unlock(&rwlock_mutex);
	return strlen(buf);
}

static ssize_t store_reg_dump(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	//unsigned long val = simple_strtoul(buf, NULL, 10);
	unsigned int reg, dat;
	int ret;	
	struct i2c_client *client = to_i2c_client(dev);                             

	sscanf(buf,"%02x %02x", &reg, &dat);
	mutex_lock(&rwlock_mutex);
 
	ret = i2c_smbus_write_byte_data(client, (u8)reg, (u8)dat);


	mutex_unlock(&rwlock_mutex);
	return count;
}

static DEVICE_ATTR(reg_dump, 0644, show_reg_dump, store_reg_dump); 
/* Attributes of ISL29124 RGB light sensor */
static DEVICE_ATTR(red, 0644 , show_red, NULL);
static DEVICE_ATTR(green, 0644 , show_green, NULL);
static DEVICE_ATTR(blue, 0644 , show_blue, NULL);
static DEVICE_ATTR(rgb, 0644 , show_rgb, NULL);

#if SENSOR_COM
static DEVICE_ATTR(cct, 0644 , show_cct, NULL);
static DEVICE_ATTR(lux, 0644 , show_lux, NULL);
#endif 

static DEVICE_ATTR(mode, 0644 , show_mode, store_mode);
static DEVICE_ATTR(optical_range, 0644, show_optical_range, NULL);
static DEVICE_ATTR(adc_resolution_bits, 0644 , show_adc_resolution_bits, store_adc_resolution_bits);

#ifdef ISL29124_INTERRUPT_MODE
static DEVICE_ATTR(intr_threshold_high , 0644 , show_intr_threshold_high, store_intr_threshold_high);
static DEVICE_ATTR(intr_threshold_low , 0644 , show_intr_threshold_low, store_intr_threshold_low);

static DEVICE_ATTR(intr_threshold_assign , 0644 , show_intr_threshold_assign, store_intr_threshold_assign);
static DEVICE_ATTR(intr_persistency, 0644 , show_intr_persistency, store_intr_persistency);
static DEVICE_ATTR(rgb_conv_intr, 0644 , show_rgb_conv_intr, store_rgb_conv_intr);

static DEVICE_ATTR(adc_start_sync, 0644 , show_adc_start_sync, store_adc_start_sync);
#endif

static DEVICE_ATTR(ir_comp_ctrl, 0644 , show_ir_comp_ctrl, store_ir_comp_ctrl);
static DEVICE_ATTR(active_ir_comp, 0644 , show_active_ir_comp, store_active_ir_comp);
#if SENSOR_INPUT
static DEVICE_ATTR(sensor_enable, 0644 ,isl29124_show_enable_sensor, isl29124_store_enable_sensor);
static DEVICE_ATTR(poll_delay, 0644 ,isl29124_show_delay, isl29124_store_delay);
#endif
static struct attribute *isl29124_attributes[] = {
	/* read RGB value attributes */
	&dev_attr_red.attr,
	&dev_attr_green.attr,
	&dev_attr_blue.attr,
	&dev_attr_rgb.attr,
#if SENSOR_COM
        &dev_attr_cct.attr,
        &dev_attr_lux.attr,
#endif 
	/* Device operating mode */ 
	&dev_attr_mode.attr,

	/* Current optical sensing range */
	&dev_attr_optical_range.attr,

	/* Current adc resolution */
	&dev_attr_adc_resolution_bits.attr,
#ifdef ISL29124_INTERRUPT_MODE
	/* Interrupt related attributes */
	&dev_attr_intr_threshold_high.attr,
	&dev_attr_intr_threshold_low.attr,
	&dev_attr_intr_threshold_assign.attr,
	&dev_attr_intr_persistency.attr,
	&dev_attr_rgb_conv_intr.attr,
	&dev_attr_adc_start_sync.attr,
#endif
	/* IR compensation related attributes */
	&dev_attr_ir_comp_ctrl.attr,
	&dev_attr_active_ir_comp.attr,
#if SENSOR_INPUT
	&dev_attr_sensor_enable.attr,
	&dev_attr_poll_delay.attr,
#endif
	&dev_attr_reg_dump.attr,
	NULL
};

static struct attribute_group isl29124_attr_group = {
	.attrs = isl29124_attributes
};


#ifdef ISL29124_INTERRUPT_MODE
/*
 * @fn          sensor_irq_thread 
 *
 * @brief       This thread is scheduled by sensor interrupt 
 *
 * @return     	void
 */
static void sensor_irq_thread(struct work_struct *work)
{

	short int reg, intr_assign, intr_threshold_low, intr_threshold_high;
	unsigned short int green, red, blue;
	int ret;

	/* Read the interrupt status flags from sensor */
	reg = i2c_smbus_read_byte_data(isl29124_client_data, STATUS_FLAGS_REG);
	if (reg < 0) {
		printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);	
		goto err_out;	
	}

	/* A threshold interrupt occured */
	if(reg & RGBTHF_FLAG_POS) {
		intr_assign = i2c_smbus_read_byte_data(isl29124_client_data, CONFIG3_REG);		
		if (intr_assign < 0) {
			printk(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);	
			goto err_out;	
		}

		intr_assign &= 0x3;

		if (intr_assign == INTR_THRESHOLD_ASSIGN_GREEN) {
			/* GREEN interrupt occured */								
			ret = isl29124_i2c_read_word16(isl29124_client_data, GREEN_DATA_LBYTE_REG, &green);	
			if (ret < 0) {
				printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);	
				goto err_out;
			}

			autorange(green);
		}		

	}

	if(reg & BOUTF_FLAG_POS) {
		/* Brownout interrupt occured */
		ret = i2c_smbus_read_byte_data(isl29124_client_data, STATUS_FLAGS_REG);
		if( ret < 0) {
			printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);	
			goto err_out;
		}

		ret &= ~(1 << BOUTF_FLAG_POS);

		/* Clear the BOUTF flag */
		ret = i2c_smbus_write_byte_data(isl29124_client_data, STATUS_FLAGS_REG, ret);
		if( ret < 0) {
			printk(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);	
			goto err_out;
		}

	}

err_out:
	enable_irq(irq_num);
}

/*
 * @fn          isl_sensor_irq_handler
 *
 * @brief       This function is the interrupt handler for sensor. It schedules and interrupt
 *              thread and resturns.
 *
 * @return      IRQ_HANDLED
 *
 */
static irqreturn_t isl_sensor_irq_handler(int irq, void *dev_id)
{
	disable_irq(irq_num);
	schedule_work(&work);	
	return IRQ_HANDLED;
}

#endif

/*
 * @fn          initialize_isl29124
 *
 * @brief       This function initializes the sensor device with default values
 *
 * @return      void
 *
 */
void initialize_isl29124(struct i2c_client *client)
{
	unsigned char reg;

#ifdef ISL29124_INTERRUPT_MODE
	/* Set device mode to RGB , 
	   RGB Data sensing range 4000 Lux,
	   ADC resolution 16-bit,
	   ADC start at intb start*/
	i2c_smbus_write_byte_data(client, CONFIG1_REG, 0x2D); 
#else
	/* Set device mode to RGB , 
	   RGB Data sensing range 4000 Lux,
	   ADC resolution 16-bit,
	   ADC start at i2c write 0x01*/
       //  i2c_smbus_write_byte_data(client, CONFIG1_REG, 0x0D);
           i2c_smbus_write_byte_data(client, CONFIG1_REG, 0x08);
#endif

	/* Default IR Active compenstation,
	   Disable IR compensation control */
	i2c_smbus_write_byte_data(client, CONFIG2_REG, 0x00);// changed by louis Mar 28 2014 

#ifdef ISL29124_INTERRUPT_MODE
	/* Interrupt threshold assignment for Green,
	   Interrupt persistency as 8 conversion data out of windows */
	i2c_smbus_write_byte_data(client, CONFIG2_REG, 0x1D); 

	/* Writing interrupt low threshold as 0xCCC (5% of max range) */
	i2c_smbus_write_byte_data(client, LOW_THRESHOLD_LBYTE_REG, 0xCC);	
	i2c_smbus_write_byte_data(client, LOW_THRESHOLD_HBYTE_REG, 0x0C);	

	/* Writing interrupt high threshold as 0xF333 (80% of max range)  */
	i2c_smbus_write_byte_data(client, HIGH_THRESHOLD_LBYTE_REG, 0xCC);	
	i2c_smbus_write_byte_data(client, HIGH_THRESHOLD_HBYTE_REG, 0xCC);	
#endif
	/* Clear the brownout status flag */
	reg = i2c_smbus_read_byte_data(client, STATUS_FLAGS_REG);
	reg &= ~(1 << BOUTF_FLAG_POS);
	i2c_smbus_write_byte_data(client, STATUS_FLAGS_REG, reg);		

}
#if SENSOR_VREG
static int isl29124_power_init(struct i2c_client *client)
{
	int ret;

	vdd = regulator_get(&client->dev, "vdd");
	if (IS_ERR(vdd)) {
		dev_info(&client->dev,
			"Regulator get failed vdd \n");
	} else if (regulator_count_voltages(vdd) > 0) {
		ret = regulator_set_voltage(vdd, GOODIX_VTG_MIN_UV,
					   GOODIX_VTG_MAX_UV);
		if (ret) {
			dev_err(&client->dev,
				"Regulator set_vtg failed vdd ret=%d\n", ret);
		}
	}

	vcc_i2c = regulator_get(&client->dev, "vcc_i2c");
	if (IS_ERR(vcc_i2c)) {
		dev_info(&client->dev,
			"Regulator get failed vdd \n");
	} else if (regulator_count_voltages(vcc_i2c) > 0) {
		ret = regulator_set_voltage(vdd, GOODIX_I2C_VTG_MIN_UV,
					   GOODIX_I2C_VTG_MAX_UV);
		if (ret) {
			dev_err(&client->dev,
				"Regulator set_vtg failed vdd ret=%d\n", ret);
		}
	}

	return 0;

}
static void isl29124_power_dinit(void)
{
regulator_put(vdd);
regulator_put(vcc_i2c);
}
static int isl29124_power_on(void)
{
	int ret = 0;

	if (!IS_ERR(vdd)) {
		printk("%s,vdd is correct",__func__);
		ret = regulator_enable(vdd);
		if (ret) {
			printk("%s,Regulator vdd enable failed ret=%d\n", __func__,ret);
		}
	}
	if (!IS_ERR(vcc_i2c)) {
		ret = regulator_enable(vcc_i2c);
		if (ret) {
			printk("%s,Regulator vdd enable failed ret=%d\n", __func__,ret);
		}
	}
	return ret;
}
static int isl29124_power_off(void)
{
	int ret = 0;
	if (!IS_ERR(vdd)) {
		ret = regulator_set_voltage(vdd, 0,GOODIX_VTG_MAX_UV);
		if (ret < 0)
			printk("%s,Regulator vdd set_vtg failed ret=%d\n",__func__,ret);
		ret = regulator_disable(vdd);
		if (ret)
		printk("%s,Regulator vdd disable failed ret=%d\n",__func__,ret);
}
	if (!IS_ERR(vcc_i2c)) {
		ret = regulator_set_voltage(vcc_i2c, 0,GOODIX_I2C_VTG_MAX_UV);
		if (ret < 0)
			printk("%s,Regulator vcc_i2c set_vtg failed ret=%d\n",__func__,ret);
		ret = regulator_disable(vcc_i2c);
		if (ret)
		printk("%s,Regulator vcc_i2c disable failed ret=%d\n",__func__,ret);
}
return ret;
}
#endif
#if SENSOR_INPUT
static void isl29124_work_handler(struct work_struct *work)
{
	unsigned short regr = 0;
	unsigned short regg = 0;
	unsigned short regb = 0;
	int ret;
	ret = isl29124_i2c_read_word16(isl29124_client_data, RED_DATA_LBYTE_REG, &regr);	
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);	
	}

	ret = isl29124_i2c_read_word16(isl29124_client_data, GREEN_DATA_LBYTE_REG, &regg);	
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);	
	}
	ret = isl29124_i2c_read_word16(isl29124_client_data, BLUE_DATA_LBYTE_REG, &regb);	
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);	
	}

	      input_report_abs(sensor_input, ABS_R, regr);
	      input_report_abs(sensor_input, ABS_G, regg);
	      input_report_abs(sensor_input, ABS_B, regb);
	      input_sync(sensor_input);
	schedule_delayed_work(&sensor_dwork, msecs_to_jiffies(POLL_DELAY));	// restart timer
}
#endif
/*
 * @fn          isl_sensor_probe
 *
 * @brief       This function is called by I2C Bus driver on detection of sensor device.
 *              It validates the device and initialize the resources required by driver. 
 *
 * @return      Returns 0 on success otherwise returns an error (-1)
 *
 */
static int __devinit isl_sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        int i = 0;
#ifdef ISL29124_INTERRUPT_MODE
	struct isl29124_platform_data *pdata;
#endif
	short int reg;
	int ret;
	struct isl29124_data_t *isl29124; 
#ifdef ISL29124_INTERRUPT_MODE
	pdata = client->dev.platform_data;
	if(pdata == NULL) {
		printk(KERN_ERR "%s: Unable to find platform data\n", __FUNCTION__);
		return -1;
	} 
#endif
	isl29124 = kzalloc(sizeof(struct isl29124_data_t), GFP_KERNEL);
	if(!isl29124)
	{
		printk(KERN_ERR "%s: failed to alloc memory for module data\n", __FUNCTION__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, isl29124);
 
printk("+++%s\n",__func__);
#if SENSOR_VREG
	isl29124_power_init(client);
	isl29124_power_on();
#endif
	/* Read the device id register from ISL29124 sensor device */
	mdelay(10);
	for(i = 0;i<10;i++)
	reg = i2c_smbus_read_byte_data(client, DEVICE_ID_REG);
	printk("%s,i2c client address is 0x%x,chip id is 0x%x",__func__,client->addr,reg);
	/* Verify whether we have a valid sensor */
	if( reg != ISL29124_DEV_ID) {
		printk(KERN_ERR "%s: Invalid device id for ISL29124 sensor device\n", __FUNCTION__);  
		goto err;
	}


#ifdef ISL29124_INTERRUPT_MODE
	/* Request gpio for sensor interrupt */
	ret = gpio_request(pdata->gpio_irq, "isl29124_intr");
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to request GPIO %d for ISL29124 sensor interrupt\n", __FUNCTION__, pdata->irq);
		goto gpio_err;

	}	

	/* Configure interrupt GPIO as input pin */
	ret = gpio_direction_input(pdata->gpio_irq);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to set direction for ISL29124 interrupt gpio\n", __FUNCTION__);
		goto err;
	}


	irq_num = gpio_to_irq(pdata->gpio_irq);

	if (irq_num < 0) {
		printk(KERN_ERR "%s: Failed to get IRQ number for ISL29124 sensor GPIO\n", __FUNCTION__);	
		goto err;	
	}


	/* Register irq handler for sensor */
	ret = request_irq(irq_num, isl_sensor_irq_handler, 0, "isl29124", NULL);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to register irq handler for ISL29124 sensor interrupt\n", __FUNCTION__);  
		goto err;
	}

	/* Initialize the sensor interrupt thread that would be scheduled by sensor
	   interrupt handler */
	INIT_WORK(&work, sensor_irq_thread);

#endif
#if SENSOR_INPUT
	sensor_input = input_allocate_device();
	if (!sensor_input) {
		printk("%s: Failed to allocate input device als\n", __func__);
	}
	set_bit(EV_ABS, sensor_input->evbit);
	input_set_abs_params(sensor_input, ABS_R, 0, 0xFFFF, 0, 0);
	input_set_abs_params(sensor_input, ABS_G, 0, 0xFFFF, 0, 0);
	input_set_abs_params(sensor_input, ABS_B, 0, 0xFFFF, 0, 0);
	sensor_input->name = "rgbsensor_isl29124_f";
	ret = input_register_device(sensor_input);
	if (ret) {
		printk("%s: Unable to register input device als: %s\n",
		       __func__,sensor_input->name);
	}
	INIT_DELAYED_WORK(&sensor_dwork, isl29124_work_handler); 
#endif
	/* Initialize the default configurations for ISL29124 sensor device */ 
	initialize_isl29124(client);

	/* Register sysfs hooks */                                                  
	ret = sysfs_create_group(&client->dev.kobj, &isl29124_attr_group);          
	if(ret) {                                                                   
		printk(KERN_ERR "%s: Failed to create sysfs\n", __FUNCTION__);                    
		goto err;                                                           
	}                                                                           

	/* Have a copy of i2c client information */
	isl29124_client_data = client;                                 

	/* Initialize a mutex for synchronization in sysfs file access */
	mutex_init(&rwlock_mutex);
	return 0;

#ifdef ISL29124_INTERRUPT_MODE
gpio_err:
	gpio_free(pdata->gpio_irq);
#endif

err: 
	return -1;
}

/*
 * @fn          isl_sensor_remove
 *
 * @brief       This function is called when sensor device gets removed from bus
 *
 * @return      Returns 0 
 *
 */
static int __devexit isl_sensor_remove(struct i2c_client *client)
{
	struct isl29124_data_t *isl29124 = i2c_get_clientdata(client);
#ifdef ISL29124_INTERRUPT_MODE
	/* Free interrupt number */
	free_irq(irq_num, NULL);

	/* Free requested gpio */
	gpio_free(ISL29124_INTR_GPIO);
#endif
#if SENSOR_VREG
	isl29124_power_off();
	isl29124_power_dinit();
#endif
	
	kfree(isl29124);	
	return 0;
}


/*
 * @fn          isl_sensor_suspend
 *
 * @brief       This function puts the sensor device in standby mode
 *
 * @return      Returns 0 on success otherwise returns an error (-1)
 *
 */
static int isl_sensor_suspend(struct i2c_client *client, pm_message_t msg)
{
	int ret;
	short int reg;

	reg = i2c_smbus_read_byte_data(client, CONFIG1_REG);
	if(reg < 0) {
		printk(KERN_ALERT "%s: Failed to read CONFIG1_REG\n", __FUNCTION__); 
		goto err;
	}		

	reg &= RGB_OP_MODE_CLEAR;
	reg |= RGB_OP_STANDBY_MODE_SET; 

	/* Put the sensor device in standby mode */
	ret = i2c_smbus_write_byte_data(client, CONFIG1_REG, reg);
	if (ret < 0) {
		printk(KERN_ALERT "%s: Failed to write to CONFIG1_REG\n", __FUNCTION__);
		goto err;
	}	 	

#ifdef ISL29124_INTERRUPT_MODE
	disable_irq(irq_num);	
#endif
	return 0;
err:
	return -1;
}


/*
 * @fn          isl_sensor_resume
 *
 * @brief       This function Resumes the sensor device from suspend mode and puts it in Active conversion mode
 *
 * @return      Returns 0 on success otherwise returns an error (-1)
 *
 */
static int isl_sensor_resume(struct i2c_client *client)
{

	int ret;
	short int reg;

	reg = i2c_smbus_read_byte_data(client, CONFIG1_REG);
	if(reg < 0) {
		printk(KERN_ALERT "%s: Failed to read CONFIG1_REG\n", __FUNCTION__); 
		goto err;
	}		

	reg &= RGB_OP_MODE_CLEAR;
	reg |= RGB_OP_GRB_MODE_SET; 

	/* Put the sensor device in active conversion mode */
	ret = i2c_smbus_write_byte_data(client, CONFIG1_REG, reg);
	if (ret < 0) {
		printk(KERN_ALERT "%s: Failed to write to CONFIG1_REG\n", __FUNCTION__);
		goto err;
	}	 	

#ifdef ISL29124_INTERRUPT_MODE
	enable_irq(irq_num);	
#endif
	return 0;
err:
	return -1;
}

#ifdef CONFIG_OF
static struct of_device_id isl_match_table[] = {
	{ .compatible = "isl29124_f",},
	{ },
};
#else
#define rmi4_match_table NULL
#endif

/* i2c device driver information */
static struct i2c_driver isl_sensor_driver = {
	.driver = {
		.name = "isl29124_f",
		.owner = THIS_MODULE,
		.of_match_table = isl_match_table,
	},
	.probe    = isl_sensor_probe	   ,
	.remove   = isl_sensor_remove	   ,
	.id_table = isl_sensor_device_table,
	.suspend  = isl_sensor_suspend	   ,
	.resume	  = isl_sensor_resume	   ,
};


/*
 *  @fn          isl29124_init
 *
 *  @brief       This function initializes the driver 
 *
 *  @return      Returns 0 on success otherwise returns an error (-1)
 *
 */
static int __init isl29124_init(void)
{

	/* Register i2c driver with i2c core */	
	return i2c_add_driver(&isl_sensor_driver);

}

/*
 * @fn          isl29124_exit
 *
 * @brief       This function is called to cleanup driver entry
 *
 * @return      Void
 *
 */
static void __exit isl29124_exit(void)
{
	/* Unregister i2c driver with i2c core */
	i2c_del_driver(&isl_sensor_driver);
}

module_init(isl29124_init);
module_exit(isl29124_exit);
