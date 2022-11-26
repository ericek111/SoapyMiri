# SoapyMiri
## Installation

For Arch Linux, there's a package in the AUR: https://aur.archlinux.org/packages/soapymiri-git

For other distributions, clone and compile manually:

    cd SoapyMiri
    mkdir build
    cd build
    cmake ..
    make -j4
    make install

## Dependencies

### Libmirisdr
This driver works with the [**libmirisdr-5**](https://github.com/ericek111/libmirisdr-5) library -- an open-source version reverse-engineered by Miroslav Slugen.

It is much more stable than the proprietary SDRplay API (that presents a serious security risk -- it needs to run under root). It doesn't need to run in the background, doesn't need to be restarted and thanks to LibUSB, it can work even on Android devices inside chroot.

### SoapySDR

For this to compile and run, you need to have [Pothosware/SoapySDR](https://github.com/pothosware/SoapySDR) installed.

## Usage

### Gains
There are 5 individual gains. While it may be confusing, the method of operation is pretty straightforward -- trial and error. :)

One of them is called "Automatic". This uses an algorithm to configure all the other gains to hopefully provide a linear and easily understandable gain control.

However, if this is not enough for you and you really want to get the best performance (signal to noise ratio, image rejection) out of your SDR, you can control the individual amplification stages inside your SDR using the 4 other gains (LNA, Mixer, Mixbuffer and Baseband).