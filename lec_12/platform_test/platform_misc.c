#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/file.h>
#include <linux/string.h>


static int misc_dummy_open(struct inode *inode, struct file *filep)
{
	return nonseekable_open(inode, filep);
}

static int misc_dummy_close(struct inode *inode, struct file *filep)
{
	return 0;
}

ssize_t misc_dummy_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	const char *str = "This is misc read method\n";
	size_t len = strlen(str) +1;

	len = len < count ? len: count;
	if (copy_to_user(buf, str, len)) {
		return -EFAULT;
	}
	return len;
}


static const struct file_operations dummy_fops = {
	.owner		= THIS_MODULE,
	.open	= misc_dummy_open,
	.read	= misc_dummy_read,
	.release	= misc_dummy_close,
};

static struct miscdevice plat_dummy = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dummy_cdev",
	.fops = &dummy_fops,
};


static int __init platform_misc_init(void)
{
	int ret;

	/* register device */
	ret = misc_register(&plat_dummy);

	if (ret != 0) {
		pr_err("fail to misc_register (MISC_DYNAMIC_MINOR)\n");
		goto err_exit;
	}

err_exit:
	return ret;
}

static void  platform_misc_release(void)
{
	misc_deregister(&plat_dummy);
}

MODULE_LICENSE("GPL");
module_init(platform_misc_init);
module_exit(platform_misc_release);

