/* kernel.c - Main kernel with null process */
#include "types.h"
#include "serial.h"
#include "string.h"
#include "memory.h"
#include "process.h"
#include "scheduler.h"

#define MAX_INPUT 128

void test_memory_manager(void);
void test_process_manager(void);
void test_scheduler(void);
void dummy_process_1(void);
void dummy_process_2(void);
void dummy_process_3(void);

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
    scheduler_init(SCHED_POLICY_PRIORITY, 100);
    
    /* Start scheduler */
    scheduler_start();
    
    /* Create some demo processes for testing */
    serial_puts("\n[DEMO] Creating test processes...\n");
    process_create_with_time("CriticalTask", dummy_process_1, PROC_PRIORITY_CRITICAL, 250);
    process_create_with_time("HighPrioJob", dummy_process_2, PROC_PRIORITY_HIGH, 400);
    process_create_with_time("NormalWork", dummy_process_3, PROC_PRIORITY_NORMAL, 300);
    process_create_with_time("LowPrioTask", dummy_process_1, PROC_PRIORITY_LOW, 500);
    process_create_with_time("QuickHigh", dummy_process_2, PROC_PRIORITY_HIGH, 150);
    process_create_with_time("BackgroundJob", dummy_process_3, PROC_PRIORITY_LOW, 5000);
    serial_puts("[DEMO] 6 test processes created. Type 'ps' to view, 'tick 100' to advance.\n\n");
    
    /* Print welcome message */
    serial_puts("========================================\n");
    serial_puts("    kacchiOS - Minimal Baremetal OS\n");
    serial_puts("========================================\n");
    serial_puts("Hello from kacchiOS!\n");
    serial_puts("Type 'help' for commands, 'tick 100' to run scheduler\n\n");
    
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
                serial_puts("  ps        - Show process table\n");
                serial_puts("  proctest  - Run process manager tests\n");
                serial_puts("  create <name> <priority> <time> - Create a process\n");
                serial_puts("  kill <n>  - Terminate process with PID n\n");
                serial_puts("  info <n>  - Show process info for PID n\n");
                serial_puts("  schedtest - Run scheduler tests\n");
                serial_puts("  schedstats- Show scheduler statistics\n");
                serial_puts("  schedconf - Show scheduler configuration\n");
                serial_puts("  sched     - Start the scheduler\n");
                serial_puts("  tick [n]  - Advance scheduler by n ticks (default 1)\n");
                serial_puts("  clear     - Clear the screen\n");
            }
            else if (strcmp(input, "memstats") == 0) {
                memory_print_stats();
            }
            else if (strcmp(input, "memtest") == 0) {
                test_memory_manager();
            }
            else if (strcmp(input, "ps") == 0) {
                process_print_table();
            }
            else if (strcmp(input, "proctest") == 0) {
                test_process_manager();
            }
            else if (strcmp(input, "schedtest") == 0) {
                test_scheduler();
            }
            else if (strcmp(input, "schedstats") == 0) {
                scheduler_print_stats();
            }
            else if (strcmp(input, "schedconf") == 0) {
                scheduler_print_config();
            }
            else if (strcmp(input, "sched") == 0) {
                scheduler_start();
            }
            else if (strlen(input) >= 4 && input[0] == 't' && input[1] == 'i' && 
                     input[2] == 'c' && input[3] == 'k') {
                /* Parse tick count */
                uint32_t ticks = 1;
                if (input[4] == ' ') {
                    int idx = 5;
                    ticks = 0;
                    while (input[idx] >= '0' && input[idx] <= '9') {
                        ticks = ticks * 10 + (input[idx] - '0');
                        idx++;
                    }
                    if (ticks == 0) ticks = 1;
                }
                
                serial_puts("Advancing scheduler by ");
                serial_put_dec(ticks);
                serial_puts(" tick(s)\n");
                
                for (uint32_t i = 0; i < ticks; i++) {
                    scheduler_tick();
                }
            }
            else if (strlen(input) >= 6 && input[0] == 'c' && input[1] == 'r' && 
                     input[2] == 'e' && input[3] == 'a' && input[4] == 't' && input[5] == 'e') {
                /* Parse: create <name> <priority> <required_time> */
                char name[32] = "Process";
                process_priority_t priority = PROC_PRIORITY_NORMAL;
                uint32_t required_time = 0;
                
                /* Skip "create " */
                int idx = 7;
                
                /* Parse name */
                int name_idx = 0;
                while (input[idx] != '\0' && input[idx] != ' ' && name_idx < 31) {
                    name[name_idx++] = input[idx++];
                }
                name[name_idx] = '\0';
                
                /* Skip spaces */
                while (input[idx] == ' ') idx++;
                
                /* Parse priority */
                if (input[idx] != '\0') {
                    if (input[idx] == 'h' || input[idx] == 'H') {
                        priority = PROC_PRIORITY_HIGH;
                    } else if (input[idx] == 'n' || input[idx] == 'N') {
                        priority = PROC_PRIORITY_NORMAL;
                    } else if (input[idx] == 'l' || input[idx] == 'L') {
                        priority = PROC_PRIORITY_LOW;
                    } else if (input[idx] == 'c' || input[idx] == 'C') {
                        priority = PROC_PRIORITY_CRITICAL;
                    } else {
                        /* Try numeric priority (0-3) */
                        int prio = input[idx] - '0';
                        if (prio >= 0 && prio <= 3) {
                            priority = (process_priority_t)prio;
                        }
                    }
                    
                    /* Skip to next space */
                    while (input[idx] != '\0' && input[idx] != ' ') idx++;
                    while (input[idx] == ' ') idx++;
                    
                    /* Parse required time */
                    if (input[idx] >= '0' && input[idx] <= '9') {
                        required_time = 0;
                        while (input[idx] >= '0' && input[idx] <= '9') {
                            required_time = required_time * 10 + (input[idx] - '0');
                            idx++;
                        }
                    }
                }
                
                process_t *proc;
                if (required_time > 0) {
                    proc = process_create_with_time(name, dummy_process_1, priority, required_time);
                } else {
                    proc = process_create(name, dummy_process_1, priority);
                }
                
                if (proc) {
                    serial_puts("Created process '");
                    serial_puts(name);
                    serial_puts("' with PID ");
                    serial_put_dec(proc->pid);
                    serial_puts(" and priority ");
                    serial_put_dec(priority);
                    serial_puts("\n");
                }
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

/* Test the scheduler */
void test_scheduler(void) {
    serial_puts("\n=== Scheduler Test ===\n");
    
    /* Test 1: Create test processes */
    serial_puts("Test 1: Creating test processes...\n");
    process_t *p1 = process_create("HighPri", dummy_process_1, PROC_PRIORITY_HIGH);
    process_t *p2 = process_create("Normal", dummy_process_2, PROC_PRIORITY_NORMAL);
    process_t *p3 = process_create("LowPri", dummy_process_3, PROC_PRIORITY_LOW);
    
    if (p1 && p2 && p3) {
        serial_puts("  Created 3 processes with different priorities\n");
    }
    
    /* Test 2: Show initial configuration */
    serial_puts("\nTest 2: Initial scheduler configuration:\n");
    scheduler_print_config();
    
    /* Test 3: Test scheduling selection */
    serial_puts("Test 3: Testing process selection...\n");
    process_t *selected = scheduler_select_next_process();
    if (selected) {
        serial_puts("  Selected: ");
        serial_puts(selected->name);
        serial_puts(" (PID ");
        serial_put_dec(selected->pid);
        serial_puts(", Priority ");
        serial_put_dec(selected->priority);
        serial_puts(")\n");
        process_enqueue_ready(selected);  /* Put it back */
    }
    
    /* Test 4: Simulate scheduler ticks */
    serial_puts("\nTest 4: Simulating 10 scheduler ticks...\n");
    for (int i = 0; i < 10; i++) {
        scheduler_tick();
    }
    
    /* Test 5: Context switch simulation */
    serial_puts("\nTest 5: Context switch simulation...\n");
    process_t *from = process_dequeue_ready();
    process_t *to = process_dequeue_ready();
    
    if (from && to) {
        serial_puts("  Switching from ");
        serial_puts(from->name);
        serial_puts(" to ");
        serial_puts(to->name);
        serial_puts("\n");
        
        scheduler_switch_context(from, to);
        serial_puts("  Context switch completed\n");
    }
    
    /* Test 6: Test aging */
    serial_puts("\nTest 6: Testing aging mechanism...\n");
    if (p3) {
        serial_puts("  Process ");
        serial_puts(p3->name);
        serial_puts(" age before: ");
        serial_put_dec(p3->age);
        serial_puts("\n");
        
        /* Artificially age the process */
        p3->age = 95;
        serial_puts("  Artificially set age to 95\n");
        
        scheduler_check_aging();
        
        serial_puts("  Process age after: ");
        serial_put_dec(p3->age);
        serial_puts(", Priority: ");
        serial_put_dec(p3->priority);
        serial_puts("\n");
    }
    
    /* Test 7: Policy changes */
    serial_puts("\nTest 7: Testing policy changes...\n");
    serial_puts("  Changing to Round-Robin...\n");
    scheduler_set_policy(SCHED_POLICY_ROUND_ROBIN);
    
    serial_puts("  Changing to FCFS...\n");
    scheduler_set_policy(SCHED_POLICY_FCFS);
    
    serial_puts("  Changing back to Priority...\n");
    scheduler_set_policy(SCHED_POLICY_PRIORITY);
    
    /* Test 8: Quantum configuration */
    serial_puts("\nTest 8: Testing quantum configuration...\n");
    serial_puts("  Setting default quantum to 50 ticks\n");
    scheduler_set_quantum(50);
    
    if (p1) {
        serial_puts("  Setting process quantum for ");
        serial_puts(p1->name);
        serial_puts(" to 200 ticks\n");
        scheduler_set_process_quantum(p1->pid, 200);
    }
    
    /* Test 9: Preemption control */
    serial_puts("\nTest 9: Testing preemption control...\n");
    serial_puts("  Disabling preemption\n");
    scheduler_enable_preemption(0);
    
    serial_puts("  Enabling preemption\n");
    scheduler_enable_preemption(1);
    
    /* Test 10: Statistics */
    serial_puts("\nTest 10: Scheduler statistics:\n");
    scheduler_print_stats();
    
    /* Clean up */
    serial_puts("\nCleaning up test processes...\n");
    if (p1) process_terminate(p1->pid);
    if (p2) process_terminate(p2->pid);
    if (p3) process_terminate(p3->pid);
    
    serial_puts("=== Scheduler Test Complete ===\n\n");
}
