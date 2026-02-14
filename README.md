# Reprogramming a JL / Jieli based LED RGB device

![image of LED RGB device](images/device.jpg)

[DEMO: images/demo.mp4](images/demo.mp4)

Available at least from [Clas Ohlson](https://www.clasohlson.com/fi/LED-lista,-varitehosteita,-ladattava-18-cm/p/36-9075)

The device contains a JL / Jieli AD6983D microcontroller that can be reprogrammed, with features such as:

- DSP Audio Processing
- Bluetooth V5.4+BR+EDR+BLE
- Multiple ADC/DAC
- USB controller

The device itself also contains multiple physical buttons, a microphone and 32 addressable RGB LED's.

Link to compatible JL SDK: [fw-AC63_BT_SDK](https://github.com/Jieli-Tech/fw-AC63_BT_SDK)

- There is an [English README](https://github.com/Jieli-Tech/fw-AC63_BT_SDK/blob/master/README-en.md) in the repository
- In JL terms, AD6983D belongs to the "br34" family, so any SDK demo apps that support br34, will at least in theory be compatible.

- **WARNING:** The SDK does contain questionable executables, use of a virtual machine is recommended.

## Instructions

1. [Connecting a JL programmer to the device](connecting-programmer-to-device.md)
2. [Setting up the compilation environment](sdk-setup.md)

## Useful reading and links

- [kagaimiq/jielie](https://github.com/kagaimiq/jielie)
- [kagaimiq/jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool)
