#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/ide.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/input.h>

#define KEY_NUM         1
//#define KEY_VALUE       KEY_0
#define KEY_VALUE       BTN_0
#define KEYINPUT_NAME   "keyinput"

static int keyinput_probe(struct platform_device *dev);
static int keyinput_remove(struct platform_device *dev);

struct irq_keydesc {
        int gpio;
        int irqnum;
        char label[10];
        int value;
        irqreturn_t (*handler)(int, void *);
        struct timer_list timer;
};
struct keyinput_dev_struct {
        struct device_node *node;
        struct irq_keydesc keyirq[KEY_NUM];
        struct input_dev *inputdev;
};
static struct keyinput_dev_struct keyinput_dev;

static const struct of_device_id keyinput_of_match[] = {
        { .compatible = "alientek, key", },
        {},
};

static struct platform_driver keyinput_driver = {
        .driver = {
                .name = "keyinput",
                .of_match_table = keyinput_of_match,
        },
        .probe = keyinput_probe,
        .remove = keyinput_remove,
};


void key0_timer_func(unsigned long arg)
{
        int value = 0;

        struct keyinput_dev_struct *dev = (struct keyinput_dev_struct *)arg;

        value = gpio_get_value(dev->keyirq[0].gpio);
        if (value) {
                //printk("key release!\n");
                input_event(dev->inputdev, EV_KEY, dev->keyirq[0].value, 0);
                input_sync(dev->inputdev);
        } else {
                //printk("key press!\n");
                input_event(dev->inputdev, EV_KEY, dev->keyirq[0].value, 1);
                input_sync(dev->inputdev);
        }
}

irqreturn_t key0_irq_handler(int irq, void *dev_id)
{
        struct keyinput_dev_struct *dev = (struct keyinput_dev_struct *)dev_id;

        dev->keyirq[0].timer.data = (unsigned long)dev_id;
        mod_timer(&dev->keyirq[0].timer, jiffies + msecs_to_jiffies(100));

        return IRQ_HANDLED;
}

static int keyinput_probe(struct platform_device *dev)
{
        int ret = 0;
        int i = 0;
        int num = 0, num_irq = 0;
        
        keyinput_dev.node = dev->dev.of_node;
        if (keyinput_dev.node == NULL) {
                ret = -EINVAL;
                goto fail_node;
        }

        for (i = 0; i < KEY_NUM; i++) {
                keyinput_dev.keyirq[i].gpio = of_get_named_gpio(keyinput_dev.node,
                                                                "key-gpios", i);
                if (keyinput_dev.keyirq[i].gpio < 0) {
                        ret = -EINVAL;
                        goto fail_get_named;
                }
        }

        for (i = 0; i < KEY_NUM; i++) {
                memset(keyinput_dev.keyirq[i].label, 0,
                       sizeof(keyinput_dev.keyirq[i].label));
                sprintf(keyinput_dev.keyirq[i].label, "KEY%d", i);
                ret = gpio_request(keyinput_dev.keyirq[i].gpio,
                                   keyinput_dev.keyirq[i].label);
                if (ret) {
                        ret = -EINVAL;
                        goto fail_gpio_request;
                }
                num ++;
        }

        for (i = 0; i < KEY_NUM; i++) {
                ret = gpio_direction_input(keyinput_dev.keyirq[i].gpio);
                if (ret < 0) {
                        ret = -EINVAL;
                        goto fail_dir_set;
                }
        }

        for (i = 0; i < KEY_NUM; i++) {
                keyinput_dev.keyirq[i].irqnum = gpio_to_irq(keyinput_dev.keyirq[i].gpio);
                printk("key%d:gpio=%d, irqnum=%d\n", i,
                       keyinput_dev.keyirq[i].gpio,
                       keyinput_dev.keyirq[i].irqnum);
        }

        keyinput_dev.keyirq[0].handler = key0_irq_handler;
        keyinput_dev.keyirq[0].value = KEY_VALUE;
        init_timer(&keyinput_dev.keyirq[0].timer);
        keyinput_dev.keyirq[0].timer.function = key0_timer_func;

        for (i = 0; i < KEY_NUM; i++) {
                ret = request_irq(keyinput_dev.keyirq[i].irqnum,
                                  keyinput_dev.keyirq[i].handler,
                                  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                                  keyinput_dev.keyirq[i].label,
                                  &keyinput_dev);
                if (ret) {
                        ret = -EINVAL;
                        goto fail_request_irq;
                }
                num_irq ++;
        }

        /* input 输入子系统 */
        /* 申请inputdev结构体 */
        keyinput_dev.inputdev = input_allocate_device();
        if (keyinput_dev.inputdev == NULL) {
                ret = -EINVAL;
                goto fail_input;
        }
        keyinput_dev.inputdev->name = KEYINPUT_NAME;

        /* 设置产生按键事件 */
        __set_bit(EV_KEY, keyinput_dev.inputdev->evbit);
        __set_bit(EV_REP, keyinput_dev.inputdev->evbit);
        /* 设置产生哪些按键值 */
        __set_bit(KEY_0, keyinput_dev.inputdev->keybit);
        __set_bit(BTN_0, keyinput_dev.inputdev->keybit);

        ret = input_register_device(keyinput_dev.inputdev);
        if (ret)
                goto fail_input;

        printk("keyinput probe success!\n");

        goto success;

fail_input:
fail_request_irq:
        for (i = 0; i < num_irq; i++) {
                free_irq(keyinput_dev.keyirq[i].irqnum, &keyinput_dev);
        }
fail_dir_set:
fail_gpio_request:
        for (i = 0; i < num; i++) {
                gpio_free(keyinput_dev.keyirq[i].gpio);
        }
fail_get_named:
fail_node:
success:
        return 0;
}

static int keyinput_remove(struct platform_device *dev)
{
        int i = 0;
        for (i = 0; i < KEY_NUM; i++) {
                free_irq(keyinput_dev.keyirq[i].irqnum, &keyinput_dev);
        }
        for (i = 0; i < KEY_NUM; i++) {
                gpio_free(keyinput_dev.keyirq[i].gpio);
        }
        input_unregister_device(keyinput_dev.inputdev);
        input_free_device(keyinput_dev.inputdev);
        return 0;
}

static int __init keyinput_init(void)
{
        return platform_driver_register(&keyinput_driver);
}

static void __exit keyinput_exit(void)
{
        platform_driver_unregister(&keyinput_driver);
}

module_init(keyinput_init);
module_exit(keyinput_exit);
MODULE_AUTHOR("wanglei");
MODULE_LICENSE("GPL");