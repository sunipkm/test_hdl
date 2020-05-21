#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>

#define DEVNAME "interrupt_ip"

static irq_handler_t __interrupt_isr(int irq, void *dev_id, struct pt_regs *regs)
{
    printk(KERN_INFO DEVNAME ": ISR\n");
    return (irq_handler_t) IRQ_HANDLED;
}

static int __interrupt_ip_driver_probe(struct platform_device *pdev)
{
    int irq_num;
    irq_num = platform_get_irq(pdev, 0);
    printk(KERN_INFO DEVNAME ": IRQ %d about to be registered\n", irq_num);
    return request_irq(irq_num, (irq_handler_t) __interrupt_isr, 0, DEVNAME, NULL);
}

static int __interrupt_ip_driver_remove(struct platform_device *pdev)
{
    int irq_num;
    irq_num = platform_get_irq(pdev, 0);
    printk(KERN_INFO DEVNAME ": IRQ %d about to be freed\n", irq_num);
    free_irq(irq_num, NULL);
    return 0;
}

static const struct of_device_id __interrupt_ip_driver_id[] = {
    {.compatible = "xlnx,interrupt-ip-2.0"},
    {}
};

static struct platform_driver __interrupt_ip_driver = {
    .driver = {
        .name = DEVNAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(__interrupt_ip_driver_id),
    },
    .probe = __interrupt_ip_driver_probe,
    .remove = __interrupt_ip_driver_remove
};

module_platform_driver(__interrupt_ip_driver);

MODULE_LICENSE("GPL");