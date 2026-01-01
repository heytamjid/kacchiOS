/* kernel.c - Main kernel with null process */
#include "types.h"
#include "serial.h"
#include "string.h"
#include "memory.h"

#define MAX_INPUT 128

void test_memory_manager(void);

void kmain(void) {
    char input[MAX_INPUT];
    int pos = 0;
    
    /* Initialize hardware */
    serial_init();
    
    /* Initialize memory manager */
    memory_init();
    
    /* Print welcome message */
    serial_puts("\n");
    serial_puts("========================================\n");
    serial_puts("    kacchiOS - Minimal Baremetal OS\n");
    serial_puts("========================================\n");
    serial_puts("Hello from kacchiOS!\n");
    serial_puts("Running null process...\n\n");
    
    /* Main loop - the "null process" */
    while (1) {
        serial_puts("kacchiOS> ");
        pos = 0;
        
        /* Read input line */
        while (1) {
            char c = serial_getc();
            
            /* Handle Enter key */
            if (c == '\r' || c == '\n') {
                input[pos] = '\0';
                serial_puts("\n");
                break;
            }
            /* Handle Backspace */
            else if ((c == '\b' || c == 0x7F) && pos > 0) {
                pos--;
                serial_puts("\b \b");  /* Erase character on screen */
            }
            /* Handle normal characters */
            else if (c >= 32 && c < 127 && pos < MAX_INPUT - 1) {
                input[pos++] = c;
                serial_putc(c);  /* Echo character */
            }
        }
        
        /* Echo back the input */
        if (pos > 0) {
            /* Check for built-in commands */
            if (strcmp(input, "help") == 0) {
                serial_puts("Available commands:\n");
                serial_puts("  help      - Show this help message\n");
                serial_puts("  memstats  - Display memory statistics\n");
                serial_puts("  memtest   - Run memory allocation tests\n");
                serial_puts("  clear     - Clear the screen\n");
            }
            else if (strcmp(input, "memstats") == 0) {
                memory_print_stats();
            }
            else if (strcmp(input, "memtest") == 0) {
                test_memory_manager();
            }
            else if (strcmp(input, "clear") == 0) {
                serial_puts("\033[2J\033[H");  /* ANSI escape codes to clear screen */
            }
            else {
                serial_puts("You typed: ");
                serial_puts(input);
                serial_puts("\n");
                serial_puts("Type 'help' for available commands\n");
            }
        }
    }
    
    /* Should never reach here */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* Test the memory manager */
void test_memory_manager(void) {
    serial_puts("\n=== Memory Manager Test ===\n");
    
    /* Test 1: Basic allocation and deallocation */
    serial_puts("Test 1: Basic allocation...\n");
    void *ptr1 = kmalloc(1024);
    if (ptr1) {
        serial_puts("  Allocated 1KB at 0x");
        serial_put_hex((uint32_t)ptr1);
        serial_puts("\n");
        kfree(ptr1);
        serial_puts("  Freed 1KB\n");
    }
    
    /* Test 2: Multiple allocations */
    serial_puts("Test 2: Multiple allocations...\n");
    void *ptr2 = kmalloc(512);
    void *ptr3 = kmalloc(2048);
    void *ptr4 = kmalloc(256);
    serial_puts("  Allocated 512B, 2KB, 256B\n");
    
    /* Test 3: Free middle block */
    serial_puts("Test 3: Free middle block...\n");
    kfree(ptr3);
    serial_puts("  Freed 2KB block\n");
    
    /* Test 4: Allocate in freed space */
    serial_puts("Test 4: Reallocate freed space...\n");
    void *ptr5 = kmalloc(1024);
    serial_puts("  Allocated 1KB in freed space\n");
    
    /* Test 5: calloc test */
    serial_puts("Test 5: calloc test...\n");
    uint32_t *arr = (uint32_t*)kcalloc(10, sizeof(uint32_t));
    if (arr) {
        serial_puts("  Allocated and zeroed array of 10 uint32_t\n");
        int all_zero = 1;
        for (int i = 0; i < 10; i++) {
            if (arr[i] != 0) all_zero = 0;
        }
        serial_puts("  All elements zero: ");
        serial_puts(all_zero ? "YES\n" : "NO\n");
        kfree(arr);
    }
    
    /* Test 6: Stack allocation */
    serial_puts("Test 6: Stack allocation...\n");
    void *stack1 = stack_alloc(1);
    void *stack2 = stack_alloc(2);
    if (stack1 && stack2) {
        serial_puts("  Allocated 2 process stacks\n");
        serial_puts("  Stack 1 at 0x");
        serial_put_hex((uint32_t)stack1);
        serial_puts("\n");
        serial_puts("  Stack 2 at 0x");
        serial_put_hex((uint32_t)stack2);
        serial_puts("\n");
        stack_free(1);
        stack_free(2);
        serial_puts("  Freed both stacks\n");
    }
    
    /* Clean up remaining allocations */
    kfree(ptr2);
    kfree(ptr4);
    kfree(ptr5);
    
    serial_puts("=== Test Complete ===\n\n");
    memory_print_stats();
}