# Memory Manager Quick Reference

## Initialization
```c
memory_init();  // Call once at kernel startup
```

## Heap Allocation

| Function | Description | Returns |
|----------|-------------|---------|
| `kmalloc(size)` | Allocate memory | Pointer or NULL |
| `kfree(ptr)` | Free memory | void |
| `kcalloc(num, size)` | Allocate & zero | Pointer or NULL |
| `krealloc(ptr, new_size)` | Resize allocation | Pointer or NULL |

## Stack Management

| Function | Description | Returns |
|----------|-------------|---------|
| `stack_alloc(pid)` | Allocate process stack | Stack top or NULL |
| `stack_free(pid)` | Free process stack | void |
| `stack_get_base(pid)` | Get stack base | Pointer or NULL |
| `stack_get_top(pid)` | Get stack top | Pointer or NULL |

## Utilities

| Function | Description |
|----------|-------------|
| `memory_print_stats()` | Display memory statistics |
| `memory_get_stats(&stats)` | Get statistics struct |
| `memory_defragment()` | Manual heap defragmentation |

## Configuration

| Constant | Value | Description |
|----------|-------|-------------|
| `HEAP_START` | 0x200000 | Heap start address (2MB) |
| `HEAP_SIZE` | 0x1E00000 | Heap size (30MB) |
| `STACK_SIZE` | 0x4000 | Per-process stack (16KB) |
| `MAX_BLOCKS` | 1024 | Max heap blocks |
| `MAX_PROCESS_STACKS` | 32 | Max concurrent stacks |

## Example Usage

```c
// Allocate and use memory
void *buffer = kmalloc(1024);
if (buffer) {
    memset(buffer, 0, 1024);
    // Use buffer...
    kfree(buffer);
}

// Allocate zeroed array
uint32_t *array = kcalloc(100, sizeof(uint32_t));

// Allocate process stack
void *stack = stack_alloc(process_id);

// Get statistics
memory_stats_t stats;
memory_get_stats(&stats);
```

## Interactive Commands

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `memstats` | Display memory statistics |
| `memtest` | Run allocation tests |
| `clear` | Clear screen |

## Error Handling

All allocation functions return NULL on failure. The manager automatically:
- Detects double-free attempts
- Validates pointers before freeing
- Attempts defragmentation on allocation failure
- Logs errors to serial output

## Memory Layout

```
┌─────────────────────────────┐ 0x00000000
│ Kernel Code/Data            │
│ (boot.o, kernel.o, etc.)    │
├─────────────────────────────┤ 0x00200000 (2MB)
│                             │
│ Heap (30MB)                 │
│ - Dynamic allocations       │
│ - First-fit algorithm       │
│                             │
├─────────────────────────────┤ 0x02000000
│ Stack Region                │
│ - Process stacks (16KB ea)  │
│ - Up to 32 processes        │
│ - Grows downward            │
└─────────────────────────────┘
```

## Performance

- **Allocation**: O(n) - first-fit search
- **Deallocation**: O(n) - merging pass
- **Space overhead**: ~24 bytes per block
- **Alignment**: 4-byte boundaries

---
**kacchiOS Memory Manager** • See MEMORY_MANAGER.md for full documentation
