/* kernel.c - Main kernel with null process */
#include "types.h"
#include "serial.h"
#include "string.h"
#include "memory.h"
#include "process.h"

#define MAX_INPUT 128

void test_memory_manager(void);
void test_process_manager(void);
void dummy_process_1(void);
void dummy_process_2(void);
void dummy_process_3(void);

/* Helper function to parse priority from string */
static process_priority_t parse_priority(const char *str) {
    if (strcmp(str, "critical") == 0) return PROC_PRIORITY_CRITICAL;
    if (strcmp(str, "high") == 0) return PROC_PRIORITY_HIGH;
    if (strcmp(str, "normal") == 0) return PROC_PRIORITY_NORMAL;
    if (strcmp(str, "low") == 0) return PROC_PRIORITY_LOW;
    return PROC_PRIORITY_NORMAL;  /* Default */
}

/* Helper function to parse integer from string */
static uint32_t parse_uint(const char *str) {
    uint32_t val = 0;
    for (int i = 0; str[i] >= '0' && str[i] <= '9'; i++) {
        val = val * 10 + (str[i] - '0');
    }
    return val;
}

/* Helper function to extract tokens from input */
static int tokenize(char *input, char *tokens[], int max_tokens) {
    int count = 0;
    char *p = input;
    
    while (*p && count < max_tokens) {
        /* Skip leading spaces */
        while (*p == ' ') p++;
        
        if (*p == '\0') break;
        
        /* Mark token start */
        tokens[count++] = p;
        
        /* Find token end */
        while (*p && *p != ' ') p++;
        
        if (*p) {
            *p = '\0';  /* Null-terminate token */
            p++;
        }
    }
    
    return count;
}

void kmain(void) {
    char input[MAX_INPUT];
    int pos = 0;
    
    /* Initialize hardware */
    serial_init();
    
    /* Initialize memory manager */
    memory_init();
    
    /* Initialize process manager */
    process_init();
    
    /* Initialize scheduler */
    scheduler_init();
    
    /* Create initial demo processes */
    serial_puts("\n[DEMO] Creating initial processes...\n");
    process_create_with_time("WebServer", PROC_PRIORITY_HIGH, 300);
    process_create_with_time("Database", PROC_PRIORITY_HIGH, 250);
    process_create_with_time("Logger", PROC_PRIORITY_NORMAL, 400);
    process_create_with_time("BackupTask", PROC_PRIORITY_LOW, 500);
    process_create_with_time("Monitor", PROC_PRIORITY_NORMAL, 200);
    serial_puts("[DEMO] Initial processes created\n");
    serial_puts("[DEMO] Type 'ps' to see process table, 'tick' to advance time\n\n");
    
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
                serial_puts("  help              - Show this help message\n");
                serial_puts("  memstats          - Display memory statistics\n");
                serial_puts("  memtest           - Run memory allocation tests\n");
                serial_puts("  ps                - Show process table (scheduler)\n");
                serial_puts("  proctest          - Run process manager tests\n");
                serial_puts("  create <name> <priority> <time> - Create process\n");
                serial_puts("      priority: critical|high|normal|low\n");
                serial_puts("      time: required execution time\n");
                serial_puts("  tick [n]          - Advance scheduler by n ticks (default 1)\n");
                serial_puts("  kill <n>          - Terminate process with PID n\n");
                serial_puts("  info <n>          - Show process info for PID n\n");
                serial_puts("  clear             - Clear the screen\n");
            }
            else if (strcmp(input, "memstats") == 0) {
                memory_print_stats();
            }
            else if (strcmp(input, "memtest") == 0) {
                test_memory_manager();
            }
            else if (strcmp(input, "ps") == 0) {
                scheduler_print_table();
            }
            else if (strcmp(input, "proctest") == 0) {
                test_process_manager();
            }
            else if (strlen(input) >= 6 && input[0] == 'c' && input[1] == 'r' && 
                     input[2] == 'e' && input[3] == 'a' && input[4] == 't' && input[5] == 'e') {
                /* Parse: create <name> <priority> <time> */
                char *tokens[10];
                int token_count = tokenize(input, tokens, 10);
                
                if (token_count == 4) {
                    char *name = tokens[1];
                    process_priority_t priority = parse_priority(tokens[2]);
                    uint32_t required_time = parse_uint(tokens[3]);
                    
                    if (required_time > 0) {
                        process_create_with_time(name, priority, required_time);
                    } else {
                        serial_puts("Error: Required time must be > 0\n");
                    }
                } else {
                    serial_puts("Usage: create <name> <priority> <time>\n");
                    serial_puts("  priority: critical|high|normal|low\n");
                    serial_puts("  time: required execution time (in ticks)\n");
                    serial_puts("Example: create Worker1 high 200\n");
                }
            }
            else if (strlen(input) >= 4 && input[0] == 't' && input[1] == 'i' && 
                     input[2] == 'c' && input[3] == 'k') {
                /* Parse: tick [n] */
                uint32_t n = 1;  /* Default to 1 tick */
                
                if (strlen(input) > 5 && input[4] == ' ') {
                    n = parse_uint(&input[5]);
                    if (n == 0) n = 1;
                }
                
                for (uint32_t i = 0; i < n; i++) {
                    scheduler_tick();
                }
                
                serial_puts("Tick advanced by ");
                serial_put_dec(n);
                serial_puts(" (Total ticks: ");
                serial_put_dec(scheduler_get_ticks());
                serial_puts(")\n");
            }
            else if (strlen(input) > 5 && input[0] == 'k' && input[1] == 'i' && 
                     input[2] == 'l' && input[3] == 'l' && input[4] == ' ') {
                /* Simple atoi for PID */
                uint32_t pid = 0;
                for (int i = 5; input[i] >= '0' && input[i] <= '9'; i++) {
                    pid = pid * 10 + (input[i] - '0');
                }
                process_terminate(pid);
            }
            else if (strlen(input) > 5 && input[0] == 'i' && input[1] == 'n' && 
                     input[2] == 'f' && input[3] == 'o' && input[4] == ' ') {
                /* Simple atoi for PID */
                uint32_t pid = 0;
                for (int i = 5; input[i] >= '0' && input[i] <= '9'; i++) {
                    pid = pid * 10 + (input[i] - '0');
                }
                process_print_info(pid);
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
/* Dummy process functions for testing */
void dummy_process_1(void) {
    serial_puts("[Process 1] Running...\n");
    /* In a real OS, this would execute process code */
}

void dummy_process_2(void) {
    serial_puts("[Process 2] Running...\n");
}

void dummy_process_3(void) {
    serial_puts("[Process 3] Running...\n");
}

/* Test the process manager */
void test_process_manager(void) {
    serial_puts("\n=== Process Manager Test ===\n");
    
    /* Test 1: Create processes */
    serial_puts("Test 1: Creating processes...\n");
    process_t *p1 = process_create("Worker1", dummy_process_1, PROC_PRIORITY_NORMAL);
    process_t *p2 = process_create("Worker2", dummy_process_2, PROC_PRIORITY_HIGH);
    process_t *p3 = process_create("Worker3", dummy_process_3, PROC_PRIORITY_LOW);
    
    if (p1 && p2 && p3) {
        serial_puts("  Created 3 processes\n");
    }
    
    /* Test 2: Process table */
    serial_puts("\nTest 2: Process table:\n");
    process_print_table();
    
    /* Test 3: State transitions */
    serial_puts("Test 3: State transitions...\n");
    serial_puts("  Setting Worker1 to BLOCKED\n");
    process_block(p1->pid);
    
    serial_puts("  Setting Worker2 to CURRENT\n");
    process_set_state(p2->pid, PROC_STATE_CURRENT);
    
    process_print_table();
    
    /* Test 4: Priority changes */
    serial_puts("Test 4: Priority management...\n");
    serial_puts("  Boosting Worker3 priority\n");
    process_boost_priority(p3->pid);
    process_print_info(p3->pid);
    
    /* Test 5: IPC - Message passing */
    serial_puts("Test 5: Inter-Process Communication...\n");
    serial_puts("  Sending message from Worker1 to Worker2\n");
    process_send_message(p2->pid, 0xDEADBEEF);
    
    serial_puts("  Worker2 has ");
    serial_put_dec(process_has_message(p2->pid));
    serial_puts(" message(s)\n");
    
    /* Test 6: Process info */
    serial_puts("\nTest 6: Detailed process info:\n");
    process_print_info(p1->pid);
    
    /* Test 7: Statistics */
    serial_puts("Test 7: Process statistics:\n");
    process_stats_t stats;
    process_get_stats(&stats);
    serial_puts("  Total processes created: ");
    serial_put_dec(stats.total_processes);
    serial_puts("\n  Active processes: ");
    serial_put_dec(stats.active_processes);
    serial_puts("\n  Ready processes: ");
    serial_put_dec(stats.ready_processes);
    serial_puts("\n  Blocked processes: ");
    serial_put_dec(stats.blocked_processes);
    serial_puts("\n");
    
    /* Test 8: Termination */
    serial_puts("\nTest 8: Process termination...\n");
    serial_puts("  Terminating Worker1\n");
    process_terminate(p1->pid);
    
    process_print_table();
    
    /* Clean up remaining processes */
    serial_puts("\nCleaning up remaining processes...\n");
    process_terminate(p2->pid);
    process_terminate(p3->pid);
    
    serial_puts("=== Test Complete ===\n\n");
    
    /* Final state */
    process_print_table();
}
