# Setting up the compilation environment

## VM setup

1. Create a new VirtualBox Windows VM
2. Install `codeblocks-latest.exe` to VM
	- Important: start and close Codeblocks once after installation
3. Install JL toolchain `jerry-2.5.2.exe`
4. Install `Python 3` from Microsoft Store
5. Copy SDK folder to VM, so that it resides in `C:\sdk`
6. Enable USB passthrough for the `BR34 UBOOT` disk drive to the VM in VirtualBox settings
7. Shutdown and start again the VM for USB passthrough to activate

## Authentication key generation

Requirement: Within the VM, device manager now shows the presense of a `BR34 UBOOT` disk drive

Programming the target board requires an authentication key. The file is not provided in this repo, but can be generated with the following steps:

1. Transfer these repositories to the VM:
	- https://github.com/kagaimiq/jl-uboot-tool
	- https://github.com/kagaimiq/jl-misctools
2. Install required python packages by running `pip install -r requirements.txt` within the jl-uboot-tool repository
3. Run the script `jluboottool.py`
4. Script should detect the BR34 device, and display a chip key right at the start
5. Use the chipkey and script `keyfgen.py` from the `jl-misctools` repository, example: https://github.com/kagaimiq/jl-misctools/tree/main/keyfile
6. Generate file `br34.key` and place it to `C:\sdk\cpu\br34\tools\download\data_trans\` folder.

## SDK compilation

Requirement: Within the VM, device manager now shows the presense of a `BR34 UBOOT` disk drive
Requirement: Authentication key is created in previous step

1. Open file `C:\sdk\apps\spp_and_le\board\br34\AC638N_spp_and_le.cbp` with Codeblocks
2. Build the project by selecting Build -> Build

--> During first compilation, you might get a security warning about files being created. Select option `ALLOW execution of this command for all scripts from now on`.

If successful, towards the end the build log should say `Download completed`

## Testing the board functionality

1. Plugin the programmed device to your PC using a USB cable, WITHOUT the JL tool in between
2. Check device manager, it should display a new `USB Input Device` under the `Human Interface Devices` category, with hardware ID's:

`VENDOR_ID = 0x4C4F `
`PRODUCT_ID = 0x494C`

3. Using pip, install python package hidapi to your host PC: `pip install hidapi`
4. Run the provided `usb-led.py` script in the host PC. Some LED's should come alive, with some of them blinking once a second
