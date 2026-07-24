# Main Window

![The DABstar main window](../../res/for_readme/mainwidget.png)

The main window is the central control of DABstar. It contains the
[service list](30-service-list.md) on the left, the currently played service with its dynamic label
and slide show (MOT) picture in the middle, and the buttons opening the other windows.

Move the mouse over the status elements like **EEP 3-A**, **PS** or **SBR** to get tooltip
information.

## Status indicators

The status bar below the MOT picture shows:

| Indicator | Meaning                                                                            |
|-----------|--------------------------------------------------------------------------------------|
| EEP 3-A   | The current (Viterbi) protection mode                                              |
| 72 kbps   | AAC decoder input bit rate (including MOT data)                                    |
| 48 kSps   | The sample rate to the audio signal                                                |
| Stereo    | Whether stereo mode is active                                                      |
| PS        | Whether Parametric Stereo mode is active                                           |
| SBR       | Whether Spectral Band Replication mode is active                                   |
| MOT       | A Media Object Transfer is currently happening (e.g. Slide Show)                   |
| EPG       | Whether Electronic Program Guide data are available (needs further implementation) |
| Ann       | Whether an announcement is currently being broadcast                               |
| RS        | For DAB+: The Reed Solomon decoder has detected errors (and almost repaired them)  |
| CRC       | The RS decoder was not able to repair the error. Audible noise may occur           |

## The clock

The clock at the top of the service list can be used as a reception indicator. Its time is only
shown, and its background lights up, if the DAB time information can be received. The time is the
one coded within the DAB signal, not the system time of the computer.

## Buttons

| Button                                                        | Function                                                     |
|---------------------------------------------------------------|----------------------------------------------------------------|
| **EL**                                                        | Opens the [Ensemble List](40-ensemble-list.md)               |
| ![](../../res/for_readme/device-button.png)     | Opens the device widget of the currently selected device     |
| ![](../../res/for_readme/scope-button.png)      | Opens the [Spectrum Scope](60-scope-and-spectrum.md)         |
| ![](../../res/for_readme/target-button.png)     | Scrolls the service list to the current service              |
| ![](../../res/for_readme/favorite-button.png)   | Toggles the favorite state of the current service            |
| ![](../../res/for_readme/updown-buttons.png)    | Steps one service up or down in the service list             |
