/*
* Copyright (c) 2025 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
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

// This is a configuration file and called several times from setting-helper, so do not provide an include-guard here.

// ReSharper disable once CppMissingIncludeGuard
#ifdef FILTER_DECLARATIONS
  #define CATEGORY_BEGIN(category_)                        struct category_ { static QString category;
  #define CATEGORY_BEGIN_NEW_NAME(category_, newCatName_)  struct category_ { static QString category;
  #define CATEGORY_END(category_)                                           };
  #define DEFINE_POS_SIZE(category_)                       static PosAndSize posAndSize;
  #define DEFINE_VARIANT_NODEF(category_, name_)           static Variant name_;
  #define DEFINE_VARIANT(category_, name_, default_)       static Variant name_;
  #define DEFINE_WIDGET(category_, name_)                  static Widget name_;
#endif

#ifdef FILTER_DEFINITIONS
  #define CATEGORY_BEGIN(category_)                        QString category_::category = #category_;
  #define CATEGORY_BEGIN_NEW_NAME(category_, newCatName_)  QString category_::category = newCatName_;
  #define CATEGORY_END(category_)
  #define DEFINE_POS_SIZE(category_)                       PosAndSize category_::posAndSize{category};
  #define DEFINE_VARIANT_NODEF(category_, name_)           Variant category_::name_{category, #name_};
  #define DEFINE_VARIANT(category_, name_, default_)       Variant category_::name_{category, #name_, default_};
  #define DEFINE_WIDGET(category_, name_)                  Widget category_::name_{category, #name_};
#endif



/*********************************
 * BEGIN OF INI FILE DEFINITIONS *
 *********************************/

CATEGORY_BEGIN(Main)
  DEFINE_POS_SIZE(Main)
  DEFINE_VARIANT(Main, varDeviceUiVisible, true)
  DEFINE_VARIANT(Main, varSdrDevice, "no device")
  DEFINE_VARIANT(Main, varDeviceFile, "")
  DEFINE_VARIANT(Main, varPresetChannel, "")
  DEFINE_VARIANT(Main, varPresetSId, 0)
  DEFINE_VARIANT(Main, varChannel, "")
  DEFINE_WIDGET(Main, sliderVolume)
CATEGORY_END(Main)

CATEGORY_BEGIN_NEW_NAME(Config, "Configuration")  // provide the name "Configuration" as category name in ini file instead of "Config"
  DEFINE_POS_SIZE(Config)
  DEFINE_VARIANT_NODEF(Config, varPicturesPath) // with ..._NODEF the defaults are set later in the code
  DEFINE_VARIANT_NODEF(Config, varMotPath)
  DEFINE_VARIANT_NODEF(Config, varEpgPath)
  DEFINE_VARIANT_NODEF(Config, varSkipFile)
  DEFINE_VARIANT_NODEF(Config, varTiiFile)
  DEFINE_VARIANT(Config, varBrowserAddress, "http://localhost")
  DEFINE_VARIANT(Config, varMapPort, 8080)
  DEFINE_VARIANT(Config, varLatitude, 0)
  DEFINE_VARIANT(Config, varLongitude, 0)
  DEFINE_WIDGET(Config, cbCloseDirect)
  DEFINE_WIDGET(Config, cbUseStrongestPeak)
  DEFINE_WIDGET(Config, cbUseNativeFileDialog)
  DEFINE_WIDGET(Config, cbUseUtcTime)
  DEFINE_WIDGET(Config, cbAlwaysOnTop)
  DEFINE_WIDGET(Config, cbManualBrowserStart)
  DEFINE_WIDGET(Config, cmbMotObjectSaving)
  DEFINE_WIDGET(Config, cmbEpgObjectSaving)
  DEFINE_WIDGET(Config, cbSaveTransToCsv)
  DEFINE_WIDGET(Config, cbUseDcAvoidance)
  DEFINE_WIDGET(Config, cbUseDcRemoval)
  DEFINE_WIDGET(Config, sbTiiThreshold)
  DEFINE_WIDGET(Config, cbTiiCollisions)
  DEFINE_WIDGET(Config, sbTiiSubId)
  DEFINE_WIDGET(Config, cbUrlClickable)
  DEFINE_WIDGET(Config, cbAutoIterTiiEntries)
  DEFINE_WIDGET(Config, cmbSoundOutput)
CATEGORY_END(Config)

CATEGORY_BEGIN(SpectrumViewer)
  DEFINE_POS_SIZE(SpectrumViewer)
  DEFINE_VARIANT(SpectrumViewer, varUiVisible, false)
  DEFINE_WIDGET(SpectrumViewer, cmbIqScope)
  DEFINE_WIDGET(SpectrumViewer, cmbCarrier)
  DEFINE_WIDGET(SpectrumViewer, sliderRfScopeZoom);
  DEFINE_WIDGET(SpectrumViewer, sliderIqScopeZoomExp);
  DEFINE_WIDGET(SpectrumViewer, cbMap1stQuad);
CATEGORY_END(SpectrumViewer)

CATEGORY_BEGIN(CirViewer)
  DEFINE_POS_SIZE(CirViewer)
  DEFINE_VARIANT(CirViewer, varUiVisible, false)
CATEGORY_END(CirViewer)

CATEGORY_BEGIN(TiiViewer)
  DEFINE_POS_SIZE(TiiViewer)
  DEFINE_VARIANT(TiiViewer, varUiVisible, false)
CATEGORY_END(TiiViewer)

CATEGORY_BEGIN(ServiceList)
  DEFINE_VARIANT(ServiceList, varSortCol, 0)
  DEFINE_VARIANT(ServiceList, varSortDesc, false)
CATEGORY_END(ServiceList)

CATEGORY_BEGIN(TechDataViewer)
  DEFINE_POS_SIZE(TechDataViewer)
  DEFINE_VARIANT(TechDataViewer, varUiVisible, false)
CATEGORY_END(TechDataViewer)

CATEGORY_BEGIN(SdrPlayV3) 
  DEFINE_POS_SIZE(SdrPlayV3)
  DEFINE_WIDGET(SdrPlayV3, sbPpmControl)
  DEFINE_WIDGET(SdrPlayV3, sbIfGainRed)
  DEFINE_WIDGET(SdrPlayV3, sbLnaStage)
  DEFINE_WIDGET(SdrPlayV3, cbBiasT)
  DEFINE_WIDGET(SdrPlayV3, cbAgc)
  DEFINE_WIDGET(SdrPlayV3, cbNotch)
  DEFINE_WIDGET(SdrPlayV3, cmbAntenna)
CATEGORY_END(SdrPlayV3)

CATEGORY_BEGIN(SpyServer)
  DEFINE_POS_SIZE(SpyServer)
  DEFINE_WIDGET(SpyServer, sbGain)
  DEFINE_WIDGET(SpyServer, cbAutoGain)
  DEFINE_VARIANT(SpyServer, varIpAddress, "localhost")
CATEGORY_END(SpyServer)

CATEGORY_BEGIN(FileReaderXml)
  DEFINE_POS_SIZE(FileReaderXml)
CATEGORY_END(FileReaderXml)

CATEGORY_BEGIN(FileReaderRaw)
  DEFINE_POS_SIZE(FileReaderRaw)
CATEGORY_END(FileReaderRaw)

CATEGORY_BEGIN(FileReaderWav)
  DEFINE_POS_SIZE(FileReaderWav)
CATEGORY_END(FileReaderWav)

CATEGORY_BEGIN(FibContentViewer)
  DEFINE_POS_SIZE(FibContentViewer)
CATEGORY_END(FibContentViewer)

CATEGORY_BEGIN(Journaline)
  DEFINE_POS_SIZE(Journaline)
CATEGORY_END(Journaline)


/********************************
 *  DO NOT EDIT BELOW THIS LINE *
 ********************************/
#undef CATEGORY_BEGIN
#undef CATEGORY_BEGIN_NEW_NAME
#undef CATEGORY_END
#undef DEFINE_POS_SIZE
#undef DEFINE_VARIANT_NODEF
#undef DEFINE_VARIANT
#undef DEFINE_WIDGET
