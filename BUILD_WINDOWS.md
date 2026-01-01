# Building kacchiOS on Windows

## Prerequisites

Since kacchiOS is designed for a Unix-like build environment, you have several options on Windows:

### Option 1: WSL (Windows Subsystem for Linux) - Recommended

1. **Install WSL2:**
   ```powershell
   wsl --install
   ```

2. **Install build tools in WSL:**
   ```bash
   sudo apt-get update
   sudo apt-get install build-essential qemu-system-x86 gcc-multilib
   ```

3. **Build and run:**
   ```bash
   cd /mnt/c/Users/USER/Desktop/kacchios
   make clean
   make
   make run
   ```

### Option 2: MSYS2/MinGW

1. **Install MSYS2** from https://www.msys2.org/

2. **Install build tools:**
   ```bash
   pacman -S base-devel mingw-w64-x86_64-gcc qemu
   ```

3. **Build and run:**
   ```bash
   cd /c/Users/USER/Desktop/kacchios
   make clean
   make
   make run
   ```

### Option 3: Manual Build (Current Environment)

If you don't have `make` available, you can build manually using the commands below:

#### Clean previous build:
```powershell
Remove-Item *.o, kernel.elf -ErrorAction SilentlyContinue
```

#### Build object files:
```powershell
# Assemble boot.S
as --32 boot.S -o boot.o

# Compile C files
gcc -m32 -ffreestanding -O2 -Wall -Wextra -nostdinc -fno-builtin -fno-stack-protector -I. -c kernel.c -o kernel.o
gcc -m32 -ffreestanding -O2 -Wall -Wextra -nostdinc -fno-builtin -fno-stack-protector -I. -c serial.c -o serial.o
gcc -m32 -ffreestanding -O2 -Wall -Wextra -nostdinc -fno-builtin -fno-stack-protector -I. -c string.c -o string.o
gcc -m32 -ffreestanding -O2 -Wall -Wextra -nostdinc -fno-builtin -fno-stack-protector -I. -c memory.c -o memory.o

# Link
ld -m elf_i386 -T link.ld -o kernel.elf boot.o kernel.o serial.o string.o memory.o
```

#### Run in QEMU:
```powershell
qemu-system-i386 -kernel kernel.elf -m 64M -serial stdio -display none
```

## Build Script for Windows

You can also create a `build.ps1` PowerShell script:

```powershell
# build.ps1
Write-Host "Building kacchiOS..." -ForegroundColor Green

# Clean
Remove-Item *.o, kernel.elf -ErrorAction SilentlyContinue

# Compile flags
$CFLAGS = "-m32", "-ffreestanding", "-O2", "-Wall", "-Wextra", "-nostdinc", "-fno-builtin", "-fno-stack-protector", "-I."

# Assemble
Write-Host "Assembling boot.S..." -ForegroundColor Cyan
& as --32 boot.S -o boot.o

# Compile
Write-Host "Compiling C files..." -ForegroundColor Cyan
& gcc @CFLAGS -c kernel.c -o kernel.o
& gcc @CFLAGS -c serial.c -o serial.o
& gcc @CFLAGS -c string.c -o string.o
& gcc @CFLAGS -c memory.c -o memory.o

# Link
Write-Host "Linking..." -ForegroundColor Cyan
& ld -m elf_i386 -T link.ld -o kernel.elf boot.o kernel.o serial.o string.o memory.o

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful! kernel.elf created." -ForegroundColor Green
    Write-Host "Run with: qemu-system-i386 -kernel kernel.elf -m 64M -serial stdio -display none" -ForegroundColor Yellow
} else {
    Write-Host "Build failed!" -ForegroundColor Red
}
```

Run it with:
```powershell
.\build.ps1
```

## Testing the Memory Manager

Once the OS is running, you can test the memory manager with these commands:

- `help` - Show available commands
- `memstats` - Display memory statistics
- `memtest` - Run comprehensive memory allocation tests

### Expected Output

```
========================================
    kacchiOS - Minimal Baremetal OS
========================================
[MEMORY] Memory manager initialized
[MEMORY] Heap: 0x00200000 - 0x02000000 (30 MB)
Hello from kacchiOS!
Running null process...

kacchiOS> memtest

=== Memory Manager Test ===
Test 1: Basic allocation...
  Allocated 1KB at 0x00200000
  Freed 1KB
Test 2: Multiple allocations...
  Allocated 512B, 2KB, 256B
Test 3: Free middle block...
  Freed 2KB block
Test 4: Reallocate freed space...
  Allocated 1KB in freed space
Test 5: calloc test...
  Allocated and zeroed array of 10 uint32_t
  All elements zero: YES
Test 6: Stack allocation...
  Allocated 2 process stacks
  Stack 1 at 0x02000000
  Stack 2 at 0x02004000
  Freed both stacks
=== Test Complete ===

=== Memory Statistics ===
Heap Total:  30720 KB
Heap Used:   0 KB
Heap Free:   30720 KB
Allocations: 0
Stacks:      0 (0 KB)
Heap Blocks: 1
========================
```

## Troubleshooting

### Missing `gcc` or `as`
Install a cross-compiler or use WSL. The compiler must support 32-bit x86 (`-m32` flag).

### Missing `ld`
You need binutils for linking. Install via MSYS2 or use WSL.

### Missing `qemu-system-i386`
Install QEMU from https://www.qemu.org/download/#windows or use WSL.

### Build errors
Make sure all source files are present:
- boot.S
- kernel.c
- serial.c, serial.h
- string.c, string.h
- memory.c, memory.h
- io.h
- types.h
- link.ld

## Next Steps

After successfully building and testing the memory manager, you can proceed to implement:

1. **Process Manager** - Process table, creation, termination, state transitions
2. **Scheduler** - Context switching, time quantum, scheduling policy

See [MEMORY_MANAGER.md](MEMORY_MANAGER.md) for detailed documentation on the memory manager API and usage.
