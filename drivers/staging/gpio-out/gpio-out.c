#include <linux/err.h>
// #include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
// #include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>

#include <asm/uaccess.h> /* copy_*_user */
#include <linux/cdev.h>
#include <linux/errno.h> /* error codes */
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/fs.h>    /* everything... */
#include <linux/init.h>
#include <linux/kernel.h> /* printk() */
#include <linux/module.h>
#include <linux/slab.h>  /* kmalloc() */
#include <linux/types.h> /* size_t */

struct gpio_learn_res
{
    struct device_node *node;  // 设备树节点
    struct gpio_desc *gpio[8]; // gpio
    struct cdev dev;           // cdev
    struct class *class;
    int gpioValue[8];
    dev_t devt;
};

static const struct of_device_id gpio_learn_id[] = {
    {.compatible = "gpio-out"},
    {/* sentinel */},
};

MODULE_DEVICE_TABLE(of, gpio_learn_id);

static long gpio_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct gpio_learn_res *res = filp->private_data;
    if (arg < 4)
    {
        switch (cmd)
        {
        case 0:
            gpiod_direction_output(res->gpio[arg], 0);
            break;

        case 1:
        default:
            gpiod_direction_output(res->gpio[arg], 0);
            break;
        }
        return 0;
    }
    printk(KERN_ERR "arg must < 4\r\n");
    return -EFAULT;
}

ssize_t getvalue(struct file *filp, char __user *buf, size_t count,
                 loff_t *f_pos)
{
    // struct gpio_learn_res *res = (struct gpio_learn_res *)filp->private_data;
    //buf[0] = gpiod_get_value(gpio_to_desc(194));
    //*f_pos++;
    char buff[10] = {};
    struct gpio_learn_res *res = filp->private_data;
    int nbytes;
    if (count > 5)
    {
        count = 5;
    }
    buff[0] = res->gpioValue[0] + '0';
    buff[1] = res->gpioValue[1] + '0';
    buff[2] = res->gpioValue[2] + '0';
    buff[3] = res->gpioValue[3] + '0';
    buff[4] = 0;
    nbytes = copy_to_user(buf, buff, 5);
    *f_pos += count;
    return count;
}

ssize_t setvalue(struct file *filp, const char __user *buf, size_t count,
                 loff_t *f_pos)
{
    // printk("set vaule:%s\r\n", buf[0]);
    //struct gpio_desc *io = gpio_to_desc(194);
    struct gpio_learn_res *res = filp->private_data;
    char buff[10];
    int i;
    int nbytes;
    size_t countsave;
    countsave = count;
    if (count > 4)
    {
        count = 4;
    }
    nbytes = copy_from_user(buff, buf, count);
    // printk(KERN_INFO "set value:%s", buff);
    if (count >= nbytes)
    {
        if (count > 4)
        {
            count = 4;
        }
        for (i = 0; i < count; i++)
        {
            if (buff[i] >= '0')
            {
                buff[i] = buff[i] - '0';
            }
            gpiod_direction_output(res->gpio[i], buff[i]);
            res->gpioValue[i] = buff[i];
        }
    }
    return countsave;
}

int release(struct inode *inode, struct file *filp)
{
    // printk(KERN_DEBUG "process %i (%s) success release minor(%u) file\n", current->pid, current->comm, iminor(inode));
    return 0;
}

/*
 * Open and close
 */

int open(struct inode *inode, struct file *filp)
{
    //  struct scull_dev *dev = (struct scull_dev *)container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = container_of(inode->i_cdev, struct gpio_learn_res, dev);

    //  printk(KERN_DEBUG "process %i (%s) success open minor(%u) file, private_data->0x%X\n", current->pid, current->comm, iminor(inode), filp->private_data);
    return 0;
}
struct file_operations gpio_fops = {
    .owner = THIS_MODULE,
    .read = getvalue,
    .write = setvalue,
    .unlocked_ioctl = gpio_ioctl,
    .open = open,
    .release = release,
};

static int gpio_learn_probe(struct platform_device *pdev)
{
    struct gpio_learn_res *gpiores;
    struct device_node *node;
    int i;
    int result;

    printk("gpio-learn match ok->%s\r\n", pdev->name);

    gpiores = kmalloc(sizeof(struct gpio_learn_res), GFP_KERNEL);
    if (!gpiores)
    {
        printk(KERN_INFO "GPIO alloc error\r\n");
        return 0;
    }

    node = of_find_node_by_path("/gpio-out");
    if (!node)
    {
        printk(KERN_ERR "Failed to find node \"/gpio-out\"\r\n");
        return 0;
    }
    gpiores->node = node;

    gpiores->gpio[0] = devm_gpiod_get(&pdev->dev, "out1", GPIOD_OUT_LOW);
    gpiores->gpio[1] = devm_gpiod_get(&pdev->dev, "out2", GPIOD_OUT_LOW);
    gpiores->gpio[2] = devm_gpiod_get(&pdev->dev, "out3", GPIOD_OUT_LOW);
    gpiores->gpio[3] = devm_gpiod_get(&pdev->dev, "out4", GPIOD_OUT_LOW);

    for (i = 0; i < 4; i++)
    {
        if (IS_ERR(gpiores->gpio[i]))
        {
            int ret = PTR_ERR(gpiores->gpio[i]);
            printk("gpiod_get out%d error %i\n", i, ret);
            gpiores->gpio[i] = NULL;
        }
    }

    gpiod_direction_output(gpiores->gpio[0], 1);

    result = alloc_chrdev_region(&gpiores->devt, 0, 1, "gpio-out");

    if (result < 0)
    {
        printk(KERN_WARNING "gpio-out: can't get major %d\n", MAJOR(gpiores->devt));
        return result;
    }
    else
    {
        printk(KERN_INFO "gpio-out: get major %d success\n", MAJOR(gpiores->devt));
    }

    cdev_init(&gpiores->dev, &gpio_fops);
    gpiores->dev.owner = THIS_MODULE;
    gpiores->dev.ops = &gpio_fops;
    cdev_add(&gpiores->dev, gpiores->devt, 1);
    gpiores->class = class_create(THIS_MODULE, "learn");
    device_create(gpiores->class, NULL, gpiores->devt, NULL, "gpio-out");

    platform_set_drvdata(pdev, gpiores);

    return 0;
}
/*定义平台设备结构体*/
struct platform_driver gpio_learn_driver = {
    .probe = gpio_learn_probe,
    .driver = {
        .name = "gpio-out",
        .owner = THIS_MODULE,
        .of_match_table = gpio_learn_id,
    },
};
module_platform_driver(gpio_learn_driver);

MODULE_AUTHOR("tips tips@tips");
MODULE_DESCRIPTION("GPIO Driver Learn");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gpio-learn");
