# DABstar

---

![screenshot.png](res/screenshot.png)
Pictures from Version 1.4.0

## Table of Content
<!-- TOC -->
  * [Introduction](#introduction)
  * [Changes in DABstar version 1.5.0](#changes-in-dabstar-version-150)
  * [Changes in DABstar version 1.4.0](#changes-in-dabstar-version-140)
  * [Changes in DABstar version 1.3.0](#changes-in-dabstar-version-130)
  * [Changes in DABstar version 1.2.0](#changes-in-dabstar-version-120)
    * [What is new?](#what-is-new)
    * [Known things that do not work yet](#known-things-that-do-not-work-yet)
    * [What is the DC Avoidance Algorithm?](#what-is-the-dc-avoidance-algorithm-)
  * [Changes in DABstar version 1.1.0](#changes-in-dabstar-version-110)
    * [What is new?](#what-is-new-)
    * [Things that do not work yet or anymore](#things-that-do-not-work-yet-or-anymore-)
    * [Changes to the Carrier Plot](#changes-to-the-carrier-plot)
  * [Changes in DABstar version 1.0.2](#changes-in-dabstar-version-102)
    * [How to apply TII info](#how-to-apply-tii-info)
  * [Changes in DABstar version 1.0.1](#changes-in-dabstar-version-101-)
    * [Graphical changes](#graphical-changes)
    * [Functional changes](#functional-changes)
    * [Code refactorings](#code-refactorings)
    * [Not more working or removed things](#not-more-working-or-removed-things)
<!-- TOC -->

## Introduction

**Current main branch version is [V1.5.0](#changes-in-dabstar-version-150).**

[DABstar](https://github.com/tomneda/DABstar) is originally forked from Jan van Katwijk's great work of [Qt-DAB](https://github.com/JvanKatwijk/qt-dab) 
from [commit](https://github.com/JvanKatwijk/qt-dab/commits/b083a8e169ca2b7dd47167a07b92fa5a1970b249) ([tree](https://github.com/JvanKatwijk/qt-dab/tree/b083a8e169ca2b7dd47167a07b92fa5a1970b249)) from 2023-05-30. Some fixes afterwards to Qt-DAB are included.

As there are meanwhile huge changes made from my side and there will be bigger changes in the future, 
I decided to give it the new name **DABstar**.

I will try to maintain always a working state on `main` branch.
Only when I change the MAJOR (1st digit) and/or MINOR (2nd digit) part of the version number (see [https://semver.org/](https://semver.org/) for nomenclature)
I will describe the changes here.
If I only raise the PATCH version number (3rd digit) when I provide (urgent) intermediate patches.

For at least each new version change in the MAJOR and/or MINOR part I will provide a version tag for easy referencing.
Please use the tags page on Github: [https://github.com/tomneda/DABstar/tags](https://github.com/tomneda/DABstar/tags)

For building there is one bigger difference to Qt-DAB: I maintain only one GUI version and I provide no *.pro file for qmake anymore, only a CMakeLists.txt file.
So, use only the cmake related installation process.

I saw that Qt-DAB 6.x uses also code parts and ideas from here. I am very appreciated about this :smiley:. 
This is of course very acknowledged that my work can give something back.

Still, I do not provide any precompiled setup packages, yet.

## Changes in DABstar version 1.5.0
                                   
- Still many refactorings made.
- Fix some issues (introduced by myself in the past).
- Some adaptions in the GUI regarding size of elements and colors. 
- Change the default soft-bit calculation strategy (was patch to V1.4.1).
- Avoid flickering in the spectrum scope due to the Null symbol.
- Add two new outputs for the carrier scope:
  - **Null Sym. no TII**: Shows the averaged Null symbol level without TII carriers in percentage of the maximum peak.
  - **Null Sym. ovr. Pow.**: Shows the averaged Null symbol (without TII) power relation to the averaged overall carrier power in dB. 
    This reveals disturbing (non-DAB) signals which can degrade decoding quality.

## Changes in DABstar version 1.4.0

- Move style sheet to local resource file.
- Improve (imo) the waterfall color scheme.
- Selector for soft-bit calculation algorithm on **Configuration and Control** widget.
- Add further soft-bit generation schemes (for experimenting while bad receiving conditions).
- Refine hugely (in the background) **Select Coordinates** widget and place clickable URLs on that for some maps websites to grab the coordinates there (still by hand). 

## Changes in DABstar version 1.3.0

- Further many refactorings and some fixes made.
- Many adaption regarding waterfall display (there is still room for improvement in the coloring).
- Some UI adaptions.
- Internal resources packed, so the executable footprint got smaller.
- The combo boxes in the **Scope Plot** are stored persistently.
- Remove **Scan Mode** selector and no asking for storing scan data to facilitate the scanning process.
- Change the default coloring after first installation of the different spectrum plots.
- New pause slide.

## Changes in DABstar version 1.2.0

### What is new?

- Many code and UI refactorings made and some minor fixes.
- Introduce a **DC Avoidance Algorithm** (default off). With that the RF frequency offset and baseband frequency offset are shown independently.
- Introduce a phase error correction per OFDM carrier. This can be seen in the Carrier Plot with **Corr. Phase Error**. The former **Mean Absolute Phase** plot got obsolete as this would only show a straight line after that correction.
  This phase correcton should be in most cases very minor, so no big effect (a better decoding) should be expected.
- The IQ plot can now show different views where one view shows the still not phase corrected versions. 
- Fixes in Carrier **Null Symbol TII** plot (wrong scaling calculation).

### Known things that do not work yet

- The settings of the current view of the IQ plot and Carrier plot cannot stored persistently.

### What is the DC Avoidance Algorithm? 

This new feature tries to avoid the DC component on used OFDM-bins around the 0Hz-OFDM-bin.

Many SDR devices have an analog IQ receiving concept. This concept tends to have a DC component at 0Hz baseband.

Therefore, the DAB standard do not use the OFDM-bin at 0Hz to avoid decoding problems. But as the frequency correction 
is usually done only in the baseband, the DC could be seen also in the neighbor OFDM-bins where the demodulation of the data could be badly influenced.

This feature tries to use the RF synthesizer on the SDR device to shift the DC component to the unsused 0Hz-OFDM-bin. The remaining frequency correction is still done in the baseband.

Not all devices allows fine RF frequency tuning (and not on all relevant frequencies), so this is not always working fully well, but should also not cause any harm.

This feature should make a difference in bad reception conditions. See needed RS (Reed-Solomon) correction.

Use the checkbox **Use DC avoidance algorithm** on the **Configuration and Control** page to control this feature. 
The audio could drop for a short time after switching.

## Changes in DABstar version 1.1.0
 
As there are many changes done in the UI and also in the background, I did a bigger step in the version numbering.

### What is new? 
 
- Many code refinements regarding SonarLint and ClangTidy in different modules.
- Refine coloring in the UI.
- New soft-bit evaluation for the Virterbi decoder. It should theoretically (imo) be better than the old one but I could not clearly prove that.
- Add further outputs to the **Carrier Plot** which are selectable (see more details below).
- Many changes in the processing of the OFDM decoder were necessary to realize the two points before.
- The buttons to open/close the widgets for **Controls**, **Spectrum**, **Details** are moved to the main widget.
- Add a frame around the picture and make its size fix.
 
### Things that do not work yet or anymore 
- The **IQ Scope** is prepared for different kinds of outputs but not yet selectable.
- The TII widget is removed at it is replaced by the **Carrier Plot**.
- The SNR widget is removed without replacement. I see no really need for that and I want to get rid of too many widgets.

### Changes to the Carrier Plot

The **Carrier Plot** are able to show different plots which are all related to the used 1536 OFDM carrier:

![](res/carrier_scope_combobox.png)

If the check-box **Nom carrier** (Nominal carrier) is checked then the carriers are sorted in the logical numbering after the frequency interleaver. But the following explained plots would not look nice if doing this. So better let this check-box unchecked.

- **Modulation Quality:** The value 0...100% shows the quality how sure the carrier can be decoded. This value is used for the soft bit decision at the viterbi decoder. The higher the better.  
    ![](res/CarrierPlots/ModulQuality.png)
- **Standard Deviation:** This is the basis for the Modulation Quality: the standard deviation (in °) of the noise of the phase. The lower the better.
    ![](res/CarrierPlots/StdDeviation.png)
- **Mean Absolute Phase:** This is the average over many OFDM symbols of the absolute decoded phase. Best is a straight line at 45°. A bigger frequency offset would show a slope here.
    ![](res/CarrierPlots/MeanAbsPhase.png)
- **4-quadrant Phase:** This shows the IQ Plot in another way: the current phase of the 4 possible quadrants. Best would be 4 (overlaid seen) "lines" at -135°, -45°, +45° and +135°.
    ![](res/CarrierPlots/4quadrPhase.png)
- **Relative Power:** As the absolute power is difficult to measure, this shows the power (in dB) relative to the medium over all carriers. A most straight line would imply the better signal quality (less fading).
    ![](res/CarrierPlots/RelativePower.png)  
- **S/N-Ratio:** This is ratio between the signal power and the noise power in dB. A higher value is better.  
    ![](res/CarrierPlots/SNRatio.png)
- **Null Symbol TII**: This is the replacement of the old TII Scope. It shows the TII (Transmitter Identification Information) carriers within certain Null Symbols. The content is shown decoded on the right corner in the main widget ("TII: ...").
    ![](res/CarrierPlots/NullSymbolTii.png)

## Changes in DABstar version 1.0.2

- Applied latest fixes from Qt-DAB up to [commit](https://github.com/JvanKatwijk/qt-dab/commit/775dc3d9411545ecd07480f625b499f292998818) from 2023-08-13.
- Minor changes in GUI.
- Further different class refinements.
- Handling of the TII distance info improved

### How to apply TII info

That the name, distance and direction to the transmitter can be shown, do following:

1) Provide your home coordinates with button **Coordinates** on the "Configuraton and Control" windows.
2) Copy **libtii-lib.so** from project sub folder **/tii-library** to **/usr/local/lib/**    (you will need sudo rights).
3) Press one time **Load Table** on the "Configuraton and Control" window.
4) If 3) should fail you can unzip the content of **/tii-library/tiiFile.zip** to your home folder. Restart DABstar. 
    Here you will maybe not have the newest in 3) downloaded version of the data base.


## Changes in DABstar version 1.0.1 

**Qt 5.15.2** is used for build.

### Graphical changes

- Added the correlation graph together with the spectrum/waterfall view.  
- Below the correlation graph it is shown the relative distance in kilometers and miles behind the second (and more) matched markers. 
- The IQ diagram is updated faster and lets show the data in logarithmic and linear scale (switchable).
- There is a new *Phase vs. Carrier* graph which shows the decoded phase component of each of the OFDM carrier. 
- Exchange the Qt-DAB color selector with the more flexible Qt color selector.
- Adapt the looking of the GUI (adapt style sheet).
- New desktop logos for program symbol. The main window symbol is brighter in the task bar to distinguish it from the other sub windows more easily.
- Simplify clock and runtime display.
- New DAB logo on pause slide:  
    <img  width="80" height="50" src="res/DABLogoCrop.png"/> 

### Functional changes

- Faster establishment to a new DAB channel (maybe this *could* be bad in poor receiving conditions).
- No modal message boxes anymore if a channel or service establishment fails. The message is shown red colored in the main window instead.
- Introduce a state machine in DAB processor to get rid of the `goto` and make the code more readable (imo).
- Fine phase adjustment in OFDM decoder (the "cross" in the IQ diagram should now looking balanced if it was not before).
- Many minor changes I almost forgot. :blush:

### Code refactorings

*(with RTL-SDR, HackRf, SdrPlay device libs included, other device libs not touched yet)*

- Do many code refactories regarding better readability (in my opinion) and higher compile speed (removing obsolete headers and code).
- Remove all gcc warnings.
- Find deep `std::vector` copies at method interfaces and replace them with const reference vectors.
- Find non-const input C array pointers and turned them to const (while this I found an unwanted memory changing issue which probably had no side effect).
- Use SonarLint on some code parts (this leads to e.g. using `std::array` templates).
- Make the FFT part to an own lib (was experimental and not really necessary).

### Not more working or removed things

- Coloring each button with an individual color is not more possible (took a huge amount of code).
- Remove obsolete code (with no functional lack).
- Remove the timer behind the mute button (I got really a shock as suddenly the muting timed out at a loud audio level :unamused: ).
- Remove the schedule feature (I had no use for that sophisticated feature and hinders doing the refactoring).
- The color selector for the graph background is removed as the stylesheet would overwrite this.

