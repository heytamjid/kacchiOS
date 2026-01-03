/* scheduler.c - Process Scheduler Implementation */
#include "scheduler.h"
#include "process.h"
#include "serial.h"
#include "string.h"

/* External assembly functions for REAL hardware context switching */
extern void asm_save_context(cpu_context_t *ctx);
extern void asm_restore_context(cpu_context_t *ctx);
extern uint32_t asm_get_eflags(void);
extern uint32_t asm_get_eip(void);

/* Scheduler state */
static sched_config_t sched_config;
static sched_stats_t sched_stats;
static uint8_t scheduler_running = 0;
static uint32_t current_tick = 0;
static uint32_t time_slice_remaining = 0;

/* Forward declarations */
static process_t *select_round_robin(void);
static process_t *select_priority(void);
static process_t *select_priority_rr(void);
static process_t *select_fcfs(void);
static void save_context(process_t *proc);
static void restore_context(process_t *proc);

/*
 * Initialize the scheduler
 */
void scheduler_init(sched_policy_t policy, uint32_t default_quantum) {
    /* Initialize configuration */
    sched_config.policy = policy;
    sched_config.default_quantum = default_quantum;
    sched_config.min_quantum = 10;
    sched_config.max_quantum = 1000;
    sched_config.aging_threshold = 100;
    sched_config.aging_boost_interval = 50;
    sched_config.enable_aging = 1;
    sched_config.enable_preemption = 1;
    
    /* Initialize statistics */
    memset(&sched_stats, 0, sizeof(sched_stats_t));
    
    scheduler_running = 0;
    current_tick = 0;
    time_slice_remaining = default_quantum;
    
    serial_puts("[SCHEDULER] Scheduler initialized\n");
    serial_puts("[SCHEDULER] Policy: ");
    serial_puts(scheduler_policy_to_string(policy));
    serial_puts("\n[SCHEDULER] Time quantum: ");
    serial_put_dec(default_quantum);
    serial_puts(" ticks\n");
}

/*
 * Start the scheduler
 */
void scheduler_start(void) {
    scheduler_running = 1;
    serial_puts("[SCHEDULER] Scheduler started\n");
    
    /* Immediately schedule first process */
    scheduler_schedule();
}

/*
 * Stop the scheduler
 */
void scheduler_stop(void) {
    scheduler_running = 0;
    serial_puts("[SCHEDULER] Scheduler stopped\n");
}

/*
 * Timer tick handler - called on each timer interrupt
 */
void scheduler_tick(void) {
    if (!scheduler_running) {
        return;
    }
    
    current_tick++;
    sched_stats.total_ticks++;
    
    process_t *current = process_get_current();
    
    if (current == NULL) {
        sched_stats.idle_ticks++;
        /* No process running, try to schedule */
        scheduler_schedule();
        return;
    }
    
    /* Update current process CPU time */
    current->cpu_time++;
    
    /* Check if process has completed its required time */
    if (current->required_time > 0 && current->cpu_time >= current->required_time) {
        serial_puts("[SCHEDULER] Process ");
        serial_put_dec(current->pid);
        serial_puts(" (");
        serial_puts(current->name);
        serial_puts(") completed after ");
        serial_put_dec(current->cpu_time);
        serial_puts(" ticks\n");
        
        process_terminate(current->pid);
        scheduler_schedule();  /* Schedule next process */
        return;
    }
    
    /* Decrease time slice */
    if (time_slice_remaining > 0) {
        time_slice_remaining--;
    }
    
    /* Check if time slice expired and preemption is enabled */
    if (sched_config.enable_preemption && time_slice_remaining == 0) {
        serial_puts("[SCHEDULER] Time quantum expired for PID ");
        serial_put_dec(current->pid);
        serial_puts("\n");
        
        sched_stats.preemptions++;
        scheduler_schedule();  /* Preempt current process */
        return;
    }
    
    /* Periodically check for aging */
    if (sched_config.enable_aging && 
        (current_tick % sched_config.aging_boost_interval) == 0) {
        scheduler_check_aging();
    }
}

/*
 * Main scheduling function - makes scheduling decision with REAL context switch
 */
void scheduler_schedule(void) {
    if (!scheduler_running) {
        return;
    }
    
    process_t *current = process_get_current();
    process_t *prev = current;  /* Save reference for context switch */
    
    /* If there's a current process, save it back to READY state first */
    if (current != NULL && current->state == PROC_STATE_CURRENT) {
        serial_puts("[SCHEDULER DEBUG] Saving current process: ");
        serial_puts(current->name);
        serial_puts(" -> READY\n");
        process_set_state(current->pid, PROC_STATE_READY);
    }
    
    /* Now select next process from ready queue */
    process_t *next = scheduler_select_next_process();
    
    serial_puts("[SCHEDULER DEBUG] Selected next=");
    if (next) {
        serial_puts(next->name);
    } else {
        serial_puts("NULL");
    }
    serial_puts("\n");
    
    /* No process to run */
    if (next == NULL) {
        serial_puts("[SCHEDULER] No process to schedule\n");
        return;
    }
    
    /* Load the next process and set it to CURRENT */
    serial_puts("[SCHEDULER] Switching to: ");
    serial_puts(next->name);
    serial_puts(" (PID ");
    serial_put_dec(next->pid);
    serial_puts(")\n");
    
    process_set_state(next->pid, PROC_STATE_CURRENT);
    time_slice_remaining = next->time_quantum;
    sched_stats.total_context_switches++;
    
    /* ========== REAL HARDWARE CONTEXT SWITCH ========== */
    /* Only perform context switch if switching to a different process */
    if (prev != next) {
        scheduler_switch_context(prev, next);
    }
}

/*
 * Select next process based on scheduling policy
 */
process_t *scheduler_select_next_process(void) {
    switch (sched_config.policy) {
        case SCHED_POLICY_ROUND_ROBIN:
            return select_round_robin();
        
        case SCHED_POLICY_PRIORITY:
            return select_priority();
        
        case SCHED_POLICY_PRIORITY_RR:
            return select_priority_rr();
        
        case SCHED_POLICY_FCFS:
            return select_fcfs();
        
        default:
            return select_round_robin();
    }
}

/*
 * Round-robin selection
 */
static process_t *select_round_robin(void) {
    /* Simply get next process from ready queue */
    return process_dequeue_ready();
}

/*
 * Priority-based selection (already handled by ready queue)
 */
static process_t *select_priority(void) {
    /* Ready queue is priority-ordered, just dequeue */
    return process_dequeue_ready();
}

/*
 * Priority with round-robin per level
 */
static process_t *select_priority_rr(void) {
    /* Get highest priority process */
    process_t *next = process_dequeue_ready();
    
    /* Round-robin within same priority level would require
     * tracking position within each priority level - simplified here */
    return next;
}

/*
 * First-Come-First-Served selection
 */
static process_t *select_fcfs(void) {
    /* Get first process from ready queue */
    return process_dequeue_ready();
}

/*
 * Context switch (REAL hardware context switch using assembly)
 */
void scheduler_switch_context(process_t *from, process_t *to) {
    serial_puts("[CONTEXT SWITCH] ===== REAL HARDWARE CONTEXT SWITCH =====\n");
    
    /* Save context of outgoing process */
    if (from != NULL) {
        save_context(from);
    }
    
    /* Load context of incoming process */
    if (to != NULL) {
        restore_context(to);
    }
    
    serial_puts("[CONTEXT SWITCH] ===== CONTEXT SWITCH COMPLETE =====\n");
}

/*
 * Save CPU context - REAL hardware register save using x86 assembly
 */
static void save_context(process_t *proc) {
    if (proc == NULL) {
        return;
    }
    
    serial_puts("[CONTEXT SAVE] Saving REAL CPU registers for PID ");
    serial_put_dec(proc->pid);
    serial_puts(" (");
    serial_puts(proc->name);
    serial_puts(")\n");
    
    /* Call assembly routine to save ALL CPU registers */
    asm_save_context(&proc->context);
    
    /* Log the saved register values */
    serial_puts("  [HW] EAX=0x");
    serial_put_hex(proc->context.eax);
    serial_puts(" EBX=0x");
    serial_put_hex(proc->context.ebx);
    serial_puts(" ECX=0x");
    serial_put_hex(proc->context.ecx);
    serial_puts(" EDX=0x");
    serial_put_hex(proc->context.edx);
    serial_puts("\n");
    
    serial_puts("  [HW] ESI=0x");
    serial_put_hex(proc->context.esi);
    serial_puts(" EDI=0x");
    serial_put_hex(proc->context.edi);
    serial_puts(" EBP=0x");
    serial_put_hex(proc->context.ebp);
    serial_puts(" ESP=0x");
    serial_put_hex(proc->context.esp);
    serial_puts("\n");
    
    serial_puts("  [HW] EIP=0x");
    serial_put_hex(proc->context.eip);
    serial_puts(" EFLAGS=0x");
    serial_put_hex(proc->context.eflags);
    serial_puts("\n");
    
    serial_puts("  [HW] CS=0x");
    serial_put_hex(proc->context.cs);
    serial_puts(" DS=0x");
    serial_put_hex(proc->context.ds);
    serial_puts(" SS=0x");
    serial_put_hex(proc->context.ss);
    serial_puts("\n");
}

/*
 * Restore CPU context - REAL hardware register restore using x86 assembly
 */
static void restore_context(process_t *proc) {
    if (proc == NULL) {
        return;
    }
    
    serial_puts("[CONTEXT RESTORE] Restoring REAL CPU registers for PID ");
    serial_put_dec(proc->pid);
    serial_puts(" (");
    serial_puts(proc->name);
    serial_puts(")\n");
    
    /* Log the register values being restored */
    serial_puts("  [HW] EAX=0x");
    serial_put_hex(proc->context.eax);
    serial_puts(" EBX=0x");
    serial_put_hex(proc->context.ebx);
    serial_puts(" ECX=0x");
    serial_put_hex(proc->context.ecx);
    serial_puts(" EDX=0x");
    serial_put_hex(proc->context.edx);
    serial_puts("\n");
    
    serial_puts("  [HW] ESI=0x");
    serial_put_hex(proc->context.esi);
    serial_puts(" EDI=0x");
    serial_put_hex(proc->context.edi);
    serial_puts(" EBP=0x");
    serial_put_hex(proc->context.ebp);
    serial_puts(" ESP=0x");
    serial_put_hex(proc->context.esp);
    serial_puts("\n");
    
    serial_puts("  [HW] EIP=0x");
    serial_put_hex(proc->context.eip);
    serial_puts(" EFLAGS=0x");
    serial_put_hex(proc->context.eflags);
    serial_puts("\n");
    
    /* Call assembly routine to restore ALL CPU registers */
    asm_restore_context(&proc->context);
    
    serial_puts("  [HW] Registers restored from PCB to CPU\n");
}

/*
 * Current process voluntarily yields CPU
 */
void scheduler_yield(void) {
    sched_stats.voluntary_yields++;
    
    serial_puts("[SCHEDULER] Process ");
    process_t *current = process_get_current();
    if (current != NULL) {
        serial_put_dec(current->pid);
    }
    serial_puts(" yielded CPU\n");
    
    scheduler_schedule();
}

/*
 * Check for aging and boost priorities
 */
void scheduler_check_aging(void) {
    if (!sched_config.enable_aging) {
        return;
    }
    
    /* Check all ready processes for aging */
    uint32_t count = process_count();
    
    for (uint32_t pid = 1; pid <= count + 10; pid++) {
        process_t *proc = process_get_by_pid(pid);
        
        if (proc == NULL) {
            continue;
        }
        
        /* Only age processes in ready state */
        if (proc->state != PROC_STATE_READY) {
            continue;
        }
        
        /* Increment age */
        proc->age++;
        
        /* Check if age threshold exceeded */
        if (proc->age >= sched_config.aging_threshold) {
            serial_puts("[SCHEDULER] Aging: Boosting priority of PID ");
            serial_put_dec(proc->pid);
            serial_puts(" (age=");
            serial_put_dec(proc->age);
            serial_puts(")\n");
            
            process_boost_priority(proc->pid);
            process_reset_age(proc->pid);
            sched_stats.total_aging_boosts++;
        }
    }
}

/*
 * Set scheduling policy
 */
void scheduler_set_policy(sched_policy_t policy) {
    sched_config.policy = policy;
    serial_puts("[SCHEDULER] Policy changed to: ");
    serial_puts(scheduler_policy_to_string(policy));
    serial_puts("\n");
}

/*
 * Get scheduling policy
 */
sched_policy_t scheduler_get_policy(void) {
    return sched_config.policy;
}

/*
 * Set default time quantum
 */
void scheduler_set_quantum(uint32_t quantum) {
    if (quantum < sched_config.min_quantum) {
        quantum = sched_config.min_quantum;
    }
    if (quantum > sched_config.max_quantum) {
        quantum = sched_config.max_quantum;
    }
    
    sched_config.default_quantum = quantum;
    serial_puts("[SCHEDULER] Time quantum set to: ");
    serial_put_dec(quantum);
    serial_puts(" ticks\n");
}

/*
 * Get default time quantum
 */
uint32_t scheduler_get_quantum(void) {
    return sched_config.default_quantum;
}

/*
 * Enable or disable aging
 */
void scheduler_enable_aging(uint8_t enable) {
    sched_config.enable_aging = enable;
    serial_puts("[SCHEDULER] Aging ");
    serial_puts(enable ? "enabled" : "disabled");
    serial_puts("\n");
}

/*
 * Set aging threshold
 */
void scheduler_set_aging_threshold(uint32_t threshold) {
    sched_config.aging_threshold = threshold;
}

/*
 * Set aging check interval
 */
void scheduler_set_aging_interval(uint32_t interval) {
    sched_config.aging_boost_interval = interval;
}

/*
 * Enable or disable preemption
 */
void scheduler_enable_preemption(uint8_t enable) {
    sched_config.enable_preemption = enable;
    serial_puts("[SCHEDULER] Preemption ");
    serial_puts(enable ? "enabled" : "disabled");
    serial_puts("\n");
}

/*
 * Check if scheduler is preemptive
 */
uint8_t scheduler_is_preemptive(void) {
    return sched_config.enable_preemption;
}

/*
 * Set time quantum for specific process
 */
void scheduler_set_process_quantum(uint32_t pid, uint32_t quantum) {
    process_t *proc = process_get_by_pid(pid);
    
    if (proc != NULL) {
        if (quantum < sched_config.min_quantum) {
            quantum = sched_config.min_quantum;
        }
        if (quantum > sched_config.max_quantum) {
            quantum = sched_config.max_quantum;
        }
        
        proc->time_quantum = quantum;
    }
}

/*
 * Get time quantum for specific process
 */
uint32_t scheduler_get_process_quantum(uint32_t pid) {
    process_t *proc = process_get_by_pid(pid);
    return (proc != NULL) ? proc->time_quantum : 0;
}

/*
 * Update process time statistics
 */
void scheduler_update_process_time(process_t *proc, uint32_t ticks) {
    if (proc != NULL) {
        proc->cpu_time += ticks;
    }
}

/*
 * Get scheduler statistics
 */
void scheduler_get_stats(sched_stats_t *stats) {
    if (stats != NULL) {
        memcpy(stats, &sched_stats, sizeof(sched_stats_t));
    }
}

/*
 * Print scheduler statistics
 */
void scheduler_print_stats(void) {
    serial_puts("\n=== Scheduler Statistics ===\n");
    serial_puts("Total Ticks:          ");
    serial_put_dec(sched_stats.total_ticks);
    serial_puts("\n");
    
    serial_puts("Idle Ticks:           ");
    serial_put_dec(sched_stats.idle_ticks);
    serial_puts("\n");
    
    serial_puts("Context Switches:     ");
    serial_put_dec(sched_stats.total_context_switches);
    serial_puts("\n");
    
    serial_puts("Preemptions:          ");
    serial_put_dec(sched_stats.preemptions);
    serial_puts("\n");
    
    serial_puts("Voluntary Yields:     ");
    serial_put_dec(sched_stats.voluntary_yields);
    serial_puts("\n");
    
    serial_puts("Aging Boosts:         ");
    serial_put_dec(sched_stats.total_aging_boosts);
    serial_puts("\n");
    
    /* Calculate CPU utilization */
    if (sched_stats.total_ticks > 0) {
        uint32_t busy_ticks = sched_stats.total_ticks - sched_stats.idle_ticks;
        uint32_t utilization = (busy_ticks * 100) / sched_stats.total_ticks;
        
        serial_puts("CPU Utilization:      ");
        serial_put_dec(utilization);
        serial_puts("%\n");
    }
    
    serial_puts("===========================\n\n");
}

/*
 * Reset scheduler statistics
 */
void scheduler_reset_stats(void) {
    memset(&sched_stats, 0, sizeof(sched_stats_t));
    serial_puts("[SCHEDULER] Statistics reset\n");
}

/*
 * Print scheduler configuration
 */
void scheduler_print_config(void) {
    serial_puts("\n=== Scheduler Configuration ===\n");
    serial_puts("Policy:               ");
    serial_puts(scheduler_policy_to_string(sched_config.policy));
    serial_puts("\n");
    
    serial_puts("Default Quantum:      ");
    serial_put_dec(sched_config.default_quantum);
    serial_puts(" ticks\n");
    
    serial_puts("Quantum Range:        ");
    serial_put_dec(sched_config.min_quantum);
    serial_puts(" - ");
    serial_put_dec(sched_config.max_quantum);
    serial_puts(" ticks\n");
    
    serial_puts("Aging:                ");
    serial_puts(sched_config.enable_aging ? "Enabled" : "Disabled");
    serial_puts("\n");
    
    if (sched_config.enable_aging) {
        serial_puts("  Threshold:          ");
        serial_put_dec(sched_config.aging_threshold);
        serial_puts(" ticks\n");
        
        serial_puts("  Check Interval:     ");
        serial_put_dec(sched_config.aging_boost_interval);
        serial_puts(" ticks\n");
    }
    
    serial_puts("Preemption:           ");
    serial_puts(sched_config.enable_preemption ? "Enabled" : "Disabled");
    serial_puts("\n");
    
    serial_puts("Scheduler:            ");
    serial_puts(scheduler_running ? "Running" : "Stopped");
    serial_puts("\n");
    
    serial_puts("==============================\n\n");
}

/*
 * Convert scheduling policy to string
 */
const char *scheduler_policy_to_string(sched_policy_t policy) {
    switch (policy) {
        case SCHED_POLICY_ROUND_ROBIN:
            return "Round-Robin";
        case SCHED_POLICY_PRIORITY:
            return "Priority-Based";
        case SCHED_POLICY_PRIORITY_RR:
            return "Priority Round-Robin";
        case SCHED_POLICY_FCFS:
            return "First-Come-First-Served";
        default:
            return "Unknown";
    }
}
