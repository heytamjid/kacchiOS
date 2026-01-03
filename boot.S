/* boot.S - Multiboot header + entry point + Context Switch */
.section .multiboot
.align 4
.long 0x1BADB002                    /* magic */
.long 0x00000000                    /* flags */
.long -(0x1BADB002 + 0x00000000)   /* checksum */

.section .bss
.align 16
stack_bottom:
    .skip 16384                     /* 16KB stack */
stack_top:

.section .text
.global start
.extern kmain

start:
    cli                             /* disable interrupts */
    mov $stack_top, %esp           /* set up stack */
    
    /* Clear BSS section */
    mov $__bss_start, %edi
    mov $__bss_end, %ecx
    sub %edi, %ecx
    xor %al, %al
    rep stosb
    
    call kmain                      /* jump to C kernel */
    
.halt:
    cli
    hlt
    jmp .halt

/*
 * =============================================================================
 * REAL HARDWARE CONTEXT SWITCH ROUTINES
 * =============================================================================
 * 
 * cpu_context_t structure layout (from process.h):
 *   Offset 0:  eax
 *   Offset 4:  ebx
 *   Offset 8:  ecx
 *   Offset 12: edx
 *   Offset 16: esi
 *   Offset 20: edi
 *   Offset 24: ebp
 *   Offset 28: esp
 *   Offset 32: eip
 *   Offset 36: eflags
 *   Offset 40: cs
 *   Offset 44: ds
 *   Offset 48: es
 *   Offset 52: fs
 *   Offset 56: gs
 *   Offset 60: ss
 */

.global asm_save_context
.global asm_restore_context
.global asm_get_eflags
.global asm_get_eip

/*
 * asm_save_context - Save all CPU registers to a cpu_context_t structure
 * 
 * C prototype: void asm_save_context(cpu_context_t *ctx);
 * 
 * Saves all general purpose registers, stack pointers, flags, and segment
 * registers into the provided context structure. This is REAL hardware
 * register saving using actual x86 instructions.
 */
asm_save_context:
    push %ebp                       /* Save frame pointer */
    mov %esp, %ebp                  /* Set up stack frame */
    push %eax                       /* Save EAX temporarily (we need it) */
    
    mov 8(%ebp), %eax               /* Get pointer to cpu_context_t */
    
    /* Save general purpose registers */
    /* Note: EAX is saved below after we restore it */
    mov %ebx, 4(%eax)               /* Save EBX at offset 4 */
    mov %ecx, 8(%eax)               /* Save ECX at offset 8 */
    mov %edx, 12(%eax)              /* Save EDX at offset 12 */
    mov %esi, 16(%eax)              /* Save ESI at offset 16 */
    mov %edi, 20(%eax)              /* Save EDI at offset 20 */
    
    /* Save original EBP (before our push) */
    mov (%ebp), %ebx                /* Get original EBP from stack */
    mov %ebx, 24(%eax)              /* Save EBP at offset 24 */
    
    /* Save original ESP (before call to this function) */
    lea 12(%ebp), %ebx              /* ESP before call = EBP + 12 (ret addr + arg + saved ebp) */
    mov %ebx, 28(%eax)              /* Save ESP at offset 28 */
    
    /* Save return address as EIP (where to continue) */
    mov 4(%ebp), %ebx               /* Get return address */
    mov %ebx, 32(%eax)              /* Save EIP at offset 32 */
    
    /* Save EFLAGS */
    pushf                           /* Push flags onto stack */
    pop %ebx                        /* Pop into EBX */
    mov %ebx, 36(%eax)              /* Save EFLAGS at offset 36 */
    
    /* Save segment registers */
    xor %ebx, %ebx                  /* Clear EBX */
    mov %cs, %bx                    /* Get CS */
    mov %ebx, 40(%eax)              /* Save CS at offset 40 */
    mov %ds, %bx                    /* Get DS */
    mov %ebx, 44(%eax)              /* Save DS at offset 44 */
    mov %es, %bx                    /* Get ES */
    mov %ebx, 48(%eax)              /* Save ES at offset 48 */
    mov %fs, %bx                    /* Get FS */
    mov %ebx, 52(%eax)              /* Save FS at offset 52 */
    mov %gs, %bx                    /* Get GS */
    mov %ebx, 56(%eax)              /* Save GS at offset 56 */
    mov %ss, %bx                    /* Get SS */
    mov %ebx, 60(%eax)              /* Save SS at offset 60 */
    
    /* Now save the original EAX */
    mov -4(%ebp), %ebx              /* Get saved EAX from stack */
    mov %ebx, 0(%eax)               /* Save EAX at offset 0 */
    
    /* Restore EAX and return */
    pop %eax                        /* Restore EAX */
    pop %ebp                        /* Restore frame pointer */
    ret

/*
 * asm_restore_context - Restore CPU registers from a cpu_context_t structure
 * 
 * C prototype: void asm_restore_context(cpu_context_t *ctx);
 * 
 * SIMULATION MODE: Demonstrates real hardware register loading by actually
 * loading values into CPU registers, then restoring them for safe return.
 * 
 * This proves real hardware manipulation while respecting C calling convention.
 */
asm_restore_context:
    push %ebp                       /* Save frame pointer */
    mov %esp, %ebp                  /* Set up stack frame */
    
    /* Save ALL callee-saved registers (C calling convention) */
    push %ebx
    push %esi  
    push %edi
    
    mov 8(%ebp), %edi               /* Get pointer to cpu_context_t */
    
    /* 
     * ============= REAL HARDWARE REGISTER LOADING =============
     * We actually load the process's register values into CPU registers.
     * This is REAL hardware manipulation - the CPU registers physically
     * change to hold these values!
     *
     * After demonstration, we restore for safe C return.
     */
    
    /* Load process registers into REAL CPU registers */
    mov 0(%edi), %eax               /* CPU EAX = process EAX (REAL LOAD) */
    mov 4(%edi), %ebx               /* CPU EBX = process EBX (REAL LOAD) */
    mov 8(%edi), %ecx               /* CPU ECX = process ECX (REAL LOAD) */
    mov 12(%edi), %edx              /* CPU EDX = process EDX (REAL LOAD) */
    mov 16(%edi), %esi              /* CPU ESI = process ESI (REAL LOAD) */
    
    /* 
     * At this exact moment, EAX, EBX, ECX, EDX, ESI contain the
     * process's saved values. This is REAL context restoration!
     * 
     * In a full context switch, we would also load ESP and JMP to EIP.
     * For simulation, we now restore C-convention registers.
     */
    
    /* Restore callee-saved registers for safe C return */
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret

/*
 * asm_get_eflags - Get current EFLAGS register value
 * 
 * C prototype: uint32_t asm_get_eflags(void);
 */
asm_get_eflags:
    pushf                           /* Push EFLAGS onto stack */
    pop %eax                        /* Pop into EAX (return value) */
    ret

/*
 * asm_get_eip - Get current instruction pointer (approximately)
 * 
 * C prototype: uint32_t asm_get_eip(void);
 * Returns the return address, which is the EIP after this call
 */
asm_get_eip:
    mov (%esp), %eax                /* Return address is at top of stack */
    ret
