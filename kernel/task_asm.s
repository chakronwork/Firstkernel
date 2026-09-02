/*
 * ============================================================
 * FirstOS cooperative task context switch
 * ============================================================
 *
 * C prototype:
 *
 *     void task_switch_asm(
 *         uint32_t *old_esp,
 *         uint32_t new_esp
 *     );
 *
 *
 * Registers preserved:
 *
 *     EBP
 *     EBX
 *     ESI
 *     EDI
 *
 *
 * Stack layout after save:
 *
 *     ESP + 0   = EDI
 *     ESP + 4   = ESI
 *     ESP + 8   = EBX
 *     ESP + 12  = EBP
 *     ESP + 16  = return address
 *     ESP + 20  = old_esp argument
 *     ESP + 24  = new_esp argument
 */


/*
 * ============================================================
 * 32-bit mode
 * ============================================================
 */
.code32


/*
 * ============================================================
 * Text section
 * ============================================================
 */
.section .text


/*
 * ============================================================
 * Export symbol
 * ============================================================
 */
.global task_switch_asm

.type task_switch_asm, @function


/*
 * ============================================================
 * task_switch_asm
 * ============================================================
 */
task_switch_asm:

    /*
     * --------------------------------------------------------
     * Save current task registers.
     * --------------------------------------------------------
     */
    push %ebp
    push %ebx
    push %esi
    push %edi


    /*
     * --------------------------------------------------------
     * Save current ESP.
     *
     * Function arguments before pushes:
     *
     *     4(%esp) = old_esp
     *     8(%esp) = new_esp
     *
     * Four pushes added 16 bytes.
     *
     * Therefore:
     *
     *     20(%esp) = old_esp
     *     24(%esp) = new_esp
     * --------------------------------------------------------
     */
    mov 20(%esp), %eax

    mov %esp, (%eax)


    /*
     * --------------------------------------------------------
     * Load next task ESP.
     *
     * new_esp is a value, not a pointer.
     * --------------------------------------------------------
     */
    mov 24(%esp), %esp


    /*
     * --------------------------------------------------------
     * Restore next task registers.
     * --------------------------------------------------------
     */
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp


    /*
     * --------------------------------------------------------
     * Resume next task.
     *
     * ret uses the return address located immediately
     * after the four saved registers.
     *
     * For a new task:
     *
     *     task_bootstrap
     *
     * For an existing task:
     *
     *     instruction after task_switch_asm()
     * --------------------------------------------------------
     */
    ret


/*
 * ============================================================
 * End
 * ============================================================
 */
.size task_switch_asm, .-task_switch_asm