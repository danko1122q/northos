#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "gui.h"

# bootstrap sequence
# Transitions the system from BIOS real mode to 32-bit protected mode.
# Execution begins at physical address 0x7c00.

.code16
.globl start
start:
  cli                         # Disable interrupts

setupsvga:
  # Initialize VESA BIOS Extensions (VBE)
  movb    $0x4f,%ah           # VBE function prefix
  movb    $0x01,%al           # Get VBE mode information
  movw    $0x4115,%cx         # Resolution mode (800x600 24-bit)
  movw    $0x1000,%di         # Information buffer offset
  int     $0x10               # BIOS video interrupt

  movw    $GUI_BUF,%bx
  movw    %bx, %es

  # Set VBE Video Mode
  movb    $0x4f,%ah
  movb    $0x02,%al
  movw    $0x4115,%bx         # Mode 0x115 with Linear Frame Buffer flag
  int     $0x10

  # Reset data segment registers
  xorw    %ax,%ax
  movw    %ax,%ds
  movw    %ax,%es
  movw    %ax,%ss

seta20.1:
  # Enable A20 gate to access memory above 1MB
  inb     $0x64,%al
  testb   $0x2,%al
  jnz     seta20.1

  movb    $0xd1,%al
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al
  outb    %al,$0x60

  # Transition to Protected Mode
  # Load the Global Descriptor Table (GDT)
  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE, %eax       # Set Protected Mode Enable bit
  movl    %eax, %cr0

  # Perform a long jump to reload segment registers and enter 32-bit mode
  ljmp    $(SEG_KCODE<<3), $start32

.code32
start32:
  # Initialize 32-bit data segment registers
  movw    $(SEG_KDATA<<3), %ax
  movw    %ax, %ds
  movw    %ax, %es
  movw    %ax, %ss
  movw    $0, %ax
  movw    %ax, %fs
  movw    %ax, %gs

  # Initialize stack pointer and hand over to C bootmain
  movl    $start, %esp
  call    bootmain

  # Trap execution if bootmain returns
  movw    $0x8a00, %ax
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax
  outw    %ax, %dx
spin:
  jmp     spin

# Global Descriptor Table (GDT) Configuration
.p2align 2
gdt:
  SEG_NULLASM                             # Null segment
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # Kernel Code Segment
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # Kernel Data Segment

gdtdesc:
  .word   (gdtdesc - gdt - 1)             # GDT limit
  .long   gdt                             # GDT base address