# Function: start
# Purpose: Entry point for the very first user-space process.
# This code executes an 'exec' system call to launch the /init binary.

#include "syscall.h"
#include "traps.h"

.globl start
start:
  # Prepare arguments for the exec(init, argv) system call
  pushl $argv           # Push pointer to the argument array
  pushl $init           # Push pointer to the executable path string
  pushl $0              # Dummy return address to satisfy C calling convention
  
  # Invoke the exec system call
  movl $SYS_exec, %eax  # Load exec system call number
  int $T_SYSCALL        # Trigger software interrupt to enter kernel mode

# Function: exit
# Purpose: Fallback loop that calls exit if exec fails for any reason.
exit:
  movl $SYS_exit, %eax  # Load exit system call number
  int $T_SYSCALL        # Trigger software interrupt
  jmp exit              # Ensure the process never continues past this point

# Data: init
# Purpose: The null-terminated path of the initial program to execute.
init:
  .string "/init\0"

# Data: argv
# Purpose: Argument array for exec, containing the program name and a null terminator.
.p2align 2
argv:
  .long init
  .long 0