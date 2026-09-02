.section .text


/*
 * ============================================================
 * Start first task
 * ============================================================
 *
 * Argument:
 *
 *     4(%esp) = pointer to task interrupt frame
 *
 * Frame:
 *
 *     edi
 *     esi
 *     ebp
 *     esp
 *     ebx
 *     edx
 *     ecx
 *     eax
 *     int_no
 *     err_code
 *     eip
 *     cs
 *     eflags
 *
 * We restore the frame exactly as isr_common() does.
 */
.global task_start_asm
.type task_start_asm, @function

task_start_asm:

    /*
     * Load task ESP.
     */
    mov 4(%esp), %eax


    /*
     * Switch to task stack.
     */
    mov %eax, %esp


    /*
     * Restore general-purpose registers.
     */
    popa


    /*
     * Remove:
     *
     *     int_no
     *     err_code
     */
    add $8, %esp


    /*
     * Enter task.
     */
    iret

.size task_start_asm, . - task_start_asm


.section .note.GNU-stack,"",@progbits
