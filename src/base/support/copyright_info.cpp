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
#include <fftw3.h>
#include <sndfile.h>
#include <zlib.h>
#ifdef HAVE_SSE_OR_AVX
  #include <volk/volk.h>
#endif
#ifdef __WITH_FDK_AAC__
  #include <aacdecoder_lib.h>
#else
  #include <neaacdec.h> // faad2
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

// Create a link to iUrl but show iText (e.g. the library name and version) as the clickable label.
QString hyperlink_text(const QString & iUrl, const QString & iText)
{
  return QStringLiteral("<a href=\"%1\">%2</a>").arg(iUrl, iText);
}

QString get_copyright_text()
{
#ifdef HAVE_SSE_OR_AVX
  QString volkVers = hyperlink_text("https://github.com/gnuradio/volk", QString("Volk %1.%2.%3").arg(VOLK_VERSION_MAJOR).arg(VOLK_VERSION_MINOR).arg(VOLK_VERSION_MAINT)) + "<br/>";
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
  // QString fdkVers = QString(libInfo[i].title) + " " + QString(libInfo[i].versionStr) + "<br/>";
  QString fdkVers = hyperlink_text("https://github.com/mstorsjo/fdk-aac", "FDK-AAC " + QString(libInfo[i].versionStr)) + "<br/>";
  QString faadVers;
#else
  QString usedDecoder = ", libfaad";
  char * faadIdString = nullptr;
  char * faadCopyrightString = nullptr;
  NeAACDecGetVersion(&faadIdString, &faadCopyrightString);
  QString faadVers = hyperlink_text("https://github.com/knik0/faad2", (faadIdString != nullptr ? "Faad " + QString(faadIdString) : QString("Faad unknown"))) +
                     // "  (" + (faadCopyrightString != nullptr ? QString(faadCopyrightString) : QString("")) + ")"
                     + "<br/>";
  QString fdkVers;
#endif

#ifdef HAVE_LIQUID
  QString liquidVers = hyperlink_text("https://github.com/jgaeddert/liquid-dsp", "liquid-dsp " + QString(LIQUID_VERSION)) + "<br/>";
  QString useLiquid = ", liquid-DSP";
#else
  QString liquidVers;
  QString useLiquid;
#endif

  QString versionText = "<html><head/><body>";
  versionText += "<h3>" + QString(PRJ_NAME) + " " + PRJ_VERS + "</h3>";
  // __TIMESTAMP__ seems to use the file date not the compile date, so use __DATE__/__TIME__ instead
  versionText += "<p><b>Built on " + QString(__DATE__) + "&nbsp;&nbsp;" + QString(__TIME__) + "<br/>Commit " + QString(GITHASH) + "</b></p>";
  versionText += "<p><b>Used libraries with version:</b><br/>" +
                 hyperlink_text("https://www.qt.io", "Qt " QT_VERSION_STR) + "<br/>" +
                 volkVers +
                 hyperlink_text("https://www.fftw.org", QString(fftwf_version)) + "<br/>" +
                 faadVers +
                 fdkVers +
                 hyperlink_text("https://github.com/libsndfile/libsndfile", QString(sf_version_string())) + "<br/>" +
                 liquidVers +
                 hyperlink_text("https://www.zlib.net", "zlib " + QString(ZLIB_VERSION)) +
                 "</p>";
  versionText += "<p>Forked from Qt-DAB, then extensively changed, extended and partly reduced, by Thomas Neder "
                 "(" + hyperlink("https://github.com/tomneda/DABstar") + ").<br/>"
                 "For Qt-DAB see " + hyperlink("https://github.com/JvanKatwijk/qt-dab") + " by Jan van Katwijk "
                 "(" + hyperlink("J.vanKatwijk@gmail.com", true) + ").</p>";
  versionText += "<p>Rights of Qt, FFTW" + usedDecoder + useVolk + useLiquid + ", libsndfile and zlib gratefully acknowledged.<br/>"
                 "Rights of developers of " + hyperlink_text("https://github.com/old-dab/rtlsdr", "RTLSDR library") + " (using the improved fork from old-dab), "
                 + hyperlink_text("https://www.sdrplay.com", "SDRplay libraries") + ", "
                 + hyperlink_text("https://github.com/airspy/airspyone_host", "AIRspy library") + " and others gratefully acknowledged.<br/>"
                 "Rights of other contributors gratefully acknowledged.</p>";
  versionText += "<p>Features NewsService Journaline(R) decoder technology by Fraunhofer IIS, Erlangen, Germany.<br/>"
                 "For more information visit " + hyperlink("http://www.iis.fhg.de/dab") + ".</p>";
  versionText += "</body></html>";
  return versionText;
}
