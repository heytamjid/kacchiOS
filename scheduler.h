/* scheduler.h - Process Scheduler Interface */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "process.h"

/* Scheduling policies */
typedef enum {
    SCHED_POLICY_ROUND_ROBIN = 0,   /* Round-robin scheduling */
    SCHED_POLICY_PRIORITY,          /* Priority-based scheduling */
    SCHED_POLICY_PRIORITY_RR,       /* Priority with round-robin per level */
    SCHED_POLICY_FCFS               /* First-Come-First-Served */
} sched_policy_t;

/* Scheduler configuration */
typedef struct {
    sched_policy_t policy;          /* Current scheduling policy */
    uint32_t default_quantum;       /* Default time quantum (ticks) */
    uint32_t min_quantum;           /* Minimum time quantum */
    uint32_t max_quantum;           /* Maximum time quantum */
    uint32_t aging_threshold;       /* Ticks before aging kicks in */
    uint32_t aging_boost_interval;  /* How often to check for aging */
    uint8_t enable_aging;           /* 1 if aging enabled, 0 otherwise */
    uint8_t enable_preemption;      /* 1 if preemptive, 0 if cooperative */
} sched_config_t;

/* Scheduler statistics */
typedef struct {
    uint32_t total_context_switches;    /* Total context switches */
    uint32_t total_ticks;                /* Total scheduler ticks */
    uint32_t idle_ticks;                 /* Ticks spent idle */
    uint32_t total_aging_boosts;         /* Total priority boosts from aging */
    uint32_t preemptions;                /* Number of preemptions */
    uint32_t voluntary_yields;           /* Number of voluntary yields */
} sched_stats_t;

/* Scheduler Initialization */
void scheduler_init(sched_policy_t policy, uint32_t default_quantum);

/* Scheduling Functions */
void scheduler_start(void);                     /* Start the scheduler */
void scheduler_stop(void);                      /* Stop the scheduler */
void scheduler_tick(void);                      /* Called on timer tick */
void scheduler_schedule(void);                  /* Trigger scheduling decision */
void scheduler_yield(void);                     /* Current process yields CPU */

/* Context Switch */
void scheduler_switch_context(process_t *from, process_t *to);

/* Policy Configuration */
void scheduler_set_policy(sched_policy_t policy);
sched_policy_t scheduler_get_policy(void);
void scheduler_set_quantum(uint32_t quantum);
uint32_t scheduler_get_quantum(void);

/* Aging Configuration */
void scheduler_enable_aging(uint8_t enable);
void scheduler_set_aging_threshold(uint32_t threshold);
void scheduler_set_aging_interval(uint32_t interval);
void scheduler_check_aging(void);               /* Check and apply aging */

/* Preemption Control */
void scheduler_enable_preemption(uint8_t enable);
uint8_t scheduler_is_preemptive(void);

/* Process Time Quantum Management */
void scheduler_set_process_quantum(uint32_t pid, uint32_t quantum);
uint32_t scheduler_get_process_quantum(uint32_t pid);

/* Statistics and Monitoring */
void scheduler_get_stats(sched_stats_t *stats);
void scheduler_print_stats(void);
void scheduler_reset_stats(void);

/* Utility Functions */
const char *scheduler_policy_to_string(sched_policy_t policy);
void scheduler_print_config(void);

/* Advanced Scheduling */
process_t *scheduler_select_next_process(void);  /* Select next process to run */
void scheduler_update_process_time(process_t *proc, uint32_t ticks);

#endif /* SCHEDULER_H */
