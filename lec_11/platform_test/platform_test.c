#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <asm/io.h>


#define DRV_NAME  "plat_dummy"
/*Device has 2 resources:
* 1) 4K of memory at address 0x9f200000 - used for data transfer;
* 2) Four 32-bit registers at address 0x20001000;
*  2.1. Flag Register: 0x9f201000
*	bit 0: PLAT_IO_DATA_READY - set to 1 if data from device ready
*	other bits: reserved;
* 2.2. Data size Register: - Contain data size from device (0..4095);
* 2.3 Transfer data register 0x9f201008
* 2.4 Transfer size register 0x9f20100c
*/


#define MEM_SIZE	(2048)
#define REG_SIZE	(8*2)
#define DEVICE_POOLING_TIME_MS (500) /*500 ms*/
/**/
#define PLAT_INPUT_FLAG_REG		(0) /*Offset of input flag register*/
#define PLAT_INPUT_SIZE_REG		(4) /*Offset of input size register*/
#define PLAT_OUTPUT_FLAG_REG	(8) /*Offset of output flag register*/
#define PLAT_OUTPUT_SIZE_REG	(0xc) /*Offset of output size register*/
#define PLAT_IO_DATA_READY	(1) /*IO data ready flag */
#define MAX_DUMMY_PLAT_THREADS 1 /*Maximum amount of threads for this */


struct plat_dummy_device {
	void __iomem *read_mem;
	void __iomem *write_mem;
	void __iomem *regs;
	struct delayed_work     dwork;
	struct workqueue_struct *data_read_wq;
	struct workqueue_struct *data_write_wq;
	u64 js_pool_time;
};


static u32 plat_dummy_mem_read8(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread8(my_dev->read_mem + offset);
}

static void plat_dummy_mem_write8(struct plat_dummy_device *my_dev, u32 offset, u8 val)
{
        iowrite8(val, my_dev->write_mem + offset);
}

static u32 plat_dummy_reg_read32(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread32(my_dev->regs + offset);
}
static void plat_dummy_reg_write32(struct plat_dummy_device *my_dev, u32 offset, u32 val)
{
	iowrite32(val, my_dev->regs + offset);
}

static void plat_dummy_work(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i, size, read_status, write_status;
	u8 data;

	pr_info("++%s(%u)\n", __func__, jiffies_to_msecs(jiffies));

	my_device = container_of(work, struct plat_dummy_device, dwork.work);
	read_status = plat_dummy_reg_read32(my_device, PLAT_INPUT_FLAG_REG);

	if (read_status & PLAT_IO_DATA_READY) {
		size = plat_dummy_reg_read32(my_device, PLAT_INPUT_SIZE_REG);
		pr_info("%s: size = %d\n", __func__, size);

		if (size > MEM_SIZE)
			size = MEM_SIZE;

        write_status = plat_dummy_reg_read32(my_device, PLAT_OUTPUT_FLAG_REG);
        if (write_status & PLAT_IO_DATA_READY) {
                pr_info("%s: Data wasn't read from user side exiting\n", __func__);
                goto exit;
        }
		for(i = 0; i < size; i++) {
			data = plat_dummy_mem_read8(my_device, i);
			pr_info("%s: mem[%d] = 0x%x ('%c')\n", __func__,  i, data, data);
            data ^=0xff;
            plat_dummy_mem_write8(my_device, i, data);
		}
		rmb();
		read_status &= ~PLAT_IO_DATA_READY;
		plat_dummy_reg_write32(my_device, PLAT_INPUT_FLAG_REG, read_status);

        plat_dummy_reg_write32(my_device, PLAT_OUTPUT_SIZE_REG, size);
        write_status |= PLAT_IO_DATA_READY;
		plat_dummy_reg_write32(my_device, PLAT_OUTPUT_FLAG_REG, write_status);

	}
exit:
	queue_delayed_work(my_device->data_read_wq, &my_device->dwork, my_device->js_pool_time);
}
static const struct of_device_id plat_dummy_of_match[] = {
        {
                .compatible = "ti,plat_dummy",
        }, {
        },
 };


static int plat_dummy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct plat_dummy_device *my_device;
	struct resource *res;

	pr_info("++%s\n", __func__);

	my_device = devm_kzalloc(dev, sizeof(struct plat_dummy_device), GFP_KERNEL);
	if (!my_device)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res)
    {
            pr_info("res 0 error\n");    
            return -ENOMEM;
    }

	pr_info("res 0 = %zx..%zx\n", res->start, res->end);


	my_device->read_mem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->read_mem))
		return PTR_ERR(my_device->read_mem);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    if (!res)
    {
            pr_info("res 1 error\n");    
            return -ENOMEM;
    }

	pr_info("res 1 = %zx..%zx\n", res->start, res->end);

	my_device->write_mem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->write_mem))
		return PTR_ERR(my_device->write_mem);

    
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
    if (!res)
    {
            pr_info("res 2 error\n");    
            return -ENOMEM;
    }
    
	pr_info("res 2 = %zx..%zx\n", res->start, res->end);

	my_device->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->regs))
		return PTR_ERR(my_device->regs);

	platform_set_drvdata(pdev, my_device);

	pr_info("Read memory mapped to %p\n", my_device->read_mem);
	pr_info("Write memory mapped to %p\n", my_device->write_mem);
	pr_info("Registers mapped to %p\n", my_device->regs);

	/*Init data read WQ*/
	my_device->data_read_wq = alloc_workqueue("plat_dummy_read",
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!my_device->data_read_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&my_device->dwork, plat_dummy_work);
	my_device->js_pool_time = msecs_to_jiffies(DEVICE_POOLING_TIME_MS);
	queue_delayed_work(my_device->data_read_wq, &my_device->dwork, 0);

	return PTR_ERR_OR_ZERO(my_device->read_mem);
}

static int plat_dummy_remove(struct platform_device *pdev)
{
	struct plat_dummy_device *my_device = platform_get_drvdata(pdev);

	pr_info("++%s\n", __func__);

	if (my_device->data_read_wq) {
	/* Destroy work Queue */
		cancel_delayed_work_sync(&my_device->dwork);
		destroy_workqueue(my_device->data_read_wq);
	}

        return 0;
}


static struct platform_driver plat_dummy_driver = {
	.driver = {
		.name	= DRV_NAME,
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

