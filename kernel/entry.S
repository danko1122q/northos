# Function: entry
# Purpose: Initial kernel setup. Transitions the CPU from physical addressing 
# to virtual memory paging and initializes the primary kernel stack.

#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"

# Multiboot header for compatibility with standard bootloaders (e.g., GRUB).
.p2align 2
.text
.globl multiboot_header
multiboot_header:
  #define magic 0x1badb002
  #define flags 0
  .long magic
  .long flags
  .long (-magic-flags)

# Entry point definition. 
# Map the ELF entry point to the physical address of the entry label.
.globl _start
_start = V2P_WO(entry)

# Primary kernel entry point execution starts here with paging disabled.
.globl entry
entry:
  # Enable Page Size Extension (PSE) to allow 4MB pages.
  movl    %cr4, %eax
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4
  
  # Load the physical address of the initial page directory into CR3.
  movl    $(V2P_WO(entrypgdir)), %eax
  movl    %eax, %cr3
  
  # Enable Paging (PG) and Write Protect (WP) bits in CR0.
  movl    %cr0, %eax
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

  # Initialize the kernel stack pointer.
  movl $(stack + KSTACKSIZE), %esp

  # Transition to high-memory virtual addresses and hand over control to main().
  # An indirect jump is used to force an absolute address jump rather than PC-relative.
  mov $main, %eax
  jmp *%eax

# Allocate uninitialized storage for the initial bootstrap stack.
.comm stack, KSTACKSIZE