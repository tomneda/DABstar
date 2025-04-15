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
  explicit SettingsStorage(QSettings * ipSettings)
  : mpSettings(ipSettings)
    {}

  QSettings * get() const
  {
    assert(mpSettings != nullptr);
    return mpSettings;
  }
private:
  QSettings * const mpSettings;
};

template<typename T>
class Setting
{
private:
  QString mKey;
  T mDefaultValue;
  // QSettings * const mpSettings;

public:
  Setting(const QString & key, const T & defaultValue = T())
    : mKey(key)
    , mDefaultValue(defaultValue)
  {}

  Setting & operator=(const T & value)
  {
    SettingsStorage::instance().get()->setValue(mKey, value);
    return *this;
  }

  void reset()
  {
    SettingsStorage::instance().get()->setValue(mKey, mDefaultValue);
  }

  operator T() const
  {
    return SettingsStorage::instance().get()->value(mKey, mDefaultValue).template value<T>();
  }

  T get() const { return operator T(); }
  void set(const T & value) { operator=(value); }
};

class SettingsManager
{
public:
  static SettingsManager & instance(/*QSettings * ipSettings = nullptr*/)
  {
    static SettingsManager instance;
    return instance; 
  }

  template<typename T>
  Setting<T> define(const QString & key, const T & defaultValue = T())
  {
    return Setting<T>(key, defaultValue);
  }

  static inline Setting<int> windowWidth{"window/width", 800};
private:
  // QSettings * const mpSettings;
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
    cbManualBrowserStart,
    cbCloseDirect,
    cbUseDcAvoidance,
    cbUseDcRemoval,
    cbGenXmlFromEpg,
    hidden,
    latitude,
    longitude,
    cbAlwaysOnTop,
    saveDirAudioDump,
    saveDirSampleDump,
    saveDirContent,
    cbSaveTransToCsv,
    cbSaveSlides,
    serviceListSortCol,
    serviceListSortDesc,
    cbShowNonAudioInServiceList,
    cbUse_strongest_peak,
    cbTiiCollisions,
    cbUseNativeFileDialog,
    cbUseUtcTime,
    cbShowTiiList,
    tii_threshold,
    tii_subid,
    cbUrlClickable,
    cbAutoIterTiiEntries,

    // special enums for windows position and size storage
    configWidget,
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
