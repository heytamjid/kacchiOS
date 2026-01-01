# Scheduler Testing Guide

## Simple Scheduler Simulation

kacchiOS now includes a **manual scheduler simulation** - no complex timer interrupts, just simple commands to test scheduling behavior.

## How It Works

1. **Scheduler is started automatically** when kernel boots
2. Use the `tick` command to manually advance time
3. Scheduler makes decisions based on policy and priority
4. Watch processes get scheduled in real-time!

## Commands

### Create Processes with Priority

```
create <name> <priority>
```

**Priority options:**
- `c` or `C` or `0` = CRITICAL (highest)
- `h` or `H` or `1` = HIGH
- `n` or `N` or `2` = NORMAL (default)
- `l` or `L` or `3` = LOW

**Examples:**
```
create Worker1 h        # High priority
create Worker2 n        # Normal priority
create Worker3 l        # Low priority
create Kernel c         # Critical priority
create Task1 1          # High (numeric)
create Task2 2          # Normal (numeric)
```

### Advance Scheduler Time

```
tick [n]                # Advance by n ticks (default: 1)
```

**Examples:**
```
tick                    # Advance by 1 tick
tick 10                 # Advance by 10 ticks
tick 100                # Advance by 100 ticks
```

### Monitor Scheduler

```
ps                      # Show all processes
schedstats              # Show scheduling statistics
schedconf               # Show configuration
```

## Testing Scenarios

### Scenario 1: Priority Scheduling

```bash
# Create processes with different priorities
create HighPri h
create NormalPri n
create LowPri l

# Check initial state
ps

# Advance scheduler
tick 50

# Check statistics
schedstats

# See which process got more CPU time
ps
```

### Scenario 2: Aging Mechanism

```bash
# Create high and low priority processes
create Important h
create Background l

# Advance many ticks to trigger aging
tick 150

# Background should get priority boost after 100 ticks
schedstats
ps
```

### Scenario 3: Round-Robin

```bash
# Change to round-robin policy
# (Currently policy is set in kernel init, but you can modify)

# Create equal priority processes
create A n
create B n
create C n

# Advance scheduler - should rotate between them
tick 100

# Check context switches
schedstats
```

### Scenario 4: Testing Preemption

```bash
# Create processes
create Fast h
create Slow l

# Advance by quantum amount
tick 100

# Should see preemption counter increase
schedstats
```

## Understanding Output

### Process State (ps command)
- **READY**: Waiting to be scheduled
- **CURRENT**: Currently running
- **BLOCKED**: Waiting for event

### Scheduler Stats (schedstats)
- **Total Ticks**: How many ticks have passed
- **Context Switches**: How many times scheduler switched processes
- **Preemptions**: Forced switches (quantum expired)
- **Aging Boosts**: Priority increases due to starvation prevention
- **CPU Utilization**: Percentage of time CPU was busy

## Configuration

Default scheduler settings (in kernel.c):
- **Policy**: Priority-Based
- **Quantum**: 100 ticks
- **Aging Threshold**: 100 ticks
- **Aging Enabled**: Yes
- **Preemption Enabled**: Yes

## Tips for Testing

1. **Start Simple**: Create 2-3 processes with different priorities
2. **Small Ticks**: Use `tick 10` to see gradual behavior
3. **Watch Stats**: Run `schedstats` frequently to see changes
4. **Check Ages**: Use `ps` to see process ages increasing
5. **Test Aging**: Create low-priority process, then `tick 150` to see boost

## Example Session

```
kacchiOS> create Worker1 h
Created process 'Worker1' with PID 1 and priority 1

kacchiOS> create Worker2 l
Created process 'Worker2' with PID 2 and priority 3

kacchiOS> ps
PID  Name          State    Pri Age  CPU    Stack    
---- ------------- -------- --- ---- ------ --------
1    Worker1       READY    1   0    0      0x00200000
2    Worker2       READY    3   0    0      0x00204000

kacchiOS> tick 50
Advancing scheduler by 50 tick(s)
[SCHEDULER] Switching: IDLE -> Worker1 (PID 1)
[SCHEDULER] Time quantum expired for PID 1

kacchiOS> schedstats

=== Scheduler Statistics ===
Total Ticks:          50
Idle Ticks:           0
Context Switches:     2
Preemptions:          1
Voluntary Yields:     0
Aging Boosts:         0
CPU Utilization:      100%
===========================

kacchiOS> tick 100
Advancing scheduler by 100 tick(s)
[SCHEDULER] Aging: Boosting priority of PID 2 (age=100)

kacchiOS> ps
PID  Name          State    Pri Age  CPU    Stack    
---- ------------- -------- --- ---- ------ --------
1    Worker1       CURRENT  1   0    100    0x00200000
2    Worker2       READY    2   0    50     0x00204000
                           ^ Boosted from 3!
```

## Why Manual Ticking?

This approach is **much simpler** than real timer interrupts because:
- ✅ No interrupt handling code needed
- ✅ No PIC (Programmable Interrupt Controller) setup
- ✅ No IDT (Interrupt Descriptor Table)
- ✅ Full control over timing for testing
- ✅ Can pause and inspect state anytime
- ✅ Easier to debug and understand

Perfect for **learning and testing** scheduler algorithms!
