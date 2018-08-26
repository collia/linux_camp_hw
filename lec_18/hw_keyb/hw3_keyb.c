// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for handling externally connected button and LED.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/sched/signal.h>
#include <linux/wait.h>
#include <linux/pm.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "hw3_keyb.h"

#define DRIVER_NAME	"hw3_keyb"
#define READ_BUF_LEN	2
#define SCAN_TIMEOUT_MS    50
#define MAX_SCAN_LINES_NUM 4
#define MAX_READ_LINES_NUM 4

struct hw3_keyb {
	struct miscdevice mdev;
	struct gpio_desc *read_gpio[MAX_READ_LINES_NUM];
	struct gpio_desc *scan_gpio[MAX_SCAN_LINES_NUM];
	int read_irqs[MAX_READ_LINES_NUM];
	u32 scan_line;
	char key_symbol;
	struct task_struct *scan_thread;
	spinlock_t lock;
	wait_queue_head_t wait;
	bool data_ready;	/* new data ready to read */
};

static const char read_gpio_lines[MAX_READ_LINES_NUM][8] = {
	"read0",
	"read1",
	"read2",
	"read3"
};

static const char scan_gpio_lines[MAX_SCAN_LINES_NUM][8] = {
	"scan0",
	"scan1",
	"scan2",
	"scan3",
};

static const char keyboard_symbols[MAX_READ_LINES_NUM][MAX_SCAN_LINES_NUM] = {
	{'*', '0', '#', 'D'},
	{'4', '5', '6', 'B'},
	{'7', '8', '9', 'C'},
	{'1', '2', '3', 'A'},
};

static inline struct hw3_keyb *to_hw3_keyb_struct(struct file *file)
{
	struct miscdevice *miscdev = file->private_data;

	return container_of(miscdev, struct hw3_keyb, mdev);
}

static ssize_t hw3_keyb_read(struct file *file, char __user * buf, size_t count,
			     loff_t * ppos)
{
	struct hw3_keyb *hw3_keyb = to_hw3_keyb_struct(file);
	unsigned long flags;
	ssize_t ret;
	char val[2];

	spin_lock_irqsave(&hw3_keyb->lock, flags);
	while (!hw3_keyb->data_ready) {
		spin_unlock_irqrestore(&hw3_keyb->lock, flags);
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible
		    (hw3_keyb->wait, hw3_keyb->data_ready))
			return -ERESTARTSYS;
		spin_lock_irqsave(&hw3_keyb->lock, flags);
	}
	val[0] = hw3_keyb->key_symbol;
	val[1] = 0;
	hw3_keyb->data_ready = false;
	spin_unlock_irqrestore(&hw3_keyb->lock, flags);

	/* Do not advance ppos, do not use simple_read_from_buffer() */
	if (copy_to_user(buf, val, READ_BUF_LEN))
		ret = -EFAULT;
	else
		ret = READ_BUF_LEN;

	return ret;
}

static __poll_t hw3_keyb_poll(struct file *file, poll_table * wait)
{
	struct hw3_keyb *hw3_keyb = to_hw3_keyb_struct(file);
	unsigned long flags;
	__poll_t mask = 0;

	poll_wait(file, &hw3_keyb->wait, wait);

	spin_lock_irqsave(&hw3_keyb->lock, flags);
	if (hw3_keyb->data_ready)
		mask = EPOLLIN | EPOLLRDNORM;
	spin_unlock_irqrestore(&hw3_keyb->lock, flags);

	return mask;
}

static const struct file_operations hw3_keyb_fops = {
	.owner = THIS_MODULE,
	.read = hw3_keyb_read,
	.poll = hw3_keyb_poll,
	.llseek = no_llseek,
};

static irqreturn_t hw3_keyb_read_isr(int irq, void *data)
{
	struct hw3_keyb *hw3_keyb = data;
	unsigned long flags;
	int irq_line = -1;
	int i;
	for (i = 0; i < MAX_READ_LINES_NUM; i++) {
		if (irq == hw3_keyb->read_irqs[i]) {
			irq_line = i;
		}
	}

	if (irq_line == -1) {
		return IRQ_NONE;
	}

	spin_lock_irqsave(&hw3_keyb->lock, flags);
	hw3_keyb->data_ready = true;
	//gpiod_get_value(hw3->btn_gpio);
	hw3_keyb->key_symbol = keyboard_symbols[irq_line][hw3_keyb->scan_line];
	spin_unlock_irqrestore(&hw3_keyb->lock, flags);

	wake_up_interruptible(&hw3_keyb->wait);

	return IRQ_HANDLED;
}

static int hw3_keyb_scan_thread_func(void *data)
{
	struct hw3_keyb *hw3_keyb = (struct hw3_keyb *)data;
	unsigned long flags;
	int i;

	/* Poll the button */
	while (!kthread_should_stop()) {
		spin_lock_irqsave(&hw3_keyb->lock, flags);

		hw3_keyb->scan_line++;
		if (hw3_keyb->scan_line >= MAX_SCAN_LINES_NUM) {
			hw3_keyb->scan_line = 0;
		}

		for (i = 0; i < MAX_SCAN_LINES_NUM; i++) {
			gpiod_set_value(hw3_keyb->scan_gpio[i], 1);
		}
		gpiod_set_value(hw3_keyb->scan_gpio[hw3_keyb->scan_line], 0);
		spin_unlock_irqrestore(&hw3_keyb->lock, flags);
		msleep_interruptible(SCAN_TIMEOUT_MS);
	}

	return 0;
}

static int hw3_keyb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct hw3_keyb *hw3_keyb;
	u32 debounce = 0;
	bool wakeup_source;
	int ret, i;

	hw3_keyb = devm_kzalloc(&pdev->dev, sizeof(*hw3_keyb), GFP_KERNEL);
	if (!hw3_keyb)
		return -ENOMEM;

	of_property_read_u32(node, "debounce-delay-ms", &debounce);

	/* "scanX-gpios" in dts */
	for (i = 0; i < MAX_SCAN_LINES_NUM; i++) {
		hw3_keyb->scan_gpio[i] =
		    devm_gpiod_get(dev, scan_gpio_lines[i], GPIOD_OUT_HIGH);
		if (IS_ERR(hw3_keyb->scan_gpio[i]))
			return PTR_ERR(hw3_keyb->scan_gpio[i]);
	}

	/* "readX-gpios" in dts */
	for (i = 0; i < MAX_READ_LINES_NUM; i++) {
		hw3_keyb->read_gpio[i] =
		    devm_gpiod_get(dev, read_gpio_lines[i], GPIOD_IN);
		if (IS_ERR(hw3_keyb->read_gpio[i]))
			return PTR_ERR(hw3_keyb->read_gpio[i]);

		hw3_keyb->read_irqs[i] = gpiod_to_irq(hw3_keyb->read_gpio[i]);
		if (hw3_keyb->read_irqs[i] < 0)
			return hw3_keyb->read_irqs[i];
		ret =
		    devm_request_irq(dev, hw3_keyb->read_irqs[i],
				     hw3_keyb_read_isr, IRQF_TRIGGER_FALLING,
				     dev_name(dev), hw3_keyb);
		if (ret < 0)
			return ret;

		if (debounce != 0) {
			ret =
			    gpiod_set_debounce(hw3_keyb->read_gpio[i],
					       debounce * 1000);
			if (ret < 0)
				dev_warn(dev, "No HW support for debouncing\n");
		}

	}

	wakeup_source = of_property_read_bool(node, "wakeup-source");

	device_init_wakeup(dev, wakeup_source);
	platform_set_drvdata(pdev, hw3_keyb);
	spin_lock_init(&hw3_keyb->lock);
	init_waitqueue_head(&hw3_keyb->wait);

	hw3_keyb->mdev.minor = MISC_DYNAMIC_MINOR;
	hw3_keyb->mdev.name = DRIVER_NAME;
	hw3_keyb->mdev.fops = &hw3_keyb_fops;
	hw3_keyb->mdev.parent = dev;
	ret = misc_register(&hw3_keyb->mdev);
	if (ret)
		return ret;

	hw3_keyb->scan_line = 0;
	hw3_keyb->scan_thread =
	    kthread_run(hw3_keyb_scan_thread_func, hw3_keyb,
			"hw3_keyb_scan_thread");
	if (IS_ERR(hw3_keyb->scan_thread)) {
		pr_err("kthread_run() failed\n");
		ret = PTR_ERR(hw3_keyb->scan_thread);
		return ret;
	}

	return 0;
}

static int hw3_keyb_remove(struct platform_device *pdev)
{
	struct hw3_keyb *hw3_keyb = platform_get_drvdata(pdev);

	misc_deregister(&hw3_keyb->mdev);
	return 0;
}

static const struct of_device_id hw3_keyb_of_match[] = {
	{.compatible = "globallogic,hw3_keyb"},
	{},			/* sentinel */
};

MODULE_DEVICE_TABLE(of, hw3_keyb_of_match);

static struct platform_driver hw3_keyb_driver = {
	.probe = hw3_keyb_probe,
	.remove = hw3_keyb_remove,
	.driver = {
		   .name = DRIVER_NAME,
		   .of_match_table = hw3_keyb_of_match,
		   },
};

module_platform_driver(hw3_keyb_driver);

MODULE_ALIAS("platform:hw3_keyb");
MODULE_AUTHOR("Nikolay Klimchuk <collia@email.ua>");
MODULE_DESCRIPTION("Homework#18");
MODULE_LICENSE("GPL");
