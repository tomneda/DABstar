/*
 * Copyright (c) 2024 by Thomas Neder (https://github.com/tomneda)
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

#ifndef SETTING_HELPER_H
#define SETTING_HELPER_H

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QSettings>

#include "filereaders/raw-files/raw-reader.h"

#if 1

// class SettingsManager
// {
// public:
//   // Define all setting keys as enum
//   enum class Key
//   {
//     WindowWidth,
//     IsDarkMode,
//     LastChannel,
//     // ... add more keys
//   };
//
//   // Template specialization to define types and defaults for each key
//   template<Key K>
//   struct SettingTraits;
//
//   template<>
//   struct SettingTraits<Key::WindowWidth>
//   {
//     using Type = int;
//     static constexpr Type defaultValue = 800;
//     static const QString key; // = "window/width"
//   };
//
//   template<>
//   struct SettingTraits<Key::IsDarkMode>
//   {
//     using Type = bool;
//     static constexpr Type defaultValue = false;
//     static const QString key; // = "appearance/darkMode"
//   };
//
//   // Get setting value
//   template<typename Key K>
//   static K SettingTraits<K>::Type get()
//   {
//     return instance().getSetting<K>();
//   }
//
//   // Set setting value
//   template<Key K>
//   static void set(const typename SettingTraits<K>::Type & value)
//   {
//     instance().setSetting<K>(value);
//   }
//
//   // ... rest of the singleton implementation ...
//
// private:
//   template<Key K>
//   typename SettingTraits<K>::Type getSetting()
//   {
//     return mSettings.value(
//       SettingTraits<K>::key,
//       SettingTraits<K>::defaultValue
//     ).template value<typename SettingTraits<K>::Type>();
//   }
//
//   template<Key K>
//   void setSetting(const typename SettingTraits<K>::Type & value)
//   {
//     mSettings.setValue(SettingTraits<K>::key, value);
//   }
// };
//
// // Define the static key strings
// const QString SettingsManager::SettingTraits<SettingsManager::Key::WindowWidth>::key = "window/width";
// const QString SettingsManager::SettingTraits<SettingsManager::Key::IsDarkMode>::key = "appearance/darkMode";

#else
class SettingsManager
{
public:
  template<typename T>
  class Setting
  {
  private:
    QString mKey;
    T mDefaultValue;
    QSettings & mSettings;

  public:
    Setting(QSettings & settings, const QString & key, const T & defaultValue = T())
      : mKey(key)
      , mDefaultValue(defaultValue)
      , mSettings(settings)
    {}

    explicit operator T() const
    {
      return mSettings.value(mKey, mDefaultValue).template value<T>();
    }

    Setting & operator=(const T & value)
    {
      mSettings.setValue(mKey, value);
      return *this;
    }

    void reset()
    {
      mSettings.setValue(mKey, mDefaultValue);
    }

    // Add getter/setter for more explicit access if preferred
    T get() const { return operator T(); }
    void set(const T & value) { operator=(value); }
  };

  // Singleton access
  static SettingsManager & instance()
  {
    static SettingsManager instance("DABstar", "DABstar");  // Created once
    return instance;
  }

  // Delete copy and move operations
  SettingsManager(const SettingsManager &) = delete;
  SettingsManager & operator=(const SettingsManager &) = delete;
  SettingsManager(SettingsManager &&) = delete;
  SettingsManager & operator=(SettingsManager &&) = delete;

  // Helper method to define new settings (if needed)
  template<typename T>
  Setting<T> define(const QString & key, const T & defaultValue = T())
  {
    return Setting<T>(mSettings, key, defaultValue);
  }

  // Define your application settings here as static members
  static inline Setting<int> windowWidth = instance().define<int>("window/width", 800);
  static inline Setting<bool> isDarkMode = instance().define<bool>("appearance/darkMode", false);
  static inline Setting<QString> lastChannel = instance().define<QString>("radio/lastChannel", "");
  // Add more settings as needed...

private:
  SettingsManager(const QString & organization, const QString & application)
    : mSettings(organization, application)
  {}

  QSettings mSettings;
};
#endif


class SettingsStorage
{
public:
  static SettingsStorage & instance(QSettings * ipSettings = nullptr)
  {
    static SettingsStorage instance(ipSettings);
    return instance;
  }

  QSettings * get() const
  {
    assert(mpSettings != nullptr);
    return mpSettings;
  }
private:
  explicit SettingsStorage(QSettings * ipSettings)
  : mpSettings(ipSettings)
  {}

  QSettings * const mpSettings;
};

template<typename T>
class SettingEnum
{
public:
  explicit SettingEnum(const QString & key, const T & defaultValue = T())
    : mKey(key)
    , mDefaultValue(defaultValue)
  {}

  void reset()
  {
    SettingsStorage::instance().get()->setValue(mKey, mDefaultValue);
  }

  // direct getter method (direct usage of Setter instance)
  operator T() const // do not make explicit!
  {
    return SettingsStorage::instance().get()->value(mKey, mDefaultValue).template value<T>();
  }

  // direct setter method (direct usage of Setter instance)
  SettingEnum & operator=(const T & value)
  {
    SettingsStorage::instance().get()->setValue(mKey, value);
    return *this;
  }

  // provide getter method
  T get() const
  {
    return operator T();
  }

  // provide setter method
  void set(const T & value)
  {
    operator=(value);
  }

private:
  QString mKey;
  T mDefaultValue;
};

class SettingWidget : public QObject
{
  Q_OBJECT
public:
  explicit SettingWidget(const QString & key)
    : mKey(key)
  {
  }

  template<typename T>
  void register_widget_and_update_ui_from_setting(T * const ipWidget, const QVariant & iDefaultValue)
  {
    assert(mpWidget == nullptr); // only one-time registration necessary
    mpWidget = ipWidget;
    mDefaultValue = iDefaultValue;
    update_ui_state_from_setting();

    if (auto * const pD = dynamic_cast<QCheckBox *>(mpWidget); pD != nullptr)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
      connect(pD, &QCheckBox::checkStateChanged, [this](int iState){ update_ui_state_to_setting(); });
#else
      connect(pD, &QCheckBox::stateChanged, [this](int iState){ update_ui_state_to_setting(); });
#endif
      return;
    }

    if (auto * const pD = dynamic_cast<QComboBox *>(mpWidget); pD != nullptr)
    {
      connect(pD, &QComboBox::currentTextChanged, [this](const QString &){ update_ui_state_to_setting(); });
      return;
    }

    if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
    {
      connect(pD, qOverload<int>(&QSpinBox::valueChanged), [this](int iValue){ update_ui_state_to_setting(); });
      return;
    }

    qFatal("Pointer to mpWidget not handled (1)");
  }

  void update_ui_state_from_setting()
  {
    assert(mpWidget != nullptr); // only one-time registration necessary
    
    if (auto * const pD = dynamic_cast<QCheckBox *>(mpWidget); pD != nullptr)
    {
      const int32_t var = SettingsStorage::instance().get()->value(mKey, mDefaultValue).toInt();
      pD->setCheckState(static_cast<Qt::CheckState>(var));
      return;
    }

    if (auto * const pD = dynamic_cast<QComboBox *>(mpWidget); pD != nullptr)
    {
      const QString var = SettingsStorage::instance().get()->value(mKey, mDefaultValue).toString();
      if (const int32_t k = pD->findText(var);
          k >= 0)
      {
        pD->setCurrentIndex(k);
      }
      return;
    }

    if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
    {
      const int32_t var = SettingsStorage::instance().get()->value(mKey, mDefaultValue).toInt();
      pD->setValue(var);
      return;
    }

    qFatal("Pointer to mpWidget not handled (2)");
  }

  void update_ui_state_to_setting()
  {
    assert(mpWidget != nullptr); // only one-time registration necessary

    if (auto * const pD = dynamic_cast<QCheckBox *>(mpWidget); pD != nullptr)
    {
      SettingsStorage::instance().get()->setValue(mKey, pD->checkState());
      return;
    }

    if (auto * const pD = dynamic_cast<QComboBox *>(mpWidget); pD != nullptr)
    {
      SettingsStorage::instance().get()->setValue(mKey, pD->currentText());
      return;
    }

    if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
    {
      SettingsStorage::instance().get()->setValue(mKey, pD->value());
      return;
    }

    qFatal("Pointer to pWidget not handled");
  }

  QVariant get_variant() const
  {
    return SettingsStorage::instance().get()->value(mKey, mDefaultValue);
  }

private:
  QString mKey;
  QWidget * mpWidget = nullptr;
  QVariant mDefaultValue;
};

class SettingPosAndSize
{
public:
  explicit SettingPosAndSize(const QString & iCat)
  : mCat(iCat)
  {}
  
  void read_widget_geometry(QWidget * const iopWidget, const int32_t iXPosDef, const int32_t iYPosDef, const int32_t iWidthDef, const int32_t iHeightDef, const bool iIsFixedSized) const
  {
    const QVariant var = SettingsStorage::instance().get()->value(mCat + "posAndSize", QVariant());

    if(!var.canConvert<QByteArray>())
    {
      qWarning("Cannot retrieve widget geometry from settings. Using default settings.");
      iopWidget->resize(QSize(iWidthDef, iHeightDef));
      iopWidget->move(QPoint(iXPosDef, iYPosDef));
      return;
    }

    if (!iopWidget->restoreGeometry(var.toByteArray()))
    {
      qWarning("restoreGeometry() returns false");
    }

    if (iIsFixedSized)
    {
      iopWidget->setFixedSize(QSize(iWidthDef, iHeightDef));
    }
  }

  void write_widget_geometry(const QWidget * const ipWidget) const
  {
    const QByteArray var = ipWidget->saveGeometry();
    SettingsStorage::instance().get()->setValue(mCat + "posAndSize", var);
  }

private:
  QString mCat;
};

class SettingsManager
{
public:
  static SettingsManager & instance()
  {
    static SettingsManager instance;
    return instance; 
  }

  // template<typename T>
  // Setting<T> define(const QString & key, const T & defaultValue = T())
  // {
  //   return Setting<T>(key, defaultValue);
  // }

  struct Spectrum
  {
    #define catSpectrumViewer "spectrumViewer/" // did not find nicer way to declare that once
    static inline SettingPosAndSize posAndSize{catSpectrumViewer};
  };

  struct Configuration
  {
    #define catConfiguration "configuration/" // did not find nicer way to declare that once
    static inline SettingPosAndSize posAndSize{catConfiguration};

    static inline SettingEnum<int> width{catConfiguration "width", 800};

    static inline SettingWidget sbTiiThreshold{catConfiguration "sbTiiThreshold"};
    static inline SettingWidget cbCloseDirect{catConfiguration "cbCloseDirect"};
    static inline SettingWidget cbUse_strongest_peak{catConfiguration "cbUseStrongestPeak"};
    static inline SettingWidget cbUseNativeFileDialog{catConfiguration "cbUseNativeFileDialog"};
    static inline SettingWidget cbUseUtcTime{catConfiguration "cbUseUtcTime"};
    static inline SettingWidget cbGenXmlFromEpg{catConfiguration "cbGenXmlFromEpg"};
    static inline SettingWidget cbAlwaysOnTop{catConfiguration "cbAlwaysOnTop"};
    static inline SettingWidget cbManualBrowserStart{catConfiguration "cbManualBrowserStart"};
    static inline SettingWidget cbSaveSlides{catConfiguration "cbSaveSlides"};
    static inline SettingWidget cbSaveTransToCsv{catConfiguration "cbSaveTransToCsv"};
    static inline SettingWidget cbUseDcAvoidance{catConfiguration "cbUseDcAvoidance"};
    static inline SettingWidget cbUseDcRemoval{catConfiguration "cbUseDcRemoval"};
    static inline SettingWidget cbShowNonAudioInServiceList{catConfiguration "cbShowNonAudioInService"};
    static inline SettingWidget cbTiiCollisions{catConfiguration "cbTiiCollisions"};
    static inline SettingWidget cbUrlClickable{catConfiguration "cbUrlClickable"};
    static inline SettingWidget cbAutoIterTiiEntries{catConfiguration "cbAutoIterTiiEntries"};

  };

private:
  SettingsManager() {}
};


// inline static SettingsManager settingManager;

// static inline Setting<int> windowWidth = define<int>("window/width", 800);

// class SettingsGroup
// {
// public:
//   SettingsGroup(QSettings & settings, const QString & group)
//     : mSettings(settings)
//   {
//     mSettings.beginGroup(group);
//   }
//
//   ~SettingsGroup()
//   {
//     mSettings.endGroup();
//   }
//
// private:
//   QSettings & mSettings;
// };


class WidgetSettingsHelper
{
public:
  template<typename T>
  static void saveWidgetState(QSettings & settings, T * widget, const QString & key)
  {
    if constexpr (std::is_base_of_v<QCheckBox, T>)
    {
      settings.setValue(key, widget->isChecked());
    }
    else if constexpr (std::is_base_of_v<QComboBox, T>)
    {
      settings.setValue(key, widget->currentIndex());
    }
    else if constexpr (std::is_base_of_v<QSpinBox, T>)
    {
      settings.setValue(key, widget->value());
    }
    // Add more widget types as needed
  }

  template<typename T>
  static void loadWidgetState(QSettings & settings, T * widget, const QString & key)
  {
    if constexpr (std::is_base_of_v<QCheckBox, T>)
    {
      widget->setChecked(settings.value(key, false).toBool());
    }
    else if constexpr (std::is_base_of_v<QComboBox, T>)
    {
      widget->setCurrentIndex(settings.value(key, 0).toInt());
    }
    else if constexpr (std::is_base_of_v<QSpinBox, T>)
    {
      widget->setValue(settings.value(key, 0).toInt());
    }
    // Add more widget types as needed
  }
};







class QWidget;

class SettingHelper : public QObject
{
  Q_OBJECT
public:
  static SettingHelper & get_instance(QSettings * ipSettings = nullptr)
  {
    static SettingHelper instance(ipSettings);
    return instance;
  }

  SettingHelper(SettingHelper &other) = delete;
  void operator=(const SettingHelper &) = delete;

  enum EElem
  {
    // main widget
    threshold,
    diffLength,
    tiiDelay,
    tiiDepth,
    echoDepth,
    soundChannel,
    picturesPath,
    filePath,
    epgPath,
    dabBand,
    skipFile,
    tiiFile,
    device,
    deviceFile,
    deviceVisible,
    spectrumVisible,
    cirVisible,
    techDataVisible,
    showDeviceWidget,
    presetName,
    channel,
    epgWidth,
    browserAddress,
    mapPort,
    epgFlag,

    // needed in config widget
    hidden,
    latitude,
    longitude,
    saveDirAudioDump,
    saveDirSampleDump,
    saveDirContent,
    serviceListSortCol,
    serviceListSortDesc,
    cbShowTiiList,
    // tii_threshold,
    tii_subid,

    // special enums for windows position and size storage
    mainWidget
  };

  QVariant read(const EElem iElem) const;
  void write(const EElem iElem, const QVariant & iVal);

  void read_widget_geometry(const EElem iElem, QWidget * const iopWidget) const;
  void write_widget_geometry(const EElem iElem, const QWidget * const ipWidget);

  template<EElem iElem> void register_and_connect_ui_element(QCheckBox * const ipPushButton);
  template<EElem iElem> void register_and_connect_ui_element(QSpinBox * const ipSpinBox);
  void sync_ui_state(const EElem iElem, const bool iWriteSetting);
  void sync_all_ui_states(const bool iWriteSetting) noexcept;

  void sync_and_unregister_ui_elements();
  QSettings * get_settings() const { return mpSettings; } // for a direct access to the QSettings (should be removed at last)

private:
  explicit SettingHelper(QSettings * ipSettings);
  ~SettingHelper() override = default; // do not cleanup things here, this is one of the last live instances in the system

  QSettings * const mpSettings;

  struct SMapElem
  {
    QString  Group;
    QString  KeyStr;
    QVariant KeyVal;
    QWidget * pWidget = nullptr;
  };

  QMap<EElem, SMapElem> mMap;

  template<typename T> void _write_setting(SMapElem & me, const T iValue);
  void _fill_map_from_settings();
  void _fill_map_with_defaults();
  const SMapElem & _get_map_elem(SettingHelper::EElem iElem) const;
  SMapElem & _get_map_elem(SettingHelper::EElem iElem);
};

template<typename T>
void SettingHelper::_write_setting(SMapElem & me, const T iValue)
{
  if (me.KeyVal != iValue)
  {
    me.KeyVal = iValue;
    mpSettings->beginGroup(me.Group);
    mpSettings->setValue(me.KeyStr, me.KeyVal);
    mpSettings->endGroup();
  }
}

template<SettingHelper::EElem iElem>
void SettingHelper::register_and_connect_ui_element(QCheckBox * const ipPushButton)
{
  SettingHelper::SMapElem & me = _get_map_elem(iElem);
  me.pWidget = ipPushButton;

  const auto * const pAB = dynamic_cast<QCheckBox *>(me.pWidget);
  Q_ASSERT(pAB != nullptr);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(pAB, &QCheckBox::checkStateChanged, [this, &me](int iState){ _write_setting(me, iState); });
#else
  connect(pAB, &QCheckBox::stateChanged, [this, &me](int iState){ _write_setting(me, iState); });
#endif
}

template<SettingHelper::EElem iElem>
void SettingHelper::register_and_connect_ui_element(QSpinBox * const ipSpinBox)
{
  SettingHelper::SMapElem & me = _get_map_elem(iElem);
  me.pWidget = ipSpinBox;

  const auto * const pSB = dynamic_cast<QSpinBox *>(me.pWidget);
  Q_ASSERT(pSB != nullptr);
  connect(pSB, qOverload<int>(&QSpinBox::valueChanged), [this, &me](int iValue){ _write_setting(me, iValue); });
}

#endif // SETTING_HELPER_H
