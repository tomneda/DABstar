# Further Windows

## Technical details of the selected service

![Technical details window of the selected service](../../res/for_readme/tech-details.png){width=45%}

This window shows the technical parameters of the service that is currently played — protection
level, bit rate, sub-channel data and the decoder state.

## FIB content

![FIB content window](../../res/for_readme/fib-content.png){width=100%}

The Fast Information Block (FIB) carries the Fast Information Groups (FIGs) that describe the
ensemble: services, sub-channels, time, announcements and more. This window shows the decoded
content as it arrives, which is mainly of interest when analysing an ensemble.

An overview of the individual FIG types is kept with the developer documentation in
[`doc/FIG-Overview`](../FIG-Overview).

## Journaline

![Journaline window](../../res/for_readme/journaline.png){width=85%}

Journaline is a text based news service carried in the data part of a DAB ensemble. If a service
offers Journaline, its pages can be browsed in this window.

Features NewsService Journaline(R) decoder technology by Fraunhofer IIS, Erlangen, Germany.
For more information visit <http://www.iis.fhg.de/dab>.

## Device windows

Each supported SDR device has its own window with the controls of that device — gain, AGC, bandwidth
and, depending on the hardware, further settings. Open it with the
![](../../res/for_readme/device-button.png){height=1.1em} button.

![RTL-SDR device window](../../res/for_readme/device-rtlsdr.png){width=60%}

![HackRF device window](../../res/for_readme/device-hackrf.png){width=60%}

![SDRplay device window](../../res/for_readme/device-sdrplay.png){width=60%}

A good leveling of the device is essential for reliable reception, see
[Some help for scanning](30-service-list.md#some-help-for-scanning).

## Version update dialog

![The DABstar version update dialog](../../res/for_readme/update.png){width=70%}

DABstar can check whether a newer version has been released and shows the result in this dialog.

(Unfortunately, the link coloring could not be made more readable; it looks better in Windows.)
