#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/of_gpio.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
///////////////
#include <linux/module.h>

#include <linux/types.h>

#include <linux/cdev.h>

#include <linux/semaphore.h>
#include <linux/device.h>

#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <linux/gpio.h>

#include <linux/sched.h>

#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

#include <linux/workqueue.h>
#include <../../../drivers/staging/android/timed_output.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/of_gpio.h>

#include "zte_tps65132.h"


#define TPS65132_DRIVER_NAME	"tps65132"

#define NXP_TPS65132_DEBUG

#define DRIVER_DESC	"NFC driver for TPS65132"

#define MAX_BUFFER_SIZE		512
#define TPS65132_MSG_MAX_SIZE	0x21 /* at normal HCI mode */

/* Timing restrictions (ms) */
#define TPS65132_RESETVEN_TIME	35 /* 7 */

//struct tps65132_nfc_platform_data tps65132_nfc_platform_data;

enum tps65132_irq {
	TPS65132_NONE,
	TPS65132_INT,
};

struct stps65132_dev	{
	wait_queue_head_t	read_wq;
	//struct tps65132_nfc_platform_data *pdata;
	struct mutex read_mutex;
	struct i2c_client	*client;
	struct miscdevice	miscdev;
	bool irq_enabled;
	spinlock_t irq_enabled_lock;
	enum tps65132_irq read_irq;
	
	int updata_gpio;
	int ven_gpio;
	int irq_gpio;
	int (*request_resources) (void);
	void (*free_resources) (void);
	void (*enable) (int fw);
	int (*test) (void);
	void (*disable) (void);
	int (*irq_status) (void);
};

static struct stps65132_dev * tps65132_dev;

static int tps65132_write_reg_val(unsigned char reg, unsigned char val)
{
	int i = 0;
	int err = 0;

	while (i < 3)
	{
#ifdef NXP_TPS65132_DEBUG
		pr_debug("%s: 0x%x:0x%x", __FUNCTION__,reg, val);
#endif
		err = i2c_smbus_write_byte_data(tps65132_dev->client, reg, val);
		if(err < 0){
			printk(KERN_ERR"%s, err=%d\n", __FUNCTION__, err);
			break;
		}
		i++;
	}

	return err;
}

static unsigned char tps65132_read_reg( unsigned char reg)
{
	return i2c_smbus_read_byte_data(tps65132_dev->client, reg);
}

void tps65132_set_output_voltage(int positive_voltage, int negative_voltage)
{
	int pos_val=0;
	int neg_val=0;
	int regff_val=0;

	pos_val= ( positive_voltage - 4000)/100;
	neg_val= ( negative_voltage - 4000)/100;	

#ifdef NXP_TPS65132_DEBUG
	pr_debug("[tps]%s:positive_voltage=%d,negative_voltage=%d, ",__FUNCTION__,positive_voltage,negative_voltage);
	pr_debug("pos_val=0x%x , neg_val= 0x%x ~~~~~~~~~\n",pos_val,neg_val);
#endif

	tps65132_write_reg_val(0x00,pos_val);
	tps65132_write_reg_val(0x01,neg_val);
	regff_val=tps65132_read_reg(0xff)	;
#ifdef NXP_TPS65132_DEBUG
	pr_debug("regff_val=0x%x~~~~~~~~~\n",regff_val);	
#endif
	regff_val |=(1<<8);
#ifdef NXP_TPS65132_DEBUG
	pr_debug("regff_val=0x%x~~~~~~~~~\n",regff_val);
#endif
	tps65132_write_reg_val(0xff,regff_val);	

}

void  tps65132_show_reg(void )
{
	int val0,val1,regff_val;
	 val0=tps65132_read_reg(0x00);
	 val1=tps65132_read_reg(0x01);
	 regff_val=tps65132_read_reg(0xff);
#ifdef NXP_TPS65132_DEBUG
	pr_debug("[tps]%s:reg00=0x%x, reg01=0x%x ,regff_val= 0x%x~~~~~~~~~~~~\n",__FUNCTION__,val0,val1,regff_val);
#endif	
}

static __devinit int tps65132_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int irq_gpio = -1;
	int updata_gpio = -1;
	int ven_gpio = -1;
	int ret;

#ifdef NXP_TPS65132_DEBUG
   pr_debug("[tps] %s:line%d +\n",__FUNCTION__,__LINE__);
#endif

	if (tps65132_dev != NULL) {
		dev_warn(&client->dev, "only one tps65132 supported.\n");
		return -EBUSY;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}
	
	tps65132_dev = kzalloc(sizeof(struct stps65132_dev), GFP_KERNEL);
	if (tps65132_dev == NULL) {
		dev_err(&client->dev, "failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	
	tps65132_dev->client = client;
	tps65132_dev->irq_gpio = irq_gpio;
	tps65132_dev->updata_gpio = updata_gpio;
	tps65132_dev->ven_gpio = ven_gpio;
	
	i2c_set_clientdata(client, tps65132_dev);

#if defined(CONFIG_ZTEMT_MIPI_1080P_R63311_SHARP_IPS)
	//tps65132_set_output_voltage(5600,5600);
	tps65132_set_output_voltage(5000,5000);
#else
  tps65132_set_output_voltage(4900,4900);
#endif


#ifdef NXP_TPS65132_DEBUG
   pr_debug("[tps] %s:line%d -\n",__FUNCTION__,__LINE__);
#endif

	return 0;

err_exit:
	return ret;
	
}

static __devexit int tps65132_remove(struct i2c_client *client)
{
	struct stps65132_dev *dev = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "%s\n", __func__);

	if (dev->disable)
		dev->disable();

	mutex_destroy(&dev->read_mutex);
	kfree(dev);

	tps65132_dev = NULL;
	
	return 0;
}

static struct of_device_id tps_match_table[] = {
	{ .compatible = "tps,tps65132_i2c_adapter",},
	{ },
};

static const struct i2c_device_id tps65132_id_table[] = {
	{ TPS65132_DRIVER_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tps65132_id_table);

static struct i2c_driver tps65132_driver = {
	.id_table	= tps65132_id_table,
	.probe		= tps65132_probe,
	.remove		= tps65132_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= 	TPS65132_DRIVER_NAME,
		.of_match_table = tps_match_table,
	},
};

module_i2c_driver(tps65132_driver);

MODULE_DESCRIPTION("I2C_TEST_TPS");
MODULE_LICENSE("GPL");

