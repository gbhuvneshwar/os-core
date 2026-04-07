CC      = aarch64-elf-gcc
LD      = aarch64-elf-ld
OBJCOPY = aarch64-elf-objcopy

CFLAGS  = -ffreestanding \
          -nostdlib \
          -nostdinc \
          -fno-builtin \
          -mcpu=cortex-a72 \
          -O1 \
          -Wall \
          -Wextra \
          -g

SRCS    = boot.S uart.c memory.c exceptions.c usermode.c user_task.c kernel.c

all: kernel8.img

boot.o: boot.S
	$(CC) $(CFLAGS) -c boot.S -o boot.o

uart.o: uart.c uart.h
	$(CC) $(CFLAGS) -c uart.c -o uart.o

memory.o: memory.c memory.h uart.h
	$(CC) $(CFLAGS) -c memory.c -o memory.o

exceptions.o: exceptions.c exceptions.h uart.h syscall.h usermode.h
	$(CC) $(CFLAGS) -c exceptions.c -o exceptions.o

usermode.o: usermode.c usermode.h uart.h memory.h
	$(CC) $(CFLAGS) -c usermode.c -o usermode.o

user_task.o: user_task.c usermode.h
	$(CC) $(CFLAGS) -c user_task.c -o user_task.o

kernel.o: kernel.c uart.h memory.h exceptions.h usermode.h
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

kernel.elf: boot.o uart.o memory.o exceptions.o usermode.o user_task.o kernel.o
	$(LD) -T kernel.ld \
	      boot.o uart.o memory.o exceptions.o usermode.o user_task.o kernel.o \
	      -o kernel.elf

kernel8.img: kernel.elf
	$(OBJCOPY) -O binary \
	           kernel.elf kernel8.img
	@echo ""
	@echo "=== BUILD SUCCESS ==="
	@aarch64-elf-objdump -f kernel.elf \
	  | grep "start address"
	@ls -la kernel8.img
	@echo "====================="

run: kernel8.img
	qemu-system-aarch64 \
	  -M virt \
	  -cpu cortex-a72 \
	  -kernel kernel8.img \
	  -nographic \
	  -m 128M

debug: kernel8.img
	qemu-system-aarch64 \
	  -M virt \
	  -cpu cortex-a72 \
	  -kernel kernel8.img \
	  -nographic \
	  -m 128M \
	  -s -S &
	aarch64-elf-gdb kernel.elf \
	  -ex "target remote localhost:1234" \
	  -ex "break kernel_main" \
	  -ex "continue"

clean:
	rm -f *.o *.elf *.img
