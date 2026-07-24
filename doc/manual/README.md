# DABstar User Manual

The manual is written in Markdown so that it can be read directly here on GitHub, and it is
rendered to a PDF with [pandoc](https://pandoc.org/) for offline and printed use.

## Chapters

1. [Introduction](10-introduction.md)
2. [Main Window](20-main-window.md)
3. [Service List](30-service-list.md)
4. [Ensemble List](40-ensemble-list.md)
5. [Configuration](50-configuration.md)
6. [Scope and Spectrum](60-scope-and-spectrum.md)
7. [TII and Map](70-tii-and-map.md)
8. [Further Windows](80-further-windows.md)

Building and installing DABstar is described in the [README](../../README.md) of the repository
root.

## Building the PDF

```
./doc/manual/build-manual.sh
```

The script can be run from anywhere; it always builds against the repository root and writes
`doc/manual/DABstar-Manual.pdf` (the output path can be given as first argument).

Alternatively, if you configured the project with CMake:

```
cmake --build <build-dir> --target manual
```

### Requirements

| Tool         | Debian / Ubuntu package                                                                 |
|--------------|-------------------------------------------------------------------------------------------|
| pandoc       | `pandoc`                                                                                |
| XeLaTeX      | `texlive-xetex texlive-latex-recommended texlive-latex-extra texlive-fonts-recommended` |

If you do not want to install a full TeX distribution, install
[tectonic](https://tectonic-typesetting.github.io) instead — a single binary that downloads the
LaTeX packages it needs on its first run. `build-manual.sh` uses XeLaTeX when it is available and
falls back to tectonic otherwise.

## Writing rules for this manual

* One chapter per file, named `<number>-<topic>.md`. Only files matching `[0-9]*.md` go into the
  PDF, and they are concatenated in file name order — leave gaps in the numbering so chapters can
  be inserted later.
* Each file starts with exactly one level-1 heading (`#`), which becomes a PDF chapter.
* Image paths are relative to this folder, i.e. `../../res/for_readme/<file>.png`. This works both
  on GitHub and in the PDF build.
* Always give images a size attribute, otherwise wide screenshots overflow the PDF text block:
  * full width screenshot: `{width=100%}`
  * inline button icon: `{height=1.1em}`
* Text in the alt position becomes the figure caption in the PDF, so use it for real screenshots
  and leave it empty for inline icons.
* Cross-chapter links use the plain file name (`[Ensemble List](40-ensemble-list.md)`); pandoc
  turns them into internal PDF links.
