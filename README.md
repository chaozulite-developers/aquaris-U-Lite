WHAT IS THIS?
=============

Linux Kernel source code for the devices:
* bq aquaris U Lite


BUILD INSTRUCTIONS?
===================

Specific sources are separated by releases with it's corresponding number. First, you should
clone the project:

        $ git clone https://github.com/bq/aquaris-U-Lite.git

After it, choose the release you would like to build:

*Aquaris U Lite*

        $ mv aquaris-U-Lite kernel
        $ cd kernel
        $ git checkout tags/{release}

At the same level of the "kernel" directory:

Download a prebuilt gcc

        $ git clone https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8

Create KERNEL_OUT dir:

        $ mkdir KERNEL_OUT

Your directory tree should look like this:
* kernel
* arm-eabi-4.8
* KERNEL_OUT

Finally, build the kernel according the next table of product names:

| device                    | product                 |
| --------------------------|-------------------------|
| bq aquaris U Lite         | chaozulite              |


        $ make -C kernel  O=../KERNEL_OUT  ARCH=arm CROSS_COMPILE=../arm-eabi-4.8/bin/arm-eabi- {product}_defconfig
        $ make O=../KERNEL_OUT/ -C kernel ARCH=arm  CROSS_COMPILE=../arm-eabi-4.8/bin/arm-eabi-

You can specify "-j CORES" argument to speed-up your compilation, example:

        $ make O=../KERNEL_OUT/ -C kernel ARCH=arm  CROSS_COMPILE=../arm-eabi-4.8/bin/arm-eabi- -j 8

Thanks to:
==========

| BQ              | Provides kernel source                    |
| ----------------|-------------------------------------------|
| Jose Alberto    | For teach me how to oc                    |
| Maanush Putcha  | For help me with sources                  |
| Pablo Fraile    | For help me with WiFi                     |
| Jake            | For help me with WiFi                     |
| Tomac           | For help me with WiFi                     |
| Me              | For my hard work with this device         |
