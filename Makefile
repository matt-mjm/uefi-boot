.POSIX:
.PHONY: all clean

SRC_DIR = source
INC_DIR = uefi-spec/include

SOURCES = ${SRC_DIR}/main.c
TARGET = boot.efi

CC = clang \
	-target x86_64-unknown-windows
CFLAGS = \
	-std=c17 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-mno-red-zone \
	-ffreestanding \
	-nostdlib
LDFLAGS = \
	-fuse-ld=lld-link \
	-Wl,-subsystem:efi_application \
	-Wl,-entry:efi_main

all: ${TARGET}

${TARGET}: ${SOURCES}
	${CC} ${CFLAGS} ${LDFLAGS} -I ${INC_DIR} -o $@ $<

clean:
	rm ${TARGET}
