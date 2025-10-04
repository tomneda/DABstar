/*
 * Copyright (c) 2025 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "copyright_info.h"
#include <qwt.h>
#include <fftw3.h>
#include <sndfile.h>
#include <zlib.h>
#ifdef HAVE_SSE_OR_AVX
  #include <volk/volk.h>
#endif
#ifdef __WITH_FDK_AAC__
  #include <aacdecoder_lib.h>
#else
  #include <faad.h> // faad2
#endif
#ifdef HAVE_LIQUID
  #include <liquid/liquid.h>
#endif

QString get_copyright_text()
{
#ifdef HAVE_SSE_OR_AVX
  QString volkVers = QString(", Volk %1.%2.%3").arg(VOLK_VERSION_MAJOR).arg(VOLK_VERSION_MINOR).arg(VOLK_VERSION_MAINT);
  QString useVolk = ", Volk";
#else
  QString volkVers;
  QString useVolk;
#endif
#ifdef __WITH_FDK_AAC__
  int i;
  QString usedDecoder = ", FDK-AAC";
  LIB_INFO libInfo[FDK_MODULE_LAST];
  FDKinitLibInfo(libInfo);
  aacDecoder_GetLibInfo(libInfo);
  for (i = 0; i < FDK_MODULE_LAST; i++)
  {
    if (libInfo[i].module_id == FDK_AACDEC) break;
  }
  QString fdkVers = ", " + QString(libInfo[i].title) + " " + QString(libInfo[i].versionStr);
  QString faadVers;
#else
  QString usedDecoder = ", libfaad";
  QString faadVers = ", Faad " + QString(FAAD2_VERSION);
  QString fdkVers;
#endif
#ifdef HAVE_LIQUID
  QString liquidVers = ", liquid " + QString(LIQUID_VERSION);
  QString useLiquid = ", liquid-DSP";
#else
  QString liquidVers;
  QString useLiquid;
#endif

  QString versionText = "<html><head/><body><p>";
  versionText = "<h3>" + QString(PRJ_NAME) + " " + PRJ_VERS + "</h3>";
  versionText += "<p><b>Built on " + QString(__DATE__) + "&nbsp;&nbsp;" + QString(__TIME__) + QString("<br/>Commit ") + QString(GITHASH) + "</b></p>"; // __TIMESTAMP__ seems to use the file date not the compile date
  versionText += "<p><b>Used libs: Qt " QT_VERSION_STR ", Qwt " QWT_VERSION_STR + volkVers + ", " + fftwf_version + faadVers + fdkVers + ", " + sf_version_string() + liquidVers + ", zlib " + ZLIB_VERSION + "</b></p>";
  versionText += "<p>Forked from Qt-DAB, partly extensive changed, extended, some things also removed, by Thomas Neder "
                 "(<a href=\"https://github.com/tomneda/DABstar\">https://github.com/tomneda/DABstar</a>).<br/>"
                 "For Qt-DAB see <a href=\"https://github.com/JvanKatwijk/qt-dab\">https://github.com/JvanKatwijk/qt-dab</a> by Jan van Katwijk<br/>"
                 "(<a href=\"mailto:J.vanKatwijk@gmail.com\">J.vanKatwijk@gmail.com</a>).</p>";
  versionText += "<p>Rights of Qt, Qwt, FFTW" + usedDecoder + useVolk + useLiquid + ", libsndfile and zlib gratefully acknowledged.<br/>"
                 "Rights of developers of RTLSDR library, SDRplay libraries, AIRspy library and others gratefully acknowledged.<br/>"
                 "Rights of other contributors gratefully acknowledged.</p>";
  versionText += "</p></body></html>";
  return versionText;
}
