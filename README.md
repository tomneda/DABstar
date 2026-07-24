# DABstar

## Table of Content
<!-- TOC -->
  * [Table of Content](#table-of-content)
  * [Documentation](#documentation)
  * [Pictures from V5.x.x](#pictures-from-v5xx)
    * [Main window](#main-window)
    * [Ensemble List window](#ensemble-list-window)
    * [Configuration window](#configuration-window)
    * [Scope Spectrum Correlation Statistic window](#scope-spectrum-correlation-statistic-window)
    * [Map view of received transmitters](#map-view-of-received-transmitters)
  * [Releases](#releases)
  * [Introduction and history](#introduction-and-history)
  * [What is new in V5.0.0](#what-is-new-in-v500)
  * [Installing on Linux from current mainline](#installing-on-linux-from-current-mainline)
    * [Installing VOLK](#installing-volk)
    * [Installing FDK-AAC](#installing-fdk-aac)
    * [Building DABstar](#building-dabstar)
    * [RTL-SDR driver installation](#rtl-sdr-driver-installation)
    * [Installing USRP UHD](#installing-usrp-uhd)
    * [Installing AirSpy](#installing-airspy)
  * [Licences](#licences)
    * [Journaline](#journaline)
<!-- TOC -->

---

## Documentation

How to *use* DABstar is described in the **[User Manual](doc/manual/README.md)**:

| Chapter                                                        | Content                                                               |
|----------------------------------------------------------------|-------------------------------------------------------------------------|
| [Introduction](doc/manual/10-introduction.md)                  | Device mode, file mode and how to read this manual                    |
| [Main Window](doc/manual/20-main-window.md)                    | The central window, playback and the status indicators                |
| [Service List](doc/manual/30-service-list.md)                  | Selecting services, favorites, sorting and stepping                   |
| [Ensemble List](doc/manual/40-ensemble-list.md)                | Cataloging and scanning channels and recorded files                   |
| [Configuration](doc/manual/50-configuration.md)                | The configuration and control window                                  |
| [Scope and Spectrum](doc/manual/60-scope-and-spectrum.md)      | Signal analysis windows                                               |
| [TII and Map](doc/manual/70-tii-and-map.md)                    | Transmitter identification and the map view                           |
| [Further Windows](doc/manual/80-further-windows.md)            | FIB content, Journaline, technical details, device and update dialogs |

A PDF version of the manual can be built with `./doc/manual/build-manual.sh`, see
[doc/manual/README.md](doc/manual/README.md).

This README covers the remaining topics: releases, project history and how to build and install
DABstar yourself.

---

## Pictures from V5.x.x

(More screenshots and their explanation are in the [User Manual](doc/manual/README.md).)

### Main window

![](res/for_readme/mainwidget.png)

(move over the status elements like **EEP 3-A**, **PS** or **SBR** to get tooltip information)

### Ensemble List window

The Ensemble List in device/channel scan mode. The table shows all known DAB channels with their
scan status, ensemble name, ensemble ID, ITU country code, signal quality (SNR/MER) and more.
Color coding gives a quick overview: not-yet-scanned channels appear greenish, successfully scanned
ones in a neutral gray, and channels with no detectable signal are highlighted in red.

![](res/for_readme/ensemble-list-device.png)

### Configuration window

![](res/for_readme/configuration.png)

(Use tooltips for detailed explanations)

### Scope Spectrum Correlation Statistic window

![](res/for_readme/scope-spec-corr.png)

### Map view of received transmitters

![](res/for_readme/map.png)

---

## Releases
For the latest AppImage versions see the
[Linux Release Page](https://github.com/tomneda/DABstar/releases/).

[old-dab](https://github.com/old-dab) also provides Windows versions, see the
[Windows Release Page](https://github.com/old-dab/DABstar/releases).
The feature-set could be a bit different between Linux and Windows, even with same version number.

## Introduction and history
[DABstar](https://github.com/tomneda/DABstar) was originally forked from Jan van Katwijk's great work of [Qt-DAB](https://github.com/JvanKatwijk/qt-dab)
from [commit](https://github.com/JvanKatwijk/qt-dab/commits/b083a8e169ca2b7dd47167a07b92fa5a1970b249) ([tree](https://github.com/JvanKatwijk/qt-dab/tree/b083a8e169ca2b7dd47167a07b92fa5a1970b249)) from 2023-05-30. Some fixes and adaptations from Qt-DAB made afterwards are included.

With what now amounts to (not just perceived) hundreds to thousands of hours of work, there are huge changes and additions (but also reductions) made and there will be bigger changes in the future,
I decided to give it the new name **DABstar**.

I noticed that since Qt-DAB 6.x, Qt-DAB also uses new code parts and ideas from here. I very much appreciate this :smiley:.
It is very gratifying that my work can give something back.

I will try to maintain always a working state on the head of the `main` branch, so use this for building the software for yourself.
**Please do not try any other branch besides `main` branch.**
They are intended for development, backups and tests and will not always work in their current state.

Recommended is to checkout a [Tag](https://github.com/tomneda/DABstar/tags) of a released version, these are better tested.

## What is new in V5.0.0

The biggest new addition in this development cycle is the **Ensemble List** — a dedicated window
for cataloging and scanning DAB ensembles (groups of radio stations) either from a live SDR
device or from pre-recorded IQ sample files.

It is described in the [Ensemble List](doc/manual/40-ensemble-list.md) chapter of the manual.

## Installing on Linux from current mainline

This is what I needed to install DABstar on a fresh Ubuntu 24.04:
```
sudo apt-get update
sudo apt-get install git
sudo apt-get install cmake
sudo apt-get install build-essential
sudo apt-get install g++
sudo apt-get install libsndfile1-dev
sudo apt-get install libfftw3-dev
sudo apt-get install zlib1g-dev
sudo apt-get install libsamplerate0-dev
sudo apt-get install libusb-dev
sudo apt-get install libusb-1.0-0-dev
sudo apt-get install qt6-base-dev
sudo apt-get install qt6-multimedia-dev
sudo apt-get install qt6-charts-dev
```

### Installing VOLK

Several parts (not only the OFDM decoder) use the [VOLK](https://www.libvolk.org/)
(Vector-Optimized Library of Kernels) library for vectorized signal-processing operations.
This noticeably reduces CPU load especially on machines with SSE/AVX capable processors.
VOLK version 3.3.0 or newer is recommended.

Ubuntu 24.04 ships VOLK 3.1; install the packaged version or build from source for 3.3.0 or newer:
```
sudo apt-get install libvolk-dev
```
or build from source:
```
git clone --recursive https://github.com/gnuradio/volk.git
cd volk
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
sudo make install
sudo ldconfig
```
Enable VOLK in the DABstar build by adding `-DSSE_OR_AVX=ON` to the `cmake` command (see below).

### Installing FDK-AAC

As libfaad had issues with low-rate services I switched over to FDK-AAC.
But also the repository version in Ubuntu 24.04 has still flaws with PS (Parametric Stereo) services.
So I recommend to build it for your own. I used the latest main version which is v2.0.3.
```
git clone https://github.com/mstorsjo/fdk-aac.git
cd fdk-aac
git checkout v2.0.3
mkdir build
cd build
cmake ..
make
sudo make install
```

### Building DABstar
```
git clone https://github.com/tomneda/DABstar.git
cd DABstar
mkdir build
cd build
cmake .. -DAIRSPY=ON -DSDRPLAY=ON -DHACKRF=ON -DLIMESDR=ON -DRTL_TCP=ON -DPLUTO=ON -DUHD=ON -DRTLSDR=ON -DVITERBI_SSE2=ON -DVITERBI_NEON=OFF -DFDK_AAC=ON -DSSE_OR_AVX=ON
make
```
Reduce or adapt the `cmake` command line for the devices/features you need.

E.G.: If you have an RTL-SDR stick and work on a desktop PC (I have only tested this on an Intel-PC), this should be the minimum recommendation:
```
cmake .. -DRTLSDR=ON -DVITERBI_SSE2=ON -DSSE_OR_AVX=ON
```

To speed up compilation you can provide `-j<n>` as argument with `<n>` number of threads after the `make` command. E.G. `make -j4`.
Do not choose a too high number (or at my side only providing a `-j`) the system can hang due to running out memory and needed swapping!

Finally, in the build folder you can find the program file which you can start with
```
./dabstar
```

You could try to install the software within your system with
```
sudo make install
sudo ldconfig
```
To uninstall DABstar again, do this:
```
sudo make uninstall
```

### RTL-SDR driver installation

It is recommended to build the RTL-SDR library from source from the repository of old-dab. See https://github.com/old-dab/rtlsdr.

It is also recommended then to remove the default driver, if installed.

If you are using the default driver while running DABStar you will get a message box hinting on that but DABstar will somehow work with that also.

The driver from old-dab should still be compatible with other applications using this driver.

Remove the default driver with:
```
sudo apt remove rtl-sdr librtlsdr-dev
sudo ldconfig
```


If you nevertheless want to use the default version from Ubuntu (but this is not recommended as the driver from old-dab has some improvements):
```
sudo apt install rtl-sdr librtlsdr-dev
sudo ldconfig
```

### Installing USRP UHD

Best worked for me was building UHD from the repository of Ettus Research.

```
sudo add-apt-repository ppa:ettusresearch/uhd
sudo apt-get update
sudo apt-get install libuhd-dev uhd-host
```

### Installing AirSpy

Details see https://github.com/airspy/airspyone_host

```
git clone https://github.com/airspy/airspyone_host.git
cd airspyone_host
mkdir build
cd build
cmake ../ -DINSTALL_UDEV_RULES=ON
make
sudo make install
sudo ldconfig
```

## Licences

Rights of Qt-DAB, AbracaDABra, Qt, FFTW, VOLK, FDK-AAC, libfaad, libsamplerate and libsndfile gratefully acknowledged.

Rights of developers of RTLSDR library, SDRplay libraries, AIRspy library and others gratefully acknowledged.

Rights of other contributors gratefully acknowledged.

As I use some icons, I get them from [FlatIcon](https://www.flaticon.com/). The work of the icon authors is gratefully acknowledged:

| Icon set                                                                     | Author        |
|------------------------------------------------------------------------------|---------------|
| [Electromagnetic icons](https://www.flaticon.com/free-icons/electromagnetic) | muh zakaria   |
| [Radio tower icons](https://www.flaticon.com/free-icons/radio-tower)         | sonnycandra   |
| [Frequency icons](https://www.flaticon.com/free-icons/frequency)             | DinosoftLabs  |
| [Spectrum icons](https://www.flaticon.com/free-icons/spectrum)               | JunGSa        |
| [Spectrum icons](https://www.flaticon.com/free-icons/spectrum)               | Eucalyp       |
| [Target icons](https://www.flaticon.com/free-icons/target)                   | Pixel perfect |
| [Folder icons](https://www.flaticon.com/free-icons/folder)                   | gariebaldy    |

### Journaline

Features NewsService Journaline(R) decoder technology by
Fraunhofer IIS, Erlangen, Germany.
For more information visit http://www.iis.fhg.de/dab
