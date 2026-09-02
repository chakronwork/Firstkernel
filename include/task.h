#ifndef FIRSTOS_TASK_H
#define FIRSTOS_TASK_H

#include <stdint.h>

#include "address_space.h"


/*
 * ============================================================
 * Task limits
 * ============================================================
 */
#define TASK_MAX 32U

#define TASK_STACK_SIZE 8192U


/*
 * ============================================================
 * Task states
 * ============================================================
 */
#define TASK_UNUSED  0U
#define TASK_READY   1U
#define TASK_RUNNING 2U
#define TASK_BLOCKED 3U
#define TASK_DEAD    4U


/*
 * ============================================================
 * CPU context
 * ============================================================
 *
 * The assembly context switch currently preserves:
 *
 *     EBP
 *     EBX
 *     ESI
 *     EDI
 *
 * ESP points to the saved register frame.
 */
struct task_context {

    uint32_t esp;
};


/*
 * ============================================================
 * Task structure
 * ============================================================
 */
struct task {

    /*
     * Unique task identifier.
     */
    uint32_t id;

    /*
     * Scheduler state.
     */
    uint32_t state;

    /*
     * Saved CPU context.
     */
    struct task_context context;

    /*
     * Kernel stack.
     */
    uint8_t *stack;

    uint32_t stack_size;

    /*
     * Task entry point.
     */
    void (*entry)(
        void *arg
    );

    /*
     * Entry argument.
     */
    void *arg;

    /*
     * Future user address space.
     *
     * v0.0.17 keeps this NULL because all tasks
     * execute in the kernel address space.
     */
    struct address_space *address_space;
};


/*
 * ============================================================
 * Task subsystem
 * ============================================================
 */
int task_init(void);


/*
 * ============================================================
 * Create task
 * ============================================================
 *
 * Returns:
 *
 *     task ID
 *     0 on failure
 */
uint32_t task_create(
    void (*entry)(void *arg),
    void *arg
);


/*
 * ============================================================
 * Get current task
 * ============================================================
 */
struct task *task_current(void);


/*
 * ============================================================
 * Get task by ID
 * ============================================================
 */
struct task *task_get(
    uint32_t id
);


/*
 * ============================================================
 * Switch directly to task
 * ============================================================
 */
int task_switch(
    struct task *next
);


/*
 * ============================================================
 * Cooperative yield
 * ============================================================
 */
void task_yield(void);


/*
 * ============================================================
 * Start scheduler
 * ============================================================
 */
int task_start(void);


/*
 * ============================================================
 * Number of created tasks
 * ============================================================
 */
uint32_t task_count(void);


#endif