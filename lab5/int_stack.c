#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/usb.h>

#define DEVICE_NAME "int_stack"
#define CLASS_NAME "int_stack_class"

#define USB_KEY_VENDOR_ID 0x0bda
#define USB_KEY_PRODUCT_ID 0x8152

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Artyom Shaposhnikov");
MODULE_VERSION("0.2");

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
    bool device_created;
} int_stack_device;

// USB device tracking 
static struct usb_device *usb_key_device = NULL;

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

static int create_device(void)
{
    if (int_stack_device.device_created) {
        return 0;
    }

    int_stack_device.device = device_create(int_stack_device.class, NULL,
                                            int_stack_device.dev_number, NULL,
                                            DEVICE_NAME);
    if (IS_ERR(int_stack_device.device)) {
        pr_err("INT_STACK: Failed to create device\n");
        return PTR_ERR(int_stack_device.device);
    }

    int_stack_device.device_created = true;
    pr_info("INT_STACK: Device created at /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void remove_device(void)
{
    if (int_stack_device.device_created) {
        device_destroy(int_stack_device.class, int_stack_device.dev_number);
        int_stack_device.device_created = false;
        pr_info("INT_STACK: Device removed from /dev/%s\n", DEVICE_NAME);
    }
}

static int usb_key_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    
    pr_info("INT_STACK: USB device with VID:PID %04x:%04x connected\n", 
           udev->descriptor.idVendor, udev->descriptor.idProduct);
    
    usb_key_device = udev;
    
    create_device();
    
    return 0;
}

static void usb_key_disconnect(struct usb_interface *interface)
{
    usb_key_device = NULL;
    
    remove_device();
    
    pr_info("INT_STACK: USB key disconnected, device removed\n");
}

static struct usb_device_id usb_key_table[] = {
    { USB_DEVICE(USB_KEY_VENDOR_ID, USB_KEY_PRODUCT_ID) },
    { }
};
MODULE_DEVICE_TABLE(usb, usb_key_table);

static struct usb_driver usb_key_driver = {
    .name = "int_stack_key",
    .id_table = usb_key_table,
    .probe = usb_key_probe,
    .disconnect = usb_key_disconnect,
};

static int init_stack_data(void)
{
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
    
    return 0;
}

static int init_char_device(void)
{
    int ret;
    
    // allocate a major/minor number for the device
    ret = alloc_chrdev_region(&int_stack_device.dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("INT_STACK: Failed to allocate device number\n");
        return ret;
    }

    // create device class
    int_stack_device.class = class_create(CLASS_NAME);
    if (IS_ERR(int_stack_device.class)) {
        pr_err("INT_STACK: Failed to create device class\n");
        ret = PTR_ERR(int_stack_device.class);
        unregister_chrdev_region(int_stack_device.dev_number, 1);
        return ret;
    }

    // initialize character device
    cdev_init(&int_stack_device.cdev, &int_stack_fops);
    int_stack_device.cdev.owner = THIS_MODULE;

    // add character device to the system
    ret = cdev_add(&int_stack_device.cdev, int_stack_device.dev_number, 1);
    if (ret < 0) {
        pr_err("INT_STACK: Failed to add character device\n");
        class_destroy(int_stack_device.class);
        unregister_chrdev_region(int_stack_device.dev_number, 1);
        return ret;
    }
    
    int_stack_device.device_created = false;
    
    return 0;
}

// initialisation
static int __init int_stack_init(void)
{
    int ret;

    ret = init_stack_data();
    if (ret < 0) {
        return ret;
    }

    ret = init_char_device();
    if (ret < 0) {
        kfree(stack->data);
        kfree(stack);
        return ret;
    }

    ret = usb_register(&usb_key_driver);
    if (ret < 0) {
        pr_err("INT_STACK: Failed to register USB driver\n");
        cdev_del(&int_stack_device.cdev);
        class_destroy(int_stack_device.class);
        unregister_chrdev_region(int_stack_device.dev_number, 1);
        kfree(stack->data);
        kfree(stack);
        return ret;
    }

    pr_info("INT_STACK: Module loaded successfully\n");
    pr_info("INT_STACK: Waiting for USB key with VID:PID %04x:%04x to be inserted\n", 
            USB_KEY_VENDOR_ID, USB_KEY_PRODUCT_ID);

    return 0;
}

// cleanup
static void __exit int_stack_exit(void)
{
    usb_deregister(&usb_key_driver);
    
    if (int_stack_device.device_created) {
        device_destroy(int_stack_device.class, int_stack_device.dev_number);
    }
    
    cdev_del(&int_stack_device.cdev);
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
