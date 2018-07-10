#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/timer.h>
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nikolay Klimchuk");
MODULE_DESCRIPTION("Blink USR0 led");
MODULE_VERSION("0.1");

#define MAX_USR_LED_NUM 4
#define MIN_USR_LED_GPIO_NUM (21 + 32)

#define BLINK_TIMEOUT_MS  1000

static int led = 0;
static bool ledStatus = true;
static int gpioLED; 
static struct timer_list b_timer;

module_param(led, int, S_IRUGO); //just can be read


MODULE_PARM_DESC(led, "LED for blink");  ///< parameter description

static void blink_timer(struct timer_list *t)
{
        ledStatus = !ledStatus;
        gpio_direction_output(gpioLED, ledStatus);   // Set the gpio to be in output mode and on
        mod_timer(t,jiffies+ msecs_to_jiffies(BLINK_TIMEOUT_MS));
        printk("New LED %d status %d\n", led, ledStatus);
}

static int __init led_blinker_init(void){
        if(led >= MAX_USR_LED_NUM || led < 0){
                printk(KERN_INFO "%s: invalid led number\n", __func__);
                return -ENODEV;
        }
        
        gpioLED = led + MIN_USR_LED_GPIO_NUM;
        
        printk(KERN_INFO "LED %d, GPIO %d!\n", led, gpioLED);
        
        if (!gpio_is_valid(gpioLED)){
                printk(KERN_INFO "%s: invalid LED GPIO\n", __func__);
                return -ENODEV;
        }
        gpio_request(gpioLED, "sysfs");
        gpio_direction_output(gpioLED, ledStatus);  
        gpio_export(gpioLED, false);             

        timer_setup(&b_timer, blink_timer, 0);
        if(mod_timer(&b_timer,jiffies+ msecs_to_jiffies(BLINK_TIMEOUT_MS))){
                printk(KERN_INFO "%s: invalid LED GPIO\n", __func__);
                return -ENODEV;
        }
        //add_timer(&b_timer); // this is the same as mod_timer. TODO fill experation time field
        /*      __init_timer(&b_timer);
        b_timer.expires = jiffies+ msecs_to_jiffies(BLINK_TIMEOUT_MS);
        b_timer.function = blink_timer;
        b_timer.data = &b_timer;
        add_timer(&b_timer);*/
        return 0;
}
 
static void __exit led_blinker_exit(void){
        gpio_direction_output(gpioLED, false);
        
        gpio_unexport(gpioLED);
        gpio_free(gpioLED);
        del_timer_sync(&b_timer);
}
 
module_init(led_blinker_init);
module_exit(led_blinker_exit);
