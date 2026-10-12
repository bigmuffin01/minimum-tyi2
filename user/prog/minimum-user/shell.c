#include <stdint.h>

#include "shell.h"
#include "minemu/user_abi.h"

#define COMMAND_SIZE 21

// Adapted mostly from HW1 implementation of msh

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
    while (ioctl(MINEMU_FD_UART_IN, MINEMU_IOCTL_UART_READ, (void *)&c, 1U) != 1) {
        __asm__ volatile("wfi");
    }
    return c;
}

static int isCMD(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

void run_msh(void) {
    while (1) {
        char command[COMMAND_SIZE];
        uint32_t length = 0;
        int started = 0;

        printString("msh> ");
        while (1) {
            char c = readChar();
            if (c=='\n') { // DOUBLE CHECK (\r?)
                break;
            }
            if (c == '\b' || c == 0x7f) {
                if (length > 0) {
                    length--;
                }
                continue;
            }
            if (!started && c == ' ') {
                continue;
            }
            started = 1;
            if (length < COMMAND_SIZE - 1) {
                command[length] = c;
                length++;
            }
        }
        command[length] = '\0';
        char *start = command;
        while (*start == ' ') {
            start++;
        }

        if (*start == '\0') { // newline for empty argument not taken as bogus argument
            continue;
        }

        char *args = start; // parse args and cmd
        while (*args != '\0' && *args != ' ') {
            args++;
        }
        if (*args != '\0') {
            *args = '\0';
            args++;
            while (*args == ' ') {
                args++;
            }
        }

        if (isCMD(start, "echo")) {
            int first = 1;
            while (*args != '\0') {
                while (*args == ' ') {
                    args++;
                }
                if (*args == '\0') {
                    break;
                }
                if (!first) {
                    printString(" ");
                }

                char word[COMMAND_SIZE];
                uint32_t wordLength = 0;
                while (*args != '\0' && *args != ' ') {
                    word[wordLength] = *args;
                    wordLength++;
                    args++;
                }
                word[wordLength] = '\0';
                printString(word);
                first = 0;
            }
            printString("\n");
        } else {
            printString("command not found: ");
            printString(start);
            printString("\n");
        }
    }
}