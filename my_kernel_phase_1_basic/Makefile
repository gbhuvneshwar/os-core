CC      = aarch64-linux-gnu-gcc
LD      = aarch64-linux-gnu-ld
OBJCOPY = aarch64-linux-gnu-objcopy

CFLAGS  = -ffreestanding -nostdlib \
          -nostdinc -fno-builtin \
          -mcpu=cortex-a72 \
          -O2 -Wall

all: kernel8.img

boot.o: boot.S
	$(CC) $(CFLAGS) -c boot.S -o boot.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

kernel.elf: boot.o kernel.o
	$(LD) -T kernel.ld boot.o kernel.o \
	      -o kernel.elf

kernel8.img: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel8.img
	@echo "Build successful!"

run: kernel8.img
	qemu-system-aarch64 \
	  -M virt \
	  -cpu cortex-a72 \
	  -kernel kernel8.img \
	  -nographic \
	  -m 128M

clean:
	rm -f *.o *.elf *.img
