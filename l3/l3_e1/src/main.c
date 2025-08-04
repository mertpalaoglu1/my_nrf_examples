#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h> //printk için
int main(void)
{
 while (1) {
    printk("Hello World!\n");
    k_msleep(1000);
 }
}