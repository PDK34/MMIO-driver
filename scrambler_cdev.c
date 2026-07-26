#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/types.h>

#define DEV_NAME "scrambler_driver"
#define CLASS_NAME "scrambler_class"
// Register Offset Definitions
#define REG_DATA_IN   0
#define REG_CTRL      1
#define REG_STATUS    2
#define REG_DATA_OUT  3
#define NUM_REGISTERS 4

// Bitmask Definitions
#define CTRL_START    (1 << 0)
#define CTRL_RESET    (1 << 1)
#define STAT_BUSY     (1 << 0)
#define STAT_READY    (1 << 1)

static dev_t dev_num;
static struct cdev hw_cdev;
static struct class *dev_class;

// 4 x 32-bit registers simulating memory-mapped hardware
static u32 hw_registers[NUM_REGISTERS];

static DEFINE_MUTEX(hw_mutex);

// Helper function to simulate hardware processing logic
static void simulate_hardware_logic(void) {
    // Check if START bit is set
    if (hw_registers[REG_CTRL] & CTRL_START) {
        // Set STATUS to BUSY, clear READY
        hw_registers[REG_STATUS] |= STAT_BUSY;
        hw_registers[REG_STATUS] &= ~STAT_READY;

        pr_info("%s: HW Engine - Processing Data: 0x%02X\n", DEV_NAME, hw_registers[REG_DATA_IN]);
        
        mutex_unlock(&hw_mutex);
        // Simulate hardware propagation delay (10 milliseconds)
        msleep(10);

        mutex_lock(&hw_mutex);
        // Simple hardware XOR scrambling logic (Key: 0x5A)
        hw_registers[REG_DATA_OUT] = hw_registers[REG_DATA_IN] ^ 0x5A;

        // Clear BUSY, Set READY
        hw_registers[REG_STATUS] &= ~STAT_BUSY;
        hw_registers[REG_STATUS] |= STAT_READY;

        // Clear START bit automatically (Hardware auto-clear behavior)
        hw_registers[REG_CTRL] &= ~CTRL_START;
    }
}

static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

// User reads from the device to get the processed data
static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    u8 out_val;

    if(len < 1) {
        return 0;
    }

    if (*offset > 0) return 0; // Only allow single-byte read per transaction

    if(mutex_lock_interruptible(&hw_mutex)) return -ERESTARTSYS;

    // Poll status register to check if data is ready
    if (!(hw_registers[REG_STATUS] & STAT_READY)) {
        mutex_unlock(&hw_mutex);
        if(filep->f_flags &  O_NONBLOCK ) {
            return -EAGAIN; //EAGAIN should not be returned in blocking mode 
        }

        //In blocking mode, so sleep on a wait_queue here.
        //a simple mock, so just warn and return
        pr_warn("%s: Blocking read wait-queue not implemented. Returning -EAGAIN.\n", DEV_NAME);
        return -EAGAIN;
    }

    out_val = (u8)hw_registers[REG_DATA_OUT];
    hw_registers[REG_STATUS] &= ~STAT_READY;

    mutex_unlock(&hw_mutex);     // Unlock mutex before doing copy_to_user (which can sleep/page-fault)

    if (copy_to_user(buffer, &out_val, 1) ) {
        return -EFAULT;
        }

        *offset += 1;
        return 1; //return no of bytes read as per POSIX standard
    }
// User writes to the device to send data and commands
static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    u8 input_command[2]; // [0] = Data Byte, [1] = Control Byte


    if (len < 2) {
        pr_err("%s: Write requires 2 bytes [Data, Control]\n", DEV_NAME);
        return -EINVAL;
    }

    if (copy_from_user(input_command, buffer, 2) != 0) {
        return -EFAULT;
    }
    //Lock hw for exclusinve write access
    if(mutex_lock_interruptible(&hw_mutex)) return -ERESTARTSYS;
    // Write values directly into the simulated hardware registers
    hw_registers[REG_DATA_IN] = input_command[0];
    hw_registers[REG_CTRL] = input_command[1] & (CTRL_START | CTRL_RESET);

    // Trigger the hardware processing cycle
    simulate_hardware_logic();

    mutex_unlock(&hw_mutex);
    *offset += 2; //2 bytes writeen, so advance file ptr by 2
    return 2; //POSIX standard, the write() system call must return the exact number of bytes it successfully processed

}

static int dev_release(struct inode *inodep, struct file *filep) {
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
    .llseek = default_llseek,
};

static int __init encrypt_driver_init(void) {
    int ret;
    struct device *dev;    

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME); //Dynamic allocation of major + minor num
    if(ret<0){
        pr_err("%s: FAiled to allocate char dev region\n" , DEV_NAME);
        return ret;
    }

    //Add cdev 
    cdev_init(&hw_cdev, &fops);
    hw_cdev.owner = THIS_MODULE;
    ret = cdev_add(&hw_cdev, dev_num, 1);
    if(ret<0){
        pr_err("%s: Failed to add cdev\n", DEV_NAME);
        goto unreg_chrdev;
    }

    //sysfs class
    dev_class = class_create(THIS_MODULE, CLASS_NAME);
    if(IS_ERR(dev_class)){
        pr_err("%s: Failed to create class\n", DEV_NAME);
        ret = PTR_ERR(dev_class);
        goto del_cdev;
    }

    //create device node
    dev = device_create(dev_class, NULL, dev_num, NULL, DEV_NAME);
    if(IS_ERR(dev)){
        pr_err("%s: Failed to create device\n", DEV_NAME);
        ret = PTR_ERR(dev);
        goto destroy_class;
    }

    pr_info("%s: Driver loaded successfully\n", DEV_NAME);
    return 0;

//Error rollback
destroy_class:
    class_destroy(dev_class);
del_cdev:
    cdev_del(&hw_cdev);
unreg_chrdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}


static void __exit encrypt_driver_exit(void) {
    device_destroy(dev_class, dev_num);
    class_destroy(dev_class);
    cdev_del(&hw_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("%s: Driver Unloaded\n", DEV_NAME);
}

module_init(encrypt_driver_init);
module_exit(encrypt_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PDK34");
MODULE_DESCRIPTION("Simulated MMIO Hardware Peripheral Driver");
MODULE_VERSION("1.1");