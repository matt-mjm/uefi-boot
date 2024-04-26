# uefi-boot
Simple UEFI boot manager.

### Description
The program loads the current boot options then allow the user to select one of them.
If no keys are pressed for at least five seconds after boot then the second boot option is loaded automatically.

### Build Instructions
The [uefi-spec](https://github.com/matt-mjm/uefi-spec) repository needs to exist in the same directory as this repository for the build to work.
```
git clone https://github.com/matt-mjm/uefi-boot.git
git clone https://github.com/matt-mjm/uefi-spec.git
cd uefi-boot
make
```

### Instalation Instructions
```
cp boot.efi /efi/EFI/<ImagePath>.efi
sbctl sign /efi/EFI/<ImagePath>.efi

efibootmgr --create \
  --disk /dev/<drive> \
  --part <efi-partition-number> \
  --label <label> \
  --loader /EFI/<ImagePath>.efi
```
For proper behavior this program should be the first boot entry.
The second boot entry should be the default operating system.
All subsiquent operating systems should appear on the boot order.

To list current boot order:
```
efibootmgr
```
To update boot order:
```
efibootmgr -o <Boot Manager Number>,<Default Operating System>,...
```
