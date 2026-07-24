# Scope and Spectrum

The signal analysis windows give the most detailed feedback about the reception quality. They are
the right place to look when leveling the device or when judging whether a channel is worth a scan.

Open the **Spectrum Scope** with the ![](../../res/for_readme/scope-button.png)
button in the main window.

## Spectrum, correlation and statistics

![Scope, spectrum, correlation and statistics window](../../res/for_readme/scope-spec-corr.png)

This window combines several diagrams. Explaining every detail here would go far beyond the scope of
this manual — each diagram and each control carries a tooltip with the necessary explanation, so
hover the mouse over the element you are interested in.

## PRS correlation over a whole DAB frame (CIR)

![PRS correlation over a whole DAB frame](../../res/for_readme/corr-over-frame.png)

This display shows the correlation of the Phase Reference Symbol over a complete DAB frame, which
corresponds to the Channel Impulse Response (CIR). Each peak is an echo path, so this view makes
multipath propagation and the delay of the individual transmitters visible. Together with the
[TII list](70-tii-and-map.md) it is the basis for identifying which transmitters are being received.

<!--
TODO(manual): The following screenshots exist in res/for_readme/ but were never referenced from the
old README, so no description was carried over. They appear to belong to this chapter. Add prose and
figures for them, or delete the files if they are obsolete:
  SNRatio.png, ModulQuality.png, StdDeviation.png, RelativePower.png,
  MeanAbsPhase.png, 4quadrPhase.png, NullSymbolTii.png
-->
