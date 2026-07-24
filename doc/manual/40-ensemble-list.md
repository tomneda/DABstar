# Ensemble List

The **Ensemble List** is a dedicated window for cataloging and scanning DAB ensembles (groups of
radio stations) either from a live SDR device or from pre-recorded IQ sample files.

**How to open it:** Click the blue **EL** (Ensemble List) button in the main window toolbar.

## Two operation modes

Select the mode on the main window, left from the **EL** button.

| Mode                  | Description                                                                                                     |
|-----------------------|-------------------------------------------------------------------------------------------------------------------|
| Device / Channel scan | Scans all standard DAB channels via the connected SDR device and stores the result for each channel.            |
| File scan             | Scans a folder (or several folders) of recorded DAB IQ files and catalogs which ones contain valid signal data. |

![Ensemble List in device / channel scan mode](../../res/for_readme/ensemble-list-device.png){width=100%}

The table shows all known DAB channels with their scan status, ensemble name, ensemble ID, ITU
country code, signal quality (SNR/MER) and more.

![Ensemble List in file playback mode](../../res/for_readme/ensemble-list-file.png){width=100%}

In file mode you can add recorded DAB IQ sample files, have them scanned automatically and keep
track of which files contain usable DAB signals. The "Remove 'No signal' files" button helps clean
up the list of invalid recordings.

## Controls

| Control                                    | Mode      | Description                                                                                                                                                                                                                                       |
|------------------------------|:-------------|--------------------------------------------------------------------------|
| Reset database                             | both      | Clears the entire database for the current mode. Use this to force a complete rescan. *Note: rescanning a large set of files takes time.*                                                                                                         |
| Show current ensemble in service list only | both      | Filters the **Service List** in the main window to show the current selected ensemble only.                                                                                                                                                       |
| Auto Scan                                  | both      | Scans all entries currently visible in the list. Individual rows can also be scanned by clicking them directly.                                                                                                                                   |
| Remove 'No signal' files                   | file only | Removes entries identified as containing no valid DAB signal. The "No signal" filter checkbox must be active to enable this button.                                                                                                               |
| Folder with files to add                   | file only | Specifies the folder path whose files will be added to the database.                                                                                                                                                                              |
| Add files in folder to list                | file only | Adds all files from the selected folder to the database. Non-sample files are accepted and will be flagged as invalid during scanning. Minimum 4 MB per file; maximum 10 000 files to avoid system slowdown.                                      |
| Minimum file size (MB)                     | file only | Files smaller than this threshold are skipped when adding files to the database.                                                                                                                                                                  |
| Add a single file and play                 | file only | Adds one recording to the database and starts playback immediately. Replaces the former "Eject" button. If the file is already in the database it is selected and played. The minimum size threshold does not apply (only the hard 4 MB minimum). |

## Color coding in the table

The table uses background colors to give a quick status overview:

| Color    | Meaning                        |
|----------|--------------------------------|
| Greenish | Channel / file not yet scanned |
| Grayish  | Successfully scanned entry     |
| Reddish  | No signal detected (failed)    |

## Filter checkboxes

Three filter checkboxes control which entries are visible in the table.
Their state may be changed automatically when a certain scan situation arises.

| Checkbox    | Shows                                       |
|-------------|-----------------------------------------------|
| Not scanned | Entries that have not been scanned yet      |
| Scanned     | Entries that already have scan data         |
| No signal   | Entries where no valid DAB signal was found |

## Stored data per ensemble

The SQLite database stores the following information for each entry (exact fields differ between
device and file mode):

| Field               | Description                                                                        |
|---------------------|--------------------------------------------------------------------------------------|
| Channel / file path | DAB channel name (device mode) or full file path (file mode)                       |
| FId                 | Unique file identifier — hash of part of the file content (file mode only)         |
| Ensemble name & EId | Human-readable ensemble name and its numeric ensemble ID                           |
| ITU country code    | Country of origin encoded per ITU standard                                         |
| Frequency           | Nominal frequency (if known)                                                       |
| Offset              | The measured baseband offset                                                       |
| Error protection    | Error protection level and scheme parameters                                       |
| DSCTy / AppType     | Service type information, e.g. Journaline, EPG                                     |
| SNR / MER           | Signal-to-Noise Ratio and Modulation Error Ratio                                   |
| Last SId            | Last played Service ID — used to resume the same service after switching ensembles |
| Date                | The date (UTC) which is coded within the DAB signal                                |
| Scan level          | Highest scan level reached (not shown as a column in the list)                     |
