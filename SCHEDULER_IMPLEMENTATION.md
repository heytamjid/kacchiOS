# Scheduler - Implementation Details

## Architecture Overview

The scheduler is a preemptive, priority-based process scheduler that supports multiple scheduling policies. It works in close coordination with the process manager to select and execute processes.

```
┌─────────────────────────────────────────────────────────┐
│                    Timer Interrupt                       │
│                 (Hardware/Emulator)                      │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
         ┌───────────────────────┐
         │  scheduler_tick()     │
         │  - Update counters    │
         │  - Decrement quantum  │
         │  - Check preemption   │
         │  - Periodic aging     │
         └───────────┬───────────┘
                     │
                     ▼
         ┌───────────────────────┐
         │ scheduler_schedule()  │
         │  - Select next proc   │
         │  - Compare with curr  │
         │  - Context switch     │
         └───────────┬───────────┘
                     │
        ┌────────────┴────────────┐
        │                         │
        ▼                         ▼
┌──────────────┐      ┌───────────────────────┐
│  Same Proc   │      │  Different Process    │
│  Continue    │      │  Context Switch       │
└──────────────┘      └───────────┬───────────┘
                                  │
                      ┌───────────┴───────────┐
                      │                       │
                      ▼                       ▼
          ┌────────────────────┐   ┌────────────────────┐
          │   Save Context     │   │  Restore Context   │
          │   Update States    │   │  Set as CURRENT    │
          │   Enqueue Ready    │   │  Resume Execution  │
          └────────────────────┘   └────────────────────┘
```

---

## Core Components

### 1. Scheduler State

```c
/* Global scheduler state */
static sched_config_t sched_config;      // Configuration
static sched_stats_t sched_stats;        // Statistics
static uint8_t scheduler_running = 0;    // Running flag
static uint32_t current_tick = 0;        // Global tick counter
static uint32_t time_slice_remaining = 0; // Current quantum
```

**Design Rationale:**
- Static variables provide internal state hiding
- Single global instance (no multi-scheduler support needed)
- Tick counters enable time-based decisions

### 2. Policy Selection Functions

Four static functions implement different scheduling algorithms:

```c
static process_t *select_round_robin(void);
static process_t *select_priority(void);
static process_t *select_priority_rr(void);
static process_t *select_fcfs(void);
```

**Implementation Strategy:**
- All use `process_dequeue_ready()` from process manager
- Ready queue is already priority-ordered
- Minimal logic needed due to process manager integration

---

## Detailed Function Analysis

### Initialization: `scheduler_init()`

```c
void scheduler_init(sched_policy_t policy, uint32_t default_quantum)
```

**Initialization Sequence:**
1. Set configuration parameters (policy, quantum, limits)
2. Initialize aging settings (threshold, interval, enabled)
3. Enable preemption by default
4. Zero statistics structure
5. Set scheduler to stopped state
6. Reset tick counters

**Default Configuration:**
- Policy: As specified (typically PRIORITY)
- Default Quantum: As specified (typically 100)
- Min Quantum: 10 ticks
- Max Quantum: 1000 ticks
- Aging Threshold: 100 ticks
- Aging Interval: 50 ticks
- Aging: Enabled
- Preemption: Enabled

### Tick Handler: `scheduler_tick()`

```c
void scheduler_tick(void)
```

**Execution Flow:**

1. **Early Exit**: If scheduler not running, return immediately
2. **Update Counters**: Increment global tick and statistics tick counter
3. **Get Current Process**: Query process manager for current process
4. **Idle Handling**: If no current process:
   - Increment idle tick counter
   - Attempt to schedule new process
   - Return
5. **CPU Time**: Increment current process CPU time
6. **Quantum Management**: Decrement time slice remaining
7. **Preemption Check**: If quantum expired and preemption enabled:
   - Log preemption event
   - Increment preemption counter
   - Trigger scheduling
8. **Periodic Aging**: If aging enabled and interval reached:
   - Call `scheduler_check_aging()`

**Time Complexity**: O(1) in normal case, O(n) when aging check triggered

### Scheduling Decision: `scheduler_schedule()`

```c
void scheduler_schedule(void)
```

**Decision Logic:**

1. **Running Check**: Return if scheduler stopped
2. **Get Processes**:
   - Current: `process_get_current()`
   - Next: `scheduler_select_next_process()`
3. **No Process Available**:
   - If current exists, keep running it
   - Otherwise log "no process to schedule"
4. **Same Process**: Just reset quantum, continue
5. **Different Process**: Perform context switch
6. **Reset Quantum**: Set time slice for new process

**Scheduling Scenarios:**

| Current | Next  | Action                  |
|---------|-------|-------------------------|
| NULL    | NULL  | No-op, log message      |
| NULL    | P1    | Context switch to P1    |
| P1      | NULL  | Keep P1 running         |
| P1      | P1    | Reset quantum, continue |
| P1      | P2    | Context switch P1→P2    |

### Process Selection: `scheduler_select_next_process()`

```c
process_t *scheduler_select_next_process(void)
```

**Policy Dispatch:**
```c
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
```

**Individual Policy Implementations:**

#### Round-Robin (`select_round_robin`)
```c
static process_t *select_round_robin(void) {
    return process_dequeue_ready();
}
```
- Simply dequeues next ready process
- Process manager's FIFO queue provides round-robin
- Equal time sharing through quantum expiration

#### Priority (`select_priority`)
```c
static process_t *select_priority(void) {
    return process_dequeue_ready();
}
```
- Identical to round-robin at scheduler level
- Process manager's priority-ordered queue ensures highest priority first
- Strict priority ordering maintained by process manager

#### Priority Round-Robin (`select_priority_rr`)
```c
static process_t *select_priority_rr(void) {
    process_t *next = process_dequeue_ready();
    return next;
}
```
- Currently simplified implementation
- Would require tracking position within each priority level
- Future enhancement: maintain separate queues per priority

#### FCFS (`select_fcfs`)
```c
static process_t *select_fcfs(void) {
    return process_dequeue_ready();
}
```
- First process in queue runs
- Typically combined with disabled preemption
- Best for batch processing

**Key Insight**: The process manager's priority-ordered ready queue does most of the work. The scheduler just needs to dequeue and optionally re-enqueue.

### Context Switching: `scheduler_switch_context()`

```c
void scheduler_switch_context(process_t *from, process_t *to)
```

**Context Switch Sequence:**

1. **Save Old Process** (if `from` != NULL):
   ```c
   save_context(from);
   if (from->state != PROC_STATE_TERMINATED) {
       process_set_state(from->pid, PROC_STATE_READY);
       process_enqueue_ready(from);
   }
   ```
   - Save CPU registers to PCB
   - Move to READY state (unless terminated)
   - Return to ready queue

2. **Load New Process** (if `to` != NULL):
   ```c
   process_set_state(to->pid, PROC_STATE_CURRENT);
   restore_context(to);
   ```
   - Set as CURRENT process
   - Restore CPU registers from PCB

**Context Save/Restore (Simplified Educational Implementation):**

```c
static void save_context(process_t *proc) {
    // In real OS, would execute:
    // asm volatile("mov %%eax, %0" : "=r"(proc->context.eax));
    // asm volatile("mov %%ebx, %0" : "=r"(proc->context.ebx));
    // ... for all registers
    
    // For educational OS, context is conceptual
}

static void restore_context(process_t *proc) {
    // In real OS, would execute:
    // asm volatile("mov %0, %%eax" : : "r"(proc->context.eax));
    // asm volatile("mov %0, %%esp" : : "r"(proc->context.esp));
    // asm volatile("jmp *%0" : : "r"(proc->context.eip));
    
    // For educational OS, context is conceptual
}
```

**Real Implementation Would Include:**
- General purpose registers: EAX, EBX, ECX, EDX, ESI, EDI
- Stack pointers: ESP, EBP
- Instruction pointer: EIP
- Segment registers: CS, DS, ES, FS, GS, SS
- Flags register: EFLAGS
- FPU state (if used)
- SSE state (if used)

### Aging Mechanism: `scheduler_check_aging()`

```c
void scheduler_check_aging(void)
```

**Aging Algorithm:**

1. **Check Enabled**: Return if aging disabled
2. **Iterate All Processes**: Loop through process table
3. **For Each READY Process**:
   ```c
   proc->age++;  // Increment age counter
   
   if (proc->age >= sched_config.aging_threshold) {
       process_boost_priority(proc->pid);
       process_reset_age(proc->pid);
       sched_stats.total_aging_boosts++;
   }
   ```
4. **Log Boosts**: Print information for each aged process

**Aging Strategy:**
- Only READY processes age (waiting processes don't)
- Age increments on each check (every `aging_boost_interval` ticks)
- Boost occurs at threshold (default: 100 ticks)
- Age resets after boost to prevent double-boosting

**Starvation Prevention:**
```
Time 0:   P1 (HIGH) | P2 (NORMAL) | P3 (LOW)
          ▲         Ready          Ready
          Currently running

Time 100: P1 (HIGH) | P2 (NORMAL) | P3 (LOW, age=100)
          ▲         Scheduled      Starving!
          
Aging:    P1 (HIGH) | P2 (NORMAL) | P3 (NORMAL ← boosted)
          ▲         Ready          Now has priority!
```

---

## Configuration Management

### Quantum Clamping

```c
void scheduler_set_quantum(uint32_t quantum) {
    if (quantum < sched_config.min_quantum) {
        quantum = sched_config.min_quantum;
    }
    if (quantum > sched_config.max_quantum) {
        quantum = sched_config.max_quantum;
    }
    sched_config.default_quantum = quantum;
}
```

**Rationale:**
- Prevents too-small quantum (excessive overhead)
- Prevents too-large quantum (poor responsiveness)
- Ensures reasonable scheduling behavior

### Runtime Policy Changes

```c
void scheduler_set_policy(sched_policy_t policy) {
    sched_config.policy = policy;
}
```

**Implications:**
- Policy change takes effect on next `scheduler_select_next_process()` call
- No need to reorganize ready queue (process manager handles order)
- Seamless transition between policies

---

## Statistics and Monitoring

### Performance Metrics

```c
typedef struct {
    uint32_t total_context_switches;  // Overhead indicator
    uint32_t total_ticks;             // Uptime
    uint32_t idle_ticks;              // Underutilization
    uint32_t total_aging_boosts;      // Fairness metric
    uint32_t preemptions;             // Forced switches
    uint32_t voluntary_yields;        // Cooperative switches
} sched_stats_t;
```

**CPU Utilization Calculation:**
```c
uint32_t busy_ticks = sched_stats.total_ticks - sched_stats.idle_ticks;
uint32_t utilization = (busy_ticks * 100) / sched_stats.total_ticks;
```

**Metrics Interpretation:**

| Metric               | High Value Indicates               |
|----------------------|------------------------------------|
| Context Switches     | High scheduling overhead           |
| Idle Ticks           | Under-utilized system              |
| Aging Boosts         | Priority imbalance or starvation   |
| Preemptions          | Time-sharing workload              |
| Voluntary Yields     | Cooperative workload               |

---

## Integration Points

### With Process Manager

```c
/* Dependencies on process manager API */
process_t *process_get_current(void);        // Current process
process_t *process_dequeue_ready(void);      // Get next ready
void process_enqueue_ready(process_t *proc); // Return to queue
void process_set_state(uint32_t pid, proc_state_t state);
void process_boost_priority(uint32_t pid);
void process_reset_age(uint32_t pid);
process_t *process_get_by_pid(uint32_t pid);
uint32_t process_count(void);
```

**Coupling Level**: Tight
- Scheduler depends heavily on process manager
- Process manager is unaware of scheduler (loose coupling upward)

### With Timer Hardware

```c
/* Timer interrupt handler (hypothetical) */
void timer_isr(void) {
    scheduler_tick();
    // EOI (End of Interrupt) to PIC
}
```

**Typical Timer Configuration:**
- Frequency: 100 Hz (10ms per tick)
- Quantum of 100 ticks = 1 second
- Aging check every 50 ticks = 500ms

---

## Design Patterns Used

### 1. Strategy Pattern
- Multiple scheduling policies (strategies)
- Runtime policy selection
- Common interface (`select_next_process()`)

### 2. Singleton Pattern
- Single global scheduler instance
- Static state variables
- No constructor (just init function)

### 3. Observer Pattern
- Scheduler observes timer ticks
- Statistics track scheduler events
- No explicit observer registration (direct calls)

---

## Performance Analysis

### Time Complexity

| Operation                    | Complexity | Notes                        |
|------------------------------|------------|------------------------------|
| `scheduler_tick()`           | O(1)       | Amortized; periodic O(n)     |
| `scheduler_schedule()`       | O(1)       | Uses ready queue             |
| `scheduler_switch_context()` | O(1)       | Fixed register count         |
| `scheduler_check_aging()`    | O(n)       | Scans all processes          |
| `scheduler_set_policy()`     | O(1)       | Just updates flag            |

### Space Complexity

- Scheduler state: ~100 bytes (config + stats)
- Per-process overhead: 0 bytes (uses process manager's structures)
- Total: O(1) - constant space

### Optimization Opportunities

1. **Aging Optimization**: Only scan READY processes
   ```c
   // Instead of scanning all processes
   for (pid = 1; pid <= MAX_PID; pid++) {
       proc = process_get_by_pid(pid);
       if (proc && proc->state == PROC_STATE_READY) {
           // Age this process
       }
   }
   
   // Could maintain separate aging list
   for (proc in ready_list) {
       // Age only ready processes
   }
   ```

2. **Multi-Level Queue**: Separate queues per priority
   ```c
   // Current: Single priority-ordered queue
   // Optimization: Array of queues
   queue_t ready_queues[NUM_PRIORITIES];
   
   // O(1) selection within priority level
   for (int i = HIGHEST_PRIORITY; i >= LOWEST_PRIORITY; i--) {
       if (!queue_empty(&ready_queues[i])) {
           return queue_dequeue(&ready_queues[i]);
       }
   }
   ```

3. **Tick Batching**: Reduce tick processing overhead
   ```c
   // Instead of processing every tick
   if ((current_tick % TICK_BATCH_SIZE) == 0) {
       scheduler_schedule();
   }
   ```

---

## Testing Strategy

### Unit Tests (in `test_scheduler()`)

1. **Process Selection**: Verify policy selects correct process
2. **Tick Simulation**: Ensure quantum decrements properly
3. **Context Switch**: Verify state transitions
4. **Aging**: Test priority boosting after threshold
5. **Policy Changes**: Confirm runtime reconfiguration
6. **Quantum Configuration**: Test min/max clamping
7. **Preemption Control**: Verify enable/disable works
8. **Statistics**: Validate counter updates

### Integration Tests

```c
// Create test workload
for (int i = 0; i < 10; i++) {
    process_create(...);
}

// Run for 1000 ticks
for (int i = 0; i < 1000; i++) {
    scheduler_tick();
}

// Verify:
// - All processes got CPU time
// - Context switches occurred
// - No crashes or hangs
// - Statistics make sense
```

---

## Known Limitations

1. **Simplified Context Switch**
   - Doesn't actually save/restore registers
   - Educational implementation only
   - Real OS would use assembly

2. **No Multi-Core Support**
   - Single CPU scheduler
   - No load balancing
   - No CPU affinity

3. **Aging Scan Performance**
   - O(n) scan every 50 ticks
   - Could be slow with many processes
   - Optimization: maintain aging list

4. **Fixed Quantum Granularity**
   - Quantum in ticks, not microseconds
   - Depends on timer frequency
   - No sub-tick precision

5. **Priority Round-Robin Incomplete**
   - Simplified implementation
   - Doesn't track position in priority level
   - Future enhancement needed

---

## Future Enhancements

### 1. Real Context Switching
```asm
; Save context
push %eax
push %ebx
; ... all registers
mov %esp, [current_proc->context.esp]

; Restore context
mov [next_proc->context.esp], %esp
pop %ebx
pop %eax
jmp *[next_proc->context.eip]
```

### 2. Multi-Level Feedback Queue (MLFQ)
```c
// Processes move between priority levels based on behavior
// Interactive processes get high priority
// CPU-bound processes get lower priority
```

### 3. Completely Fair Scheduler (CFS)
```c
// Track virtual runtime for each process
// Select process with minimum vruntime
// Ensures fairness over time
```

### 4. SMP (Symmetric Multiprocessing) Support
```c
typedef struct {
    process_t *current[NUM_CPUS];
    queue_t ready_queues[NUM_CPUS];
    // Load balancing logic
} smp_scheduler_t;
```

### 5. Real-Time Scheduling
```c
// Hard real-time: EDF (Earliest Deadline First)
// Soft real-time: Rate Monotonic Scheduling
// Priority inheritance to prevent priority inversion
```

---

## References and Further Reading

1. **Operating System Concepts** (Silberschatz et al.)
   - Chapter 5: CPU Scheduling

2. **Modern Operating Systems** (Tanenbaum)
   - Chapter 2: Processes and Threads

3. **Linux Kernel Development** (Love)
   - Chapter 4: Process Scheduling

4. **The Design of the UNIX Operating System** (Bach)
   - Chapter 8: Process Scheduling

5. **Real-Time Systems** (Liu)
   - Scheduling algorithms for real-time systems

---

## Conclusion

The kacchiOS scheduler provides a solid foundation for process scheduling with:
- Multiple scheduling policies
- Preemptive multitasking
- Priority aging for fairness
- Comprehensive statistics
- Runtime configurability

While simplified for educational purposes, the design mirrors production schedulers and demonstrates key scheduling concepts effectively.
