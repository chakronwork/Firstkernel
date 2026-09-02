#ifndef FIRSTOS_TASK_H
#define FIRSTOS_TASK_H

#include <stdint.h>

#include "address_space.h"
#include "idt.h"


#define TASK_MAX 32U
#define TASK_STACK_SIZE 4096U


#define TASK_UNUSED  0U
#define TASK_READY   1U
#define TASK_RUNNING 2U
#define TASK_BLOCKED 3U
#define TASK_DEAD    4U


struct task_context
{
    uint32_t esp;
};


struct task
{
    uint32_t id;

    uint32_t state;

    /*
     * Absolute PIT tick at which a sleeping task
     * becomes READY.
     */
    uint32_t wake_tick;

    struct task_context context;

    uint8_t *stack;
    uint32_t stack_size;

    /*
     * Dedicated kernel entry stack.
     *
     * TSS.ESP0 points to the top of this stack while
     * this task is executing in user mode.
     */
    uint8_t *kernel_stack;
    uint32_t kernel_stack_size;

    /*
     * User-mode execution state.
     */
    uint32_t user_entry;
    uint32_t user_esp;
    uint32_t user_mode;

    /*
     * User stack backing page.
     */
    uint32_t user_stack_physical;
    uint32_t user_stack_virtual;
    uint32_t user_stack_size;

    void (*entry)(
        void *arg
    );

    void *arg;

    struct address_space *address_space;
};


int task_init(void);


uint32_t task_create(
    void (*entry)(void *arg),
    void *arg
);

/*
 * Create a task whose saved context returns to CPL3.
 */
uint32_t task_create_user(
    uint32_t user_entry,
    uint32_t user_esp
);



struct task *task_current(void);


struct task *task_get(
    uint32_t id
);


int task_switch(
    struct task *next
);


void task_yield(void);


/*
 * Terminate the current task.
 *
 * The task is marked TASK_DEAD and the scheduler
 * is responsible for selecting another runnable task.
 *
 * Resources are intentionally not freed here because
 * the current task may still be executing on its
 * kernel stack.
 */
void task_exit(void);


/*
 * Block the current task for a number of PIT ticks.
 *
 * ticks == 0 performs a normal yield.
 */
void task_sleep(
    uint32_t ticks
);


/*
 * Wake a blocked task immediately.
 */
void task_wake(
    struct task *task
);


int task_start(void);


struct registers *task_scheduler_tick(
    struct registers *regs
);


uint32_t task_count(void);


#endif /* FIRSTOS_TASK_H */
