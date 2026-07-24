#!/usr/bin/env bash
#
# Build the PDF version of the DABstar user manual.
#
# Usage: build-manual.sh [output.pdf]
#
# Needs pandoc and a XeLaTeX installation:
#   sudo apt-get install pandoc texlive-xetex texlive-latex-recommended \
#                        texlive-latex-extra texlive-fonts-recommended
#
# If XeLaTeX is not installed, tectonic (https://tectonic-typesetting.github.io) is used
# instead. That is a single binary needing no TeX installation, it downloads the LaTeX
# packages it needs on the first run.

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

OUTPUT="${1:-${SCRIPT_DIR}/DABstar-Manual.pdf}"

case "${1:-}" in
  -h|--help)
    sed -n '3,13p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit 0
    ;;
esac

usage_hint()
{
  echo "" >&2
  echo "On Debian/Ubuntu install them with:" >&2
  echo "  sudo apt-get install pandoc texlive-xetex texlive-latex-recommended \\" >&2
  echo "                       texlive-latex-extra texlive-fonts-recommended" >&2
}

if ! command -v pandoc >/dev/null 2>&1; then
  echo "error: pandoc not found in PATH" >&2
  usage_hint
  exit 1
fi

if command -v xelatex >/dev/null 2>&1; then
  PDF_ENGINE=xelatex
elif command -v tectonic >/dev/null 2>&1; then
  PDF_ENGINE=tectonic
else
  echo "error: neither xelatex nor tectonic found in PATH" >&2
  usage_hint
  exit 1
fi

# pandoc.yaml uses paths relative to the repository root
cd "${ROOT_DIR}"

# Only the numbered chapter files go into the PDF, in file name order.
CHAPTERS=()
while IFS= read -r chapter; do
  CHAPTERS+=("${chapter}")
done < <(find doc/manual -maxdepth 1 -name '[0-9]*.md' | sort)
if [ "${#CHAPTERS[@]}" -eq 0 ]; then
  echo "error: no chapter files (doc/manual/[0-9]*.md) found" >&2
  exit 1
fi

VERSION="$(sed -n 's/^ *project(DABstar VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt | head -n1)"
[ -n "${VERSION}" ] || VERSION="unknown"

# Prefer the date of the last change to the manual over the current date, so that rebuilding an
# unchanged manual gives the same document.
MANUAL_DATE="$(git log -1 --format=%cd --date=short -- doc/manual 2>/dev/null || true)"
[ -n "${MANUAL_DATE}" ] || MANUAL_DATE="$(date -u +%Y-%m-%d)"

echo "Building manual for DABstar ${VERSION} (${#CHAPTERS[@]} chapters, engine ${PDF_ENGINE}) ..."

pandoc \
  --defaults=doc/manual/pandoc.yaml \
  --pdf-engine="${PDF_ENGINE}" \
  --metadata=subtitle:"User Manual for Version ${VERSION}" \
  --metadata=date:"${MANUAL_DATE}" \
  --output="${OUTPUT}" \
  "${CHAPTERS[@]}"

echo "Written: ${OUTPUT}"
