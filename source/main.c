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

void Cleanup(void) {
    SystemTable->BootServices->CloseProtocol(
            LoadedImageProtocol->DeviceHandle, &EFI_DEVICE_PATH_PROTOCOL_GUID, ImageHandle, NULL);
    SystemTable->BootServices->CloseProtocol(
            ImageHandle, &EFI_LOADED_IMAGE_PROTOCOL_GUID, ImageHandle, NULL);
}

void Copy(void *dst, void *src, size_t length) {
    for (size_t i = 0; i < length; i++)
        ((uint8_t *)dst)[i] = ((uint8_t *)src)[i];
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

EFI_STATUS LoadBootNumber(uint16_t BootNumber, EFI_EXPANDED_LOAD_OPTION *LoadOption) {
    static char16_t VariableName[9] = u"Boot";
    VariableName[4] = HEX[(BootNumber / 4096) % 16];
    VariableName[5] = HEX[(BootNumber / 256) % 16];
    VariableName[6] = HEX[(BootNumber / 16) % 16];
    VariableName[7] = HEX[BootNumber % 16];
    VariableName[8] = u'\0';

    EFI_STATUS status = LoadGlobalVariable(VariableName, &LoadOption->LoadOptionLength, (void**)&LoadOption->LoadOption);
    if (status != EFI_SUCCESS)
        return status;

    ExpandLoadOption(LoadOption);

    return EFI_SUCCESS;
}

EFI_STATUS LoadBootOptions(UINTN *LoadOptionCount, EFI_EXPANDED_LOAD_OPTION **LoadOptions) {
    UINTN BootOrderSize = 0;
    uint16_t *BootOrder = NULL;
    LoadGlobalVariable(u"BootOrder", &BootOrderSize, (void **)&BootOrder);
    *LoadOptionCount = BootOrderSize / sizeof(uint16_t);

    SystemTable->BootServices->AllocatePool(EfiLoaderData, *LoadOptionCount * sizeof(EFI_EXPANDED_LOAD_OPTION), (void **)LoadOptions);
    for (UINTN i = 0; i < *LoadOptionCount; i++) {
        LoadBootNumber(BootOrder[i], &(*LoadOptions)[i]);
    }

    SystemTable->BootServices->FreePool(BootOrder);

    return EFI_SUCCESS;
}

EFI_STATUS FreeBootOptions(UINTN LoadOptionCount, EFI_EXPANDED_LOAD_OPTION *LoadOptions) {
    for (UINTN i = 0; i < LoadOptionCount; i++)
        SystemTable->BootServices->FreePool(LoadOptions[i].LoadOption);
    SystemTable->BootServices->FreePool(LoadOptions);

    return EFI_SUCCESS;
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
        Copy(Temp, Node, Node->Length);

        if (Node->Type == EFI_DEVICE_PATH_END) {
            break;
        }

        Node = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)Node + Node->Length);
        Temp = (EFI_DEVICE_PATH_PROTOCOL *)((uint8_t *)Temp + Temp->Length);
    }

    return Result;
}

void DisplayBootOptions(UINTN LoadOptionCount, EFI_EXPANDED_LOAD_OPTION *LoadOptions, UINTN Selection) {
    for (UINTN i = 0; i < LoadOptionCount; i++) {
        SystemTable->ConsoleOut->SetAttribute(SystemTable->ConsoleOut, EFI_TEXT_ATTRIBUTE(
            Selection == i ? EFI_BLACK : EFI_LIGHTGRAY,
            Selection == i ? EFI_LIGHTGRAY : EFI_BLACK));
        Print(u"  %s  \r\n", LoadOptions[i].Description);
    }
}

EFI_STATUS EFI_API efi_main(EFI_HANDLE _ImageHandle, EFI_SYSTEM_TABLE *_SystemTable) {
    Init(_ImageHandle, _SystemTable);

    UINTN LoadOptionCount = 0;
    EFI_EXPANDED_LOAD_OPTION *LoadOptions = NULL;
    LoadBootOptions(&LoadOptionCount, &LoadOptions);

    EFI_EVENT Events[2];
    SystemTable->BootServices->CreateEvent(
            EFI_EVENT_TIMER,
            TPL_APPLICATION,
            NULL, NULL,
            Events
    );
    SystemTable->BootServices->SetTimer(Events[0], TimerPeriodic, 10000000);
    Events[1] = SystemTable->ConsoleIn->WaitForKey;

    SystemTable->ConsoleOut->SetAttribute(SystemTable->ConsoleOut,
        EFI_TEXT_ATTRIBUTE(EFI_LIGHTGRAY, EFI_BLACK));
    SystemTable->ConsoleOut->ClearScreen(SystemTable->ConsoleOut);
    DisplayBootOptions(LoadOptionCount, LoadOptions, 1);
    Print(u"\r\n");

    UINTN Selection = 1;
    int Countdown = 6;
    while (--Countdown >= 0) {
        Print(u"Automatically Booting in %u Seconds   \r", Countdown);
        UINTN index = 0;
        SystemTable->BootServices->WaitForEvent(2, Events, &index);
        if (index == 1) {
            break;
        }
    }

    SystemTable->BootServices->CloseEvent(Events[0]);

    if (Countdown >= 0) {
        for (;;) {
            SystemTable->ConsoleOut->SetAttribute(SystemTable->ConsoleOut,
                EFI_TEXT_ATTRIBUTE(EFI_LIGHTGRAY, EFI_BLACK));
            SystemTable->ConsoleOut->ClearScreen(SystemTable->ConsoleOut);

            DisplayBootOptions(LoadOptionCount, LoadOptions, Selection);

            EFI_INPUT_KEY key = WaitForKey();
            if (key.ScanCode == SCANCODE_UP_ARROW) {
                if (Selection > 0)
                    Selection--;
            } else if (key.ScanCode == SCANCODE_DOWN_ARROW) {
                if (Selection < LoadOptionCount - 1)
                    Selection++;
            } else if (key.UnicodeChar == u'\r') {
                break;
            }
        }
    }

    SystemTable->ConsoleOut->SetAttribute(SystemTable->ConsoleOut,
        EFI_TEXT_ATTRIBUTE(EFI_LIGHTGRAY, EFI_BLACK));
    SystemTable->ConsoleOut->ClearScreen(SystemTable->ConsoleOut);

    EFI_DEVICE_PATH_PROTOCOL *FilePath = SpliceDevicePaths(DevicePathProtocol, LoadOptions[Selection].FilePath);
    EFI_HANDLE NextImageHandle = NULL;
    SystemTable->BootServices->LoadImage(
            TRUE, ImageHandle,
            FilePath,
            NULL, 0,
            &NextImageHandle);

    void *OptionalData = NULL;
    SystemTable->BootServices->AllocatePool(EfiLoaderData, LoadOptions[Selection].OptionalDataLength, (void **)&OptionalData);
    Copy(OptionalData, LoadOptions[Selection].OptionalData, LoadOptions[Selection].OptionalDataLength);

    EFI_LOADED_IMAGE_PROTOCOL *NextLoadedImageProtocol = NULL;
    SystemTable->BootServices->OpenProtocol(
            NextImageHandle,
            &EFI_LOADED_IMAGE_PROTOCOL_GUID,
            (VOID **)&NextLoadedImageProtocol,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    NextLoadedImageProtocol->LoadOptions = OptionalData;
    NextLoadedImageProtocol->LoadOptionsSize = LoadOptions[Selection].OptionalDataLength;

    FreeBootOptions(LoadOptionCount, LoadOptions);
    Cleanup();

    SystemTable->BootServices->StartImage(NextImageHandle, NULL, NULL);
    SystemTable->BootServices->CloseProtocol(NextImageHandle, &EFI_LOADED_IMAGE_PROTOCOL_GUID, ImageHandle, NULL);
    SystemTable->BootServices->FreePool(OptionalData);
    SystemTable->BootServices->UnloadImage(NextImageHandle);

    return EFI_SUCCESS;
}


