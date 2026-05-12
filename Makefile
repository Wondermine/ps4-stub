CC      := gcc
LD      := ld
OBJCOPY := objcopy
PYTHON  := python3

CFLAGS := \
	-m64 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-fno-pie \
	-fno-builtin \
	-fno-asynchronous-unwind-tables \
	-fno-unwind-tables \
	-nostdlib \
	-Wall \
	-Wextra \
	-O2

ASFLAGS := \
	-m64 \
	-ffreestanding \
	-fno-pic \
	-fno-pie \
	-nostdlib

LDFLAGS := \
	-m elf_x86_64 \
	-nostdlib \
	-T linker.ld

EFI_CFLAGS := \
	-m64 \
	-ffreestanding \
	-fshort-wchar \
	-fno-stack-protector \
	-fPIC \
	-fno-builtin \
	-fno-asynchronous-unwind-tables \
	-fno-unwind-tables \
	-nostdlib \
	-Wall \
	-Wextra \
	-O2

all: bzImage.stub

entry.o: entry.S
	$(CC) $(ASFLAGS) -c -o $@ $<

stub.o: stub.c pe_loader.h
	$(CC) $(CFLAGS) -c -o $@ stub.c

pe_loader.o: pe_loader.c pe_loader.h
	$(CC) $(CFLAGS) -c -o $@ pe_loader.c

stub.elf: entry.o stub.o pe_loader.o linker.ld
	$(LD) $(LDFLAGS) -o $@ entry.o stub.o pe_loader.o

stub.bin: stub.elf
	$(OBJCOPY) -O binary $< $@

efi_test.o: efi_test.c
	$(CC) $(EFI_CFLAGS) -c -o $@ $<

efi_test.so: efi_test.o
	$(LD) \
		-shared \
		-Bsymbolic \
		-nostdlib \
		-Ttext 0x100000 \
		-o $@ \
		$<

EFI_CC := x86_64-w64-mingw32-gcc

efi_test.efi: efi_test.c
	$(EFI_CC) \
		-ffreestanding \
		-fshort-wchar \
		-mno-red-zone \
		-fno-stack-protector \
		-fno-builtin \
		-nostdlib \
		-Wl,--entry,efi_main \
		-Wl,--subsystem,10 \
		-Wl,--image-base,0x08000000 \
		-o $@ \
		$<

bzImage.stub: stub.bin efi_test.efi make_fake_bzimage.py
	$(PYTHON) make_fake_bzimage.py

clean:
	rm -f *.o *.elf *.bin *.so *.efi bzImage.stub