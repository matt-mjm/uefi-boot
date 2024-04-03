#include "efi/efi.h"
#include <stdarg.h>

static EFI_HANDLE MyImageHandle = NULL;
static EFI_HANDLE NextImageHandle = NULL;

static EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConsoleIn = NULL;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConsoleOut = NULL;
static EFI_RUNTIME_SERVICES *RuntimeServices = NULL;
static EFI_BOOT_SERVICES *BootServices = NULL;

static EFI_LOADED_IMAGE_PROTOCOL *LoadedImageProtocol = NULL;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystemProtocol = NULL;
static EFI_DEVICE_PATH_PROTOCOL *DevicePathProtocol = NULL;

EFI_STATUS EFI_API efi_init(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    MyImageHandle = ImageHandle;
    ConsoleIn = SystemTable->ConsoleIn;
    ConsoleOut = SystemTable->ConsoleOut;
    RuntimeServices = SystemTable->RuntimeServices;
    BootServices = SystemTable->BootServices;

    BootServices->OpenProtocol(
            ImageHandle,
            &EFI_LOADED_IMAGE_PROTOCOL_GUID,
            (VOID **)&LoadedImageProtocol,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    BootServices->OpenProtocol(
            LoadedImageProtocol->DeviceHandle,
            &EFI_DEVICE_PATH_PROTOCOL_GUID,
            (VOID **)&DevicePathProtocol,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    BootServices->OpenProtocol(
            LoadedImageProtocol->DeviceHandle,
            &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,
            (VOID **)&SimpleFileSystemProtocol,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    return EFI_SUCCESS;
}

EFI_STATUS efi_cleanup(void) {
    BootServices->CloseProtocol(
            LoadedImageProtocol->DeviceHandle, &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID, MyImageHandle, NULL);
    BootServices->CloseProtocol(
            LoadedImageProtocol->DeviceHandle, &EFI_DEVICE_PATH_PROTOCOL_GUID, MyImageHandle, NULL);
    BootServices->CloseProtocol(
            MyImageHandle, &EFI_LOADED_IMAGE_PROTOCOL_GUID, MyImageHandle, NULL);

    return EFI_SUCCESS;
}

EFI_INPUT_KEY WaitForKey(void) {
    UINTN index = 0;
    EFI_EVENT event = ConsoleIn->WaitForKey;
    BootServices->WaitForEvent(1, &event, &index);

    EFI_INPUT_KEY key = { 0, u'\0' };
    ConsoleIn->ReadKeyStroke(ConsoleIn, &key);
    return key;
}

EFI_STATUS memset(void *dst, uint8_t value, size_t size) {
    for (size_t i = 0; i < size; i++) {
        ((uint8_t *)dst)[i] = value;
    }

    return EFI_SUCCESS;
}

EFI_STATUS memcpy(void *dst, const void *src, size_t size) {
    for (size_t i = 0; i < size; i++) {
        ((uint8_t *)dst)[i] = ((uint8_t *)src)[i];
    }

    return EFI_SUCCESS;
}

EFI_STATUS printf(char16_t *fmt, ...) {
    char16_t buffer[64];

    va_list args;
    va_start(args, fmt);

    static const char16_t HEX[] = u"0123456789abcdef";

    for (size_t index = 0, bufferIndex = 0; fmt[index] != u'\0'; index++) {
        if (fmt[index] == u'%') {
            buffer[bufferIndex] = u'\0';
            ConsoleOut->OutputString(ConsoleOut, buffer);

            index += 1;
            switch (fmt[index]) {
                case u's': {
                    char16_t *str = (char16_t *)va_arg(args, char16_t*);
                    ConsoleOut->OutputString(ConsoleOut, str);
                } break;
                /*case u'S': {
                    char16_t *str = (char16_t *)va_arg(args, char16_t*);
                    size_t length = (size_t)va_arg(args, size_t);
                    memcpy(buffer, str, length);
                    buffer[length] = u'\0';
                    ConsoleOut->OutputString(ConsoleOut, buffer);
                } break;*/
                case u'c': {
                    buffer[0] = (char16_t)va_arg(args, int);
                    buffer[1] = u'\0';
                    ConsoleOut->OutputString(ConsoleOut, buffer);
                } break;
                case u'u': {
                    uint64_t num = va_arg(args, uint64_t);
                    bufferIndex = sizeof(buffer) / sizeof(char16_t);
                    buffer[--bufferIndex] = u'\0';
                    do {
                        buffer[--bufferIndex] = u'0' + (num % 10);
                        num /= 10;
                    } while (num > 0);
                    ConsoleOut->OutputString(ConsoleOut, &buffer[bufferIndex]);
                } break;
                case u'x': {
                    uint64_t num = va_arg(args, uint64_t);
                    bufferIndex = sizeof(buffer) / sizeof(char16_t);
                    buffer[--bufferIndex] = u'\0';
                    do {
                        buffer[--bufferIndex] = HEX[num % 16];
                        num /= 16;
                    } while (num > 0);
                    ConsoleOut->OutputString(ConsoleOut, &buffer[bufferIndex]);
                } break;
                default: {
                    // TODO
                } break;
            }

            bufferIndex = 0;
        } else {
            buffer[bufferIndex++] = fmt[index];
            if (fmt[index + 1] == u'\0') {
                buffer[bufferIndex] = u'\0';
                ConsoleOut->OutputString(ConsoleOut, buffer);
                bufferIndex = 0;
            }
        }
    }

    return EFI_SUCCESS;
}

size_t strlen(const char16_t *src) {
    size_t length = 0;
    while (src[length] != u'\0')
        length += 1;
    return length;
}

EFI_STATUS strcpy(char16_t *dst, const char16_t *src) {
    while (*src)
        *dst++ = *src++;
    *dst = u'\0';
    return EFI_SUCCESS;
}

size_t DevicePathLength(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    size_t length = 0;
    for (EFI_DEVICE_PATH_PROTOCOL *Node = DevicePath;;) {
        length += Node->Length;

        if (Node->Type == EFI_DEVICE_PATH_END) {
            break;
        }

        Node = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)Node + Node->Length);
    }
    return length;
}

EFI_STATUS DisplayDevicePath(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    for (EFI_DEVICE_PATH_PROTOCOL *Node = DevicePath;;) {
        printf(u"Type: %u\r\n", Node->Type);
        printf(u"SubType: %u\r\n", Node->SubType);
        printf(u"Length: %u\r\n", Node->Length);

        if (Node->Type == EFI_DEVICE_PATH_MEDIA && Node->SubType == EFI_DEVICE_PATH_MEDIA_FILE_PATH) {
            printf(u"Path: [%s]\r\n", Node->Data);
        }

        if (Node->Type == EFI_DEVICE_PATH_END) {
            break;
        }

        Node = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)Node + Node->Length);
    }

    return EFI_SUCCESS;
}

EFI_STATUS LoadImageFromPath(const char16_t *path) {
    size_t NodeSize = sizeof(EFI_DEVICE_PATH_PROTOCOL);
    size_t DeviceSize = DevicePathLength(DevicePathProtocol) - NodeSize;
    size_t PathSize = (strlen(path) + 1) * sizeof(char16_t);
    size_t TotalSize = DeviceSize + PathSize + NodeSize * 2;

    uint8_t *Buffer = NULL;
    BootServices->AllocatePool(EfiLoaderData, TotalSize, (void **)&Buffer);
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)(Buffer + 0);
    EFI_DEVICE_PATH_PROTOCOL *FilePath = (EFI_DEVICE_PATH_PROTOCOL *)(Buffer + DeviceSize);
    EFI_DEVICE_PATH_PROTOCOL *DeviceEnd = (EFI_DEVICE_PATH_PROTOCOL *)(Buffer + DeviceSize + PathSize + NodeSize);

    memcpy(Buffer, DevicePathProtocol, DeviceSize);
    FilePath->Type = EFI_DEVICE_PATH_MEDIA;
    FilePath->SubType = EFI_DEVICE_PATH_MEDIA_FILE_PATH;
    FilePath->Length = PathSize + NodeSize;
    memcpy(FilePath->Data, path, PathSize);
    DeviceEnd->Type = EFI_DEVICE_PATH_END;
    DeviceEnd->SubType = 0xFF;
    DeviceEnd->Length = NodeSize;

    ConsoleOut->ClearScreen(ConsoleOut);
    DisplayDevicePath(DevicePath);

    EFI_STATUS status = BootServices->LoadImage(TRUE, MyImageHandle, DevicePath, NULL, 0, &NextImageHandle);
    printf(u"Status: %x\r\n", status);
    WaitForKey();

    BootServices->StartImage(NextImageHandle, NULL, NULL);

    BootServices->FreePool(Buffer);

    return EFI_SUCCESS;
}

EFI_STATUS EFI_API efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    efi_init(ImageHandle, SystemTable);

    SystemTable->ConsoleOut->EnableCursor(SystemTable->ConsoleOut, TRUE);
    SystemTable->ConsoleOut->SetAttribute(SystemTable->ConsoleOut,
            EFI_TEXT_ATTRIBUTE(EFI_LIGHTGRAY, EFI_BLACK));
    SystemTable->ConsoleOut->ClearScreen(SystemTable->ConsoleOut);

    DisplayDevicePath(DevicePathProtocol);
    DisplayDevicePath(LoadedImageProtocol->FilePath);
    WaitForKey();
    LoadImageFromPath(u"\\EFI\\BOOT\\BOOTX64.EFI");

    efi_cleanup();
    return EFI_SUCCESS;
}
