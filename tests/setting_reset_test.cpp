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

  if (!ok)
  {
    std::fprintf(stderr, "setting_reset_test FAILED\n");
    return 1;
  }

  std::printf("setting_reset_test passed\n");
  return 0;
}
