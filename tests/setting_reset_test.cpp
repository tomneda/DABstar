#include "setting_helper.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <cstdio>

namespace
{
bool check(const char* const iName, const bool iCondition)
{
  if (!iCondition)
  {
    std::fprintf(stderr, "FAILED: %s\n", iName);
  }
  return iCondition;
}
} // namespace

int main(int argc, char** argv)
{
  QCoreApplication app(argc, argv);

  const QTemporaryDir tempDir;
  if (!tempDir.isValid())
  {
    std::fprintf(stderr, "setting_reset_test FAILED: no temporary directory\n");
    return 1;
  }

  QSettings settings(tempDir.filePath("settings.ini"), QSettings::IniFormat);
  Settings::Storage::instance(&settings);

  Settings::Config::varLatitude.write("48.112");
  Settings::Config::varLongitude.write("11.530");
  Settings::Main::varSdrDevice.write("rtlsdr");
  settings.setValue("hackrfSettings/hack_lnaGain", 24); // device settings written directly to the storage
  settings.setValue("skipTable/5A", 1);
  settings.setValue(Settings::Config::sbPeakLevelDelay.get_key(), 7); // widget setting

  Settings::reset_to_defaults({ Settings::Config::varLatitude.get_key(), Settings::Config::varLongitude.get_key(),
                                Settings::Config::sbPeakLevelDelay.get_key(), Settings::Main::varPresetCh.get_key() });

  bool ok = true;
  ok &= check("latitude kept", settings.value(Settings::Config::varLatitude.get_key()).toString() == "48.112");
  ok &= check("longitude kept", settings.value(Settings::Config::varLongitude.get_key()).toString() == "11.530");
  ok &= check("peak level delay kept", settings.value(Settings::Config::sbPeakLevelDelay.get_key()).toInt() == 7);
  ok &= check("missing kept entry not created", !settings.contains(Settings::Main::varPresetCh.get_key()));
  ok &= check("sdr device removed", !settings.contains(Settings::Main::varSdrDevice.get_key()));
  ok &= check("sdr device default used", Settings::Main::varSdrDevice.read().toString() == "no device");
  ok &= check("device group removed", !settings.contains("hackrfSettings/hack_lnaGain"));
  ok &= check("skip table removed", !settings.contains("skipTable/5A"));
  ok &= check("only kept entries left", settings.allKeys().size() == 3);

  // the entries must also be persisted to the file
  const QSettings fileSettings(tempDir.filePath("settings.ini"), QSettings::IniFormat);
  ok &= check("file contains only kept entries", fileSettings.allKeys().size() == 3);
  ok &= check("file latitude kept", fileSettings.value(Settings::Config::varLatitude.get_key()).toString() == "48.112");

  // window geometry keys
  ok &= check("pos key is geometry", Settings::is_window_geometry_key("Main/posAndSize-x"));
  ok &= check("named pos key is geometry", Settings::is_window_geometry_key("EnsembleList/posAndSizeChMode-h"));
  ok &= check("device position key is geometry", Settings::is_window_geometry_key("hackrfSettings/position-y"));
  ok &= check("splitter key is geometry", Settings::is_window_geometry_key(Settings::EpgViewer::varSplitterStateH.get_key()));
  ok &= check("visibility key is no geometry", !Settings::is_window_geometry_key(Settings::SpectrumViewer::varUiVisible.get_key()));
  ok &= check("latitude key is no geometry", !Settings::is_window_geometry_key(Settings::Config::varLatitude.get_key()));
  ok &= check("device gain key is no geometry", !Settings::is_window_geometry_key("hackrfSettings/hack_lnaGain"));

  // reset all settings but keep the window geometries
  settings.clear();
  settings.setValue("Main/posAndSize-x", 10);
  settings.setValue("Main/posAndSize-w", 800);
  settings.setValue("airspySettings/position-x", 100);
  Settings::MapViewer::varSplitterState.write(QByteArray("state"));
  Settings::Main::varSdrDevice.write("rtlsdr");
  Settings::Config::varLatitude.write("48.112");

  const std::vector<QString> geometryKeys = Settings::get_window_geometry_keys();
  ok &= check("four geometry keys found", geometryKeys.size() == 4);

  Settings::reset_to_defaults(geometryKeys);
  ok &= check("geometry kept after reset", settings.value("Main/posAndSize-w").toInt() == 800);
  ok &= check("splitter kept after reset", Settings::MapViewer::varSplitterState.read().toByteArray() == "state");
  ok &= check("sdr device removed with kept geometry", !settings.contains(Settings::Main::varSdrDevice.get_key()));
  ok &= check("latitude removed with kept geometry", !settings.contains(Settings::Config::varLatitude.get_key()));

  // remove only the window geometries
  Settings::Main::varSdrDevice.write("rtlsdr");
  Settings::remove_keys(Settings::get_window_geometry_keys());
  ok &= check("no geometry keys left", Settings::get_window_geometry_keys().empty());
  ok &= check("other key kept on geometry removal", settings.value(Settings::Main::varSdrDevice.get_key()).toString() == "rtlsdr");
  ok &= check("only other key left", settings.allKeys().size() == 1);

  if (!ok)
  {
    std::fprintf(stderr, "setting_reset_test FAILED\n");
    return 1;
  }

  std::printf("setting_reset_test passed\n");
  return 0;
}
