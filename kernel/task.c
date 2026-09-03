#include <stdint.h>

#include "task.h"
#include "serial.h"
#include "gdt.h"
#include "kmalloc.h"
#include "pmm.h"
#include "idt.h"


#include "timer.h"
#include "tss.h"
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

static uint32_t user_switch_log_count = 0U;


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

        
        tasks[i].wake_tick =
            0;
tasks[i].context.esp =
            0;

        tasks[i].stack =
            0;

        tasks[i].stack_size =
            0;

        tasks[i].kernel_stack =
            0;

        tasks[i].kernel_stack_size =
            0;

        tasks[i].user_entry =
            0;

        tasks[i].user_esp =
            0;

        tasks[i].user_mode =
            0;

        tasks[i].user_code_physical =
            0;

        tasks[i].user_code_virtual =
            0;

        tasks[i].user_code_size =
            0;

        tasks[i].user_stack_physical =
            0;

        tasks[i].user_stack_virtual =
            0;

        tasks[i].user_stack_size =
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
 * Prepare initial Ring 3 interrupt frame
 * ============================================================
 *
 * Frame:
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
 *   user_esp
 *   user_ss
 *
 * idt.s restores this with:
 *
 *   popa
 *   add $8, %esp
 *   iret
 */
static uint32_t prepare_user_stack(
    struct task *task
)
{
    if (task == 0)
        return 0;

    if (task->kernel_stack == 0)
        return 0;

    if (task->kernel_stack_size < 128U)
        return 0;

    uintptr_t stack_top =
        (uintptr_t)task->kernel_stack +
        task->kernel_stack_size;

    stack_top &=
        ~(uintptr_t)0x0FU;

    uint32_t *stack =
        (uint32_t *)stack_top;

    /*
     * CPU-level iret frame.
     */
    *(--stack) =
        GDT_USER_DATA | 0x3U;

    *(--stack) =
        task->user_esp;

    /*
     * EFLAGS:
     *
     * bit 1 = reserved
     * bit 9 = IF
     */
    *(--stack) =
        0x00000202U;

    *(--stack) =
        GDT_USER_CODE | 0x3U;

    *(--stack) =
        task->user_entry;

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
     * General registers.
     */
    *(--stack) = 0; /* eax */
    *(--stack) = 0; /* ecx */
    *(--stack) = 0; /* edx */
    *(--stack) = 0; /* ebx */
    *(--stack) = 0; /* esp slot */
    *(--stack) = 0; /* ebp */
    *(--stack) = 0; /* esi */
    *(--stack) = 0; /* edi */

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
     *
     * Do this before creating the address space so a
     * failed stack allocation cannot leak an address space.
     */
    uint8_t *stack =
        (uint8_t *)kmalloc(
            TASK_STACK_SIZE
        );

    if (stack == 0)
        return 0;

    /*
     * Every task owns an address space.
     */
    struct address_space *space =
        address_space_create();

    if (space == 0) {
        kfree(stack);
        return 0;
    }


    /*
     * Initialize metadata.
     */
    task->id =
        next_task_id++;

    task->state =
        TASK_READY;

    
    task->wake_tick =
        0;
task->stack =
        stack;

    task->stack_size =
        TASK_STACK_SIZE;

    /*
     * For the current task model, the task-private stack
     * is also its Ring 0 kernel entry stack.
     *
     * A future user task will additionally have a separate
     * user-mode stack.
     */
    task->kernel_stack =
        task->stack;

    task->kernel_stack_size =
        TASK_STACK_SIZE;

    task->user_entry =
        0;

    task->user_esp =
        0;

    task->user_mode =
        0;

    task->user_code_physical =
        0;

    task->user_code_virtual =
        0;

    task->user_code_size =
        0;

    task->user_stack_physical =
        0;

    task->user_stack_virtual =
        0;

    task->user_stack_size =
        0;

    task->entry =
        entry;

    task->arg =
        arg;

    task->address_space =
        space;


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

        address_space_destroy(
            task->address_space
        );

        task->address_space =
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
 * Create User Task
 * ============================================================
 *
 * The task begins in CPL3 through its saved iret frame.
 *
 * user_entry:
 *     user virtual instruction pointer
 *
 * user_esp:
 *     user virtual stack pointer
 */
uint32_t task_create_user(
    uint32_t user_entry,
    uint32_t user_esp
)
{
    if (user_entry == 0)
        return 0;

    if (user_esp == 0)
        return 0;

    if (total_tasks >= TASK_MAX)
        return 0;

    struct task *task =
        find_free_task();

    if (task == 0)
        return 0;

    uint8_t *stack =
        (uint8_t *)kmalloc(
            TASK_STACK_SIZE
        );

    if (stack == 0)
        return 0;

    struct address_space *space =
        address_space_create();

    if (space == 0) {
        kfree(stack);
        return 0;
    }

    task->id =
        next_task_id++;

    task->state =
        TASK_BLOCKED;

    task->wake_tick =
        0;

    task->stack =
        stack;

    task->stack_size =
        TASK_STACK_SIZE;

    task->kernel_stack =
        stack;

    task->kernel_stack_size =
        TASK_STACK_SIZE;

    task->user_entry =
        user_entry;

    task->user_esp =
        user_esp;

    task->user_mode =
        1;

    task->user_code_physical =
        0;

    task->user_code_virtual =
        0;

    task->user_code_size =
        0;

    task->user_stack_physical =
        0;

    task->user_stack_virtual =
        0;

    task->user_stack_size =
        0;

    task->entry =
        0;

    task->arg =
        0;

    task->address_space =
        space;

    /*
     * Saved context is an iret frame that enters CPL3.
     */
    task->context.esp =
        prepare_user_stack(
            task
        );

    if (task->context.esp == 0) {

        address_space_destroy(
            task->address_space
        );

        task->address_space =
            0;

        kfree(
            task->stack
        );

        task->stack =
            0;

        task->kernel_stack =
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
 * Reap dead task
 * ============================================================
 *
 * A dead task can only be reclaimed after the scheduler
 * has switched to another task.
 *
 * The current task is therefore never reaped by this pass.
 */
static uint32_t reuse_test_triggered =
    0;

/*
 * Expected slot that must be reused by the replacement task.
 */
static struct task *reuse_test_slot =
    0;

/*
 * Replacement task ID.
 */
static uint32_t reuse_test_task_id =
    0;

/*
 * Free-page count before replacement task creation.
 */
static uint32_t reuse_test_baseline_free_pages =
    0;

static uint32_t reuse_test_baseline_total_tasks =
    0;

/*
 * Lifecycle verification result.
 */
static uint32_t reuse_test_failed =
    0;

/*
 * Internal regression-test helper.
 */
static uint32_t task_reuse_test(void);

static void reap_dead_tasks(void)
{
    for (
        uint32_t i = 0;
        i < TASK_MAX;
        ++i
    ) {
        struct task *task =
            &tasks[i];

        if (
            task->state !=
            TASK_DEAD
        ) {
            continue;
        }

        /*
         * Never reclaim the task whose CPU context
         * is currently active.
         */
        if (
            task ==
            current_task
        ) {
            continue;
        }

        /*
         * User code page.
         */
        if (
            task->user_code_physical != 0
        ) {
            if (
                task->address_space != 0 &&
                task->user_code_virtual != 0
            ) {
                address_space_unmap_page(
                    task->address_space,
                    task->user_code_virtual
                );
            }

            pmm_free_page(
                task->user_code_physical
            );

            task->user_code_physical =
                0;
        }

        task->user_code_virtual =
            0;

        task->user_code_size =
            0;

        /*
         * User stack page.
         */
        if (
            task->user_stack_physical != 0
        ) {
            if (
                task->address_space != 0 &&
                task->user_stack_virtual != 0
            ) {
                address_space_unmap_page(
                    task->address_space,
                    task->user_stack_virtual
                );
            }

            pmm_free_page(
                task->user_stack_physical
            );

            task->user_stack_physical =
                0;
        }

        task->user_stack_virtual =
            0;

        task->user_stack_size =
            0;

        /*
         * Address space.
         *
         * At this point the dead task is no longer
         * the active CR3, so destruction is safe.
         */
        if (
            task->address_space != 0
        ) {
            address_space_destroy(
                task->address_space
            );

            task->address_space =
                0;
        }

        /*
         * Free task-owned kernel stack.
         *
         * Current implementation aliases stack and
         * kernel_stack, so free the underlying allocation
         * exactly once.
         */
        if (
            task->stack != 0
        ) {
            kfree(
                task->stack
            );
        }

        task->stack =
            0;

        task->stack_size =
            0;

        task->kernel_stack =
            0;

        task->kernel_stack_size =
            0;

        /*
         * Preserve identity for lifecycle verification.
         */
        uint32_t reaped_task_id =
            task->id;

        struct task *reaped_task_slot =
            task;

        /*
         * Clear execution metadata.
         */
        task->id =
            0;

        task->wake_tick =
            0;

        task->context.esp =
            0;

        task->user_entry =
            0;

        task->user_esp =
            0;

        task->user_mode =
            0;

        task->entry =
            0;

        task->arg =
            0;

        task->state =
            TASK_UNUSED;

        if (
            total_tasks > 0
        ) {
            total_tasks--;
        }

        serial_write(
            "[reaper] task resources reclaimed\n"
        );

        /*
         * Run the lifecycle regression test once.
         *
         * The dead task has now been fully reclaimed and
         * its slot is TASK_UNUSED.
         */
        if (
            reuse_test_triggered == 0
        ) {
            reuse_test_triggered =
                1;

            reuse_test_slot =
                reaped_task_slot;

            task_reuse_test();
        }
        else if (
            reuse_test_task_id != 0 &&
            reaped_task_id ==
            reuse_test_task_id
        ) {
            /*
             * The replacement task reached TASK_DEAD and
             * has now been fully reclaimed.
             *
             * Verify the task accounting returned to the
             * pre-create state.
             */
            if (
                total_tasks != reuse_test_baseline_total_tasks
            ) {
                serial_write(
                    "[reuse-test] FAIL: total_tasks did not recover\n"
                );

                reuse_test_failed =
                    1;
            }
            else {
                serial_write(
                    "[reuse-test] OK: total_tasks recovered\n"
                );
            }

            if (
                pmm_get_free_pages() !=
                reuse_test_baseline_free_pages
            ) {
                serial_write(
                    "[reuse-test] FAIL: free_pages did not recover\n"
                );

                reuse_test_failed =
                    1;
            }
            else {
                serial_write(
                    "[reuse-test] OK: free_pages recovered\n"
                );
            }

            if (
                reuse_test_failed == 0
            ) {
                serial_write(
                    "[reuse-test] PASS: lifecycle verification\n"
                );
            }
        }
    }
}


/*
 * ============================================================
 * Exit current task
 * ============================================================
 *
 * Mark the current task as DEAD.
 *
 * Do not free the task stack or address space here.
 * The CPU is still executing on this task's kernel stack
 * while the syscall/ISR path is active.
 *
 * Resource reclamation is performed after a scheduler switch.
 */
void task_exit(void)
{
    if (current_task == 0)
        return;

    current_task->state =
        TASK_DEAD;
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

/*
 * ============================================================
 * Wake blocked task
 * ============================================================
 */
void task_wake(
    struct task *task
)
{
    if (task == 0)
        return;

    if (
        task->state !=
        TASK_BLOCKED
    ) {
        return;
    }

    task->wake_tick =
        0;

    task->state =
        TASK_READY;
}


/*
 * ============================================================
 * Sleep current task
 * ============================================================
 */
void task_sleep(
    uint32_t ticks
)
{
    if (current_task == 0)
        return;

    /*
     * Zero ticks means normal voluntary yield.
     */
    if (ticks == 0U) {
        task_yield();
        return;
    }

    /*
     * Set absolute wakeup tick.
     */
    current_task->wake_tick =
        timer_get_ticks() +
        ticks;

    /*
     * Block current task.
     */
    current_task->state =
        TASK_BLOCKED;

    /*
     * Enter scheduler.
     */
    task_yield();
}


/*
 * ============================================================
 * Wake expired blocked tasks
 * ============================================================
 */
static void wake_expired_tasks(
    uint32_t current_tick
)
{
    for (
        uint32_t i = 0;
        i < TASK_MAX;
        ++i
    ) {

        if (
            tasks[i].state !=
            TASK_BLOCKED
        ) {
            continue;
        }

        if (
            tasks[i].wake_tick <=
            current_tick
        ) {

            task_wake(
                &tasks[i]
            );
        }
    }
}


/*
 * ============================================================
 * Scheduler invariants
 * ============================================================
 *
 * These checks verify the relationship between:
 *
 *     current_task
 *     task state
 *     address space / CR3
 *     kernel stack
 *     TSS.ESP0
 *
 * A violation is fatal because continuing with an invalid
 * scheduler state can corrupt execution context.
 */
static int task_check_scheduler_invariants(void)
{
    if (
        current_task == 0
    ) {
        serial_write(
            "[panic] scheduler invariant: current_task is NULL\n"
        );

        return 0;
    }

    if (
        current_task->state !=
        TASK_RUNNING
    ) {
        serial_write(
            "[panic] scheduler invariant: current task not RUNNING\n"
        );

        return 0;
    }

    if (
        current_task->address_space == 0
    ) {
        serial_write(
            "[panic] scheduler invariant: current address space is NULL\n"
        );

        return 0;
    }

    if (
        current_task->kernel_stack == 0 ||
        current_task->kernel_stack_size == 0
    ) {
        serial_write(
            "[panic] scheduler invariant: current kernel stack invalid\n"
        );

        return 0;
    }

    if (
        address_space_get_current_cr3() !=
        current_task->address_space->page_directory_physical
    ) {
        serial_write(
            "[panic] scheduler invariant: CR3 mismatch\n"
        );

        return 0;
    }

    uint32_t expected_esp0 =
        (uint32_t)(
            uintptr_t
        )(
            current_task->kernel_stack +
            current_task->kernel_stack_size
        );

    if (
        tss_get_kernel_stack() !=
        expected_esp0
    ) {
        serial_write(
            "[panic] scheduler invariant: TSS.ESP0 mismatch\n"
        );

        return 0;
    }

    return 1;
}


/*
 * ============================================================
 * Scheduler tick
 * ============================================================
 */

struct registers *task_scheduler_tick(
    struct registers *regs
)
{

    /*
     * Wake tasks whose sleep interval has expired.
     */
    wake_expired_tasks(
        timer_get_ticks()
    );

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
     * Validate incoming task before changing current state.
     * ========================================================
     *
     * If any validation fails, the current task remains
     * RUNNING and its CR3/TSS state remains authoritative.
     */
    if (
        next->address_space == 0
    ) {
        serial_write(
            "[panic] scheduler: next task has no address space\n"
        );

        return regs;
    }

    if (
        next->kernel_stack == 0 ||
        next->kernel_stack_size == 0
    ) {
        serial_write(
            "[panic] scheduler: next task has invalid kernel stack\n"
        );

        return regs;
    }

    /*
     * ========================================================
     * Activate incoming address space.
     * ========================================================
     */
    if (
        !address_space_switch(
            next->address_space
        )
    ) {
        serial_write(
            "[panic] scheduler: address-space switch failed\n"
        );

        return regs;
    }

    /*
     * Update TSS.ESP0 only after the incoming task has been
     * validated completely.
     */
    tss_set_kernel_stack(
        (uint32_t)(
            uintptr_t
        )(
            next->kernel_stack +
            next->kernel_stack_size
        )
    );

    /*
     * ========================================================
     * Commit task switch.
     * ========================================================
     *
     * Only now is the old RUNNING task allowed to become
     * READY, and only now does current_task change.
     */
    if (
        current_task->state ==
        TASK_RUNNING
    ) {
        current_task->state =
            TASK_READY;
    }

    current_task =
        next;

    current_task->state =
        TASK_RUNNING;

    /*
     * The old task is no longer the active task.
     * It is now safe to reclaim resources belonging
     * to DEAD tasks.
     */
    reap_dead_tasks();

    /*
     * Verify scheduler invariants after the switch and
     * resource reclamation have completed.
     */
    if (
        !task_check_scheduler_invariants()
    ) {
        task_halt();
    }

    /*
     * Diagnostic:
     *
     * Show that the preemptive scheduler has actually
     * selected a CPL3 task.
     */
    if (
        current_task->user_mode != 0 &&
        user_switch_log_count < 20U
    ) {
        user_switch_log_count++;

        serial_write(
            "[sched] switched to Ring 3 task\n"
        );
    }



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
    /*
     * Load the first task's address space before
     * entering its saved CPU context.
     */
    if (
        first->address_space == 0
    ) {
        return 0;
    }

    if (
        !address_space_switch(
            first->address_space
        )
    ) {
        return 0;
    }

    /*
     * TSS.ESP0 must point to the kernel stack belonging
     * to the task that is about to run.
     */
    if (
        first->kernel_stack == 0 ||
        first->kernel_stack_size == 0
    ) {
        return 0;
    }

    tss_set_kernel_stack(
        (uint32_t)(
            uintptr_t
        )(
            first->kernel_stack +
            first->kernel_stack_size
        )
    );

    current_task =
        first;

    first->state =
        TASK_RUNNING;

    /*
     * Verify the initial scheduler state before entering
     * the first task.
     */
    if (
        !task_check_scheduler_invariants()
    ) {
        return 0;
    }


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

/*
 * ============================================================
 * Task reuse regression test
 * ============================================================
 */

static void task_reuse_test_entry(
    void *arg
)
{
    (void)arg;

    serial_write(
        "[reuse-test] replacement task running\n"
    );

    /*
     * Returning marks this task TASK_DEAD.
     * The normal reaper path will reclaim it later.
     */
}


static uint32_t task_reuse_test(void)
{
    uint32_t before_count =
        total_tasks;

    reuse_test_baseline_free_pages =
        pmm_get_free_pages();

    reuse_test_baseline_total_tasks =
        before_count;

    serial_write(
        "[reuse-test] before total_tasks="
    );

    serial_write_dec(
        before_count
    );

    serial_write(
        " free_pages="
    );

    serial_write_dec(
        reuse_test_baseline_free_pages
    );

    serial_write(
        "\n"
    );

    uint32_t task_id =
        task_create(
            task_reuse_test_entry,
            0
        );

    if (task_id == 0) {
        serial_write(
            "[reuse-test] FAIL: replacement task creation\n"
        );

        reuse_test_failed =
            1;

        return 0;
    }

    reuse_test_task_id =
        task_id;

    /*
     * Verify that find_free_task() reused exactly the slot
     * that the reaper just returned to TASK_UNUSED.
     */
    struct task *replacement =
        task_get(
            task_id
        );

    if (
        replacement !=
        reuse_test_slot
    ) {
        serial_write(
            "[reuse-test] FAIL: task slot was not reused\n"
        );

        reuse_test_failed =
            1;
    }
    else {
        serial_write(
            "[reuse-test] OK: task slot reused\n"
        );
    }

    serial_write(
        "[reuse-test] replacement task id="
    );

    serial_write_dec(
        task_id
    );

    serial_write(
        "\n"
    );

    serial_write(
        "[reuse-test] after total_tasks="
    );

    serial_write_dec(
        total_tasks
    );

    serial_write(
        " free_pages="
    );

    serial_write_dec(
        pmm_get_free_pages()
    );

    serial_write(
        "\n"
    );

    if (
        total_tasks !=
        before_count + 1U
    ) {
        serial_write(
            "[reuse-test] FAIL: total_tasks create accounting\n"
        );

        reuse_test_failed =
            1;
    }
    else {
        serial_write(
            "[reuse-test] OK: total_tasks create accounting\n"
        );
    }

    return task_id;
}


uint32_t task_count(void)
{
    return total_tasks;
}