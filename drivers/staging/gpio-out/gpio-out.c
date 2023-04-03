#include <linux/err.h>
// #include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
// #include <linux/leds.h>
#include <asm/uaccess.h> /* copy_*_user */
#include <linux/cdev.h>
#include <linux/errno.h> /* error codes */
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/fs.h>    /* everything... */
#include <linux/init.h>
#include <linux/kernel.h> /* printk() */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/slab.h>  /* kmalloc() */
#include <linux/types.h> /* size_t */

struct gpios_res {
  struct device_node *node;    // 设备树节点
  struct gpio_desc *gpio[20];  // gpio
  struct cdev dev;             // cdev
  struct class *class;
  struct device *device;
  int gpioValue[20];
  int gpio_num;
  dev_t devt;
};

static int DEBUG = 0;

static const struct of_device_id gpio_learn_id[] = {
    {.compatible = "gpio-out"},
    {/* sentinel */},
};

MODULE_DEVICE_TABLE(of, gpio_learn_id);

static long gpio_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
  struct gpios_res *res = filp->private_data;
  if (arg < 4) {
    switch (cmd) {
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
                 loff_t *f_pos) {
  // struct gpio_learn_res *res = (struct gpio_learn_res *)filp->private_data;
  // buf[0] = gpiod_get_value(gpio_to_desc(194));
  //*f_pos++;
  char buff[20] = {};
  struct gpios_res *res = filp->private_data;
  int nbytes;
  int i;
  if (*f_pos > res->gpio_num - 1) {
    return 0;
  }
  if (count + *f_pos > res->gpio_num - 1) {
    count = res->gpio_num - *f_pos;
  }

  for (i = *f_pos; i < *f_pos + count; i++) {
    buff[i] = gpiod_get_value(res->gpio[*f_pos + i]) + '0';
  }
  nbytes = copy_to_user(buf, buff, count);
  *f_pos += count;
  return count;
}

ssize_t setvalue(struct file *filp, const char __user *buf, size_t count,
                 loff_t *f_pos) {
  // printk("set vaule:%s\r\n", buf[0]);
  // struct gpio_desc *io = gpio_to_desc(194);
  struct gpios_res *res = filp->private_data;
  char buff[20];
  int i;
  int nbytes = count;
  if (*f_pos > (res->gpio_num - 1)) {
    return -ENOSPC;
  }
  if (*f_pos + count > res->gpio_num) {
    count = res->gpio_num - *f_pos;
  }

  nbytes = copy_from_user(buff, buf, count);
  if (nbytes) {
    if (DEBUG) printk(KERN_WARNING "copy_from_user failed:%d\n", nbytes);
    return -1;
  }

  if (DEBUG)
    printk(KERN_INFO "copying %d bytes; start: %lld\r\n", count, *f_pos);

  for (i = *f_pos; i < *f_pos + count; i++) {
    if (i > (res->gpio_num - 1)) {
      continue;
    }
    if (buff[i] >= '0' && buff[i] <= '9') {
      buff[i] = buff[i] - '0';
    }

    if (DEBUG) printk(KERN_INFO "set %d->%d\n", i, buff[i]);
    gpiod_direction_output(res->gpio[i], buff[i]);
    res->gpioValue[i] = buff[i];
  }
  *f_pos += count;
  return count;
}

int release(struct inode *inode, struct file *filp) {
  // printk(KERN_DEBUG "process %i (%s) success release minor(%u) file\n",
  // current->pid, current->comm, iminor(inode));
  return 0;
}

/*
 * Open and close
 */

int open(struct inode *inode, struct file *filp) {
  //  struct scull_dev *dev = (struct scull_dev *)container_of(inode->i_cdev,
  //  struct scull_dev, cdev);
  filp->private_data = container_of(inode->i_cdev, struct gpios_res, dev);

  //  printk(KERN_DEBUG "process %i (%s) success open minor(%u) file,
  //  private_data->0x%X\n", current->pid, current->comm, iminor(inode),
  //  filp->private_data);
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

static int gpio_out_probe(struct platform_device *pdev) {
  struct gpios_res *gpiores;
  struct device_node *node;
  int i;
  int result;
  uint32_t num = 0;

  printk(KERN_INFO "gpio-out match ok->%s\r\n", pdev->name);

  gpiores = kmalloc(sizeof(struct gpios_res), GFP_KERNEL);
  if (!gpiores) {
    printk(KERN_INFO "GPIO alloc error\r\n");
    return 0;
  }

  node = of_find_node_by_path("/gpio-out");
  if (!node) {
    printk(KERN_ERR "Failed to find node \"/gpio-out\"\r\n");
    return 0;
  }

  if (of_property_read_bool(node, "debug")) {
    DEBUG = 1;
  }

  if (of_property_read_u32(node, "gpionum", &num)) {
    printk(KERN_ERR "Failed to read gpionum\n");
  }

  if (num >= 20) {
    printk(KERN_INFO "Only 20 GPIOs can be used at most, find %d\n", num);
    num = 20;
  } else {
    printk(KERN_INFO "find %d GPIOs\n", num);
  }
  gpiores->gpio_num = num;
  gpiores->node = node;
  const char *gpio_name[20] = {
      "out1",  "out2",  "out3",  "out4",  "out5",  "out6",  "out7",
      "out8",  "out9",  "out10", "out11", "out12", "out13", "out14",
      "out15", "out16", "out17", "out18", "out19",
  };
  for (i = 0; i < num; i++) {
    gpiores->gpio[i] = devm_gpiod_get(&pdev->dev, gpio_name[i], GPIOD_OUT_LOW);
    gpiod_direction_output(gpiores->gpio[i], 0);
  }

  for (i = 0; i < num; i++) {
    if (IS_ERR(gpiores->gpio[i])) {
      int ret = PTR_ERR(gpiores->gpio[i]);
      printk("gpio out%d error %i\n", i, ret);
      gpiores->gpio[i] = NULL;
    } else {
      printk("gpio out%d ok\n", i + 1);
    }
  }

  result = alloc_chrdev_region(&gpiores->devt, 0, 1, "gpio-out");

  if (result < 0) {
    printk(KERN_WARNING "gpio-out: can't get major %d\n", MAJOR(gpiores->devt));
    return result;
  } else {
    printk(KERN_INFO "gpio-out: get major %d success\n", MAJOR(gpiores->devt));
  }

  cdev_init(&gpiores->dev, &gpio_fops);
  gpiores->dev.owner = THIS_MODULE;
  gpiores->dev.ops = &gpio_fops;
  cdev_add(&gpiores->dev, gpiores->devt, 1);
  gpiores->class = class_create(THIS_MODULE, "out");
  gpiores->device =
      device_create(gpiores->class, NULL, gpiores->devt, NULL, "gpio-out");

  platform_set_drvdata(pdev, gpiores);

  return 0;
}
static int gpio_out_remove(struct platform_device *pdev) {
  struct gpios_res *res = platform_get_drvdata(pdev);
  if (!IS_ERR(res->class)) {
    device_destroy(res->class, res->devt);
  }
  if (!IS_ERR(res->class) && !IS_ERR(res->device)) {
    class_destroy(res->class);
  }
  unregister_chrdev(res->devt, "gpio-out");

  return 0;
}
/*定义平台设备结构体*/
struct platform_driver gpio_learn_driver = {
    .probe = gpio_out_probe,
    .remove = gpio_out_remove,
    .driver =
        {
            .name = "gpio-out",
            .owner = THIS_MODULE,
            .of_match_table = gpio_learn_id,
        },
};
module_platform_driver(gpio_learn_driver);

MODULE_AUTHOR("tips tips@tips");
MODULE_DESCRIPTION("GPIO Out Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gpio-out");
