.POSIX:
.PHONY: all clean

SOURCE_DIR = source
SPEC_DIR = ../uefi-spec

SOURCES = ${SOURCE_DIR}/main.c
INCLUDE = ${SPEC_DIR}/include
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
	${CC} ${CFLAGS} ${LDFLAGS} -I ${INCLUDE} -o $@ $<

clean:
	rm ${TARGET}
