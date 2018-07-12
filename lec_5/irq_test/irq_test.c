#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nikolay Klimchuk");
MODULE_DESCRIPTION("Button check module");
MODULE_VERSION("0.1");

#define BUT_GPIO_NUM (8 + 2*32)

#define BLINK_TIMEOUT_MS  1000

static struct timer_list b_timer;
static unsigned int gpioButton = BUT_GPIO_NUM;
static unsigned int irqNumber; 

static irq_handler_t irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){
   printk(KERN_INFO "GPIO_TEST: Interrupt! (button state is %d)\n", gpio_get_value(gpioButton));
   return (irq_handler_t) IRQ_HANDLED;     
}


static void test_timer(struct timer_list *t)
{
        unsigned int butt_status =0;
        mod_timer(t,jiffies+ msecs_to_jiffies(BLINK_TIMEOUT_MS));
        butt_status = gpio_get_value(gpioButton);
        printk("button status %d\n", butt_status);
}

static int __init irq_test_init(void){

        int result;
        if (!gpio_is_valid(BUT_GPIO_NUM)){
                printk(KERN_INFO "%s: invalid GPIO\n", __func__);
                return -ENODEV;
        }
        gpio_request(gpioButton, "sysfs");
        gpio_direction_input(gpioButton);
        gpio_set_debounce(gpioButton, 200);  
        gpio_export(gpioButton, false);             

        irqNumber = gpio_to_irq(gpioButton);
        printk(KERN_INFO "GPIO_TEST: The button is mapped to IRQ: %d\n", irqNumber);
 
        
        timer_setup(&b_timer, test_timer, 0);
        if(mod_timer(&b_timer,jiffies+ msecs_to_jiffies(BLINK_TIMEOUT_MS))){
                printk(KERN_INFO "%s: invalid LED GPIO\n", __func__);
                return -ENODEV;
        }

         // This next call requests an interrupt line
        result = request_irq(irqNumber,             // The interrupt number requested
                             (irq_handler_t) irq_handler, // The pointer to the handler function below
                             IRQF_TRIGGER_RISING,   // Interrupt on rising edge (button press, not release)
                             "test_gpio_handler",    // Used in /proc/interrupts to identify the owner
                             NULL);                 // The *dev_id for shared interrupt lines, NULL is okay
        
        printk(KERN_INFO "GPIO_TEST: The interrupt request result is: %d\n", result);
        return result;
}
 
static void __exit irq_test_exit(void){
        gpio_unexport(gpioButton);
        gpio_free(gpioButton);
        del_timer_sync(&b_timer);
}
 
module_init(irq_test_init);
module_exit(irq_test_exit);
