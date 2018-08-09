#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include "platform_test.h"


#define DRV_NAME  "plat_dummy"
/*Device has 2 resources:
* 1) 4K of memory at address defined by dts - used for data transfer;
* 2) Two 32-bit registers at address (defined by dts)
*  2.1. Flag Register: @offset 0
*	bit 0: PLAT_IO_DATA_READY - set to 1 if data from device ready
*	other bits: reserved;
* 2.2. Data size Register @offset 4: - Contain data size from device (0..4095);
*/

/*Following has to be added to dts file to support it
*aliases {
*		dummy0 = &my_dummy1;
*		dummy1 = &my_dummy2;
*};
*
*my_dummy1: dummy@9f200000 {
*		compatible = "ti,plat_dummy";
*		reg = <0x9f200000 0x1000>,
*				<0x9f201000 0x8>;
*};
*
*my_dummy2: dummy@9f210000 {
*		compatible = "ti,plat_dummy";
*		reg = <0x9f210000 0x1000>,
*				<0x9f211000 0x8>;
*};
*
*/

#define MEM_SIZE	(0x1000)
#define REG_SIZE	(8)
#define DEVICE_POOLING_TIME_MS (500) /*500 ms*/

#define PLAT_READ_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_READ_SIZE_REG		(4) /*Offset of flag register*/
#define PLAT_WRITE_FLAG_REG		(8) /*Offset of flag register*/
#define PLAT_WRITE_SIZE_REG		(0xc) /*Offset of flag register*/

#define PLAT_IO_DATA_READY	(1) /*IO data ready flag */
#define MAX_DUMMY_PLAT_THREADS 1 /*Maximum amount of threads for this */

static struct plat_dummy_device *mydevs[DUMMY_DEVICES];

static u32 plat_dummy_mem_read8(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread8(my_dev->read_mem + offset);
}

static void plat_dummy_mem_write8(struct plat_dummy_device *my_dev, u32 offset, u8 value)
{
        iowrite8(value, my_dev->write_mem + offset);
}


static u32 plat_dummy_reg_read32(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread32(my_dev->regs + offset);
}
static void plat_dummy_reg_write32(struct plat_dummy_device *my_dev, u32 offset, u32 val)
{
	iowrite32(val, my_dev->regs + offset);
}

/* How much space is free? */
static int spacefree(struct plat_dummy_device *my_dev)
{
	if (my_dev->rp == my_dev->wp)
		return my_dev->buffersize - 1;
	return ((my_dev->rp + my_dev->buffersize - my_dev->wp) % my_dev->buffersize) - 1;
}

static ssize_t plat_dummy_read(struct plat_dummy_device *my_device, char __user *buf, size_t count)
{
	if (!my_device)
		return -EFAULT;

	if (mutex_lock_interruptible(&my_device->rd_mutex))
		return -ERESTARTSYS;

//	pr_info("++%s: my_device->rp = %p, my_device->wp = %p\n", __func__, my_device->rp, my_device->wp);

	while (my_device->rp == my_device->wp) { /* nothing to read */
		mutex_unlock(&my_device->rd_mutex); /* release the lock */
//		pr_info("\"%s\" reading: going to sleep\n", current->comm);
		if (wait_event_interruptible(my_device->rwq, (my_device->rp != my_device->wp)))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (mutex_lock_interruptible(&my_device->rd_mutex))
			return -ERESTARTSYS;
	}
	/* ok, data is there, return something */
//	pr_info("count = %zu, my_device->rp = %p, my_device->wp = %p\n", count, my_device->rp, my_device->wp);

	if (my_device->wp > my_device->rp)
		count = min(count, (size_t)(my_device->wp - my_device->rp));
	else /* the write pointer has wrapped, return data up to dev->end */
		count = min(count, (size_t)(my_device->end - my_device->rp));

	if (copy_to_user(buf, my_device->rp, count)) {
		mutex_unlock (&my_device->rd_mutex);
		return -EFAULT;
	}
	my_device->rp += count;
	if (my_device->rp == my_device->end)
		my_device->rp = my_device->buffer; /* wrapped */
	mutex_unlock (&my_device->rd_mutex);

//	pr_info("\"%s\" did read %li bytes\n",current->comm, (long)count);
	return count;
}

static ssize_t plat_dummy_write(struct plat_dummy_device *my_device, char __user *buf, size_t count)
{
	if (!my_device)
		return -EFAULT;

	if (mutex_lock_interruptible(&my_device->wr_mutex))
		return -ERESTARTSYS;


	/* ok, data is there, return something */
	pr_info("count = %zu, my_device->wrp = %p, my_device->wwp = %p\n", count, my_device->wrp, my_device->wwp);

    if(count >= my_device->buffersize)
            count = my_device->buffersize;
    
	if (my_device->wwp > my_device->wrp)
            count = min(count, (size_t)(my_device->wend - my_device->wwp));
    else if (my_device->wwp < my_device->wrp)
            count = min(count, (size_t)(my_device->wrp - my_device->wwp));
    
	if (copy_from_user( my_device->wwp,buf, count)) {
		mutex_unlock (&my_device->wr_mutex);
		return -EFAULT;
	}
	my_device->wwp += count;
	if (my_device->wwp == my_device->wend)
		my_device->wwp = my_device->write_buffer; /* wrapped */
	mutex_unlock (&my_device->wr_mutex);

	pr_info("\"%s\" did write %li bytes\n",current->comm, (long)count);
	return count;
}



/*intervals in ms*/
#define MIN_PULL_INTERVAL 10
#define MAX_PULL_INTERVAL 10000

int set_poll_interval(struct plat_dummy_device *my_device, u32 ms_interval)
{
	pr_info("++%s(%p)\n", __func__, my_device);
	if (!my_device)
		return -EFAULT;

	if ((ms_interval < MIN_PULL_INTERVAL) ||
	     (ms_interval > MAX_PULL_INTERVAL)) {
		pr_err("%s: Value out of range %d\n", __func__, ms_interval);
		return -EFAULT;
	}
	spin_lock(&my_device->pool_lock);
	my_device->js_pool_time = msecs_to_jiffies(ms_interval);
	spin_unlock(&my_device->pool_lock);
	pr_info("%s: Setting Poliing Interval to %d ms\n", __func__, ms_interval);
	return 0;
}

static void plat_dummy_read_work(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i, size, status, count;
	u64 js_time;

	my_device = container_of(work, struct plat_dummy_device, rdwork.work);

	//pr_info("++%s(%d)\n", __func__, jiffies_to_msecs(jiffies));
	spin_lock(&my_device->pool_lock);
	js_time = my_device->js_pool_time;
	spin_unlock(&my_device->pool_lock);


	status = plat_dummy_reg_read32(my_device, PLAT_READ_FLAG_REG);

	if (status & PLAT_IO_DATA_READY) {
	
		if (mutex_lock_interruptible(&my_device->rd_mutex))
			goto exit_wq;

		
		size = plat_dummy_reg_read32(my_device, PLAT_READ_SIZE_REG);
		//pr_info("++%s: size = %d\n", __func__, size);

		if (size > MEM_SIZE)
			size = MEM_SIZE;

		count = min((size_t)size, (size_t)spacefree(my_device));
		pr_info("count = %d\n", count);
		if (count < size) {
			mutex_unlock(&my_device->rd_mutex);
			goto exit_wq;
		}

		for(i = 0; i < count; i++) {
			*my_device->wp++ = plat_dummy_mem_read8(my_device, /*my_device->residue + */i);
			/*pr_info("%s: mem[%d] = 0x%x ('%c')\n", __func__, i, *(my_device->wp-1), *(my_device->wp-1));*/
			if (my_device->wp == my_device->end)
				my_device->wp = my_device->buffer; /* wrapped */
		}

		mutex_unlock (&my_device->rd_mutex);
		wake_up_interruptible(&my_device->rwq);
		rmb();
		status &= ~PLAT_IO_DATA_READY;
		plat_dummy_reg_write32(my_device, PLAT_READ_FLAG_REG, status);
	}
exit_wq:
	queue_delayed_work(my_device->data_read_wq, &my_device->rdwork, js_time);
}


static void plat_dummy_write_work(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i, size, status, count;
	u64 js_time;
    char data;

	my_device = container_of(work, struct plat_dummy_device, wdwork.work);

	//pr_info("++%s(%d)\n", __func__, jiffies_to_msecs(jiffies));
	spin_lock(&my_device->pool_lock);
	js_time = my_device->js_pool_time;
	spin_unlock(&my_device->pool_lock);


	status = plat_dummy_reg_read32(my_device, PLAT_WRITE_FLAG_REG);
    //pr_info("++%s(%d)\n", __func__, status);
    if ((status & PLAT_IO_DATA_READY) == 0) {
	
		if (mutex_lock_interruptible(&my_device->wr_mutex))
			goto exit_wq;


        if (my_device->wrp != my_device->wwp) {
                if (my_device->wwp > my_device->wrp)
                        count = (size_t)(my_device->wwp - my_device->wrp);
                else /* the write pointer has wrapped, return data up to dev->end */
                        count = (size_t)(my_device->wend - my_device->wrp);
                pr_info("++%s count %d\n", __func__, count);
                for(i = 0; i < count; i++) {
                        data = *my_device->wrp++;
                        plat_dummy_mem_write8(my_device, i, data);
                        if (my_device->wrp == my_device->wend)
                                my_device->wrp = my_device->write_buffer;
                }
                plat_dummy_reg_write32(my_device, PLAT_WRITE_SIZE_REG, count);

                mutex_unlock (&my_device->wr_mutex);
                wake_up_interruptible(&my_device->wwq);
                wmb();
                status |= PLAT_IO_DATA_READY;
                plat_dummy_reg_write32(my_device, PLAT_WRITE_FLAG_REG, status);
        } else {
            mutex_unlock (&my_device->wr_mutex);
    }
	} 
    
exit_wq:
	queue_delayed_work(my_device->data_write_wq, &my_device->wdwork, js_time);
}


static void dummy_init_data_buffer(struct plat_dummy_device *my_device)
{
	my_device->buffersize = DUMMY_IO_BUFF_SIZE;
	my_device->end = my_device->buffer + my_device->buffersize;
	my_device->rp = my_device->wp = my_device->buffer;
    my_device->wrp = my_device->wwp = my_device->write_buffer;
    my_device->wend = my_device->write_buffer + my_device->buffersize;
}

static const struct of_device_id plat_dummy_of_match[] = {
	{
		.compatible = "ti,plat_dummy",
	}, {
	},
 };

struct plat_dummy_device *get_dummy_platfrom_device(enum dummy_dev devnum)
{
	if (mydevs[devnum])
		return mydevs[devnum];

	return NULL;
}
EXPORT_SYMBOL(get_dummy_platfrom_device);


static int plat_dummy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct plat_dummy_device *my_device;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	int id;

	pr_info("++%s(%p)\n", __func__, pdev);

	if (!np) {
		pr_info("No device node found!\n");
		return -ENOMEM;
	}
	pr_info("Device name: %s\n", np->name);
	my_device = devm_kzalloc(dev, sizeof(struct plat_dummy_device), GFP_KERNEL);
	if (!my_device)
		return -ENOMEM;

	id = of_alias_get_id(np, "dummy");
	if (id < 0) {
		dev_err(&pdev->dev, "failed to get alias id: %d\n", id);
		return id;
	}
	pr_info("Device ID: %d\n", id);

	if (id >= DUMMY_DEVICES) {
		pr_err("Device ID is wrong!\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pr_info("res 0 = %zx..%zx\n", res->start, res->end);


	my_device->read_mem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->read_mem))
		return PTR_ERR(my_device->read_mem);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	pr_info("res 1 = %zx..%zx\n", res->start, res->end);

	my_device->write_mem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->write_mem))
		return PTR_ERR(my_device->write_mem);

    
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	pr_info("res 2 = %zx..%zx\n", res->start, res->end);

	my_device->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->regs))
		return PTR_ERR(my_device->regs);

	platform_set_drvdata(pdev, my_device);

	pr_info("Read memory mapped to %p\n", my_device->read_mem);
	pr_info("Write memory mapped to %p\n", my_device->write_mem);
	pr_info("Registers mapped to %p\n", my_device->regs);

	/*Init data read WQ*/
	my_device->data_read_wq = alloc_workqueue(res->name,
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!my_device->data_read_wq)
		return -ENOMEM;
		
	mutex_init(&my_device->rd_mutex);
	init_waitqueue_head(&my_device->rwq);

	/*Init data write WQ*/
	my_device->data_write_wq = alloc_workqueue(res->name,
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!my_device->data_write_wq)
		return -ENOMEM;
		
	mutex_init(&my_device->wr_mutex);
	init_waitqueue_head(&my_device->wwq);

    
    dummy_init_data_buffer(my_device);
	my_device->dummy_read = plat_dummy_read;
	my_device->dummy_write = plat_dummy_write;
	my_device->set_poll_interval = set_poll_interval;
	spin_lock_init(&my_device->pool_lock);
    
	INIT_DELAYED_WORK(&my_device->rdwork, plat_dummy_read_work);
	INIT_DELAYED_WORK(&my_device->wdwork, plat_dummy_write_work);
    
	my_device->js_pool_time = msecs_to_jiffies(DEVICE_POOLING_TIME_MS);
	queue_delayed_work(my_device->data_read_wq, &my_device->rdwork, 0);
	queue_delayed_work(my_device->data_read_wq, &my_device->wdwork, 0);
	my_device->pdev = pdev;
	mydevs[id] = my_device;

	return PTR_ERR_OR_ZERO(my_device->read_mem) ||PTR_ERR_OR_ZERO(my_device->write_mem);
}

static int plat_dummy_remove(struct platform_device *pdev)
{
	struct plat_dummy_device *my_device = platform_get_drvdata(pdev);

	pr_info("++%s(%p)\n", __func__, pdev);

	if (my_device->data_read_wq) {
	/* Destroy work Queue */
		cancel_delayed_work_sync(&my_device->rdwork);
        destroy_workqueue(my_device->data_read_wq);
		pr_info("Read delayed work cancelled\n");
    }
    if (my_device->data_write_wq) {
            /* Destroy work Queue */
            cancel_delayed_work_sync(&my_device->wdwork);
            destroy_workqueue(my_device->data_write_wq);
            pr_info("Write delayed work cancelled\n");
	}
	
	pr_info("--%s\n", __func__);
	return 0;
}

static struct platform_driver plat_dummy_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = plat_dummy_of_match,
	},
	.probe		= plat_dummy_probe,
	.remove		= plat_dummy_remove,
};

MODULE_DEVICE_TABLE(of, plat_dummy_of_match);

module_platform_driver(plat_dummy_driver);

MODULE_AUTHOR("Vitaliy Vasylskyy <vitaliy.vasylskyy@globallogic.com>");
MODULE_DESCRIPTION("Dummy platform driver");
MODULE_LICENSE("GPL");
