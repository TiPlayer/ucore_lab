#include <x86.h>
#include <intr.h>
#include <stdio.h>

/* intr_enable - enable irq interrupt */
void
intr_enable(void) {
  sti();
}

/* intr_disable - disable irq interrupt */
void
intr_disable(void) {
    cli();
}

