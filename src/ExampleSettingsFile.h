#ifndef TRACKIRMOUSE_EXAMPLE_SETTINGS_FILE_H
#define TRACKIRMOUSE_EXAMPLE_SETTINGS_FILE_H

#include <string>

constexpr std::string_view ExampleSettingsFileString = R"(# PROFILE ID'S:
# FreeSpace2 = 13302
# Aerofly = 2025

[General]
track_on_start = true
quit_on_loss_of_track_ir = false
watchdog_enabled = true
trackir_dll_directory = "default" # Specify "default" to get value from registry or an absolute path eg. "C:\Program Files..."
active_profile = 1

[DefaultPadding]
left = 3
right = 3
top = 0
bottom = 0

[Profiles]
[Profiles.1]
profile_ID = 13302

# Desktop Dual Monitor Setup
[Profiles.1.DisplayMappings]
[Profiles.1.DisplayMappings.0]
left = -43
right = 50
top = 19
bottom = -19

[Profiles.1.DisplayMappings.1]
left = 62
right = 180
top = 19
bottom = -19

[Profiles.2]
profile_id = 2025

[Profiles.2.DisplayMappings]
[Profiles.2.DisplayMappings.0]
left = -50
right = 50
top = 32
bottom = -32

# Laptop Monitor
[Profiles.3]
profile_id = 2025

[Profiles.3.DisplayMappings]
[Profiles.3.DisplayMappings.0]
left = -25
right = 25
top = 15
bottom = -15)"
#endif /* TRACKIRMOUSE_EXAMPLE_SETTINGS_FILE_H */
