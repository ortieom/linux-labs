#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>

#define DEVICE_NAME "int_stack"
#define CLASS_NAME "int_stack_class"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Artyom Shaposhnikov");
MODULE_VERSION("0.1");

#define DEFAULT_MAX_STACK_SIZE 10

// IOCTL commands
#define INT_STACK_MAGIC 'S'
#define INT_STACK_SET_SIZE _IOW(INT_STACK_MAGIC, 1, unsigned int)

struct int_stack {
    int *data;
    unsigned int size;
    unsigned int max_size;
    struct mutex lock;
};

static struct int_stack *stack = NULL;

static struct {
    struct cdev cdev;
    dev_t dev_number;
    struct class *class;
    struct device *device;
} int_stack_device;

// file operation prototypes
static int int_stack_open(struct inode *inode, struct file *filp);
static int int_stack_release(struct inode *inode, struct file *filp);
static ssize_t int_stack_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t int_stack_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static long int_stack_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

// file operations structure
static struct file_operations int_stack_fops = {
    .open = int_stack_open,
    .release = int_stack_release,
    .read = int_stack_read,
    .write = int_stack_write,
    .unlocked_ioctl = int_stack_ioctl,
    .owner = THIS_MODULE
};

static int int_stack_open(struct inode *inode, struct file *filp)
{
    pr_info("INT_STACK: Device opened\n");
    return 0;
}

static int int_stack_release(struct inode *inode, struct file *filp)
{
    pr_info("INT_STACK: Device closed\n");
    return 0;
}

static ssize_t int_stack_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    int value;
    ssize_t ret = 0;

    if (count < sizeof(int)) {
        return -EINVAL;
    }

    // locking
    mutex_lock(&stack->lock);

    if (stack->size == 0) {
        ret = 0;
        goto out;
    }

    // popping value
    stack->size--;
    value = stack->data[stack->size];

    if (copy_to_user(buf, &value, sizeof(int))) {
        ret = -EFAULT;
        // restoring stack size on error
        stack->size++;
        goto out;
    }

    ret = sizeof(int);

out:
    mutex_unlock(&stack->lock);
    return ret;
}

static ssize_t int_stack_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    int value;
    ssize_t ret = 0;

    // checking if count is enough to hold an integer
    if (count < sizeof(int)) {
        return -EINVAL;
    }

    if (copy_from_user(&value, buf, sizeof(int))) {
        return -EFAULT;
    }

    mutex_lock(&stack->lock);

    if (stack->size >= stack->max_size) {
        ret = -ERANGE;
        goto out;
    }

    stack->data[stack->size] = value;
    stack->size++;
    ret = sizeof(int);

out:
    mutex_unlock(&stack->lock);
    return ret;
}

static long int_stack_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    unsigned int new_size;
    int *new_data;

    if (_IOC_TYPE(cmd) != INT_STACK_MAGIC) {
        return -ENOTTY;
    }

    switch (cmd) {
    case INT_STACK_SET_SIZE:
        if (copy_from_user(&new_size, (unsigned int *)arg, sizeof(unsigned int))) {
            return -EFAULT;
        }

        if (new_size == 0) {
            return -EINVAL;
        }

        mutex_lock(&stack->lock);

        if (new_size < stack->size) {
            // updating stack size to point at the new last element
            // following ones will be dropped after reallocation
            stack->size = new_size;
        }

        new_data = krealloc(stack->data, sizeof(int) * new_size, GFP_KERNEL);
        if (!new_data) {
            ret = -ENOMEM;
            goto unlock;
        }

        stack->data = new_data;
        stack->max_size = new_size;

unlock:
        mutex_unlock(&stack->lock);
        break;

    default:
        ret = -ENOTTY;  // unknown command
    }

    return ret;
}

// initialisation
static int __init int_stack_init(void)
{
    int ret;

    stack = kmalloc(sizeof(struct int_stack), GFP_KERNEL);
    if (!stack) {
        pr_err("INT_STACK: Failed to allocate memory for stack\n");
        return -ENOMEM;
    }

    stack->size = 0;
    stack->max_size = DEFAULT_MAX_STACK_SIZE;
    mutex_init(&stack->lock);

    stack->data = kmalloc(sizeof(int) * stack->max_size, GFP_KERNEL);
    if (!stack->data) {
        kfree(stack);
        pr_err("INT_STACK: Failed to allocate memory for stack data\n");
        return -ENOMEM;
    }

    // allocate a major/minor number for the device
    ret = alloc_chrdev_region(&int_stack_device.dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("INT_STACK: Failed to allocate device number\n");
        kfree(stack->data);
        kfree(stack);
        return ret;
    }

    // create device class
    int_stack_device.class = class_create(CLASS_NAME);
    if (IS_ERR(int_stack_device.class)) {
        pr_err("INT_STACK: Failed to create device class\n");
        ret = PTR_ERR(int_stack_device.class);
        unregister_chrdev_region(int_stack_device.dev_number, 1);
        kfree(stack->data);
        kfree(stack);
        return ret;
    }

    // create device
    int_stack_device.device = device_create(int_stack_device.class, NULL,
                                           int_stack_device.dev_number, NULL,
                                           DEVICE_NAME);
    if (IS_ERR(int_stack_device.device)) {
        pr_err("INT_STACK: Failed to create device\n");
        ret = PTR_ERR(int_stack_device.device);
        class_destroy(int_stack_device.class);
        unregister_chrdev_region(int_stack_device.dev_number, 1);
        kfree(stack->data);
        kfree(stack);
        return ret;
    }

    // initialize character device
    cdev_init(&int_stack_device.cdev, &int_stack_fops);
    int_stack_device.cdev.owner = THIS_MODULE;

    // add character device to the system
    ret = cdev_add(&int_stack_device.cdev, int_stack_device.dev_number, 1);
    if (ret < 0) {
        pr_err("INT_STACK: Failed to add character device\n");
        device_destroy(int_stack_device.class, int_stack_device.dev_number);
        class_destroy(int_stack_device.class);
        unregister_chrdev_region(int_stack_device.dev_number, 1);
        kfree(stack->data);
        kfree(stack);
        return ret;
    }

    pr_info("INT_STACK: Module loaded successfully\n");
    pr_info("INT_STACK: Create a device file with 'mknod /dev/%s c %d %d'\n",
            DEVICE_NAME, MAJOR(int_stack_device.dev_number),
            MINOR(int_stack_device.dev_number));

    return 0;
}

// cleanup
static void __exit int_stack_exit(void)
{
    cdev_del(&int_stack_device.cdev);
    device_destroy(int_stack_device.class, int_stack_device.dev_number);
    class_destroy(int_stack_device.class);
    unregister_chrdev_region(int_stack_device.dev_number, 1);
    
    if (stack) {
        if (stack->data) {
            kfree(stack->data);
        }
        kfree(stack);
    }
    
    pr_info("INT_STACK: Module unloaded successfully\n");
}

module_init(int_stack_init);
module_exit(int_stack_exit);
