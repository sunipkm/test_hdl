#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/mman.h>
#include <stdint.h>

#define INTERRUPT_IP_RESET 0x00 >> 2
#define INTERRUPT_IP_ENABLE 0x4 >> 2
#define INTERRUPT_IP_TSTAMP 0x8 >> 2
#define INTERRUPT_IP_SET 0x100 >> 2
#define INTERRUPT_IP_DEBUG 0x104 >> 2

void reset_interrupt_ip(unsigned int *reg)
{
    reg[INTERRUPT_IP_RESET] = 0x1;
    printf("After reset: 0x%02x, 0x%02x, 0x%04x, %d\n", reg[INTERRUPT_IP_RESET], reg[INTERRUPT_IP_ENABLE], reg[INTERRUPT_IP_DEBUG], reg[INTERRUPT_IP_TSTAMP]);
    return;
}

void enable_interrupt_ip(unsigned int *reg)
{
    reg[1] = 0x1;
    printf("After enable: 0x%02x, 0x%02x, 0x%04x, %d\n", reg[INTERRUPT_IP_RESET], reg[INTERRUPT_IP_ENABLE], reg[INTERRUPT_IP_DEBUG], reg[INTERRUPT_IP_TSTAMP]);
}

void set_interrupt_val(unsigned int *reg, unsigned char val)
{
    reg[INTERRUPT_IP_SET] = val;
    printf("After set: 0x%02x, 0x%02x, 0x%04x, %d\n", reg[INTERRUPT_IP_RESET], reg[INTERRUPT_IP_ENABLE], reg[INTERRUPT_IP_DEBUG], reg[INTERRUPT_IP_TSTAMP]);
}

void get_interrupt_ip_ts(unsigned int *reg)
{
    printf("Interrupt IP Timestamp: %d\n", reg[INTERRUPT_IP_TSTAMP]);
}

int main()
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    unsigned int *reg_address = mmap(NULL, 65535, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x43c10000); // memory map AXI Lite register block
    get_interrupt_ip_ts(reg_address);
    reset_interrupt_ip(reg_address);
    enable_interrupt_ip(reg_address);
    for (int i = 0; i < 20; i++)
    {
        enable_interrupt_ip(reg_address);
        if (i % 2)                              // if even
            set_interrupt_val(reg_address, 10); // set to 0
        else                                    // if odd
            set_interrupt_val(reg_address, 20); // set to 1
        reset_interrupt_ip(reg_address);
        usleep(500 * 1000);                     // 500 ms sleep
    }
    close(fd);
    return 0;
}