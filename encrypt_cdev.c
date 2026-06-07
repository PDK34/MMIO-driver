#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#define DEV_NAME "encrypt_cdev"

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

static int major_num;
static struct class *dev_class;

// 4 x 32-bit registers simulating memory-mapped hardware
static uint32_t hw_registers[NUM_REGISTERS];

// Helper function to simulate hardware processing logic
static void simulate_hardware_logic(void) {
    // Check if START bit is set
    if (hw_registers[REG_CTRL] & CTRL_START) {
        // Set STATUS to BUSY, clear READY
        hw_registers[REG_STATUS] |= STAT_BUSY;
        hw_registers[REG_STATUS] &= ~STAT_READY;

        pr_info("%s: HW Engine - Processing Data: 0x%02X\n", DEV_NAME, hw_registers[REG_DATA_IN]);
        
        // Simulate hardware propagation delay (10 milliseconds)
        msleep(10);

        // Simple hardware XOR scrambling logic (Key: 0x5A)
        hw_registers[REG_DATA_OUT] = hw_registers[REG_DATA_IN] ^ 0x5A;

        // Clear BUSY, Set READY
        hw_registers[REG_STATUS] &= ~STAT_BUSY;
        hw_registers[REG_STATUS] |= STAT_READY;

        // Clear START bit automatically (Hardware auto-clear behavior)
        hw_registers[REG_CTRL] &= ~CTRL_START;
    }

    // Check if RESET bit is set
    if (hw_registers[REG_CTRL] & CTRL_RESET) {
        hw_registers[REG_DATA_IN] = 0;
        hw_registers[REG_CTRL] = 0;
        hw_registers[REG_STATUS] = 0;
        hw_registers[REG_DATA_OUT] = 0;
        pr_info("%s: HW Engine - Registers Reset.\n", DEV_NAME);
    }
}

static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

// User reads from the device to get the processed data
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    uint8_t out_val;

    if (*offset > 0) return 0; // Only allow single-byte read per transaction

    // Poll status register to check if data is ready
    if (!(hw_registers[REG_STATUS] & STAT_READY)) {
        pr_warn("%s: Read error - Hardware data not ready.\n", DEV_NAME);
        return -EAGAIN; 
    }

    out_val = (uint8_t)hw_registers[REG_DATA_OUT];

    if (copy_to_user(buffer, &out_val, 1) != 0) {
        return -EFAULT;
    }

    // Clear ready status once read is complete
    hw_registers[REG_STATUS] &= ~STAT_READY;

    *offset += 1;
    return 1;
}

// User writes to the device to send data and commands
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    uint8_t input_command[2]; // [0] = Data Byte, [1] = Control Byte

    if (len < 2) {
        pr_err("%s: Write requires 2 bytes [Data, Control]\n", DEV_NAME);
        return -EINVAL;
    }

    if (copy_from_user(input_command, buffer, 2) != 0) {
        return -EFAULT;
    }

    // Write values directly into the simulated hardware registers
    hw_registers[REG_DATA_IN] = input_command[0];
    hw_registers[REG_CTRL] = input_command[1];

    // Trigger the hardware processing cycle
    simulate_hardware_logic();

    return len;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    return 0;
}

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// Renamed to avoid namespace collision with kernel core
static int __init encrypt_driver_init(void) {
    major_num = register_chrdev(0, DEV_NAME, &fops);
    if (major_num < 0) return major_num;

    dev_class = class_create(THIS_MODULE, "hw_mock_class");
    device_create(dev_class, NULL, MKDEV(major_num, 0), NULL, DEV_NAME);

    // Initialize registers to default reset state
    hw_registers[REG_DATA_IN] = 0;
    hw_registers[REG_CTRL] = 0;
    hw_registers[REG_STATUS] = 0;
    hw_registers[REG_DATA_OUT] = 0;

    pr_info("%s: Mock Hardware Driver Loaded\n", DEV_NAME);
    return 0;
}

// Renamed for naming consistency
static void __exit encrypt_driver_exit(void) {
    device_destroy(dev_class, MKDEV(major_num, 0));
    class_destroy(dev_class);
    unregister_chrdev(major_num, DEV_NAME);
    pr_info("%s: Mock Hardware Driver Unloaded\n", DEV_NAME);
}

module_init(encrypt_driver_init);
module_exit(encrypt_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Candidate");
MODULE_DESCRIPTION("Simulated MMIO Hardware Peripheral Driver");