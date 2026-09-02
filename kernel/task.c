#include <stdint.h>

#include "task.h"
#include "kmalloc.h"


/*
 * ============================================================
 * External assembly context switch
 * ============================================================
 */
extern void task_switch_asm(
    uint32_t *old_esp,
    uint32_t new_esp
);


/*
 * ============================================================
 * Task table
 * ============================================================
 */
static struct task tasks[
    TASK_MAX
];


/*
 * ============================================================
 * Scheduler state
 * ============================================================
 */
static uint32_t next_task_id = 1U;

static uint32_t total_tasks = 0U;

static struct task *current_task = 0;


/*
 * ============================================================
 * CPU halt
 * ============================================================
 */
static void task_halt(void)
{
    for (;;) {

        __asm__ volatile ("cli");
        __asm__ volatile ("hlt");
    }
}


/*
 * ============================================================
 * Task bootstrap
 * ============================================================
 *
 * A newly created task starts here.
 *
 * The initial task stack is prepared so that after:
 *
 *     pop edi
 *     pop esi
 *     pop ebx
 *     pop ebp
 *     ret
 *
 * execution enters this function.
 */
static void task_bootstrap(void)
{
    struct task *task =
        current_task;


    if (task == 0)
        task_halt();


    /*
     * Newly started task is now running.
     */
    task->state =
        TASK_RUNNING;


    /*
     * Execute task entry.
     */
    if (task->entry != 0) {

        task->entry(
            task->arg
        );
    }


    /*
     * Entry function returned.
     *
     * The task is now permanently dead.
     */
    task->state =
        TASK_DEAD;


    /*
     * Find another runnable task.
     *
     * A DEAD task must never return to its old entry
     * function.
     */
    for (;;) {

        task_yield();


        /*
         * If task_yield() returned and the current task
         * is still DEAD, no other runnable task exists.
         */
        if (
            task->state ==
            TASK_DEAD
        ) {

            task_halt();
        }
    }
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
 * Prepare initial task stack
 * ============================================================
 *
 * task_switch_asm() restores:
 *
 *     pop edi
 *     pop esi
 *     pop ebx
 *     pop ebp
 *     ret
 *
 *
 * Therefore the stack must be:
 *
 *     ESP + 0   fake EDI
 *     ESP + 4   fake ESI
 *     ESP + 8   fake EBX
 *     ESP + 12  fake EBP
 *     ESP + 16  task_bootstrap
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
        64U
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
     * Align stack to 16 bytes.
     */
    stack_top &=
        ~(uintptr_t)0x0FU;


    uint32_t *stack =
        (uint32_t *)stack_top;


    /*
     * Push return address first.
     */
    *(--stack) =
        (uint32_t)(uintptr_t)
        task_bootstrap;


    /*
     * Fake EBP.
     */
    *(--stack) =
        0;


    /*
     * Fake EBX.
     */
    *(--stack) =
        0;


    /*
     * Fake ESI.
     */
    *(--stack) =
        0;


    /*
     * Fake EDI.
     */
    *(--stack) =
        0;


    /*
     * ESP now points at fake EDI.
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
     * Initialize task metadata.
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


    /*
     * v0.0.17 tasks use the existing kernel
     * address space.
     *
     * This field will be used when user mode
     * is implemented.
     */
    task->address_space =
        0;


    /*
     * Create initial CPU context.
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
 * Find next READY task
 * ============================================================
 *
 * Simple round-robin search.
 */
static struct task *find_next_task(void)
{
    uint32_t start_index =
        0;


    if (
        current_task != 0
    ) {

        uintptr_t current_address =
            (uintptr_t)current_task;

        uintptr_t first_address =
            (uintptr_t)&tasks[0];


        uintptr_t index =
            (
                current_address -
                first_address
            ) / sizeof(struct task);


        start_index =
            (
                (uint32_t)index +
                1U
            ) % TASK_MAX;
    }


    for (
        uint32_t offset = 0;
        offset < TASK_MAX;
        ++offset
    ) {

        uint32_t index =
            (
                start_index +
                offset
            ) % TASK_MAX;


        if (
            tasks[index].state ==
            TASK_READY
        ) {

            return
                &tasks[index];
        }
    }


    return 0;
}


/*
 * ============================================================
 * Switch task
 * ============================================================
 */
int task_switch(
    struct task *next
)
{
    if (next == 0)
        return 0;


    /*
     * Only READY/RUNNING tasks can execute.
     */
    if (
        next->state != TASK_READY &&
        next->state != TASK_RUNNING
    ) {

        return 0;
    }


    /*
     * ==================================================
     * First task
     * ==================================================
     *
     * No old task exists yet.
     */
    if (
        current_task == 0
    ) {

        current_task =
            next;

        next->state =
            TASK_RUNNING;


        /*
         * Dummy storage because there is no
         * previous task context.
         */
        uint32_t dummy_esp =
            0;


        task_switch_asm(
            &dummy_esp,
            next->context.esp
        );


        return 1;
    }


    /*
     * Don't switch to ourselves.
     */
    if (
        current_task ==
        next
    ) {
        return 1;
    }


    struct task *old =
        current_task;


    /*
     * Only a running task becomes READY.
     *
     * A DEAD task remains DEAD.
     */
    if (
        old->state ==
        TASK_RUNNING
    ) {

        old->state =
            TASK_READY;
    }


    /*
     * Activate next task.
     */
    current_task =
        next;

    next->state =
        TASK_RUNNING;


    /*
     * Save current context and restore next.
     */
    task_switch_asm(
        &old->context.esp,
        next->context.esp
    );


    /*
     * When the old task is selected again,
     * execution resumes here.
     */
    return 1;
}


/*
 * ============================================================
 * Cooperative yield
 * ============================================================
 */
void task_yield(void)
{
    struct task *current =
        current_task;


    struct task *next =
        find_next_task();


    /*
     * Nothing else is runnable.
     */
    if (next == 0)
        return;


    /*
     * If current task is still running,
     * it becomes READY during the switch.
     */
    if (
        current != 0 &&
        current->state == TASK_RUNNING
    ) {

        current->state =
            TASK_READY;
    }


    /*
     * Switch.
     *
     * task_switch() sees current_task already set,
     * so it saves the current CPU context.
     */
    task_switch(
        next
    );
}


/*
 * ============================================================
 * Start scheduler
 * ============================================================
 */
int task_start(void)
{
    if (
        current_task != 0
    ) {
        return 0;
    }


    struct task *first =
        find_next_task();


    if (first == 0)
        return 0;


    return
        task_switch(
            first
        );
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