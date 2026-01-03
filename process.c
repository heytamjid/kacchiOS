/* process.c - Process Manager Implementation */
#include "process.h"
#include "memory.h"
#include "string.h"
#include "serial.h"

/* Process table - array of process pointers */
static process_t *process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;  /* PID 0 reserved for null process */
static process_t *current_process = NULL;
static uint32_t total_processes_created = 0;

/* Ready queue (circular linked list) */
static process_t *ready_queue_head = NULL;
static process_t *ready_queue_tail = NULL;

/* Simple tick counter for timing */
static uint32_t system_ticks = 0;

/* Forward declarations for internal functions */
static void process_init_pcb(process_t *proc, const char *name, process_priority_t priority);
static void process_add_to_table(process_t *proc);
static void process_remove_from_table(uint32_t pid);
static void process_add_to_ready_queue(process_t *proc);
static void process_remove_from_ready_queue(process_t *proc);

/*
 * Initialize the process manager
 */
void process_init(void) {
    /* Clear process table */
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        process_table[i] = NULL;
    }
    
    current_process = NULL;
    ready_queue_head = NULL;
    ready_queue_tail = NULL;
    next_pid = 1;
    total_processes_created = 0;
    system_ticks = 0;
    
    serial_puts("[PROCESS] Process manager initialized\n");
    serial_puts("[PROCESS] Max processes: ");
    serial_put_dec(MAX_PROCESSES);
    serial_puts("\n");
}

/*
 * Initialize a Process Control Block
 */
static void process_init_pcb(process_t *proc, const char *name, process_priority_t priority) {
    proc->pid = next_pid++;
    
    /* Copy process name */
    size_t len = strlen(name);
    if (len > 31) len = 31;
    memcpy(proc->name, name, len);
    proc->name[len] = '\0';
    
    proc->state = PROC_STATE_READY;
    proc->priority = priority;
    
    /* Memory info will be set by caller */
    proc->stack_base = NULL;
    proc->stack_top = NULL;
    proc->stack_size = 0;
    
    /* Clear CPU context */
    memset(&proc->context, 0, sizeof(cpu_context_t));
    
    /* Scheduling info */
    proc->time_quantum = 100;  /* Default time quantum */
    proc->cpu_time = 0;
    proc->required_time = 0;   /* No requirement by default */
    proc->wait_time = 0;
    proc->creation_time = system_ticks;
    
    /* IPC */
    memset(proc->message_queue, 0, sizeof(proc->message_queue));
    proc->msg_count = 0;
    proc->waiting_for_msg = 0;
    
    /* Relationships */
    proc->parent_pid = (current_process != NULL) ? current_process->pid : 0;
    
    /* Exit status */
    proc->exit_code = 0;
    
    /* Aging */
    proc->age = 0;
    
    /* List pointers */
    proc->next = NULL;
    proc->prev = NULL;
}

/*
 * Add process to process table
 */
static void process_add_to_table(process_t *proc) {
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] == NULL) {
            process_table[i] = proc;
            return;
        }
    }
    serial_puts("[PROCESS] Warning: Process table full!\n");
}

/*
 * Remove process from process table
 */
static void process_remove_from_table(uint32_t pid) {
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] != NULL && process_table[i]->pid == pid) {
            process_table[i] = NULL;
            return;
        }
    }
}

/*
 * Add process to ready queue (priority-based insertion)
 */
static void process_add_to_ready_queue(process_t *proc) {
    proc->state = PROC_STATE_READY;
    
    /* Empty queue */
    if (ready_queue_head == NULL) {
        ready_queue_head = proc;
        ready_queue_tail = proc;
        proc->next = NULL;
        proc->prev = NULL;
        return;
    }
    
    /* Insert based on priority (higher number = higher priority) */
    process_t *current = ready_queue_head;
    
    /* Insert at head if higher priority than head */
    if (proc->priority > current->priority) {
        proc->next = ready_queue_head;
        proc->prev = NULL;
        ready_queue_head->prev = proc;
        ready_queue_head = proc;
        return;
    }
    
    /* Find insertion point */
    while (current->next != NULL && current->next->priority >= proc->priority) {
        current = current->next;
    }
    
    /* Insert after current */
    proc->next = current->next;
    proc->prev = current;
    
    if (current->next != NULL) {
        current->next->prev = proc;
    } else {
        ready_queue_tail = proc;  /* Inserting at tail */
    }
    
    current->next = proc;
}

/*
 * Remove process from ready queue
 */
static void process_remove_from_ready_queue(process_t *proc) {
    if (proc->prev != NULL) {
        proc->prev->next = proc->next;
    } else {
        ready_queue_head = proc->next;  /* Removing head */
    }
    
    if (proc->next != NULL) {
        proc->next->prev = proc->prev;
    } else {
        ready_queue_tail = proc->prev;  /* Removing tail */
    }
    
    proc->next = NULL;
    proc->prev = NULL;
}

/*
 * Create a new process
 */
process_t *process_create(const char *name, process_func_t entry_point, process_priority_t priority) {
    /* Allocate PCB */
    process_t *proc = (process_t *)kmalloc(sizeof(process_t));
    if (proc == NULL) {
        serial_puts("[PROCESS] Failed to allocate PCB\n");
        return NULL;
    }
    
    /* Initialize PCB */
    process_init_pcb(proc, name, priority);
    
    /* Allocate stack */
    proc->stack_top = stack_alloc(proc->pid);
    if (proc->stack_top == NULL) {
        serial_puts("[PROCESS] Failed to allocate stack\n");
        kfree(proc);
        return NULL;
    }
    
    proc->stack_base = stack_get_base(proc->pid);
    proc->stack_size = STACK_SIZE;
    
    /* ========== Initialize CPU context for REAL hardware context switch ========== */
    /* Set up initial register values that will be loaded on first context switch */
    
    /* Instruction pointer - where process will start executing */
    proc->context.eip = (uint32_t)entry_point;
    
    /* Stack pointers - process's own stack */
    proc->context.esp = (uint32_t)proc->stack_top;
    proc->context.ebp = (uint32_t)proc->stack_top;
    
    /* General purpose registers - initialize to distinguishable values */
    /* Using PID-based values so each process has unique register values */
    proc->context.eax = 0xAAAA0000 | proc->pid;  /* EAX = 0xAAAA00XX where XX is PID */
    proc->context.ebx = 0xBBBB0000 | proc->pid;  /* EBX = 0xBBBB00XX */
    proc->context.ecx = 0xCCCC0000 | proc->pid;  /* ECX = 0xCCCC00XX */
    proc->context.edx = 0xDDDD0000 | proc->pid;  /* EDX = 0xDDDD00XX */
    proc->context.esi = 0x5151E000 | proc->pid;  /* ESI = 0x5151E0XX */
    proc->context.edi = 0xD1D10000 | proc->pid;  /* EDI = 0xD1D100XX */
    
    /* Flags register - interrupts enabled, reserved bit set */
    proc->context.eflags = 0x202;  /* IF=1 (interrupts enabled), reserved bit 1 = 1 */
    
    /* Segment registers - use kernel segments (flat model) */
    proc->context.cs = 0x08;   /* Kernel code segment */
    proc->context.ds = 0x10;   /* Kernel data segment */
    proc->context.es = 0x10;
    proc->context.fs = 0x10;
    proc->context.gs = 0x10;
    proc->context.ss = 0x10;   /* Kernel stack segment */
    
    /* Add to process table */
    process_add_to_table(proc);
    
    /* Add to ready queue */
    process_add_to_ready_queue(proc);
    
    total_processes_created++;
    
    serial_puts("[PROCESS] Created process '");
    serial_puts(proc->name);
    serial_puts("' (PID ");
    serial_put_dec(proc->pid);
    serial_puts(", Priority ");
    serial_put_dec(proc->priority);
    serial_puts(")\n");
    
    return proc;
}

/*
 * Create a new process with required time
 */
process_t *process_create_with_time(const char *name, process_func_t entry_point, 
                                     process_priority_t priority, uint32_t required_time) {
    process_t *proc = process_create(name, entry_point, priority);
    
    if (proc != NULL) {
        proc->required_time = required_time;
        serial_puts("[PROCESS] Set required time: ");
        serial_put_dec(required_time);
        serial_puts(" ticks\n");
    }
    
    return proc;
}

/*
 * Terminate a process by PID
 */
void process_terminate(uint32_t pid) {
    process_t *proc = process_get_by_pid(pid);
    
    if (proc == NULL) {
        serial_puts("[PROCESS] Cannot terminate: PID ");
        serial_put_dec(pid);
        serial_puts(" not found\n");
        return;
    }
    
    serial_puts("[PROCESS] Terminating process '");
    serial_puts(proc->name);
    serial_puts("' (PID ");
    serial_put_dec(pid);
    serial_puts(")\n");
    
    /* Remove from ready queue if in READY state */
    if (proc->state == PROC_STATE_READY) {
        process_remove_from_ready_queue(proc);
    }
    
    /* Clear current process if this is it */
    if (current_process == proc) {
        current_process = NULL;
    }
    
    /* Set state to terminated */
    proc->state = PROC_STATE_TERMINATED;
    
    /* Free stack */
    stack_free(proc->pid);
    
    /* Remove from process table */
    process_remove_from_table(proc->pid);
    
    /* Free PCB */
    kfree(proc);
    
    /* If terminating current process, clear pointer */
    if (current_process == proc) {
        current_process = NULL;
    }
}

/*
 * Current process exits voluntarily
 */
void process_exit(int exit_code) {
    if (current_process == NULL) {
        serial_puts("[PROCESS] Warning: No current process to exit\n");
        return;
    }
    
    current_process->exit_code = exit_code;
    serial_puts("[PROCESS] Process '");
    serial_puts(current_process->name);
    serial_puts("' exiting with code ");
    serial_put_dec(exit_code);
    serial_puts("\n");
    
    process_terminate(current_process->pid);
}

/*
 * Set process state
 */
void process_set_state(uint32_t pid, process_state_t new_state) {
    process_t *proc = process_get_by_pid(pid);
    
    if (proc == NULL) {
        return;
    }
    
    process_state_t old_state = proc->state;
    proc->state = new_state;
    
    /* Handle queue transitions */
    if (old_state == PROC_STATE_READY && new_state != PROC_STATE_READY) {
        process_remove_from_ready_queue(proc);
    } else if (old_state != PROC_STATE_READY && new_state == PROC_STATE_READY) {
        process_add_to_ready_queue(proc);
    }
    
    /* Update current process pointer */
    if (new_state == PROC_STATE_CURRENT) {
        current_process = proc;
    } else if (proc == current_process) {
        current_process = NULL;
    }
}

/*
 * Get process state
 */
process_state_t process_get_state(uint32_t pid) {
    process_t *proc = process_get_by_pid(pid);
    return (proc != NULL) ? proc->state : PROC_STATE_TERMINATED;
}

/*
 * Block a process
 */
void process_block(uint32_t pid) {
    process_set_state(pid, PROC_STATE_BLOCKED);
}

/*
 * Unblock a process
 */
void process_unblock(uint32_t pid) {
    process_set_state(pid, PROC_STATE_READY);
}

/*
 * Put process to sleep (simplified - just blocks it)
 */
void process_sleep(uint32_t pid, uint32_t ticks) {
    process_set_state(pid, PROC_STATE_SLEEPING);
    /* In a real implementation, would set a wakeup time */
}

/*
 * Get process by PID
 */
process_t *process_get_by_pid(uint32_t pid) {
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] != NULL && process_table[i]->pid == pid) {
            return process_table[i];
        }
    }
    return NULL;
}

/*
 * Get current running process
 */
process_t *process_get_current(void) {
    return current_process;
}

/*
 * Get current process PID
 */
uint32_t process_get_current_pid(void) {
    return (current_process != NULL) ? current_process->pid : 0;
}

/*
 * Get process name
 */
const char *process_get_name(uint32_t pid) {
    process_t *proc = process_get_by_pid(pid);
    return (proc != NULL) ? proc->name : "Unknown";
}

/*
 * Get process priority
 */
process_priority_t process_get_priority(uint32_t pid) {
    process_t *proc = process_get_by_pid(pid);
    return (proc != NULL) ? proc->priority : PROC_PRIORITY_NORMAL;
}

/*
 * Set process priority
 */
void process_set_priority(uint32_t pid, process_priority_t priority) {
    process_t *proc = process_get_by_pid(pid);
    
    if (proc == NULL) {
        return;
    }
    
    proc->priority = priority;
    
    /* If in ready queue, re-insert to maintain priority order */
    if (proc->state == PROC_STATE_READY) {
        process_remove_from_ready_queue(proc);
        process_add_to_ready_queue(proc);
    }
}

/*
 * Boost process priority (for aging)
 */
void process_boost_priority(uint32_t pid) {
    process_t *proc = process_get_by_pid(pid);
    
    if (proc == NULL) {
        return;
    }
    
    if (proc->priority < PROC_PRIORITY_CRITICAL) {
        proc->priority++;
        
        /* Re-insert in ready queue if applicable */
        if (proc->state == PROC_STATE_READY) {
            process_remove_from_ready_queue(proc);
            process_add_to_ready_queue(proc);
        }
    }
}

/*
 * Reset age counter
 */
void process_reset_age(uint32_t pid) {
    process_t *proc = process_get_by_pid(pid);
    
    if (proc != NULL) {
        proc->age = 0;
    }
}

/*
 * Get process statistics
 */
void process_get_stats(process_stats_t *stats) {
    if (stats == NULL) {
        return;
    }
    
    stats->total_processes = total_processes_created;
    stats->active_processes = 0;
    stats->ready_processes = 0;
    stats->blocked_processes = 0;
    stats->terminated_processes = 0;
    
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] != NULL) {
            stats->active_processes++;
            
            switch (process_table[i]->state) {
                case PROC_STATE_READY:
                    stats->ready_processes++;
                    break;
                case PROC_STATE_BLOCKED:
                case PROC_STATE_WAITING:
                case PROC_STATE_SLEEPING:
                    stats->blocked_processes++;
                    break;
                case PROC_STATE_TERMINATED:
                    stats->terminated_processes++;
                    break;
                default:
                    break;
            }
        }
    }
}

/*
 * Print process table
 */
void process_print_table(void) {
    serial_puts("\n=== Process Table ===\n");
    serial_puts("PID  Name          State    Pri  CPU  Req  Progress\n");
    serial_puts("---  ------------  -------  ---  ---  ---  --------\n");
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] != NULL) {
            process_t *p = process_table[i];
            
            /* PID */
            if (p->pid < 10) serial_puts(" ");
            serial_put_dec(p->pid);
            serial_puts("   ");
            
            /* Name (12 chars) */
            serial_puts(p->name);
            for (uint32_t j = strlen(p->name); j < 14; j++) {
                serial_puts(" ");
            }
            
            /* State (7 chars) */
            const char *state_str = process_state_to_string(p->state);
            serial_puts(state_str);
            for (uint32_t j = strlen(state_str); j < 9; j++) {
                serial_puts(" ");
            }
            
            /* Priority */
            serial_put_dec(p->priority);
            serial_puts("    ");
            
            /* CPU Time */
            if (p->cpu_time < 100) serial_puts(" ");
            if (p->cpu_time < 10) serial_puts(" ");
            serial_put_dec(p->cpu_time);
            serial_puts("  ");
            
            /* Required Time */
            if (p->required_time > 0) {
                if (p->required_time < 100) serial_puts(" ");
                if (p->required_time < 10) serial_puts(" ");
                serial_put_dec(p->required_time);
                serial_puts("  ");
                
                /* Progress */
                if (p->cpu_time >= p->required_time) {
                    serial_puts("DONE");
                } else {
                    uint32_t percent = (p->cpu_time * 100) / p->required_time;
                    if (percent < 100) serial_puts(" ");
                    if (percent < 10) serial_puts(" ");
                    serial_put_dec(percent);
                    serial_puts("%");
                }
            } else {
                serial_puts("  -   -");
            }
            
            serial_puts("\n");
            
            count++;
        }
    }
    
    serial_puts("---\n");
    serial_puts("Total: ");
    serial_put_dec(count);
    serial_puts(" active processes\n");
    serial_puts("====================\n\n");
}

/*
 * Print detailed process info
 */
void process_print_info(uint32_t pid) {
    process_t *proc = process_get_by_pid(pid);
    
    if (proc == NULL) {
        serial_puts("Process not found\n");
        return;
    }
    
    serial_puts("\n=== Process Information ===\n");
    serial_puts("PID:          "); serial_put_dec(proc->pid); serial_puts("\n");
    serial_puts("Name:         "); serial_puts(proc->name); serial_puts("\n");
    serial_puts("State:        "); serial_puts(process_state_to_string(proc->state)); serial_puts("\n");
    serial_puts("Priority:     "); serial_puts(process_priority_to_string(proc->priority)); serial_puts("\n");
    serial_puts("Parent PID:   "); serial_put_dec(proc->parent_pid); serial_puts("\n");
    serial_puts("Stack Base:   0x"); serial_put_hex((uint32_t)proc->stack_base); serial_puts("\n");
    serial_puts("Stack Top:    0x"); serial_put_hex((uint32_t)proc->stack_top); serial_puts("\n");
    serial_puts("Stack Size:   "); serial_put_dec(proc->stack_size); serial_puts(" bytes\n");
    serial_puts("CPU Time:     "); serial_put_dec(proc->cpu_time); serial_puts("\n");
    serial_puts("Wait Time:    "); serial_put_dec(proc->wait_time); serial_puts("\n");
    serial_puts("Age:          "); serial_put_dec(proc->age); serial_puts("\n");
    serial_puts("Messages:     "); serial_put_dec(proc->msg_count); serial_puts("\n");
    serial_puts("==========================\n\n");
}

/*
 * Count total processes
 */
uint32_t process_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] != NULL) {
            count++;
        }
    }
    return count;
}

/*
 * Count processes by state
 */
uint32_t process_count_by_state(process_state_t state) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] != NULL && process_table[i]->state == state) {
            count++;
        }
    }
    return count;
}

/*
 * Send a message to another process (IPC)
 */
int process_send_message(uint32_t dest_pid, uint32_t message) {
    process_t *dest = process_get_by_pid(dest_pid);
    
    if (dest == NULL) {
        serial_puts("[IPC] Destination process not found\n");
        return -1;
    }
    
    if (dest->msg_count >= 16) {
        serial_puts("[IPC] Message queue full\n");
        return -1;
    }
    
    dest->message_queue[dest->msg_count++] = message;
    
    /* If destination was waiting for a message, unblock it */
    if (dest->waiting_for_msg) {
        dest->waiting_for_msg = 0;
        process_unblock(dest_pid);
    }
    
    return 0;
}

/*
 * Receive a message (blocking)
 */
int process_receive_message(uint32_t *message) {
    if (current_process == NULL) {
        return -1;
    }
    
    if (current_process->msg_count == 0) {
        /* No messages, block the process */
        current_process->waiting_for_msg = 1;
        process_block(current_process->pid);
        return -1;  /* Would need scheduler to resume later */
    }
    
    /* Get first message */
    *message = current_process->message_queue[0];
    
    /* Shift remaining messages */
    for (uint32_t i = 0; i < current_process->msg_count - 1; i++) {
        current_process->message_queue[i] = current_process->message_queue[i + 1];
    }
    
    current_process->msg_count--;
    
    return 0;
}

/*
 * Check if process has messages
 */
int process_has_message(uint32_t pid) {
    process_t *proc = process_get_by_pid(pid);
    return (proc != NULL) ? (proc->msg_count > 0) : 0;
}

/*
 * Get ready queue head
 */
process_t *process_get_ready_queue(void) {
    return ready_queue_head;
}

/*
 * Dequeue next ready process
 */
process_t *process_dequeue_ready(void) {
    if (ready_queue_head == NULL) {
        return NULL;
    }
    
    process_t *proc = ready_queue_head;
    process_remove_from_ready_queue(proc);
    
    /* Mark as dequeued (not in READY state anymore) to prevent double-removal */
    /* This prevents process_set_state from trying to remove again */
    proc->state = PROC_STATE_WAITING;  /* Transitional state: dequeued but not yet CURRENT */
    
    return proc;
}

/*
 * Enqueue process to ready queue
 */
void process_enqueue_ready(process_t *proc) {
    if (proc == NULL) {
        return;
    }
    process_add_to_ready_queue(proc);
}

/*
 * Convert process state to string
 */
const char *process_state_to_string(process_state_t state) {
    switch (state) {
        case PROC_STATE_READY:      return "READY";
        case PROC_STATE_CURRENT:    return "CURRENT";
        case PROC_STATE_TERMINATED: return "TERMINATED";
        case PROC_STATE_BLOCKED:    return "BLOCKED";
        case PROC_STATE_WAITING:    return "WAITING";
        case PROC_STATE_SLEEPING:   return "SLEEPING";
        default:                    return "UNKNOWN";
    }
}

/*
 * Convert priority to string
 */
const char *process_priority_to_string(process_priority_t priority) {
    switch (priority) {
        case PROC_PRIORITY_LOW:      return "LOW";
        case PROC_PRIORITY_NORMAL:   return "NORMAL";
        case PROC_PRIORITY_HIGH:     return "HIGH";
        case PROC_PRIORITY_CRITICAL: return "CRITICAL";
        default:                     return "UNKNOWN";
    }
}
