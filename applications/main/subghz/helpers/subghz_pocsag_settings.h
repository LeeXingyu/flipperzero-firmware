#pragma once

/*
 * Internal POCSAG defaults.
 * Keep the pager configuration in firmware so the feature does not depend on SD card files.
 */

#define SUBGHZ_POCSAG_CUSTOM_PRESET_NAME  "FM95"
#define SUBGHZ_POCSAG_DEFAULT_FREQUENCY_HZ 439987500UL

static const char* const subghz_pocsag_custom_preset_data =
    "02 0D 0B 06 08 32 07 04 14 00 13 02 12 04 11 83 10 67 15 24 18 18 19 16 1D 91 1C 00 1B 07 20 FB 22 10 21 56 00 00 C0 00 00 00 00 00 00 00";
