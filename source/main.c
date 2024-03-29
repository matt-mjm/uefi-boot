#include "efi/efi.h"

static EFI_BOOT_SERVICES *BootServices = NULL;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConsoleOut = NULL;
static EFI_LOADED_IMAGE_PROTOCOL *LoadedImageProtocol = NULL;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystemProtocol = NULL;

static EFI_GUID LoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID SimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

EFI_STATUS EFI_API efi_init(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    BootServices = SystemTable->BootServices;
    ConsoleOut = SystemTable->ConsoleOut;

    BootServices->OpenProtocol(
            ImageHandle,
            &LoadedImageProtocolGuid,
            (VOID **)&LoadedImageProtocol,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    BootServices->OpenProtocol(
            LoadedImageProtocol->DeviceHandle,
            &SimpleFileSystemProtocolGuid,
            (VOID **)&SimpleFileSystemProtocol,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    return EFI_SUCCESS;
}

EFI_STATUS efi_cleanup(EFI_HANDLE ImageHandle) {
    BootServices->CloseProtocol(
            LoadedImageProtocol->DeviceHandle, &SimpleFileSystemProtocolGuid, ImageHandle, NULL);
    BootServices->CloseProtocol(
            ImageHandle, &LoadedImageProtocolGuid, ImageHandle, NULL);

    return EFI_SUCCESS;
}

EFI_STATUS PrintDirectory(EFI_FILE_PROTOCOL *Directory, size_t depth) {
    uint8_t buffer[sizeof(EFI_FILE_INFO) + 64];

    EFI_FILE_INFO *fileInfo = (EFI_FILE_INFO *)buffer;
    UINTN fileInfoSize = sizeof(buffer);
    Directory->SetPosition(Directory, 0);
    Directory->Read(Directory, &fileInfoSize, fileInfo);
    while (fileInfoSize > 0) {
        for (size_t i = 0; i < depth; i++)
            ConsoleOut->OutputString(ConsoleOut, u" ");
        ConsoleOut->OutputString(ConsoleOut, (fileInfo->Attribute & EFI_FILE_DIRECTORY) ? u"[DIR]  " : u"[FILE] ");
        ConsoleOut->OutputString(ConsoleOut, fileInfo->FileName);
        ConsoleOut->OutputString(ConsoleOut, u"\r\n");

        if ((fileInfo->Attribute & EFI_FILE_DIRECTORY) && fileInfo->FileName[0] != u'.' && depth < 4) {
            EFI_FILE_PROTOCOL *Child;
            Directory->Open(Directory,
                    &Child,fileInfo->FileName, EFI_FILE_MODE_READ, 0);
            PrintDirectory(Child, depth + 2);
            Child->Close(Child);
        }

        fileInfoSize = sizeof(buffer);
        Directory->Read(Directory, &fileInfoSize, fileInfo);
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFI_API efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    efi_init(ImageHandle, SystemTable);

    SystemTable->ConsoleOut->SetAttribute(SystemTable->ConsoleOut,
            EFI_TEXT_ATTRIBUTE(EFI_RED, EFI_BLACK));
    SystemTable->ConsoleOut->ClearScreen(SystemTable->ConsoleOut);

    EFI_FILE_PROTOCOL *Root;
    SimpleFileSystemProtocol->OpenVolume(
            SimpleFileSystemProtocol, &Root);
    PrintDirectory(Root, 0);
    Root->Close(Root);

    ConsoleOut->OutputString(ConsoleOut, u"\r\nPress any key to shutdown...\r\n");

    EFI_INPUT_KEY key;
    while (SystemTable->ConsoleIn->ReadKeyStroke(SystemTable->ConsoleIn, &key) != EFI_SUCCESS);

    SystemTable->RuntimeServices->ResetSystem(EFI_RESET_SHUTDOWN, EFI_SUCCESS, 0, NULL);

    efi_cleanup(ImageHandle);
    return EFI_SUCCESS;
}
