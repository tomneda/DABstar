# Introduction

**DABstar** is a software defined radio (SDR) receiver for DAB and DAB+ broadcasts.
It decodes the signal delivered by a connected SDR device or read from a recorded IQ sample file,
plays the audio and shows the data services carried within the ensemble.

This manual describes how to *operate* DABstar. For downloading, building and installing the program,
and for the project history, see the [README](../../README.md) in the repository root.

## Scope of this manual

| Chapter                                    | Content                                                          |
|--------------------------------------------|------------------------------------------------------------------|
| [Main Window](20-main-window.md)           | The central window, playback and the status indicators            |
| [Service List](30-service-list.md)         | Selecting services, favorites, sorting and stepping               |
| [Ensemble List](40-ensemble-list.md)       | Cataloging and scanning channels and recorded files               |
| [Configuration](50-configuration.md)       | The configuration and control window                              |
| [Scope and Spectrum](60-scope-and-spectrum.md) | Signal analysis windows                                           |
| [TII and Map](70-tii-and-map.md)           | Transmitter identification and the map view                       |
| [Further Windows](80-further-windows.md)   | FIB content, Journaline, technical details, device and update dialogs |

## A note on tooltips

Many controls, indicators and diagrams in DABstar carry a tooltip with a detailed explanation.
Wherever this manual stays brief, hovering the mouse over the element in question is usually the
fastest way to get the full story. This is especially true for the
[Configuration](50-configuration.md) window and the [Scope and Spectrum](60-scope-and-spectrum.md)
displays, where nearly every element is documented that way.

## Device mode and file mode

DABstar can be fed from two sources, and several features behave differently depending on which
one is active:

| Mode        | Signal source                                                                 |
|-------------|--------------------------------------------------------------------------------|
| Device mode | A connected SDR device (RTL-SDR, Airspy, SDRplay, HackRF, LimeSDR, PlutoSDR, USRP, RTL-TCP) |
| File mode   | A pre-recorded DAB IQ sample file that is played back                          |

The mode is selected in the main window, left of the **EL** (Ensemble List) button.
