# compilers
NASM = nasm
CC   = gcc
LD   = ld
GENISO = genisoimage

# flags
NASM_FLAGS = -f elf32
CFLAGS = -m32 -ffreestanding -c -fno-builtin
LD_FLAGS = -m elf_i386 -T link.ld

# files
ASM = loader.s
C_SRC = kernel.c vesa.c bootinfo.c klog.c ac97.c bioskbd.c terminal.c commands.c string.c reboot.c fs.c diskinfo.c  library.c libdiv.c rtc.c font.c pmm.c paging.c idt.c task.c app.c pci.c speaker.c templar.c
OBJ = loader.o kernel.o vesa.o bootinfo.o klog.o ac97.o bioskbd.o terminal.o commands.o string.o reboot.o fs.o diskinfo.o  library.o libdiv.o rtc.o font.o pmm.o paging.o idt.o interrupts.o task.o app.o pci.o speaker.o templar.o
ISO_DIR = iso
GRUB_DIR = $(ISO_DIR)/boot/grub
ISO = os.iso
KERNEL = ./kernel.elf

all: $(ISO)
interrupts.o: interrupts.s
	$(NASM) $(NASM_FLAGS) interrupts.s -o interrupts.o
idt.o: idt.c idt.h
	$(CC) $(CFLAGS) idt.c -o idt.o
loader.o: loader.s
	$(NASM) $(NASM_FLAGS) loader.s -o loader.o
task.o: task.c task.h
	$(CC) $(CFLAGS) task.c -o task.o
kernel.o: kernel.c
	$(CC) $(CFLAGS) kernel.c -o kernel.o
klog.o: klog.c
	$(CC) $(CFLAGS) klog.c -o klog.o
bioskbd.o: bioskbd.c bioskbd.h
	$(CC) $(CFLAGS) bioskbd.c -o bioskbd.o
libdiv.o: libdiv.c
	$(CC) $(CFLAGS) libdiv.c -o libdiv.o
font.o: font.c
	$(CC) $(CFLAGS) font.c -o font.o
paging.o: paging.c
	$(CC) $(CFLAGS) paging.c -o paging.o
commands.o: commands.c commands.h
	$(CC) $(CFLAGS) commands.c -o commands.o
templar.o: templar.c templar.h
	$(CC) $(CFLAGS) templar.c -o templar.o
kernel.elf: $(OBJ)
	$(LD) $(LD_FLAGS) $(OBJ) -o $(KERNEL)

$(ISO): $(KERNEL)
	rm -rf $(ISO_DIR)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/
	echo "set timeout=5" > $(ISO_DIR)/boot/grub/grub.cfg
	echo "insmod all_video" >> $(ISO_DIR)/boot/grub/grub.cfg
	echo "set default=1" >> $(ISO_DIR)/boot/grub/grub.cfg
	
	#TEXT MODE
	#echo "menuentry 'Crusader OS (Text Mode)' {" >> $(ISO_DIR)/boot/grub/grub.cfg
	#echo "  terminal_output console" >> $(ISO_DIR)/boot/grub/grub.cfg
	#echo "  set gfxpayload=text" >> $(ISO_DIR)/boot/grub/grub.cfg
	#echo "  multiboot /boot/kernel.elf text" >> $(ISO_DIR)/boot/grub/grub.cfg
	#echo "  boot" >> $(ISO_DIR)/boot/grub/grub.cfg
	#echo "}" >> $(ISO_DIR)/boot/grub/grub.cfg

	#GRAPHICAL MODE
	echo "menuentry 'Crusader OS' {" >> $(ISO_DIR)/boot/grub/grub.cfg
	echo "  set gfxpayload=1280x720x32" >> $(ISO_DIR)/boot/grub/grub.cfg
	echo "  multiboot /boot/kernel.elf" >> $(ISO_DIR)/boot/grub/grub.cfg
	echo "  boot" >> $(ISO_DIR)/boot/grub/grub.cfg
	echo "}" >> $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)
	
	echo "MAKE HAS DONE EVERYTHING FOR YOU, DRIVE SIZE: 128MB"
clean:
	rm -f *.o $(KERNEL) $(ISO)
	rm -rf $(ISO_DIR)
run:
	qemu-system-i386 -cdrom os.iso \
		-drive file=disk.img,format=raw,bus=0,unit=0,media=disk \
		-drive file=music.wav,format=raw,bus=0,unit=1,media=disk \
		-audiodev pa,id=snd0 -device ac97,audiodev=snd0 \
		-m 4G -vga std -serial stdio -enable-kvm
dd_second:
	dd if=/dev/zero of=disk2.img bs=1M count=64 status=progress
dd32:
	dd if=/dev/zero of=disk.img bs=1M count=32 status=progress
dd128:
	dd if=/dev/zero of=disk.img bs=1M count=128 status=progress
dd4:
	dd if=/dev/zero of=disk.img bs=1M count=4 status=progress
hd:
	hexdump -C disk.img | less
a:
	make clean
	rm disk.img
	rm disk2.img
	make dd128
	make dd_second
	make 
	make run
	echo MAKE HAS DONE EVERYTHING FOR YOU, DRIVE SIZE: 128MB

d: 	
	qemu-system-i386 -cdrom os.iso -no-reboot -d int,cpu_reset