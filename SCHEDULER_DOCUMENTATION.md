# CPU Scheduler Documentation - kacchiOS

## Overview

The kacchiOS CPU scheduler implements a **priority-based preemptive Round Robin** scheduling algorithm with automatic context switching and aging mechanism to prevent starvation.

---

## Scheduling Policy

### Algorithm: Priority-Based Preemptive Round Robin

The scheduler combines two classic scheduling algorithms:
- **Priority Scheduling**: Processes are selected based on priority levels
- **Round Robin**: Within the same priority level, processes are scheduled in a circular queue with time quantums

### Key Features

1. **Preemptive Scheduling**: Higher-priority processes can interrupt lower-priority processes
2. **Automatic Context Switching**: No manual intervention required
3. **Aging Mechanism**: Prevents starvation of low-priority processes
4. **Configurable Time Quantums**: Each priority level has its own time slice

---

## Priority Levels

The scheduler supports four priority levels (from highest to lowest):

| Priority Level | Value | Time Quantum | Description |
|----------------|-------|--------------|-------------|
| `CRITICAL`     | 3     | 50 ticks     | Highest priority, shortest quantum |
| `HIGH`         | 2     | 100 ticks    | High priority tasks |
| `NORMAL`       | 1     | 150 ticks    | Default priority |
| `LOW`          | 0     | 200 ticks    | Lowest priority, longest quantum |

### Time Quantum Philosophy

- **Critical processes** get the shortest quantum (50 ticks) to ensure they can run frequently
- **Low priority processes** get the longest quantum (200 ticks) to maximize throughput when they do run
- This design balances responsiveness with efficiency

---

## Process States

Processes can be in one of the following states:

| State | Description |
|-------|-------------|
| `READY` | Process is in the ready queue, waiting to be scheduled |
| `RUNNING` | Process is currently executing on the CPU |
| `FINISHED` | Process has completed execution |
| `BLOCKED` | Process is waiting for I/O or other event |
| `WAITING` | Process is waiting for a message (IPC) |
| `SLEEPING` | Process is sleeping for a specified time |

---

## Scheduling Algorithm Details

### Process Selection

The scheduler always selects the **highest-priority process** from the ready queue:

1. Ready queue is maintained as a **priority-ordered linked list**
2. Processes are inserted into the ready queue based on priority
3. Within the same priority, processes are ordered by insertion time (FIFO)
4. The scheduler always picks the head of the ready queue

### Context Switch Triggers

A context switch occurs automatically when:

1. **Running process exhausts its time quantum**
   - The process's `remaining_slice` counter reaches 0
   - Process is moved back to the ready queue
   - Next highest-priority process is selected

2. **Higher-priority process becomes READY**
   - A new process is created with higher priority than the current process
   - A blocked process is unblocked with higher priority
   - Aging promotes a process above the current running process

3. **Running process completes execution**
   - The process's `remaining_time` reaches 0
   - Process state changes to `FINISHED`
   - Next process is selected from the ready queue

4. **Running process blocks voluntarily**
   - Process waits for I/O or message
   - Process is removed from ready queue
   - Next process is selected

### Context Switch Process

When a context switch occurs:

1. **Save current process state**
   - CPU context (registers, stack pointers) is saved to PCB
   - Process state changes from `RUNNING` to `READY`
   - Process is added back to the ready queue (if not finished/blocked)

2. **Select new process**
   - Highest-priority process is dequeued from ready queue
   - If no processes are ready, CPU enters IDLE state

3. **Restore new process state**
   - CPU context is restored from new process's PCB
   - Process state changes to `RUNNING`
   - `remaining_slice` is reset to the process's time quantum
   - Age counter is reset to 0

4. **Log the switch**
   - Context switch event is logged with timestamp
   - Shows: old process → new process transition

---

## Tick Mechanism

The scheduler operates on a discrete time system based on **ticks**.

### What is a Tick?

- A tick represents **one unit of CPU time**
- Each tick advances the system clock by 1
- In a real OS, this would be driven by a hardware timer interrupt
- In kacchiOS, ticks are advanced manually via the `tick` command

### Tick Operation

When `scheduler_tick()` is called:

1. **Increment system tick counter**
   - Global `system_ticks` variable is incremented

2. **Update running process**
   - Decrement `remaining_time` (total execution time left)
   - Decrement `remaining_slice` (quantum time left)
   - Increment `cpu_time` (total CPU time used)
   - Check if process finished or quantum expired

3. **Age processes in ready queue**
   - Increment `age` counter for each ready process
   - Increment `wait_time` for each ready process
   - Promote processes that exceed aging threshold

4. **Check for context switch**
   - If running process exhausted quantum → reschedule
   - If running process finished → reschedule
   - If higher-priority process exists → preempt and reschedule

5. **Schedule next process**
   - Call `scheduler_schedule()` if needed
   - Perform context switch if appropriate

---

## Aging Mechanism

Aging prevents **starvation** - the situation where low-priority processes never get CPU time because higher-priority processes keep arriving.

### How Aging Works

1. **Age Counter**
   - Each process has an `age` counter (starts at 0)
   - Incremented by 1 on every tick while the process is in the READY state
   - Reset to 0 when the process starts running

2. **Aging Threshold**
   - Configured as `AGING_THRESHOLD = 500` ticks
   - When a process's age reaches 500, it gets promoted

3. **Priority Promotion**
   - Process priority is increased by one level
   - Process is removed from ready queue and re-inserted at new priority
   - Maximum priority is `CRITICAL` (no promotion beyond this)
   - Age counter is reset to 0

4. **Promotion Examples**
   - `LOW` → `NORMAL` after 500 ticks waiting
   - `NORMAL` → `HIGH` after 500 ticks waiting
   - `HIGH` → `CRITICAL` after 500 ticks waiting
   - `CRITICAL` stays at `CRITICAL` (no further promotion)

### Why 500 Ticks?

The aging threshold of 500 ticks is chosen to balance:
- **Prevent starvation**: Ensures even low-priority processes eventually run
- **Preserve priority semantics**: Doesn't promote too quickly, maintaining priority distinctions
- **Realistic behavior**: In a system with varied workloads, 500 ticks is enough time to detect true starvation

---

## Process Creation

### Command Format

```
create <name> <priority> <required_time>
```

### Parameters

- **name**: String identifier for the process (e.g., "Worker1", "Task_A")
- **priority**: One of `critical`, `high`, `normal`, `low`
- **required_time**: Total CPU time required to complete (in ticks)

### Examples

```bash
create WebServer critical 500
create Database high 1000
create BackupTask normal 2000
create LogCleanup low 3000
```

### Process Creation Flow

1. **Allocate Process Control Block (PCB)**
   - Memory allocated for process metadata
   - PCB contains all scheduling and execution state

2. **Initialize Process Fields**
   - Auto-assign unique PID (Process ID)
   - Set process name
   - Set priority level
   - Set `required_time` = `remaining_time`
   - Set `time_quantum` based on priority
   - Initialize `remaining_slice` = `time_quantum`

3. **Allocate Stack**
   - 4KB stack allocated for process
   - Stack base and top pointers saved in PCB

4. **Add to Process Table**
   - Process added to global process table
   - Allows lookup by PID

5. **Add to Ready Queue**
   - Process inserted into ready queue by priority
   - State set to `READY`

6. **Trigger Scheduling**
   - If new process has higher priority than current, context switch occurs immediately
   - Otherwise, process waits in ready queue

---

## Process Table

The process table displays the current state of all processes:

```
========================================
    CPU SCHEDULER STATUS
========================================
System Ticks:     1234
Scheduler:        ENABLED
Current Process:  'Worker1' (PID 2)
========================================

=== Process Table ===
PID  Name          Priority   State      Remain  Wait   Slice
---  ------------  ---------  ---------  ------  -----  -----
 1   IdleTask      LOW        READY       450    100     -
 2   Worker1       HIGH       RUNNING     200     50    75
 3   CriticalJob   CRITICAL   READY       100    500     -
 4   BackupTask    NORMAL     FINISHED      0    200     -
---
Total: 4 processes
====================
```

### Table Columns

- **PID**: Process ID (auto-assigned, unique)
- **Name**: Process name (user-specified)
- **Priority**: Current priority level (can change due to aging)
- **State**: Current process state (READY, RUNNING, FINISHED)
- **Remain**: Remaining execution time in ticks
- **Wait**: Total time spent waiting in READY state
- **Slice**: Remaining time quantum (only for RUNNING process)

---

## Scheduling Examples

### Example 1: Basic Priority Preemption

```bash
# Start with no processes
create Task_A low 1000
# Task_A starts running (only process)

tick 50
# Task_A runs for 50 ticks (remaining: 950)

create Task_B high 500
# Task_B has higher priority → immediate context switch
# Task_A moved to ready queue
# Task_B starts running
```

**Result**: Higher-priority process preempts lower-priority process immediately.

---

### Example 2: Time Quantum Expiration

```bash
create Worker1 normal 500
create Worker2 normal 500
# Both have same priority, Worker1 runs first

tick 150
# Worker1's quantum expires (QUANTUM_NORMAL = 150)
# Context switch: Worker1 → Worker2
# Worker1 moved to back of ready queue

tick 150
# Worker2's quantum expires
# Context switch: Worker2 → Worker1
# Round-robin continues
```

**Result**: Processes with same priority share CPU using round-robin.

---

### Example 3: Process Completion

```bash
create QuickTask high 100
# QuickTask starts running

tick 100
# QuickTask finishes (remaining_time = 0)
# State changes to FINISHED
# Next process in ready queue is scheduled
```

**Result**: Completed processes are marked FINISHED and removed from scheduling.

---

### Example 4: Aging in Action

```bash
create LowPrioTask low 2000
create HighPrioTask high 1000
# HighPrioTask preempts LowPrioTask

tick 1000
# HighPrioTask finishes
# LowPrioTask has been waiting 1000 ticks
# LowPrioTask has been promoted twice:
#   - At tick 500: LOW → NORMAL
#   - At tick 1000: NORMAL → HIGH
# LowPrioTask now runs with HIGH priority
```

**Result**: Aging prevents starvation of low-priority tasks.

---

## Scheduler Commands

### `ps` - Show Process Table

Displays the current scheduler status and process table.

```bash
kacchiOS> ps
```

Shows:
- System tick count
- Current running process
- All processes with their states

---

### `tick [n]` - Advance Time

Advances the scheduler by `n` ticks (default: 1).

```bash
kacchiOS> tick       # Advance by 1 tick
kacchiOS> tick 100   # Advance by 100 ticks
kacchiOS> tick 1000  # Advance by 1000 ticks
```

Each tick:
- Updates running process execution time
- Checks for quantum expiration
- Ages waiting processes
- Triggers context switches as needed

---

### `create <name> <priority> <time>` - Create Process

Creates a new process with specified parameters.

```bash
kacchiOS> create Worker1 high 500
kacchiOS> create BackupJob low 2000
kacchiOS> create WebServer critical 1000
```

Parameters:
- **name**: Process identifier
- **priority**: `critical`, `high`, `normal`, `low`
- **time**: Required execution time in ticks

---

### `kill <pid>` - Terminate Process

Terminates a process by PID.

```bash
kacchiOS> kill 3
```

Immediately terminates the process and removes it from scheduling.

---

### `info <pid>` - Show Process Details

Shows detailed information about a specific process.

```bash
kacchiOS> info 2
```

Displays:
- Full process metadata
- Memory allocation
- Scheduling statistics
- Parent PID
- Message queue status

---

## Scheduler Performance Metrics

### CPU Utilization

- **CPU Time**: Total ticks spent executing processes
- **Idle Time**: Ticks with no ready processes
- **Utilization** = CPU Time / Total Ticks

### Process Metrics

For each process:
- **Turnaround Time** = Completion Time - Creation Time
- **Waiting Time** = Total time in READY state
- **Response Time** = First run time - Creation Time
- **CPU Time** = Total ticks executed

### Fairness

The scheduler ensures fairness through:
1. **Priority-based allocation**: Higher-priority tasks get more CPU time
2. **Round-robin within priority**: Equal sharing among same-priority tasks
3. **Aging**: Prevents indefinite starvation

---

## Implementation Details

### Data Structures

#### Process Control Block (PCB)
```c
typedef struct process {
    uint32_t pid;                   // Process ID
    char name[32];                  // Process name
    process_state_t state;          // Current state
    process_priority_t priority;    // Priority level
    
    uint32_t required_time;         // Total execution time needed
    uint32_t remaining_time;        // Time left to complete
    uint32_t remaining_slice;       // Quantum time left
    uint32_t cpu_time;              // Total CPU time used
    uint32_t wait_time;             // Total wait time
    uint32_t age;                   // Age for priority boost
    
    struct process *next;           // Next in queue
    struct process *prev;           // Previous in queue
} process_t;
```

#### Ready Queue

- **Type**: Priority-ordered doubly-linked list
- **Head**: Highest-priority process
- **Tail**: Lowest-priority process
- **Insertion**: O(n) - finds correct priority position
- **Removal**: O(1) - uses head pointer
- **Traversal**: O(n) - for aging

### Key Functions

#### `scheduler_init()`
- Initializes scheduler state
- Sets configuration parameters
- Logs initialization message

#### `scheduler_tick()`
- Main scheduler heartbeat
- Updates all timing counters
- Triggers context switches
- Implements aging logic

#### `scheduler_schedule()`
- Makes scheduling decisions
- Selects next process to run
- Triggers context switch if needed
- Handles IDLE state

#### `scheduler_context_switch(process_t *new_process)`
- Saves current process state
- Restores new process state
- Updates process states
- Logs context switch event

#### `process_create_with_time(name, priority, time)`
- Creates new process
- Initializes all fields
- Adds to ready queue
- May trigger immediate context switch

#### `scheduler_age_processes()`
- Iterates through ready queue
- Increments age counters
- Promotes processes exceeding threshold
- Re-inserts promoted processes

---

## Deterministic Behavior

The scheduler is **deterministic** - given the same sequence of commands, it will always produce the same results:

1. **Priority ordering is stable**: Processes with equal priority maintain insertion order
2. **Context switches are logged**: Every scheduling decision is traceable
3. **Timing is precise**: Tick-based execution is exact and reproducible
4. **State is observable**: Process table shows complete system state

This determinism makes the scheduler ideal for:
- Testing and debugging
- Teaching scheduling concepts
- Performance analysis
- Reproducible experiments

---

## Limitations and Future Enhancements

### Current Limitations

1. **No real-time guarantees**: Priority helps but doesn't guarantee deadlines
2. **No CPU affinity**: Single CPU simulation only
3. **No process migration**: Processes don't move between queues except for aging
4. **Simple aging**: Fixed threshold, no adaptive aging

### Potential Enhancements

1. **Multi-level feedback queue**: Dynamically adjust priority based on behavior
2. **Real-time scheduling classes**: Support for deadline-based scheduling
3. **CPU burst prediction**: Estimate process behavior for better decisions
4. **Load balancing**: Multi-CPU support with work stealing
5. **Dynamic quantum adjustment**: Vary time slices based on system load
6. **Process groups**: Gang scheduling for related processes
7. **Nice values**: User-configurable priority adjustments
8. **I/O awareness**: Priority boost for I/O-bound processes

---

## Educational Value

This scheduler implementation demonstrates:

1. **Core OS Concepts**
   - Process states and transitions
   - Context switching
   - Priority scheduling
   - Round-robin time-sharing

2. **Starvation Prevention**
   - Aging mechanism
   - Priority inversion awareness
   - Fairness guarantees

3. **Preemptive Multitasking**
   - Timer-driven scheduling
   - Quantum management
   - State preservation

4. **Data Structure Design**
   - Priority queues
   - Process tables
   - Linked list management

5. **Real-World Trade-offs**
   - Responsiveness vs. throughput
   - Fairness vs. priority
   - Overhead vs. granularity

---

## Conclusion

The kacchiOS CPU scheduler provides a complete, working implementation of priority-based preemptive scheduling with automatic context switching and starvation prevention. It balances theoretical correctness with practical usability, making it an excellent platform for learning operating system concepts and experimenting with scheduling algorithms.

The scheduler's deterministic behavior, comprehensive logging, and observable state make it ideal for understanding how modern operating systems manage CPU resources and ensure fair, efficient process execution.
