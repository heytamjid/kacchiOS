# Scheduler - API Documentation

## Overview

The scheduler component implements a preemptive process scheduler with multiple scheduling policies, configurable time quanta, and priority aging mechanisms. It works closely with the process manager to select and switch between runnable processes.

## Features Implemented

### Core Features
- ✅ **Clear Scheduling Policy** - 4 different scheduling algorithms to choose from
- ✅ **Context Switching** - Complete context save/restore mechanism
- ✅ **Configurable Time Quantum** - Both global and per-process time slice configuration
- ✅ **Aging Implementation** - Prevents process starvation through priority boosting

### Advanced Features
- ✅ **Multiple Scheduling Policies** - Round-Robin, Priority, Priority+RR, FCFS
- ✅ **Preemptive Scheduling** - Time-based preemption with configurable enable/disable
- ✅ **Statistics Tracking** - Comprehensive scheduler performance metrics
- ✅ **Runtime Configuration** - Change policies and parameters without restart

---

## Data Structures

### Scheduling Policy (`sched_policy_t`)

```c
typedef enum {
    SCHED_POLICY_ROUND_ROBIN,    // Fair time-sharing
    SCHED_POLICY_PRIORITY,       // Strict priority-based
    SCHED_POLICY_PRIORITY_RR,    // Priority with round-robin per level
    SCHED_POLICY_FCFS            // First-Come-First-Served
} sched_policy_t;
```

**Policies:**
- **Round-Robin**: All processes get equal CPU time in circular order
- **Priority**: Highest priority process always runs first
- **Priority Round-Robin**: Round-robin within each priority level
- **FCFS**: Processes run in order of arrival (non-preemptive)

### Scheduler Configuration (`sched_config_t`)

```c
typedef struct {
    sched_policy_t policy;           // Current scheduling policy
    uint32_t default_quantum;        // Default time quantum (ticks)
    uint32_t min_quantum;            // Minimum allowed quantum
    uint32_t max_quantum;            // Maximum allowed quantum
    uint32_t aging_threshold;        // Ticks before aging boost
    uint32_t aging_boost_interval;   // How often to check for aging
    uint8_t enable_aging;            // Aging on/off
    uint8_t enable_preemption;       // Preemption on/off
} sched_config_t;
```

**Default Values:**
- Policy: Priority-Based
- Default Quantum: 100 ticks
- Min/Max Quantum: 10 - 1000 ticks
- Aging Threshold: 100 ticks
- Aging Check Interval: 50 ticks
- Aging: Enabled
- Preemption: Enabled

### Scheduler Statistics (`sched_stats_t`)

```c
typedef struct {
    uint32_t total_context_switches;  // Number of context switches
    uint32_t total_ticks;             // Total scheduler ticks
    uint32_t idle_ticks;              // Ticks with no process running
    uint32_t total_aging_boosts;      // Priority boosts from aging
    uint32_t preemptions;             // Forced context switches
    uint32_t voluntary_yields;        // Processes yielding CPU
} sched_stats_t;
```

---

## API Functions

### Initialization

#### `void scheduler_init(sched_policy_t policy, uint32_t default_quantum)`

Initialize the scheduler with a specific policy and time quantum.

**Parameters:**
- `policy`: Initial scheduling policy to use
- `default_quantum`: Default time slice in ticks

**Example:**
```c
// Initialize with priority scheduling, 100-tick quantum
scheduler_init(SCHED_POLICY_PRIORITY, 100);

// Initialize with round-robin, 50-tick quantum
scheduler_init(SCHED_POLICY_ROUND_ROBIN, 50);
```

### Control Functions

#### `void scheduler_start(void)`

Start the scheduler and begin process scheduling.

**Example:**
```c
scheduler_start();  // Begin scheduling processes
```

#### `void scheduler_stop(void)`

Stop the scheduler.

**Example:**
```c
scheduler_stop();  // Halt scheduling
```

#### `void scheduler_tick(void)`

Timer tick handler - called on each timer interrupt.

**Functionality:**
- Updates tick counters
- Decrements current process time slice
- Triggers preemption when quantum expires
- Periodically checks for aging

**Example:**
```c
// In timer interrupt handler
void timer_interrupt_handler(void) {
    scheduler_tick();
}
```

### Scheduling Functions

#### `void scheduler_schedule(void)`

Make a scheduling decision and potentially switch processes.

**Functionality:**
- Selects next process to run
- Compares with current process
- Performs context switch if needed
- Updates statistics

**Example:**
```c
// Explicit scheduling request
scheduler_schedule();

// After blocking current process
process_block(current_pid);
scheduler_schedule();  // Find another process to run
```

#### `process_t *scheduler_select_next_process(void)`

Select the next process to run based on current policy.

**Returns:**
- Pointer to next process, or NULL if no runnable process

**Example:**
```c
process_t *next = scheduler_select_next_process();
if (next) {
    serial_puts("Next process: ");
    serial_puts(next->name);
}
```

#### `void scheduler_switch_context(process_t *from, process_t *to)`

Perform context switch between two processes.

**Parameters:**
- `from`: Process to switch away from (can be NULL)
- `to`: Process to switch to (can be NULL)

**Functionality:**
- Saves current process state
- Updates process states (CURRENT → READY)
- Loads next process state
- Updates statistics

**Example:**
```c
process_t *current = process_get_current();
process_t *next = scheduler_select_next_process();
scheduler_switch_context(current, next);
```

#### `void scheduler_yield(void)`

Current process voluntarily yields the CPU.

**Example:**
```c
// Process gives up its time slice
scheduler_yield();
```

### Policy Management

#### `void scheduler_set_policy(sched_policy_t policy)`

Change the scheduling policy at runtime.

**Parameters:**
- `policy`: New scheduling policy to use

**Example:**
```c
// Switch to round-robin
scheduler_set_policy(SCHED_POLICY_ROUND_ROBIN);

// Switch to FCFS
scheduler_set_policy(SCHED_POLICY_FCFS);
```

#### `sched_policy_t scheduler_get_policy(void)`

Get the current scheduling policy.

**Returns:**
- Current scheduling policy

**Example:**
```c
sched_policy_t policy = scheduler_get_policy();
if (policy == SCHED_POLICY_PRIORITY) {
    serial_puts("Using priority scheduling\n");
}
```

### Time Quantum Configuration

#### `void scheduler_set_quantum(uint32_t quantum)`

Set the default time quantum for new processes.

**Parameters:**
- `quantum`: Time slice in ticks (clamped to min/max range)

**Example:**
```c
// Set 200-tick quantum
scheduler_set_quantum(200);

// Try to set 5 ticks (will be clamped to min_quantum=10)
scheduler_set_quantum(5);
```

#### `uint32_t scheduler_get_quantum(void)`

Get the default time quantum.

**Returns:**
- Default quantum in ticks

**Example:**
```c
uint32_t quantum = scheduler_get_quantum();
serial_put_dec(quantum);
```

#### `void scheduler_set_process_quantum(uint32_t pid, uint32_t quantum)`

Set time quantum for a specific process.

**Parameters:**
- `pid`: Process ID
- `quantum`: Time slice in ticks

**Example:**
```c
// Give high-priority process longer time slice
scheduler_set_process_quantum(10, 500);
```

#### `uint32_t scheduler_get_process_quantum(uint32_t pid)`

Get time quantum for a specific process.

**Parameters:**
- `pid`: Process ID

**Returns:**
- Process quantum, or 0 if process not found

**Example:**
```c
uint32_t quantum = scheduler_get_process_quantum(pid);
```

### Aging Configuration

#### `void scheduler_enable_aging(uint8_t enable)`

Enable or disable priority aging.

**Parameters:**
- `enable`: 1 to enable, 0 to disable

**Example:**
```c
scheduler_enable_aging(1);  // Enable aging
scheduler_enable_aging(0);  // Disable aging
```

#### `void scheduler_check_aging(void)`

Manually check all processes for aging and boost priorities as needed.

**Functionality:**
- Scans all READY processes
- Increments age counters
- Boosts priority if age exceeds threshold
- Resets age counter after boost

**Example:**
```c
// Force aging check
scheduler_check_aging();
```

#### `void scheduler_set_aging_threshold(uint32_t threshold)`

Set the age threshold for priority boosting.

**Parameters:**
- `threshold`: Number of ticks before boost

**Example:**
```c
// Boost after 200 ticks of waiting
scheduler_set_aging_threshold(200);
```

#### `void scheduler_set_aging_interval(uint32_t interval)`

Set how often to check for aging.

**Parameters:**
- `interval`: Check interval in ticks

**Example:**
```c
// Check for aging every 100 ticks
scheduler_set_aging_interval(100);
```

### Preemption Control

#### `void scheduler_enable_preemption(uint8_t enable)`

Enable or disable preemptive scheduling.

**Parameters:**
- `enable`: 1 to enable, 0 to disable

**Example:**
```c
scheduler_enable_preemption(1);  // Enable preemption
scheduler_enable_preemption(0);  // Cooperative scheduling only
```

#### `uint8_t scheduler_is_preemptive(void)`

Check if scheduler is currently preemptive.

**Returns:**
- 1 if preemptive, 0 if cooperative

**Example:**
```c
if (scheduler_is_preemptive()) {
    serial_puts("Preemptive scheduling active\n");
}
```

### Statistics

#### `void scheduler_get_stats(sched_stats_t *stats)`

Get scheduler statistics.

**Parameters:**
- `stats`: Pointer to statistics structure to fill

**Example:**
```c
sched_stats_t stats;
scheduler_get_stats(&stats);
serial_put_dec(stats.total_context_switches);
```

#### `void scheduler_print_stats(void)`

Print formatted scheduler statistics to serial console.

**Example:**
```c
scheduler_print_stats();
```

**Output:**
```
=== Scheduler Statistics ===
Total Ticks:          5000
Idle Ticks:           150
Context Switches:     45
Preemptions:          30
Voluntary Yields:     15
Aging Boosts:         5
CPU Utilization:      97%
===========================
```

#### `void scheduler_reset_stats(void)`

Reset all scheduler statistics to zero.

**Example:**
```c
scheduler_reset_stats();
```

### Utility Functions

#### `void scheduler_print_config(void)`

Print current scheduler configuration to serial console.

**Example:**
```c
scheduler_print_config();
```

**Output:**
```
=== Scheduler Configuration ===
Policy:               Priority-Based
Default Quantum:      100 ticks
Quantum Range:        10 - 1000 ticks
Aging:                Enabled
  Threshold:          100 ticks
  Check Interval:     50 ticks
Preemption:           Enabled
Scheduler:            Running
==============================
```

#### `const char *scheduler_policy_to_string(sched_policy_t policy)`

Convert scheduling policy to string name.

**Parameters:**
- `policy`: Scheduling policy

**Returns:**
- String representation of policy

**Example:**
```c
const char *name = scheduler_policy_to_string(SCHED_POLICY_ROUND_ROBIN);
serial_puts(name);  // Prints "Round-Robin"
```

---

## Usage Examples

### Basic Initialization

```c
void kmain(void) {
    // Initialize dependencies first
    memory_init();
    process_init();
    
    // Initialize scheduler with priority policy, 100-tick quantum
    scheduler_init(SCHED_POLICY_PRIORITY, 100);
    
    // Create some processes
    process_create("Worker1", worker_func, PROC_PRIORITY_HIGH);
    process_create("Worker2", worker_func, PROC_PRIORITY_NORMAL);
    
    // Start scheduling
    scheduler_start();
}
```

### Timer Integration

```c
void timer_interrupt_handler(void) {
    // Called on each timer tick (e.g., every 10ms)
    scheduler_tick();
}
```

### Dynamic Policy Changes

```c
// Start with round-robin
scheduler_init(SCHED_POLICY_ROUND_ROBIN, 50);

// Switch to priority-based under load
if (system_load > THRESHOLD) {
    scheduler_set_policy(SCHED_POLICY_PRIORITY);
    scheduler_set_quantum(100);
}

// Switch to FCFS for batch jobs
if (batch_mode) {
    scheduler_set_policy(SCHED_POLICY_FCFS);
    scheduler_enable_preemption(0);
}
```

### Custom Per-Process Quantum

```c
// Create processes
process_t *interactive = process_create("UI", ui_task, PROC_PRIORITY_HIGH);
process_t *background = process_create("BG", bg_task, PROC_PRIORITY_LOW);

// Interactive gets short quantum for responsiveness
scheduler_set_process_quantum(interactive->pid, 20);

// Background gets long quantum to reduce overhead
scheduler_set_process_quantum(background->pid, 500);
```

### Monitoring Performance

```c
void print_scheduler_info(void) {
    // Show configuration
    scheduler_print_config();
    
    // Show statistics
    scheduler_print_stats();
    
    // Calculate averages
    sched_stats_t stats;
    scheduler_get_stats(&stats);
    
    if (stats.total_context_switches > 0) {
        uint32_t avg_run_time = stats.total_ticks / stats.total_context_switches;
        serial_puts("Average time between switches: ");
        serial_put_dec(avg_run_time);
        serial_puts(" ticks\n");
    }
}
```

### Aging Configuration

```c
// Configure aggressive aging for real-time system
scheduler_enable_aging(1);
scheduler_set_aging_threshold(50);   // Boost after 50 ticks
scheduler_set_aging_interval(10);     // Check every 10 ticks

// Configure conservative aging for throughput
scheduler_set_aging_threshold(500);   // Boost after 500 ticks
scheduler_set_aging_interval(100);    // Check every 100 ticks
```

---

## Integration with Process Manager

The scheduler relies heavily on the process manager:

### Ready Queue
```c
// Scheduler uses process manager's ready queue
process_t *next = process_dequeue_ready();  // Get next process
process_enqueue_ready(prev);                 // Return previous process
```

### State Management
```c
// Scheduler updates process states
process_set_state(pid, PROC_STATE_CURRENT);
process_set_state(pid, PROC_STATE_READY);
```

### Priority Boosting
```c
// Aging uses process manager's boost function
process_boost_priority(pid);
process_reset_age(pid);
```

### Context Structure
```c
// Stored in process PCB
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip;
    uint32_t eflags;
    uint16_t cs, ds, es, fs, gs, ss;
} cpu_context_t;
```

---

## Performance Considerations

### Time Complexity
- `scheduler_tick()`: O(1)
- `scheduler_schedule()`: O(1) - uses ready queue
- `scheduler_select_next_process()`: O(1) - dequeue operation
- `scheduler_check_aging()`: O(n) - scans all processes

### Overhead
- Context switch cost: Minimal (simplified context save/restore)
- Aging check: Periodic, configurable interval
- Statistics: Minimal overhead (simple counters)

### Optimization Tips
1. **Quantum Selection**: Balance responsiveness vs overhead
   - Small quantum (10-50): More responsive, higher overhead
   - Large quantum (200-500): Better throughput, less responsive

2. **Aging Configuration**: Prevent starvation without excessive boosts
   - Threshold too low: Excessive priority changes
   - Threshold too high: Possible starvation

3. **Policy Selection**:
   - Round-Robin: Fair, predictable
   - Priority: Best for mixed workloads
   - FCFS: Lowest overhead, batch processing

---

## Limitations

1. **Simplified Context Switch**: Educational implementation doesn't save all CPU state
2. **No Multi-Core Support**: Single-CPU scheduler only
3. **Fixed Priority Levels**: 4 priority levels (from process manager)
4. **No Load Balancing**: Single ready queue
5. **Integer Division**: CPU utilization calculation may lose precision

---

## See Also

- [Process Manager Documentation](PROCESS_MANAGER.md)
- [Scheduler Implementation Details](SCHEDULER_IMPLEMENTATION.md)
- [Memory Manager Documentation](MEMORY_MANAGER.md)
