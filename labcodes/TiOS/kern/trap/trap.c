#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <x86.h>
#include <stdio.h>
#include <assert.h>
#include <console.h>
#include <vmm.h>
#include <swap.h>
#include <kdebug.h>
#include <unistd.h>
#include <syscall.h>
#include <error.h>
#include <sched.h>
#include <sync.h>
#include <proc.h>

#define TICK_NUM 100

static void print_ticks() {
  cprintf("%d ticks\n",TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* *
 * Interrupt descriptor table:
 *
 * Must be built at run time because shifted function addresses can't
 * be represented in relocation records.
 * */
static struct gatedesc idt[256] = {{0}};

static struct pseudodesc idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt
};

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S */
void
idt_init(void) {
  /* LAB1 2012011295 : STEP 2 */
  /* LAB5 2012011295 */
  int i;
  extern uintptr_t __vectors[];
  for (i = 0; i < 256; i++) {
    SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
  }
  SETGATE(idt[T_SYSCALL], 1, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);
  lidt(&idt_pd);
}

static const char *
trapname(int trapno) {
  static const char * const excnames[] = {
      "Divide error",
      "Debug",
      "Non-Maskable Interrupt",
      "Breakpoint",
      "Overflow",
      "BOUND Range Exceeded",
      "Invalid Opcode",
      "Device Not Available",
      "Double Fault",
      "Coprocessor Segment Overrun",
      "Invalid TSS",
      "Segment Not Present",
      "Stack Fault",
      "General Protection",
      "Page Fault",
      "(unknown trap)",
      "x87 FPU Floating-Point Error",
      "Alignment Check",
      "Machine-Check",
      "SIMD Floating-Point Exception"
  };

  if (trapno < sizeof(excnames)/sizeof(const char * const)) {
    return excnames[trapno];
  }
  if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
    return "Hardware Interrupt";
  }
  return "(unknown trap)";
}

/* trap_in_kernel - test if trap happened in kernel */
bool
trap_in_kernel(struct trapframe *tf) {
  return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static const char *IA32flags[] = {
    "CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",
    "TF", "IF", "DF", "OF", NULL, NULL, "NT", NULL,
    "RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void
print_trapframe(struct trapframe *tf) {
  cprintf("trapframe at %p\n", tf);
  print_regs(&tf->tf_regs);
  cprintf("  ds   0x----%04x\n", tf->tf_ds);
  cprintf("  es   0x----%04x\n", tf->tf_es);
  cprintf("  fs   0x----%04x\n", tf->tf_fs);
  cprintf("  gs   0x----%04x\n", tf->tf_gs);
  cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
  cprintf("  err  0x%08x\n", tf->tf_err);
  cprintf("  eip  0x%08x\n", tf->tf_eip);
  cprintf("  cs   0x----%04x\n", tf->tf_cs);
  cprintf("  flag 0x%08x ", tf->tf_eflags);

  int i, j;
  for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i ++, j <<= 1) {
    if ((tf->tf_eflags & j) && IA32flags[i] != NULL) {
      cprintf("%s,", IA32flags[i]);
    }
  }
  cprintf("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

  if (!trap_in_kernel(tf)) {
    cprintf("  esp  0x%08x\n", tf->tf_esp);
    cprintf("  ss   0x----%04x\n", tf->tf_ss);
  }
}

void
print_regs(struct pushregs *regs) {
  cprintf("  edi  0x%08x\n", regs->reg_edi);
  cprintf("  esi  0x%08x\n", regs->reg_esi);
  cprintf("  ebp  0x%08x\n", regs->reg_ebp);
  cprintf("  oesp 0x%08x\n", regs->reg_oesp);
  cprintf("  ebx  0x%08x\n", regs->reg_ebx);
  cprintf("  edx  0x%08x\n", regs->reg_edx);
  cprintf("  ecx  0x%08x\n", regs->reg_ecx);
  cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static inline void
print_pgfault(struct trapframe *tf) {
  /* error_code:
   * bit 0 == 0 means no page found, 1 means protection fault
   * bit 1 == 0 means read, 1 means write
   * bit 2 == 0 means kernel, 1 means user
   * */
  cprintf("page fault at 0x%08x: %c/%c [%s].\n", rcr2(),
      (tf->tf_err & 4) ? 'U' : 'K',
      (tf->tf_err & 2) ? 'W' : 'R',
      (tf->tf_err & 1) ? "protection fault" : "no page found");
}

static int
pgfault_handler(struct trapframe *tf) {
  extern struct mm_struct *check_mm_struct;
  if(check_mm_struct !=NULL) { //used for test check_swap
    print_pgfault(tf);
  }
  struct mm_struct *mm;
  if (check_mm_struct != NULL) {
    mm = check_mm_struct;
  }
  else {
    if (current[getCurrentCPU()->id] == NULL) {
      print_trapframe(tf);
      print_pgfault(tf);
      panic("unhandled page fault.\n");
    }
    mm = current[getCurrentCPU()->id]->mm;
  }
  return do_pgfault(mm, tf->tf_err, rcr2());
}

static volatile int in_swap_tick_event = 0;
extern struct mm_struct *check_mm_struct;

static void
trap_dispatch(struct trapframe *tf) {
  char c;

  int ret=0;

  switch (tf->tf_trapno) {
    case T_PGFLT:  //page fault
      cprintf("page fault\n");
      if ((ret = pgfault_handler(tf)) != 0) {
        print_trapframe(tf);
        if (current[getCurrentCPU()->id] == NULL) {
          panic("handle pgfault failed. ret=%d\n", ret);
        }
        else {
          if (trap_in_kernel(tf)) {
            panic("handle pgfault failed in kernel mode. ret=%d\n", ret);
          }
          cprintf("killed by kernel.\n");
          panic("handle user mode pgfault failed. ret=%d\n", ret);
          do_exit(-E_KILLED);
        }
      }
      lapiceoi();
      break;
    case T_SYSCALL:
      syscall();
      //cprintf("syscall done\n");
      //debug_break();
      break;
    case IRQ_OFFSET + IRQ_TIMER:
      ticks++;
      if (ticks % 1000 == 0) cprintf("This is CPU #%d, Current is #%d\n", getCurrentCPU()->id, current[getCurrentCPU()->id]->pid);
      assert(current[getCurrentCPU()->id] != NULL);
      run_timer_list();
      lapiceoi();
      break;
    case IRQ_OFFSET + IRQ_COM1:
    case IRQ_OFFSET + IRQ_KBD:
      // There are user level shell in LAB8, so we need change COM/KBD interrupt processing.
      c = cons_getc();
      {
        extern void dev_stdin_write(char c);
        dev_stdin_write(c);
      }
      lapiceoi();
      break;
    case IRQ_OFFSET + IRQ_IDE1:
    case IRQ_OFFSET + IRQ_IDE2:
      lapiceoi();
      /* do nothing */
      break;
    default:
      print_trapframe(tf);
      if (current[getCurrentCPU()->id] != NULL) {
        cprintf("unhandled trap.\n");
        do_exit(-E_KILLED);
      }
      // in kernel, it must be a mistake
      panic("unexpected trap in kernel.\n");

  }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf) {
  loadgs(SEG_KCPU << 3);
  // dispatch based on what type of trap occurred
  // used for previous projects
  if (current[getCurrentCPU()->id] == NULL) {
    trap_dispatch(tf);
  } else {
    // keep a trapframe chain in stack
    struct trapframe *otf = current[getCurrentCPU()->id]->tf;
    current[getCurrentCPU()->id]->tf = tf;
    bool in_kernel = trap_in_kernel(tf);
    trap_dispatch(tf);
    current[getCurrentCPU()->id]->tf = otf;
    if (!in_kernel) {
      if (current[getCurrentCPU()->id]->flags & PF_EXITING) {
        do_exit(-E_KILLED);
      }
      if (current[getCurrentCPU()->id]->need_resched) {
        schedule();
      }
    }
  }
}

