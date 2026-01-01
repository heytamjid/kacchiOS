# Memory Manager Documentation

## Overview

The Memory Manager for kacchiOS provides comprehensive memory allocation and management services for both heap and stack memory. It supports dynamic memory allocation similar to `malloc/free` and manages per-process stacks.

## Features Implemented

### Core Features
- ✅ **Heap Memory Allocation** - Dynamic memory allocation with `kmalloc()`
- ✅ **Heap Memory Deallocation** - Free allocated memory with `kfree()`
- ✅ **Stack Allocation** - Per-process stack allocation
- ✅ **Stack Deallocation** - Free process stacks
- ✅ **Memory Statistics** - Track memory usage and allocations
- ✅ **Defragmentation** - Merge free blocks to reduce fragmentation

### Advanced Features (Good to Have)
- ✅ **Optimized Memory Allocation** - First-fit algorithm with block splitting
- ✅ **calloc** - Allocate and zero-initialize memory
- ✅ **realloc** - Reallocate memory blocks
- ✅ **Automatic Merging** - Adjacent free blocks are automatically merged

## Architecture

### Memory Layout

```
0x000000 - 0x100000  : Kernel code/data (1 MB)
0x200000 - 0x2000000 : Heap (30 MB)
0x2000000+           : Stack region (for process stacks)
```

### Data Structures

#### Heap Block (`heap_block_t`)
```c
typedef struct heap_block {
    void *address;          // Starting address of the block
    size_t size;            // Size of the block in bytes
    uint8_t is_free;        // 1 if free, 0 if allocated
    struct heap_block *next; // Next block in the list
} heap_block_t;
```

#### Stack Descriptor (`stack_descriptor_t`)
```c
typedef struct stack_descriptor {
    void *base;             // Base address of the stack
    void *top;              // Current top of the stack
    size_t size;            // Total size of the stack
    uint32_t pid;           // Process ID owning this stack
    uint8_t is_free;        // 1 if free, 0 if allocated
} stack_descriptor_t;
```

#### Memory Statistics (`memory_stats_t`)
```c
typedef struct memory_stats {
    size_t total_heap;      // Total heap memory
    size_t used_heap;       // Used heap memory
    size_t free_heap;       // Free heap memory
    size_t total_stacks;    // Total stack memory allocated
    uint32_t num_allocations; // Number of active allocations
    uint32_t num_stacks;    // Number of active stacks
} memory_stats_t;
```

## API Reference

### Initialization

#### `void memory_init(void)`
Initializes the memory manager. Must be called once during kernel startup.

**Example:**
```c
memory_init();
```

### Heap Memory Functions

#### `void *kmalloc(size_t size)`
Allocates a block of memory from the heap.

**Parameters:**
- `size`: Number of bytes to allocate

**Returns:**
- Pointer to allocated memory, or `NULL` if allocation fails

**Example:**
```c
void *buffer = kmalloc(1024);  // Allocate 1KB
if (buffer != NULL) {
    // Use the buffer
}
```

#### `void kfree(void *ptr)`
Frees a previously allocated block of memory.

**Parameters:**
- `ptr`: Pointer to memory block to free

**Example:**
```c
kfree(buffer);
```

#### `void *krealloc(void *ptr, size_t new_size)`
Reallocates a memory block to a new size.

**Parameters:**
- `ptr`: Pointer to previously allocated memory
- `new_size`: New size in bytes

**Returns:**
- Pointer to reallocated memory, or `NULL` if reallocation fails

**Example:**
```c
void *new_buffer = krealloc(buffer, 2048);  // Resize to 2KB
```

#### `void *kcalloc(size_t num, size_t size)`
Allocates memory for an array and initializes all bytes to zero.

**Parameters:**
- `num`: Number of elements
- `size`: Size of each element

**Returns:**
- Pointer to allocated and zeroed memory, or `NULL` if allocation fails

**Example:**
```c
int *array = (int*)kcalloc(100, sizeof(int));  // Array of 100 ints, all zero
```

### Stack Memory Functions

#### `void *stack_alloc(uint32_t pid)`
Allocates a stack for a process.

**Parameters:**
- `pid`: Process ID

**Returns:**
- Pointer to stack top (stacks grow downward), or `NULL` if allocation fails

**Example:**
```c
void *stack = stack_alloc(process_id);
```

#### `void stack_free(uint32_t pid)`
Frees a process stack.

**Parameters:**
- `pid`: Process ID

**Example:**
```c
stack_free(process_id);
```

#### `void *stack_get_base(uint32_t pid)`
Gets the base address of a process stack.

**Parameters:**
- `pid`: Process ID

**Returns:**
- Pointer to stack base, or `NULL` if not found

#### `void *stack_get_top(uint32_t pid)`
Gets the top address of a process stack.

**Parameters:**
- `pid`: Process ID

**Returns:**
- Pointer to stack top, or `NULL` if not found

### Utility Functions

#### `void memory_get_stats(memory_stats_t *stats)`
Retrieves current memory statistics.

**Parameters:**
- `stats`: Pointer to `memory_stats_t` structure to fill

**Example:**
```c
memory_stats_t stats;
memory_get_stats(&stats);
serial_puts("Used heap: ");
serial_put_dec(stats.used_heap);
```

#### `void memory_print_stats(void)`
Prints formatted memory statistics to serial output.

**Example:**
```c
memory_print_stats();
```

#### `void memory_defragment(void)`
Manually triggers heap defragmentation by merging adjacent free blocks.

**Example:**
```c
memory_defragment();
```

## Algorithms

### First-Fit Allocation
The memory manager uses a first-fit algorithm to find free blocks:
1. Search through the block list sequentially
2. Return the first block that is large enough
3. If no suitable block is found, attempt defragmentation and try again

### Block Splitting
When allocating memory, if a free block is significantly larger than needed:
1. Allocate the requested size from the beginning of the block
2. Create a new free block with the remaining space
3. Link the new block into the block list

### Automatic Merging
When memory is freed:
1. Mark the block as free
2. Search for adjacent free blocks
3. Merge adjacent free blocks to create larger contiguous free space
4. This reduces fragmentation automatically

## Configuration

Key constants in [memory.h](memory.h):

```c
#define PAGE_SIZE           4096        // 4KB pages
#define HEAP_START          0x200000    // Heap starts at 2MB
#define HEAP_SIZE           0x1E00000   // 30MB heap
#define STACK_SIZE          0x4000      // 16KB per stack
#define MAX_BLOCKS          1024        // Max heap blocks
#define MAX_PROCESS_STACKS  32          // Max process stacks
```

## Testing

The kernel includes built-in memory tests accessible via commands:

### Commands
- `memstats` - Display current memory statistics
- `memtest` - Run comprehensive memory allocation tests

### Test Coverage
The `memtest` command tests:
1. Basic allocation and deallocation
2. Multiple simultaneous allocations
3. Freeing non-contiguous blocks
4. Reusing freed space
5. `calloc` with zero initialization
6. Stack allocation and deallocation

## Usage in Kernel

```c
#include "memory.h"

void kmain(void) {
    // Initialize memory manager
    memory_init();
    
    // Allocate heap memory
    void *data = kmalloc(256);
    
    // Allocate a process stack
    void *stack = stack_alloc(1);
    
    // Use the memory...
    
    // Free memory
    kfree(data);
    stack_free(1);
    
    // View statistics
    memory_print_stats();
}
```

## Error Handling

The memory manager handles various error conditions:

- **Out of Memory**: Returns `NULL` when allocation fails
- **Double Free**: Detects and warns about freeing already-free memory
- **Invalid Pointer**: Warns when attempting to free unallocated memory
- **Stack Exhaustion**: Returns `NULL` when all stack slots are used

All errors are logged to serial output with descriptive messages.

## Performance Characteristics

- **Allocation Time**: O(n) where n is the number of blocks (first-fit search)
- **Deallocation Time**: O(n) due to merging pass
- **Space Overhead**: ~24 bytes per allocation (block metadata)
- **Fragmentation**: Minimized through automatic merging and block splitting

## Future Enhancements

Potential improvements for advanced students:
- Best-fit or worst-fit allocation strategies
- Buddy system allocator
- Slab allocator for fixed-size objects
- Memory protection and bounds checking
- Memory mapped I/O support
- Virtual memory and paging

## Integration with Process Manager

The memory manager is designed to work seamlessly with the process manager:

```c
// Process creation
process_t *create_process(void) {
    process_t *proc = (process_t*)kmalloc(sizeof(process_t));
    proc->stack = stack_alloc(proc->pid);
    return proc;
}

// Process termination
void terminate_process(process_t *proc) {
    stack_free(proc->pid);
    kfree(proc);
}
```

## Files

- [memory.h](memory.h) - Memory manager interface and data structures
- [memory.c](memory.c) - Memory manager implementation
- [kernel.c](kernel.c) - Integration and test code

---

**kacchiOS Memory Manager** - A foundational component for process memory management
