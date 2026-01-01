/* memory.c - Memory Manager Implementation */
#include "memory.h"
#include "string.h"
#include "serial.h"

/* Global heap management */
static heap_block_t heap_blocks[MAX_BLOCKS];
static uint32_t num_heap_blocks = 0;
static void *heap_start = (void*)HEAP_START;
static void *heap_current = (void*)HEAP_START;
static size_t heap_used = 0;

/* Global stack management */
#define MAX_PROCESS_STACKS 32
static stack_descriptor_t stack_table[MAX_PROCESS_STACKS];
static uint32_t num_stacks = 0;
static void *stack_region_start = NULL;

/* Forward declarations for internal functions */
static heap_block_t *find_free_block(size_t size);
static void merge_free_blocks(void);
static void split_block(heap_block_t *block, size_t size);

/*
 * Initialize the memory manager
 */
void memory_init(void) {
    /* Initialize heap blocks */
    for (uint32_t i = 0; i < MAX_BLOCKS; i++) {
        heap_blocks[i].address = NULL;
        heap_blocks[i].size = 0;
        heap_blocks[i].is_free = BLOCK_FREE;
        heap_blocks[i].next = NULL;
    }
    
    /* Create initial free block covering entire heap */
    heap_blocks[0].address = heap_start;
    heap_blocks[0].size = HEAP_SIZE;
    heap_blocks[0].is_free = BLOCK_FREE;
    heap_blocks[0].next = NULL;
    num_heap_blocks = 1;
    
    /* Initialize stack region (after heap) */
    stack_region_start = (void*)((uint32_t)HEAP_START + HEAP_SIZE);
    
    /* Initialize stack table */
    for (uint32_t i = 0; i < MAX_PROCESS_STACKS; i++) {
        stack_table[i].base = NULL;
        stack_table[i].top = NULL;
        stack_table[i].size = 0;
        stack_table[i].pid = 0;
        stack_table[i].is_free = 1;
    }
    
    serial_puts("[MEMORY] Memory manager initialized\n");
    serial_puts("[MEMORY] Heap: 0x");
    serial_put_hex((uint32_t)heap_start);
    serial_puts(" - 0x");
    serial_put_hex((uint32_t)heap_start + HEAP_SIZE);
    serial_puts(" (");
    serial_put_dec(HEAP_SIZE / 1024 / 1024);
    serial_puts(" MB)\n");
}

/*
 * Find a free block that can fit the requested size using First Fit algorithm
 */
static heap_block_t *find_free_block(size_t size) {
    for (uint32_t i = 0; i < num_heap_blocks; i++) {
        if (heap_blocks[i].is_free == BLOCK_FREE && heap_blocks[i].size >= size) {
            return &heap_blocks[i];
        }
    }
    return NULL;
}

/*
 * Split a block into two if it's larger than needed
 */
static void split_block(heap_block_t *block, size_t size) {
    if (block->size > size + 32 && num_heap_blocks < MAX_BLOCKS) {
        /* Find a free slot in the heap_blocks array */
        uint32_t new_idx = num_heap_blocks;
        
        /* Create new block with remaining space */
        heap_blocks[new_idx].address = (void*)((uint32_t)block->address + size);
        heap_blocks[new_idx].size = block->size - size;
        heap_blocks[new_idx].is_free = BLOCK_FREE;
        heap_blocks[new_idx].next = block->next;
        
        /* Update original block */
        block->size = size;
        block->next = &heap_blocks[new_idx];
        
        num_heap_blocks++;
    }
}

/*
 * Merge adjacent free blocks to reduce fragmentation
 */
static void merge_free_blocks(void) {
    for (uint32_t i = 0; i < num_heap_blocks - 1; i++) {
        if (heap_blocks[i].is_free == BLOCK_FREE) {
            for (uint32_t j = i + 1; j < num_heap_blocks; j++) {
                /* Check if blocks are adjacent */
                void *end_of_i = (void*)((uint32_t)heap_blocks[i].address + heap_blocks[i].size);
                
                if (heap_blocks[j].is_free == BLOCK_FREE && 
                    heap_blocks[j].address == end_of_i) {
                    /* Merge block j into block i */
                    heap_blocks[i].size += heap_blocks[j].size;
                    heap_blocks[i].next = heap_blocks[j].next;
                    
                    /* Remove block j by shifting remaining blocks */
                    for (uint32_t k = j; k < num_heap_blocks - 1; k++) {
                        heap_blocks[k] = heap_blocks[k + 1];
                    }
                    num_heap_blocks--;
                    j--; /* Check again with the shifted block */
                }
            }
        }
    }
}

/*
 * Allocate heap memory (like malloc)
 */
void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    /* Align size to 4-byte boundary */
    size = (size + 3) & ~3;
    
    /* Find a suitable free block */
    heap_block_t *block = find_free_block(size);
    
    if (block == NULL) {
        /* Try defragmentation and search again */
        merge_free_blocks();
        block = find_free_block(size);
        
        if (block == NULL) {
            serial_puts("[MEMORY] kmalloc failed: out of memory\n");
            return NULL;
        }
    }
    
    /* Split block if it's much larger than needed */
    split_block(block, size);
    
    /* Mark block as allocated */
    block->is_free = BLOCK_ALLOCATED;
    heap_used += block->size;
    
    return block->address;
}

/*
 * Free heap memory (like free)
 */
void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    
    /* Find the block containing this address */
    for (uint32_t i = 0; i < num_heap_blocks; i++) {
        if (heap_blocks[i].address == ptr) {
            if (heap_blocks[i].is_free == BLOCK_FREE) {
                serial_puts("[MEMORY] Warning: Double free detected\n");
                return;
            }
            
            heap_blocks[i].is_free = BLOCK_FREE;
            heap_used -= heap_blocks[i].size;
            
            /* Merge adjacent free blocks */
            merge_free_blocks();
            return;
        }
    }
    
    serial_puts("[MEMORY] Warning: Attempt to free invalid pointer\n");
}

/*
 * Reallocate memory (like realloc)
 */
void *krealloc(void *ptr, size_t new_size) {
    if (ptr == NULL) {
        return kmalloc(new_size);
    }
    
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    /* Find the original block */
    heap_block_t *old_block = NULL;
    for (uint32_t i = 0; i < num_heap_blocks; i++) {
        if (heap_blocks[i].address == ptr) {
            old_block = &heap_blocks[i];
            break;
        }
    }
    
    if (old_block == NULL) {
        return NULL;
    }
    
    /* If new size fits in current block, just adjust size */
    if (new_size <= old_block->size) {
        return ptr;
    }
    
    /* Allocate new block and copy data */
    void *new_ptr = kmalloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    /* Copy old data to new location */
    memcpy(new_ptr, ptr, old_block->size);
    kfree(ptr);
    
    return new_ptr;
}

/*
 * Allocate and zero memory (like calloc)
 */
void *kcalloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void *ptr = kmalloc(total_size);
    
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

/*
 * Allocate a stack for a process
 */
void *stack_alloc(uint32_t pid) {
    /* Find a free stack slot */
    for (uint32_t i = 0; i < MAX_PROCESS_STACKS; i++) {
        if (stack_table[i].is_free) {
            /* Calculate stack address */
            void *stack_base = (void*)((uint32_t)stack_region_start + (i * STACK_SIZE));
            void *stack_top = (void*)((uint32_t)stack_base + STACK_SIZE);
            
            /* Initialize stack descriptor */
            stack_table[i].base = stack_base;
            stack_table[i].top = stack_top;
            stack_table[i].size = STACK_SIZE;
            stack_table[i].pid = pid;
            stack_table[i].is_free = 0;
            
            num_stacks++;
            
            /* Zero out the stack memory */
            memset(stack_base, 0, STACK_SIZE);
            
            return stack_top; /* Stack grows downward, so return top */
        }
    }
    
    serial_puts("[MEMORY] stack_alloc failed: no free stack slots\n");
    return NULL;
}

/*
 * Free a stack for a process
 */
void stack_free(uint32_t pid) {
    for (uint32_t i = 0; i < MAX_PROCESS_STACKS; i++) {
        if (!stack_table[i].is_free && stack_table[i].pid == pid) {
            stack_table[i].base = NULL;
            stack_table[i].top = NULL;
            stack_table[i].size = 0;
            stack_table[i].pid = 0;
            stack_table[i].is_free = 1;
            
            num_stacks--;
            return;
        }
    }
}

/*
 * Get stack base address for a process
 */
void *stack_get_base(uint32_t pid) {
    for (uint32_t i = 0; i < MAX_PROCESS_STACKS; i++) {
        if (!stack_table[i].is_free && stack_table[i].pid == pid) {
            return stack_table[i].base;
        }
    }
    return NULL;
}

/*
 * Get stack top address for a process
 */
void *stack_get_top(uint32_t pid) {
    for (uint32_t i = 0; i < MAX_PROCESS_STACKS; i++) {
        if (!stack_table[i].is_free && stack_table[i].pid == pid) {
            return stack_table[i].top;
        }
    }
    return NULL;
}

/*
 * Get memory statistics
 */
void memory_get_stats(memory_stats_t *stats) {
    if (stats == NULL) {
        return;
    }
    
    stats->total_heap = HEAP_SIZE;
    stats->used_heap = heap_used;
    stats->free_heap = HEAP_SIZE - heap_used;
    stats->total_stacks = num_stacks * STACK_SIZE;
    stats->num_stacks = num_stacks;
    
    /* Count active allocations */
    stats->num_allocations = 0;
    for (uint32_t i = 0; i < num_heap_blocks; i++) {
        if (heap_blocks[i].is_free == BLOCK_ALLOCATED) {
            stats->num_allocations++;
        }
    }
}

/*
 * Print memory statistics
 */
void memory_print_stats(void) {
    memory_stats_t stats;
    memory_get_stats(&stats);
    
    serial_puts("\n=== Memory Statistics ===\n");
    serial_puts("Heap Total:  ");
    serial_put_dec(stats.total_heap / 1024);
    serial_puts(" KB\n");
    
    serial_puts("Heap Used:   ");
    serial_put_dec(stats.used_heap / 1024);
    serial_puts(" KB\n");
    
    serial_puts("Heap Free:   ");
    serial_put_dec(stats.free_heap / 1024);
    serial_puts(" KB\n");
    
    serial_puts("Allocations: ");
    serial_put_dec(stats.num_allocations);
    serial_puts("\n");
    
    serial_puts("Stacks:      ");
    serial_put_dec(stats.num_stacks);
    serial_puts(" (");
    serial_put_dec(stats.total_stacks / 1024);
    serial_puts(" KB)\n");
    
    serial_puts("Heap Blocks: ");
    serial_put_dec(num_heap_blocks);
    serial_puts("\n");
    serial_puts("========================\n\n");
}

/*
 * Defragment heap memory by merging adjacent free blocks
 */
void memory_defragment(void) {
    merge_free_blocks();
    serial_puts("[MEMORY] Heap defragmented\n");
}
