#include "efi/efi.h"
#include <stdarg.h>

EFI_HANDLE ImageHandle = NULL;
static EFI_SYSTEM_TABLE *SystemTable = NULL;
static EFI_LOADED_IMAGE_PROTOCOL *LoadedImageProtocol = NULL;
static EFI_DEVICE_PATH_PROTOCOL *DevicePathProtocol = NULL;

static const char16_t HEX[] = u"0123456789abcdef";

void Init(EFI_HANDLE _ImageHandle, EFI_SYSTEM_TABLE *_SystemTable) {
    ImageHandle = _ImageHandle;
    SystemTable = _SystemTable;

    SystemTable->BootServices->OpenProtocol(
            ImageHandle,
            &EFI_LOADED_IMAGE_PROTOCOL_GUID,
            (VOID **)&LoadedImageProtocol,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    SystemTable->BootServices->OpenProtocol(
            LoadedImageProtocol->DeviceHandle,
            &EFI_DEVICE_PATH_PROTOCOL_GUID,
            (VOID **)&DevicePathProtocol,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
}

void Print(char16_t *fmt, ...) {
    char16_t buffer[64];

    va_list args;
    va_start(args, fmt);

    for (size_t index = 0, bufferIndex = 0; fmt[index] != u'\0'; index++) {
        if (fmt[index] == u'%') {
            buffer[bufferIndex] = u'\0';
            SystemTable->ConsoleOut->OutputString(
                    SystemTable->ConsoleOut, buffer);

            index += 1;
            switch (fmt[index]) {
                case u's': {
                    char16_t *str = (char16_t *)va_arg(args, char16_t*);
                    SystemTable->ConsoleOut->OutputString(
                            SystemTable->ConsoleOut, str);
                } break;
                case u'c': {
                    buffer[0] = (char16_t)va_arg(args, int);
                    buffer[1] = u'\0';
                    SystemTable->ConsoleOut->OutputString(
                            SystemTable->ConsoleOut, buffer);
                } break;
                case u'u': {
                    uint64_t num = va_arg(args, uint64_t);
                    bufferIndex = sizeof(buffer) / sizeof(char16_t);
                    buffer[--bufferIndex] = u'\0';
                    do {
                        buffer[--bufferIndex] = u'0' + (num % 10);
                        num /= 10;
                    } while (num > 0);
                    SystemTable->ConsoleOut->OutputString(
                            SystemTable->ConsoleOut, &buffer[bufferIndex]);
                } break;
                case u'x': {
                    uint64_t num = va_arg(args, uint64_t);
                    bufferIndex = sizeof(buffer) / sizeof(char16_t);
                    buffer[--bufferIndex] = u'\0';
                    do {
                        buffer[--bufferIndex] = HEX[num % 16];
                        num /= 16;
                    } while (num > 0);
                    SystemTable->ConsoleOut->OutputString(
                            SystemTable->ConsoleOut, &buffer[bufferIndex]);
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
                SystemTable->ConsoleOut->OutputString(
                        SystemTable->ConsoleOut, buffer);
                bufferIndex = 0;
            }
        }
    }
}

EFI_INPUT_KEY WaitForKey(void) {
    UINTN index = 0;
    EFI_EVENT event = SystemTable->ConsoleIn->WaitForKey;
    SystemTable->BootServices->WaitForEvent(1, &event, &index);

    EFI_INPUT_KEY key;
    key.ScanCode = 0x00;
    key.UnicodeChar = u'\0';
    SystemTable->ConsoleIn->ReadKeyStroke(SystemTable->ConsoleIn, &key);
    return key;
}

EFI_STATUS LoadGlobalVariable(char16_t *VariableName, UINTN *DataSize, void **Data) {
    if (VariableName == NULL || DataSize == NULL || Data == NULL)
        return EFI_INVALID_PARAMETER;
    *DataSize = 0;
    *Data = NULL;

    EFI_STATUS status = EFI_SUCCESS;
    SystemTable->RuntimeServices->GetVariable(VariableName, &EFI_GLOBAL_VARIABLE_GUID, NULL, DataSize, NULL);
    status = SystemTable->BootServices->AllocatePool(EfiLoaderData, *DataSize, Data);
    if (status != EFI_SUCCESS)
        return status;

    status = SystemTable->RuntimeServices->GetVariable(VariableName, &EFI_GLOBAL_VARIABLE_GUID, NULL, DataSize, *Data);
    if (status != EFI_SUCCESS)
        return status;

    return EFI_SUCCESS;
}

EFI_STATUS ExpandLoadOption(EFI_EXPANDED_LOAD_OPTION *LoadOption) {
    UINTN DescriptionLength = 0;
    char16_t *Description = (char16_t *)((uint8_t *)LoadOption->LoadOption +
            sizeof(EFI_LOAD_OPTION));
    while (Description[DescriptionLength] != u'\0')
        DescriptionLength += 1;

    LoadOption->Description = Description;
    LoadOption->DescriptionLength = (DescriptionLength + 1) * sizeof(char16_t);

    LoadOption->FilePath = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)LoadOption->LoadOption +
            sizeof(EFI_LOAD_OPTION) + LoadOption->DescriptionLength);
    LoadOption->FilePathLength = LoadOption->LoadOption->FilePathListLength;

    LoadOption->OptionalData = (UINT8 *)((uint8_t *)LoadOption->LoadOption +
            sizeof(EFI_LOAD_OPTION) + LoadOption->DescriptionLength + LoadOption->FilePathLength);
    LoadOption->OptionalDataLength = LoadOption->LoadOptionLength - sizeof(EFI_LOAD_OPTION) - LoadOption->DescriptionLength - LoadOption->FilePathLength;

    return EFI_SUCCESS;
}

UINTN DevicePathLength(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    UINTN length = 0;
    for (EFI_DEVICE_PATH_PROTOCOL *Node = DevicePath;;) {
        length += Node->Length;

        if (Node->Type == EFI_DEVICE_PATH_END) {
            break;
        }

        Node = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)Node + Node->Length);
    }
    return length;
}

BOOLEAN SecureBootEnabled(void) {
    uint8_t isSecureBoot = 0;
    UINTN isSecureBootSize = sizeof(isSecureBoot);
    EFI_STATUS status = SystemTable->RuntimeServices->GetVariable(u"SecureBoot", &EFI_GLOBAL_VARIABLE_GUID, NULL, &isSecureBootSize, &isSecureBoot);

    Print(u"SecureBoot: %u %u %x\r\n", isSecureBoot, isSecureBootSize, status);

    return status == EFI_SUCCESS && isSecureBoot == 1;
}

void Copy(void *dst, void *src, size_t length) {
    for (size_t i = 0; i < length; i++)
        ((uint8_t *)dst)[i] = ((uint8_t *)src)[i];
}

EFI_DEVICE_PATH_PROTOCOL *SpliceDevicePaths(EFI_DEVICE_PATH_PROTOCOL *Base, EFI_DEVICE_PATH_PROTOCOL *Path) {
    EFI_DEVICE_PATH_PROTOCOL *Result = NULL;
    UINTN BaseLength = DevicePathLength(Base);
    UINTN PathLength = DevicePathLength(Path);
    UINTN ResultLength = BaseLength + PathLength;

    EFI_STATUS status = SystemTable->BootServices->AllocatePool(EfiLoaderData, ResultLength, (void **)&Result);
    if (status != EFI_SUCCESS)
        return NULL;

    EFI_DEVICE_PATH_PROTOCOL *Temp = Result;
    for (EFI_DEVICE_PATH_PROTOCOL *Node = Base;;) {
        Print(u"Base %u %u %u\r\n", Node->Type, Node->SubType, Node->Length);

        if (Node->Type == Path->Type && Node->SubType == Path->SubType && Node->Length == Path->Length) {
            break;
        }

        if (Node->Type == EFI_DEVICE_PATH_END) {
            break;
        }

        Copy(Temp, Node, Node->Length);

        Node = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)Node + Node->Length);
        Temp = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)Temp + Temp->Length);
    }

    for (EFI_DEVICE_PATH_PROTOCOL *Node = Path;;) {
        Print(u"Path %u %u %u\r\n", Node->Type, Node->SubType, Node->Length);

        Copy(Temp, Node, Node->Length);

        if (Node->Type == EFI_DEVICE_PATH_END) {
            break;
        }

        Node = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)Node + Node->Length);
        Temp = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)Temp + Temp->Length);
    }

    return Result;
}

EFI_STATUS EFI_API efi_main(EFI_HANDLE _ImageHandle, EFI_SYSTEM_TABLE *_SystemTable) {
    Init(_ImageHandle, _SystemTable);

    SystemTable->ConsoleOut->SetAttribute(SystemTable->ConsoleOut,
        EFI_TEXT_ATTRIBUTE(EFI_LIGHTGRAY, EFI_BLACK));
    SystemTable->ConsoleOut->ClearScreen(SystemTable->ConsoleOut);

    EFI_EXPANDED_LOAD_OPTION loadOption;
    LoadGlobalVariable(u"Boot0000", &loadOption.LoadOptionLength, (void **)&loadOption.LoadOption);
    ExpandLoadOption(&loadOption);

    Print(u"Load Option: %u %u %u %u %s\r\n", loadOption.LoadOptionLength, loadOption.DescriptionLength, loadOption.FilePathLength, loadOption.OptionalDataLength, loadOption.Description);

    for (EFI_DEVICE_PATH_PROTOCOL *n1 = DevicePathProtocol, *n2 = loadOption.FilePath;;) {
        Print(u"%u %u %u <=> %u %u %u\r\n",
                n1->Type, n1->SubType, n1->Length,
                n2->Type, n2->SubType, n2->Length);

        BOOLEAN c1 = n1->Type == EFI_DEVICE_PATH_MEDIA && n1->SubType == EFI_DEVICE_PATH_MEDIA_FILE_PATH;
        BOOLEAN c2 = n2->Type == EFI_DEVICE_PATH_MEDIA && n2->SubType == EFI_DEVICE_PATH_MEDIA_FILE_PATH;

        if (c1 || c2) {
            Print(u"[%s] <=> [%s]\r\n", c1 ? (char16_t *)n1->Data : u"", c2 ? (char16_t *)n2->Data : u"");
        }

        if (n1->Type == EFI_DEVICE_PATH_END && n2->Type == EFI_DEVICE_PATH_END) {
            break;
        }

        if (n1->Type != EFI_DEVICE_PATH_END) {
            n1 = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)n1 + n1->Length);
        }
        if (n2->Type != EFI_DEVICE_PATH_END) {
            n2 = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)n2 + n2->Length);
        }
    }

    EFI_DEVICE_PATH_PROTOCOL *Path = SpliceDevicePaths(DevicePathProtocol, loadOption.FilePath);
    for (EFI_DEVICE_PATH_PROTOCOL *Node = Path;;) {
        Print(u"Result %u %u %u %s\r\n", Node->Type, Node->SubType, Node->Length,
                Node->Type == EFI_DEVICE_PATH_MEDIA && Node->SubType == EFI_DEVICE_PATH_MEDIA_FILE_PATH ? (char16_t *)Node->Data : u"");

        if (Node->Type == EFI_DEVICE_PATH_END) {
            break;
        }

        Node = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)Node + Node->Length);
    }

    EFI_HANDLE NextImageHandle = NULL;
    EFI_STATUS s0 = SystemTable->BootServices->LoadImage(
            TRUE, ImageHandle,
            Path,
            NULL, 0,
            &NextImageHandle);
    Print(u"Status: %x %x\r\n", s0, NextImageHandle);

    Print(u"Press any key to continue...\r\n");
    WaitForKey();

    SystemTable->BootServices->StartImage(NextImageHandle, NULL, NULL);

    return EFI_SUCCESS;
}


