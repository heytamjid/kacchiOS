# Process Manager Documentation

## Overview

The Process Manager for kacchiOS provides comprehensive process management capabilities including process creation, state management, scheduling support, priority management, and inter-process communication (IPC).

## Features Implemented

### Required Features ✅
- ✅ **Process Table** - Tracks up to 32 concurrent processes
- ✅ **Process Creation** - Create processes with entry points and priorities
- ✅ **State Transition** - Support for READY, CURRENT, TERMINATED states
- ✅ **Process Termination** - Clean process cleanup with resource deallocation
- ✅ **Utility Functions** - Query process information and statistics

### Advanced Features (Good to Have) ✅
- ✅ **Additional States** - BLOCKED, WAITING, SLEEPING states
- ✅ **Inter-Process Communication (IPC)** - Message passing between processes
- ✅ **Priority Management** - 4 priority levels with aging support
- ✅ **Priority-based Scheduling** - Ready queue ordered by priority
- ✅ **Process Relationships** - Parent-child tracking
- ✅ **CPU Context** - Full register context for context switching

## Architecture

### Process States

```
┌──────────┐
│  READY   │ ◄──── Initial state after creation
└────┬─────┘
     │
     │ Scheduler selects
     ▼
┌──────────┐
│ CURRENT  │ ◄──── Currently executing
└────┬─────┘
     │
     ├──► READY      (preempted/yielded)
     ├──► BLOCKED    (waiting for resource)
     ├──► WAITING    (waiting for event)
     ├──► SLEEPING   (timed wait)
     └──► TERMINATED (exited/killed)
```

### Process Control Block (PCB)

```c
typedef struct process {
    uint32_t pid;                   /* Process ID */
    char name[32];                  /* Process name */
    process_state_t state;          /* Current state */
    process_priority_t priority;    /* Process priority */
    
    void *stack_base;               /* Stack base */
    void *stack_top;                /* Stack top */
    size_t stack_size;              /* Stack size */
    
    cpu_context_t context;          /* CPU registers */
    
    uint32_t time_quantum;          /* Time slice */
    uint32_t cpu_time;              /* Total CPU time */
    uint32_t wait_time;             /* Wait time */
    uint32_t creation_time;         /* Creation timestamp */
    
    uint32_t message_queue[16];     /* IPC messages */
    uint32_t msg_count;             /* Message count */
    
    uint32_t parent_pid;            /* Parent process */
    int exit_code;                  /* Exit status */
    uint32_t age;                   /* Aging counter */
    
    struct process *next, *prev;    /* Queue links */
} process_t;
```

## API Reference

### Initialization

#### `void process_init(void)`
Initializes the process manager. Must be called once during kernel startup.

**Example:**
```c
process_init();
```

### Process Creation and Termination

#### `process_t *process_create(const char *name, process_func_t entry_point, process_priority_t priority)`
Creates a new process with the specified name, entry point, and priority.

**Parameters:**
- `name`: Process name (max 31 characters)
- `entry_point`: Function pointer to process entry point
- `priority`: PROC_PRIORITY_LOW, NORMAL, HIGH, or CRITICAL

**Returns:**
- Pointer to process_t on success, NULL on failure

**Example:**
```c
void worker_func(void) {
    serial_puts("Worker running\n");
}

process_t *proc = process_create("Worker", worker_func, PROC_PRIORITY_NORMAL);
```

#### `void process_terminate(uint32_t pid)`
Terminates a process by PID, freeing all resources.

**Parameters:**
- `pid`: Process ID to terminate

**Example:**
```c
process_terminate(proc->pid);
```

#### `void process_exit(int exit_code)`
Current process exits voluntarily with an exit code.

**Parameters:**
- `exit_code`: Exit status code

**Example:**
```c
process_exit(0);  // Successful exit
```

### State Transition Functions

#### `void process_set_state(uint32_t pid, process_state_t new_state)`
Sets the process state to a new value.

**Parameters:**
- `pid`: Process ID
- `new_state`: New state (READY, CURRENT, BLOCKED, etc.)

**Example:**
```c
process_set_state(pid, PROC_STATE_READY);
```

#### `process_state_t process_get_state(uint32_t pid)`
Gets the current state of a process.

**Returns:**
- Current process state

**Example:**
```c
if (process_get_state(pid) == PROC_STATE_READY) {
    // Process is ready to run
}
```

#### `void process_block(uint32_t pid)`
Blocks a process (sets state to BLOCKED).

**Example:**
```c
process_block(pid);  // Block until resource available
```

#### `void process_unblock(uint32_t pid)`
Unblocks a process (sets state back to READY).

**Example:**
```c
process_unblock(pid);  // Resource now available
```

#### `void process_sleep(uint32_t pid, uint32_t ticks)`
Puts a process to sleep for a number of ticks.

**Parameters:**
- `pid`: Process ID
- `ticks`: Number of ticks to sleep

**Example:**
```c
process_sleep(pid, 100);  // Sleep for 100 ticks
```

### Process Table Queries

#### `process_t *process_get_by_pid(uint32_t pid)`
Gets a process pointer by PID.

**Returns:**
- Pointer to process_t, or NULL if not found

**Example:**
```c
process_t *proc = process_get_by_pid(pid);
if (proc != NULL) {
    serial_puts(proc->name);
}
```

#### `process_t *process_get_current(void)`
Gets the currently running process.

**Returns:**
- Pointer to current process, or NULL

**Example:**
```c
process_t *current = process_get_current();
```

#### `uint32_t process_get_current_pid(void)`
Gets the PID of the currently running process.

**Returns:**
- Current process PID, or 0 if none

#### `const char *process_get_name(uint32_t pid)`
Gets the name of a process by PID.

**Returns:**
- Process name string, or "Unknown"

**Example:**
```c
serial_puts(process_get_name(pid));
```

#### `process_priority_t process_get_priority(uint32_t pid)`
Gets the priority of a process.

**Returns:**
- Process priority level

### Priority Management

#### `void process_set_priority(uint32_t pid, process_priority_t priority)`
Sets a new priority for a process.

**Parameters:**
- `pid`: Process ID
- `priority`: New priority level

**Example:**
```c
process_set_priority(pid, PROC_PRIORITY_HIGH);
```

#### `void process_boost_priority(uint32_t pid)`
Boosts process priority by one level (for aging).

**Example:**
```c
// Prevent starvation
if (proc->age > 100) {
    process_boost_priority(proc->pid);
    process_reset_age(proc->pid);
}
```

#### `void process_reset_age(uint32_t pid)`
Resets the aging counter for a process.

**Example:**
```c
process_reset_age(pid);
```

### Statistics and Utilities

#### `void process_get_stats(process_stats_t *stats)`
Retrieves process statistics.

**Parameters:**
- `stats`: Pointer to process_stats_t structure to fill

**Example:**
```c
process_stats_t stats;
process_get_stats(&stats);
serial_put_dec(stats.active_processes);
```

#### `void process_print_table(void)`
Prints formatted process table to serial output.

**Example:**
```c
process_print_table();
```

#### `void process_print_info(uint32_t pid)`
Prints detailed information about a specific process.

**Example:**
```c
process_print_info(pid);
```

#### `uint32_t process_count(void)`
Returns the total number of active processes.

**Returns:**
- Number of active processes

#### `uint32_t process_count_by_state(process_state_t state)`
Counts processes in a specific state.

**Returns:**
- Number of processes in the given state

**Example:**
```c
uint32_t ready_count = process_count_by_state(PROC_STATE_READY);
```

### Inter-Process Communication (IPC)

#### `int process_send_message(uint32_t dest_pid, uint32_t message)`
Sends a message to another process.

**Parameters:**
- `dest_pid`: Destination process ID
- `message`: 32-bit message value

**Returns:**
- 0 on success, -1 on failure

**Example:**
```c
process_send_message(dest_pid, 0xDEADBEEF);
```

#### `int process_receive_message(uint32_t *message)`
Receives a message (blocking if no messages available).

**Parameters:**
- `message`: Pointer to store received message

**Returns:**
- 0 on success, -1 if no messages or error

**Example:**
```c
uint32_t msg;
if (process_receive_message(&msg) == 0) {
    // Process message
}
```

#### `int process_has_message(uint32_t pid)`
Checks if a process has pending messages.

**Returns:**
- 1 if messages available, 0 otherwise

**Example:**
```c
if (process_has_message(pid)) {
    // Process has messages waiting
}
```

### Scheduler Support Functions

#### `process_t *process_get_ready_queue(void)`
Gets the head of the ready queue.

**Returns:**
- Pointer to first ready process, or NULL

#### `process_t *process_dequeue_ready(void)`
Removes and returns the next ready process (highest priority).

**Returns:**
- Pointer to dequeued process, or NULL if empty

**Example:**
```c
process_t *next = process_dequeue_ready();
if (next != NULL) {
    process_set_state(next->pid, PROC_STATE_CURRENT);
}
```

#### `void process_enqueue_ready(process_t *proc)`
Adds a process to the ready queue (priority-based insertion).

**Parameters:**
- `proc`: Process to enqueue

**Example:**
```c
process_enqueue_ready(proc);
```

### Helper Functions

#### `const char *process_state_to_string(process_state_t state)`
Converts state enum to string.

**Returns:**
- State name as string

#### `const char *process_priority_to_string(process_priority_t priority)`
Converts priority enum to string.

**Returns:**
- Priority name as string

## Configuration

Key constants in [process.h](process.h):

```c
#define MAX_PROCESSES 32            // Maximum concurrent processes
```

## Process Priority Levels

```c
typedef enum {
    PROC_PRIORITY_LOW = 0,          // Background tasks
    PROC_PRIORITY_NORMAL,           // Normal user processes
    PROC_PRIORITY_HIGH,             // Important processes
    PROC_PRIORITY_CRITICAL          // System-critical processes
} process_priority_t;
```

## Process States

```c
typedef enum {
    PROC_STATE_READY = 0,           // Ready to run
    PROC_STATE_CURRENT,             // Currently executing
    PROC_STATE_TERMINATED,          // Has terminated
    PROC_STATE_BLOCKED,             // Waiting for resource
    PROC_STATE_WAITING,             // Waiting for event
    PROC_STATE_SLEEPING             // Sleeping for time
} process_state_t;
```

## Testing

The kernel includes built-in process tests accessible via commands:

### Commands
- `ps` - Display process table
- `proctest` - Run comprehensive process manager tests
- `create` - Create a test process
- `kill <pid>` - Terminate process with given PID
- `info <pid>` - Show detailed process information

### Test Coverage
The `proctest` command tests:
1. Process creation with different priorities
2. Process table display
3. State transitions (READY, BLOCKED, CURRENT)
4. Priority management and boosting
5. Inter-process communication (message passing)
6. Detailed process information queries
7. Process statistics
8. Process termination and cleanup

## Usage with Memory Manager

The process manager integrates seamlessly with the memory manager:

```c
// Process creation allocates:
// 1. PCB via kmalloc()
process_t *proc = kmalloc(sizeof(process_t));

// 2. Stack via stack_alloc()
proc->stack_top = stack_alloc(proc->pid);

// Process termination frees:
// 1. Stack via stack_free()
stack_free(proc->pid);

// 2. PCB via kfree()
kfree(proc);
```

## Integration with Scheduler

The process manager provides all necessary functions for scheduler implementation:

```c
// Get next process to run
process_t *next = process_dequeue_ready();

// Switch context (simplified)
if (current != NULL) {
    // Save current process context
    process_set_state(current->pid, PROC_STATE_READY);
    process_enqueue_ready(current);
}

// Load next process
if (next != NULL) {
    process_set_state(next->pid, PROC_STATE_CURRENT);
    // Restore context and switch to process
}
```

## Performance Characteristics

- **Process Creation**: O(1) for PCB allocation + O(n) for queue insertion
- **Process Termination**: O(n) for table/queue removal
- **State Transition**: O(n) for queue management
- **Priority Queue**: Maintains priority order automatically
- **IPC**: O(1) message send/receive (queue operations)

## Example: Complete Process Lifecycle

```c
// 1. Create process
void worker_task(void) {
    for (int i = 0; i < 10; i++) {
        serial_puts("Working...\n");
        // Do work
    }
    process_exit(0);  // Clean exit
}

process_t *worker = process_create("Worker", worker_task, PROC_PRIORITY_NORMAL);

// 2. Process runs (scheduled by scheduler)
// ... process executes ...

// 3. Process may be blocked
process_block(worker->pid);  // Wait for resource

// 4. Later, unblock
process_unblock(worker->pid);  // Resource available

// 5. Priority boost (aging)
if (worker->age > 50) {
    process_boost_priority(worker->pid);
    process_reset_age(worker->pid);
}

// 6. IPC communication
process_send_message(worker->pid, 0x1234);

// 7. Process terminates (either via exit or kill)
process_terminate(worker->pid);
```

## Future Enhancements

Potential improvements for advanced students:
- Multi-level feedback queue scheduling
- Process groups and sessions
- Signal handling mechanism
- Copy-on-write for process forking
- Priority inheritance for synchronization
- Real-time scheduling support

## Files

- [process.h](process.h) - Process manager interface and structures
- [process.c](process.c) - Process manager implementation (700+ lines)
- [kernel.c](kernel.c) - Integration and test code

---

**kacchiOS Process Manager** - Full-featured process management for educational OS
