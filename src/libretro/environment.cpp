//
// src/libretro/environment.cpp
//
// Bare Metal Sega Genesis
// libretro environment callback.
//
// Handles the subset of RETRO_ENVIRONMENT_* queries the genesis core issues
// during init and game load.  Unknown queries return false (unsupported).
//

#include "environment.h"
#include <circle/logger.h>
#include <string.h>

static const char FromEnv[] = "libretro";

retro_pixel_format g_pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;

// Set by kernel.cpp before retro_load_game() so GET_GAME_INFO_EXT works.
const void *g_rom_data = 0;
size_t      g_rom_size = 0;

// Logging shim: bridge retro_log_printf_t to Circle's CLogger.
static void retro_log_cb(enum retro_log_level level, const char *fmt, ...)
{
    TLogSeverity severity;
    switch (level)
    {
        case RETRO_LOG_DEBUG: severity = LogDebug;   break;
        case RETRO_LOG_INFO:  severity = LogNotice;  break;
        case RETRO_LOG_WARN:  severity = LogWarning; break;
        case RETRO_LOG_ERROR: severity = LogError;   break;
        default:              severity = LogNotice;  break;
    }

    va_list args;
    va_start(args, fmt);
    CLogger::Get()->WriteV(FromEnv, severity, fmt, args);
    va_end(args);
}

bool environment_cb(unsigned cmd, void *data)
{
    switch (cmd)
    {
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        {
            struct retro_log_callback *cb =
                reinterpret_cast<struct retro_log_callback *>(data);
            cb->log = retro_log_cb;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        {
            const enum retro_pixel_format *fmt =
                reinterpret_cast<const enum retro_pixel_format *>(data);
            g_pixel_format = *fmt;
            CLogger::Get()->Write(FromEnv, LogNotice,
                "Pixel format: %d", (int)*fmt);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        {
            const char **dir = reinterpret_cast<const char **>(data);
            *dir = "/";
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        {
            const char **dir = reinterpret_cast<const char **>(data);
            *dir = "/";
            return true;
        }

        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        {
            bool *can_dupe = reinterpret_cast<bool *>(data);
            *can_dupe = true;
            return true;
        }

        // Provide in-memory ROM data so retro_load_game() doesn't try to
        // open "GAME.MD" via the (stubbed) filesystem.
        case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
        {
            if (!g_rom_data || g_rom_size == 0)
                return false;

            // Static storage — valid for the lifetime of the core.
            static struct retro_game_info_ext s_info_ext;
            memset(&s_info_ext, 0, sizeof(s_info_ext));

            s_info_ext.full_path       = "GAME.MD";
            s_info_ext.dir             = "/";
            s_info_ext.name            = "GAME";
            s_info_ext.ext             = "md";
            s_info_ext.meta            = "";
            s_info_ext.data            = g_rom_data;
            s_info_ext.size            = g_rom_size;
            s_info_ext.file_in_archive = false;
            s_info_ext.persistent_data = true;

            // The core expects: const struct retro_game_info_ext **
            const struct retro_game_info_ext **pp =
                reinterpret_cast<const struct retro_game_info_ext **>(data);
            *pp = &s_info_ext;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE:
        {
            // Disable the "Wide" core's widescreen extra columns (default 10),
            // which render 400px-wide with side-fill artifacts and cost ~25%
            // extra per-frame work. "0" -> native 320-wide rendering.
            retro_variable *var = reinterpret_cast<retro_variable *>(data);
            if (var != 0 && var->key != 0 &&
                strcmp(var->key, "genesis_plus_gx_wide_h40_extra_columns") == 0)
            {
                var->value = "0";
                return true;
            }
            return false;   // other options: core keeps its defaults
        }

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            // We answer the RETRO_DEVICE_ID_JOYPAD_MASK query in input_state_cb.
            return true;

        default:
            return false;
    }
}
