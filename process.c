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
    proc->wait_time = 0;
    proc->creation_time = system_ticks;
    
    /* Scheduler simulation fields */
    proc->required_time = 0;
    proc->remaining_time = 0;
    proc->remaining_slice = 0;
    
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
    
    /* Insert based on priority (higher priority first) */
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
    
    /* Set up initial stack frame for process entry */
    /* In a real OS, we'd set up the stack with entry_point address */
    /* For now, just initialize the context */
    proc->context.eip = (uint32_t)entry_point;
    proc->context.esp = (uint32_t)proc->stack_top;
    proc->context.ebp = (uint32_t)proc->stack_top;
    proc->context.eflags = 0x202;  /* Interrupts enabled */
    
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
    
    /* Set state to terminated */
    proc->state = PROC_STATE_TERMINATED;
    
    /* Remove from ready queue if present */
    if (proc->state == PROC_STATE_READY) {
        process_remove_from_ready_queue(proc);
    }
    
    serial_puts("[PROCESS] Terminating process '");
    serial_puts(proc->name);
    serial_puts("' (PID ");
    serial_put_dec(pid);
    serial_puts(")\n");
    
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
    serial_puts("PID  Name                State      Priority   CPU Time\n");
    serial_puts("---  ------------------  ---------  ---------  --------\n");
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] != NULL) {
            process_t *p = process_table[i];
            
            /* PID */
            if (p->pid < 10) serial_puts(" ");
            serial_put_dec(p->pid);
            serial_puts("   ");
            
            /* Name */
            serial_puts(p->name);
            for (uint32_t j = strlen(p->name); j < 20; j++) {
                serial_puts(" ");
            }
            
            /* State */
            const char *state_str = process_state_to_string(p->state);
            serial_puts(state_str);
            for (uint32_t j = strlen(state_str); j < 11; j++) {
                serial_puts(" ");
            }
            
            /* Priority */
            const char *prio_str = process_priority_to_string(p->priority);
            serial_puts(prio_str);
            for (uint32_t j = strlen(prio_str); j < 11; j++) {
                serial_puts(" ");
            }
            
            /* CPU Time */
            serial_put_dec(p->cpu_time);
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
    return proc;
}

/*
 * Enqueue process to ready queue
 */
void process_enqueue_ready(process_t *proc) {
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

/* ============================================================
 * CPU SCHEDULER SIMULATION
 * ============================================================ */

/* Scheduler state */
static uint32_t scheduler_enabled = 0;
static uint32_t last_context_switch_tick = 0;

/*
 * Initialize the scheduler
 */
void scheduler_init(void) {
    scheduler_enabled = 1;
    last_context_switch_tick = 0;
    
    serial_puts("[SCHEDULER] Priority-based preemptive scheduler initialized\n");
    serial_puts("[SCHEDULER] Time quantums: CRITICAL=");
    serial_put_dec(QUANTUM_CRITICAL);
    serial_puts(", HIGH=");
    serial_put_dec(QUANTUM_HIGH);
    serial_puts(", NORMAL=");
    serial_put_dec(QUANTUM_NORMAL);
    serial_puts(", LOW=");
    serial_put_dec(QUANTUM_LOW);
    serial_puts("\n");
    serial_puts("[SCHEDULER] Aging threshold: ");
    serial_put_dec(AGING_THRESHOLD);
    serial_puts(" ticks\n");
}

/*
 * Get time quantum for a priority level
 */
static uint32_t get_time_quantum(process_priority_t priority) {
    switch (priority) {
        case PROC_PRIORITY_CRITICAL: return QUANTUM_CRITICAL;
        case PROC_PRIORITY_HIGH:     return QUANTUM_HIGH;
        case PROC_PRIORITY_NORMAL:   return QUANTUM_NORMAL;
        case PROC_PRIORITY_LOW:      return QUANTUM_LOW;
        default:                     return QUANTUM_NORMAL;
    }
}

/*
 * Create a process with execution time requirement
 */
process_t *process_create_with_time(const char *name, process_priority_t priority, uint32_t required_time) {
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
    
    /* Set up scheduler-specific fields */
    proc->required_time = required_time;
    proc->remaining_time = required_time;
    proc->time_quantum = get_time_quantum(priority);
    proc->remaining_slice = proc->time_quantum;
    
    /* Set up initial context */
    proc->context.eip = 0;
    proc->context.esp = (uint32_t)proc->stack_top;
    proc->context.ebp = (uint32_t)proc->stack_top;
    proc->context.eflags = 0x202;
    
    /* Add to process table */
    process_add_to_table(proc);
    
    /* Add to ready queue */
    process_add_to_ready_queue(proc);
    
    total_processes_created++;
    
    serial_puts("[SCHEDULER] Created process '");
    serial_puts(proc->name);
    serial_puts("' (PID ");
    serial_put_dec(proc->pid);
    serial_puts(", Priority: ");
    serial_puts(process_priority_to_string(proc->priority));
    serial_puts(", Required Time: ");
    serial_put_dec(required_time);
    serial_puts(")\n");
    
    /* Trigger scheduling decision */
    if (scheduler_enabled) {
        scheduler_schedule();
    }
    
    return proc;
}

/*
 * Context switch to a new process
 */
void scheduler_context_switch(process_t *new_process) {
    process_t *old_process = current_process;
    
    /* Log context switch */
    serial_puts("[SCHEDULER] Context switch @ tick ");
    serial_put_dec(system_ticks);
    serial_puts(": ");
    
    if (old_process != NULL) {
        serial_puts("'");
        serial_puts(old_process->name);
        serial_puts("' (PID ");
        serial_put_dec(old_process->pid);
        serial_puts(")");
    } else {
        serial_puts("IDLE");
    }
    
    serial_puts(" -> ");
    
    if (new_process != NULL) {
        serial_puts("'");
        serial_puts(new_process->name);
        serial_puts("' (PID ");
        serial_put_dec(new_process->pid);
        serial_puts(")\n");
    } else {
        serial_puts("IDLE\n");
    }
    
    /* Save old process state */
    if (old_process != NULL && old_process->state == PROC_STATE_CURRENT) {
        old_process->state = PROC_STATE_READY;
        process_add_to_ready_queue(old_process);
    }
    
    /* Restore new process state */
    if (new_process != NULL) {
        process_remove_from_ready_queue(new_process);
        new_process->state = PROC_STATE_CURRENT;
        new_process->remaining_slice = get_time_quantum(new_process->priority);
        process_reset_age(new_process->pid);
        current_process = new_process;
    } else {
        current_process = NULL;
    }
    
    last_context_switch_tick = system_ticks;
}

/*
 * Scheduler decision - select next process to run
 */
void scheduler_schedule(void) {
    if (!scheduler_enabled) {
        return;
    }
    
    /* Get highest priority ready process */
    process_t *next_process = ready_queue_head;
    
    /* Check if we need to switch */
    int should_switch = 0;
    
    if (current_process == NULL && next_process != NULL) {
        /* No current process, switch to ready process */
        should_switch = 1;
    } else if (current_process != NULL && next_process != NULL) {
        /* Compare priorities */
        if (next_process->priority > current_process->priority) {
            /* Higher priority process is ready */
            should_switch = 1;
        } else if (current_process->remaining_slice == 0) {
            /* Current process exhausted its quantum */
            should_switch = 1;
        } else if (current_process->remaining_time == 0) {
            /* Current process finished */
            should_switch = 1;
        }
    } else if (current_process != NULL && current_process->remaining_time == 0) {
        /* Current process finished, no ready processes */
        should_switch = 1;
        next_process = NULL;
    }
    
    if (should_switch) {
        scheduler_context_switch(next_process);
    }
}

/*
 * Age processes in ready queue (for starvation prevention)
 */
void scheduler_age_processes(void) {
    process_t *proc = ready_queue_head;
    
    while (proc != NULL) {
        proc->age++;
        proc->wait_time++;
        
        /* Check if process should be boosted */
        if (proc->age >= AGING_THRESHOLD && proc->priority < PROC_PRIORITY_CRITICAL) {
            serial_puts("[SCHEDULER] Aging: Boosting '");
            serial_puts(proc->name);
            serial_puts("' (PID ");
            serial_put_dec(proc->pid);
            serial_puts(") from ");
            serial_puts(process_priority_to_string(proc->priority));
            
            /* Boost priority */
            process_t *next = proc->next;
            process_remove_from_ready_queue(proc);
            proc->priority++;
            proc->age = 0;
            process_add_to_ready_queue(proc);
            
            serial_puts(" to ");
            serial_puts(process_priority_to_string(proc->priority));
            serial_puts("\n");
            
            proc = next;
        } else {
            proc = proc->next;
        }
    }
}

/*
 * Tick the scheduler - advance time by one unit
 */
void scheduler_tick(void) {
    system_ticks++;
    
    /* Update current process */
    if (current_process != NULL) {
        current_process->cpu_time++;
        
        /* Decrement remaining execution time */
        if (current_process->remaining_time > 0) {
            current_process->remaining_time--;
            
            /* Check if process completed */
            if (current_process->remaining_time == 0) {
                serial_puts("[SCHEDULER] Process '");
                serial_puts(current_process->name);
                serial_puts("' (PID ");
                serial_put_dec(current_process->pid);
                serial_puts(") FINISHED at tick ");
                serial_put_dec(system_ticks);
                serial_puts(" (CPU time: ");
                serial_put_dec(current_process->cpu_time);
                serial_puts(", Wait time: ");
                serial_put_dec(current_process->wait_time);
                serial_puts(")\n");
                
                /* Mark as terminated */
                current_process->state = PROC_STATE_TERMINATED;
                process_t *finished = current_process;
                current_process = NULL;
                
                /* Don't free immediately - keep for display */
                /* Just schedule next process */
                scheduler_schedule();
                return;
            }
        }
        
        /* Decrement remaining time slice */
        if (current_process->remaining_slice > 0) {
            current_process->remaining_slice--;
            
            /* Check if quantum expired */
            if (current_process->remaining_slice == 0) {
                /* Time quantum exhausted, trigger reschedule */
                scheduler_schedule();
                return;
            }
        }
    }
    
    /* Age processes in ready queue */
    scheduler_age_processes();
    
    /* Check for preemption by higher priority process */
    if (ready_queue_head != NULL && current_process != NULL) {
        if (ready_queue_head->priority > current_process->priority) {
            scheduler_schedule();
        }
    } else if (ready_queue_head != NULL && current_process == NULL) {
        /* No running process but ready processes exist */
        scheduler_schedule();
    }
}

/*
 * Get current system ticks
 */
uint32_t scheduler_get_ticks(void) {
    return system_ticks;
}

/*
 * Print scheduler state and process table
 */
void scheduler_print_table(void) {
    serial_puts("\n");
    serial_puts("========================================\n");
    serial_puts("    CPU SCHEDULER STATUS\n");
    serial_puts("========================================\n");
    serial_puts("System Ticks:     ");
    serial_put_dec(system_ticks);
    serial_puts("\n");
    serial_puts("Scheduler:        ");
    serial_puts(scheduler_enabled ? "ENABLED" : "DISABLED");
    serial_puts("\n");
    serial_puts("Current Process:  ");
    if (current_process != NULL) {
        serial_puts("'");
        serial_puts(current_process->name);
        serial_puts("' (PID ");
        serial_put_dec(current_process->pid);
        serial_puts(")\n");
    } else {
        serial_puts("IDLE\n");
    }
    serial_puts("========================================\n\n");
    
    serial_puts("=== Process Table ===\n");
    serial_puts("PID  Name          Priority   State      Remain  Wait   Slice\n");
    serial_puts("---  ------------  ---------  ---------  ------  -----  -----\n");
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] != NULL) {
            process_t *p = process_table[i];
            
            /* PID */
            if (p->pid < 10) serial_puts(" ");
            serial_put_dec(p->pid);
            serial_puts("   ");
            
            /* Name */
            serial_puts(p->name);
            for (uint32_t j = strlen(p->name); j < 14; j++) {
                serial_puts(" ");
            }
            
            /* Priority */
            const char *prio_str = process_priority_to_string(p->priority);
            serial_puts(prio_str);
            for (uint32_t j = strlen(prio_str); j < 11; j++) {
                serial_puts(" ");
            }
            
            /* State */
            const char *state_str;
            if (p->state == PROC_STATE_CURRENT) {
                state_str = "RUNNING";
            } else if (p->state == PROC_STATE_READY) {
                state_str = "READY";
            } else if (p->state == PROC_STATE_TERMINATED) {
                state_str = "FINISHED";
            } else {
                state_str = process_state_to_string(p->state);
            }
            serial_puts(state_str);
            for (uint32_t j = strlen(state_str); j < 11; j++) {
                serial_puts(" ");
            }
            
            /* Remaining time */
            if (p->remaining_time < 10) serial_puts("  ");
            else if (p->remaining_time < 100) serial_puts(" ");
            serial_put_dec(p->remaining_time);
            serial_puts("    ");
            
            /* Wait time */
            if (p->wait_time < 10) serial_puts("  ");
            else if (p->wait_time < 100) serial_puts(" ");
            serial_put_dec(p->wait_time);
            serial_puts("   ");
            
            /* Remaining slice */
            if (p->state == PROC_STATE_CURRENT) {
                if (p->remaining_slice < 10) serial_puts("  ");
                else if (p->remaining_slice < 100) serial_puts(" ");
                serial_put_dec(p->remaining_slice);
            } else {
                serial_puts("  -");
            }
            
            serial_puts("\n");
            count++;
        }
    }
    
    serial_puts("---\n");
    serial_puts("Total: ");
    serial_put_dec(count);
    serial_puts(" processes\n");
    serial_puts("====================\n\n");
}

