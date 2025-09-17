GPPPARAMS = -m32 -Iinclude -fno-use-cxa-atexit -fleading-underscore -fno-exceptions -fno-builtin -nostdlib -fno-rtti -fno-pie

ASPARAMS = --32
LDPARAMS = -melf_i386 -no-pie

objects = obj/boot/boot.o \
          obj/kernel/kernel.o \
	  obj/kernel/kerio.o \
	  obj/kernel/gdt.o \
	  obj/kernel/ioctl.o \
	  obj/kernel/pic.o \
	  obj/kernel/interrupt/interruptstubs.o \
	  obj/kernel/interrupt/interrupt.o \
	  obj/kernel/memory/malloc.o \
	  obj/kernel/memory/paging.o \
	  obj/kernel/multitask/process.o \
	  obj/kernel/string.o \
	  obj/kernel/syscall/syscall.o \
	  obj/driver/driver.o \
	  obj/driver/keyboard.o \
	  obj/driver/block.o \
	  obj/fs/vfs.o \
	  obj/fs/devfs.o \
	  obj/fs/ext4.o \
	  obj/user/shell/shell.o \
	  obj/user/installer/installer.o \
	  obj/stdio.o

obj/%.o: src/%.c
	mkdir -p $(@D)
	gcc ${GPPPARAMS} -o $@ -c $<

obj/%.o: src/%.s
	mkdir -p $(@D)	
	as ${ASPARAMS} -o $@ $<

kernel.bin: linker.ld ${objects}
	ld ${LDPARAMS} -T $< -o $@ ${objects}

install: kernel.bin
	sudo cp $< /boot/kernel.bin

iso: kernel.bin
	mkdir iso
	mkdir iso/boot
	mkdir iso/boot/grub
	cp $< iso/boot/
	echo 'set timeout=10' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo '' >> iso/boot/grub/grub.cfg
	echo 'menuentry "Jarvis OS - launch the temporay system" {' >> iso/boot/grub/grub.cfg
	echo '  multiboot /boot/kernel.bin' >> iso/boot/grub/grub.cfg
	echo '  boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	echo '' >> iso/boot/grub/grub.cfg
	echo 'menuentry "Jarvis OS - install system" {' >> iso/boot/grub/grub.cfg
	echo '  multiboot /boot/kernel.bin --install' >> iso/boot/grub/grub.cfg
	echo '  boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue --output=os.iso iso
	rm -rf iso

run: iso
	qemu-system-i386 -cdrom os.iso -boot d -hda hda.img -m 1G

debug: iso
	qemu-system-i386 -cdrom os.iso -boot d -hda hda.img -m 1G -nographic


# 创建一个100MB的空白硬盘镜像
hda.img:
	dd if=/dev/zero of=hda.img bs=1M count=100

.PHONY: clean
clean:
	rm -rf kernel.bin os.iso obj
