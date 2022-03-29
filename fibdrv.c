#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h> /* vmalloc */

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 500

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

char test_buffer[] = "testing writing";

/*fast doubling method*/
static long long fib_sequence(long long k)
{
    if (k < 2)
        return k;

    long long f[2];
    f[0] = 0;
    f[1] = 1;

    for (unsigned int i = 1U << 31; i; i >>= 1) {
        long long k1 = f[0] * (f[1] * 2 - f[0]);
        long long k2 = f[0] * f[0] + f[1] * f[1];
        if (k & i) {
            f[0] = k2;
            f[1] = k1 + k2;
        } else {
            f[0] = k1;
            f[1] = k2;
        }
    }
    return f[0];
}
/*with ctz macro*/
static long long fib_fast_ctz(long long k)
{
    if (k < 2)
        return k;

    long long f[2];
    f[0] = 0;
    f[1] = 1;

    for (unsigned int i = 1U << (31 - __builtin_clz(k)); i; i >>= 1) {
        long long k1 = f[0] * (f[1] * 2 - f[0]);
        long long k2 = f[0] * f[0] + f[1] * f[1];
        if (k & i) {
            f[0] = k2;
            f[1] = k1 + k2;
        } else {
            f[0] = k1;
            f[1] = k2;
        }
    }
    return f[0];
}
/* origin method */
static long long fib_iter(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    unsigned long long result, f0 = 0, f1 = 1;

    for (int i = 2; i < k; i++) {
        result = f0 + f1;
        f0 = f1;
        f1 = result;
    }
    return result;
}

/* strlen() can't be used */
static int my_strlen(char *x)
{
    int count = 0;
    while (*x != '\0') {
        count++;
        x++;
    }
    return count;
}

static char fib_big_num(long long k, const char *buf)
{
    char *fib1 = vmalloc(sizeof(char) * 2);
    char *fib0 = vmalloc(sizeof(char) * 2);
    fib0[0] = '0';
    fib0[1] = '\0';
    fib1[0] = '1';
    fib1[1] = '\0';

    if (k == 0) {
        copy_to_user(buf, fib0, 2);
        return 1;
    }
    if (k == 1) {
        copy_to_user(buf, fib1, 2);
        return 1;
    }

    int carry = 0;
    int sum = 0;
    int i = 1;
    while (i < k) {
        size_t len1 = my_strlen(fib0);
        size_t len2 = my_strlen(fib1);
        int len3 = 128;

        char *result = vmalloc(sizeof(char) * 128);

        while (len1 || len2) {
            if (len1) {
                sum += (fib0[--len1] - '0');
            }

            if (len2) {
                sum += (fib1[--len2] - '0');
            }
            sum += carry;
            result[--len3] = (sum % 10) + '0';
            carry = sum / 10;
            sum = 0;
        }
        if (carry)
            result[--len3] = 1 + '0';
        carry = 0;

        fib0 = fib1;
        fib1 = result + len3;
        i++;
        copy_to_user(buf, result + len3, 128 - len3 + 1);
    }
    vfree(fib0);
    vfree(fib1);
    return 1;
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
    return (ssize_t) fib_big_num(*offset, buf);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    ktime_t kt;
    switch (size) {
    /*origin method*/
    case 1:
        kt = ktime_get();
        fib_iter(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;
    /*fast doubling*/
    case 2:
        kt = ktime_get();
        fib_sequence(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;
    /*fast doubling with ctz macro*/
    case 3:
        kt = ktime_get();
        fib_fast_ctz(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;
    default:
        return 1;
    }

    return (ssize_t) ktime_to_ns(kt);
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
    fib_cdev->ops = &fib_fops;
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
