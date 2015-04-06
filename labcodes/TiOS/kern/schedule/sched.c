#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <assert.h>
#include <default_sched.h>
#include <spinlock.h>



// the list of timer
static list_entry_t timer_list;

static struct sched_class *sched_class;

static struct run_queue *rq;

static inline void
sched_class_enqueue(struct proc_struct *proc) {
  if (not_equal_to_guard(proc)) {
//    cprintf("%d Enqueued, %p, %p\n", proc->pid, proc, guard_proc_smp);
    sched_class->enqueue(rq, proc);
  }
}

static inline void
sched_class_dequeue(struct proc_struct *proc) {
  sched_class->dequeue(rq, proc);
}

static inline struct proc_struct *
sched_class_pick_next(void) {
  return sched_class->pick_next(rq);
}

static void
sched_class_proc_tick(struct proc_struct *proc) {
//  cprintf("This is it\n");
//  cprintf("%p, %p\n", proc, idleproc);
  if (not_equal_to_guard(proc)) {
    sched_class->proc_tick(rq, proc);
  }
  else {
    proc->need_resched = 1;
  }
}

static struct run_queue __rq;

void
sched_init(void) {
  list_init(&timer_list);

  sched_class = &default_sched_class;

  rq = &__rq;
  rq->max_time_slice = MAX_TIME_SLICE;
  sched_class->init(rq);

  cprintf("sched class: %s\n", sched_class->name);
}

void
wakeup_proc(struct proc_struct *proc) {
  assert(proc->state != PROC_ZOMBIE);
  bool intr_flag;
  local_intr_save(intr_flag);
  {
    if (proc->state != PROC_RUNNABLE) {
      proc->state = PROC_RUNNABLE;
      proc->wait_state = 0;
      if (proc != current[getCurrentCPU()->id]) {
        sched_class_enqueue(proc);
      }
    }
    else {
      warn("wakeup runnable process.\n");
    }
  }
  local_intr_restore(intr_flag);
}

extern struct spinlock process_lock;

void schedule(void) {
  loadgs(SEG_KCPU << 3);
  bool intr_flag;
  struct proc_struct *next;
//  cprintf("CPU:#%d, current:#%d\n", getCurrentCPU()->id, current[getCurrentCPU()->id]->pid);
  {
    current[getCurrentCPU()->id]->need_resched = 0;
    acquire(&process_lock);
    if (current[getCurrentCPU()->id]->state == PROC_RUNNABLE) {
      sched_class_enqueue(current[getCurrentCPU()->id]);
    }
    if ((next = sched_class_pick_next()) != NULL) {
      sched_class_dequeue(next);
    }
    release(&process_lock);
    if (next == NULL) {
      next = guard_proc_smp[getCurrentCPU()->id];
    }
    next->runs ++;
    if (next != current[getCurrentCPU()->id]) {
      proc_run(next);
    }
  }
}

void
add_timer(timer_t *timer) {
  bool intr_flag;
  local_intr_save(intr_flag);
  {
    assert(timer->expires > 0 && timer->proc != NULL);
    assert(list_empty(&(timer->timer_link)));
    list_entry_t *le = list_next(&timer_list);
    while (le != &timer_list) {
      timer_t *next = le2timer(le, timer_link);
      if (timer->expires < next->expires) {
        next->expires -= timer->expires;
        break;
      }
      timer->expires -= next->expires;
      le = list_next(le);
    }
    list_add_before(le, &(timer->timer_link));
  }
  local_intr_restore(intr_flag);
}

// del timer from timer_list
void
del_timer(timer_t *timer) {
  bool intr_flag;
  local_intr_save(intr_flag);
  {
    if (!list_empty(&(timer->timer_link))) {
      if (timer->expires != 0) {
        list_entry_t *le = list_next(&(timer->timer_link));
        if (le != &timer_list) {
          timer_t *next = le2timer(le, timer_link);
          next->expires += timer->expires;
        }
      }
      list_del_init(&(timer->timer_link));
    }
  }
  local_intr_restore(intr_flag);
}

// call scheduler to update tick related info, and check the timer is expired? If expired, then wakup proc
void
run_timer_list(void) {
  bool intr_flag;
  local_intr_save(intr_flag);
  {
    list_entry_t *le = list_next(&timer_list);

    if (le != &timer_list) {
      timer_t *timer = le2timer(le, timer_link);
      assert(timer->expires != 0);
      timer->expires --;
      while (timer->expires == 0) {
        le = list_next(le);
        struct proc_struct *proc = timer->proc;
        if (proc->wait_state != 0) {
          assert(proc->wait_state & WT_INTERRUPTED);
        } else {
          warn("process %d's wait_state == 0.\n", proc->pid);
        }
        wakeup_proc(proc);
        del_timer(timer);
        if (le == &timer_list) {
          break;
        }
        timer = le2timer(le, timer_link);
      }

    }
//    cprintf("This is CPU handling Interrupt #%d\n", getCurrentCPU()->id);
    sched_class_proc_tick(current[getCurrentCPU()->id]);
  }
  local_intr_restore(intr_flag);
}