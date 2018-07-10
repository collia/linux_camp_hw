#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrii Tymkiv");
MODULE_DESCRIPTION("Say hello and goodbye");
MODULE_VERSION("0.22");
 
static char *name = "user";
static int  number_a = 0;
static int  number_b = 0;

module_param(name, charp, S_IRUGO); //just can be read
module_param(number_a, int, S_IRUGO); //just can be read
module_param(number_b, int, S_IRUGO); //just can be read


MODULE_PARM_DESC(name, "The name to display");  ///< parameter description
MODULE_PARM_DESC(number_a, "The first number");  ///< parameter description
MODULE_PARM_DESC(number_b, "The second number");  ///< parameter description
 
static int __init hello_init(void){
   printk(KERN_INFO "Hello, %s!\n", name);
   printk(KERN_INFO "Sum of numbers %d + %d = %d!\n", number_a, number_b, number_a + number_b);
   return 0;
}
 
static void __exit hello_exit(void){
   printk(KERN_INFO "Goodbye, %s !\n", name);
}
 
module_init(hello_init);
module_exit(hello_exit);
