/* process.h - Process Manager Interface */
#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"

/* Process states */
typedef enum {
    PROC_STATE_READY = 0,       /* Process is ready to run */
    PROC_STATE_CURRENT,          /* Process is currently running */
    PROC_STATE_TERMINATED,       /* Process has terminated */
    PROC_STATE_BLOCKED,          /* Process is blocked (Good to Have) */
    PROC_STATE_WAITING,          /* Process is waiting (Good to Have) */
    PROC_STATE_SLEEPING          /* Process is sleeping (Good to Have) */
} process_state_t;

/* Process priority levels */
typedef enum {
    PROC_PRIORITY_LOW = 0,
    PROC_PRIORITY_NORMAL,
    PROC_PRIORITY_HIGH,
    PROC_PRIORITY_CRITICAL
} process_priority_t;

/* CPU register context for context switching */
typedef struct {
    uint32_t eax, ebx, ecx, edx;    /* General purpose registers */
    uint32_t esi, edi;              /* Index registers */
    uint32_t ebp, esp;              /* Stack pointers */
    uint32_t eip;                   /* Instruction pointer */
    uint32_t eflags;                /* Flags register */
    uint32_t cs, ds, es, fs, gs, ss; /* Segment registers */
} cpu_context_t;

/* Process Control Block (PCB) */
typedef struct process {
    uint32_t pid;                   /* Process ID */
    char name[32];                  /* Process name */
    process_state_t state;          /* Current state */
    process_priority_t priority;    /* Process priority */
    
    /* Memory management */
    void *stack_base;               /* Stack base address */
    void *stack_top;                /* Stack top address */
    size_t stack_size;              /* Stack size */
    
    /* CPU context for context switching */
    cpu_context_t context;          /* Saved CPU state */
    
    /* Scheduling information */
    uint32_t time_quantum;          /* Time slice for scheduling */
    uint32_t cpu_time;              /* Total CPU time used */
    uint32_t wait_time;             /* Time spent waiting */
    uint32_t creation_time;         /* When process was created */
    
    /* IPC - Message passing (Good to Have) */
    uint32_t message_queue[16];     /* Simple message queue */
    uint32_t msg_count;             /* Number of messages */
    uint32_t waiting_for_msg;       /* 1 if waiting for message */
    
    /* Process relationships */
    uint32_t parent_pid;            /* Parent process ID */
    
    /* Exit status */
    int exit_code;                  /* Exit code when terminated */
    
    /* Aging for priority scheduling (Good to Have) */
    uint32_t age;                   /* Age counter for priority boost */
    
    /* Linked list pointers */
    struct process *next;           /* Next process in queue */
    struct process *prev;           /* Previous process in queue */
} process_t;

/* Process table statistics */
typedef struct {
    uint32_t total_processes;       /* Total processes created */
    uint32_t active_processes;      /* Currently active processes */
    uint32_t ready_processes;       /* Processes in ready state */
    uint32_t blocked_processes;     /* Processes in blocked state */
    uint32_t terminated_processes;  /* Terminated processes count */
} process_stats_t;

/* Maximum number of processes */
#define MAX_PROCESSES 32

/* Process function pointer type */
typedef void (*process_func_t)(void);

/* Process Manager Initialization */
void process_init(void);

/* Process Creation and Termination */
process_t *process_create(const char *name, process_func_t entry_point, process_priority_t priority);
void process_terminate(uint32_t pid);
void process_exit(int exit_code);

/* State Transition Functions */
void process_set_state(uint32_t pid, process_state_t new_state);
process_state_t process_get_state(uint32_t pid);
void process_block(uint32_t pid);
void process_unblock(uint32_t pid);
void process_sleep(uint32_t pid, uint32_t ticks);

/* Process Table Queries */
process_t *process_get_by_pid(uint32_t pid);
process_t *process_get_current(void);
uint32_t process_get_current_pid(void);
const char *process_get_name(uint32_t pid);
process_priority_t process_get_priority(uint32_t pid);

/* Process Priority Management */
void process_set_priority(uint32_t pid, process_priority_t priority);
void process_boost_priority(uint32_t pid);    /* For aging */
void process_reset_age(uint32_t pid);

/* Process Statistics and Utilities */
void process_get_stats(process_stats_t *stats);
void process_print_table(void);
void process_print_info(uint32_t pid);
uint32_t process_count(void);
uint32_t process_count_by_state(process_state_t state);

/* Inter-Process Communication (IPC) - Good to Have */
int process_send_message(uint32_t dest_pid, uint32_t message);
int process_receive_message(uint32_t *message);  /* Blocking receive */
int process_has_message(uint32_t pid);

/* Process List Management (for scheduler) */
process_t *process_get_ready_queue(void);
process_t *process_dequeue_ready(void);
void process_enqueue_ready(process_t *proc);

/* Helper functions for process state names */
const char *process_state_to_string(process_state_t state);
const char *process_priority_to_string(process_priority_t priority);

#endif /* PROCESS_H */
