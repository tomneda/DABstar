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
#include "git_hash.h"
#include "qt_compat.h"
#include <QLibrary>
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
  if (iIsMail) return QSL("<a href=\"mailto:%1\">%1</a>").arg(iUrl);
  else         return QSL("<a href=\"%1\">%1</a>").arg(iUrl);
}

// Create a link to iUrl but show iText (e.g. the library name and version) as the clickable label.
QString hyperlink_text(const QString & iUrl, const QString & iText)
{
  return QSL("<a href=\"%1\">%2</a>").arg(iUrl, iText);
}

#ifdef HAVE_RTLSDR
static QString get_rtlsdr_version_string()
{
  const char * libraryString = "librtlsdr";
  QLibrary lib(libraryString);
  if (!lib.load())
  {
    return {};
  }

  using pfn_rtlsdr_get_version = uint32_t (*)();
  using pfn_rtlsdr_get_ver_id  = const char * (*)();

  const auto get_version = reinterpret_cast<pfn_rtlsdr_get_version>(lib.resolve("rtlsdr_get_version"));
  const auto get_ver_id  = reinterpret_cast<pfn_rtlsdr_get_ver_id>(lib.resolve("rtlsdr_get_ver_id"));

  QString result;
  if (get_version != nullptr)
  {
    const uint32_t v = get_version();
    const uint32_t major = (v >> 24) & 0xFF;
    const uint32_t minor = (v >> 16) & 0xFF;
    const uint32_t micro = (v >> 8)  & 0xFF;
    const uint32_t nano  = v & 0xFF;

    if (nano > 0)
    {
      result = QString::asprintf("RTL-SDR (old-dab) %u.%u.%u.%u", major, minor, micro, nano);
    }
    else
    {
      result = QString::asprintf("RTL-SDR (old-dab) %u.%u.%u", major, minor, micro);
    }

    if (get_ver_id != nullptr)
    {
      const char * verId = get_ver_id();
      if (verId != nullptr)
      {
        const QString idStr = QString::fromUtf8(verId);
        const int dateStart = idStr.indexOf(QLatin1Char('('));
        if (dateStart != -1)
        {
          result += QSL(" ") + idStr.mid(dateStart);
        }
      }
    }
  }
  else
  {
    result = QSL("RTL-SDR (osmocom / unknown version)");
  }

  lib.unload();
  return result;
}
#endif

QString get_copyright_text()
{
#ifdef HAVE_SSE_OR_AVX
  const QString volkVers = hyperlink_text(QSL("https://github.com/gnuradio/volk"), QSL("Volk %1.%2.%3").arg(VOLK_VERSION_MAJOR).arg(VOLK_VERSION_MINOR).arg(VOLK_VERSION_MAINT)) + QSL("<br/>");
  const QString useVolk = QSL(", VOLK");
#else
  QString volkVers;
  QString useVolk;
#endif

#ifdef HAVE_RTLSDR
  const QString rtlSdrInfo = get_rtlsdr_version_string();
  QString rtlSdrVers;
  if (!rtlSdrInfo.isEmpty())
  {
    rtlSdrVers = hyperlink_text(QSL("https://github.com/old-dab/rtlsdr"), rtlSdrInfo) + QSL("<br/>");
  }
#else
  QString rtlSdrVers;
#endif

#ifdef __WITH_FDK_AAC__
  int i;
  QString usedDecoder = QSL(", FDK-AAC");
  LIB_INFO libInfo[FDK_MODULE_LAST];
  FDKinitLibInfo(libInfo);
  aacDecoder_GetLibInfo(libInfo);
  for (i = 0; i < FDK_MODULE_LAST; i++)
  {
    if (libInfo[i].module_id == FDK_AACDEC) break;
  }
  // QString fdkVers = QString(libInfo[i].title) + " " + QString(libInfo[i].versionStr) + "<br/>";
  QString fdkVers = hyperlink_text(QSL("https://github.com/mstorsjo/fdk-aac"), QSL("FDK-AAC ") + QString::fromUtf8(libInfo[i].versionStr)) + QSL("<br/>");
  QString faadVers;
#else
  QString usedDecoder = QSL(", libfaad");
  char * faadIdString = nullptr;
  char * faadCopyrightString = nullptr;
  NeAACDecGetVersion(&faadIdString, &faadCopyrightString);
  QString faadVers = hyperlink_text(QSL("https://github.com/knik0/faad2"), (faadIdString != nullptr ? QSL("Faad ") + QString::fromUtf8(faadIdString) : QSL("Faad unknown"))) +
                     // "  (" + (faadCopyrightString != nullptr ? QString(faadCopyrightString) : QString("")) + ")"
                     QSL("<br/>");
  QString fdkVers;
#endif

#ifdef HAVE_LIQUID
  QString liquidVers = hyperlink_text(QSL("https://github.com/jgaeddert/liquid-dsp"), QSL("liquid-dsp ") + QSL(LIQUID_VERSION)) + QSL("<br/>");
  QString useLiquid = QSL(", liquid-DSP");
#else
  QString liquidVers;
  QString useLiquid;
#endif

  QString versionText = QSL("<html><head/><body>");
  versionText += QSL("<h3>") + QSL(PRJ_NAME) + QSL(" ") + QSL(PRJ_VERS) + QSL("</h3>");
  // __TIMESTAMP__ seems to use the file date not the compile date, so use __DATE__/__TIME__ instead
  versionText += QSL("<p><b>Built on ") + QSL(__DATE__) + QSL("&nbsp;&nbsp;") + QSL(__TIME__) + QSL("<br/>Commit ") + QSL(GITHASH) + QSL("</b></p>");
  versionText += QSL("<p><b>Used libraries with version:</b><br/>") +
                 hyperlink_text(QSL("https://www.qt.io"), QSL("Qt " QT_VERSION_STR)) + QSL("<br/>") +
                 volkVers +
                 rtlSdrVers +
                 hyperlink_text(QSL("https://www.fftw.org"), QString::fromUtf8(fftwf_version)) + QSL("<br/>") +
                 faadVers +
                 fdkVers +
                 hyperlink_text(QSL("https://github.com/libsndfile/libsndfile"), QString::fromUtf8(sf_version_string())) + QSL("<br/>") +
                 liquidVers +
                 hyperlink_text(QSL("https://www.zlib.net"), QSL("zlib ") + QSL(ZLIB_VERSION)) +
                 QSL("</p>");
  versionText += QSL("<p>Forked from Qt-DAB in June 2023, then extensively changed, extended and partly reduced, by Thomas Neder ") +
                 QSL("(") + hyperlink(QSL("https://github.com/tomneda/DABstar")) + QSL(").<br/>") +
                 QSL("For Qt-DAB see ") + hyperlink(QSL("https://github.com/JvanKatwijk/qt-dab")) + QSL(" by Jan van Katwijk ") +
                 QSL("(") + hyperlink(QSL("J.vanKatwijk@gmail.com"), true) + QSL(").</p>");
  versionText += QSL("<p>Rights of Qt, FFTW") + usedDecoder + useVolk + useLiquid + QSL(", libsndfile and zlib gratefully acknowledged.<br/>") +
                 QSL("Rights of developers of ") + hyperlink_text(QSL("https://github.com/old-dab/rtlsdr"), QSL("RTLSDR library")) + QSL(" (using the improved fork from old-dab), ")
                 + hyperlink_text(QSL("https://www.sdrplay.com"), QSL("SDRplay libraries")) + QSL(", ")
                 + hyperlink_text(QSL("https://github.com/airspy/airspyone_host"), QSL("AIRspy library")) + QSL(" and others gratefully acknowledged.<br/>") +
                 QSL("Rights of other contributors gratefully acknowledged.</p>");
  versionText += QSL("<p>Features NewsService Journaline(R) decoder technology by Fraunhofer IIS, Erlangen, Germany.<br/>") +
                 QSL("For more information visit ") + hyperlink(QSL("http://www.iis.fhg.de/dab")) + QSL(".</p>");
  versionText += QSL("</body></html>");
  return versionText;
}
