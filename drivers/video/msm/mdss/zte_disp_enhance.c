#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include "mdss_dsi.h"

enum {
	INTENSITY_NORMAL=24,
	INTENSITY_01,
	INTENSITY_02
};

#define ZTE_DISP_ENHANCE_DEBUG

unsigned int zte_intensity_value;
struct mdss_dsi_ctrl_pdata *zte_mdss_dsi_ctrl = NULL;
void zte_send_cmd(struct dsi_cmd_desc *cmds);


#ifdef CONFIG_ZTEMT_MIPI_1080P_R63311_SHARP_IPS
static char sharpca_basic_9b[] = {0xca, 0x01,0x80, 0x98,0x98,0x9b,0x40,0xbe,0xbe,0x20,0x20,
	0x80,0xfe,0x0a, 0x4a,0x37,0xa0,0x55,0xf8,0x0c,0x0c,0x20,0x10,0x3f,0x3f,0x00,
	0x00,0x10,0x10,0x3f,0x3f,0x3f,0x3f,
};
static struct dsi_cmd_desc sharp_1080p_display_9b = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(sharpca_basic_9b)}, sharpca_basic_9b

};

static char sharpca_basic_92[] = {0xca, 0x01,0x80, 0x92,0x92,0x9b,0x75,0x9b,0x9b,0x20,0x20,
	0x80,0xfe,0x0a, 0x4a,0x37,0xa0,0x55,0xf8,0x0c,0x0c,0x20,0x10,0x3f,0x3f,0x00,
	0x00,0x10,0x10,0x3f,0x3f,0x3f,0x3f,
};

static struct dsi_cmd_desc sharp_1080p_display_92 = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(sharpca_basic_92)}, sharpca_basic_92

};
static char sharpca_basic_norm[] = {0xca, 0x00,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x08,0x20,
	0x80,0x80,0x0a, 0x4a,0x37,0xa0,0x55,0xf8,0x0c,0x0c,0x20,0x10,0x3f,0x3f,0x00,
	0x00,0x10,0x10,0x3f,0x3f,0x3f,0x3f,
};

static struct dsi_cmd_desc sharp_1080p_display_norm = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(sharpca_basic_norm)}, sharpca_basic_norm

};

void zte_sharp_r63311_disp_inc(unsigned int state)
{
	unsigned int value;
	value =state;
#ifdef ZTE_DISP_ENHANCE_DEBUG
	printk("lcd:%s value=%d\n", __func__, value);
#endif
	switch (value) {
	case INTENSITY_NORMAL:
		zte_send_cmd(&sharp_1080p_display_norm);
		break;
	case INTENSITY_01:
		zte_send_cmd(&sharp_1080p_display_92);
		break;
	case INTENSITY_02:
		zte_send_cmd(&sharp_1080p_display_9b);
		break;
	default:
		zte_send_cmd(&sharp_1080p_display_92);
		break;

	}
	
}
#endif

void zte_send_cmd(struct dsi_cmd_desc *cmds)
{
  struct dcs_cmd_req cmdreq;

  if(!zte_mdss_dsi_ctrl)
  {
    pr_err("lcd:faild:%s zte_mdss_dsi_ctrl is null\n",__func__);
    return;
  }

  memset(&cmdreq, 0, sizeof(cmdreq));
  cmdreq.cmds = cmds;
  cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

  mdss_dsi_cmdlist_put(zte_mdss_dsi_ctrl, &cmdreq);
}

void zte_mipi_disp_inc(unsigned int state)
{
#ifdef CONFIG_ZTEMT_MIPI_1080P_R63311_SHARP_IPS
  zte_sharp_r63311_disp_inc(state);
#endif
}

static ssize_t intensity_show(struct kobject *kobj, struct kobj_attribute *attr,
   char *buf)
{
	//snprintf(buf, 50, "%u\n", zte_intensity_value);

	return 0;
}
static ssize_t intensity_store(struct kobject *kobj, struct kobj_attribute *attr,
    const char *buf, size_t size)
{
	ssize_t ret = 0;
	int val;
	static bool is_firsttime = true;
	sscanf(buf, "%d", &val);

#ifdef ZTE_DISP_ENHANCE_DEBUG
	pr_debug("lcd:%s state=%d size=%d\n", __func__, (int)val, (int)size);
#endif
	zte_intensity_value = val;
	if (is_firsttime) {
		is_firsttime = false;
		return ret;
	}
	zte_mipi_disp_inc(val);

	return ret;
}

static struct kobj_attribute disptype_attribute =
 __ATTR(disptype, 0664, intensity_show, intensity_store);

 static struct attribute *attrs[] = {
  &disptype_attribute.attr,
  NULL, /* need to NULL terminate the list of attributes */
 };
 static struct attribute_group attr_group = {
  .attrs = attrs,
 };
 
 static struct kobject *id_kobj;
 
 static int __init id_init(void)
 {
  int retval;
 
  id_kobj = kobject_create_and_add("lcd_enhance", kernel_kobj);
  if (!id_kobj)
   return -ENOMEM;
 
  /* Create the files associated with this kobject */
  retval = sysfs_create_group(id_kobj, &attr_group);
  if (retval)
   kobject_put(id_kobj);

  zte_mdss_dsi_ctrl = NULL;
 
  return retval;
 }
 
 static void __exit id_exit(void)
 {
  kobject_put(id_kobj);
 }
 
 module_init(id_init);
 module_exit(id_exit);
 
