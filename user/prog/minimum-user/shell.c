#include <stdint.h>

#include "shell.h"
#include "minemu/user_abi.h"

#define COMMAND_SIZE 21

static void printString(const char *s) {
    uint32_t length = 0;

    while (s[length] != '\0') {
        length++;
    }

    (void)ioctl(
        MINEMU_FD_UART_OUT,
        MINEMU_IOCTL_UART_WRITE,
        (void *)s,
        length
    );
}

static char readChar(void) {
    char c;

    while (ioctl(
        MINEMU_FD_UART_IN,
        MINEMU_IOCTL_UART_READ,
        (void *)&c,
        1U
    ) != 1) {
        __asm__ volatile("wfi");
    }

    return c;
}
/*
static int isCMD(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
} */

void run_msh(void) {
    printString("msh> ");
    while (1) {
        char c = readChar();

        if (c == '\n') { // DOUBLE CHECK
            printString("msh> ");
        }
    }
}