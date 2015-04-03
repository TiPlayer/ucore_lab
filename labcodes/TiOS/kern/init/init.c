#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <vmm.h>
#include <ide.h>
#include <swap.h>
#include <proc.h>
#include <fs.h>
#include <kmonitor.h>
#include <defs.h>
#include <x86.h>
#include <pmm.h>
#include "kmalloc.h"

static void startothers(void);
int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);
static void lab1_switch_test(void);
static void mpmain(void) __attribute__((noreturn));


int kern_init(void) {
  extern char edata[], end[];
  memset(edata, 0, end - edata);
  cons_init_cga();           // init the console
  cprintf("\n\n\nThis is TiOS, uCore with SMP support\n");
  cprintf("Shichao Yue\n\n\n");
  // Changed Mem Layout
  pmm_init();                 // init physical memory management
  mpinit();
  lapicinit();
  cprintf("============================\n");
  cprintf("There are %d cpu\n", ncpu);
  cprintf("============================\n");

  pic_init();                 // init interrupt controller
  ioapicinit();
  cons_init_keyboard();

  idt_init();                 // init interrupt descriptor table

  cprintf("============================\n");
  cprintf("SMP Coming Soon\n");
  cprintf("============================\n");
  vmm_init();                 // init virtual memory management
  sched_init();               // init scheduler
  proc_init();                // init process table
  ide_init();                 // init ide devices
  swap_init();                // init swap
  fs_init();                  // init fs

//  clock_init();    // init clock interrupt
  startothers();
  cprintf("============================\n");
  cprintf("SMP Coming!!!!!!, %p\n", current);
  cprintf("============================\n");
  intr_enable();              // enable irq interrupt
  //LAB1: CAHLLENGE 1 If you try to do it, uncomment lab1_switch_test()
  // user/kernel mode switch test
  //lab1_switch_test();
  mpmain();                 // run idle process
}

void __attribute__((noinline))
grade_backtrace2(int arg0, int arg1, int arg2, int arg3) {
  mon_backtrace(0, NULL, NULL);
}

void __attribute__((noinline))
grade_backtrace1(int arg0, int arg1) {
  grade_backtrace2(arg0, (int)&arg0, arg1, (int)&arg1);
}

void __attribute__((noinline))
grade_backtrace0(int arg0, int arg1, int arg2) {
  grade_backtrace1(arg0, arg2);
}

void
grade_backtrace(void) {
  grade_backtrace0(0, (int)kern_init, 0xffff0000);
}

static void
lab1_print_cur_status(void) {
  static int round = 0;
  uint16_t reg1, reg2, reg3, reg4;
  asm volatile (
  "mov %%cs, %0;"
      "mov %%ds, %1;"
      "mov %%es, %2;"
      "mov %%ss, %3;"
  : "=m"(reg1), "=m"(reg2), "=m"(reg3), "=m"(reg4));
  cprintf("%d: @ring %d\n", round, reg1 & 3);
  cprintf("%d:  cs = %x\n", round, reg1);
  cprintf("%d:  ds = %x\n", round, reg2);
  cprintf("%d:  es = %x\n", round, reg3);
  cprintf("%d:  ss = %x\n", round, reg4);
  round ++;
}

static void
lab1_switch_to_user(void) {
  //LAB1 CHALLENGE 1 : TODO
}

static void
lab1_switch_to_kernel(void) {
  //LAB1 CHALLENGE 1 :  TODO
}

static void
lab1_switch_test(void) {
  lab1_print_cur_status();
  cprintf("+++ switch to  user  mode +++\n");
  lab1_switch_to_user();
  lab1_print_cur_status();
  cprintf("+++ switch to kernel mode +++\n");
  lab1_switch_to_kernel();
  lab1_print_cur_status();
}

// Common CPU setup code.

static void  mpmain(void) {
  cprintf("cpu%d: starting\n", cpu->id);
  xchg(&cpu->started, 1); // tell startothers() we're up
  if (cpu->id == 0) {
    cpu_idle();
  } else {
    while (1) {}
  }
}

void enable_paging(uintptr_t pgdir) {
  lcr3(pgdir);
  // turn on paging
  uint32_t cr0 = rcr0();
  cr0 |= CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP;
  cr0 &= ~(CR0_TS | CR0_EM);
  lcr0(cr0);
}
pde_t entrypgdir[];  // For entry.S

// Other CPUs jump here from entryother.S.
static void mpenter(int cpuid) {
  cprintf("Trying to wake up %d# CPU\n", cpuid);
  cpu[cpuid].id = cpuid;
  cprintf("pgdir: %p\n", boot_pgdir);
  pmm_init_second(cpuid);
  cprintf("ap: pmm_init done\n");
  idt_init();
  cprintf("ap: idt init done\n");
  lapicinit();
  cprintf("ap: lapicinit done\n");
  mpmain();
}



// Start the non-boot (AP) processors.
void startothers(void) {
  cprintf("startothers\n");
  extern uint8_t _binary_obj_entryother_o_start[], _binary_obj_entryother_o_size[]; //I do not know where is this
  uint8_t *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_obj_start.
  code = KADDR(0x7000);
  memmove(code, _binary_obj_entryother_o_start, (uint32_t) _binary_obj_entryother_o_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == cpus+cpunum())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    stack = kmalloc(KSTACKSIZE);
    *(void**)(code-4) = stack + KSTACKSIZE;  //kva
    *(void**)(code-8) = mpenter; //kva
    *(int**)(code-12) = c->id;  //No more ad-hoc methods.

    lapicstartap(c->id, PADDR(code));
    cprintf("waking up cpu %d\n", c->id);

    // wait for cpu to finish mpmain()
    while(c->started == 0);
  }
}

// Boot page table used in entry.S and entryother.S.
// Page directories (and page tables), must start on a page boundary,
// hence the "__aligned__" attribute.
// Use PTE_PS in page directory entry to enable 4Mbyte pages.
__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
    // Map VA's [0, 4MB) to PA's [0, 4MB)
    [0] = (0) | PTE_P | PTE_W | PTE_PS,
    // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
    [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};
