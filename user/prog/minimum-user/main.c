#include <stdint.h>

#include "shell.h"

void minemu_user_main(void) {
    run_msh();
    /*
    for (;;) {
        __asm__ volatile("nop");
    }
    */
}
