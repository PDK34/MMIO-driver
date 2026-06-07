# MMIO-crypto-driver

## Simulated Crypto-Peripheral Linux Character Driver

A Linux Loadable Kernel Module (LKM) that emulates a hardware-based encryption accelerator via a custom character device layout. 

## Hardware Register Layout

The driver allocates a 16-byte continuous memory block in kernel space to simulate Memory-Mapped I/O (MMIO) registers.

| Name          | Index Offset | Bit-Level Flags & Functionality |
| :---          | :---         | :--- |
| `REG_DATA_IN`  | `0` (0x00)   | **Write-Only:** Holds the raw byte to process. |
| `REG_CTRL`     | `1` (0x04)   | **Read-Write:** <br>Bit 0 (`START`): Set to 1 to begin processing.<br>Bit 1 (`RESET`): Set to 1 to clear registers. |
| `REG_STATUS`   | `2` (0x08)   | **Read-Only:** <br>Bit 0 (`BUSY`): High during 10ms processing delay.<br>Bit 1 (`READY`): High when output data is valid. |
| `REG_DATA_OUT` | `3` (0x0C)   | **Read-Only:** Holds processed data (Input XOR 0x5A). |

## How to Build and Run

1. Build the kernel module and test app:
   ```bash
   make
   gcc test_cdev.c -o test_cdev
2. Load the driver:
    ```bash
    sudo insmod encrypt_cdev.ko
3. Run the user-space verification test:
    ```bash
    sudo ./test_cdev
4. Check kernel execution log traces:
    ```bash
    dmesg | tail -n 10
5. Unload the module:
    ```bash
    sudo rmmod encrypt_cdev