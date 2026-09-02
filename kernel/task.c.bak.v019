#include <stdint.h>

#include "task.h"
#include "kmalloc.h"
#include "idt.h"


/*
 * ============================================================
 * External assembly
 * ============================================================
 */
extern void task_start_asm(
    uint32_t esp
);


/*
 * ============================================================
 * Task table
 * ============================================================
 */
static struct task tasks[TASK_MAX];


/*
 * ============================================================
 * Scheduler state
 * ============================================================
 */
static uint32_t next_task_id = 1U;

static uint32_t total_tasks = 0U;

static struct task *current_task = 0;


/*
 * Requested task.
 *
 * Used by task_switch().
 *
 * The actual context switch still happens inside the
 * scheduler/interrupt path.
 */
static struct task *requested_task = 0;


/*
 * ============================================================
 * CPU halt
 * ============================================================
 *
 * IMPORTANT:
 *
 * Interrupts remain enabled.
 *
 * This allows PIT IRQ0 to wake the CPU and the scheduler
 * to switch to another task.
 */
static void task_halt(void)
{
    for (;;) {

        __asm__ volatile (
            "sti\n"
            "hlt"
        );
    }
}


/*
 * ============================================================
 * Task bootstrap
 * ============================================================
 *
 * Every newly created task starts here.
 *
 * task_start_asm() enters this function using iret().
 */
static void task_bootstrap(void)
{
    struct task *task =
        current_task;


    if (task == 0) {

        task_halt();
    }


    /*
     * Task is now running.
     */
    task->state =
        TASK_RUNNING;


    /*
     * Execute task entry function.
     */
    if (task->entry != 0) {

        task->entry(
            task->arg
        );
    }


    /*
     * Entry returned.
     *
     * Task is permanently dead.
     */
    task->state =
        TASK_DEAD;


    /*
     * Do not return to task_start_asm().
     *
     * We stay in HLT.
     *
     * PIT interrupts remain enabled.
     *
     * When IRQ0 arrives, the scheduler can switch
     * to another READY task.
     */
    task_halt();
}


/*
 * ============================================================
 * Initialize task subsystem
 * ============================================================
 */
int task_init(void)
{
    for (
        uint32_t i = 0;
        i < TASK_MAX;
        ++i
    ) {

        tasks[i].id =
            0;

        tasks[i].state =
            TASK_UNUSED;

        tasks[i].context.esp =
            0;

        tasks[i].stack =
            0;

        tasks[i].stack_size =
            0;

        tasks[i].entry =
            0;

        tasks[i].arg =
            0;

        tasks[i].address_space =
            0;
    }


    next_task_id =
        1U;

    total_tasks =
        0U;

    current_task =
        0;

    requested_task =
        0;


    return 1;
}


/*
 * ============================================================
 * Find free task slot
 * ============================================================
 */
static struct task *find_free_task(void)
{
    for (
        uint32_t i = 0;
        i < TASK_MAX;
        ++i
    ) {

        if (
            tasks[i].state ==
            TASK_UNUSED
        ) {

            return &tasks[i];
        }
    }


    return 0;
}


/*
 * ============================================================
 * Prepare initial interrupt frame
 * ============================================================
 *
 * IMPORTANT:
 *
 * The layout MUST match kernel/idt.s:
 *
 *   edi
 *   esi
 *   ebp
 *   esp
 *   ebx
 *   edx
 *   ecx
 *   eax
 *   int_no
 *   err_code
 *   eip
 *   cs
 *   eflags
 *
 * After task_start_asm() performs:
 *
 *   popa
 *   add $8, %esp
 *   iret
 *
 * execution enters task_bootstrap().
 */
static uint32_t prepare_stack(
    struct task *task
)
{
    if (task == 0)
        return 0;

    if (task->stack == 0)
        return 0;

    if (
        task->stack_size <
        128U
    ) {
        return 0;
    }


    /*
     * Stack grows downward.
     */
    uintptr_t stack_top =
        (uintptr_t)task->stack +
        task->stack_size;


    /*
     * 16-byte alignment.
     */
    stack_top &=
        ~(uintptr_t)0x0FU;


    uint32_t *stack =
        (uint32_t *)stack_top;


    /*
     * ========================================================
     * CPU return frame
     * ========================================================
     */

    /*
     * EFLAGS
     *
     * Bit 9 = IF
     *
     * 0x202 = reserved bit + interrupt enable.
     */
    *(--stack) =
        0x00000202U;


    /*
     * CS
     *
     * Kernel code segment.
     */
    *(--stack) =
        0x00000008U;


    /*
     * EIP
     */
    *(--stack) =
        (uint32_t)(uintptr_t)
        task_bootstrap;


    /*
     * Error code.
     */
    *(--stack) =
        0;


    /*
     * Interrupt number.
     */
    *(--stack) =
        0;


    /*
     * EAX.
     */
    *(--stack) =
        0;


    /*
     * ECX.
     */
    *(--stack) =
        0;


    /*
     * EDX.
     */
    *(--stack) =
        0;


    /*
     * EBX.
     */
    *(--stack) =
        0;


    /*
     * ESP slot.
     *
     * popa skips this value.
     */
    *(--stack) =
        0;


    /*
     * EBP.
     */
    *(--stack) =
        0;


    /*
     * ESI.
     */
    *(--stack) =
        0;


    /*
     * EDI.
     */
    *(--stack) =
        0;


    /*
     * Return pointer to EDI.
     */
    return
        (uint32_t)(uintptr_t)
        stack;
}


/*
 * ============================================================
 * Create task
 * ============================================================
 */
uint32_t task_create(
    void (*entry)(void *arg),
    void *arg
)
{
    if (entry == 0)
        return 0;


    if (
        total_tasks >=
        TASK_MAX
    ) {
        return 0;
    }


    struct task *task =
        find_free_task();


    if (task == 0)
        return 0;


    /*
     * Allocate kernel stack.
     */
    uint8_t *stack =
        (uint8_t *)kmalloc(
            TASK_STACK_SIZE
        );


    if (stack == 0)
        return 0;


    /*
     * Initialize metadata.
     */
    task->id =
        next_task_id++;

    task->state =
        TASK_READY;

    task->stack =
        stack;

    task->stack_size =
        TASK_STACK_SIZE;

    task->entry =
        entry;

    task->arg =
        arg;

    task->address_space =
        0;


    /*
     * Prepare first CPU context.
     */
    task->context.esp =
        prepare_stack(
            task
        );


    if (
        task->context.esp == 0
    ) {

        kfree(
            task->stack
        );

        task->stack =
            0;

        task->state =
            TASK_UNUSED;

        return 0;
    }


    total_tasks++;


    return task->id;
}


/*
 * ============================================================
 * Get current task
 * ============================================================
 */
struct task *task_current(void)
{
    return current_task;
}


/*
 * ============================================================
 * Get task by ID
 * ============================================================
 */
struct task *task_get(
    uint32_t id
)
{
    if (id == 0)
        return 0;


    for (
        uint32_t i = 0;
        i < TASK_MAX;
        ++i
    ) {

        if (
            tasks[i].state !=
            TASK_UNUSED
        ) {

            if (
                tasks[i].id ==
                id
            ) {

                return &tasks[i];
            }
        }
    }


    return 0;
}


/*
 * ============================================================
 * Find current task index
 * ============================================================
 */
static uint32_t current_index(void)
{
    if (current_task == 0)
        return 0;


    uintptr_t current_address =
        (uintptr_t)current_task;

    uintptr_t first_address =
        (uintptr_t)&tasks[0];


    uintptr_t offset =
        current_address -
        first_address;


    uint32_t index =
        (uint32_t)(
            offset /
            sizeof(struct task)
        );


    if (
        index >=
        TASK_MAX
    ) {
        return 0;
    }


    return index;
}


/*
 * ============================================================
 * Find next READY task
 * ============================================================
 *
 * Simple round-robin scan.
 */
static struct task *find_next_task(void)
{
    uint32_t start =
        current_index();


    start =
        (
            start +
            1U
        ) %
        TASK_MAX;


    for (
        uint32_t offset = 0;
        offset < TASK_MAX;
        ++offset
    ) {

        uint32_t index =
            (
                start +
                offset
            ) %
            TASK_MAX;


        if (
            tasks[index].state ==
            TASK_READY
        ) {

            return &tasks[index];
        }
    }


    return 0;
}


/*
 * ============================================================
 * Scheduler tick
 * ============================================================
 *
 * Called while handling:
 *
 *     IRQ0
 *
 * or:
 *
 *     int 0x30
 *
 * regs points to the complete interrupt frame for the
 * currently executing task.
 *
 * Returns the interrupt frame that must be restored.
 */
struct registers *task_scheduler_tick(
    struct registers *regs
)
{
    if (regs == 0)
        return regs;


    /*
     * No task is running yet.
     */
    if (current_task == 0)
        return regs;


    /*
     * ========================================================
     * Save current task CPU context
     * ========================================================
     *
     * The current task may be:
     *
     *   RUNNING
     *   DEAD
     *   BLOCKED
     *
     * Its current interrupt frame is saved here.
     */
    current_task->context.esp =
        (uint32_t)(uintptr_t)regs;


    /*
     * ========================================================
     * Select next task
     * ========================================================
     */
    struct task *next =
        0;


    /*
     * Honor explicit task_switch() request first.
     */
    if (
        requested_task != 0 &&
        requested_task->state ==
        TASK_READY
    ) {

        next =
            requested_task;

        requested_task =
            0;
    }


    /*
     * Normal round-robin scheduling.
     */
    if (next == 0) {

        next =
            find_next_task();
    }


    /*
     * No READY task.
     */
    if (next == 0) {

        /*
         * Current task is still runnable.
         */
        if (
            current_task->state ==
            TASK_RUNNING
        ) {

            return regs;
        }


        /*
         * Current task is DEAD/BLOCKED and no replacement
         * exists.
         *
         * Leave the current frame alone.
         */
        return regs;
    }


    /*
     * ========================================================
     * Same task
     * ========================================================
     */
    if (
        next ==
        current_task
    ) {

        current_task->state =
            TASK_RUNNING;

        return regs;
    }


    /*
     * ========================================================
     * Current task state
     * ========================================================
     *
     * A RUNNING task becomes READY.
     *
     * A DEAD task remains DEAD.
     *
     * A BLOCKED task remains BLOCKED.
     */
    if (
        current_task->state ==
        TASK_RUNNING
    ) {

        current_task->state =
            TASK_READY;
    }


    /*
     * ========================================================
     * Activate next task
     * ========================================================
     */
    current_task =
        next;


    current_task->state =
        TASK_RUNNING;


    /*
     * ========================================================
     * Return new task's saved interrupt frame
     * ========================================================
     *
     * idt.s will:
     *
     *     mov %eax, %esp
     *     popa
     *     add $8, %esp
     *     iret
     */
    return
        (struct registers *)
        (uintptr_t)
        current_task->context.esp;
}


/*
 * ============================================================
 * Direct task switch request
 * ============================================================
 *
 * The actual switch occurs inside the interrupt-driven
 * scheduler.
 */
int task_switch(
    struct task *next
)
{
    if (next == 0)
        return 0;


    if (
        next->state !=
        TASK_READY
    ) {
        return 0;
    }


    requested_task =
        next;


    /*
     * Enter scheduler through software interrupt.
     */
    task_yield();


    return 1;
}


/*
 * ============================================================
 * Cooperative yield
 * ============================================================
 *
 * Scheduler software interrupt.
 */
void task_yield(void)
{
    __asm__ volatile (
        "int $0x30"
        :
        :
        : "memory"
    );
}


/*
 * ============================================================
 * Start scheduler
 * ============================================================
 *
 * This starts the first READY task.
 *
 * It does not return during normal operation.
 */
int task_start(void)
{
    if (
        current_task != 0
    ) {
        return 0;
    }


    struct task *first =
        0;


    /*
     * Find first READY task.
     */
    for (
        uint32_t i = 0;
        i < TASK_MAX;
        ++i
    ) {

        if (
            tasks[i].state ==
            TASK_READY
        ) {

            first =
                &tasks[i];

            break;
        }
    }


    if (first == 0)
        return 0;


    /*
     * Activate first task.
     */
    current_task =
        first;

    first->state =
        TASK_RUNNING;


    /*
     * Enter first task through an interrupt-style
     * return frame.
     *
     * task_start_asm() does not return.
     */
    task_start_asm(
        first->context.esp
    );


    /*
     * Unreachable.
     */
    return 0;
}


/*
 * ============================================================
 * Task count
 * ============================================================
 */
uint32_t task_count(void)
{
    return total_tasks;
}