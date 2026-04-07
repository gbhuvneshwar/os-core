┌─────────────────────────────────────────────────────┐
│                    ARM64 QEMU virt                  │
│                   128 MB RAM Layout                  │
└─────────────────────────────────────────────────────┘

Virtual Address Space (Identity Mapped)
═════════════════════════════════════════════════════════

  0x40000000  ┌─────────────────────────────┐
              │  KERNEL CODE (.text)        │  15 KB
              │  • boot.S (_start)          │
              │  • kernel.c (kernel_main)   │
              │  • uart.c (uart functions)  │
              │  • memory.c (pmm, heap)     │
  0x40003b00  └─────────────────────────────┘
              
  0x40004000  ┌─────────────────────────────┐
              │  READ-ONLY DATA (.rodata)   │  2 KB
              │  • String literals          │
              │  • Hex lookup tables        │
  0x40004800  └─────────────────────────────┘
              
  0x40005000  ┌─────────────────────────────┐
              │  INITIALIZED DATA (.data)   │  4 KB
              │  • static variables         │
              │  • initialized globals      │
  0x40006000  └─────────────────────────────┘
              
  0x40006000  ┌─────────────────────────────┐
              │  UNINITIALIZED DATA (.bss)  │  4 KB
              │  • l1_table[512]            │
  0x40007000  └─────────────────────────────┘  ← __end
              
  0x40007000  ┌─────────────────────────────┐
              │  HEAP (malloc)              │  1 MB
              │  • kernel allocations       │
  0x40107000  └─────────────────────────────┘
              
  0x40107000  ┌─────────────────────────────┐
              │  FREE PAGES (PMM)           │  127 MB
              │  • Physical memory pool     │
              │  • Page allocations         │
  0x48000000  └─────────────────────────────┘
              
              ┌─────────────────────────────┐
              │  MEMORY TO CPU              │
              │  (grows downward)           │
0x47FFF000    └─────────────────────────────┘  ← STACK POINTER





House Layout (Linker Script):
═════════════════════════════

Foundation (0x40000000):
├─ Front Room (0x40000000) = .text (electrical wiring = code)
│  Size: 15 KB
│
├─ Foyer (0x40004000) = .rodata (artwork on walls = strings)
│  Size: 2 KB
│
├─ Kitchen (0x40005000) = .data (furniture = initialized variables)
│  Size: 4 KB
│
├─ Bedroom (0x40006000) = .bss (empty rooms to fill = uninitialized variables)
│  Size: 4 KB
│
├─ Living Room (0x40007000) = HEAP (storage closet = dynamic memory)
│  Size: 1 MB
│
└─ Yard (0x40107000) = FREE MEMORY (garden = unused land)
   Size: 127 MB