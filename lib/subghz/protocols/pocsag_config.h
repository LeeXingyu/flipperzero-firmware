#pragma once

/*
 * Build-time switch for the POCSAG protocol port.
 * Set to 0 to compile out the protocol and registry entry.
 */
#ifndef SUBGHZ_PROTOCOL_POCSAG_ENABLE
#define SUBGHZ_PROTOCOL_POCSAG_ENABLE 1
#endif

#define SUBGHZ_PROTOCOL_POCSAG_KEY_FILE_VERSION 1
#define SUBGHZ_PROTOCOL_POCSAG_KEY_FILE_TYPE "Flipper POCSAG Pager Key File"
