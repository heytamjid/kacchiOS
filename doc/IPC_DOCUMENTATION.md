# Inter-Process Communication (IPC) - Message Passing

## Overview

kacchiOS implements a **simple message passing** system for inter-process communication. This allows processes to exchange data and coordinate their activities without sharing memory directly.

---

## Architecture

### Message Queue Model

Each process has its own **message queue**:
- **Queue Size**: 16 messages (fixed)
- **Message Type**: 32-bit unsigned integer (uint32_t)
- **Order**: FIFO (First In, First Out)
- **Blocking**: Receivers can block when queue is empty

### Message Flow

```
Sender Process                    Receiver Process
     |                                   |
     | send_message(dest_pid, msg)       |
     |---------------------------------->|
     |                                   | msg added to queue
     |                                   |
     |                           recv_message()
     |                                   | msg removed from queue
     |                                   |
```

---

## Data Structures

### Process Control Block - IPC Fields

```c
typedef struct process {
    // ... other fields ...
    
    /* IPC - Message passing */
    uint32_t message_queue[16];     // Message queue (16 messages max)
    uint32_t msg_count;             // Number of messages in queue
    uint32_t waiting_for_msg;       // 1 if blocked waiting for message
    
    // ... other fields ...
} process_t;
```

### Key Fields

- **message_queue[16]**: Array storing up to 16 messages
- **msg_count**: Current number of messages (0-16)
- **waiting_for_msg**: Flag indicating process is blocked waiting for a message

---

## API Functions

### 1. Send Message

```c
int process_send_message(uint32_t dest_pid, uint32_t message);
```

**Purpose**: Send a message to another process

**Parameters**:
- `dest_pid`: Process ID of the destination process
- `message`: 32-bit unsigned integer message

**Return Values**:
- `0`: Success
- `-1`: Failure (process not found or queue full)

**Behavior**:
1. Validates destination process exists
2. Checks if destination's message queue has space
3. Appends message to destination's queue
4. If destination was blocked waiting for a message, unblocks it
5. Returns success/failure status

**Example**:
```c
// Send value 12345 to process with PID 3
int result = process_send_message(3, 12345);
if (result == 0) {
    serial_puts("Message sent successfully\n");
}
```

---

### 2. Receive Message

```c
int process_receive_message(uint32_t *message);
```

**Purpose**: Receive a message from the current process's queue

**Parameters**:
- `message`: Pointer to store the received message

**Return Values**:
- `0`: Success (message received)
- `-1`: Failure (no current process or queue empty)

**Behavior**:
1. Checks if there's a current running process
2. If queue is empty:
   - Sets `waiting_for_msg` flag
   - Blocks the current process
   - Returns -1 (process will be unblocked when message arrives)
3. If queue has messages:
   - Retrieves first message (FIFO)
   - Shifts remaining messages down
   - Decrements `msg_count`
   - Returns 0

**Example**:
```c
uint32_t message;
int result = process_receive_message(&message);
if (result == 0) {
    serial_puts("Received: ");
    serial_put_dec(message);
    serial_puts("\n");
}
```

---

### 3. Check for Messages

```c
int process_has_message(uint32_t pid);
```

**Purpose**: Check if a process has messages in its queue

**Parameters**:
- `pid`: Process ID to check

**Return Values**:
- `1`: Process has messages
- `0`: Process has no messages or doesn't exist

**Example**:
```c
if (process_has_message(3)) {
    serial_puts("Process 3 has messages\n");
}
```

---

## Command Line Interface

### `send <pid> <message>` - Send Message to Process

Sends a 32-bit integer message to the specified process.

**Syntax**:
```bash
send <dest_pid> <message_value>
```

**Parameters**:
- `dest_pid`: Destination process ID
- `message_value`: Integer message (0 to 4294967295)

**Examples**:
```bash
kacchiOS> send 2 12345
[IPC] Message 12345 sent to PID 2

kacchiOS> send 3 999
[IPC] Message 999 sent to PID 3

kacchiOS> send 5 100
[IPC] Destination process not found
```

**Error Cases**:
- Process not found
- Message queue full (16 messages)

---

### `recv <pid>` - Receive Message from Process Queue

Receives and removes the first message from the specified process's queue.

**Syntax**:
```bash
recv <pid>
```

**Parameters**:
- `pid`: Process ID whose queue to read

**Examples**:
```bash
kacchiOS> recv 2
[IPC] Received message from PID 2: 12345 (2 remaining)

kacchiOS> recv 2
[IPC] Received message from PID 2: 67890 (1 remaining)

kacchiOS> recv 2
[IPC] No messages in queue for PID 2
```

**Notes**:
- Messages are retrieved in FIFO order
- Message is removed from queue after reading
- Shows how many messages remain

---

### `msgs <pid>` - Show Message Queue Status

Displays the complete message queue for a process.

**Syntax**:
```bash
msgs <pid>
```

**Parameters**:
- `pid`: Process ID to inspect

**Example Output**:
```bash
kacchiOS> msgs 2

=== Message Queue for PID 2 ('Worker1') ===
Queue Size: 3 / 16
Messages:
  [0] 12345
  [1] 67890
  [2] 999
Waiting for message: NO
===================
```

**Empty Queue Example**:
```bash
kacchiOS> msgs 3

=== Message Queue for PID 3 ('TaskB') ===
Queue Size: 0 / 16
(Queue is empty)
Waiting for message: YES
===================
```

**Information Shown**:
- Process name and PID
- Queue size (current / maximum)
- All messages in queue (indexed)
- Whether process is blocked waiting for a message

---

## Message Passing Semantics

### Asynchronous Send

Sending is **non-blocking**:
- Sender doesn't wait for receiver to read the message
- Message is queued immediately
- Sender continues execution
- Only fails if queue is full

### Blocking Receive

Receiving can **block**:
- If queue has messages: immediate return with message
- If queue is empty: process blocks until message arrives
- When blocked, process state changes to `BLOCKED`
- Process is automatically unblocked when message is sent

### FIFO Ordering

Messages are delivered in **order sent**:
- First message sent is first message received
- Queue maintains insertion order
- No priority or reordering

---

## Use Cases and Scenarios

### 1. Producer-Consumer Pattern

**Setup**:
```bash
create Producer high 1000
create Consumer normal 2000
```

**Producer sends data**:
```bash
send 2 100    # Send data item 100 to Consumer
send 2 200    # Send data item 200 to Consumer
send 2 300    # Send data item 300 to Consumer
```

**Consumer receives data**:
```bash
msgs 2        # Check queue (should show 3 messages)
recv 2        # Get 100
recv 2        # Get 200
recv 2        # Get 300
```

---

### 2. Request-Response Pattern

**Setup**:
```bash
create Client critical 500
create Server normal 1000
```

**Client sends request**:
```bash
send 2 1001   # Request ID 1001 to Server
```

**Server processes and responds**:
```bash
recv 2        # Server receives request 1001
send 1 2001   # Server sends response 2001 to Client
```

**Client receives response**:
```bash
recv 1        # Client receives response 2001
```

---

### 3. Synchronization via Messages

**Processes waiting for signal**:
```bash
create Task1 normal 500
create Task2 normal 500
create Coordinator high 200
```

**Coordinator sends start signals**:
```bash
send 1 1      # Signal Task1 to start
send 2 1      # Signal Task2 to start
```

**Tasks check for signal**:
```bash
msgs 1        # Check if Task1 has signal
msgs 2        # Check if Task2 has signal
```

---

### 4. Broadcast Pattern

**One sender, multiple receivers**:
```bash
create Broadcaster high 300
create Receiver1 normal 500
create Receiver2 normal 500
create Receiver3 normal 500
```

**Send to all receivers**:
```bash
send 2 100    # Send to Receiver1
send 3 100    # Send to Receiver2
send 4 100    # Send to Receiver3
```

---

### 5. Message Queue Overflow

**Testing queue limits**:
```bash
create Target normal 1000

# Send 16 messages (fill queue)
send 2 1
send 2 2
send 2 3
...
send 2 16

# This will fail (queue full)
send 2 17
[IPC] Message queue full
```

**Drain queue**:
```bash
recv 2        # Get message 1
recv 2        # Get message 2
...
recv 2        # Get message 16

# Now queue is empty
send 2 17     # This will succeed
```

---

## Blocking and Unblocking

### Blocking Scenario

```bash
# Create a process
create Waiter normal 1000

# Try to receive (no messages)
recv 2
[IPC] No messages in queue for PID 2

# In actual implementation with receive_message():
# Process would be BLOCKED
msgs 2
Waiting for message: YES

# Send message unblocks the process
send 2 12345
[IPC] Message 12345 sent to PID 2
# Process automatically unblocks and can receive
```

### Unblocking Behavior

When a message is sent to a blocked process:
1. Message is added to queue
2. `waiting_for_msg` flag is cleared
3. Process state changes from `BLOCKED` to `READY`
4. Process is added back to ready queue
5. Scheduler will eventually schedule the process
6. Process can then successfully receive the message

---

## Message Encoding

Since messages are 32-bit integers, you can encode different types of data:

### 1. Simple Values
```bash
send 2 12345         # Just a number
send 2 0             # Zero value
send 2 4294967295    # Maximum 32-bit value
```

### 2. Command Codes
```bash
# Define commands as numbers
send 2 1    # Command: START
send 2 2    # Command: STOP
send 2 3    # Command: PAUSE
send 2 4    # Command: RESUME
```

### 3. Bit Fields (conceptual)
```
Bits 31-24: Message type
Bits 23-16: Priority
Bits 15-0:  Data payload
```

Example:
```bash
send 2 16777216      # Type=1, Priority=0, Data=0
send 2 33619968      # Type=2, Priority=2, Data=512
```

### 4. Error Codes
```bash
send 2 0       # Success
send 2 1       # Error: Not found
send 2 2       # Error: Invalid
send 2 3       # Error: Timeout
```

---

## Integration with Scheduler

### Message Passing and Context Switching

1. **Sending doesn't affect scheduling**:
   - Sender remains in current state
   - No context switch triggered
   - Exception: If sender's quantum expires during send

2. **Receiving can trigger scheduling**:
   - If queue empty → process blocks → context switch
   - Scheduler selects next ready process
   - Blocked process removed from scheduling

3. **Unblocking affects scheduling**:
   - Blocked process becomes READY
   - May preempt current process if higher priority
   - Scheduler makes preemption decision

### Example Timeline

```
Tick 0:   Process A (high) created, running
Tick 100: Process B (normal) created, ready
Tick 150: A sends message to B
Tick 150: B receives message (has message, doesn't block)
Tick 200: A's quantum expires, context switch to B
Tick 350: B's quantum expires, context switch to A
Tick 400: A tries to receive (no messages)
Tick 400: A blocks, context switch to B
Tick 500: B sends message to A
Tick 500: A unblocks, becomes READY
Tick 500: A has higher priority, preempts B
Tick 500: Context switch to A
```

---

## Limitations

### Current Implementation

1. **Fixed Queue Size**: 16 messages maximum
   - No dynamic allocation
   - Sender fails if queue full
   - No overflow handling beyond rejection

2. **Simple Message Type**: Only 32-bit integers
   - Cannot send complex data structures
   - No string messages
   - No variable-length messages

3. **No Message Priorities**: FIFO only
   - Urgent messages can't jump queue
   - No express lanes

4. **No Timeouts**: Blocking is indefinite
   - Process waits forever for message
   - No timeout mechanism
   - Can't detect deadlocks automatically

5. **No Acknowledgments**: Fire and forget
   - Sender doesn't know if message was received
   - No delivery confirmation
   - No return receipts

6. **No Selective Receive**: Get next message only
   - Can't skip messages
   - Can't filter by message type
   - Must process in order

---

## Advanced Scenarios

### Deadlock Example

```bash
# Create two processes
create P1 normal 1000
create P2 normal 1000

# Simulate deadlock scenario:
# P1 waits for message from P2
# P2 waits for message from P1
# Neither sends first → deadlock

# This must be avoided by design
```

**Prevention**: Ensure at least one process sends before receiving.

---

### Round-Trip Communication

```bash
create Client critical 500
create Server normal 1000

# Client sends request
send 2 1001

# Server receives and processes
recv 2              # Gets 1001

# Server sends response
send 1 2001

# Client receives response
recv 1              # Gets 2001
```

**Latency**: Depends on scheduling and priority levels.

---

### Multi-Stage Pipeline

```bash
create Stage1 normal 1000
create Stage2 normal 1000
create Stage3 normal 1000

# Data flows through pipeline
send 1 100          # Input to Stage1
recv 1              # Stage1 processes
send 2 101          # Stage1 to Stage2
recv 2              # Stage2 processes
send 3 102          # Stage2 to Stage3
recv 3              # Stage3 processes
```

---

## Comparison with Other IPC Mechanisms

### Message Passing vs. Shared Memory

| Feature | Message Passing | Shared Memory |
|---------|----------------|---------------|
| Data Transfer | Copy data to queue | Direct memory access |
| Synchronization | Implicit (blocking) | Explicit (locks/semaphores) |
| Safety | Protected (separate queues) | Risky (race conditions) |
| Performance | Slower (copying) | Faster (no copying) |
| Implementation | Simpler | More complex |
| Isolation | High (no shared state) | Low (shared state) |

kacchiOS uses message passing for **safety and simplicity**.

---

### Message Passing vs. Signals

| Feature | Message Passing | Signals |
|---------|----------------|---------|
| Data | 32-bit value | No data (just notification) |
| Queuing | Yes (16 messages) | Usually no queuing |
| Ordering | FIFO guaranteed | May not preserve order |
| Blocking | Can block on receive | Usually asynchronous |
| Use Case | Data exchange | Event notification |

Message passing is richer than signals.

---

## Performance Considerations

### Costs

1. **Memory**: Each process uses 16 × 4 bytes = 64 bytes for queue
2. **Copying**: Message is copied into destination queue
3. **Queue Management**: Shifting messages on receive
4. **Context Switch**: Blocking receive triggers context switch

### Optimization Tips

1. **Minimize Blocking**: Check queue before receiving
2. **Batch Messages**: Send multiple related values
3. **Use Priorities**: High-priority senders/receivers get faster service
4. **Avoid Queue Full**: Receivers should consume messages regularly

---

## Debugging and Testing

### Checking Message Flow

```bash
# Create processes
create Sender high 500
create Receiver normal 1000

# Send messages
send 2 111
send 2 222
send 2 333

# Check queue status
msgs 2
# Shows: 3 messages queued

# Receive one
recv 2
# Shows: Received 111, 2 remaining

# Check again
msgs 2
# Shows: 2 messages queued (222, 333)
```

### Testing Queue Overflow

```bash
create Target normal 1000

# Fill queue (16 messages)
send 2 1; send 2 2; send 2 3; send 2 4
send 2 5; send 2 6; send 2 7; send 2 8
send 2 9; send 2 10; send 2 11; send 2 12
send 2 13; send 2 14; send 2 15; send 2 16

# Try overflow
send 2 17
# Should fail: Message queue full

# Drain queue
recv 2
# Now send succeeds
send 2 17
```

### Observing Blocking

```bash
create Waiter normal 1000
msgs 2
# Shows: Waiting for message: NO

# (In actual code, call process_receive_message)
# Process would block

msgs 2
# Shows: Waiting for message: YES

# Unblock by sending
send 2 123

msgs 2
# Shows: Waiting for message: NO
# Shows: 1 message in queue
```

---

## Future Enhancements

Potential improvements to the IPC system:

1. **Variable-Length Messages**: Support messages larger than 32 bits
2. **Message Types**: Tagged messages with type identifiers
3. **Priority Messages**: Urgent messages jump queue
4. **Timeouts**: Timed blocking with timeout
5. **Broadcast**: Send to multiple processes at once
6. **Message Filters**: Selective receive by type
7. **Acknowledgments**: Delivery confirmation
8. **Named Channels**: Communicate via names instead of PIDs
9. **Message Groups**: Multicast to process groups
10. **Flow Control**: Backpressure when receiver is slow

---

## Example Session

```bash
kacchiOS> create Producer high 500
[SCHEDULER] Created process 'Producer' (PID 1, Priority: HIGH, Required Time: 500)

kacchiOS> create Consumer normal 1000
[SCHEDULER] Created process 'Consumer' (PID 2, Priority: NORMAL, Required Time: 1000)

kacchiOS> ps
========================================
    CPU SCHEDULER STATUS
========================================
System Ticks:     0
Current Process:  'Producer' (PID 1)
========================================

kacchiOS> send 2 12345
[IPC] Message 12345 sent to PID 2

kacchiOS> send 2 67890
[IPC] Message 67890 sent to PID 2

kacchiOS> msgs 2
=== Message Queue for PID 2 ('Consumer') ===
Queue Size: 2 / 16
Messages:
  [0] 12345
  [1] 67890
Waiting for message: NO
===================

kacchiOS> recv 2
[IPC] Received message from PID 2: 12345 (1 remaining)

kacchiOS> msgs 2
=== Message Queue for PID 2 ('Consumer') ===
Queue Size: 1 / 16
Messages:
  [0] 67890
Waiting for message: NO
===================

kacchiOS> recv 2
[IPC] Received message from PID 2: 67890 (0 remaining)

kacchiOS> recv 2
[IPC] No messages in queue for PID 2
```

---

## Conclusion

The kacchiOS IPC message passing system provides a simple, safe mechanism for inter-process communication. While basic compared to full-featured IPC systems, it demonstrates core concepts:

- **Asynchronous communication**: Sender and receiver operate independently
- **Blocking semantics**: Receivers can wait for messages
- **FIFO ordering**: Predictable message delivery
- **Process isolation**: No shared memory risks

This implementation serves as an excellent foundation for understanding how processes coordinate in operating systems.
