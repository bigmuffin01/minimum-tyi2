#include "minemu/boot.h"
#include "minemu/trap.h"
#include "minemu/trace.h"
#include "minemu/platform.h"
#include "minemu/irq.h"

#define BUFFER_SIZE 128

static volatile uint32_t interrupt_count;

static void printChar(char c) {// print individual character
    while ((MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY) == 0) {}
    MINEMU_UART0->tx_data = (uint32_t)c;
}
static void printString(const char* s) { // print a string
    while (*s != '\0') {
        printChar(*s);
        s++;
    }
}

static volatile char rxBuf[BUFFER_SIZE]; // volatile so they stay consistent
static volatile uint32_t rxHead = 0;
static volatile uint32_t rxTail = 0;

static void bufferWrite(void){
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        uint8_t b = (uint8_t) MINEMU_UART0->rx_data;
        uint32_t next = (rxHead + 1) % BUFFER_SIZE;
        if (next != rxTail) {
            rxBuf[rxHead] = b;
            rxHead = next;
        }
    }
}

struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    uint32_t source = (uint32_t)frame->exception_id;
    ++interrupt_count;
    if (source == MINEMU_IRQ_SYSTICK) {
        MINEMU_SYSTICK->ack = MINEMU_SYSTICK_ACK;
    } else if (source == MINEMU_IRQ_UART0) {
        bufferWrite();
    } else if (source == MINEMU_IRQ_UART1) {
        (void)MINEMU_UART1->rx_data;
    } else if (source == MINEMU_IRQ_BLOCK) {
        MINEMU_BLOCK->ack = MINEMU_BLOCK_ACK;
    }
    MINEMU_INTERRUPT->eoi = source;
    return frame;
}

static inline uint32_t irq_save(void) {
    uint32_t cpsr;
    __asm__ volatile("mrs %0, cpsr\n cpsid i" : "=r"(cpsr) : : "memory");
    return cpsr;
}
static inline void irq_restore(uint32_t cpsr) {
    if (!(cpsr & 0x80)) minemu_irq_enable();   /* I bit clear => was enabled */
}


int bufferRead(void) {
    uint32_t flags = irq_save();
    int c = -1;
    if (rxTail != rxHead) {
        c = rxBuf[rxTail];
        rxTail = (rxTail + 1) % BUFFER_SIZE;
    }
    irq_restore(flags);
    return c;
}

char readChar(void) {
    int c;
    while ((c = bufferRead()) == -1) {
        __asm__ volatile("wfi");
    }
    return (char) c;
}

static int isCMD(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->system_rom_base != UINT32_C(0x08000000) ||
        boot_info->direct_map_vaddr != UINT32_C(0xc0000000) ||
        boot_info->direct_map_paddr != UINT32_C(0x40000000) ||
        boot_info->direct_map_size != UINT32_C(0x04000000)) {
        minemu_trace_event(UINT32_C(0xb007bad0));
        minemu_fail_stop();
    }
    minemu_trace_event(1);

    MINEMU_UART0->control |= MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    MINEMU_INTERRUPT->enable |= UINT32_C(1) << MINEMU_IRQ_UART0;

    minemu_irq_enable();
    printString("hello world\n");

    char command[21];
    //printString(command);
    //printString("\n");
    int length = 0;
    while (1) {
        printString("msh> ");

        int tooLong = 0;

        while (1) {
            char c = readChar();
            if (c == '\n') {
                break;
            }

            if (c == 0x08 || c == 0x7f) { // backspace
                if (tooLong > 0) {
                    tooLong --;
                } else if (length > 0) {
                    length --;
                }
                continue;
            }

            if (length <20) {
                command[length++] = c;
            } else {
                tooLong++;
            }
        }

        command[length] = '\0';

        if (tooLong) { // command too long
            printString("command too long\n");
            length = 0;
            continue;
        }

        int cmdStart = 0;
        while (command[cmdStart] == ' ') {
            cmdStart++;
        }
        if (command[cmdStart] == '\0') {
            length = 0;
            continue;
        }
        int cmdEnd = cmdStart;
        while (command[cmdEnd] != ' ' && command[cmdEnd] != '\0') {
            cmdEnd++;
        }
        char* cmd = &command[cmdStart];
        char* args;
        if (command[cmdEnd] == '\0') {
            args = &command[cmdEnd];
        } else {
            command[cmdEnd] = '\0';
            args = &command[cmdEnd + 1];
            while (*args == ' ') {
                args++;
            }
        }

        // check commands
        if (isCMD(cmd, "echo")) {
            printString(args);
            printChar('\n');
        } else {
            printString("command not found: ");
            printString(cmd);
            printChar('\n');
        }
        command[cmdEnd] = '\0';
        length = 0;
    }
}
