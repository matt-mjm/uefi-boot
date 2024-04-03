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

typedef struct {
    char16_t **Data;
    size_t Length;
    size_t Capacity;
} DynamicPath;

EFI_STATUS DynamicPathInit(DynamicPath *This) {
    if (This == NULL)
        return EFI_INVALID_PARAMETER;

    This->Data = NULL;
    This->Length = 0;
    This->Capacity = 0;

    return EFI_SUCCESS;
}
EFI_STATUS DynamicPathFree(DynamicPath *This) {
    if (This == NULL)
        return EFI_INVALID_PARAMETER;

    if (This->Data != NULL) {
        for (size_t i = 0; i < This->Length; i++) {
            BootServices->FreePool(This->Data[i]);
        }

        EFI_STATUS status = BootServices->FreePool(This->Data);
        if (status != EFI_SUCCESS)
            return status;
    }

    DynamicPathInit(This);

    return EFI_SUCCESS;
}

EFI_STATUS DynamicPathExpand(DynamicPath *This) {
    if (This == NULL)
        return EFI_INVALID_PARAMETER;

    size_t newCapacity = This->Capacity > 8 ? This->Capacity * 2 : 8;
    char16_t **newData = NULL;
    BootServices->AllocatePool(EfiLoaderData,
            sizeof(EFI_DEVICE_PATH_PROTOCOL **) * newCapacity,
            (VOID **)&newData);

    if (This->Data != NULL) {
        memcpy(newData, This->Data, sizeof(char16_t **) * This->Capacity);
        BootServices->FreePool(This->Data);
    }

    This->Data = newData;
    This->Capacity = newCapacity;

    return EFI_SUCCESS;
}

EFI_STATUS DynamicPathAppend(DynamicPath *This, const uint16_t *path) {
    if (This == NULL || path == NULL)
        return EFI_INVALID_PARAMETER;

    if (This->Length >= This->Capacity)
        DynamicPathExpand(This);

    size_t length = strlen(path);
    char16_t *Node = NULL;
    BootServices->AllocatePool(EfiLoaderData,
            (length + 1) * sizeof(char16_t),
            (void **)&Node);
    strcpy(Node, path);
    Node[length] = u'\0';

    This->Data[This->Length++] = Node;

    return EFI_SUCCESS;
}

EFI_STATUS DynamicPathRemove(DynamicPath *This) {
    if (This == NULL || This->Length == 0)
        return EFI_INVALID_PARAMETER;

    BootServices->FreePool(This->Data[--This->Length]);

    return EFI_SUCCESS;
}

EFI_STATUS DynamicPathFlatten(DynamicPath *This, EFI_DEVICE_PATH_PROTOCOL **DevicePath) {
    if (This == NULL || DevicePath == NULL)
        return EFI_INVALID_PARAMETER;

    size_t totalLength = 1;
    for (size_t i = 0; i < This->Length; i++) {
        totalLength += strlen(This->Data[i]) + 1;
    }

    EFI_DEVICE_PATH_PROTOCOL *PathNode = NULL, *EndNode = NULL;
    BootServices->AllocatePool(EfiLoaderData, sizeof(EFI_DEVICE_PATH_PROTOCOL) * 2 + totalLength * sizeof(char16_t), (void **)&PathNode);
    EndNode = (EFI_DEVICE_PATH_PROTOCOL *)(PathNode->Data + totalLength * sizeof(char16_t));
    *DevicePath = PathNode;

    PathNode->Type = EFI_DEVICE_PATH_MEDIA;
    PathNode->SubType = EFI_DEVICE_PATH_MEDIA_FILE_PATH;
    PathNode->Length = sizeof(EFI_DEVICE_PATH_PROTOCOL) + totalLength * sizeof(char16_t);

    EndNode->Type = EFI_DEVICE_PATH_END;
    EndNode->SubType = 0xFF;
    EndNode->Length = sizeof(EFI_DEVICE_PATH_PROTOCOL);

    size_t totalIndex = 0;
    char16_t *path = (char16_t *)PathNode->Data;
    for (size_t i = 0; i < This->Length; i++) {
        path[totalIndex++] = u'\\';
        for (size_t j = 0; This->Data[i][j] != u'\0'; j++) {
            path[totalIndex++] = This->Data[i][j];
        }
    }
    path[totalIndex++] = u'\0';

    return EFI_SUCCESS;
}

EFI_STATUS DisplayDevicePath(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {

    printf(u"Type: %u\r\n", DevicePath->Type);
    printf(u"SubType: %u\r\n", DevicePath->SubType);
    printf(u"Length: %u\r\n", DevicePath->Length);

    if (DevicePath->Type == EFI_DEVICE_PATH_MEDIA && DevicePath->SubType == EFI_DEVICE_PATH_MEDIA_FILE_PATH) {
        printf(u"Path: [%s]\r\n", DevicePath->Data);
    }

    if (DevicePath->Type != EFI_DEVICE_PATH_END) {
        DisplayDevicePath((EFI_DEVICE_PATH_PROTOCOL *)(DevicePath->Data + DevicePath->Length - sizeof(EFI_DEVICE_PATH_PROTOCOL)));
    }

    return EFI_SUCCESS;
}

EFI_STATUS DirectoryNavigationMenu(void) {
    uint8_t FileBuffer[2][sizeof(EFI_FILE_INFO) + 128];

    DynamicPath dynamicPath;
    DynamicPathInit(&dynamicPath);

    EFI_FILE_PROTOCOL *Directory = NULL;
    SimpleFileSystemProtocol->OpenVolume(SimpleFileSystemProtocol, &Directory);

    uint64_t fileSelect = 1;
    for (;;) {
        ConsoleOut->SetAttribute(ConsoleOut,
            EFI_TEXT_ATTRIBUTE(EFI_LIGHTGRAY, EFI_BLACK));
        ConsoleOut->ClearScreen(ConsoleOut);

        Directory->SetPosition(Directory, 0);
        EFI_FILE_INFO *fileInfo = (EFI_FILE_INFO *)FileBuffer[0];
        EFI_FILE_INFO *fileInfoActive = (EFI_FILE_INFO *)FileBuffer[1];

        UINTN fileInfoSize = sizeof(FileBuffer[0]);

        uint64_t fileCount = 0;
        for (;;) {
            fileInfoSize = sizeof(FileBuffer[0]);
            Directory->Read(Directory, &fileInfoSize, fileInfo);

            if (fileInfoSize == 0)
                break;
            fileCount += 1;

            if (fileSelect == fileCount)
                memcpy(fileInfoActive, fileInfo, fileInfoSize);
            ConsoleOut->SetAttribute(ConsoleOut, EFI_TEXT_ATTRIBUTE(
                    fileSelect == fileCount ? EFI_BLACK : EFI_LIGHTGRAY,
                    fileSelect == fileCount ? EFI_LIGHTGRAY : EFI_BLACK));
            printf(u"%s %s\r\n", (fileInfo->Attribute & EFI_FILE_DIRECTORY) ? u"[DIR]  " : u"[FILE] ", fileInfo->FileName);
        }

        EFI_INPUT_KEY key = WaitForKey();
        if (key.ScanCode == SCANCODE_ESCAPE || key.UnicodeChar == u'q') {
            break;
        } else if (key.ScanCode == SCANCODE_UP_ARROW) {
            if (fileSelect > 1)
                    fileSelect--;
        } else if (key.ScanCode == SCANCODE_DOWN_ARROW) {
            if (fileSelect < fileCount)
                    fileSelect++;
        } else if (key.UnicodeChar == u'\r') {
            if (fileInfoActive->FileName[0] == '.') {
                if (fileInfoActive->FileName[1] == '.') {
                    DynamicPathRemove(&dynamicPath);
                }
            } else {
                DynamicPathAppend(&dynamicPath, fileInfoActive->FileName);
            }

            if (fileInfoActive->Attribute & EFI_FILE_DIRECTORY) {
                EFI_FILE_PROTOCOL *Next = NULL;
                Directory->Open(Directory, &Next, fileInfoActive->FileName, EFI_FILE_READ_ONLY, 0);
                Directory->Close(Directory);
                Directory = Next;
                fileSelect = 1;
            } else {
                ConsoleOut->SetAttribute(ConsoleOut,
                        EFI_TEXT_ATTRIBUTE(EFI_CYAN, EFI_BLACK));
                printf(u"\r\n[SELECTED] %s\r\n", fileInfoActive->FileName);

                EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;
                DynamicPathFlatten(&dynamicPath, &DevicePath);
                DisplayDevicePath(DevicePath);

                /*EFI_STATUS status = BootServices->LoadImage(FALSE, MyImageHandle, DevicePath, NULL, 0, &NextImageHandle);
                printf(u"Status: %u\r\n", 0xFFFFFFFF & status);
                printf(u"ImageHandle: %u\r\n", NextImageHandle);
                printf(u"DevicePath: %u\r\n", DevicePath);*/

                BootServices->FreePool(DevicePath);
                DynamicPathRemove(&dynamicPath);

                WaitForKey();
                // break;
            }
        } else {
            printf(u"\r\n[%u] [%c]\r\n", key.ScanCode, key.UnicodeChar);
            WaitForKey();
        }
    }

    ConsoleOut->SetAttribute(ConsoleOut,
                EFI_TEXT_ATTRIBUTE(EFI_LIGHTGRAY, EFI_BLACK));
    ConsoleOut->ClearScreen(ConsoleOut);

    Directory->Close(Directory);

    DynamicPathFree(&dynamicPath);

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

    DirectoryNavigationMenu();

    if (NextImageHandle == NULL) {
        SystemTable->RuntimeServices->ResetSystem(EFI_RESET_SHUTDOWN, EFI_SUCCESS, 0, NULL);
    } else {
        ConsoleOut->SetAttribute(ConsoleOut,
                EFI_TEXT_ATTRIBUTE(EFI_CYAN, EFI_BLACK));
        printf(u"\r\n[LOADED]\r\n");
        WaitForKey();
    }

    efi_cleanup();
    return EFI_SUCCESS;
}
