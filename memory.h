/* memory.h - Memory Manager Interface */
#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

/* Memory configuration constants */
#define PAGE_SIZE           4096
#define HEAP_START          0x200000    /* 2MB - Start of heap memory */
#define HEAP_SIZE           0x1E00000   /* 30MB - Total heap size */
#define STACK_SIZE          0x4000      /* 16KB per process stack */
#define MAX_BLOCKS          1024        /* Maximum heap blocks */

/* Memory block states */
#define BLOCK_FREE          0
#define BLOCK_ALLOCATED     1

/* Heap block structure for managing heap memory */
typedef struct heap_block {
    void *address;          /* Starting address of the block */
    size_t size;            /* Size of the block in bytes */
    uint8_t is_free;        /* 1 if free, 0 if allocated */
    struct heap_block *next; /* Next block in the list */
} heap_block_t;

/* Stack descriptor for process stacks */
typedef struct stack_descriptor {
    void *base;             /* Base address of the stack */
    void *top;              /* Current top of the stack */
    size_t size;            /* Total size of the stack */
    uint32_t pid;           /* Process ID owning this stack */
    uint8_t is_free;        /* 1 if free, 0 if allocated */
} stack_descriptor_t;

/* Memory statistics structure */
typedef struct memory_stats {
    size_t total_heap;      /* Total heap memory */
    size_t used_heap;       /* Used heap memory */
    size_t free_heap;       /* Free heap memory */
    size_t total_stacks;    /* Total stack memory allocated */
    uint32_t num_allocations; /* Number of active allocations */
    uint32_t num_stacks;    /* Number of active stacks */
} memory_stats_t;

/* Memory manager initialization */
void memory_init(void);

/* Heap memory functions */
void *kmalloc(size_t size);                 /* Allocate heap memory */
void kfree(void *ptr);                      /* Free heap memory */
void *krealloc(void *ptr, size_t new_size); /* Reallocate heap memory */
void *kcalloc(size_t num, size_t size);     /* Allocate and zero memory */

/* Stack memory functions */
void *stack_alloc(uint32_t pid);            /* Allocate stack for a process */
void stack_free(uint32_t pid);              /* Free stack for a process */
void *stack_get_base(uint32_t pid);         /* Get stack base for a process */
void *stack_get_top(uint32_t pid);          /* Get stack top for a process */

/* Memory utility functions */
void memory_get_stats(memory_stats_t *stats); /* Get memory statistics */
void memory_print_stats(void);               /* Print memory statistics */
void memory_defragment(void);               /* Defragment heap memory */

#endif /* MEMORY_H */
