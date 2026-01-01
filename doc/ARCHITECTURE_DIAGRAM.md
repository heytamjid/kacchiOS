# Memory Manager Architecture Diagram

## System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                        kacchiOS                              │
│                                                              │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐            │
│  │   Kernel   │  │  Process   │  │ Scheduler  │            │
│  │            │  │  Manager   │  │            │            │
│  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘            │
│        │               │               │                    │
│        └───────────────┼───────────────┘                    │
│                        │                                    │
│                        ▼                                    │
│         ┌──────────────────────────────┐                   │
│         │     Memory Manager           │                   │
│         │  ┌────────────┬────────────┐ │                   │
│         │  │    Heap    │   Stack    │ │                   │
│         │  │  Manager   │  Manager   │ │                   │
│         │  └────────────┴────────────┘ │                   │
│         └──────────────────────────────┘                   │
│                        │                                    │
│                        ▼                                    │
│         ┌──────────────────────────────┐                   │
│         │     Physical Memory          │                   │
│         └──────────────────────────────┘                   │
└─────────────────────────────────────────────────────────────┘
```

## Heap Manager Internal Structure

```
┌─────────────────────────────────────────────────────────────┐
│                    Heap Manager                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  heap_blocks[] array (metadata)                              │
│  ┌────────┬────────┬────────┬────────┬─────┬────────┐      │
│  │ Block  │ Block  │ Block  │ Block  │ ... │ Block  │      │
│  │   0    │   1    │   2    │   3    │     │  1023  │      │
│  └───┬────┴───┬────┴───┬────┴───┬────┴─────┴────────┘      │
│      │        │        │        │                           │
│      ▼        ▼        ▼        ▼                           │
│  ┌────────────────────────────────────────┐                │
│  │  Each block contains:                  │                │
│  │  - address:  void*     (8 bytes)       │                │
│  │  - size:     size_t    (4 bytes)       │                │
│  │  - is_free:  uint8_t   (1 byte)        │                │
│  │  - next:     pointer   (8 bytes)       │                │
│  │  Total: ~24 bytes per block            │                │
│  └────────────────────────────────────────┘                │
│                                                              │
│  Heap Memory (30 MB at 0x200000)                            │
│  ┌──────────┬──────────┬──────────┬──────────┐            │
│  │  FREE    │ ALLOC    │  FREE    │ ALLOC    │            │
│  │  10KB    │  4KB     │  2KB     │  8KB     │ ...        │
│  └──────────┴──────────┴──────────┴──────────┘            │
│       ▲          ▲          ▲          ▲                    │
│       │          │          │          │                    │
│    Block 0    Block 1    Block 2    Block 3                │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Stack Manager Internal Structure

```
┌─────────────────────────────────────────────────────────────┐
│                    Stack Manager                             │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  stack_table[] array (metadata)                              │
│  ┌────────┬────────┬────────┬────────┬─────┬────────┐      │
│  │ Stack  │ Stack  │ Stack  │ Stack  │ ... │ Stack  │      │
│  │   0    │   1    │   2    │   3    │     │   31   │      │
│  └───┬────┴───┬────┴───┬────┴───┬────┴─────┴────────┘      │
│      │        │        │        │                           │
│      ▼        ▼        ▼        ▼                           │
│  ┌────────────────────────────────────────┐                │
│  │  Each descriptor contains:             │                │
│  │  - base:     void*     (stack base)    │                │
│  │  - top:      void*     (stack top)     │                │
│  │  - size:     size_t    (16KB)          │                │
│  │  - pid:      uint32_t  (process ID)    │                │
│  │  - is_free:  uint8_t   (status)        │                │
│  └────────────────────────────────────────┘                │
│                                                              │
│  Stack Memory (starts at 0x2000000)                         │
│  ┌──────────┬──────────┬──────────┬──────────┐            │
│  │  Stack   │  Stack   │  Stack   │   FREE   │            │
│  │  PID=1   │  PID=2   │  PID=5   │          │ ...        │
│  │  16KB    │  16KB    │  16KB    │  16KB    │            │
│  └──────────┴──────────┴──────────┴──────────┘            │
│       ▲          ▲          ▲          ▲                    │
│    (grows    (grows    (grows                               │
│     down)     down)     down)                               │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Allocation Algorithm Flow

```
kmalloc(size) called
       │
       ▼
┌─────────────────┐
│ Align size to   │
│ 4-byte boundary │
└────────┬────────┘
         │
         ▼
┌─────────────────────┐
│ Search for free     │────────┐
│ block (First Fit)   │        │
└────────┬────────────┘        │
         │ Found               │ Not Found
         ▼                     ▼
┌─────────────────┐   ┌──────────────────┐
│ Block large     │   │ Defragment heap  │
│ enough?         │   │ (merge blocks)   │
└────┬───────┬────┘   └────────┬─────────┘
     │ Yes   │ No              │
     │       │                 │
     │       ▼                 │
     │  ┌─────────────┐        │
     │  │ Split block │        │
     │  └──────┬──────┘        │
     │         │               │
     │         ▼               │
     │  ┌─────────────┐        │
     │  │ Create new  │        │
     │  │ free block  │        │
     │  └──────┬──────┘        │
     │         │               │
     └─────────┴───────────────┘
               │
               ▼
     ┌──────────────────┐
     │ Mark as ALLOCATED│
     └────────┬─────────┘
              │
              ▼
     ┌──────────────────┐
     │ Return pointer   │
     └──────────────────┘
```

## Deallocation & Merging Flow

```
kfree(ptr) called
       │
       ▼
┌─────────────────┐
│ Find block by   │
│ address         │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Mark as FREE    │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────┐
│ Merge adjacent free blocks:     │
│                                  │
│  Before:                         │
│  [ALLOC][FREE][FREE][ALLOC]      │
│                                  │
│  After:                          │
│  [ALLOC][  FREE   ][ALLOC]       │
│                                  │
└──────────────────────────────────┘
```

## Memory State Transitions

```
        Heap Block States
        
    ┌──────────┐
    │   FREE   │
    └────┬─────┘
         │ kmalloc()
         ▼
    ┌──────────┐
    │ ALLOCATED│
    └────┬─────┘
         │ kfree()
         ▼
    ┌──────────┐
    │   FREE   │
    └──────────┘
         │
         │ Merge if adjacent
         │ to other FREE block
         ▼
    ┌──────────┐
    │ MERGED   │
    │   FREE   │
    └──────────┘


        Stack States
        
    ┌──────────┐
    │   FREE   │
    │ (unused) │
    └────┬─────┘
         │ stack_alloc(pid)
         ▼
    ┌──────────┐
    │   IN USE │
    │ (PID=X)  │
    └────┬─────┘
         │ stack_free(pid)
         ▼
    ┌──────────┐
    │   FREE   │
    │ (zeroed) │
    └──────────┘
```

## Integration with Process Manager

```
Process Lifecycle & Memory
══════════════════════════

CREATE PROCESS
     │
     ├─► kmalloc(sizeof(process_t))      ◄── Allocate PCB
     │         │
     │         └─► Returns process_t*
     │
     ├─► stack_alloc(pid)                ◄── Allocate Stack
     │         │
     │         └─► Returns stack_top
     │
     ▼
PROCESS RUNNING
     │
     ├─► kmalloc(...)  ◄── Process allocates memory
     ├─► kfree(...)    ◄── Process frees memory
     │
     ▼
TERMINATE PROCESS
     │
     ├─► stack_free(pid)                 ◄── Free Stack
     │
     └─► kfree(process_ptr)              ◄── Free PCB
```

## Data Flow

```
User Code
    │
    │ kmalloc(1024)
    ▼
Memory Manager
    │
    ├─► Find free block (First Fit)
    ├─► Split if too large
    ├─► Mark as allocated
    ├─► Update statistics
    │
    │ returns: void*
    ▼
User Code
    │
    │ Use memory...
    │
    │ kfree(ptr)
    ▼
Memory Manager
    │
    ├─► Find block by address
    ├─► Mark as free
    ├─► Merge adjacent free blocks
    └─► Update statistics
```

---

## Legend

```
┌────────┐
│ Block  │  = Data structure / Component
└────────┘

    │      = Data flow / Control flow
    ▼

  ─────►   = Function call / Return

[ALLOC]    = Allocated block
[FREE]     = Free block
```

---

**kacchiOS Memory Manager** • Visual Architecture Reference
