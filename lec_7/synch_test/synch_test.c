#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/sched/signal.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nikolay Klimchuk");
MODULE_DESCRIPTION("Synchronozation primitives check module");
MODULE_VERSION("0.1");

#define SLEEP_TIMEOUT_S  10
#define TASK_NUMBER 4
static atomic_t tcounter = ATOMIC_INIT(0);

struct test_task_info {
        struct task_struct * task;
        struct semaphore lock;
        struct semaphore *next_lock;
        atomic_t *counter;
        struct completion task_done;
        bool need_sleep;
};

struct test_task_info tasks[TASK_NUMBER];


static int synch_test_thread(void *data)
{
        int rc;
        struct test_task_info *info = (struct test_task_info*)data;
        allow_signal(SIGKILL);
        //set_current_state(TASK_INTERRUPTIBLE); 
        while(!kthread_should_stop()){
                rc = down_interruptible(&info->lock);
                if(rc != 0 || kthread_should_stop()) {
                       break;
                }
                if(info->need_sleep){
                        rc = msleep_interruptible(SLEEP_TIMEOUT_S*1000);
                        if(rc != 0 || kthread_should_stop()){
                                break;        
                        }
                }
                atomic_inc(info->counter);
                printk(KERN_INFO "Task pid %x counter %d\n",
                       info->task->pid,
                       atomic_read(info->counter));
                up(info->next_lock);
        }
        printk(KERN_INFO "Task pid %x exited\n",
                       info->task->pid);
        complete_and_exit(&info->task_done,0);
}

static int __init synch_test_init(void)
{
        int result = 0;
        int i;
        for(i = 0; i < TASK_NUMBER; i++){
                tasks[i].task = kthread_create(
                        synch_test_thread,
                        &tasks[i],
                        "tthread_%d", i);
                if(IS_ERR(tasks[i].task)){
                        return -EBUSY;
                }
                sema_init(&tasks[i].lock, 0);
                tasks[i].counter = &tcounter;
                tasks[i].need_sleep = false;

                init_completion(&tasks[i].task_done);
        }
        for(i = 0; i < TASK_NUMBER - 1; i++){
                tasks[i].next_lock = &(tasks[i+1].lock);
        }
        tasks[TASK_NUMBER - 1].next_lock = &tasks[0].lock;
        tasks[0].need_sleep = true;
        for(i = 0; i < TASK_NUMBER; i++){
                wake_up_process(tasks[i].task);
        }
        up(&tasks[0].lock);
        return result;
}
 
static void __exit synch_test_exit(void)
{
        int rc = 0;
        int i;
        
        for(i = 0; i < TASK_NUMBER; i++){
                printk(KERN_INFO "Send signal to %d\n", i);
                send_sig(SIGKILL, tasks[i].task, 1);
                //signal_pending(tasks[i].task);
                //wake_up_process(tasks[i].task);
                kthread_stop(tasks[i].task);
                
        }
        // comletion is not nessesary, but I added it for lerning purposes
        for(i = 0; i < TASK_NUMBER; i++){
                rc = wait_for_completion_interruptible(&tasks[i].task_done);
                if(rc != 0){
                        printk(KERN_CRIT "Cannot stop task %d\n", i);
                }
        }
}
 
module_init(synch_test_init);
module_exit(synch_test_exit);
