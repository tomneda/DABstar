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

template <typename T>
QString hyperlink(const T & iUrl, const bool iIsMail = false)
{
  if (iIsMail) return QStringLiteral("<a href=\"mailto:%1\">%1</a>").arg(iUrl);
  else         return QStringLiteral("<a href=\"%1\">%1</a>").arg(iUrl);
}

QString get_copyright_text()
{
#ifdef HAVE_SSE_OR_AVX
  QString volkVers = QString("Volk %1.%2.%3").arg(VOLK_VERSION_MAJOR).arg(VOLK_VERSION_MINOR).arg(VOLK_VERSION_MAINT) + "<br>";
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
  // QString fdkVers = QString(libInfo[i].title) + " " + QString(libInfo[i].versionStr) + "<br>";
  QString fdkVers = "FDK-AAC " + QString(libInfo[i].versionStr) + "<br>";
  QString faadVers;
#else
  QString usedDecoder = ", libfaad";
  QString faadVers = "Faad " + QString(FAAD2_VERSION) + "<br>";
  QString fdkVers;
#endif

#ifdef HAVE_LIQUID
  QString liquidVers = "liquid-dsp " + QString(LIQUID_VERSION) + "<br>";
  QString useLiquid = ", liquid-DSP";
#else
  QString liquidVers;
  QString useLiquid;
#endif

  QString versionText = "<html><head/><body><p>";
  versionText = "<h3>" + QString(PRJ_NAME) + " " + PRJ_VERS + "</h3>";
  versionText += "<p><b>Built on " + QString(__DATE__) + "&nbsp;&nbsp;" + QString(__TIME__) + QString("<br/>Commit ") + QString(GITHASH) + "</b></p>"; // __TIMESTAMP__ seems to use the file date not the compile date
  versionText += "<p><b>Used libs with version:</b><br>"
                 "Qt " QT_VERSION_STR "<br>"
                 "Qwt " QWT_VERSION_STR "<br>" +
                 volkVers +
                 fftwf_version  + "<br>" +
                 faadVers +
                 fdkVers +
                 sf_version_string() + "<br>" +
                 liquidVers +
                 "zlib " + ZLIB_VERSION +
                 "</p>";
  versionText += "<p>Forked from Qt-DAB, partly extensive changed, extended, some things also removed, by Thomas Neder "
                 "(" + hyperlink("https://github.com/tomneda/DABstar") + ").<br/>"
                 "For Qt-DAB see " + hyperlink("https://github.com/JvanKatwijk/qt-dab") + " by Jan van Katwijk<br/>"
                 "(" + hyperlink("J.vanKatwijk@gmail.com", true) + ").</p>";
  versionText += "<p>Rights of Qt, Qwt, FFTW" + usedDecoder + useVolk + useLiquid + ", libsndfile and zlib gratefully acknowledged.<br/>"
                 "Rights of developers of RTLSDR library, SDRplay libraries, AIRspy library and others gratefully acknowledged.<br/>"
                 "Rights of other contributors gratefully acknowledged.</p>";
  versionText += "Features NewsService Journaline(R) decoder technology by Fraunhofer IIS, Erlangen, Germany. For more information visit "
                 + hyperlink("http://www.iis.fhg.de/dab");
  versionText += "</p></body></html>";
  return versionText;
}
