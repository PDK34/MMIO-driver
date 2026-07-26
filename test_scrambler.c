#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#define DEVICE_PATH "/dev/scrambler_driver"

int main() {
    int fd;
    uint8_t tx_buffer[2]; 
    uint8_t rx_buffer = 0;

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device node");
        return -1;
    }

    // Test Case 1: Send Data = 0xA5, Control Bit = START (0x01)
    tx_buffer[0] = 0xA5; 
    tx_buffer[1] = 0x01; 

    printf("[User Application] Sending Data: 0x%02X with START command.\n", tx_buffer[0]);
    write(fd, tx_buffer, 2);

    // The driver simulates a 10ms delay. Sleep for 15ms (15,000 microseconds)
    // to ensure the STAT_READY bit is set before trying to read.
    usleep(15000);

    // Bring the file cursor back to 0 before reading
    lseek(fd, 0, SEEK_SET);
    if (read(fd, &rx_buffer, 1) > 0) {
        printf("[User Application] Success! Read Processed Data: 0x%02X\n", rx_buffer);
        printf("[Verification] Expected: 0xA5 XOR 0x5A = 0xFF. Got: 0x%02X\n", rx_buffer);
    } else {
        printf("[User Application] Data was not ready or read failed.\n");
    }

    close(fd);
    return 0;
}