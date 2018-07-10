#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nikolay Klimchuk");
MODULE_DESCRIPTION("tasklet test function");
MODULE_VERSION("0.1");


#define CHECK_TIMEOUT_MS  10

static struct hrtimer test_hrtimer;
static u64 counter = 0;
static u64 pass_counter = 0;
static bool was_hi_prio_called = false;

static spinlock_t hrtimer_lock;
static spinlock_t tasklet_lock;

////////////
// tasklet config

static void hi_pr_tasklet_ev(unsigned long);
static void lo_pr_tasklet_ev(unsigned long);        

DECLARE_TASKLET(hi_pr_tasklet, hi_pr_tasklet_ev, 0);
DECLARE_TASKLET(lo_pr_tasklet, lo_pr_tasklet_ev, 0);


static void hi_pr_tasklet_ev(unsigned long dummy)
{
        unsigned long flags = 0;
        spin_lock_irqsave(&hrtimer_lock, flags);
        was_hi_prio_called = true;
        spin_unlock_irqrestore(&hrtimer_lock, flags);
}
static void lo_pr_tasklet_ev(unsigned long dummy)
{
        unsigned long flags = 0;

        if(!was_hi_prio_called)
        {
                printk(KERN_CRIT "Low priority  tasklet has been called before hight priority!\n");
        }
        else
        {
                pass_counter++;
        }
        spin_lock_irqsave(&hrtimer_lock, flags);
        was_hi_prio_called = false;
        spin_unlock_irqrestore(&hrtimer_lock, flags);

}


////////////
// timer config
static enum hrtimer_restart test_timer_handler(struct hrtimer *hrtimer)
{
        
        unsigned long flags = 0;
        spin_lock_irqsave(&hrtimer_lock, flags);
        counter++;
        spin_unlock_irqrestore(&hrtimer_lock, flags);
        tasklet_schedule(&lo_pr_tasklet);
        tasklet_hi_schedule(&hi_pr_tasklet);
        hrtimer_forward_now(hrtimer,
                            ms_to_ktime(CHECK_TIMEOUT_MS));
        return HRTIMER_RESTART;
}

static int init_test_timer(void)
{
        spin_lock_init(&hrtimer_lock);
        spin_lock_init(&tasklet_lock);
        hrtimer_init(&test_hrtimer,
                     CLOCK_MONOTONIC,
                     HRTIMER_MODE_REL);
        test_hrtimer.function = test_timer_handler;
        hrtimer_start(&test_hrtimer,  ms_to_ktime(CHECK_TIMEOUT_MS), HRTIMER_MODE_REL);
        return 0;
}



static int __init tasklet_test_init(void)
{
        init_test_timer();
        return 0;
}
 
static void __exit tasklet_test_exit(void)
{
        int rc = 0;
        rc = hrtimer_cancel(&test_hrtimer);
        if(rc == 0)
                printk(KERN_INFO "Timer was inactive");
        else if(rc == 1)
                printk(KERN_INFO "Timer was active");
        else if(rc == -1)
                printk(KERN_INFO "Timer cannot be stopped");
        printk(KERN_INFO "Exiting. rc = %d, tests = %lld passed = %lld\n", rc, counter, pass_counter);
        
}
 
module_init(tasklet_test_init);
module_exit(tasklet_test_exit);
