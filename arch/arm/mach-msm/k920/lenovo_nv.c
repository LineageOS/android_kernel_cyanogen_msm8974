#include <linux/device.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <mach/msm_smem.h>

#define SMEM_ID_VENDOR_READ_NV	SMEM_ID_VENDOR1

#define NV_MAX_SIZE		512

#define NV_WIFI_ADDR_SIZE	6
#define NV_BT_ADDR_SIZE		6
#define NV_SN_SIZE		32
#define NV_MEID_SIZE		16
#define NV_IMEI_SIZE		16
#define NV_HWID_SIZE		16
#define NV_STATION_SIZE		32
#define NV_GLOBALMODE_SIZE	8
#define NV_UEMODE_SIZE		8
#define NV_2498_SIZE		100
#define NV_OTHERS_SIZE		(NV_MAX_SIZE - NV_WIFI_ADDR_SIZE	\
				- NV_BT_ADDR_SIZE - 2*NV_SN_SIZE	\
				- NV_MEID_SIZE - 2*NV_IMEI_SIZE		\
				- NV_HWID_SIZE - NV_STATION_SIZE	\
				- NV_GLOBALMODE_SIZE - NV_UEMODE_SIZE	\
				- NV_2498_SIZE)

#define NV_CLASS_NAME		"lenovo"
#define NV_DEVICE_NAME		"nv"

static struct class *lenovo_nv_class = NULL;
static struct device *lenovo_nv_device = NULL;

struct smem_nv {
	unsigned char nv_wifi[NV_WIFI_ADDR_SIZE];
	unsigned char nv_bt[NV_BT_ADDR_SIZE];
	unsigned char nv_sn1[NV_SN_SIZE];
	unsigned char nv_sn2[NV_SN_SIZE];
	unsigned char nv_meid[NV_MEID_SIZE];
	unsigned char nv_imei1[NV_IMEI_SIZE];
	unsigned char nv_imei2[NV_IMEI_SIZE];
	unsigned char nv_hwid[NV_HWID_SIZE];
	unsigned char nv_station[NV_STATION_SIZE];
	unsigned char nv_globalmode_status[NV_GLOBALMODE_SIZE];
	unsigned char nv_uemode[NV_GLOBALMODE_SIZE];
	unsigned char nv_2498[NV_2498_SIZE];
	unsigned char nv_others[NV_OTHERS_SIZE];	
};
static struct smem_nv *psmem_nv = NULL;

static int lenovo_nv_read_psmem_nv(void)
{
	if (!psmem_nv) {
		psmem_nv = smem_alloc(SMEM_ID_VENDOR_READ_NV, NV_MAX_SIZE);
		if (!psmem_nv) {
			pr_err("%s: Failed to retreive nv data!\n",
				__func__);
			return -ENODEV;
		}
	}

	return 0;
}

/*
 * Used by prima wlan driver to retreive WIFI MAC address
 */
int wlan_get_nv_mac(char *buf)
{
	int retval;

	retval = lenovo_nv_read_psmem_nv();
	if (retval < 0) {
		pr_err("%s: Failed to retreive mac address from NV!\n",
			__func__);
		return retval;
	}

        memcpy(buf, psmem_nv->nv_wifi, NV_WIFI_ADDR_SIZE);
	return 0;
}
EXPORT_SYMBOL_GPL(wlan_get_nv_mac);


static ssize_t lenovo_nv_show_wifi_mac(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%02x %02x %02x %02x %02x %02x\n",
		psmem_nv->nv_wifi[0], psmem_nv->nv_wifi[1], psmem_nv->nv_wifi[2],
		psmem_nv->nv_wifi[3], psmem_nv->nv_wifi[4], psmem_nv->nv_wifi[5]);
}

static ssize_t lenovo_nv_show_bt_mac(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%02x %02x %02x %02x %02x %02x\n",
		psmem_nv->nv_bt[0], psmem_nv->nv_bt[1], psmem_nv->nv_bt[2],
		psmem_nv->nv_bt[3], psmem_nv->nv_bt[4], psmem_nv->nv_bt[5]);
}

static ssize_t lenovo_nv_show_sn1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_sn1);
}

static ssize_t lenovo_nv_show_sn2(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_sn2);
}

static ssize_t lenovo_nv_show_meid(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_meid);
}

static ssize_t lenovo_nv_show_imei1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_imei1);
}

static ssize_t lenovo_nv_show_imei2(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_imei2);
}

static ssize_t lenovo_nv_show_hwid(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_hwid);
}

static ssize_t lenovo_nv_show_station(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!lenovo_nv_read_psmem_nv())
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_station);
}

static ssize_t lenovo_nv_show_globalmode_status(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_globalmode_status);
}

static ssize_t lenovo_nv_show_uemode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_uemode);
}

static ssize_t lenovo_nv_show_2498(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_2498);
}

static ssize_t lenovo_nv_show_others(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (lenovo_nv_read_psmem_nv() < 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", psmem_nv->nv_others);
}

static struct device_attribute attrs[] = {
	__ATTR(nv_wifi_mac, S_IRUGO,
		lenovo_nv_show_wifi_mac, NULL),
	__ATTR(nv_bt_mac, S_IRUGO,
		lenovo_nv_show_bt_mac, NULL),
	__ATTR(nv_sn1, S_IRUGO,
		lenovo_nv_show_sn1, NULL),
	__ATTR(nv_sn2, S_IRUGO,
		lenovo_nv_show_sn2, NULL),
	__ATTR(nv_meid, S_IRUGO,
		lenovo_nv_show_meid, NULL),
	__ATTR(nv_imei1, S_IRUGO,
		lenovo_nv_show_imei1, NULL),
	__ATTR(nv_imei2, S_IRUGO,
		lenovo_nv_show_imei2, NULL),
	__ATTR(nv_hwid, S_IRUGO,
		lenovo_nv_show_hwid, NULL),
	__ATTR(nv_station, S_IRUGO,
		lenovo_nv_show_station, NULL),
	__ATTR(nv_globalmode_status, S_IRUGO,
		lenovo_nv_show_globalmode_status, NULL),
	__ATTR(nv_uemode, S_IRUGO,
		lenovo_nv_show_uemode, NULL),
	__ATTR(nv_2498, S_IRUGO,
		lenovo_nv_show_2498, NULL),
	__ATTR(nv_others, S_IRUGO,
		lenovo_nv_show_others, NULL),
};

static int __init lenovo_nv_init(void)
{
	int retval = 0;
	unsigned char attr_count;

	lenovo_nv_class = class_create(THIS_MODULE, NV_CLASS_NAME);
	if (lenovo_nv_class == NULL || IS_ERR(lenovo_nv_class))
	{
		retval = PTR_ERR(lenovo_nv_class);
		pr_err("%s: Failed to create class for lenovo_nv, ret=%d\n",
			__func__, retval);
		goto err;
	}

	lenovo_nv_device = device_create(lenovo_nv_class, NULL,
				MKDEV(0, 0), NULL, NV_DEVICE_NAME);
	if (lenovo_nv_device == NULL || IS_ERR(lenovo_nv_device))
	{
		retval = PTR_ERR(lenovo_nv_device);
		pr_err("%s: Failed to create lenovo_nv device, ret=%d\n",
			__func__, retval);
		goto err;
	}	

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&lenovo_nv_device->kobj,
						&attrs[attr_count].attr);
		if (retval < 0) {
			pr_err("%s: Failed to create sysfs attributes, ret=%d\n",
				__func__, retval);
			goto err_sysfs;
		}
	}

	return retval;

err_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&lenovo_nv_device->kobj,
					&attrs[attr_count].attr);
	}
err:
	return retval;
}

module_init(lenovo_nv_init);
//module_exit(lenovo_nv_exit);
MODULE_DESCRIPTION("SMEM Lenovo NV Data reader");
MODULE_LICENSE("GPL");
