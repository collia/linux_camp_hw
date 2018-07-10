#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nikolay Klimchuk");
MODULE_DESCRIPTION("work queue testing");
MODULE_VERSION("0.1");

#define TICK_TIMEOUT_MS  1000

static struct timer_list b_timer;

static void do_delayed_work(struct work_struct *dummy);
static void do_work(struct work_struct *dummy);

static DECLARE_DELAYED_WORK(delayed_work, do_delayed_work); 
static DECLARE_WORK(work, do_work);

static unsigned long previous_jiffies;

static void do_delayed_work(struct work_struct *dummy)
{
        unsigned int time_delay = jiffies - previous_jiffies;
        printk(KERN_INFO "Delay for wq %d ms", jiffies_to_msecs(time_delay));
}
static void do_work(struct work_struct *dummy)
{
        previous_jiffies = jiffies;
}


static void test_timer(struct timer_list *t)
{
        schedule_delayed_work(&delayed_work, msecs_to_jiffies(TICK_TIMEOUT_MS/10));
        schedule_work(&work);
                              
        mod_timer(t,jiffies+ msecs_to_jiffies(TICK_TIMEOUT_MS));
}

static int __init wq_test_init(void){

        timer_setup(&b_timer, test_timer, 0);
        if(mod_timer(&b_timer,jiffies+ msecs_to_jiffies(TICK_TIMEOUT_MS))){
                printk(KERN_INFO "%s: invalid LED GPIO\n", __func__);
                return -ENODEV;
        }
        return 0;
}
 
static void __exit wq_test_exit(void){
        del_timer(&b_timer);
}
 
module_init(wq_test_init);
module_exit(wq_test_exit);
