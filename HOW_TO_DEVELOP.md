# The Badger: How To Develop

You'll need these tools to compile and flash The Badger yourself:

- [CMake](https://cmake.org/download/) 3.20 or newer
- [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (`arm-none-eabi-gcc`, `arm-none-eabi-g++`, `arm-none-eabi-gcc-ar`, `arm-none-eabi-gcc-ranlib` and `arm-none-eabi-objcopy` must be in your `PATH`)
- [Python 3](https://www.python.org/downloads/) (for the UF2 conversion and upload scripts)
- [Git](https://git-scm.com/) (the `uf2` and `tinyusb` libraries are submodules)

Optional, only for flashing with a debug probe:

- [probe-rs](https://probe.rs/docs/getting-started/installation/) (ST-Link and others)
- [J-Link software pack](https://www.segger.com/downloads/jlink/) (`JLinkExe`)

Please follow your OS/distribution guides on how to get them.

## Clone the repository

```sh
git clone --recurse-submodules https://github.com/xx0x/badger
cd badger
```

If you cloned without `--recurse-submodules`, fetch them afterwards:

```sh
git submodule update --init --recursive
```

## Build it

The firmware project lives in the `code` directory:

```sh
cd code
cmake -S . -B build
cmake --build build
```

Outputs are generated in `build/`:

- `badger.elf`
- `badger.hex`
- `badger.bin`
- `badger.uf2`

## Flash it

For simple uploads, you can use the UF2 method. Hold the *PREV* button on startup and drag the UF2 file onto the *BADGERBOOT* drive.

Or use a command:

```sh
cmake --build build --target upload-uf2
```

For advanced debugging (or bootloader flashing when using a fresh PCB) you'll need a debug probe.

## Supported debug probes

### ST-Link and others using `probe-rs`

Make sure to have probe-rs installed using the [official install guide](https://probe.rs/docs/getting-started/installation/).

### J-Link using `JLinkExe` (the official J-Link app)

Make sure you have the J-Link software pack from the [official website](https://www.segger.com/downloads/jlink/).  
Then add Puya MCU family to the J-Link (it doesn't support them out of the box): copy the contents of [code/misc/JLinkDevices](./code/misc/JLinkDevices) (the `Puya` folder) into the J-Link devices folder for your OS.

| OS      | Location                                              |
| ------- | ----------------------------------------------------- |
| Windows | C:\Users\<USER>\AppData\Roaming\SEGGER\JLinkDevices   |
| Linux   | $HOME/.config/SEGGER/JLinkDevices                     |
| macOS   | $HOME/Library/Application Support/SEGGER/JLinkDevices |

## Flashing the bootloader using a probe

This erases the whole chip and only needs to be done once per device.

The bootloader is a prebuilt binary stored in [code/bootloader](./code/bootloader). It's built from the [add-badger-py32f071](https://github.com/xx0x/tinyuf2/tree/add-badger-py32f071) branch of the TinyUF2 fork.

```sh
cmake --build build --target flash-bootloader-jlink
cmake --build build --target flash-bootloader-probe-rs
```

## Flashing the application itself using a probe

These only erase the application region, so the bootloader survives.

```sh
cmake --build build --target flash-jlink
cmake --build build --target flash-probe-rs
```
