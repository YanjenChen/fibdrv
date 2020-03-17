#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define uint64_t unsigned long long
#define clz(s) __builtin_clzll(s)
/* #include "bign.h" */

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

/* Kernel space time measurement */
static ktime_t kt;

/* Switch between different algorithm */
static uint64_t (*fib_sequence)(uint64_t);

static uint64_t fib_sequence_add(uint64_t k)
{
    /* FIXME: use clz/ctz and fast algorithms to speed up */
    uint64_t f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}

static uint64_t smul(uint64_t x, uint64_t y)
{
    uint64_t out = 0;
    uint64_t i = y;
    for (int co = 0; i; i >>= 1, co++) {
        if (i & 0x1 && co)
            out += x << co;
        else if (i & 0x1 && !co)
            out += x;
    }
    return out;
}

static uint64_t fib_sequence_fast(uint64_t k)
{
    /* FIXME: use clz/ctz and fast algorithms to speed up */
    uint64_t h = 0;
    for (uint64_t i = k; i; ++h, i >>= 1)
        ;

    uint64_t a = 0;  // F(0) = 0
    uint64_t b = 1;  // F(1) = 1

    for (uint64_t mask = 1 << (h - 1); mask; mask >>= 1) {
        uint64_t c = a * (2 * b - a);
        uint64_t d = a * a + b * b;

        if (mask & k) {
            a = d;
            b = c + d;
        } else {
            a = c;
            b = d;
        }
    }

    return a;
}

static uint64_t fib_sequence_fast_smul(uint64_t k)
{
    /* FIXME: use clz/ctz and fast algorithms to speed up */
    uint64_t h = 0;
    for (uint64_t i = k; i; ++h, i >>= 1)
        ;

    uint64_t a = 0;  // F(0) = 0
    uint64_t b = 1;  // F(1) = 1

    for (uint64_t mask = 1 << (h - 1); mask; mask >>= 1) {
        uint64_t c = smul(a, (smul(2, b) - a));
        uint64_t d = smul(a, a) + smul(b, b);

        if (mask & k) {
            a = d;
            b = c + d;
        } else {
            a = c;
            b = d;
        }
    }

    return a;
}

static uint64_t fib_sequence_fast_clz(uint64_t k)
{
    int h = clz(k);

    uint64_t a = 0;  // F(0) = 0
    uint64_t b = 1;  // F(1) = 1

    for (int mask = 1 << (h - 1); mask; mask >>= 1) {
        uint64_t c = a * (2 * b - a);
        uint64_t d = a * a + b * b;

        if (mask & k) {
            a = d;
            b = c + d;
        } else {
            a = c;
            b = d;
        }
    }

    return a;
}

static uint64_t fib_time_proxy(uint64_t k)
{
    kt = ktime_get();
    uint64_t result = fib_sequence(k);
    kt = ktime_sub(ktime_get(), kt);

    return result;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    uint64_t result = fib_time_proxy(*offset);
    copy_to_user(buf, &result, sizeof(uint64_t));
    return ktime_to_ns(kt);
}

/* write operation use as display last execution time */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    switch (size) {
    case 0:
        fib_sequence = fib_sequence_add;
        break;
    case 1:
        fib_sequence = fib_sequence_fast;
        break;
    case 2:
        fib_sequence = fib_sequence_fast_clz;
        break;
    case 3:
        fib_sequence = fib_sequence_fast_smul;
        break;
    default:
        break;
    }
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    cdev_init(fib_cdev, &fib_fops);
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }

    /* If init success, set default algorithm */
    fib_sequence = fib_sequence_add;
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
