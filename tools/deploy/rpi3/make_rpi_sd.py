import os
import urllib.request
import zipfile
import shutil
from pathlib import Path

HELENOS_EFI_SRC = Path("BOOTAA64.EFI") 
OUT_DIR = Path("sd_card_out")

RPI_FW_URL = "https://raw.githubusercontent.com/raspberrypi/firmware/master/boot/"
RPI_FILES = ["bootcode.bin", "start.elf", "fixup.dat", "bcm2710-rpi-3-b-plus.dtb"]

RPI_OVERLAYS_URL = "https://raw.githubusercontent.com/raspberrypi/firmware/master/boot/overlays/"
RPI_OVERLAYS = ["disable-bt.dtbo"]

UEFI_ZIP_URL = "https://github.com/pftf/RPi3/releases/download/v1.37/RPi3_UEFI_Firmware_v1.37.zip"

def download_file(url, dest):
    print(f"Downloading: {dest.name} ...")
    urllib.request.urlretrieve(url, dest)

def main():
    print("=== Generate SD card for HelenOS (RPi3) ===")
    
    if OUT_DIR.exists():
        shutil.rmtree(OUT_DIR)
    
    efi_boot_dir = OUT_DIR / "EFI" / "BOOT"
    efi_boot_dir.mkdir(parents=True, exist_ok=True)
    
    overlays_dir = OUT_DIR / "overlays"
    overlays_dir.mkdir(parents=True, exist_ok=True)
    
    print("\n[1/5] Downloading firmware VideoCore...")
    for filename in RPI_FILES:
        download_file(RPI_FW_URL + filename, OUT_DIR / filename)

    print("\n[2/5] Downloading device tree overlays...")
    for filename in RPI_OVERLAYS:
        download_file(RPI_OVERLAYS_URL + filename, overlays_dir / filename)
        
    print("\n[3/5] Downloading UEFI (pftf/RPi3)...")
    uefi_zip_path = OUT_DIR / "uefi_temp.zip"
    download_file(UEFI_ZIP_URL, uefi_zip_path)
    
    with zipfile.ZipFile(uefi_zip_path, 'r') as zip_ref:
        zip_ref.extract("RPI_EFI.fd", OUT_DIR)
    uefi_zip_path.unlink()
    
    print("\n[4/5] Generating config.txt...")
    config_content = (
        "arm_64bit=1\n"
        "disable_commandline_tags=2\n"
        "disable_overscan=1\n"
        "enable_uart=1\n"
        "uart_2ndstage=1\n"
        "armstub=RPI_EFI.fd\n"
        "device_tree_address=0x1f0000\n"
        "device_tree_end=0x200000\n"
        # "dtoverlay=disable-bt\n"
        "core_freq=250\n"
    )
    with open(OUT_DIR / "config.txt", "w") as f:
        f.write(config_content)
        
    print("\n[5/5] Copying HelenOS image...")
    if HELENOS_EFI_SRC.exists():
        shutil.copy2(HELENOS_EFI_SRC, efi_boot_dir / "BOOTAA64.EFI")
        print(f"Successfully copied: {HELENOS_EFI_SRC.name} -> EFI/BOOT/BOOTAA64.EFI")
    else:
        print(f"\nWARNING: File {HELENOS_EFI_SRC} wasn't found!")
        print("First build HelenOS.")
        return
        
    print(f"\n=== DONE! ===")
    print(f"Content of '{OUT_DIR}' is ready to be copied to FAT32 SD card.")

if __name__ == "__main__":
    main()
