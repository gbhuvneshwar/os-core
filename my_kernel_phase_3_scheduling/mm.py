# PHYSICAL MEMORY MANAGER
class PMM:
    def __init__(self):
        self.bitmap = [0xFF] * 512  # All pages initially used
        self.free_pages = 0
    
    def bit_set(self, page):
        entry = page // 64
        bit_pos = page % 64
        self.bitmap[entry] |= (1 << bit_pos)
    
    def bit_clear(self, page):
        entry = page // 64
        bit_pos = page % 64
        self.bitmap[entry] &= ~(1 << bit_pos)
    
    def bit_test(self, page):
        entry = page // 64
        bit_pos = page % 64
        return (self.bitmap[entry] >> bit_pos) & 1
    
    def alloc_page(self):
        for p in range(MAX_PAGES):
            if not self.bit_test(p):
                self.bit_set(p)
                self.free_pages -= 1
                return RAM_START + (p << 12)
        return 0

# HEAP ALLOCATOR
class Heap:
    def __init__(self):
        self.heap_ptr = align_page(__end)
        self.heap_end = self.heap_ptr + (1024 * 1024)
    
    def kmalloc(self, size):
        size = (size + 7) & ~7  # Align to 8 bytes
        if self.heap_ptr + size > self.heap_end:
            return None
        ptr = self.heap_ptr
        self.heap_ptr += size
        return ptr

# MMU
class MMU:
    def __init__(self):
        self.l1_table = [0] * 512
    
    def init(self):
        # Create L1 page table
        self.l1_table[0] = 0x00000000 | VALID | AF | DEVICE
        self.l1_table[1] = 0x40000000 | VALID | AF | MEMORY
        
        # Configure registers
        write_mair(MAIR_VAL)
        write_tcr(TCR_FLAGS)
        write_ttbr0(address_of(self.l1_table))
        
        # Enable MMU
        sctlr = read_sctlr()
        sctlr |= (1 << 0)   # Enable MMU
        sctlr |= (1 << 2)   # Enable dcache
        sctlr |= (1 << 12)  # Enable icache
        write_sctlr(sctlr)