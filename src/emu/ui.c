/*********************************************************************

    ui.c

    Functions used to handle MAME's user interface.

    Copyright (c) 1996-2007, Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

*********************************************************************/

#include "driver.h"
#include "osdepend.h"
#include "video/vector.h"
#include "profiler.h"
#include "cheat.h"
#include "datafile.h"
#ifdef USE_SHOW_TIME
#include <time.h>
#endif /* USE_SHOW_TIME */

#include "render.h"
#include "rendfont.h"
#include "ui.h"
#include "uimenu.h"
#include "uigfx.h"
#include "uitext.h"

#ifdef MESS
#include "mess.h"
#include "uimess.h"
#include "inputx.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <math.h>

#ifdef MAME_AVI
extern int	bAviRun;
#endif /* MAME_AVI */

#ifdef KAILLERA
#include "KailleraChat.h"
#include "ui_temp.h"
extern int kPlay;
int	quiting; //kt
#endif /* KAILLERA */



/***************************************************************************
    CONSTANTS
***************************************************************************/

enum
{
	INPUT_TYPE_DIGITAL = 0,
	INPUT_TYPE_ANALOG = 1,
	INPUT_TYPE_ANALOG_DEC = 2,
	INPUT_TYPE_ANALOG_INC = 3
};

enum
{
	ANALOG_ITEM_KEYSPEED = 0,
	ANALOG_ITEM_CENTERSPEED,
	ANALOG_ITEM_REVERSE,
	ANALOG_ITEM_SENSITIVITY,
	ANALOG_ITEM_COUNT
};

enum
{
	LOADSAVE_NONE,
	LOADSAVE_LOAD,
	LOADSAVE_SAVE
};

//mamep: to render as fixed-width font
enum
{
	CHAR_WIDTH_HALFWIDTH = 0,
	CHAR_WIDTH_FULLWIDTH,
	CHAR_WIDTH_UNKNOWN
};



/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

typedef struct _slider_state slider_state;
struct _slider_state
{
	INT32			minval;				/* minimum value */
	INT32			defval;				/* default value */
	INT32			maxval;				/* maximum value */
	INT32			incval;				/* increment value */
	INT32			(*update)(INT32 newval, char *buffer, int arg); /* callback */
	int				arg;				/* argument */
};



/***************************************************************************
    GLOBAL VARIABLES
***************************************************************************/

#ifdef INP_CAPTION
static int next_caption_frame, caption_timer;
#endif /* INP_CAPTION */

static rgb_t uifont_colortable[MAX_COLORTABLE];
static render_texture *bgtexture;
static mame_bitmap *bgbitmap;

static rgb_t ui_bgcolor;

/* font for rendering */
static render_font *ui_font;

float ui_font_height;

static int multiline_text_box_visible_lines;
static int multiline_text_box_target_lines;

//mamep: to render as fixed-width font
static int draw_text_fixed_mode;
static int draw_text_scroll_offset;

static int message_window_scroll;

static int auto_pause;
static int scroll_reset;

/* current UI handler */
static UINT32 (*ui_handler_callback)(UINT32);
static UINT32 ui_handler_param;

/* flag to track single stepping */
static int single_step;

/* FPS counter display */
static int showfps;
static osd_ticks_t showfps_end;

/* profiler display */
static int show_profiler;

/* popup text display */
static osd_ticks_t popup_text_end;

/* messagebox buffer */
static char messagebox_text[4096];
static rgb_t messagebox_backcolor;

/* slider info */
static slider_state slider_list[100];
static int slider_count;
static int slider_current;

static int display_rescale_message;
static int allow_rescale;

#ifdef UI_COLOR_DISPLAY
static int ui_transparency;
#endif /* UI_COLOR_DISPLAY */

#ifdef USE_SHOW_TIME
static int show_time = 0;
static int Show_Time_Position;
static void display_time(void);
#endif /* USE_SHOW_TIME */

#ifdef USE_SHOW_INPUT_LOG
static void display_input_log(void);
#endif /* USE_SHOW_INPUT_LOG */

#ifdef USE_PSXPLUGIN
extern void __cdecl dprintf(const char* fmt, ...);
static int force_pause = FALSE;
static int old_force_pause = FALSE;
#endif /* USE_PSXPLUGIN */

/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

static void ui_exit(running_machine *machine);
static int rescale_notifier(running_machine *machine, int width, int height);

/* text generators */
static int sprintf_disclaimer(char *buffer);
static int sprintf_warnings(char *buffer);

/* UI handlers */
static UINT32 handler_messagebox(UINT32 state);
static UINT32 handler_messagebox_ok(UINT32 state);
static UINT32 handler_messagebox_selectkey(UINT32 state);
static UINT32 handler_ingame(UINT32 state);
static UINT32 handler_slider(UINT32 state);
static UINT32 handler_load_save(UINT32 state);
static UINT32 handler_confirm_quit(UINT32 state);

/* slider controls */
static void slider_init(void);
static void slider_display(const char *text, int minval, int maxval, int defval, int curval);
static void slider_draw_bar(float leftx, float topy, float width, float height, float percentage, float default_percentage);
static INT32 slider_volume(INT32 newval, char *buffer, int arg);
static INT32 slider_mixervol(INT32 newval, char *buffer, int arg);
static INT32 slider_adjuster(INT32 newval, char *buffer, int arg);
static INT32 slider_overclock(INT32 newval, char *buffer, int arg);
static INT32 slider_refresh(INT32 newval, char *buffer, int arg);
static INT32 slider_brightness(INT32 newval, char *buffer, int arg);
static INT32 slider_contrast(INT32 newval, char *buffer, int arg);
static INT32 slider_gamma(INT32 newval, char *buffer, int arg);
static INT32 slider_xscale(INT32 newval, char *buffer, int arg);
static INT32 slider_yscale(INT32 newval, char *buffer, int arg);
static INT32 slider_xoffset(INT32 newval, char *buffer, int arg);
static INT32 slider_yoffset(INT32 newval, char *buffer, int arg);
static INT32 slider_flicker(INT32 newval, char *buffer, int arg);
static INT32 slider_beam(INT32 newval, char *buffer, int arg);
#ifdef MAME_DEBUG
static INT32 slider_crossscale(INT32 newval, char *buffer, int arg);
static INT32 slider_crossoffset(INT32 newval, char *buffer, int arg);
#endif

static void build_bgtexture(running_machine *machine);
static void free_bgtexture(running_machine *machine);

/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

/*-------------------------------------------------
    ui_set_handler - set a callback/parameter
    pair for the current UI handler
-------------------------------------------------*/

INLINE UINT32 ui_set_handler(UINT32 (*callback)(UINT32), UINT32 param)
{
	ui_handler_callback = callback;
	ui_handler_param = param;
	return param;
}


/*-------------------------------------------------
    slider_config - configure a slider entry
-------------------------------------------------*/

INLINE void slider_config(slider_state *state, INT32 minval, INT32 defval, INT32 maxval, INT32 incval, INT32 (*update)(INT32, char *, int), int arg)
{
	state->minval = minval;
	state->defval = defval;
	state->maxval = maxval;
	state->incval = incval;
	state->update = update;
	state->arg = arg;
}


rgb_t ui_get_rgb_color(rgb_t color)
{
	if (color < MAX_COLORTABLE)
		return uifont_colortable[color];

	return color;
}


/*-------------------------------------------------
    is_breakable_char - is a given unicode
    character a possible line break?
-------------------------------------------------*/

INLINE int is_breakable_char(unicode_char ch)
{
	/* regular spaces and hyphens are breakable */
	if (ch == ' ' || ch == '-')
		return TRUE;

	/* In the following character sets, any character is breakable:
        Hiragana (3040-309F)
        Katakana (30A0-30FF)
        Bopomofo (3100-312F)
        Hangul Compatibility Jamo (3130-318F)
        Kanbun (3190-319F)
        Bopomofo Extended (31A0-31BF)
        CJK Strokes (31C0-31EF)
        Katakana Phonetic Extensions (31F0-31FF)
        Enclosed CJK Letters and Months (3200-32FF)
        CJK Compatibility (3300-33FF)
        CJK Unified Ideographs Extension A (3400-4DBF)
        Yijing Hexagram Symbols (4DC0-4DFF)
        CJK Unified Ideographs (4E00-9FFF) */
	if (ch >= 0x3040 && ch <= 0x9fff)
		return TRUE;

	/* Hangul Syllables (AC00-D7AF) are breakable */
	if (ch >= 0xac00 && ch <= 0xd7af)
		return TRUE;

	/* CJK Compatibility Ideographs (F900-FAFF) are breakable */
	if (ch >= 0xf900 && ch <= 0xfaff)
		return TRUE;

	return FALSE;
}


//mamep: check fullwidth character.
//mame core does not support surrogate pairs U+10000-U+10FFFF
INLINE int is_fullwidth_char(unicode_char uchar)
{
	switch (uchar)
	{
	// Chars in Latin-1 Supplement
	// font width depends on your font
	case 0x00a7:
	case 0x00a8:
	case 0x00b0:
	case 0x00b1:
	case 0x00b4:
	case 0x00b6:
	case 0x00d7:
	case 0x00f7:
		return CHAR_WIDTH_UNKNOWN;
	}

	// Greek and Coptic
	// font width depends on your font
	if (uchar >= 0x0370 && uchar <= 0x03ff)
		return CHAR_WIDTH_UNKNOWN;

	// Cyrillic
	// font width depends on your font
	if (uchar >= 0x0400 && uchar <= 0x04ff)
		return CHAR_WIDTH_UNKNOWN;

	if (uchar < 0x1000)
		return CHAR_WIDTH_HALFWIDTH;

	//	Halfwidth CJK Chars
	if (uchar >= 0xff61 && uchar <= 0xffdc)
		return CHAR_WIDTH_HALFWIDTH;

	//	Halfwidth Symbols Variants
	if (uchar >= 0xffe8 && uchar <= 0xffee)
		return CHAR_WIDTH_HALFWIDTH;

	return CHAR_WIDTH_FULLWIDTH;
}



/***************************************************************************
    CORE IMPLEMENTATION
***************************************************************************/

#ifdef UI_COLOR_DISPLAY
//============================================================
//	setup_palette
//============================================================

static void setup_palette(void)
{
	static struct
	{
		const char *name;
		int color;
		UINT8 defval[3];
	} palette_decode_table[] =
	{
		{ OPTION_FONT_BLANK,         FONT_COLOR_BLANK,         { 0,0,0 } },
		{ OPTION_FONT_NORMAL,        FONT_COLOR_NORMAL,        { 255,255,255 } },
		{ OPTION_FONT_SPECIAL,       FONT_COLOR_SPECIAL,       { 247,203,0 } },
		{ OPTION_SYSTEM_BACKGROUND,  SYSTEM_COLOR_BACKGROUND,  { 16,16,48 } },
		{ OPTION_BUTTON_RED,         BUTTON_COLOR_RED,         { 255,64,64 } },
		{ OPTION_BUTTON_YELLOW,      BUTTON_COLOR_YELLOW,      { 255,238,0 } },
		{ OPTION_BUTTON_GREEN,       BUTTON_COLOR_GREEN,       { 0,255,64 } },
		{ OPTION_BUTTON_BLUE,        BUTTON_COLOR_BLUE,        { 0,170,255 } },
		{ OPTION_BUTTON_PURPLE,      BUTTON_COLOR_PURPLE,      { 170,0,255 } },
		{ OPTION_BUTTON_PINK,        BUTTON_COLOR_PINK,        { 255,0,170 } },
		{ OPTION_BUTTON_AQUA,        BUTTON_COLOR_AQUA,        { 0,255,204 } },
		{ OPTION_BUTTON_SILVER,      BUTTON_COLOR_SILVER,      { 255,0,255 } },
		{ OPTION_BUTTON_NAVY,        BUTTON_COLOR_NAVY,        { 255,160,0 } },
		{ OPTION_BUTTON_LIME,        BUTTON_COLOR_LIME,        { 190,190,190 } },
		{ OPTION_CURSOR,             CURSOR_COLOR,             { 60,120,240 } },
		{ NULL }
	};

	int i;

	ui_transparency = 255;

#ifdef TRANS_UI
	if (options_get_bool(mame_options(), OPTION_USE_TRANS_UI))
	{
		ui_transparency = options_get_int(mame_options(), OPTION_UI_TRANSPARENCY);
		if (ui_transparency < 0 || ui_transparency > 255)
		{
			mame_printf_error(_("Illegal value for %s = %s\n"), OPTION_UI_TRANSPARENCY, options_get_string(mame_options(), OPTION_UI_TRANSPARENCY));
			ui_transparency = 224;
		}
	}
#endif /* TRANS_UI */

	for (i = 0; palette_decode_table[i].name; i++)
	{
		const char *value = options_get_string(mame_options(), palette_decode_table[i].name);
		int col = palette_decode_table[i].color;
		int r = palette_decode_table[i].defval[0];
		int g = palette_decode_table[i].defval[1];
		int b = palette_decode_table[i].defval[2];
		int rate;

		if (value)
		{
			int pal[3];

			if (sscanf(value, "%d,%d,%d", &pal[0], &pal[1], &pal[2]) != 3 ||
				pal[0] < 0 || pal[0] >= 256 ||
				pal[1] < 0 || pal[1] >= 256 ||
				pal[2] < 0 || pal[2] >= 256 )
			{
				mame_printf_error(_("error: invalid value for palette: %s\n"), value);
				continue;
			}

			r = pal[0];
			g = pal[1];
			b = pal[2];
		}

		rate = 0xff;
#ifdef TRANS_UI
		if (col == UI_FILLCOLOR)
			rate = ui_transparency;
		else if (col == CURSOR_COLOR)
		{
			rate = ui_transparency / 2;
			if (rate < 128)
				rate = 128; //cursor should be visible
		}
#endif /* TRANS_UI */

		uifont_colortable[col] = MAKE_ARGB(rate, r, g, b);
	}
}
#endif /* UI_COLOR_DISPLAY */


/*-------------------------------------------------
    ui_init - set up the user interface
-------------------------------------------------*/

int ui_init(running_machine *machine)
{
	/* make sure we clean up after ourselves */
	add_exit_callback(machine, ui_exit);

	/* load the localization file */
	uistring_init();

#ifdef UI_COLOR_DISPLAY
	setup_palette();
#endif /* UI_COLOR_DISPLAY */

	build_bgtexture(machine);

	/* allocate the font */
	ui_font = render_font_alloc("ui.bdf");

#ifdef INP_CAPTION
	next_caption_frame = -1;
	caption_timer = 0;
#endif /* INP_CAPTION */

	ui_bgcolor = UI_FILLCOLOR;

	/* initialize the other UI bits */
	ui_menu_init(machine);
	ui_gfx_init(machine);

#ifdef KAILLERA
	if (!kPlay)
#endif /* KAILLERA */
	datafile_init(mame_options());

	/* reset globals */
	single_step = FALSE;
	ui_set_handler(handler_messagebox, 0);

	/* request callbacks when the renderer does resizing */
	render_set_rescale_notify(machine, rescale_notifier);
	allow_rescale = 0;
	display_rescale_message = FALSE;
	return 0;
}


/*-------------------------------------------------
    ui_exit - clean up ourselves on exit
-------------------------------------------------*/

static void ui_exit(running_machine *machine)
{
#ifdef KAILLERA
	if (!kPlay)
#endif /* KAILLERA */
	datafile_exit();

	/* free the font */
	if (ui_font != NULL)
		render_font_free(ui_font);
	ui_font = NULL;
}


/*-------------------------------------------------
    rescale_notifier - notifier to trigger a
    rescale message during long operations
-------------------------------------------------*/

static int rescale_notifier(running_machine *machine, int width, int height)
{
	/* always allow "small" rescaling */
	if (width < 500 || height < 500)
		return TRUE;

	/* if we've currently disallowed rescaling, turn on a message next frame */
	if (allow_rescale == 0)
		display_rescale_message = TRUE;

	/* only allow rescaling once we're sure the message is visible */
	return (allow_rescale == 1);
}


/*-------------------------------------------------
    ui_display_startup_screens - display the
    various startup screens
-------------------------------------------------*/

int ui_display_startup_screens(int first_time, int show_disclaimer)
{
#ifdef MESS
	const int maxstate = 4;
#else
	const int maxstate = 3;
#endif
	int str = options_get_int(mame_options(), OPTION_SECONDS_TO_RUN);
	int show_gameinfo = !options_get_bool(mame_options(), OPTION_SKIP_GAMEINFO);
	int show_warnings = TRUE;
	int state;

	/* disable everything if we are using -str */
	if (!first_time || (str > 0 && str < 60*5) || Machine->gamedrv == &driver_empty)
		show_gameinfo = show_warnings = show_disclaimer = FALSE;

#ifdef KAILLERA
	if (kPlay)
		show_gameinfo = show_warnings = show_disclaimer = FALSE;
#endif /* KAILLERA */
#ifdef USE_PSXPLUGIN
	if (options_get_bool(mame_options(), "use_gpu_plugin"))
		show_gameinfo = show_warnings = show_disclaimer = FALSE;
#endif /* USE_PSXPLUGIN */

	/* initialize the on-screen display system */
	slider_init();

	auto_pause = FALSE;
	scroll_reset = TRUE;
#ifdef USE_SHOW_TIME
	show_time = 0;
	Show_Time_Position = 0;
#endif /* USE_SHOW_TIME */

	/* loop over states */
	ui_set_handler(handler_ingame, 0);
	for (state = 0; state < maxstate && !mame_is_scheduled_event_pending(Machine) && !ui_menu_is_force_game_select(); state++)
	{
		/* default to standard colors */
		messagebox_backcolor = UI_FILLCOLOR;

		/* pick the next state */
		switch (state)
		{
			case 0:
				if (show_disclaimer && sprintf_disclaimer(messagebox_text))
					ui_set_handler(handler_messagebox_ok, 0);
				break;

			case 1:
				if (show_warnings && sprintf_warnings(messagebox_text))
				{
					ui_set_handler(handler_messagebox_ok, 0);
					if (Machine->gamedrv->flags & (GAME_NOT_WORKING | GAME_UNEMULATED_PROTECTION))
						messagebox_backcolor = UI_REDCOLOR;
				}
				break;

			case 2:
				if (show_gameinfo && sprintf_game_info(messagebox_text))
				{
					char *bufptr = messagebox_text + strlen(messagebox_text);

					/* append MAME version and ask for select key */
					bufptr += sprintf(bufptr, "\n\t%s %s\n\t%s", ui_getstring(UI_mame), build_version, ui_getstring(UI_selectkey));

					ui_set_handler(handler_messagebox_selectkey, 0);
				}
				break;
#ifdef MESS
			case 3:
				break;
#endif
		}

		/* clear the input memory */
		input_code_poll_switches(TRUE);
		while (input_code_poll_switches(FALSE) != INPUT_CODE_INVALID) ;

		/* loop while we have a handler */
		while (ui_handler_callback != handler_ingame && !mame_is_scheduled_event_pending(Machine) && !ui_menu_is_force_game_select())
			video_frame_update();

		/* clear the handler and force an update */
		ui_set_handler(handler_ingame, 0);
		video_frame_update();

		scroll_reset = TRUE;
	}

	/* if we're the empty driver, force the menus on */
	if (ui_menu_is_force_game_select())
		ui_set_handler(ui_menu_ui_handler, 0);

	/* clear the input memory */
	while (input_code_poll_switches(FALSE) != INPUT_CODE_INVALID)
		;

	return 0;
}


/*-------------------------------------------------
    ui_set_startup_text - set the text to display
    at startup
-------------------------------------------------*/

void ui_set_startup_text(const char *text, int force)
{
	static osd_ticks_t lastupdatetime = 0;
	osd_ticks_t curtime = osd_ticks();

	/* copy in the new text */
	strncpy(messagebox_text, text, sizeof(messagebox_text));
	messagebox_backcolor = UI_FILLCOLOR;

	/* don't update more than 4 times/second */
	if (force || (curtime - lastupdatetime) > osd_ticks_per_second() / 4)
	{
		lastupdatetime = curtime;
		video_frame_update();
	}
}


/*-------------------------------------------------
    ui_update_and_render - update the UI and
    render it; called by video.c
-------------------------------------------------*/

void ui_update_and_render(void)
{
#ifdef MAME_AVI
	extern void avi_info_view(void);
#endif /* MAME_AVI */

	/* always start clean */
	render_container_empty(render_container_get_ui());

	if (auto_pause)
	{
		auto_pause = 0;
		mame_pause(Machine, TRUE);
	}

	/* if we're paused, dim the whole screen */
	if (mame_get_phase(Machine) >= MAME_PHASE_RESET && (single_step || mame_is_paused(Machine)))
	{
		int alpha = (1.0f - options_get_float(mame_options(), OPTION_PAUSE_BRIGHTNESS)) * 255.0f;
		if (ui_menu_is_force_game_select())
			alpha = 255;
		if (alpha > 255)
			alpha = 255;
		if (alpha >= 0)
			render_ui_add_rect(0.0f, 0.0f, 1.0f, 1.0f, MAKE_ARGB(alpha,0x00,0x00,0x00), PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
	}

	/* call the current UI handler */
	assert(ui_handler_callback != NULL);
	ui_handler_param = (*ui_handler_callback)(ui_handler_param);

	/* cancel takes us back to the ingame handler */
	if (ui_handler_param == UI_HANDLER_CANCEL)
		ui_set_handler(handler_ingame, 0);

	/* add a message if we are rescaling */
	if (display_rescale_message)
	{
		display_rescale_message = FALSE;
		if (allow_rescale == 0)
			allow_rescale = 2;
		ui_draw_text_box(_("Updating Artwork..."), JUSTIFY_CENTER, 0.5f, 0.5f, messagebox_backcolor);
	}

	/* decrement the frame counter if it is non-zero */
	else if (allow_rescale != 0)
		allow_rescale--;

#ifdef MAME_AVI
    if (bAviRun) avi_info_view();
#endif /* MAME_AVI */

#ifdef MESS
	/* let MESS display its stuff */
	mess_ui_update();
#endif
}


/*-------------------------------------------------
    ui_get_font - return the UI font
-------------------------------------------------*/

render_font *ui_get_font(void)
{
	return ui_font;
}


/*-------------------------------------------------
    ui_get_line_height - return the current height
    of a line
-------------------------------------------------*/

float ui_get_line_height(void)
{
	INT32 raw_font_pixel_height = render_font_get_pixel_height(ui_font);
	INT32 target_pixel_width, target_pixel_height;
	float one_to_one_line_height;
	float target_aspect;
	float scale_factor;

	/* get info about the UI target */
	render_target_get_bounds(render_get_ui_target(), &target_pixel_width, &target_pixel_height, &target_aspect);

	/* mamep: to avoid division by zero */
	if (target_pixel_height == 0)
		return 0.0f;

	/* compute the font pixel height at the nominal size */
	one_to_one_line_height = (float)raw_font_pixel_height / (float)target_pixel_height;

	/* determine the scale factor */
	scale_factor = UI_TARGET_FONT_HEIGHT / one_to_one_line_height;

	/* if our font is small-ish, do integral scaling */
	if (raw_font_pixel_height < 24)
	{
		/* do we want to scale smaller? only do so if we exceed the threshhold */
		if (scale_factor <= 1.0f)
		{
			if (one_to_one_line_height < UI_MAX_FONT_HEIGHT || raw_font_pixel_height < 12)
				scale_factor = 1.0f;
		}

		/* otherwise, just ensure an integral scale factor */
		else
			scale_factor = floor(scale_factor);
	}

	/* otherwise, just make sure we hit an even number of pixels */
	else
	{
		INT32 height = scale_factor * one_to_one_line_height * (float)target_pixel_height;
		scale_factor = (float)height / (one_to_one_line_height * (float)target_pixel_height);
	}

	return scale_factor * one_to_one_line_height;
}


/*-------------------------------------------------
    ui_get_char_width - return the width of a
    single character
-------------------------------------------------*/

float ui_get_char_width(unicode_char ch)
{
	return render_font_get_char_width(ui_font, ui_get_line_height(), render_get_ui_aspect(), ch);
}

//mamep: to render as fixed-width font
float ui_get_char_width_no_margin(unicode_char ch)
{
	return render_font_get_char_width_no_margin(ui_font, ui_get_line_height(), render_get_ui_aspect(), ch);
}

float ui_get_char_fixed_width(unicode_char uchar, double halfwidth, double fullwidth)
{
	float chwidth;

	switch (is_fullwidth_char(uchar))
	{
	case CHAR_WIDTH_HALFWIDTH:
		return halfwidth;

	case CHAR_WIDTH_UNKNOWN:
		chwidth = ui_get_char_width_no_margin(uchar);
		if (chwidth <= halfwidth)
			return halfwidth;
	}

	return fullwidth;
}



/*-------------------------------------------------
    ui_get_string_width - return the width of a
    character string
-------------------------------------------------*/

float ui_get_string_width(const char *s)
{
	return render_font_get_utf8string_width(ui_font, ui_get_line_height(), render_get_ui_aspect(), s);
}


/*-------------------------------------------------
    ui_draw_box - add primitives to draw
    a box with the given background color
-------------------------------------------------*/

void ui_draw_box(float x0, float y0, float x1, float y1, rgb_t backcolor)
{
#ifdef UI_COLOR_DISPLAY
	if (backcolor == UI_FILLCOLOR)
		render_ui_add_quad(x0, y0, x1, y1, MAKE_ARGB(0xff, 0xff, 0xff, 0xff), bgtexture, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
	else
#endif /* UI_COLOR_DISPLAY */
		render_ui_add_rect(x0, y0, x1, y1, backcolor, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
}


/*-------------------------------------------------
    ui_draw_outlined_box - add primitives to draw
    an outlined box with the given background
    color
-------------------------------------------------*/

void ui_draw_outlined_box(float x0, float y0, float x1, float y1, rgb_t backcolor)
{
	float hw = UI_LINE_WIDTH * 0.5f;

	ui_draw_box(x0, y0, x1, y1, backcolor);
	render_ui_add_line(x0 + hw, y0 + hw, x1 - hw, y0 + hw, UI_LINE_WIDTH, ARGB_WHITE, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
	render_ui_add_line(x1 - hw, y0 + hw, x1 - hw, y1 - hw, UI_LINE_WIDTH, ARGB_WHITE, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
	render_ui_add_line(x1 - hw, y1 - hw, x0 + hw, y1 - hw, UI_LINE_WIDTH, ARGB_WHITE, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
	render_ui_add_line(x0 + hw, y1 - hw, x0 + hw, y0 + hw, UI_LINE_WIDTH, ARGB_WHITE, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
}


/*-------------------------------------------------
    ui_draw_text - simple text renderer
-------------------------------------------------*/

void ui_draw_text(const char *buf, float x, float y)
{
	ui_draw_text_full(buf, x, y, 1.0f - x, JUSTIFY_LEFT, WRAP_WORD, DRAW_OPAQUE, ARGB_WHITE, ARGB_BLACK, NULL, NULL);
}

void ui_draw_text_bk(const char *buf, float x, float y, int col)
{
	ui_draw_text_full(buf, x, y, 1.0f - x, JUSTIFY_LEFT, WRAP_WORD, DRAW_OPAQUE, ARGB_WHITE, col, NULL, NULL);
}

#if defined(MAME_AVI) || defined(KAILLERA)
void ui_draw_text2(const char *buf, float x, float y, int color)
{
	ui_draw_text_full(buf, x, y, 1.0f - x, JUSTIFY_LEFT, WRAP_WORD, DRAW_OPAQUE, ARGB_BLACK, color, NULL, NULL);
}
#endif

#ifdef KAILLERA
void ui_draw_colortext(const char *buf, float x, float y, int col)
{
	ui_draw_text_full(buf, x, y, 1.0f - x, JUSTIFY_LEFT, WRAP_WORD, DRAW_OPAQUE, col, ui_bgcolor, NULL, NULL);
}

void ui_draw_chattext(const char *buf, float x, float y, int mode, float *totalheight)
{
	const int posx[12] = { 0,-2, 2, 0, 0,-1, 1, 0,-1,-1, 1, 1};
	const int posy[12] = {-2, 0, 0, 2,-1, 0, 0, 1,-1, 1,-1, 1};

	#define ARGB_CHATEDGE ARGB_BLACK

	switch (mode) {
	case 1:
		ui_draw_text_full(buf, x, y, 1.0f - x, JUSTIFY_LEFT, WRAP_WORD, DRAW_OPAQUE, ARGB_WHITE, ui_bgcolor, NULL, totalheight);
		break;
	case 2:
		ui_draw_text_full(buf, x, y, 1.0f - x, JUSTIFY_LEFT, WRAP_WORD, DRAW_NORMAL, ARGB_WHITE, ui_bgcolor, NULL, totalheight);
		break;
	case 3:
		{
			int i=4,j=8;
			int x1, y1;

			for (; i<j; i++)
			{
				x1 = x + posx[i];
				y1 = y + posy[i];
				ui_draw_text_full(buf, x1, y1, 1.0f - x1, JUSTIFY_LEFT, WRAP_WORD, DRAW_NORMAL, ARGB_CHATEDGE, ui_bgcolor, NULL, totalheight);
			}
		}
		ui_draw_text_full(buf, x, y, 1.0f - x, JUSTIFY_LEFT, WRAP_WORD, DRAW_NORMAL, ARGB_WHITE, ui_bgcolor, NULL, totalheight);
		break;
	case 4:
		ui_draw_text_full(buf, x, y, 1.0f - x, JUSTIFY_LEFT, WRAP_TRUNCATE, DRAW_OPAQUE, ARGB_WHITE, ui_bgcolor, NULL, totalheight);
		break;
	case 5:
		ui_draw_text_full(buf, x, y, 1.0f - x, JUSTIFY_LEFT, WRAP_TRUNCATE, DRAW_NORMAL, ARGB_WHITE, ui_bgcolor, NULL, totalheight);
		break;
	default:
		{
			int i=4,j=8;
			float x1, y1;

			for (; i<j; i++)
			{
				x1 = x + (float)posx[i] * UI_LINE_WIDTH;
				y1 = y + (float)posy[i] * UI_LINE_WIDTH;
				ui_draw_text_full(buf, x1, y1, 1.0f - x1, JUSTIFY_LEFT, WRAP_TRUNCATE, DRAW_NORMAL, ARGB_CHATEDGE, ui_bgcolor, NULL, totalheight);
			}
		}
		ui_draw_text_full(buf, x, y, 1.0f - x, JUSTIFY_LEFT, WRAP_TRUNCATE, DRAW_NORMAL, ARGB_WHITE, ui_bgcolor, NULL, totalheight);
		break;
	}
}
#endif /* KAILLERA */


/*-------------------------------------------------
    ui_draw_text_full - full featured text
    renderer with word wrapping, justification,
    and full size computation
-------------------------------------------------*/

void ui_draw_text_full(const char *origs, float x, float y, float wrapwidth, int justify, int wrap, int draw, rgb_t fgcolor, rgb_t bgcolor, float *totalwidth, float *totalheight)
{
	float lineheight = ui_get_line_height();
	const char *ends = origs + strlen(origs);
	const char *s = origs;
	const char *linestart;
	float cury = y;
	float maxwidth = 0;
	const char *s_temp;
	const char *up_arrow = NULL;
	const char *down_arrow = ui_getstring(UI_downarrow);

	//mamep: control scrolling text
	int curline = 0;

	//mamep: render as fixed-width font
	float fontwidth_halfwidth = 0.0f;
	float fontwidth_fullwidth = 0.0f;

	if (draw_text_fixed_mode)
	{
		int scharcount;
		int len = strlen(origs);
		int n;

		for (n = 0; len > 0; n += scharcount, len -= scharcount)
		{
			unicode_char schar;
			float scharwidth;

			scharcount = uchar_from_utf8(&schar, &origs[n], len);
			if (scharcount == -1)
				break;

			scharwidth = ui_get_char_width_no_margin(schar);
			if (is_fullwidth_char(schar))
			{
				if (fontwidth_fullwidth < scharwidth)
					fontwidth_fullwidth = scharwidth;
			}
			else
			{
				if (fontwidth_halfwidth < scharwidth)
					fontwidth_halfwidth = scharwidth;
			}
		}

		if (fontwidth_fullwidth < fontwidth_halfwidth * 2.0f)
			fontwidth_fullwidth = fontwidth_halfwidth * 2.0f;
		if (fontwidth_halfwidth < fontwidth_fullwidth / 2.0f)
			fontwidth_halfwidth = fontwidth_fullwidth / 2.0f;
	}

	//mamep: check if we are scrolling
	if (draw_text_scroll_offset)
		up_arrow = ui_getstring (UI_uparrow);
	if (draw_text_scroll_offset == multiline_text_box_target_lines - multiline_text_box_visible_lines)
		down_arrow = NULL;

	/* if we don't want wrapping, guarantee a huge wrapwidth */
	if (wrap == WRAP_NEVER)
		wrapwidth = 1000000.0f;
	if (wrapwidth <= 0)
		return;

	/* loop over lines */
	while (*s != 0)
	{
		const char *lastbreak = NULL;
		int line_justify = justify;
		unicode_char schar;
		int scharcount;
		float lastbreak_width = 0;
		float curwidth = 0;
		float curx = x;

		/* get the current character */
		scharcount = uchar_from_utf8(&schar, s, ends - s);
		if (scharcount == -1)
			break;

		/* if the line starts with a tab character, center it regardless */
		if (schar == '\t')
		{
			s += scharcount;
			line_justify = JUSTIFY_CENTER;
		}

		/* remember the starting position of the line */
		linestart = s;

		/* loop while we have characters and are less than the wrapwidth */
		while (*s != 0 && curwidth <= wrapwidth)
		{
			float chwidth;

			/* get the current chcaracter */
			scharcount = uchar_from_utf8(&schar, s, ends - s);
			if (scharcount == -1)
				break;

			/* if we hit a newline, stop immediately */
			if (schar == '\n')
				break;

			//mamep: render as fixed-width font
			if (draw_text_fixed_mode)
				chwidth = ui_get_char_fixed_width(schar, fontwidth_halfwidth, fontwidth_fullwidth);
			else
				/* get the width of this character */
				chwidth = ui_get_char_width(schar);

			/* if we hit a space, remember the location and width *without* the space */
			if (schar == ' ')
			{
				lastbreak = s;
				lastbreak_width = curwidth;
			}

			/* add the width of this character and advance */
			curwidth += chwidth;
			s += scharcount;

			/* if we hit any non-space breakable character, remember the location and width
               *with* the breakable character */
			if (schar != ' ' && is_breakable_char(schar) && curwidth <= wrapwidth)
			{
				lastbreak = s;
				lastbreak_width = curwidth;
			}
		}

		/* if we accumulated too much for the current width, we need to back off */
		if (curwidth > wrapwidth)
		{
			/* if we're word wrapping, back up to the last break if we can */
			if (wrap == WRAP_WORD)
			{
				/* if we hit a break, back up to there with the appropriate width */
				if (lastbreak != NULL)
				{
					s = lastbreak;
					curwidth = lastbreak_width;
				}

				/* if we didn't hit a break, back up one character */
				else if (s > linestart)
				{
					/* get the previous character */
					s = (const char *)utf8_previous_char(s);
					scharcount = uchar_from_utf8(&schar, s, ends - s);
					if (scharcount == -1)
						break;

					//mamep: render as fixed-width font
					if (draw_text_fixed_mode)
						curwidth -= ui_get_char_fixed_width(schar, fontwidth_halfwidth, fontwidth_fullwidth);
					else
						curwidth -= ui_get_char_width(schar);
				}
			}

			/* if we're truncating, make sure we have enough space for the ... */
			else if (wrap == WRAP_TRUNCATE)
			{
				/* add in the width of the ... */
				curwidth += 3.0f * ui_get_char_width('.');

				/* while we are above the wrap width, back up one character */
				while (curwidth > wrapwidth && s > linestart)
				{
					/* get the previous character */
					s = (const char *)utf8_previous_char(s);
					scharcount = uchar_from_utf8(&schar, s, ends - s);
					if (scharcount == -1)
					break;

					curwidth -= ui_get_char_width(schar);
				}
			}
		}

		//mamep: add scrolling arrow
		if (draw != DRAW_NONE
		 && ((curline == 0 && up_arrow)
		 ||  (curline == multiline_text_box_visible_lines - 1 && down_arrow)))
		{
			if (curline == 0)
				linestart = up_arrow;
			else
				linestart = down_arrow;

			curwidth = ui_get_string_width(linestart);
			ends = linestart + strlen(linestart);
			s_temp = ends;
			line_justify = JUSTIFY_CENTER;
		}
		else
			s_temp = s;

		/* align according to the justfication */
		if (line_justify == JUSTIFY_CENTER)
			curx += (wrapwidth - curwidth) * 0.5f;
		else if (line_justify == JUSTIFY_RIGHT)
			curx += wrapwidth - curwidth;

		/* track the maximum width of any given line */
		if (curwidth > maxwidth)
			maxwidth = curwidth;

		/* if opaque, add a black box */
		if (draw == DRAW_OPAQUE)
			ui_draw_box(curx, cury, curx + curwidth, cury + lineheight, bgcolor);

		/* loop from the line start and add the characters */

		while (linestart < s_temp)
		{
			/* get the current character */
			unicode_char linechar;
			int linecharcount = uchar_from_utf8(&linechar, linestart, ends - linestart);
			if (linecharcount == -1)
				break;

			//mamep: consume the offset lines
			if (draw_text_scroll_offset == 0 && draw != DRAW_NONE)
			{
				//mamep: render as fixed-width font
				if (draw_text_fixed_mode)
				{
					float width = ui_get_char_fixed_width(linechar, fontwidth_halfwidth, fontwidth_fullwidth);
					float xmargin = (width - ui_get_char_width(linechar)) / 2.0f;

					render_ui_add_char(curx + xmargin, cury, lineheight, render_get_ui_aspect(), fgcolor, ui_font, linechar);
					curx += width;
				}
				else
				{
					render_ui_add_char(curx, cury, lineheight, render_get_ui_aspect(), fgcolor, ui_font, linechar);
					curx += ui_get_char_width(linechar);
				}
			}
			linestart += linecharcount;
		}

		/* append ellipses if needed */
		if (wrap == WRAP_TRUNCATE && *s != 0 && draw != DRAW_NONE)
		{
			render_ui_add_char(curx, cury, lineheight, render_get_ui_aspect(), fgcolor, ui_font, '.');
			curx += ui_get_char_width('.');
			render_ui_add_char(curx, cury, lineheight, render_get_ui_aspect(), fgcolor, ui_font, '.');
			curx += ui_get_char_width('.');
			render_ui_add_char(curx, cury, lineheight, render_get_ui_aspect(), fgcolor, ui_font, '.');
			curx += ui_get_char_width('.');
		}

		/* if we're not word-wrapping, we're done */
		if (wrap != WRAP_WORD)
			break;

		//mamep: text scrolling
		if (draw_text_scroll_offset > 0)
			draw_text_scroll_offset--;
		else
		/* advance by a row */
		{
			cury += lineheight;

			//mamep: skip overflow text
			if (draw != DRAW_NONE && curline == multiline_text_box_visible_lines - 1)
				break;

			//mamep: controll scrolling text
			if (draw_text_scroll_offset == 0)
				curline++;
		}

		/* skip past any spaces at the beginning of the next line */
		scharcount = uchar_from_utf8(&schar, s, ends - s);
		if (scharcount == -1)
			break;

		if (schar == '\n')
			s += scharcount;
		else
			while (*s && (schar < 0x80) && isspace(schar))
			{
				s += scharcount;
				scharcount = uchar_from_utf8(&schar, s, ends - s);
				if (scharcount == -1)
					break;
			}
	}

	/* report the width and height of the resulting space */
	if (totalwidth)
		*totalwidth = maxwidth;
	if (totalheight)
		*totalheight = cury - y;
}


int ui_draw_text_set_fixed_width_mode(int mode)
{
	int mode_save = draw_text_fixed_mode;

	draw_text_fixed_mode = mode;

	return mode_save;
}

void ui_draw_text_full_fixed_width(const char *origs, float x, float y, float wrapwidth, int justify, int wrap, int draw, rgb_t fgcolor, rgb_t bgcolor, float *totalwidth, float *totalheight)
{
	int mode_save = ui_draw_text_set_fixed_width_mode(TRUE);

	ui_draw_text_full(origs, x, y, wrapwidth, justify, wrap, draw, fgcolor, bgcolor, totalwidth, totalheight);
	ui_draw_text_set_fixed_width_mode(mode_save);
}

void ui_draw_text_full_scroll(const char *origs, float x, float y, float wrapwidth, int offset, int justify, int wrap, int draw, rgb_t fgcolor, rgb_t bgcolor, float *totalwidth, float *totalheight)
{
	int offset_save = draw_text_scroll_offset;

	draw_text_scroll_offset = offset;
	ui_draw_text_full(origs, x, y, wrapwidth, justify, wrap, draw, fgcolor, bgcolor, totalwidth, totalheight);

	draw_text_scroll_offset = offset_save;
}


/*-------------------------------------------------
    ui_draw_text_box - draw a multiline text
    message with a box around it
-------------------------------------------------*/

void ui_draw_text_box_scroll(const char *text, int offset, int justify, float xpos, float ypos, rgb_t backcolor)
{
	float target_width, target_height;
	float target_x, target_y;

	/* compute the multi-line target width/height */
	ui_draw_text_full(text, 0, 0, 1.0f - 2.0f * UI_BOX_LR_BORDER,
				justify, WRAP_WORD, DRAW_NONE, ARGB_WHITE, ARGB_BLACK, &target_width, &target_height);

	multiline_text_box_target_lines = (int)(target_height / ui_get_line_height() + 0.5f);
	if (target_height > 1.0f - 2.0f * UI_BOX_TB_BORDER)
		target_height = floor((1.0f - 2.0f * UI_BOX_TB_BORDER) / ui_get_line_height()) * ui_get_line_height();
	multiline_text_box_visible_lines = (int)(target_height / ui_get_line_height() + 0.5f);

	/* determine the target location */
	target_x = xpos - 0.5f * target_width;
	target_y = ypos - 0.5f * target_height;

	/* make sure we stay on-screen */
	if (target_x < UI_BOX_LR_BORDER)
		target_x = UI_BOX_LR_BORDER;
	if (target_x + target_width + UI_BOX_LR_BORDER > 1.0f)
		target_x = 1.0f - UI_BOX_LR_BORDER - target_width;
	if (target_y < UI_BOX_TB_BORDER)
		target_y = UI_BOX_TB_BORDER;
	if (target_y + target_height + UI_BOX_TB_BORDER > 1.0f)
		target_y = 1.0f - UI_BOX_TB_BORDER - target_height;

	/* add a box around that */
	ui_draw_outlined_box(target_x - UI_BOX_LR_BORDER,
					 target_y - UI_BOX_TB_BORDER,
					 target_x + target_width + UI_BOX_LR_BORDER,
					 target_y + target_height + UI_BOX_TB_BORDER, backcolor);
	ui_draw_text_full_scroll(text, target_x, target_y, target_width, offset,
				justify, WRAP_WORD, DRAW_NORMAL, ARGB_WHITE, ARGB_BLACK, NULL, NULL);
}


void ui_draw_text_box(const char *text, int justify, float xpos, float ypos, rgb_t backcolor)
{
	ui_draw_text_box_scroll(text, message_window_scroll, justify, xpos, ypos, backcolor);
}


void ui_draw_text_box_fixed_width(const char *text, int justify, float xpos, float ypos, rgb_t backcolor)
{
	int mode_save = draw_text_fixed_mode;

	draw_text_fixed_mode = 1;
	ui_draw_text_box_scroll(text, message_window_scroll, justify, xpos, ypos, backcolor);

	draw_text_fixed_mode = mode_save;
}

void ui_draw_text_box_reset_scroll(void)
{
	scroll_reset = TRUE;
}


int ui_window_scroll_keys(void)
{
	static int counter = 0;
	static int fast = 6;
	int pan_lines;
	int max_scroll;
	int do_scroll = FALSE;

	max_scroll = multiline_text_box_target_lines - multiline_text_box_visible_lines;
	pan_lines = multiline_text_box_visible_lines - 2;

	if (scroll_reset)
	{
		message_window_scroll = 0;
		scroll_reset = 0;
	}

	/* up backs up by one item */
	if (input_ui_pressed_repeat(IPT_UI_UP, fast))
	{
		message_window_scroll--;
		do_scroll = TRUE;
	}

	/* down advances by one item */
	if (input_ui_pressed_repeat(IPT_UI_DOWN, fast))
	{
		message_window_scroll++;
		do_scroll = TRUE;
	}

	/* pan-up goes to previous page */
	if (input_ui_pressed_repeat(IPT_UI_PAGE_UP,8))
	{
		message_window_scroll -= pan_lines;
		do_scroll = TRUE;
	}

	/* pan-down goes to next page */
	if (input_ui_pressed_repeat(IPT_UI_PAGE_DOWN,8))
	{
		message_window_scroll += pan_lines;
		do_scroll = TRUE;
	}

	/* home goes to the start */
	if (input_ui_pressed(IPT_UI_HOME))
	{
		message_window_scroll = 0;
		do_scroll = TRUE;
	}

	/* end goes to the last */
	if (input_ui_pressed(IPT_UI_END))
	{
		message_window_scroll = max_scroll;
		do_scroll = TRUE;
	}

	if (message_window_scroll < 0)
		message_window_scroll = 0;
	if (message_window_scroll > max_scroll)
		message_window_scroll = max_scroll;

	if (input_port_type_pressed(IPT_UI_UP,0) || input_port_type_pressed(IPT_UI_DOWN,0))
	{
		if (++counter == 25)
		{
			fast--;
			if (fast < 1)
				fast = 0;

			counter = 0;
		}
	}
	else
	{
		fast = 6;
		counter = 0;
	}

	if (do_scroll)
		return -1;

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		message_window_scroll = 0;
		return 1;
	}
	if (input_ui_pressed(IPT_UI_CANCEL))
	{
		message_window_scroll = 0;
		return 2;
	}

	return 0;
}

#ifdef KAILLERA
void displaychatlog(char *text)
{
	static char buf[65536];
	static int logsize = 0;
	//int selected = 0;
	int res;

	if (text)
	{
		strcpy(buf, text);
		logsize = strlen(text);
	}
	else
	{

		/* draw the text */
		ui_draw_message_window(buf);

		res = ui_window_scroll_keys();
		//if (res > 0)
			//return ui_menu_stack_pop();0

		if(input_ui_pressed(IPT_UI_KAILLERA_TEST1_9))
		{
			extern void KailleraChatLogClear(void);
			KailleraChatLogClear();
		}
	}

	//return selected;
}
#endif /* KAILLERA */

/*-------------------------------------------------
    ui_popup - popup a message for a standard
    amount of time
-------------------------------------------------*/

void CLIB_DECL ui_popup(const char *text, ...)
{
	int seconds;
	va_list arg;

	/* extract the text */
	va_start(arg,text);
	vsprintf(messagebox_text, text, arg);
	messagebox_backcolor = UI_FILLCOLOR;
	va_end(arg);

	/* set a timer */
	seconds = (int)strlen(messagebox_text) / 40 + 2;
	popup_text_end = osd_ticks() + osd_ticks_per_second() * seconds;
}


/*-------------------------------------------------
    ui_popup_time - popup a message for a specific
    amount of time
-------------------------------------------------*/

void CLIB_DECL ui_popup_time(int seconds, const char *text, ...)
{
	va_list arg;

	/* extract the text */
	va_start(arg,text);
	vsprintf(messagebox_text, text, arg);
	messagebox_backcolor = UI_FILLCOLOR;
	va_end(arg);

	/* set a timer */
	popup_text_end = osd_ticks() + osd_ticks_per_second() * seconds;
}


/*-------------------------------------------------
    ui_show_fps_temp - show the FPS counter for
    a specific period of time
-------------------------------------------------*/

void ui_show_fps_temp(double seconds)
{
	if (!showfps)
		showfps_end = osd_ticks() + seconds * osd_ticks_per_second();
}


/*-------------------------------------------------
    ui_set_show_fps - show/hide the FPS counter
-------------------------------------------------*/

void ui_set_show_fps(int show)
{
	showfps = show;
	if (!show)
	{
		showfps = 0;
		showfps_end = 0;
	}
}


/*-------------------------------------------------
    ui_get_show_fps - return the current FPS
    counter visibility state
-------------------------------------------------*/

int ui_get_show_fps(void)
{
	return showfps || (showfps_end != 0);
}


/*-------------------------------------------------
    ui_set_show_profiler - show/hide the profiler
-------------------------------------------------*/

void ui_set_show_profiler(int show)
{
	if (show)
	{
		show_profiler = TRUE;
		profiler_start();
	}
	else
	{
		show_profiler = FALSE;
		profiler_stop();
	}
}


/*-------------------------------------------------
    ui_get_show_profiler - return the current
    profiler visibility state
-------------------------------------------------*/

int ui_get_show_profiler(void)
{
	return show_profiler;
}


/*-------------------------------------------------
    ui_show_menu - show the menus
-------------------------------------------------*/

void ui_show_menu(void)
{
	ui_set_handler(ui_menu_ui_handler, 0);
}


/*-------------------------------------------------
    ui_is_menu_active - return TRUE if the menu
    UI handler is active
-------------------------------------------------*/

int ui_is_menu_active(void)
{
	return (ui_handler_callback == ui_menu_ui_handler);
}


/*-------------------------------------------------
    ui_is_slider_active - return TRUE if the slider
    UI handler is active
-------------------------------------------------*/

int ui_is_slider_active(void)
{
	return (ui_handler_callback == handler_slider);
}



/***************************************************************************
    TEXT GENERATORS
***************************************************************************/

/*-------------------------------------------------
    sprintf_disclaimer - print the disclaimer
    text to the given buffer
-------------------------------------------------*/

static int sprintf_disclaimer(char *buffer)
{
	char *bufptr = buffer;
	bufptr += sprintf(bufptr, "%s\n\n", ui_getstring(UI_copyright1));
	bufptr += sprintf(bufptr, ui_getstring(UI_copyright2), _LST(Machine->gamedrv->description));
	bufptr += sprintf(bufptr, "\n\n%s", ui_getstring(UI_copyright3));
	return bufptr - buffer;
}


/*-------------------------------------------------
    sprintf_warnings - print the warning flags
    text to the given buffer
-------------------------------------------------*/

static int sprintf_warnings(char *buffer)
{
#define WARNING_FLAGS (	GAME_NOT_WORKING | \
						GAME_UNEMULATED_PROTECTION | \
						GAME_WRONG_COLORS | \
						GAME_IMPERFECT_COLORS | \
						GAME_NO_SOUND |  \
						GAME_IMPERFECT_SOUND |  \
						GAME_IMPERFECT_GRAPHICS | \
						GAME_NO_COCKTAIL)
	char *bufptr = buffer;
	int i;

	/* if no warnings, nothing to return */
	if (rom_load_warnings() == 0 && !(Machine->gamedrv->flags & WARNING_FLAGS))
		return 0;

	/* add a warning if any ROMs were loaded with warnings */
	if (rom_load_warnings() > 0)
	{
		bufptr += sprintf(bufptr, "%s\n", ui_getstring(UI_incorrectroms));
		if (Machine->gamedrv->flags & WARNING_FLAGS)
			*bufptr++ = '\n';
	}

	/* if we have at least one warning flag, print the general header */
	if (Machine->gamedrv->flags & WARNING_FLAGS)
	{
		bufptr += sprintf(bufptr, "%s\n\n", ui_getstring(UI_knownproblems));

		/* add one line per warning flag */
#ifdef MESS
		if (Machine->gamedrv->flags & GAME_COMPUTER)
			bufptr += sprintf(bufptr, "%s\n\n%s\n", ui_getstring(UI_comp1), ui_getstring(UI_comp2));
#endif
		if (Machine->gamedrv->flags & GAME_IMPERFECT_COLORS)
			bufptr += sprintf(bufptr, "%s\n", ui_getstring(UI_imperfectcolors));
		if (Machine->gamedrv->flags & GAME_WRONG_COLORS)
			bufptr += sprintf(bufptr, "%s\n", ui_getstring(UI_wrongcolors));
		if (Machine->gamedrv->flags & GAME_IMPERFECT_GRAPHICS)
			bufptr += sprintf(bufptr, "%s\n", ui_getstring(UI_imperfectgraphics));
		if (Machine->gamedrv->flags & GAME_IMPERFECT_SOUND)
			bufptr += sprintf(bufptr, "%s\n", ui_getstring(UI_imperfectsound));
		if (Machine->gamedrv->flags & GAME_NO_SOUND)
			bufptr += sprintf(bufptr, "%s\n", ui_getstring(UI_nosound));
		if (Machine->gamedrv->flags & GAME_NO_COCKTAIL)
			bufptr += sprintf(bufptr, "%s\n", ui_getstring(UI_nococktail));

		/* if there's a NOT WORKING or UNEMULATED PROTECTION warning, make it stronger */
		if (Machine->gamedrv->flags & (GAME_NOT_WORKING | GAME_UNEMULATED_PROTECTION))
		{
			const game_driver *maindrv;
			const game_driver *clone_of;
			int foundworking;

			/* add the strings for these warnings */
			if (Machine->gamedrv->flags & GAME_NOT_WORKING)
				bufptr += sprintf(bufptr, "%s\n", ui_getstring(UI_brokengame));
			if (Machine->gamedrv->flags & GAME_UNEMULATED_PROTECTION)
				bufptr += sprintf(bufptr, "%s\n", ui_getstring(UI_brokenprotection));

			/* find the parent of this driver */
			clone_of = driver_get_clone(Machine->gamedrv);
			if (clone_of != NULL && !(clone_of->flags & GAME_IS_BIOS_ROOT))
 				maindrv = clone_of;
			else
				maindrv = Machine->gamedrv;

			/* scan the driver list for any working clones and add them */
			foundworking = 0;
			for (i = 0; drivers[i] != NULL; i++)
				if (drivers[i] == maindrv || driver_get_clone(drivers[i]) == maindrv)
					if ((drivers[i]->flags & (GAME_NOT_WORKING | GAME_UNEMULATED_PROTECTION)) == 0)
					{
						/* this one works, add a header and display the name of the clone */
						if (foundworking == 0)
							bufptr += sprintf(bufptr, "\n\n%s\n\n", ui_getstring(UI_workingclones));
						bufptr += sprintf(bufptr, "%s\n", drivers[i]->name);
						foundworking = 1;
					}
		}
	}

	/* add the 'press OK' string */
	bufptr += sprintf(bufptr, "\n\n%s", ui_getstring(UI_typeok));
	return bufptr - buffer;
}


#ifdef USE_SHOW_TIME

#define DISPLAY_AMPM 0

static void display_time(void)
{
	char buf[20];
#if DISPLAY_AMPM
	char am_pm[] = "am";
#endif /* DISPLAY_AMPM */
	float width;
	time_t ltime;
	struct tm *today;

	time(&ltime);
	today = localtime(&ltime);

#if DISPLAY_AMPM
	if( today->tm_hour > 12 )
	{
		strcpy( am_pm, "pm" );
		today->tm_hour -= 12;
	}
	if( today->tm_hour == 0 ) /* Adjust if midnight hour. */
		today->tm_hour = 12;
#endif /* DISPLAY_AMPM */

#if DISPLAY_AMPM
	sprintf(buf, "%02d:%02d:%02d %s", today->tm_hour, today->tm_min, today->tm_sec, am_pm);
#else
	sprintf(buf, "%02d:%02d:%02d", today->tm_hour, today->tm_min, today->tm_sec);
#endif /* DISPLAY_AMPM */
	width = ui_get_string_width(buf) + UI_LINE_WIDTH * 2.0f;
	switch(Show_Time_Position)
	{
		case 0:
			ui_draw_text_bk(buf, 1.0f - width, 1.0f - ui_get_line_height(), UI_FILLCOLOR);
			break;

		case 1:
			ui_draw_text_bk(buf, 1.0f - width, 0.0f, UI_FILLCOLOR);
			break;

		case 2:
			ui_draw_text_bk(buf, 0.0f, 0.0f, UI_FILLCOLOR);
			break;

		case 3:
			ui_draw_text_bk(buf, 0.0f, 1.0f - ui_get_line_height(), UI_FILLCOLOR);
			break;
	}
}
#endif /* USE_SHOW_TIME */

#ifdef USE_SHOW_INPUT_LOG
static void display_input_log(void)
{
	double time_now = mame_time_to_double(mame_timer_get_time());
	double time_display = mame_time_to_double(MAME_TIME_IN_MSEC(1000));
	double time_fadeout = mame_time_to_double(MAME_TIME_IN_MSEC(1000));
	float curx;
	int i;

	if (!command_buffer[0].code)
		return;

	// adjust time for load state
	{
		double max = 0.0f;
		int i;

		for (i = 0; command_buffer[i].code; i++)
			if (max < command_buffer[i].time)
				max = command_buffer[i].time;

		if (max > time_now)
		{
			double adjust = max - time_now;

			for (i = 0; command_buffer[i].code; i++)
				command_buffer[i].time -= adjust;
		}
	}

	// find position to start display
	curx = 1.0f - UI_LINE_WIDTH;
	for (i = 0; command_buffer[i].code; i++)
		curx -= ui_get_char_width(command_buffer[i].code);

	for (i = 0; command_buffer[i].code; i++)
	{
		if (curx >= UI_LINE_WIDTH)
			break;

		curx += ui_get_char_width(command_buffer[i].code);
	}

	ui_draw_box(0.0f, 1.0f - ui_get_line_height(), 1.0f, 1.0f, UI_FILLCOLOR);

	for (; command_buffer[i].code; i++)
	{
		double rate = time_now - command_buffer[i].time;

		if (rate < time_display + time_fadeout)
		{
			int level = 255 - ((rate - time_display) / time_fadeout) * 255;
			rgb_t fgcolor;

			if (level > 255)
				level = 255;

			fgcolor = MAKE_ARGB(255, level, level, level);

			render_ui_add_char(curx, 1.0f - ui_get_line_height(), ui_get_line_height(), render_get_ui_aspect(), fgcolor, ui_font, command_buffer[i].code);
		}
		curx += ui_get_char_width(command_buffer[i].code);
	}
}
#endif /* USE_SHOW_INPUT_LOG */



#ifdef INP_CAPTION
//============================================================
//	draw_caption
//============================================================

static void draw_caption(running_machine *machine)
{
	static char next_caption[512], caption_text[512];
	static int next_caption_timer;

	if (machine->caption_file && next_caption_frame < 0)
	{
		char	read_buf[512];
skip_comment:
		if (mame_fgets(read_buf, 511, machine->caption_file) == NULL)
		{
			mame_fclose(machine->caption_file);
			machine->caption_file = NULL;
		}
		else
		{
			char	buf[16] = "";
			int		i, j;

			for (i = 0, j = 0; i < 16; i++)
			{
				if (read_buf[i] == '\t' || read_buf[i] == ' ')
					continue;
				if ((read_buf[i] == '#' || read_buf[i] == '\r' || read_buf[i] == '\n') && j == 0)
					goto skip_comment;
				if (read_buf[i] < '0' || read_buf[i] > '9')
				{
					buf[j++] ='\0';
					break;
				}
				buf[j++] = read_buf[i];
			}

			next_caption_frame = strtol(buf, NULL, 10);
			next_caption_timer = 0;
			if (next_caption_frame == 0)
			{
				next_caption_frame = cpu_getcurrentframe();
				strcpy(next_caption, _("Error: illegal caption file"));
				mame_fclose(machine->caption_file);
				machine->caption_file = NULL;
			}

			for (;;i++)
			{
				if (read_buf[i] == '(')
				{
					for (i++, j = 0;;i++)
					{
						if (read_buf[i] == '\t' || read_buf[i] == ' ')
							continue;
						if (read_buf[i] < '0' || read_buf[i] > '9')
						{
							buf[j++] ='\0';
							break;
						}
						buf[j++] = read_buf[i];
					}

					next_caption_timer = strtol(buf, NULL, 10);

					for (;;i++)
					{
						if (read_buf[i] == '\t' || read_buf[i] == ' ')
							continue;
						if (read_buf[i] == ':')
							break;
					}
				}
				if (read_buf[i] != '\t' && read_buf[i] != ' ' && read_buf[i] != ':')
					break;
			}
			if (next_caption_timer == 0)
			{
				next_caption_timer = 5 * SUBSECONDS_TO_HZ(Machine->screen[0].refresh);	// 5sec.
			}

			strcpy(next_caption, &read_buf[i]);

			for (i = 0; next_caption[i] != '\0'; i++)
			{
				if (next_caption[i] == '\r' || next_caption[i] == '\n')
				{
					next_caption[i] = '\0';
					break;
				}
			}
		}
	}

	if (next_caption_timer && next_caption_frame <= cpu_getcurrentframe())
	{
		caption_timer = next_caption_timer;
		strcpy(caption_text, next_caption);
		next_caption_frame = -1;
		next_caption_timer = 0;
	}

	if (caption_timer)
	{
		ui_draw_text_box(caption_text, JUSTIFY_LEFT, 0.5, 1.0, messagebox_backcolor);
		caption_timer--;
	}
}
#endif /* INP_CAPTION */


/*-------------------------------------------------
    sprintf_game_info - print the game info text
    to the given buffer
-------------------------------------------------*/

int sprintf_game_info(char *buffer)
{
	char *bufptr = buffer;
	int cpunum, sndnum;
	int count;

	/* print description, manufacturer, and CPU: */
	bufptr += sprintf(bufptr, "%s\n%s %s\n\n%s:\n",
		_LST(Machine->gamedrv->description),
		Machine->gamedrv->year,
		_MANUFACT(Machine->gamedrv->manufacturer),
		ui_getstring(UI_cpu));

	/* loop over all CPUs */
	for (cpunum = 0; cpunum < MAX_CPU && Machine->drv->cpu[cpunum].cpu_type != CPU_DUMMY; cpunum += count)
	{
		int type = Machine->drv->cpu[cpunum].cpu_type;
		int clock = Machine->drv->cpu[cpunum].cpu_clock;

		/* count how many identical CPUs we have */
		for (count = 1; cpunum + count < MAX_CPU; count++)
			if (Machine->drv->cpu[cpunum + count].cpu_type != type ||
		        Machine->drv->cpu[cpunum + count].cpu_clock != clock)
		    	break;

		/* if more than one, prepend a #x in front of the CPU name */
		if (count > 1)
			bufptr += sprintf(bufptr, "%d" UTF8_MULTIPLY, count);
		bufptr += sprintf(bufptr, "%s", cputype_name(type));

		/* display clock in kHz or MHz */
		if (clock >= 1000000)
			bufptr += sprintf(bufptr, " %d.%06d" UTF8_NBSP "MHz\n", clock / 1000000, clock % 1000000);
		else
			bufptr += sprintf(bufptr, " %d.%03d" UTF8_NBSP "kHz\n", clock / 1000, clock % 1000);
	}

	/* append the Sound: string */
	bufptr += sprintf(bufptr, "\n%s:\n", ui_getstring(UI_sound));

	/* loop over all sound chips */
	for (sndnum = 0; sndnum < MAX_SOUND && Machine->drv->sound[sndnum].sound_type != SOUND_DUMMY; sndnum += count)
	{
		int type = Machine->drv->sound[sndnum].sound_type;
		int clock = sndnum_clock(sndnum);

		/* count how many identical sound chips we have */
		for (count = 1; sndnum + count < MAX_SOUND; count++)
			if (Machine->drv->sound[sndnum + count].sound_type != type ||
		        sndnum_clock(sndnum + count) != clock)
		    	break;

		/* if more than one, prepend a #x in front of the CPU name */
		if (count > 1)
			bufptr += sprintf(bufptr, "%d" UTF8_MULTIPLY, count);
		bufptr += sprintf(bufptr, "%s", sndnum_name(sndnum));

		/* display clock in kHz or MHz */
		if (clock >= 1000000)
			bufptr += sprintf(bufptr, " %d.%06d" UTF8_NBSP "MHz\n", clock / 1000000, clock % 1000000);
		else if (clock != 0)
			bufptr += sprintf(bufptr, " %d.%03d" UTF8_NBSP "kHz\n", clock / 1000, clock % 1000);
		else
			*bufptr++ = '\n';
	}

	/* display vector information for vector games */
	if (Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
		bufptr += sprintf(bufptr, "\n%s\n", ui_getstring(UI_vectorgame));

	/* display screen resolution and refresh rate info for raster games */
	else
		bufptr += sprintf(bufptr,"\n%s:\n%d " UTF8_MULTIPLY " %d (%s) %f" UTF8_NBSP "Hz\n",
				ui_getstring(UI_screenres),
				Machine->screen[0].visarea.max_x - Machine->screen[0].visarea.min_x + 1,
				Machine->screen[0].visarea.max_y - Machine->screen[0].visarea.min_y + 1,
				(Machine->gamedrv->flags & ORIENTATION_SWAP_XY) ? "V" : "H",
				SUBSECONDS_TO_HZ(Machine->screen[0].refresh));
	return bufptr - buffer;
}



/***************************************************************************
    UI HANDLERS
***************************************************************************/

/*-------------------------------------------------
    handler_messagebox - displays the current
    messagebox_text string but handles no input
-------------------------------------------------*/

static UINT32 handler_messagebox(UINT32 state)
{
	ui_draw_text_box(messagebox_text, JUSTIFY_LEFT, 0.5f, 0.5f, messagebox_backcolor);
	return 0;
}


/*-------------------------------------------------
    handler_messagebox_ok - displays the current
    messagebox_text string and waits for an OK
-------------------------------------------------*/

static UINT32 handler_messagebox_ok(UINT32 state)
{
	int res;

	/* draw a standard message window */
	ui_draw_text_box(messagebox_text, JUSTIFY_LEFT, 0.5f, 0.5f, messagebox_backcolor);

	res = ui_window_scroll_keys();
	if (res == 0)
	{
		/* an 'O' or left joystick kicks us to the next state */
		if (state == 0 && (input_code_pressed_once(KEYCODE_O) || input_ui_pressed(IPT_UI_LEFT)))
			state++;

		/* a 'K' or right joystick exits the state */
		else if (state == 1 && (input_code_pressed_once(KEYCODE_K) || input_ui_pressed(IPT_UI_RIGHT)))
			state = UI_HANDLER_CANCEL;
	}

	/* if the user cancels, exit out completely */
	if (res == 2)
	{
		mame_schedule_exit(Machine);
		state = UI_HANDLER_CANCEL;
	}

	return state;
}


/*-------------------------------------------------
    handler_messagebox_selectkey - displays the
    current messagebox_text string and waits for
    selectkey press
-------------------------------------------------*/

static UINT32 handler_messagebox_selectkey(UINT32 state)
{
	int res;

	/* draw a standard message window */
	ui_draw_text_box(messagebox_text, JUSTIFY_LEFT, 0.5f, 0.5f, messagebox_backcolor);

	res = ui_window_scroll_keys();
	/* if the user cancels, exit out completely */
	if (res == 2)
	{
		mame_schedule_exit(Machine);
		state = UI_HANDLER_CANCEL;
	}

	/* if select key is pressed, just exit */
	if (res == 1)
	{
		if (input_code_poll_switches(FALSE) != INPUT_CODE_INVALID)
			state = UI_HANDLER_CANCEL;
	}

	return state;
}


/*-------------------------------------------------
    handler_ingame - in-game handler takes care
    of the standard keypresses
-------------------------------------------------*/

static UINT32 handler_ingame(UINT32 state)
{
	int is_paused = mame_is_paused(Machine);

#ifdef KAILLERA
	//kt start
	if( kPlay && Kaillera_StateSave_SelectFile ) {
		int file = 0;
		input_code code;
		ui_draw_message_window( _("Select position (0-9, A-Z) to save to") );
		
		if (input_ui_pressed(IPT_UI_CANCEL)) {
			Kaillera_StateSave_SelectFile = 0;
			return 0;
		}
		/* check for A-Z or 0-9 */
		for (code = KEYCODE_A; code <= (input_code)KEYCODE_Z; code++)
			if (input_code_pressed_once(code))
				file = code - KEYCODE_A + 'a';
		if (file == 0)
			for (code = KEYCODE_0; code <= (input_code)KEYCODE_9; code++)
				if (input_code_pressed_once(code))
					file = code - KEYCODE_0 + '0';
		if (file == 0)
			for (code = KEYCODE_0_PAD; code <= (input_code)KEYCODE_9_PAD; code++)
				if (input_code_pressed_once(code))
					file = code - KEYCODE_0_PAD + '0';
		if (file > 0) {
			long dat[64];
			
			KailleraChatdataPreparationcheck.nmb			= 2;
			KailleraChatdataPreparationcheck.str			= (char *)"Select Slot";
			KailleraChatdataPreparationcheck.count			= KailleraPlayerOption.max;
			KailleraChatdataPreparationcheck.timeremainder	= 256;
			KailleraChatdataPreparationcheck.addtime		= 256;
			KailleraChatdataPreparationcheck.maxtime		= 256;
			KailleraChatdataPreparationcheck.Callback	= PreparationcheckNull;

			dat[0] = KailleraChatdataPreparationcheck.nmb;
			dat[1] = file;
			kailleraChatSend(kChatData(&dat[0], 8));//チャットで全員に伝える。
			Kaillera_StateSave_SelectFile = 0;
			return 0;
		}
		return 0;
	}

	if( kPlay && Kaillera_Overclock_Flags ) {
		int rate = 0;
		input_code code;
		ui_draw_message_window( _("Please push overclock rate (1-8) x 50%") );
		
		if (input_ui_pressed(IPT_UI_CANCEL)) {
			Kaillera_Overclock_Flags = 0;
			return 0;
		}
		for (code = KEYCODE_1; code <= (input_code)KEYCODE_8; code++)
			if (input_code_pressed_once(code))
				rate = code - KEYCODE_0;
		if (rate > 0) {
			long dat[64];
			
			KailleraChatdataPreparationcheck.nmb			= 7;
			KailleraChatdataPreparationcheck.str			= (char *)"Overclock";
			KailleraChatdataPreparationcheck.count			= KailleraPlayerOption.max;
			KailleraChatdataPreparationcheck.timeremainder	= 256;
			KailleraChatdataPreparationcheck.addtime		= 256;
			KailleraChatdataPreparationcheck.maxtime		= 256;
			KailleraChatdataPreparationcheck.Callback	= SendOverclockParam;

			dat[0] = KailleraChatdataPreparationcheck.nmb;
			dat[1] = rate;
			kailleraChatSend(kChatData(&dat[0], 8));//チャットで全員に伝える。
			Kaillera_Overclock_Flags = 0;
			return 0;
		}
		return 0;
	}

	if (kPlay && quiting) {
		ui_draw_message_window( _("Please press the [Y] key, for ending") );
		if (input_ui_pressed(IPT_UI_CANCEL)) {
			quiting = 0;
			return 0;
		}
		if (input_code_pressed_once(KEYCODE_Y)) {
			quiting = 0;
			//if(code_pressed( KEYCODE_LSHIFT ) &&
			if(KailleraStartOption.player == 1 &&
				KailleraPlayerOption.max > 1) {
				long dat[64];
				dat[0] = 12;
				dat[1] = 0xffffffff;	//全員ゲーム終了
				kailleraChatSend(kChatData(&dat[0], 8));

				return 0;
			}
			mame_schedule_exit(Machine);
			return 0;
		}



		//if (osd_quit_window() || quiting == 2 ) {
		if ( quiting == 2 ) {
			quiting = 0;
			mame_schedule_exit(Machine);
			return 0;
		}
		return 0;
	}
	//kt end

	if (kPlay)
		KailleraChatUpdate();

	if (KailleraChatIsActive())
	{
		/* This call is for the cheat, it must be called once a frame */
		//if (options.cheat) DoCheat(bitmap);
	}
	else
	{
#endif /* KAILLERA */

	/* first draw the FPS counter */
	if (showfps || osd_ticks() < showfps_end)
	{
		ui_draw_text_full_fixed_width(video_get_speed_text(), 0.0f, 0.0f, 1.0f,
					JUSTIFY_RIGHT, WRAP_WORD, DRAW_OPAQUE, ARGB_WHITE, ui_bgcolor, NULL, NULL);
	}
	else
		showfps_end = 0;

	/* draw the profiler if visible */
	if (show_profiler)
		ui_draw_text_full(profiler_get_text(), 0.0f, 0.0f, 1.0f, JUSTIFY_LEFT, WRAP_WORD, DRAW_OPAQUE, ARGB_WHITE, ui_bgcolor, NULL, NULL);

	/* let the cheat engine display its stuff */
	if (options_get_bool(mame_options(), OPTION_CHEAT))
		cheat_display_watches();

	/* display any popup messages */
	if (osd_ticks() < popup_text_end)
		ui_draw_text_box(messagebox_text, JUSTIFY_CENTER, 0.5f, 0.9f, messagebox_backcolor);
	else
		popup_text_end = 0;

	/* if we're single-stepping, pause now */
	if (single_step)
	{
		mame_pause(Machine, TRUE);
		single_step = FALSE;
	}

#ifdef MESS
	if (mess_disable_builtin_ui())
		return 0;
#endif

	/* if the user pressed ESC, stop the emulation */
	if (input_ui_pressed(IPT_UI_CANCEL))
		return ui_set_handler(handler_confirm_quit, 0);

	/* turn on menus if requested */
	if (input_ui_pressed(IPT_UI_CONFIGURE))
		return ui_set_handler(ui_menu_ui_handler, 0);

	if (options_get_bool(mame_options(), OPTION_CHEAT) && input_ui_pressed(IPT_UI_CHEAT))
		return ui_set_handler(ui_menu_ui_handler, SHORTCUT_MENU_CHEAT);

#ifdef CMD_LIST
	if (input_ui_pressed(IPT_UI_COMMAND))
		return ui_set_handler(ui_menu_ui_handler, SHORTCUT_MENU_COMMAND);
#endif /* CMD_LIST */

	/* if the on-screen display isn't up and the user has toggled it, turn it on */
#ifdef MAME_DEBUG
	if (!Machine->debug_mode)
#endif
		if (input_ui_pressed(IPT_UI_ON_SCREEN_DISPLAY))
			return ui_set_handler(handler_slider, 0);

#ifdef KAILLERA
	//input_ui_temp = 0;	//kt
	if (!kPlay)
	{
#endif /* KAILLERA */
	/* handle a reset request */
	if (input_ui_pressed(IPT_UI_RESET_MACHINE))
		mame_schedule_hard_reset(Machine);
	if (input_ui_pressed(IPT_UI_SOFT_RESET))
#ifdef KAILLERA
		input_ui_temp = 3;
#else
		mame_schedule_soft_reset(Machine);
#endif /* KAILLERA */

	/* handle a request to display graphics/palette */
	if (input_ui_pressed(IPT_UI_SHOW_GFX))
	{
		if (!is_paused)
			mame_pause(Machine, TRUE);
		return ui_set_handler(ui_gfx_ui_handler, is_paused);
	}

	/* handle a save state request */
	if (input_ui_pressed(IPT_UI_SAVE_STATE))
	{
		mame_pause(Machine, TRUE);
		return ui_set_handler(handler_load_save, LOADSAVE_SAVE);
	}

	/* handle a load state request */
	if (input_ui_pressed(IPT_UI_LOAD_STATE))
	{
		mame_pause(Machine, TRUE);
		return ui_set_handler(handler_load_save, LOADSAVE_LOAD);
	}
#ifdef KAILLERA
	}
#endif /* KAILLERA */

	/* handle a save snapshot request */
	if (input_ui_pressed(IPT_UI_SNAPSHOT))
		video_save_active_screen_snapshots(Machine);

#ifdef INP_CAPTION
	draw_caption(Machine);
#endif /* INP_CAPTION */

#ifdef KAILLERA
	if (!kPlay)
#endif /* KAILLERA */
	/* toggle pause */
	if (input_ui_pressed(IPT_UI_PAUSE))
	{
		/* with a shift key, it is single step */
		if (is_paused && (input_code_pressed(KEYCODE_LSHIFT) || input_code_pressed(KEYCODE_RSHIFT)))
		{
			single_step = TRUE;
			mame_pause(Machine, FALSE);
		}
		else
			mame_pause(Machine, !mame_is_paused(Machine));
	}


#ifdef USE_SHOW_TIME
	if (input_ui_pressed(IPT_UI_TIME))
	{
		if (show_time)
		{
			Show_Time_Position++;

			if (Show_Time_Position > 3)
			{
				Show_Time_Position = 0;
				show_time = 0;
			}
		}
		else
		{
			Show_Time_Position = 0;
			show_time = 1;
		}
	}

	if (show_time)
		display_time();
#endif /* USE_SHOW_TIME */

#ifdef USE_SHOW_INPUT_LOG
	if (input_ui_pressed(IPT_UI_SHOW_INPUT_LOG))
	{
		show_input_log ^= 1;
		command_buffer[0].code = '\0';
	}

	/* show popup message if input exist any log */
	if (show_input_log)
		display_input_log();
#endif /* USE_SHOW_INPUT_LOG */

#ifdef KAILLERA
	if (!kPlay)
	{
#endif /* KAILLERA */
	/* toggle movie recording */
	if (input_ui_pressed(IPT_UI_RECORD_MOVIE))
	{
		if (!video_is_movie_active(Machine, 0))
		{
			video_movie_begin_recording(Machine, 0, NULL);
			popmessage(_("REC START"));
		}
		else
		{
			video_movie_end_recording(Machine, 0);
			popmessage(_("REC STOP"));
		}
	}

#ifdef MAME_AVI
	if (input_ui_pressed(IPT_UI_RECORD_AVI))
		toggle_record_avi();
#endif /* MAME_AVI */

#ifdef KAILLERA
	}
	}
#endif /* KAILLERA */

	/* toggle profiler display */
	if (input_ui_pressed(IPT_UI_SHOW_PROFILER))
		ui_set_show_profiler(!ui_get_show_profiler());

	/* toggle FPS display */
	if (input_ui_pressed(IPT_UI_SHOW_FPS))
		ui_set_show_fps(!ui_get_show_fps());

	/* toggle crosshair display */
	if (input_ui_pressed(IPT_UI_TOGGLE_CROSSHAIR))
		video_crosshair_toggle();

#ifdef USE_PSXPLUGIN
	if (Machine->gpu_plugin_loaded==1)
	{
		if (old_force_pause != force_pause)
		{
			old_force_pause = force_pause;
			mame_pause(Machine, force_pause);
		}
	}
#endif /* USE_PSXPLUGIN */

#ifdef KAILLERA
	if (!kPlay)
	{
#endif /* KAILLERA */
	/* increment frameskip? */
	if (input_ui_pressed(IPT_UI_FRAMESKIP_INC))
	{
		/* get the current value and increment it */
		int newframeskip = video_get_frameskip() + 1;
		if (newframeskip > MAX_FRAMESKIP)
			newframeskip = -1;
		video_set_frameskip(newframeskip);

		/* display the FPS counter for 2 seconds */
		ui_show_fps_temp(2.0);
	}

	/* decrement frameskip? */
	if (input_ui_pressed(IPT_UI_FRAMESKIP_DEC))
	{
		/* get the current value and decrement it */
		int newframeskip = video_get_frameskip() - 1;
		if (newframeskip < -1)
			newframeskip = MAX_FRAMESKIP;
		video_set_frameskip(newframeskip);

		/* display the FPS counter for 2 seconds */
		ui_show_fps_temp(2.0);
	}

	/* toggle throttle? */
	if (input_ui_pressed(IPT_UI_THROTTLE))
		video_set_throttle(!video_get_throttle());

	/* check for fast forward */
	if (input_port_type_pressed(IPT_UI_FAST_FORWARD, 0))
	{
		video_set_fastforward(TRUE);
		ui_show_fps_temp(0.5);
	}
	else
		video_set_fastforward(FALSE);
#ifdef KAILLERA
	}
#endif /* KAILLERA */

	return 0;
}


/*-------------------------------------------------
    handler_slider - displays the current slider
    and calls the slider handler
-------------------------------------------------*/

static UINT32 handler_slider(UINT32 state)
{
	slider_state *cur = &slider_list[slider_current];
	INT32 increment = 0, newval;
	char textbuf[256];

	/* left/right control the increment */
	if (input_ui_pressed_repeat(IPT_UI_LEFT,6))
		increment = -cur->incval;
	if (input_ui_pressed_repeat(IPT_UI_RIGHT,6))
		increment = cur->incval;

	/* alt goes to 1, shift goes 10x smaller, control goes 10x larger */
	if (increment != 0)
	{
		if (input_code_pressed(KEYCODE_LALT) || input_code_pressed(KEYCODE_RALT))
			increment = (increment < 0) ? -1 : 1;
		if (input_code_pressed(KEYCODE_LSHIFT) || input_code_pressed(KEYCODE_RSHIFT))
			increment = (increment < -10 || increment > 10) ? (increment / 10) : ((increment < 0) ? -1 : 1);
		if (input_code_pressed(KEYCODE_LCONTROL) || input_code_pressed(KEYCODE_RCONTROL))
			increment *= 10;
	}

	/* determine the new value */
	newval = (*cur->update)(0, NULL, cur->arg) + increment;

	/* select resets to the default value */
	if (input_ui_pressed(IPT_UI_SELECT))
		newval = cur->defval;

	/* clamp within bounds */
	if (newval < cur->minval)
		newval = cur->minval;
	if (newval > cur->maxval)
		newval = cur->maxval;

	/* update the new data and get the text */
	(*cur->update)(newval, textbuf, cur->arg);

	/* display the UI */
	slider_display(textbuf, cur->minval, cur->maxval, cur->defval, newval);

	/* up/down select which slider to control */
	if (input_ui_pressed_repeat(IPT_UI_DOWN,6))
		slider_current = (slider_current + 1) % slider_count;
	if (input_ui_pressed_repeat(IPT_UI_UP,6))
		slider_current = (slider_current + slider_count - 1) % slider_count;

	/* the slider toggle or ESC will cancel out of our display */
	if (input_ui_pressed(IPT_UI_ON_SCREEN_DISPLAY) || input_ui_pressed(IPT_UI_CANCEL))
		return UI_HANDLER_CANCEL;

	/* the menu key will take us directly to the menu */
	if (input_ui_pressed(IPT_UI_CONFIGURE))
		return ui_set_handler(ui_menu_ui_handler, 0);

	return 0;
}


/*-------------------------------------------------
    handler_load_save - leads the user through
    specifying a game to save or load
-------------------------------------------------*/

static UINT32 handler_load_save(UINT32 state)
{
#ifndef KAILLERA
	char filename[20];
#endif /* !KAILLERA */
	input_code code;
	char file = 0;

	/* if we're not in the middle of anything, skip */
	if (state == LOADSAVE_NONE)
		return 0;

	/* okay, we're waiting for a key to select a slot; display a message */
	if (state == LOADSAVE_SAVE)
		ui_draw_message_window(_("Select position to save to"));
	else
		ui_draw_message_window(_("Select position to load from"));

	/* check for cancel key */
	if (input_ui_pressed(IPT_UI_CANCEL))
	{
		/* display a popup indicating things were cancelled */
		if (state == LOADSAVE_SAVE)
			popmessage(_("Save cancelled"));
		else
			popmessage(_("Load cancelled"));

		/* reset the state */
		mame_pause(Machine, FALSE);
		return UI_HANDLER_CANCEL;
	}

	/* check for A-Z or 0-9 */
	for (code = KEYCODE_A; code <= (input_code)KEYCODE_Z; code++)
		if (input_code_pressed_once(code))
			file = code - KEYCODE_A + 'a';
	if (file == 0)
		for (code = KEYCODE_0; code <= (input_code)KEYCODE_9; code++)
			if (input_code_pressed_once(code))
				file = code - KEYCODE_0 + '0';
	if (file == 0)
		for (code = KEYCODE_0_PAD; code <= (input_code)KEYCODE_9_PAD; code++)
			if (input_code_pressed_once(code))
				file = code - KEYCODE_0_PAD + '0';
	if (file == 0)
		return state;

#ifdef KAILLERA
	if (file > 0)
	{
		if (state == LOADSAVE_SAVE)
			input_ui_temp = 1;
		else
			input_ui_temp = 2;
		input_ui_temp_dat[0] = file;
	}
#else

	/* display a popup indicating that the save will proceed */
	sprintf(filename, "%c", file);
	if (state == LOADSAVE_SAVE)
	{
		popmessage(_("Save to position %c"), file);
		mame_schedule_save(Machine, filename);
	}
	else
	{
		popmessage(_("Load from position %c"), file);
		mame_schedule_load(Machine, filename);
	}
#endif /* KAILLERA */

	/* remove the pause and reset the state */
	mame_pause(Machine, FALSE);
	return UI_HANDLER_CANCEL;
}



static UINT32 handler_confirm_quit(UINT32 state)
{
	const char *quit_message =
		"Quit the game?\n\n"
		"Press Select key/button to quit,\n"
		"Cancel key/button to continue.";

#ifdef KAILLERA
	if(kPlay) { quiting = 1; return UI_HANDLER_CANCEL; }
#endif /* KAILLERA */

	if (!options_get_bool(mame_options(), OPTION_CONFIRM_QUIT))
	{
		mame_schedule_exit(Machine);
		return UI_HANDLER_CANCEL;
	}

	ui_draw_message_window(_(quit_message));

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		mame_schedule_exit(Machine);
		return UI_HANDLER_CANCEL;
	}

	if (input_ui_pressed(IPT_UI_CANCEL))
	{
		return UI_HANDLER_CANCEL;
	}

	return 0;
}


/***************************************************************************
    SLIDER CONTROLS
***************************************************************************/

/*-------------------------------------------------
    slider_init - initialize the list of slider
    controls
-------------------------------------------------*/

static void slider_init(void)
{
	int numitems, item;
	input_port_entry *in;

	slider_count = 0;

	/* add overall volume */
	slider_config(&slider_list[slider_count++], -32, 0, 0, 1, slider_volume, 0);

	/* add per-channel volume */
	numitems = sound_get_user_gain_count();
	for (item = 0; item < numitems; item++)
		slider_config(&slider_list[slider_count++], 0, sound_get_default_gain(item) * 1000.0f + 0.5f, 2000, 20, slider_mixervol, item);

	/* add analog adjusters */
	for (in = Machine->input_ports; in && in->type != IPT_END; in++)
		if ((in->type & 0xff) == IPT_ADJUSTER)
			slider_config(&slider_list[slider_count++], 0, in->default_value >> 8, 100, 1, slider_adjuster, in - Machine->input_ports);

#ifdef KAILLERA
	if (!kPlay)
#endif /* KAILLERA */
	if (options_get_bool(mame_options(), OPTION_CHEAT))
	{
		/* add CPU overclocking */
		numitems = cpu_gettotalcpu();
		for (item = 0; item < numitems; item++)
			slider_config(&slider_list[slider_count++], 50, 1000, 4000, 50, slider_overclock, item);

		/* add refresh rate tweaker */
		slider_config(&slider_list[slider_count++], -10000, 0, 10000, 1000, slider_refresh, 0);
	}

	for (item = 0; item < MAX_SCREENS; item++)
		if (Machine->drv->screen[item].tag != NULL)
		{
			int defxscale = floor(Machine->drv->screen[item].xscale * 1000.0f + 0.5f);
			int defyscale = floor(Machine->drv->screen[item].yscale * 1000.0f + 0.5f);
			int defxoffset = floor(Machine->drv->screen[item].xoffset * 1000.0f + 0.5f);
			int defyoffset = floor(Machine->drv->screen[item].yoffset * 1000.0f + 0.5f);

			/* add standard brightness/contrast/gamma controls per-screen */
			slider_config(&slider_list[slider_count++], 100, 1000, 2000, 10, slider_brightness, item);
			slider_config(&slider_list[slider_count++], 100, 1000, 2000, 50, slider_contrast, item);
			slider_config(&slider_list[slider_count++], 100, 1000, 3000, 50, slider_gamma, item);

			/* add scale and offset controls per-screen */
			slider_config(&slider_list[slider_count++], 500, (defxscale == 0) ? 1000 : defxscale, 1500, 2, slider_xscale, item);
			slider_config(&slider_list[slider_count++], -500, defxoffset, 500, 2, slider_xoffset, item);
			slider_config(&slider_list[slider_count++], 500, (defyscale == 0) ? 1000 : defyscale, 1500, 2, slider_yscale, item);
			slider_config(&slider_list[slider_count++], -500, defyoffset, 500, 2, slider_yoffset, item);
		}

	if (Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
	{
		/* add flicker control */
		slider_config(&slider_list[slider_count++], 0, 0, 1000, 10, slider_flicker, 0);
		slider_config(&slider_list[slider_count++], 10, 100, 1000, 10, slider_beam, 0);
	}

#ifdef MAME_DEBUG
	/* add crosshair adjusters */
	for (in = Machine->input_ports; in && in->type != IPT_END; in++)
		if (in->analog.crossaxis != CROSSHAIR_AXIS_NONE && in->player == 0)
		{
			slider_config(&slider_list[slider_count++], -3000, 1000, 3000, 100, slider_crossscale, in->analog.crossaxis);
			slider_config(&slider_list[slider_count++], -3000, 0, 3000, 100, slider_crossoffset, in->analog.crossaxis);
		}
#endif
}


/*-------------------------------------------------
    slider_display - display a slider box with
    text
-------------------------------------------------*/

static void slider_display(const char *text, int minval, int maxval, int defval, int curval)
{
	float percentage = (float)(curval - minval) / (float)(maxval - minval);
	float default_percentage = (float)(defval - minval) / (float)(maxval - minval);
	float space_width = ui_get_char_width(' ');
	float line_height = ui_get_line_height();
	float ui_width, ui_height;
	float text_height;

	/* leave a spaces' worth of room along the left/right sides, and a lines' worth on the top/bottom */
	ui_width = 1.0f - 2.0f * space_width;
	ui_height = 1.0f - 2.0f * line_height;

	/* determine the text height */
	ui_draw_text_full(text, 0, 0, ui_width - 2 * UI_BOX_LR_BORDER,
				JUSTIFY_CENTER, WRAP_WORD, DRAW_NONE, ARGB_WHITE, ARGB_BLACK, NULL, &text_height);

	/* add a box around the whole area */
	ui_draw_outlined_box(	space_width,
					line_height + ui_height - text_height - line_height - 2 * UI_BOX_TB_BORDER,
					space_width + ui_width,
					 line_height + ui_height, UI_FILLCOLOR);

	/* draw the thermometer */
	slider_draw_bar(2.0f * space_width, line_height + ui_height - UI_BOX_TB_BORDER - text_height - line_height * 0.75f,
			ui_width - 2.0f * space_width, line_height * 0.75f, percentage, default_percentage);

	/* draw the actual text */
	ui_draw_text_full(text, space_width + UI_BOX_LR_BORDER, line_height + ui_height - UI_BOX_TB_BORDER - text_height, ui_width - 2.0f * UI_BOX_LR_BORDER,
				JUSTIFY_CENTER, WRAP_WORD, DRAW_NORMAL, ARGB_WHITE, ARGB_BLACK, NULL, &text_height);
}


/*-------------------------------------------------
    slider_draw_bar - draw a slider thermometer
-------------------------------------------------*/

static void slider_draw_bar(float leftx, float topy, float width, float height, float percentage, float default_percentage)
{
	float current_x, default_x;
	float bar_top, bar_bottom;

	/* compute positions */
	bar_top = topy + 0.125f * height;
	bar_bottom = topy + 0.875f * height;
	default_x = leftx + width * default_percentage;
	current_x = leftx + width * percentage;

	/* fill in the percentage */
	render_ui_add_rect(leftx, bar_top, current_x, bar_bottom, ARGB_WHITE, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));

	/* draw the top and bottom lines */
	render_ui_add_line(leftx, bar_top, leftx + width, bar_top, UI_LINE_WIDTH, ARGB_WHITE, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
	render_ui_add_line(leftx, bar_bottom, leftx + width, bar_bottom, UI_LINE_WIDTH, ARGB_WHITE, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));

	/* draw default marker */
	render_ui_add_line(default_x, topy, default_x, bar_top, UI_LINE_WIDTH, ARGB_WHITE, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
	render_ui_add_line(default_x, bar_bottom, default_x, topy + height, UI_LINE_WIDTH, ARGB_WHITE, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
}


/*-------------------------------------------------
    slider_volume - global volume slider callback
-------------------------------------------------*/

static INT32 slider_volume(INT32 newval, char *buffer, int arg)
{
	if (buffer != NULL)
	{
		sound_set_attenuation(newval);
		sprintf(buffer, "%s %3ddB", ui_getstring(UI_volume), sound_get_attenuation());
	}
	return sound_get_attenuation();
}


/*-------------------------------------------------
    slider_mixervol - single channel volume
    slider callback
-------------------------------------------------*/

static INT32 slider_mixervol(INT32 newval, char *buffer, int arg)
{
	if (buffer != NULL)
	{
		sound_set_user_gain(arg, (float)newval * 0.001f);
		sprintf(buffer, "%s %s %4.2f", sound_get_user_gain_name(arg), ui_getstring(UI_volume), sound_get_user_gain(arg));
	}
	return floor(sound_get_user_gain(arg) * 1000.0f + 0.5f);
}


/*-------------------------------------------------
    slider_adjuster - analog adjuster slider
    callback
-------------------------------------------------*/

static INT32 slider_adjuster(INT32 newval, char *buffer, int arg)
{
	input_port_entry *in = &Machine->input_ports[arg];
	if (buffer != NULL)
	{
		in->default_value = (in->default_value & ~0xff) | (newval & 0xff);
		sprintf(buffer, "%s %d%%", _(in->name), in->default_value & 0xff);
	}
	return in->default_value & 0xff;
}


/*-------------------------------------------------
    slider_overclock - CPU overclocker slider
    callback
-------------------------------------------------*/

static INT32 slider_overclock(INT32 newval, char *buffer, int arg)
{
	if (buffer != NULL)
	{
		cpunum_set_clockscale(arg, (float)newval * 0.001f);
		sprintf(buffer, "%s %s%d %3.0f%%", ui_getstring(UI_overclock), ui_getstring(UI_cpu), arg, floor(cpunum_get_clockscale(arg) * 100.0f + 0.5f));
	}
	return floor(cpunum_get_clockscale(arg) * 1000.0f + 0.5f);
}


/*-------------------------------------------------
    slider_refresh - refresh rate slider callback
-------------------------------------------------*/

static INT32 slider_refresh(INT32 newval, char *buffer, int arg)
{
	double defrefresh = SUBSECONDS_TO_HZ(Machine->drv->screen[arg].defstate.refresh);
	double refresh;

	if (buffer != NULL)
	{
		screen_state *state = &Machine->screen[arg];
		video_screen_configure(arg, state->width, state->height, &state->visarea, HZ_TO_SUBSECONDS(defrefresh + (float)newval * 0.001f));
		sprintf(buffer, _("Screen %d %s %.3f"), arg, ui_getstring(UI_refresh_rate), SUBSECONDS_TO_HZ(Machine->screen[arg].refresh));
	}
	refresh = SUBSECONDS_TO_HZ(Machine->screen[arg].refresh);
	return floor((refresh - defrefresh) * 1000.0f + 0.5f);
}


/*-------------------------------------------------
    slider_brightness - screen brightness slider
    callback
-------------------------------------------------*/

static INT32 slider_brightness(INT32 newval, char *buffer, int arg)
{
	render_container *container = render_container_get_screen(arg);
	if (buffer != NULL)
	{
		render_container_set_brightness(container, (float)newval * 0.001f);
		sprintf(buffer, _("Screen %d %s %.3f"), arg, ui_getstring(UI_brightness), render_container_get_brightness(container));
	}
	return floor(render_container_get_brightness(container) * 1000.0f + 0.5f);
}


/*-------------------------------------------------
    slider_contrast - screen contrast slider
    callback
-------------------------------------------------*/

static INT32 slider_contrast(INT32 newval, char *buffer, int arg)
{
	render_container *container = render_container_get_screen(arg);
	if (buffer != NULL)
	{
		render_container_set_contrast(container, (float)newval * 0.001f);
		sprintf(buffer, _("Screen %d %s %.3f"), arg, ui_getstring(UI_contrast), render_container_get_contrast(container));
	}
	return floor(render_container_get_contrast(container) * 1000.0f + 0.5f);
}


/*-------------------------------------------------
    slider_gamma - screen gamma slider callback
-------------------------------------------------*/

static INT32 slider_gamma(INT32 newval, char *buffer, int arg)
{
	render_container *container = render_container_get_screen(arg);
	if (buffer != NULL)
	{
		render_container_set_gamma(container, (float)newval * 0.001f);
		sprintf(buffer, _("Screen %d %s %.3f"), arg, ui_getstring(UI_gamma), render_container_get_gamma(container));
	}
	return floor(render_container_get_gamma(container) * 1000.0f + 0.5f);
}


/*-------------------------------------------------
    slider_xscale - screen horizontal scale slider
    callback
-------------------------------------------------*/

static INT32 slider_xscale(INT32 newval, char *buffer, int arg)
{
	render_container *container = render_container_get_screen(arg);
	if (buffer != NULL)
	{
		render_container_set_xscale(container, (float)newval * 0.001f);
		sprintf(buffer, _("Screen %d %s %.3f"), arg, _("Horiz stretch"), render_container_get_xscale(container));
	}
	return floor(render_container_get_xscale(container) * 1000.0f + 0.5f);
}


/*-------------------------------------------------
    slider_yscale - screen vertical scale slider
    callback
-------------------------------------------------*/

static INT32 slider_yscale(INT32 newval, char *buffer, int arg)
{
	render_container *container = render_container_get_screen(arg);
	if (buffer != NULL)
	{
		render_container_set_yscale(container, (float)newval * 0.001f);
		sprintf(buffer, _("Screen %d %s %.3f"), arg, _("Vert stretch"), render_container_get_yscale(container));
	}
	return floor(render_container_get_yscale(container) * 1000.0f + 0.5f);
}


/*-------------------------------------------------
    slider_xoffset - screen horizontal position
    slider callback
-------------------------------------------------*/

static INT32 slider_xoffset(INT32 newval, char *buffer, int arg)
{
	render_container *container = render_container_get_screen(arg);
	if (buffer != NULL)
	{
		render_container_set_xoffset(container, (float)newval * 0.001f);
		sprintf(buffer, _("Screen %d %s %.3f"), arg, _("Horiz position"), render_container_get_xoffset(container));
	}
	return floor(render_container_get_xoffset(container) * 1000.0f + 0.5f);
}


/*-------------------------------------------------
    slider_yoffset - screen vertical position
    slider callback
-------------------------------------------------*/

static INT32 slider_yoffset(INT32 newval, char *buffer, int arg)
{
	render_container *container = render_container_get_screen(arg);
	if (buffer != NULL)
	{
		render_container_set_yoffset(container, (float)newval * 0.001f);
		sprintf(buffer, _("Screen %d %s %.3f"), arg, _("Vert position"), render_container_get_yoffset(container));
	}
	return floor(render_container_get_yoffset(container) * 1000.0f + 0.5f);
}


/*-------------------------------------------------
    slider_flicker - vector flicker slider
    callback
-------------------------------------------------*/

static INT32 slider_flicker(INT32 newval, char *buffer, int arg)
{
	if (buffer != NULL)
	{
		vector_set_flicker((float)newval * 0.1f);
		sprintf(buffer, "%s %1.2f", ui_getstring(UI_vectorflicker), vector_get_flicker());
	}
	return floor(vector_get_flicker() * 10.0f + 0.5f);
}


/*-------------------------------------------------
    slider_beam - vector beam width slider
    callback
-------------------------------------------------*/

static INT32 slider_beam(INT32 newval, char *buffer, int arg)
{
	if (buffer != NULL)
	{
		vector_set_beam((float)newval * 0.01f);
		sprintf(buffer, "%s %1.2f", _("Beam Width"), vector_get_beam());
	}
	return floor(vector_get_beam() * 100.0f + 0.5f);
}


/*-------------------------------------------------
    slider_crossscale - crosshair scale slider
    callback
-------------------------------------------------*/

#ifdef MAME_DEBUG
static INT32 slider_crossscale(INT32 newval, char *buffer, int arg)
{
	input_port_entry *in;

	if (buffer != NULL)
	{
		for (in = Machine->input_ports; in && in->type != IPT_END; in++)
			if (in->analog.crossaxis == arg)
			{
				in->analog.crossscale = (float)newval * 0.001f;
				break;
			}
		sprintf(buffer, "%s %s %1.3f", "Crosshair Scale", (in->analog.crossaxis == CROSSHAIR_AXIS_X) ? "X" : "Y", (float)newval * 0.001f);
	}
	for (in = Machine->input_ports; in && in->type != IPT_END; in++)
		if (in->analog.crossaxis == arg)
			return floor(in->analog.crossscale * 1000.0f + 0.5f);
	return 0;
}
#endif


/*-------------------------------------------------
    slider_crossoffset - crosshair scale slider
    callback
-------------------------------------------------*/

#ifdef MAME_DEBUG
static INT32 slider_crossoffset(INT32 newval, char *buffer, int arg)
{
	input_port_entry *in;

	if (buffer != NULL)
	{
		for (in = Machine->input_ports; in && in->type != IPT_END; in++)
			if (in->analog.crossaxis == arg)
			{
				in->analog.crossoffset = (float)newval * 0.001f;
				break;
			}
		sprintf(buffer, "%s %s %1.3f", "Crosshair Offset", (in->analog.crossaxis == CROSSHAIR_AXIS_X) ? "X" : "Y", (float)newval * 0.001f);
	}
	for (in = Machine->input_ports; in && in->type != IPT_END; in++)
		if (in->analog.crossaxis == arg)
			return floor(in->analog.crossoffset * 1000.0f + 0.5f);
	return 0;
}
#endif

void ui_auto_pause(void)
{
	auto_pause = 1;
}

static void build_bgtexture(running_machine *machine)
{
#ifdef UI_COLOR_DISPLAY
	float r = (float)RGB_RED(uifont_colortable[UI_FILLCOLOR]);
	float g = (float)RGB_GREEN(uifont_colortable[UI_FILLCOLOR]);
	float b = (float)RGB_BLUE(uifont_colortable[UI_FILLCOLOR]);
#else /* UI_COLOR_DISPLAY */
	UINT8 r = 0x10;
	UINT8 g = 0x10;
	UINT8 b = 0x30;
#endif /* UI_COLOR_DISPLAY */
	UINT8 a = 0xff;
	int i;

#ifdef TRANS_UI
	if (options_get_bool(mame_options(), OPTION_USE_TRANS_UI))
		a = ui_transparency;
#endif /* TRANS_UI */

	bgbitmap = bitmap_alloc(1, 1024, BITMAP_FORMAT_RGB32);
	if (!bgbitmap)
		fatalerror("build_bgtexture failed");

	for (i = 0; i < bgbitmap->height; i++)
	{
		double gradual = (float)(1024 - i) / 1024.0f + 0.1f;

		if (gradual > 1.0f)
			gradual = 1.0f;
		else if (gradual < 0.2f)
			gradual = 0.2f;

		*BITMAP_ADDR32(bgbitmap, i, 0) = MAKE_ARGB(a, (UINT8)(r * gradual), (UINT8)(g * gradual), (UINT8)(b * gradual));
	}

	bgtexture = render_texture_alloc(render_texture_hq_scale, NULL);
	render_texture_set_bitmap(bgtexture, bgbitmap, NULL, 0, TEXFORMAT_ARGB32);
	add_exit_callback(machine, free_bgtexture);
}

static void free_bgtexture(running_machine *machine)
{
	bitmap_free(bgbitmap);
	bgbitmap = NULL;
	render_texture_free(bgtexture);
	bgtexture = NULL;
}

#ifdef MAME_AVI
int get_single_step(void) { return single_step; }

int usrintrf_message_ok_cancel(void *str)
{
	int ret = FALSE;
	mame_pause(Machine, TRUE);

	while (1)
	{
		ui_draw_message_window(str);

		//update_video_and_audio();

		if (input_ui_pressed(IPT_UI_CANCEL))
			break;

		if (input_ui_pressed(IPT_UI_SELECT))
		{
			ret = TRUE;
			break;
		}
	}

	mame_pause(Machine, FALSE);

	return ret;
}
#endif /* MAME_AVI */
