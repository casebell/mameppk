/***************************************************************************

    emuopts.c

    Options file and command line management.

    Copyright (c) 1996-2007, Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/

#include "mamecore.h"
#include "mame.h"
#include "fileio.h"
#include "emuopts.h"
#include "osd_so.h"

#include <ctype.h>



/***************************************************************************
    BUILT-IN (CORE) OPTIONS
***************************************************************************/

const options_entry mame_core_options[] =
{
	/* unadorned options - only a single one supported at the moment */
	{ "<UNADORNED0>",                NULL,        0,                 NULL },

#ifdef DRIVER_SWITCH
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE CONFIGURATION OPTIONS" },
	{ "driver_config",               "mame,plus,homebrew,neod", 0,                 "switch drivers"},
#endif /* DRIVER_SWITCH */

	/* config options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE CONFIGURATION OPTIONS" },
	{ "readconfig;rc",               "1",         OPTION_BOOLEAN,    "enable loading of configuration files" },

	/* seach path options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE SEARCH PATH OPTIONS" },
	{ "rompath;rp;biospath;bp",      "roms",      0,                 "path to ROMsets and hard disk images" },
#ifdef MESS
	{ "hashpath;hash_directory;hash","hash",      0,                 "path to hash files" },
#endif /* MESS */
	{ "samplepath;sp",               "samples",   0,                 "path to samplesets" },
	{ "artpath;artwork_directory",   "artwork",   0,                 "path to artwork files" },
	{ "ctrlrpath;ctrlr_directory",   "ctrlr",     0,                 "path to controller definitions" },
	{ "inipath",                     "ini",       0,                 "path to ini files" },
	{ "fontpath",                    ".;lang",    0,                 "path to font files" },
	{ "translation_directory",       "lang",      0,                 "directory for translation table data" },
	{ "localized_directory",         "lang",      0,                 "directory for localized data files" },
#ifdef USE_IPS
	{ "ips_directory",               "ips",       0,                 "directory for ips files" },
#else /* USE_IPS */
	{ "ips_directory",               "ips",       OPTION_DEPRECATED, "(disabled by compiling option)" },
#endif /* USE_IPS */

	/* output directory options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE OUTPUT DIRECTORY OPTIONS" },
	{ "cfg_directory",               "cfg",       0,                 "directory to save configurations" },
	{ "nvram_directory",             "nvram",     0,                 "directory to save nvram contents" },
	{ "memcard_directory",           "memcard",   0,                 "directory to save memory card contents" },
	{ "input_directory",             "inp",       0,                 "directory to save input device logs" },
	{ "state_directory",             "sta",       0,                 "directory to save states" },
	{ "snapshot_directory",          "snap",      0,                 "directory to save screenshots" },
	{ "diff_directory",              "diff",      0,                 "directory to save hard drive image difference files" },
	{ "comment_directory",           "comments",  0,                 "directory to save debugger comments" },
#ifdef USE_HISCORE
	{ "hiscore_directory",           "hi",        0,                 "directory to save hiscores" },
#endif /* USE_HISCORE */
#ifdef MAME_AVI
	{ "avi_directory",               "avi",       0,                 "directory to save avi video file" },
#endif /* MAME_AVI */

	/* filename options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE FILENAME OPTIONS" },
	{ "cheat_file",                  "cheat.dat", 0,                 "cheat filename" },
	{ "history_file",               "history.dat",0,                 "history database name" },
#ifdef STORY_DATAFILE
	{ "story_file",                 "story.dat",  0,                 "story database name" },
#else /* STORY_DATAFILE */
	{ "story_file",                 "story.dat",  OPTION_DEPRECATED, "(disabled by compiling option)" },
#endif /* STORY_DATAFILE */
	{ "mameinfo_file",              "mameinfo.dat",0,                "mameinfo database name" },
#ifdef CMD_LIST
	{ "command_file",               "command.dat",0,                 "command list database name" },
#else /* CMD_LIST */
	{ "command_file",               "command.dat",OPTION_DEPRECATED, "(disabled by compiling option)" },
#endif /* CMD_LIST */
#ifdef USE_HISCORE
	{ "hiscore_file",               "hiscore.dat",0,                 "high score database name" },
#else /* STORY_DATAFILE */
	{ "hiscore_file",               "hiscore.dat",OPTION_DEPRECATED, "(disabled by compiling option)" },
#endif /* USE_HISCORE */

	/* state/playback options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE STATE/PLAYBACK OPTIONS" },
	{ "state",                       NULL,        0,                 "saved state to load" },
	{ "autosave",                    "0",         OPTION_BOOLEAN,    "enable automatic restore at startup, and automatic save at exit time" },
	{ "playback;pb",                 NULL,        0,                 "playback an input file" },
	{ "record;rec",                  NULL,        0,                 "record an input file" },
#ifdef KAILLERA
	{ "playbacksub;pbsub",           NULL,        0,                 "playbacksub an input file" },
	{ "recordsub;recsub",            NULL,        0,                 "recordsub an input file" },
	{ "auto_record_name;at_rec_name",NULL,        0,                 "auto record filename" },
#endif /* KAILLERA */
	{ "mngwrite",                    NULL,        0,                 "optional filename to write a MNG movie of the current session" },
	{ "wavwrite",                    NULL,        0,                 "optional filename to write a WAV file of the current session" },

	/* performance options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE PERFORMANCE OPTIONS" },
	{ "autoframeskip;afs",           "0",         OPTION_BOOLEAN,    "enable automatic frameskip selection" },
	{ "frameskip;fs(0-10)",          "0",         0,                 "set frameskip to fixed value, 0-10 (autoframeskip must be disabled)" },
	{ "seconds_to_run;str",          "0",         0,                 "number of emulated seconds to run before automatically exiting" },
	{ "throttle",                    "1",         OPTION_BOOLEAN,    "enable throttling to keep game running in sync with real time" },
	{ "sleep",                       "1",         OPTION_BOOLEAN,    "enable sleeping, which gives time back to other applications when idle" },
	{ "speed(0.01-100)",             "1.0",       0,                 "controls the speed of gameplay, relative to realtime; smaller numbers are slower" },
	{ "refreshspeed;rs",             "0",         OPTION_BOOLEAN,    "automatically adjusts the speed of gameplay to keep the refresh rate lower than the screen" },

	/* rotation options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE ROTATION OPTIONS" },
	{ "rotate",                      "1",         OPTION_BOOLEAN,    "rotate the game screen according to the game's orientation needs it" },
	{ "ror",                         "0",         OPTION_BOOLEAN,    "rotate screen clockwise 90 degrees" },
	{ "rol",                         "0",         OPTION_BOOLEAN,    "rotate screen counterclockwise 90 degrees" },
	{ "autoror",                     "0",         OPTION_BOOLEAN,    "automatically rotate screen clockwise 90 degrees if vertical" },
	{ "autorol",                     "0",         OPTION_BOOLEAN,    "automatically rotate screen counterclockwise 90 degrees if vertical" },
	{ "flipx",                       "0",         OPTION_BOOLEAN,    "flip screen left-right" },
	{ "flipy",                       "0",         OPTION_BOOLEAN,    "flip screen upside-down" },

	/* artwork options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE ARTWORK OPTIONS" },
	{ "artwork_crop;artcrop",        "0",         OPTION_BOOLEAN,    "crop artwork to game screen size" },
	{ "use_backdrops;backdrop",      "1",         OPTION_BOOLEAN,    "enable backdrops if artwork is enabled and available" },
	{ "use_overlays;overlay",        "1",         OPTION_BOOLEAN,    "enable overlays if artwork is enabled and available" },
	{ "use_bezels;bezel",            "1",         OPTION_BOOLEAN,    "enable bezels if artwork is enabled and available" },

	/* screen options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE SCREEN OPTIONS" },
	{ "brightness(0.1-2.0)",         "1.0",       0,                 "default game screen brightness correction" },
	{ "contrast(0.1-2.0)",           "1.0",       0,                 "default game screen contrast correction" },
	{ "gamma(0.1-3.0)",              "1.0",       0,                 "default game screen gamma correction" },
	{ "pause_brightness(0.0-1.0)",   "0.65",      0,                 "amount to scale the screen brightness when paused" },
#ifdef USE_SCALE_EFFECTS
	{ "scale_effect",                "none",      0,                 "image enhancement effect" },
#endif /* USE_SCALE_EFFECTS */

	/* vector options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE VECTOR OPTIONS" },
	{ "antialias;aa",                "1",         OPTION_BOOLEAN,    "use antialiasing when drawing vectors" },
	{ "beam",                        "1.0",       0,                 "set vector beam width" },
	{ "flicker",                     "0",         0,                 "set vector flicker effect" },

	/* sound options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE SOUND OPTIONS" },
	{ "sound",                       "1",         OPTION_BOOLEAN,    "enable sound output" },
	{ "samplerate;sr(1000-1000000)", "48000",     0,                 "set sound output sample rate" },
	{ "samples",                     "1",         OPTION_BOOLEAN,    "enable the use of external samples if available" },
	{ "volume;vol",                  "0",         0,                 "sound volume in decibels (-32 min, 0 max)" },
#ifdef USE_VOLUME_AUTO_ADJUST
	{ "volume_adjust",               "0",         OPTION_BOOLEAN,    "enable/disable volume auto adjust" },
#else /* USE_VOLUME_AUTO_ADJUST */
	{ "volume_adjust",               "0",         OPTION_DEPRECATED, "(disabled by compiling option)" },
#endif /* USE_VOLUME_AUTO_ADJUST */

	/* input options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE INPUT OPTIONS" },
	{ "ctrlr",                       "Standard",  0,                 "preconfigure for specified controller" },
	{ "mouse",                       "0",         OPTION_BOOLEAN,    "enable mouse input" },
	{ "joystick;joy",                "0",         OPTION_BOOLEAN,    "enable joystick input" },
	{ "lightgun;gun",                "0",         OPTION_BOOLEAN,    "enable lightgun input" },
	{ "multikeyboard;multikey",      "0",         OPTION_BOOLEAN,    "enable separate input from each keyboard device (if present)" },
	{ "multimouse",                  "0",         OPTION_BOOLEAN,    "enable separate input from each mouse device (if present)" },
	{ "steadykey;steady",            "0",         OPTION_BOOLEAN,    "enable steadykey support" },
	{ "offscreen_reload;reload",     "0",         OPTION_BOOLEAN,    "convert lightgun button 2 into offscreen reload" },
	{ "joystick_map;joymap",         "auto",      0,                 "explicit joystick map, or auto to auto-select" },
	{ "joystick_deadzone;joy_deadzone;jdz",      "0.3",  0,          "center deadzone range for joystick where change is ignored (0.0 center, 1.0 end)" },
	{ "joystick_saturation;joy_saturation;jsat", "0.85", 0,          "end of axis saturation range for joystick where change is ignored (0.0 center, 1.0 end)" },
#ifdef MAME32PLUSPLUS
	{ "forceuse_dinput",             "0",         OPTION_BOOLEAN,    "force use direct input" },
#endif /* MAME32PLUSPLUS */

	/* input autoenable options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE INPUT AUTOMATIC ENABLE OPTIONS" },
	{ "paddle_device;paddle",        "keyboard",  0,                 "enable (keyboard|mouse|joystick) if a paddle control is present" },
	{ "adstick_device;adstick",      "keyboard",  0,                 "enable (keyboard|mouse|joystick) if an analog joystick control is present" },
	{ "pedal_device;pedal",          "keyboard",  0,                 "enable (keyboard|mouse|joystick) if a pedal control is present" },
	{ "dial_device;dial",            "keyboard",  0,                 "enable (keyboard|mouse|joystick) if a dial control is present" },
	{ "trackball_device;trackball",  "keyboard",  0,                 "enable (keyboard|mouse|joystick) if a trackball control is present" },
	{ "lightgun_device",             "keyboard",  0,                 "enable (keyboard|mouse|joystick) if a lightgun control is present" },
	{ "positional_device",           "keyboard",  0,                 "enable (keyboard|mouse|joystick) if a positional control is present" },
	{ "mouse_device",                "mouse",     0,                 "enable (keyboard|mouse|joystick) if a mouse control is present" },

	/* debugging options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE DEBUGGING OPTIONS" },
	{ "log",                         "0",         OPTION_BOOLEAN,    "generate an error.log file" },
	{ "verbose;v",                   "0",         OPTION_BOOLEAN,    "display additional diagnostic information" },
#ifdef MAME_DEBUG
	{ "debug;d",                     "1",         OPTION_BOOLEAN,    "enable/disable debugger" },
	{ "debugscript",                 NULL,        0,                 "script for debugger" },
#else
	{ "debug;d",                     "1",         OPTION_DEPRECATED, "(debugger-only command)" },
	{ "debugscript",                 NULL,        OPTION_DEPRECATED, "(debugger-only command)" },
#endif

	/* misc options */
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE MISC OPTIONS" },
	{ "bios",                        "default",   0,                 "select the system BIOS to use" },
	{ "cheat;c",                     "0",         OPTION_BOOLEAN,    "enable cheat subsystem" },
	{ "skip_gameinfo",               "0",         OPTION_BOOLEAN,    "skip displaying the information screen at startup" },
#ifdef USE_IPS
	{ "ips",                         NULL,        0,                 "ips datfile name"},
#else /* USE_IPS */
	{ "ips",                         NULL,        OPTION_DEPRECATED, "(disabled by compiling option)" },
#endif /* USE_IPS */
	{ "confirm_quit",                "1",         OPTION_BOOLEAN,    "quit game with confirmation" },
#ifdef AUTO_PAUSE_PLAYBACK
	{ "auto_pause_playback",         "0",         OPTION_BOOLEAN,    "automatic pause when playback is finished" },
#else /* AUTO_PAUSE_PLAYBACK */
	{ "auto_pause_playback",         "0",         OPTION_DEPRECATED, "(disabled by compiling option)" },
#endif /* AUTO_PAUSE_PLAYBACK */
#if (HAS_M68000 || HAS_M68008 || HAS_M68010 || HAS_M68EC020 || HAS_M68020 || HAS_M68040)
	/* ks hcmame s switch m68k core */
	{ "m68k_core",                   "c",         0,                 "change m68k core (0:C, 1:DRC, 2:ASM+DRC)" },
#else /* (HAS_M68000 || HAS_M68008 || HAS_M68010 || HAS_M68EC020 || HAS_M68020 || HAS_M68040) */
	{ "m68k_core",                   "c",         OPTION_DEPRECATED, "(disabled by compiling option)" },
#endif /* (HAS_M68000 || HAS_M68008 || HAS_M68010 || HAS_M68EC020 || HAS_M68020 || HAS_M68040) */
#ifdef TRANS_UI
	{ "use_trans_ui",                "1",         OPTION_BOOLEAN,    "use transparent background for UI text" },
	{ "ui_transparency",             "224",       0,                 "transparency of UI background [0 - 255]" },
#else /* TRANS_UI */
	{ "use_trans_ui",                "1",         OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "ui_transparency",             "224",       OPTION_DEPRECATED, "(disabled by compiling option)" },
#endif /* TRANS_UI */
#ifdef MAME32PLUSPLUS
	{ "disp_autofire_status",        "1",         OPTION_BOOLEAN,    "display autofire status" },
#endif /* MAME32PLUSPLUS */

	// palette options
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE PALETTE OPTIONS" },
#ifdef UI_COLOR_DISPLAY
	{ "font_blank",                  "0,0,0",       0,               "font blank color" },
	{ "font_normal",                 "255,255,255", 0,               "font normal color" },
	{ "font_special",                "247,203,0",   0,               "font special color" },
	{ "system_background",           "16,16,48",    0,               "window background color" },
	{ "button_red",                  "255,64,64",   0,               "button color (red)" },
	{ "button_yellow",               "255,238,0",   0,               "button color (yellow)" },
	{ "button_green",                "0,255,64",    0,               "button color (green)" },
	{ "button_blue",                 "0,170,255",   0,               "button color (blue)" },
	{ "button_purple",               "170,0,255",   0,               "button color (purple)" },
	{ "button_pink",                 "255,0,170",   0,               "button color (pink)" },
	{ "button_aqua",                 "0,255,204",   0,               "button color (aqua)" },
	{ "button_silver",               "255,0,255",   0,               "button color (silver)" },
	{ "button_navy",                 "255,160,0",   0,               "button color (navy)" },
	{ "button_lime",                 "190,190,190", 0,               "button color (lime)" },
	{ "cursor",                      "60,120,240",  0,               "cursor color" },
#else /* UI_COLOR_DISPLAY */
	{ "font_blank",                  "0,0,0",       OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "font_normal",                 "255,255,255", OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "font_special",                "247,203,0",   OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "system_background",           "16,16,48",    OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "button_red",                  "255,64,64",   OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "button_yellow",               "255,238,0",   OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "button_green",                "0,255,64",    OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "button_blue",                 "0,170,255",   OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "button_purple",               "170,0,255",   OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "button_pink",                 "255,0,170",   OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "button_aqua",                 "0,255,204",   OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "button_silver",               "255,0,255",   OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "button_navy",                 "255,160,0",   OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "button_lime",                 "190,190,190", OPTION_DEPRECATED, "(disabled by compiling option)" },
	{ "cursor",                      "60,120,240",  OPTION_DEPRECATED, "(disabled by compiling option)" },
#endif /* UI_COLOR_DISPLAY */

	// language options
	{ NULL,                          NULL,        OPTION_HEADER,     "CORE LANGUAGE OPTIONS" },
	{ "language;lang",               "auto",      0,                 "select translation language" },
	{ "use_lang_list",               "1",         OPTION_BOOLEAN,    "enable/disable local language game list" },

#ifdef MAME_AVI
	// avi options
	{ NULL,                           NULL,   OPTION_HEADER,  "AVI RECORD OPTIONS" },
	{ "avi_avi_filename",             NULL,   0,              "avi options" },
	{ "avi_def_fps",                  "60.0", 0,              "avi options" },
	{ "avi_fps",                      "60.0", 0,              "avi options" },
	{ "avi_frame_skip",               "0",    0,              "avi options" },
	{ "avi_frame_cmp",                "0",    OPTION_BOOLEAN, "avi options" },
	{ "avi_frame_cmp_pre15",          "0",    OPTION_BOOLEAN, "avi options" },
	{ "avi_frame_cmp_few",            "0",    OPTION_BOOLEAN, "avi options" },
	{ "avi_width",                    "0",    0,              "avi options" },
	{ "avi_height",                   "0",    0,              "avi options" },
	{ "avi_depth",                    "16",   0,              "avi options" },
	{ "avi_orientation",              "0",    0,              "avi options" },
	{ "avi_rect_top",                 "0",    0,              "avi options" },
	{ "avi_rect_left",                "0",    0,              "avi options" },
	{ "avi_rect_width",               "0",    0,              "avi options" },
	{ "avi_rect_height",              "0",    0,              "avi options" },
	{ "avi_interlace",                "0",    OPTION_BOOLEAN, "avi options" },
	{ "avi_interlace_odd_field",      "0",    OPTION_BOOLEAN, "avi options" },
	{ "avi_avi_filesize",             "0",    0,              "avi options" },
	{ "avi_avi_savefile_pause",       "0",    OPTION_BOOLEAN, "avi options" },
	{ "avi_avi_width",                "0",    0,              "avi options" },
	{ "avi_avi_height",               "0",    0,              "avi options" },
	{ "avi_avi_depth",                "16",   0,              "avi options" },
	{ "avi_avi_rect_top",             "0",    0,              "avi options" },
	{ "avi_avi_rect_left",            "0",    0,              "avi options" },
	{ "avi_avi_rect_width",           "0",    0,              "avi options" },
	{ "avi_avi_rect_height",          "0",    0,              "avi options" },
	{ "avi_avi_smooth_resize_x",      "0",    OPTION_BOOLEAN, "avi options" },
	{ "avi_avi_smooth_resize_y",      "0",    OPTION_BOOLEAN, "avi options" },

	{ "avi_wav_filename",             NULL,   0,              "avi options" },
	{ "avi_audio_type",               "0",    0,              "avi options" },
	{ "avi_audio_channel",            "0",    0,              "avi options" },
	{ "avi_audio_samples_per_sec",    "0",    0,              "avi options" },
	{ "avi_audio_bitrate",            "0",    0,              "avi options" },
	{ "avi_audio_record_type",        "0",    0,              "avi options" },
	{ "avi_avi_audio_channel",        "0",    0,              "avi options" },
	{ "avi_avi_audio_samples_per_sec","0",    0,              "avi options" },
	{ "avi_avi_audio_bitrate",        "0",    0,              "avi options" },
	{ "avi_audio_cmp",                "0",    OPTION_BOOLEAN, "avi options" },

	{ "avi_hour",                     "0",    0,              "avi options" },
	{ "avi_minute",                   "0",    0,              "avi options" },
	{ "avi_second",                   "0",    0,              "avi options" },
#endif /* MAME_AVI */

#ifdef USE_PSXPLUGIN
	// psx plugin options
	{ NULL,                         NULL,               OPTION_HEADER,  "PSX PLUGIN OPTIONS" },
	{ "use_gpu_plugin;usegpu",      "0",                OPTION_BOOLEAN, "use external gpu plugins in PSX based emulation." },
	{ "gpu_plugin_name;gpu_name",   "d3d_renderer.znc", 0,              "gpu plugins file name" },
	{ "gpu_screen_size;gpu_scrsize","0",                0,              "gpu plugins options" },
	{ "gpu_screen_std;gpu_scrstd",  "1",                0,              "gpu plugins options" },
	{ "gpu_screen_ctm;gpu_scrctm",  "0",                0,              "gpu plugins options" },
	{ "gpu_screen_x;gpu_scrx",      "800",              0,              "gpu plugins options" },
	{ "gpu_screen_y;gpu_scry",      "600",              0,              "gpu plugins options" },
	{ "gpu_fullscreen;gpu_fullscr", "0",                OPTION_BOOLEAN, "gpu plugins options" },
	{ "gpu_showfps;gpu_fps",        "1",                OPTION_BOOLEAN, "gpu plugins options" },
	{ "gpu_scanline;gpu_sl",        "0",                0,              "gpu plugins options" },
	{ "gpu_blending;gpu_bld",       "0",                0,              "gpu plugins options" },
	{ "gpu_32bit;gpu_32bit",        "0",                OPTION_BOOLEAN, "gpu plugins options" },
	{ "gpu_dithering;gpu_dither",   "1",                OPTION_BOOLEAN, "gpu plugins options" },
	{ "gpu_frame_skip;gpu_fkip",    "1",                OPTION_BOOLEAN, "gpu plugins options" },
	{ "gpu_detection;gpu_detect",   "0",                OPTION_BOOLEAN, "gpu plugins options" },
	{ "gpu_frame_limit;gpu_flimit", "1",                OPTION_BOOLEAN, "gpu plugins options" },
	{ "gpu_frame_rate;gpu_frate",   "60",               0,              "gpu plugins options" },
	{ "gpu_filtering;gpu_filter",   "2",                0,              "gpu plugins options" },
	{ "gpu_quality;gpu_quality",    "0",                0,              "gpu plugins options" },
	{ "gpu_caching;gpu_cache",      "2",                0,              "gpu plugins options" },

	{ "make_gpu_gamewin;mkgpuwin",  "1",                OPTION_BOOLEAN, "make another window for PSX based game emulation. (only if external gpu plugins used)" },
	// PSX_SPU
	{ "use_spu_plugin;usespu",      "0",                OPTION_BOOLEAN, "use external spu(sound) plugins in PSX based emulation." },
	{ "spu_plugin_name;spuname",    "spuPeopsSound.dll",0,              "spu plugins file name" },
#endif /* USE_PSXPLUGIN */

	{ NULL }
};



/***************************************************************************
    CORE IMPLEMENTATION
***************************************************************************/

/*-------------------------------------------------
    memory_error - report a memory error
-------------------------------------------------*/

static void memory_error(const char *message)
{
	fatalerror("%s", message);
}



/*-------------------------------------------------
    mame_puts_info
    mame_puts_warning
    mame_puts_error
-------------------------------------------------*/

static void mame_puts_info(const char *s)
{
	mame_printf_info("%s", s);
}

static void mame_puts_warning(const char *s)
{
	mame_printf_warning("%s", s);
}

static void mame_puts_error(const char *s)
{
	mame_printf_error("%s", s);
}



/*-------------------------------------------------
    mame_options_init - create core MAME options
-------------------------------------------------*/

core_options *mame_options_init(const options_entry *entries)
{
	/* create MAME core options */
	core_options *opts = options_create(memory_error);

	/* set up output callbacks */
	options_set_output_callback(opts, OPTMSG_INFO, mame_puts_info);
	options_set_output_callback(opts, OPTMSG_WARNING, mame_puts_warning);
	options_set_output_callback(opts, OPTMSG_ERROR, mame_puts_error);

	options_add_entries(opts, mame_core_options);
	if (entries != NULL)
		options_add_entries(opts, entries);

#ifdef MESS
	mess_options_init(opts);
#endif /* MESS */

	return opts;
}
