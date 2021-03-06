/***************************************************************************

    inptport.c

    Input port handling.

    Copyright (c) 1996-2007, Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

****************************************************************************

    Theory of operation

    ------------
    OSD controls
    ------------

    There are three types of controls that the OSD can provide as potential
    input devices: digital controls, absolute analog controls, and relative
    analog controls.

    Digital controls have only two states: on or off. They are generally
    mapped to buttons and digital joystick directions (like a gamepad or a
    joystick hat). The OSD layer must return either 0 (off) or 1 (on) for
    these types of controls.

    Absolute analog controls are analog in the sense that they return a
    range of values depending on how much a given control is moved, but they
    are physically bounded. This means that there is a minimum and maximum
    limit to how far the control can be moved. They are generally mapped to
    analog joystick axes, lightguns, most PC steering wheels, and pedals.
    The OSD layer must determine the minimum and maximum range of each
    analog device and scale that to a value between -65536 and +65536
    representing the position of the control. -65536 generally refers to
    the topmost or leftmost position, while +65536 refers to the bottommost
    or rightmost position. Note that pedals are a special case here, the
    OSD layer needs to return half axis as full -65536 to + 65536 range.

    Relative analog controls are analog as well, but are not physically
    bounded. They can be moved continually in one direction without limit.
    They are generally mapped to trackballs and mice. Because they are
    unbounded, the OSD layer can only return delta values since the last
    read. Because of this, it is difficult to scale appropriately. For
    MAME's purposes, when mapping a mouse devices to a relative analog
    control, one pixel of movement should correspond to 512 units. Other
    analog control types should be scaled to return values of a similar
    magnitude. Like absolute analog controls, negative values refer to
    upward or leftward movement, while positive values refer to downward
    or rightward movement.

    -------------
    Game controls
    -------------

    Similarly, the types of controls used by arcade games fall into the same
    three categories: digital, absolute analog, and relative analog. The
    tricky part is how to map any arbitrary type of OSD control to an
    arbitrary type of game control.

    Digital controls: used for game buttons and standard 4/8-way joysticks,
    as well as many other types of game controls. Mapping an OSD digital
    control to a game's OSD control is trivial. For OSD analog controls,
    the MAME core does not directly support mapping any OSD analog devices
    to digital controls. However, the OSD layer is free to enumerate digital
    equivalents for analog devices. For example, each analog axis in the
    Windows OSD code enumerates to two digital controls, one for the
    negative direction (up/left) and one for the position direction
    (down/right). When these "digital" inputs are queried, the OSD layer
    checks the axis position against the center, adding in a dead zone,
    and returns 0 or 1 to indicate its position.

    Absolute analog controls: used for analog joysticks, lightguns, pedals,
    and wheel controls. Mapping an OSD absolute analog control to this type
    is easy. OSD relative analog controls can be mapped here as well by
    accumulating the deltas and bounding the results. OSD digital controls
    are mapped to these types of controls in pairs, one for a decrement and
    one for an increment, but apart from that, operate the same as the OSD
    relative analog controls by accumulating deltas and applying bounds.
    The speed of the digital delta is user-configurable per analog input.
    In addition, most absolute analog control types have an autocentering
    feature that is activated when using the digital increment/decrement
    sequences, which returns the control back to the center at a user-
    controllable speed if no digital sequences are pressed.

    Relative analog controls: used for trackballs and dial controls. Again,
    mapping an OSD relative analog control to this type is straightforward.
    OSD absolute analog controls can't map directly to these, but if the OSD
    layer provides a digital equivalent for each direction, it can be done.
    OSD digital controls map just like they do for absolute analog controls,
    except that the accumulated deltas are not bounded, but rather wrap.

***************************************************************************/

#include "osdepend.h"
#include "driver.h"
#include "config.h"
#include "xmlfile.h"
#include "profiler.h"
#include "inputseq.h"
#include "ui.h"
#include <math.h>
#include <ctype.h>
#include <time.h>

#include "rendfont.h"

#ifdef MESS
#include "inputx.h"
#endif

#ifdef USE_NEOGEO_HACKS
#include "neogeo.h"
#endif /* USE_NEOGEO_HACKS */

#ifdef KAILLERA
#include "kailleraclient.h"
#include "KailleraChat.h"
#include "ui_temp.h" /*//kt */
#include "extmem.h"
#include "zlib.h"
int kMaxDipSwitch;							//kt
UINT32 kDipSwitchValues[MAX_INPUT_PORTS][2];	//kt
int kPlayerDup;			//kt
static int kMaxMahjong;							//kt
static UINT32 kMahjongPlayValues[4][64-1][3];		//kt
static UINT32 kAnalogPlayValues[4][MAX_INPUT_PORTS];	//kt
int kBitsPlayValues[4][16][3*2];
int kFirstMsg;
static unsigned short kValXor;	//kt
UINT32 kDefValues[MAX_INPUT_PORTS];		//kt
extern int kPlay;
extern void addClock(osd_ticks_t a);
extern osd_ticks_t	KailleraWaitSubTick;
extern osd_ticks_t	KailleraWaitTick;
extern void ResetReplay(void);
extern BOOL GetShowSystemMessage(void);
#endif /* KAILLERA */

#define PK_DEBUG 0


/***************************************************************************
    CONSTANTS
***************************************************************************/

#define DIGITAL_JOYSTICKS_PER_PLAYER	3

/* these constants must match the order of the joystick directions in the IPT definition */
#define JOYDIR_UP			0
#define JOYDIR_DOWN			1
#define JOYDIR_LEFT			2
#define JOYDIR_RIGHT		3

#define JOYDIR_UP_BIT		(1 << JOYDIR_UP)
#define JOYDIR_DOWN_BIT		(1 << JOYDIR_DOWN)
#define JOYDIR_LEFT_BIT		(1 << JOYDIR_LEFT)
#define JOYDIR_RIGHT_BIT	(1 << JOYDIR_RIGHT)



/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

typedef struct _analog_port_info analog_port_info;
struct _analog_port_info
{
	analog_port_info *	next;			/* linked list */
	input_port_entry *	port;			/* pointer to the input port referenced */
	double				accum;			/* accumulated value (including relative adjustments) */
	double				previous;		/* previous adjusted value */
	INT32				previousanalog;	/* previous analog value */
	INT32				minimum;		/* minimum adjusted value */
	INT32				maximum;		/* maximum adjusted value */
	INT32				center;			/* center adjusted value for autocentering */
	INT32				reverse_val;	/* value where we subtract from to reverse directions */
	double				scalepos;		/* scale factor to apply to positive adjusted values */
	double				scaleneg;		/* scale factor to apply to negative adjusted values */
	double				keyscalepos;	/* scale factor to apply to the key delta field when pos */
	double				keyscaleneg;	/* scale factor to apply to the key delta field when neg */
	double				positionalscale;/* scale factor to divide a joystick into positions */
	UINT8				shift;			/* left shift to apply to the final result */
	UINT8				bits;			/* how many bits of resolution are expected? */
	UINT8				absolute;		/* is this an absolute or relative input? */
	UINT8				wraps;			/* does the control wrap around? */
	UINT8				one_of_x;		/* is this a 1 of X positional input? */
	UINT8				autocenter;		/* autocenter this input? */
	UINT8				single_scale;	/* scale joystick differently if default is between min/max */
	UINT8				interpolate;	/* should we do linear interpolation for mid-frame reads? */
	UINT8				lastdigital;	/* was the last modification caused by a digital form? */
	UINT32				crosshair_pos;	/* position of fake crosshair */
};


typedef struct _custom_port_info custom_port_info;
struct _custom_port_info
{
	custom_port_info *	next;		/* linked list */
	input_port_entry *	port;		/* pointer to the input port referenced */
	UINT8				shift;		/* left shift to apply to the final result */
};


typedef struct _input_bit_info input_bit_info;
struct _input_bit_info
{
	input_port_entry *	port;		/* port for this input */
	UINT8				impulse;	/* counter for impulse controls */
	UINT8				last;		/* were we pressed last time? */
	int				autopressed;	/* autofire status */
};


typedef struct _changed_callback_info changed_callback_info;
struct _changed_callback_info
{
	changed_callback_info *next;	/* linked list */
	UINT32				mask;		/* mask we care about */
	void				(*callback)(void *, UINT32, UINT32); /* callback */
	void *				param;		/* parameter */
};


typedef struct _input_port_info input_port_info;
struct _input_port_info
{
	const char *		tag;		/* tag for this port */
	UINT32				defvalue;	/* default value of all the bits */
	UINT32				analog;		/* value from analog inputs */
	UINT32				analogmask;	/* mask of all analog inputs */
	UINT32				digital;	/* value from digital inputs */
	UINT32				vblank;		/* value of all IPT_VBLANK bits */
	UINT32				playback;	/* current playback override */
	UINT8				has_custom;	/* do we have any custom ports? */
	input_bit_info 		bit[MAX_BITS_PER_PORT]; /* info about each bit in the port */
	analog_port_info *	analoginfo;	/* pointer to linked list of analog port info */
	custom_port_info *	custominfo;	/* pointer to linked list of custom port info */
	changed_callback_info *change_notify;/* list of people to notify if things change */
	UINT32				changed_last_value;
};


typedef struct _digital_joystick_info digital_joystick_info;
struct _digital_joystick_info
{
	input_port_entry *	port[4];	/* port for up,down,left,right respectively */
	UINT8				inuse;		/* is this joystick used? */
	UINT8				current;	/* current value */
	UINT8				current4way;/* current 4-way value */
	UINT8				previous;	/* previous value */
};


struct _input_port_init_params
{
	input_port_entry *	ports;		/* base of the port array */
	int					max_ports;	/* maximum number of ports we can support */
	int					current_port;/* current port index */
};

/* Support for eXtended INP format */
struct ext_header
{
	char header[7];  // must be "XINP" followed by NULLs
	char shortname[9];  // game shortname
	char version[32];  // MAME version string
	long starttime;  // approximate INP start time
	char dummy[32];  // for possible future expansion
};


/***************************************************************************
    MACROS
***************************************************************************/

#define IS_ANALOG(in)				((in)->type >= __ipt_analog_start && (in)->type <= __ipt_analog_end)
#define IS_DIGITAL_JOYSTICK(in)		((in)->type >= __ipt_digital_joystick_start && (in)->type <= __ipt_digital_joystick_end)
#define JOYSTICK_INFO_FOR_PORT(in)	(&joystick_info[(in)->player][((in)->type - __ipt_digital_joystick_start) / 4])
#define JOYSTICK_DIR_FOR_PORT(in)	(((in)->type - __ipt_digital_joystick_start) % 4)

#define APPLY_SENSITIVITY(x,s)		(((double)(x) * (s)) / 100)
#define APPLY_INVERSE_SENSITIVITY(x,s) (((double)(x) * 100) / (s))

#define IS_AUTOKEY(port)		((port->autofire_setting & AUTOFIRE_ON) || ((port->autofire_setting & AUTOFIRE_TOGGLE) && (autofire_toggle_port->defvalue & (1 << port->player))))

/* original input_ports without modifications */
input_port_entry *input_ports_default;


/***************************************************************************
    GLOBAL VARIABLES
***************************************************************************/

/* current value of all the ports */
static input_port_info port_info[MAX_INPUT_PORTS];

/* additiona tracking information for special types of controls */
static digital_joystick_info joystick_info[MAX_PLAYERS][DIGITAL_JOYSTICKS_PER_PLAYER];

/* memory for UI keys */
static UINT8 ui_memory[__ipt_max];

/* XML attributes for the different types */
static const char *seqtypestrings[] = { "standard", "decrement", "increment" };

static int autofiredelay[MAX_PLAYERS];
static input_port_info *autofire_toggle_port;
static int normal_buttons;
static int players;


/*************************************
 *
 *	Custom Button related
 *
 *************************************/

#ifdef USE_CUSTOM_BUTTON
UINT16 custom_button[MAX_PLAYERS][MAX_CUSTOM_BUTTONS];
int custom_buttons;

static input_bit_info *custom_button_info[MAX_PLAYERS][MAX_CUSTOM_BUTTONS];
#endif /* USE_CUSTOM_BUTTON */


/**********************************************
 *
 *	Show Input Log related
 *
 **********************************************/

#ifdef USE_SHOW_INPUT_LOG
#define COMMAND_LOG_BUFSIZE	128

input_log command_buffer[COMMAND_LOG_BUFSIZE];
int show_input_log;

static void make_input_log(void);
#endif /* USE_SHOW_INPUT_LOG */

/* recorded speed read from an INP file */
double rec_speed;

/* set to 1 if INP file being played is a standard MAME INP file */
int no_extended_inp;

/* for average speed calculations */
int framecount;
double totalspeed;


/***************************************************************************
    PORT HANDLER TABLES
***************************************************************************/

static const read8_handler port_handler8[] =
{
	input_port_0_r,			input_port_1_r,			input_port_2_r,			input_port_3_r,
	input_port_4_r,			input_port_5_r,			input_port_6_r,			input_port_7_r,
	input_port_8_r,			input_port_9_r,			input_port_10_r,		input_port_11_r,
	input_port_12_r,		input_port_13_r,		input_port_14_r,		input_port_15_r,
	input_port_16_r,		input_port_17_r,		input_port_18_r,		input_port_19_r,
	input_port_20_r,		input_port_21_r,		input_port_22_r,		input_port_23_r,
	input_port_24_r,		input_port_25_r,		input_port_26_r,		input_port_27_r,
	input_port_28_r,		input_port_29_r,		input_port_30_r,		input_port_31_r
};


static const read16_handler port_handler16[] =
{
	input_port_0_word_r,	input_port_1_word_r,	input_port_2_word_r,	input_port_3_word_r,
	input_port_4_word_r,	input_port_5_word_r,	input_port_6_word_r,	input_port_7_word_r,
	input_port_8_word_r,	input_port_9_word_r,	input_port_10_word_r,	input_port_11_word_r,
	input_port_12_word_r,	input_port_13_word_r,	input_port_14_word_r,	input_port_15_word_r,
	input_port_16_word_r,	input_port_17_word_r,	input_port_18_word_r,	input_port_19_word_r,
	input_port_20_word_r,	input_port_21_word_r,	input_port_22_word_r,	input_port_23_word_r,
	input_port_24_word_r,	input_port_25_word_r,	input_port_26_word_r,	input_port_27_word_r,
	input_port_28_word_r,	input_port_29_word_r,	input_port_30_word_r,	input_port_31_word_r
};


static const read32_handler port_handler32[] =
{
	input_port_0_dword_r,	input_port_1_dword_r,	input_port_2_dword_r,	input_port_3_dword_r,
	input_port_4_dword_r,	input_port_5_dword_r,	input_port_6_dword_r,	input_port_7_dword_r,
	input_port_8_dword_r,	input_port_9_dword_r,	input_port_10_dword_r,	input_port_11_dword_r,
	input_port_12_dword_r,	input_port_13_dword_r,	input_port_14_dword_r,	input_port_15_dword_r,
	input_port_16_dword_r,	input_port_17_dword_r,	input_port_18_dword_r,	input_port_19_dword_r,
	input_port_20_dword_r,	input_port_21_dword_r,	input_port_22_dword_r,	input_port_23_dword_r,
	input_port_24_dword_r,	input_port_25_dword_r,	input_port_26_dword_r,	input_port_27_dword_r,
	input_port_28_dword_r,	input_port_29_dword_r,	input_port_30_dword_r,	input_port_31_dword_r
};



/***************************************************************************
    COMMON SHARED STRINGS
***************************************************************************/

static const struct
{
	UINT32 id;
	const char *string;
} input_port_default_strings[] =
{
	{ INPUT_STRING_Off, "Off" },
	{ INPUT_STRING_On, "On" },
	{ INPUT_STRING_No, "No" },
	{ INPUT_STRING_Yes, "Yes" },
	{ INPUT_STRING_Lives, "Lives" },
	{ INPUT_STRING_Bonus_Life, "Bonus Life" },
	{ INPUT_STRING_Difficulty, "Difficulty" },
	{ INPUT_STRING_Demo_Sounds, "Demo Sounds" },
	{ INPUT_STRING_Coinage, "Coinage" },
	{ INPUT_STRING_Coin_A, "Coin A" },
	{ INPUT_STRING_Coin_B, "Coin B" },
	{ INPUT_STRING_9C_1C, "9 Coins/1 Credit" },
	{ INPUT_STRING_8C_1C, "8 Coins/1 Credit" },
	{ INPUT_STRING_7C_1C, "7 Coins/1 Credit" },
	{ INPUT_STRING_6C_1C, "6 Coins/1 Credit" },
	{ INPUT_STRING_5C_1C, "5 Coins/1 Credit" },
	{ INPUT_STRING_4C_1C, "4 Coins/1 Credit" },
	{ INPUT_STRING_3C_1C, "3 Coins/1 Credit" },
	{ INPUT_STRING_8C_3C, "8 Coins/3 Credits" },
	{ INPUT_STRING_4C_2C, "4 Coins/2 Credits" },
	{ INPUT_STRING_2C_1C, "2 Coins/1 Credit" },
	{ INPUT_STRING_5C_3C, "5 Coins/3 Credits" },
	{ INPUT_STRING_3C_2C, "3 Coins/2 Credits" },
	{ INPUT_STRING_4C_3C, "4 Coins/3 Credits" },
	{ INPUT_STRING_4C_4C, "4 Coins/4 Credits" },
	{ INPUT_STRING_3C_3C, "3 Coins/3 Credits" },
	{ INPUT_STRING_2C_2C, "2 Coins/2 Credits" },
	{ INPUT_STRING_1C_1C, "1 Coin/1 Credit" },
	{ INPUT_STRING_4C_5C, "4 Coins/5 Credits" },
	{ INPUT_STRING_3C_4C, "3 Coins/4 Credits" },
	{ INPUT_STRING_2C_3C, "2 Coins/3 Credits" },
	{ INPUT_STRING_4C_7C, "4 Coins/7 Credits" },
	{ INPUT_STRING_2C_4C, "2 Coins/4 Credits" },
	{ INPUT_STRING_1C_2C, "1 Coin/2 Credits" },
	{ INPUT_STRING_2C_5C, "2 Coins/5 Credits" },
	{ INPUT_STRING_2C_6C, "2 Coins/6 Credits" },
	{ INPUT_STRING_1C_3C, "1 Coin/3 Credits" },
	{ INPUT_STRING_2C_7C, "2 Coins/7 Credits" },
	{ INPUT_STRING_2C_8C, "2 Coins/8 Credits" },
	{ INPUT_STRING_1C_4C, "1 Coin/4 Credits" },
	{ INPUT_STRING_1C_5C, "1 Coin/5 Credits" },
	{ INPUT_STRING_1C_6C, "1 Coin/6 Credits" },
	{ INPUT_STRING_1C_7C, "1 Coin/7 Credits" },
	{ INPUT_STRING_1C_8C, "1 Coin/8 Credits" },
	{ INPUT_STRING_1C_9C, "1 Coin/9 Credits" },
	{ INPUT_STRING_Free_Play, "Free Play" },
	{ INPUT_STRING_Cabinet, "Cabinet" },
	{ INPUT_STRING_Upright, "Upright" },
	{ INPUT_STRING_Cocktail, "Cocktail" },
	{ INPUT_STRING_Flip_Screen, "Flip Screen" },
	{ INPUT_STRING_Service_Mode, "Service Mode" },
	{ INPUT_STRING_Pause, "Pause" },
	{ INPUT_STRING_Test, "Test" },
	{ INPUT_STRING_Tilt, "Tilt" },
	{ INPUT_STRING_Version, "Version" },
	{ INPUT_STRING_Region, "Region" },
	{ INPUT_STRING_International, "International" },
	{ INPUT_STRING_Japan, "Japan" },
	{ INPUT_STRING_USA, "USA" },
	{ INPUT_STRING_Europe, "Europe" },
	{ INPUT_STRING_Asia, "Asia" },
	{ INPUT_STRING_World, "World" },
	{ INPUT_STRING_Hispanic, "Hispanic" },
	{ INPUT_STRING_Language, "Language" },
	{ INPUT_STRING_English, "English" },
	{ INPUT_STRING_Japanese, "Japanese" },
	{ INPUT_STRING_German, "German" },
	{ INPUT_STRING_French, "French" },
	{ INPUT_STRING_Italian, "Italian" },
	{ INPUT_STRING_Spanish, "Spanish" },
	{ INPUT_STRING_Very_Easy, "Very Easy" },
	{ INPUT_STRING_Easiest, "Easiest" },
	{ INPUT_STRING_Easier, "Easier" },
	{ INPUT_STRING_Easy, "Easy" },
	{ INPUT_STRING_Normal, "Normal" },
	{ INPUT_STRING_Medium, "Medium" },
	{ INPUT_STRING_Hard, "Hard" },
	{ INPUT_STRING_Harder, "Harder" },
	{ INPUT_STRING_Hardest, "Hardest" },
	{ INPUT_STRING_Very_Hard, "Very Hard" },
	{ INPUT_STRING_Very_Low, "Very Low" },
	{ INPUT_STRING_Low, "Low" },
	{ INPUT_STRING_High, "High" },
	{ INPUT_STRING_Higher, "Higher" },
	{ INPUT_STRING_Highest, "Highest" },
	{ INPUT_STRING_Very_High, "Very High" },
	{ INPUT_STRING_Players, "Players" },
	{ INPUT_STRING_Controls, "Controls" },
	{ INPUT_STRING_Dual, "Dual" },
	{ INPUT_STRING_Single, "Single" },
	{ INPUT_STRING_Game_Time, "Game Time" },
	{ INPUT_STRING_Continue_Price, "Continue Price" },
	{ INPUT_STRING_Controller, "Controller" },
	{ INPUT_STRING_Light_Gun, "Light Gun" },
	{ INPUT_STRING_Joystick, "Joystick" },
	{ INPUT_STRING_Trackball, "Trackball" },
	{ INPUT_STRING_Continues, "Continues" },
	{ INPUT_STRING_Allow_Continue, "Allow Continue" },
	{ INPUT_STRING_Level_Select, "Level Select" },
	{ INPUT_STRING_Infinite, "Infinite" },
	{ INPUT_STRING_Stereo, "Stereo" },
	{ INPUT_STRING_Mono, "Mono" },
	{ INPUT_STRING_Unused, "Unused" },
	{ INPUT_STRING_Unknown, "Unknown" },
	{ INPUT_STRING_Standard, "Standard" },
	{ INPUT_STRING_Reverse, "Reverse" },
	{ INPUT_STRING_Alternate, "Alternate" },
	{ INPUT_STRING_None, "None" }
};



/***************************************************************************
    DEFAULT INPUT PORTS
***************************************************************************/

#define INPUT_PORT_DIGITAL_DEF(player_,group_,type_,name_,seq_) \
	{ IPT_##type_, group_, (player_ == 0) ? player_ : (player_) - 1, (player_ == 0) ? #type_ : ("P" #player_ "_" #type_), name_, seq_, SEQ_DEF_0, SEQ_DEF_0 },

#define INPUT_PORT_ANALOG_DEF(player_,group_,type_,name_,seq_,decseq_,incseq_) \
	{ IPT_##type_, group_, (player_ == 0) ? player_ : (player_) - 1, (player_ == 0) ? #type_ : ("P" #player_ "_" #type_), name_, seq_, incseq_, decseq_ },

#define INDEXED(a,b)	INPUT_CODE_SET_DEVINDEX(a,b)

static const input_port_default_entry default_ports_builtin[] =
{
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICK_UP,		"P1 Up",				SEQ_DEF_3(KEYCODE_UP, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICK_DOWN,      "P1 Down",				SEQ_DEF_3(KEYCODE_DOWN, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICK_LEFT,      "P1 Left",    			SEQ_DEF_3(KEYCODE_LEFT, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICK_RIGHT,     "P1 Right",   			SEQ_DEF_3(KEYCODE_RIGHT, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICKRIGHT_UP,   "P1 Right/Up",			SEQ_DEF_3(KEYCODE_I, SEQCODE_OR, INDEXED(JOYCODE_BUTTON2, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICKRIGHT_DOWN, "P1 Right/Down",		SEQ_DEF_3(KEYCODE_K, SEQCODE_OR, INDEXED(JOYCODE_BUTTON3, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICKRIGHT_LEFT, "P1 Right/Left",		SEQ_DEF_3(KEYCODE_J, SEQCODE_OR, INDEXED(JOYCODE_BUTTON1, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICKRIGHT_RIGHT,"P1 Right/Right",		SEQ_DEF_3(KEYCODE_L, SEQCODE_OR, INDEXED(JOYCODE_BUTTON4, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICKLEFT_UP,    "P1 Left/Up", 			SEQ_DEF_3(KEYCODE_E, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICKLEFT_DOWN,  "P1 Left/Down",			SEQ_DEF_3(KEYCODE_D, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICKLEFT_LEFT,  "P1 Left/Left", 		SEQ_DEF_3(KEYCODE_S, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	JOYSTICKLEFT_RIGHT, "P1 Left/Right",		SEQ_DEF_3(KEYCODE_F, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON1,			"P1 Button 1",			SEQ_DEF_7(KEYCODE_LCONTROL, SEQCODE_OR, INDEXED(JOYCODE_BUTTON1, 0), SEQCODE_OR, INDEXED(MOUSECODE_BUTTON1, 0), SEQCODE_OR, INDEXED(GUNCODE_BUTTON1, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON2,			"P1 Button 2",			SEQ_DEF_7(KEYCODE_LALT, SEQCODE_OR, INDEXED(JOYCODE_BUTTON2, 0), SEQCODE_OR, INDEXED(MOUSECODE_BUTTON3, 0), SEQCODE_OR, INDEXED(GUNCODE_BUTTON2, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON3,			"P1 Button 3",			SEQ_DEF_5(KEYCODE_SPACE, SEQCODE_OR, INDEXED(JOYCODE_BUTTON3, 0), SEQCODE_OR, INDEXED(MOUSECODE_BUTTON2, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON4,			"P1 Button 4",			SEQ_DEF_3(KEYCODE_LSHIFT, SEQCODE_OR, INDEXED(JOYCODE_BUTTON4, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON5,			"P1 Button 5",			SEQ_DEF_3(KEYCODE_Z, SEQCODE_OR, INDEXED(JOYCODE_BUTTON5, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON6,			"P1 Button 6",			SEQ_DEF_3(KEYCODE_X, SEQCODE_OR, INDEXED(JOYCODE_BUTTON6, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON7,			"P1 Button 7",			SEQ_DEF_3(KEYCODE_C, SEQCODE_OR, INDEXED(JOYCODE_BUTTON7, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON8,			"P1 Button 8",			SEQ_DEF_3(KEYCODE_V, SEQCODE_OR, INDEXED(JOYCODE_BUTTON8, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON9,			"P1 Button 9",			SEQ_DEF_3(KEYCODE_B, SEQCODE_OR, INDEXED(JOYCODE_BUTTON9, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON10,			"P1 Button 10",			SEQ_DEF_3(KEYCODE_N, SEQCODE_OR, INDEXED(JOYCODE_BUTTON10, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON11,			"P1 Button 11",			SEQ_DEF_3(KEYCODE_M, SEQCODE_OR, INDEXED(JOYCODE_BUTTON11, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON12,			"P1 Button 12",			SEQ_DEF_3(KEYCODE_COMMA, SEQCODE_OR, INDEXED(JOYCODE_BUTTON12, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON13,			"P1 Button 13",			SEQ_DEF_3(KEYCODE_STOP, SEQCODE_OR, INDEXED(JOYCODE_BUTTON13, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON14,			"P1 Button 14",			SEQ_DEF_3(KEYCODE_SLASH, SEQCODE_OR, INDEXED(JOYCODE_BUTTON14, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON15,			"P1 Button 15",			SEQ_DEF_3(KEYCODE_RSHIFT, SEQCODE_OR, INDEXED(JOYCODE_BUTTON15, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	BUTTON16,			"P1 Button 16",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON16, 0)) )
#ifdef USE_CUSTOM_BUTTON
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	CUSTOM1,			"P1 Custom 1",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	CUSTOM2,			"P1 Custom 2",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	CUSTOM3,			"P1 Custom 3",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	CUSTOM4,			"P1 Custom 4",			SEQ_DEF_0 )
#endif /* USE_CUSTOM_BUTTON */
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1, START,				"P1 Start",				SEQ_DEF_3(KEYCODE_1, SEQCODE_OR, INDEXED(JOYCODE_START, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1, SELECT,				"P1 Select",			SEQ_DEF_3(KEYCODE_5, SEQCODE_OR, INDEXED(JOYCODE_SELECT, 0)) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_A,          "P1 Mahjong A",			SEQ_DEF_1(KEYCODE_A) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_B,          "P1 Mahjong B",			SEQ_DEF_1(KEYCODE_B) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_C,          "P1 Mahjong C",			SEQ_DEF_1(KEYCODE_C) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_D,          "P1 Mahjong D",			SEQ_DEF_1(KEYCODE_D) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_E,          "P1 Mahjong E",			SEQ_DEF_1(KEYCODE_E) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_F,          "P1 Mahjong F",			SEQ_DEF_1(KEYCODE_F) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_G,          "P1 Mahjong G",			SEQ_DEF_1(KEYCODE_G) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_H,          "P1 Mahjong H",			SEQ_DEF_1(KEYCODE_H) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_I,          "P1 Mahjong I",			SEQ_DEF_1(KEYCODE_I) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_J,          "P1 Mahjong J",			SEQ_DEF_1(KEYCODE_J) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_K,          "P1 Mahjong K",			SEQ_DEF_1(KEYCODE_K) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_L,          "P1 Mahjong L",			SEQ_DEF_1(KEYCODE_L) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_M,          "P1 Mahjong M",			SEQ_DEF_1(KEYCODE_M) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_N,          "P1 Mahjong N",			SEQ_DEF_1(KEYCODE_N) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_O,          "P1 Mahjong O",			SEQ_DEF_1(KEYCODE_O) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_P,          "P1 Mahjong P",			SEQ_DEF_1(KEYCODE_COLON) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_Q,          "P1 Mahjong Q",			SEQ_DEF_1(KEYCODE_Q) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_KAN,        "P1 Mahjong Kan",		SEQ_DEF_1(KEYCODE_LCONTROL) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_PON,        "P1 Mahjong Pon",		SEQ_DEF_1(KEYCODE_LALT) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_CHI,        "P1 Mahjong Chi",		SEQ_DEF_1(KEYCODE_SPACE) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_REACH,      "P1 Mahjong Reach",		SEQ_DEF_1(KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_RON,        "P1 Mahjong Ron",		SEQ_DEF_1(KEYCODE_Z) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_BET,        "P1 Mahjong Bet",		SEQ_DEF_1(KEYCODE_2) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_LAST_CHANCE,"P1 Mahjong Last Chance",SEQ_DEF_1(KEYCODE_RALT) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_SCORE,      "P1 Mahjong Score",		SEQ_DEF_1(KEYCODE_RCONTROL) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_DOUBLE_UP,  "P1 Mahjong Double Up",	SEQ_DEF_1(KEYCODE_RSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_FLIP_FLOP,  "P1 Mahjong Flip Flop",	SEQ_DEF_1(KEYCODE_Y) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_BIG,        "P1 Mahjong Big",       SEQ_DEF_1(KEYCODE_ENTER) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	MAHJONG_SMALL,      "P1 Mahjong Small",     SEQ_DEF_1(KEYCODE_BACKSPACE) )
	INPUT_PORT_DIGITAL_DEF( 1, IPG_PLAYER1,	TOGGLE_AUTOFIRE,		"P1 Toggle Autofire",	SEQ_DEF_0 )

	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICK_UP,		"P2 Up",				SEQ_DEF_3(KEYCODE_R, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICK_DOWN,      "P2 Down",    			SEQ_DEF_3(KEYCODE_F, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICK_LEFT,      "P2 Left",    			SEQ_DEF_3(KEYCODE_D, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICK_RIGHT,     "P2 Right",   			SEQ_DEF_3(KEYCODE_G, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICKRIGHT_UP,   "P2 Right/Up",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICKRIGHT_DOWN, "P2 Right/Down",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICKRIGHT_LEFT, "P2 Right/Left",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICKRIGHT_RIGHT,"P2 Right/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICKLEFT_UP,    "P2 Left/Up", 			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICKLEFT_DOWN,  "P2 Left/Down",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICKLEFT_LEFT,  "P2 Left/Left",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	JOYSTICKLEFT_RIGHT, "P2 Left/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON1,			"P2 Button 1",			SEQ_DEF_7(KEYCODE_A, SEQCODE_OR, INDEXED(JOYCODE_BUTTON1, 1), SEQCODE_OR, INDEXED(MOUSECODE_BUTTON1, 1), SEQCODE_OR, INDEXED(GUNCODE_BUTTON1, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON2,			"P2 Button 2",			SEQ_DEF_7(KEYCODE_S, SEQCODE_OR, INDEXED(JOYCODE_BUTTON2, 1), SEQCODE_OR, INDEXED(MOUSECODE_BUTTON3, 1), SEQCODE_OR, INDEXED(GUNCODE_BUTTON2, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON3,			"P2 Button 3",			SEQ_DEF_5(KEYCODE_Q, SEQCODE_OR, INDEXED(JOYCODE_BUTTON3, 1), SEQCODE_OR, INDEXED(MOUSECODE_BUTTON2, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON4,			"P2 Button 4",			SEQ_DEF_3(KEYCODE_W, SEQCODE_OR, INDEXED(JOYCODE_BUTTON4, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON5,			"P2 Button 5",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON5, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON6,			"P2 Button 6",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON6, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON7,			"P2 Button 7",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON7, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON8,			"P2 Button 8",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON8, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON9,			"P2 Button 9",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON9, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON10,			"P2 Button 10",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON10, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON11,			"P2 Button 11",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON11, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON12,			"P2 Button 12",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON12, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON13,			"P2 Button 13",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON13, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON14,			"P2 Button 14",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON14, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON15,			"P2 Button 15",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON15, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	BUTTON16,			"P2 Button 16",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON16, 1)) )
#ifdef USE_CUSTOM_BUTTON
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	CUSTOM1,			"P2 Custom 1",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	CUSTOM2,			"P2 Custom 2",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	CUSTOM3,			"P2 Custom 3",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	CUSTOM4,			"P2 Custom 4",			SEQ_DEF_0 )
#endif /* USE_CUSTOM_BUTTON */
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2, START,				"P2 Start",				SEQ_DEF_3(KEYCODE_2, SEQCODE_OR, INDEXED(JOYCODE_START, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2, SELECT,				"P2 Select",			SEQ_DEF_3(KEYCODE_6, SEQCODE_OR, INDEXED(JOYCODE_SELECT, 1)) )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_A,          "P2 Mahjong A",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_B,          "P2 Mahjong B",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_C,          "P2 Mahjong C",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_D,          "P2 Mahjong D",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_E,          "P2 Mahjong E",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_F,          "P2 Mahjong F",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_G,          "P2 Mahjong G",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_H,          "P2 Mahjong H",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_I,          "P2 Mahjong I",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_J,          "P2 Mahjong J",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_K,          "P2 Mahjong K",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_L,          "P2 Mahjong L",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_M,          "P2 Mahjong M",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_N,          "P2 Mahjong N",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_O,          "P2 Mahjong O",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_P,          "P2 Mahjong P",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_Q,          "P2 Mahjong Q",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_KAN,        "P2 Mahjong Kan",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_PON,        "P2 Mahjong Pon",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_CHI,        "P2 Mahjong Chi",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_REACH,      "P2 Mahjong Reach",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_RON,        "P2 Mahjong Ron",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_BET,        "P2 Mahjong Bet",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_LAST_CHANCE,"P2 Mahjong Last Chance",SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_SCORE,      "P2 Mahjong Score",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_DOUBLE_UP,  "P2 Mahjong Double Up",	SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_FLIP_FLOP,  "P2 Mahjong Flip Flop",	SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_BIG,        "P2 Mahjong Big",       SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	MAHJONG_SMALL,      "P2 Mahjong Small",     SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 2, IPG_PLAYER2,	TOGGLE_AUTOFIRE,		"P2 Toggle Autofire",	SEQ_DEF_0 )

	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICK_UP,		"P3 Up",				SEQ_DEF_3(KEYCODE_I, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICK_DOWN,      "P3 Down",    			SEQ_DEF_3(KEYCODE_K, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICK_LEFT,      "P3 Left",    			SEQ_DEF_3(KEYCODE_J, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICK_RIGHT,     "P3 Right",   			SEQ_DEF_3(KEYCODE_L, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICKRIGHT_UP,   "P3 Right/Up",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICKRIGHT_DOWN, "P3 Right/Down",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICKRIGHT_LEFT, "P3 Right/Left",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICKRIGHT_RIGHT,"P3 Right/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICKLEFT_UP,    "P3 Left/Up", 			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICKLEFT_DOWN,  "P3 Left/Down",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICKLEFT_LEFT,  "P3 Left/Left",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	JOYSTICKLEFT_RIGHT, "P3 Left/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON1,			"P3 Button 1",			SEQ_DEF_5(KEYCODE_RCONTROL, SEQCODE_OR, INDEXED(JOYCODE_BUTTON1, 2), SEQCODE_OR, INDEXED(GUNCODE_BUTTON1, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON2,			"P3 Button 2",			SEQ_DEF_5(KEYCODE_RSHIFT, SEQCODE_OR, INDEXED(JOYCODE_BUTTON2, 2), SEQCODE_OR, INDEXED(GUNCODE_BUTTON2, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON3,			"P3 Button 3",			SEQ_DEF_3(KEYCODE_ENTER, SEQCODE_OR, INDEXED(JOYCODE_BUTTON3, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON4,			"P3 Button 4",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON4, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON5,			"P3 Button 5",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON5, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON6,			"P3 Button 6",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON6, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON7,			"P3 Button 7",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON7, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON8,			"P3 Button 8",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON8, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON9,			"P3 Button 9",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON9, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON10,			"P3 Button 10",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON10, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON11,			"P3 Button 11",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON11, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON12,			"P3 Button 12",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON12, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON13,			"P3 Button 13",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON13, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON14,			"P3 Button 14",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON14, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON15,			"P3 Button 15",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON15, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	BUTTON16,			"P3 Button 16",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON16, 2)) )
#ifdef USE_CUSTOM_BUTTON
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	CUSTOM1,			"P3 Custom 1",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	CUSTOM2,			"P3 Custom 2",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	CUSTOM3,			"P3 Custom 3",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	CUSTOM4,			"P3 Custom 4",			SEQ_DEF_0 )
#endif /* USE_CUSTOM_BUTTON */
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3, START,				"P3 Start",				SEQ_DEF_3(KEYCODE_3, SEQCODE_OR, INDEXED(JOYCODE_START, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3, SELECT,				"P3 Select",			SEQ_DEF_3(KEYCODE_7, SEQCODE_OR, INDEXED(JOYCODE_SELECT, 2)) )
	INPUT_PORT_DIGITAL_DEF( 3, IPG_PLAYER3,	TOGGLE_AUTOFIRE,		"P3 Toggle Autofire",	SEQ_DEF_0 )

	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICK_UP,		"P4 Up",				SEQ_DEF_3(KEYCODE_8_PAD, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICK_DOWN,      "P4 Down",    			SEQ_DEF_3(KEYCODE_2_PAD, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICK_LEFT,      "P4 Left",    			SEQ_DEF_3(KEYCODE_4_PAD, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICK_RIGHT,     "P4 Right",   			SEQ_DEF_3(KEYCODE_6_PAD, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICKRIGHT_UP,   "P4 Right/Up",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICKRIGHT_DOWN, "P4 Right/Down",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICKRIGHT_LEFT, "P4 Right/Left",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICKRIGHT_RIGHT,"P4 Right/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICKLEFT_UP,    "P4 Left/Up", 			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICKLEFT_DOWN,  "P4 Left/Down",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICKLEFT_LEFT,  "P4 Left/Left",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	JOYSTICKLEFT_RIGHT, "P4 Left/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON1,			"P4 Button 1",			SEQ_DEF_3(KEYCODE_0_PAD, SEQCODE_OR, INDEXED(JOYCODE_BUTTON1, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON2,			"P4 Button 2",			SEQ_DEF_3(KEYCODE_DEL_PAD, SEQCODE_OR, INDEXED(JOYCODE_BUTTON2, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON3,			"P4 Button 3",			SEQ_DEF_3(KEYCODE_ENTER_PAD, SEQCODE_OR, INDEXED(JOYCODE_BUTTON3, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON4,			"P4 Button 4",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON4, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON5,			"P4 Button 5",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON5, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON6,			"P4 Button 6",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON6, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON7,			"P4 Button 7",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON7, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON8,			"P4 Button 8",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON8, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON9,			"P4 Button 9",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON9, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON10,			"P4 Button 10",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON10, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON11,			"P4 Button 11",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON11, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON12,			"P4 Button 12",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON12, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON13,			"P4 Button 13",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON13, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON14,			"P4 Button 14",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON14, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON15,			"P4 Button 15",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON15, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	BUTTON16,			"P4 Button 16",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON16, 3)) )
#ifdef USE_CUSTOM_BUTTON
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	CUSTOM1,			"P4 Custom 1",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	CUSTOM2,			"P4 Custom 2",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	CUSTOM3,			"P4 Custom 3",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	CUSTOM4,			"P4 Custom 4",			SEQ_DEF_0 )
#endif /* USE_CUSTOM_BUTTON */
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4, START,				"P4 Start",				SEQ_DEF_3(KEYCODE_4, SEQCODE_OR, INDEXED(JOYCODE_START, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4, SELECT,				"P4 Select",			SEQ_DEF_3(KEYCODE_8, SEQCODE_OR, INDEXED(JOYCODE_SELECT, 3)) )
	INPUT_PORT_DIGITAL_DEF( 4, IPG_PLAYER4,	TOGGLE_AUTOFIRE,		"P4 Toggle Autofire",	SEQ_DEF_0 )

	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICK_UP,		"P5 Up",				SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICK_DOWN,      "P5 Down",    			SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICK_LEFT,      "P5 Left",    			SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICK_RIGHT,     "P5 Right",   			SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICKRIGHT_UP,   "P5 Right/Up",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICKRIGHT_DOWN, "P5 Right/Down",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICKRIGHT_LEFT, "P5 Right/Left",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICKRIGHT_RIGHT,"P5 Right/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICKLEFT_UP,    "P5 Left/Up", 			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICKLEFT_DOWN,  "P5 Left/Down",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICKLEFT_LEFT,  "P5 Left/Left",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	JOYSTICKLEFT_RIGHT, "P5 Left/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON1,			"P5 Button 1",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON1, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON2,			"P5 Button 2",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON2, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON3,			"P5 Button 3",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON3, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON4,			"P5 Button 4",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON4, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON5,			"P5 Button 5",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON5, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON6,			"P5 Button 6",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON6, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON7,			"P5 Button 7",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON7, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON8,			"P5 Button 8",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON8, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON9,			"P5 Button 9",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON9, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON10,			"P5 Button 10",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON10, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON11,			"P5 Button 11",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON11, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON12,			"P5 Button 12",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON12, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON13,			"P5 Button 13",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON13, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON14,			"P5 Button 14",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON14, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON15,			"P5 Button 15",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON15, 4)) )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	BUTTON16,			"P5 Button 16",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON16, 4)) )
#ifdef USE_CUSTOM_BUTTON
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	CUSTOM1,			"P5 Custom 1",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	CUSTOM2,			"P5 Custom 2",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	CUSTOM3,			"P5 Custom 3",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	CUSTOM4,			"P5 Custom 4",			SEQ_DEF_0 )
#endif /* USE_CUSTOM_BUTTON */
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5, START,				"P5 Start",				SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5, SELECT,				"P5 Select",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 5, IPG_PLAYER5,	TOGGLE_AUTOFIRE,		"P5 Toggle Autofire",	SEQ_DEF_0 )

	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICK_UP,		"P6 Up",				SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICK_DOWN,      "P6 Down",    			SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICK_LEFT,      "P6 Left",    			SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICK_RIGHT,     "P6 Right",   			SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICKRIGHT_UP,   "P6 Right/Up",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICKRIGHT_DOWN, "P6 Right/Down",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICKRIGHT_LEFT, "P6 Right/Left",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICKRIGHT_RIGHT,"P6 Right/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICKLEFT_UP,    "P6 Left/Up", 			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICKLEFT_DOWN,  "P6 Left/Down",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICKLEFT_LEFT,  "P6 Left/Left",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	JOYSTICKLEFT_RIGHT, "P6 Left/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON1,			"P6 Button 1",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON1, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON2,			"P6 Button 2",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON2, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON3,			"P6 Button 3",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON3, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON4,			"P6 Button 4",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON4, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON5,			"P6 Button 5",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON5, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON6,			"P6 Button 6",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON6, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON7,			"P6 Button 7",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON7, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON8,			"P6 Button 8",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON8, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON9,			"P6 Button 9",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON9, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON10,			"P6 Button 10",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON10, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON11,			"P6 Button 11",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON11, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON12,			"P6 Button 12",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON12, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON13,			"P6 Button 13",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON13, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON14,			"P6 Button 14",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON14, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON15,			"P6 Button 15",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON15, 5)) )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	BUTTON16,			"P6 Button 16",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON16, 5)) )
#ifdef USE_CUSTOM_BUTTON
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	CUSTOM1,			"P6 Custom 1",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	CUSTOM2,			"P6 Custom 2",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	CUSTOM3,			"P6 Custom 3",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	CUSTOM4,			"P6 Custom 4",			SEQ_DEF_0 )
#endif /* USE_CUSTOM_BUTTON */
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6, START,				"P6 Start",				SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6, SELECT,				"P6 Select",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 6, IPG_PLAYER6,	TOGGLE_AUTOFIRE,		"P6 Toggle Autofire",	SEQ_DEF_0 )

	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICK_UP,		"P7 Up",				SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICK_DOWN,      "P7 Down",    			SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICK_LEFT,      "P7 Left",    			SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICK_RIGHT,     "P7 Right",   			SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICKRIGHT_UP,   "P7 Right/Up",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICKRIGHT_DOWN, "P7 Right/Down",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICKRIGHT_LEFT, "P7 Right/Left",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICKRIGHT_RIGHT,"P7 Right/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICKLEFT_UP,    "P7 Left/Up", 			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICKLEFT_DOWN,  "P7 Left/Down",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICKLEFT_LEFT,  "P7 Left/Left",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	JOYSTICKLEFT_RIGHT, "P7 Left/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON1,			"P7 Button 1",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON1, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON2,			"P7 Button 2",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON2, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON3,			"P7 Button 3",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON3, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON4,			"P7 Button 4",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON4, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON5,			"P7 Button 5",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON5, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON6,			"P7 Button 6",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON6, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON7,			"P7 Button 7",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON7, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON8,			"P7 Button 8",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON8, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON9,			"P7 Button 9",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON9, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON10,			"P7 Button 10",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON10, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON11,			"P7 Button 11",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON11, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON12,			"P7 Button 12",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON12, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON13,			"P7 Button 13",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON13, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON14,			"P7 Button 14",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON14, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON15,			"P7 Button 15",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON15, 6)) )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	BUTTON16,			"P7 Button 16",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON16, 6)) )
#ifdef USE_CUSTOM_BUTTON
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	CUSTOM1,			"P7 Custom 1",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	CUSTOM2,			"P7 Custom 2",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	CUSTOM3,			"P7 Custom 3",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	CUSTOM4,			"P7 Custom 4",			SEQ_DEF_0 )
#endif /* USE_CUSTOM_BUTTON */
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7, START,				"P7 Start",				SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7, SELECT,				"P7 Select",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 7, IPG_PLAYER7,	TOGGLE_AUTOFIRE,		"P7 Toggle Autofire",	SEQ_DEF_0 )

	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICK_UP,		"P8 Up",				SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICK_DOWN,      "P8 Down",				SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICK_LEFT,      "P8 Left",				SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICK_RIGHT,     "P8 Right",				SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICKRIGHT_UP,   "P8 Right/Up",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICKRIGHT_DOWN, "P8 Right/Down",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICKRIGHT_LEFT, "P8 Right/Left",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICKRIGHT_RIGHT,"P8 Right/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICKLEFT_UP,    "P8 Left/Up",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICKLEFT_DOWN,  "P8 Left/Down",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICKLEFT_LEFT,  "P8 Left/Left",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	JOYSTICKLEFT_RIGHT, "P8 Left/Right",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON1,			"P8 Button 1",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON1, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON2,			"P8 Button 2",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON2, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON3,			"P8 Button 3",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON3, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON4,			"P8 Button 4",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON4, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON5,			"P8 Button 5",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON5, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON6,			"P8 Button 6",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON6, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON7,			"P8 Button 7",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON7, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON8,			"P8 Button 8",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON8, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON9,			"P8 Button 9",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON9, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON10,			"P8 Button 10",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON10, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON11,			"P8 Button 11",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON11, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON12,			"P8 Button 12",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON12, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON13,			"P8 Button 13",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON13, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON14,			"P8 Button 14",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON14, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON15,			"P8 Button 15",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON15, 7)) )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	BUTTON16,			"P8 Button 16",			SEQ_DEF_1(INDEXED(JOYCODE_BUTTON16, 7)) )
#ifdef USE_CUSTOM_BUTTON
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	CUSTOM1,			"P8 Custom 1",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	CUSTOM2,			"P8 Custom 2",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	CUSTOM3,			"P8 Custom 3",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	CUSTOM4,			"P8 Custom 4",			SEQ_DEF_0 )
#endif /* USE_CUSTOM_BUTTON */
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8, START,				"P8 Start",				SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8, SELECT,				"P8 Select",			SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 8, IPG_PLAYER8,	TOGGLE_AUTOFIRE,		"P8 Toggle Autofire",	SEQ_DEF_0 )

	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   START1,				"1 Player Start",		SEQ_DEF_3(KEYCODE_1, SEQCODE_OR, INDEXED(JOYCODE_START, 0)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   START2,				"2 Players Start",		SEQ_DEF_3(KEYCODE_2, SEQCODE_OR, INDEXED(JOYCODE_START, 1)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   START3,				"3 Players Start",		SEQ_DEF_3(KEYCODE_3, SEQCODE_OR, INDEXED(JOYCODE_START, 2)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   START4,				"4 Players Start",		SEQ_DEF_3(KEYCODE_4, SEQCODE_OR, INDEXED(JOYCODE_START, 3)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   START5,				"5 Players Start",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   START6,				"6 Players Start",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   START7,				"7 Players Start",		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   START8,				"8 Players Start",		SEQ_DEF_0 )

	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   COIN1,				"Coin 1",				SEQ_DEF_3(KEYCODE_5, SEQCODE_OR, INDEXED(JOYCODE_SELECT, 0)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   COIN2,				"Coin 2",				SEQ_DEF_3(KEYCODE_6, SEQCODE_OR, INDEXED(JOYCODE_SELECT, 1)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   COIN3,				"Coin 3",				SEQ_DEF_3(KEYCODE_7, SEQCODE_OR, INDEXED(JOYCODE_SELECT, 2)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   COIN4,				"Coin 4",				SEQ_DEF_3(KEYCODE_8, SEQCODE_OR, INDEXED(JOYCODE_SELECT, 3)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   COIN5,				"Coin 5",				SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   COIN6,				"Coin 6",				SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   COIN7,				"Coin 7",				SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   COIN8,				"Coin 8",				SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   BILL1,				"Bill 1",				SEQ_DEF_1(KEYCODE_BACKSPACE) )

	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   SERVICE1,			"Service 1",    		SEQ_DEF_1(KEYCODE_9) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   SERVICE2,			"Service 2",    		SEQ_DEF_1(KEYCODE_0) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   SERVICE3,			"Service 3",     		SEQ_DEF_1(KEYCODE_MINUS) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   SERVICE4,			"Service 4",     		SEQ_DEF_1(KEYCODE_EQUALS) )

	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   SERVICE,			"Service",	     		SEQ_DEF_1(KEYCODE_F2) )
#ifdef KAILLERA
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   TILT, 				"Tilt",			   		SEQ_DEF_0 )
#else
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   TILT, 				"Tilt",			   		SEQ_DEF_1(KEYCODE_T) )
#endif /* KAILLERA */
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   INTERLOCK,			"Door Interlock",  		SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   VOLUME_DOWN,		"Volume Down",     		SEQ_DEF_1(KEYCODE_MINUS) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   VOLUME_UP,			"Volume Up",     		SEQ_DEF_1(KEYCODE_EQUALS) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	PEDAL,				"P1 Pedal 1",     		SEQ_DEF_1(INDEXED(JOYCODE_Y_NEG_ABSOLUTE, 0)), SEQ_DEF_0, SEQ_DEF_3(KEYCODE_LCONTROL, SEQCODE_OR, INDEXED(JOYCODE_BUTTON1, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	PEDAL,				"P2 Pedal 1", 			SEQ_DEF_1(INDEXED(JOYCODE_Y_NEG_ABSOLUTE, 1)), SEQ_DEF_0, SEQ_DEF_3(KEYCODE_A, SEQCODE_OR, INDEXED(JOYCODE_BUTTON1, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	PEDAL,				"P3 Pedal 1",			SEQ_DEF_1(INDEXED(JOYCODE_Y_NEG_ABSOLUTE, 2)), SEQ_DEF_0, SEQ_DEF_3(KEYCODE_RCONTROL, SEQCODE_OR, INDEXED(JOYCODE_BUTTON1, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	PEDAL,				"P4 Pedal 1",			SEQ_DEF_1(INDEXED(JOYCODE_Y_NEG_ABSOLUTE, 3)), SEQ_DEF_0, SEQ_DEF_3(KEYCODE_0_PAD, SEQCODE_OR, INDEXED(JOYCODE_BUTTON1, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	PEDAL,				"P5 Pedal 1",			SEQ_DEF_1(INDEXED(JOYCODE_Y_NEG_ABSOLUTE, 4)), SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON1, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	PEDAL,				"P6 Pedal 1",			SEQ_DEF_1(INDEXED(JOYCODE_Y_NEG_ABSOLUTE, 5)), SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON1, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	PEDAL,				"P7 Pedal 1",			SEQ_DEF_1(INDEXED(JOYCODE_Y_NEG_ABSOLUTE, 6)), SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON1, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	PEDAL,				"P8 Pedal 1",			SEQ_DEF_1(INDEXED(JOYCODE_Y_NEG_ABSOLUTE, 7)), SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON1, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	PEDAL2,				"P1 Pedal 2",			SEQ_DEF_1(INDEXED(JOYCODE_Y_POS_ABSOLUTE, 0)), SEQ_DEF_0, SEQ_DEF_3(KEYCODE_LALT, SEQCODE_OR, INDEXED(JOYCODE_BUTTON2, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	PEDAL2,				"P2 Pedal 2",			SEQ_DEF_1(INDEXED(JOYCODE_Y_POS_ABSOLUTE, 1)), SEQ_DEF_0, SEQ_DEF_3(KEYCODE_S, SEQCODE_OR, INDEXED(JOYCODE_BUTTON2, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	PEDAL2,				"P3 Pedal 2",			SEQ_DEF_1(INDEXED(JOYCODE_Y_POS_ABSOLUTE, 2)), SEQ_DEF_0, SEQ_DEF_3(KEYCODE_RSHIFT, SEQCODE_OR, INDEXED(JOYCODE_BUTTON2, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	PEDAL2,				"P4 Pedal 2",			SEQ_DEF_1(INDEXED(JOYCODE_Y_POS_ABSOLUTE, 3)), SEQ_DEF_0, SEQ_DEF_3(KEYCODE_DEL_PAD, SEQCODE_OR, INDEXED(JOYCODE_BUTTON2, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	PEDAL2,				"P5 Pedal 2",			SEQ_DEF_1(INDEXED(JOYCODE_Y_POS_ABSOLUTE, 4)), SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON2, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	PEDAL2,				"P6 Pedal 2",			SEQ_DEF_1(INDEXED(JOYCODE_Y_POS_ABSOLUTE, 5)), SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON2, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	PEDAL2,				"P7 Pedal 2",			SEQ_DEF_1(INDEXED(JOYCODE_Y_POS_ABSOLUTE, 6)), SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON2, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	PEDAL2,				"P8 Pedal 2",			SEQ_DEF_1(INDEXED(JOYCODE_Y_POS_ABSOLUTE, 7)), SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON2, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	PEDAL3,				"P1 Pedal 3",			SEQ_DEF_0, SEQ_DEF_0, SEQ_DEF_3(KEYCODE_SPACE, SEQCODE_OR, INDEXED(JOYCODE_BUTTON3, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	PEDAL3,				"P2 Pedal 3",			SEQ_DEF_0, SEQ_DEF_0, SEQ_DEF_3(KEYCODE_Q, SEQCODE_OR, INDEXED(JOYCODE_BUTTON3, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	PEDAL3,				"P3 Pedal 3",			SEQ_DEF_0, SEQ_DEF_0, SEQ_DEF_3(KEYCODE_ENTER, SEQCODE_OR, INDEXED(JOYCODE_BUTTON3, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	PEDAL3,				"P4 Pedal 3",			SEQ_DEF_0, SEQ_DEF_0, SEQ_DEF_3(KEYCODE_ENTER_PAD, SEQCODE_OR, INDEXED(JOYCODE_BUTTON3, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	PEDAL3,				"P5 Pedal 3",			SEQ_DEF_0, SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON3, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	PEDAL3,				"P6 Pedal 3",			SEQ_DEF_0, SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON3, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	PEDAL3,				"P7 Pedal 3",			SEQ_DEF_0, SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON3, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	PEDAL3,				"P8 Pedal 3",			SEQ_DEF_0, SEQ_DEF_0, SEQ_DEF_1(INDEXED(JOYCODE_BUTTON3, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	PADDLE,				"Paddle",   	    	SEQ_DEF_3(INDEXED(MOUSECODE_X, 0), SEQCODE_OR, INDEXED(JOYCODE_X, 0)), SEQ_DEF_3(KEYCODE_LEFT, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 0)), SEQ_DEF_3(KEYCODE_RIGHT, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	PADDLE,				"Paddle 2",      		SEQ_DEF_3(INDEXED(MOUSECODE_X, 1), SEQCODE_OR, INDEXED(JOYCODE_X, 1)), SEQ_DEF_3(KEYCODE_D, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 1)), SEQ_DEF_3(KEYCODE_G, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	PADDLE,				"Paddle 3",      		SEQ_DEF_3(INDEXED(MOUSECODE_X, 2), SEQCODE_OR, INDEXED(JOYCODE_X, 2)), SEQ_DEF_3(KEYCODE_J, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 2)), SEQ_DEF_3(KEYCODE_L, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	PADDLE,				"Paddle 4",      		SEQ_DEF_3(INDEXED(MOUSECODE_X, 3), SEQCODE_OR, INDEXED(JOYCODE_X, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	PADDLE,				"Paddle 5",      		SEQ_DEF_3(INDEXED(MOUSECODE_X, 4), SEQCODE_OR, INDEXED(JOYCODE_X, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	PADDLE,				"Paddle 6",      		SEQ_DEF_3(INDEXED(MOUSECODE_X, 5), SEQCODE_OR, INDEXED(JOYCODE_X, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	PADDLE,				"Paddle 7",      		SEQ_DEF_3(INDEXED(MOUSECODE_X, 6), SEQCODE_OR, INDEXED(JOYCODE_X, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	PADDLE,				"Paddle 8",      		SEQ_DEF_3(INDEXED(MOUSECODE_X, 7), SEQCODE_OR, INDEXED(JOYCODE_X, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	PADDLE_V,			"Paddle V",				SEQ_DEF_3(INDEXED(MOUSECODE_Y, 0), SEQCODE_OR, INDEXED(JOYCODE_Y, 0)), SEQ_DEF_3(KEYCODE_UP, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 0)), SEQ_DEF_3(KEYCODE_DOWN, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	PADDLE_V,			"Paddle V 2",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 1), SEQCODE_OR, INDEXED(JOYCODE_Y, 1)), SEQ_DEF_3(KEYCODE_R, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 1)), SEQ_DEF_3(KEYCODE_F, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	PADDLE_V,			"Paddle V 3",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 2), SEQCODE_OR, INDEXED(JOYCODE_Y, 2)), SEQ_DEF_3(KEYCODE_I, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 2)), SEQ_DEF_3(KEYCODE_K, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	PADDLE_V,			"Paddle V 4",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 3), SEQCODE_OR, INDEXED(JOYCODE_Y, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	PADDLE_V,			"Paddle V 5",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 4), SEQCODE_OR, INDEXED(JOYCODE_Y, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	PADDLE_V,			"Paddle V 6",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 5), SEQCODE_OR, INDEXED(JOYCODE_Y, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	PADDLE_V,			"Paddle V 7",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 6), SEQCODE_OR, INDEXED(JOYCODE_Y, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	PADDLE_V,			"Paddle V 8",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 7), SEQCODE_OR, INDEXED(JOYCODE_Y, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	POSITIONAL,			"Positional",			SEQ_DEF_3(INDEXED(MOUSECODE_X, 0), SEQCODE_OR, INDEXED(JOYCODE_X, 0)), SEQ_DEF_3(KEYCODE_LEFT, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 0)), SEQ_DEF_3(KEYCODE_RIGHT, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	POSITIONAL,			"Positional 2",			SEQ_DEF_3(INDEXED(MOUSECODE_X, 1), SEQCODE_OR, INDEXED(JOYCODE_X, 1)), SEQ_DEF_3(KEYCODE_D, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 1)), SEQ_DEF_3(KEYCODE_G, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	POSITIONAL,			"Positional 3", 		SEQ_DEF_3(INDEXED(MOUSECODE_X, 2), SEQCODE_OR, INDEXED(JOYCODE_X, 2)), SEQ_DEF_3(KEYCODE_J, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 2)), SEQ_DEF_3(KEYCODE_L, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	POSITIONAL,			"Positional 4", 		SEQ_DEF_3(INDEXED(MOUSECODE_X, 3), SEQCODE_OR, INDEXED(JOYCODE_X, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	POSITIONAL,			"Positional 5", 		SEQ_DEF_3(INDEXED(MOUSECODE_X, 4), SEQCODE_OR, INDEXED(JOYCODE_X, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	POSITIONAL,			"Positional 6", 		SEQ_DEF_3(INDEXED(MOUSECODE_X, 5), SEQCODE_OR, INDEXED(JOYCODE_X, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	POSITIONAL,			"Positional 7", 		SEQ_DEF_3(INDEXED(MOUSECODE_X, 6), SEQCODE_OR, INDEXED(JOYCODE_X, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	POSITIONAL,			"Positional 8", 		SEQ_DEF_3(INDEXED(MOUSECODE_X, 7), SEQCODE_OR, INDEXED(JOYCODE_X, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	POSITIONAL_V,		"Positional V",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 0), SEQCODE_OR, INDEXED(JOYCODE_Y, 0)), SEQ_DEF_3(KEYCODE_UP, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 0)), SEQ_DEF_3(KEYCODE_DOWN, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	POSITIONAL_V,		"Positional V 2",		SEQ_DEF_3(INDEXED(MOUSECODE_Y, 1), SEQCODE_OR, INDEXED(JOYCODE_Y, 1)), SEQ_DEF_3(KEYCODE_R, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 1)), SEQ_DEF_3(KEYCODE_F, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	POSITIONAL_V,		"Positional V 3", 		SEQ_DEF_3(INDEXED(MOUSECODE_Y, 2), SEQCODE_OR, INDEXED(JOYCODE_Y, 2)), SEQ_DEF_3(KEYCODE_I, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 2)), SEQ_DEF_3(KEYCODE_K, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	POSITIONAL_V,		"Positional V 4", 		SEQ_DEF_3(INDEXED(MOUSECODE_Y, 3), SEQCODE_OR, INDEXED(JOYCODE_Y, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	POSITIONAL_V,		"Positional V 5", 		SEQ_DEF_3(INDEXED(MOUSECODE_Y, 4), SEQCODE_OR, INDEXED(JOYCODE_Y, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	POSITIONAL_V,		"Positional V 6", 		SEQ_DEF_3(INDEXED(MOUSECODE_Y, 5), SEQCODE_OR, INDEXED(JOYCODE_Y, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	POSITIONAL_V,		"Positional V 7", 		SEQ_DEF_3(INDEXED(MOUSECODE_Y, 6), SEQCODE_OR, INDEXED(JOYCODE_Y, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	POSITIONAL_V,		"Positional V 8", 		SEQ_DEF_3(INDEXED(MOUSECODE_Y, 7), SEQCODE_OR, INDEXED(JOYCODE_Y, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	DIAL,				"Dial",					SEQ_DEF_3(INDEXED(MOUSECODE_X, 0), SEQCODE_OR, INDEXED(JOYCODE_X, 0)), SEQ_DEF_3(KEYCODE_LEFT, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 0)), SEQ_DEF_3(KEYCODE_RIGHT, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	DIAL,				"Dial 2",				SEQ_DEF_3(INDEXED(MOUSECODE_X, 1), SEQCODE_OR, INDEXED(JOYCODE_X, 1)), SEQ_DEF_3(KEYCODE_D, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 1)), SEQ_DEF_3(KEYCODE_G, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	DIAL,				"Dial 3",				SEQ_DEF_3(INDEXED(MOUSECODE_X, 2), SEQCODE_OR, INDEXED(JOYCODE_X, 2)), SEQ_DEF_3(KEYCODE_J, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 2)), SEQ_DEF_3(KEYCODE_L, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	DIAL,				"Dial 4",				SEQ_DEF_3(INDEXED(MOUSECODE_X, 3), SEQCODE_OR, INDEXED(JOYCODE_X, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	DIAL,				"Dial 5",				SEQ_DEF_3(INDEXED(MOUSECODE_X, 4), SEQCODE_OR, INDEXED(JOYCODE_X, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	DIAL,				"Dial 6",				SEQ_DEF_3(INDEXED(MOUSECODE_X, 5), SEQCODE_OR, INDEXED(JOYCODE_X, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	DIAL,				"Dial 7",				SEQ_DEF_3(INDEXED(MOUSECODE_X, 6), SEQCODE_OR, INDEXED(JOYCODE_X, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	DIAL,				"Dial 8",				SEQ_DEF_3(INDEXED(MOUSECODE_X, 7), SEQCODE_OR, INDEXED(JOYCODE_X, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	DIAL_V,				"Dial V",				SEQ_DEF_3(INDEXED(MOUSECODE_Y, 0), SEQCODE_OR, INDEXED(JOYCODE_Y, 0)), SEQ_DEF_3(KEYCODE_UP, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 0)), SEQ_DEF_3(KEYCODE_DOWN, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	DIAL_V,				"Dial V 2",				SEQ_DEF_3(INDEXED(MOUSECODE_Y, 1), SEQCODE_OR, INDEXED(JOYCODE_Y, 1)), SEQ_DEF_3(KEYCODE_R, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 1)), SEQ_DEF_3(KEYCODE_F, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	DIAL_V,				"Dial V 3",				SEQ_DEF_3(INDEXED(MOUSECODE_Y, 2), SEQCODE_OR, INDEXED(JOYCODE_Y, 2)), SEQ_DEF_3(KEYCODE_I, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 2)), SEQ_DEF_3(KEYCODE_K, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	DIAL_V,				"Dial V 4",				SEQ_DEF_3(INDEXED(MOUSECODE_Y, 3), SEQCODE_OR, INDEXED(JOYCODE_Y, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	DIAL_V,				"Dial V 5",				SEQ_DEF_3(INDEXED(MOUSECODE_Y, 4), SEQCODE_OR, INDEXED(JOYCODE_Y, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	DIAL_V,				"Dial V 6",				SEQ_DEF_3(INDEXED(MOUSECODE_Y, 5), SEQCODE_OR, INDEXED(JOYCODE_Y, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	DIAL_V,				"Dial V 7",				SEQ_DEF_3(INDEXED(MOUSECODE_Y, 6), SEQCODE_OR, INDEXED(JOYCODE_Y, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	DIAL_V,				"Dial V 8",				SEQ_DEF_3(INDEXED(MOUSECODE_Y, 7), SEQCODE_OR, INDEXED(JOYCODE_Y, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	TRACKBALL_X,		"Track X",				SEQ_DEF_3(INDEXED(MOUSECODE_X, 0), SEQCODE_OR, INDEXED(JOYCODE_X, 0)), SEQ_DEF_3(KEYCODE_LEFT, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 0)), SEQ_DEF_3(KEYCODE_RIGHT, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	TRACKBALL_X,		"Track X 2",			SEQ_DEF_3(INDEXED(MOUSECODE_X, 1), SEQCODE_OR, INDEXED(JOYCODE_X, 1)), SEQ_DEF_3(KEYCODE_D, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 1)), SEQ_DEF_3(KEYCODE_G, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	TRACKBALL_X,		"Track X 3",			SEQ_DEF_3(INDEXED(MOUSECODE_X, 2), SEQCODE_OR, INDEXED(JOYCODE_X, 2)), SEQ_DEF_3(KEYCODE_J, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 2)), SEQ_DEF_3(KEYCODE_L, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	TRACKBALL_X,		"Track X 4",			SEQ_DEF_3(INDEXED(MOUSECODE_X, 3), SEQCODE_OR, INDEXED(JOYCODE_X, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	TRACKBALL_X,		"Track X 5",			SEQ_DEF_3(INDEXED(MOUSECODE_X, 4), SEQCODE_OR, INDEXED(JOYCODE_X, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	TRACKBALL_X,		"Track X 6",			SEQ_DEF_3(INDEXED(MOUSECODE_X, 5), SEQCODE_OR, INDEXED(JOYCODE_X, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	TRACKBALL_X,		"Track X 7",			SEQ_DEF_3(INDEXED(MOUSECODE_X, 6), SEQCODE_OR, INDEXED(JOYCODE_X, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	TRACKBALL_X,		"Track X 8",			SEQ_DEF_3(INDEXED(MOUSECODE_X, 7), SEQCODE_OR, INDEXED(JOYCODE_X, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	TRACKBALL_Y,		"Track Y",				SEQ_DEF_3(INDEXED(MOUSECODE_Y, 0), SEQCODE_OR, INDEXED(JOYCODE_Y, 0)), SEQ_DEF_3(KEYCODE_UP, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 0)), SEQ_DEF_3(KEYCODE_DOWN, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	TRACKBALL_Y,		"Track Y 2",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 1), SEQCODE_OR, INDEXED(JOYCODE_Y, 1)), SEQ_DEF_3(KEYCODE_R, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 1)), SEQ_DEF_3(KEYCODE_F, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	TRACKBALL_Y,		"Track Y 3",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 2), SEQCODE_OR, INDEXED(JOYCODE_Y, 2)), SEQ_DEF_3(KEYCODE_I, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 2)), SEQ_DEF_3(KEYCODE_K, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	TRACKBALL_Y,		"Track Y 4",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 3), SEQCODE_OR, INDEXED(JOYCODE_Y, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	TRACKBALL_Y,		"Track Y 5",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 4), SEQCODE_OR, INDEXED(JOYCODE_Y, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	TRACKBALL_Y,		"Track Y 6",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 5), SEQCODE_OR, INDEXED(JOYCODE_Y, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	TRACKBALL_Y,		"Track Y 7",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 6), SEQCODE_OR, INDEXED(JOYCODE_Y, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	TRACKBALL_Y,		"Track Y 8",			SEQ_DEF_3(INDEXED(MOUSECODE_Y, 7), SEQCODE_OR, INDEXED(JOYCODE_Y, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	AD_STICK_X,			"AD Stick X",			SEQ_DEF_3(INDEXED(JOYCODE_X, 0), SEQCODE_OR, INDEXED(MOUSECODE_X, 0)), SEQ_DEF_3(KEYCODE_LEFT, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 0)), SEQ_DEF_3(KEYCODE_RIGHT, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	AD_STICK_X,			"AD Stick X 2",			SEQ_DEF_3(INDEXED(JOYCODE_X, 1), SEQCODE_OR, INDEXED(MOUSECODE_X, 1)), SEQ_DEF_3(KEYCODE_D, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 1)), SEQ_DEF_3(KEYCODE_G, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	AD_STICK_X,			"AD Stick X 3",			SEQ_DEF_3(INDEXED(JOYCODE_X, 2), SEQCODE_OR, INDEXED(MOUSECODE_X, 2)), SEQ_DEF_3(KEYCODE_J, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 2)), SEQ_DEF_3(KEYCODE_L, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	AD_STICK_X,			"AD Stick X 4",			SEQ_DEF_3(INDEXED(JOYCODE_X, 3), SEQCODE_OR, INDEXED(MOUSECODE_X, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	AD_STICK_X,			"AD Stick X 5",			SEQ_DEF_3(INDEXED(JOYCODE_X, 4), SEQCODE_OR, INDEXED(MOUSECODE_X, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	AD_STICK_X,			"AD Stick X 6",			SEQ_DEF_3(INDEXED(JOYCODE_X, 5), SEQCODE_OR, INDEXED(MOUSECODE_X, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	AD_STICK_X,			"AD Stick X 7",			SEQ_DEF_3(INDEXED(JOYCODE_X, 6), SEQCODE_OR, INDEXED(MOUSECODE_X, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	AD_STICK_X,			"AD Stick X 8",			SEQ_DEF_3(INDEXED(JOYCODE_X, 7), SEQCODE_OR, INDEXED(MOUSECODE_X, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	AD_STICK_Y,			"AD Stick Y",			SEQ_DEF_3(INDEXED(JOYCODE_Y, 0), SEQCODE_OR, INDEXED(MOUSECODE_Y, 0)), SEQ_DEF_3(KEYCODE_UP, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 0)), SEQ_DEF_3(KEYCODE_DOWN, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	AD_STICK_Y,			"AD Stick Y 2",			SEQ_DEF_3(INDEXED(JOYCODE_Y, 1), SEQCODE_OR, INDEXED(MOUSECODE_Y, 1)), SEQ_DEF_3(KEYCODE_R, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 1)), SEQ_DEF_3(KEYCODE_F, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	AD_STICK_Y,			"AD Stick Y 3",			SEQ_DEF_3(INDEXED(JOYCODE_Y, 2), SEQCODE_OR, INDEXED(MOUSECODE_Y, 2)), SEQ_DEF_3(KEYCODE_I, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 2)), SEQ_DEF_3(KEYCODE_K, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	AD_STICK_Y,			"AD Stick Y 4",			SEQ_DEF_3(INDEXED(JOYCODE_Y, 3), SEQCODE_OR, INDEXED(MOUSECODE_Y, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	AD_STICK_Y,			"AD Stick Y 5",			SEQ_DEF_3(INDEXED(JOYCODE_Y, 4), SEQCODE_OR, INDEXED(MOUSECODE_Y, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	AD_STICK_Y,			"AD Stick Y 6",			SEQ_DEF_3(INDEXED(JOYCODE_Y, 5), SEQCODE_OR, INDEXED(MOUSECODE_Y, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	AD_STICK_Y,			"AD Stick Y 7",			SEQ_DEF_3(INDEXED(JOYCODE_Y, 6), SEQCODE_OR, INDEXED(MOUSECODE_Y, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	AD_STICK_Y,			"AD Stick Y 8",			SEQ_DEF_3(INDEXED(JOYCODE_Y, 7), SEQCODE_OR, INDEXED(MOUSECODE_Y, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	AD_STICK_Z,			"AD Stick Z",			SEQ_DEF_1(INDEXED(JOYCODE_Z, 0)), SEQ_DEF_0, SEQ_DEF_0 )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	AD_STICK_Z,			"AD Stick Z 2",			SEQ_DEF_1(INDEXED(JOYCODE_Z, 1)), SEQ_DEF_0, SEQ_DEF_0 )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	AD_STICK_Z,			"AD Stick Z 3",			SEQ_DEF_1(INDEXED(JOYCODE_Z, 2)), SEQ_DEF_0, SEQ_DEF_0 )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	AD_STICK_Z,			"AD Stick Z 4",			SEQ_DEF_1(INDEXED(JOYCODE_Z, 3)), SEQ_DEF_0, SEQ_DEF_0 )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	AD_STICK_Z,			"AD Stick Z 5",			SEQ_DEF_1(INDEXED(JOYCODE_Z, 4)), SEQ_DEF_0, SEQ_DEF_0 )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	AD_STICK_Z,			"AD Stick Z 6",			SEQ_DEF_1(INDEXED(JOYCODE_Z, 5)), SEQ_DEF_0, SEQ_DEF_0 )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	AD_STICK_Z,			"AD Stick Z 7",			SEQ_DEF_1(INDEXED(JOYCODE_Z, 6)), SEQ_DEF_0, SEQ_DEF_0 )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	AD_STICK_Z,			"AD Stick Z 8",			SEQ_DEF_1(INDEXED(JOYCODE_Z, 7)), SEQ_DEF_0, SEQ_DEF_0 )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	LIGHTGUN_X,			"Lightgun X",			SEQ_DEF_5(INDEXED(GUNCODE_X, 0), SEQCODE_OR, INDEXED(MOUSECODE_X, 0), SEQCODE_OR, INDEXED(JOYCODE_X, 0)), SEQ_DEF_3(KEYCODE_LEFT, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 0)), SEQ_DEF_3(KEYCODE_RIGHT, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	LIGHTGUN_X,			"Lightgun X 2",			SEQ_DEF_5(INDEXED(GUNCODE_X, 1), SEQCODE_OR, INDEXED(MOUSECODE_X, 1), SEQCODE_OR, INDEXED(JOYCODE_X, 1)), SEQ_DEF_3(KEYCODE_D, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 1)), SEQ_DEF_3(KEYCODE_G, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	LIGHTGUN_X,			"Lightgun X 3",			SEQ_DEF_5(INDEXED(GUNCODE_X, 2), SEQCODE_OR, INDEXED(MOUSECODE_X, 2), SEQCODE_OR, INDEXED(JOYCODE_X, 2)), SEQ_DEF_3(KEYCODE_J, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 2)), SEQ_DEF_3(KEYCODE_L, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	LIGHTGUN_X,			"Lightgun X 4",			SEQ_DEF_5(INDEXED(GUNCODE_X, 3), SEQCODE_OR, INDEXED(MOUSECODE_X, 3), SEQCODE_OR, INDEXED(JOYCODE_X, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	LIGHTGUN_X,			"Lightgun X 5",			SEQ_DEF_5(INDEXED(GUNCODE_X, 4), SEQCODE_OR, INDEXED(MOUSECODE_X, 4), SEQCODE_OR, INDEXED(JOYCODE_X, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	LIGHTGUN_X,			"Lightgun X 6",			SEQ_DEF_5(INDEXED(GUNCODE_X, 5), SEQCODE_OR, INDEXED(MOUSECODE_X, 5), SEQCODE_OR, INDEXED(JOYCODE_X, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	LIGHTGUN_X,			"Lightgun X 7",			SEQ_DEF_5(INDEXED(GUNCODE_X, 6), SEQCODE_OR, INDEXED(MOUSECODE_X, 6), SEQCODE_OR, INDEXED(JOYCODE_X, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	LIGHTGUN_X,			"Lightgun X 8",			SEQ_DEF_5(INDEXED(GUNCODE_X, 7), SEQCODE_OR, INDEXED(MOUSECODE_X, 7), SEQCODE_OR, INDEXED(JOYCODE_X, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	LIGHTGUN_Y,			"Lightgun Y",			SEQ_DEF_5(INDEXED(GUNCODE_Y, 0), SEQCODE_OR, INDEXED(MOUSECODE_Y, 0), SEQCODE_OR, INDEXED(JOYCODE_Y, 0)), SEQ_DEF_3(KEYCODE_UP, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 0)), SEQ_DEF_3(KEYCODE_DOWN, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	LIGHTGUN_Y,			"Lightgun Y 2",			SEQ_DEF_5(INDEXED(GUNCODE_Y, 1), SEQCODE_OR, INDEXED(MOUSECODE_Y, 1), SEQCODE_OR, INDEXED(JOYCODE_Y, 1)), SEQ_DEF_3(KEYCODE_R, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 1)), SEQ_DEF_3(KEYCODE_F, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	LIGHTGUN_Y,			"Lightgun Y 3",			SEQ_DEF_5(INDEXED(GUNCODE_Y, 2), SEQCODE_OR, INDEXED(MOUSECODE_Y, 2), SEQCODE_OR, INDEXED(JOYCODE_Y, 2)), SEQ_DEF_3(KEYCODE_I, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 2)), SEQ_DEF_3(KEYCODE_K, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	LIGHTGUN_Y,			"Lightgun Y 4",			SEQ_DEF_5(INDEXED(GUNCODE_Y, 3), SEQCODE_OR, INDEXED(MOUSECODE_Y, 3), SEQCODE_OR, INDEXED(JOYCODE_Y, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	LIGHTGUN_Y,			"Lightgun Y 5",			SEQ_DEF_5(INDEXED(GUNCODE_Y, 4), SEQCODE_OR, INDEXED(MOUSECODE_Y, 4), SEQCODE_OR, INDEXED(JOYCODE_Y, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	LIGHTGUN_Y,			"Lightgun Y 6",			SEQ_DEF_5(INDEXED(GUNCODE_Y, 5), SEQCODE_OR, INDEXED(MOUSECODE_Y, 5), SEQCODE_OR, INDEXED(JOYCODE_Y, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	LIGHTGUN_Y,			"Lightgun Y 7",			SEQ_DEF_5(INDEXED(GUNCODE_Y, 6), SEQCODE_OR, INDEXED(MOUSECODE_Y, 6), SEQCODE_OR, INDEXED(JOYCODE_Y, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	LIGHTGUN_Y,			"Lightgun Y 8",			SEQ_DEF_5(INDEXED(GUNCODE_Y, 7), SEQCODE_OR, INDEXED(MOUSECODE_Y, 7), SEQCODE_OR, INDEXED(JOYCODE_Y, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	MOUSE_X,			"Mouse X",				SEQ_DEF_1(INDEXED(MOUSECODE_X, 0)), SEQ_DEF_3(KEYCODE_LEFT, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 0)), SEQ_DEF_3(KEYCODE_RIGHT, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	MOUSE_X,			"Mouse X 2",			SEQ_DEF_1(INDEXED(MOUSECODE_X, 1)), SEQ_DEF_3(KEYCODE_D, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 1)), SEQ_DEF_3(KEYCODE_G, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	MOUSE_X,			"Mouse X 3",			SEQ_DEF_1(INDEXED(MOUSECODE_X, 2)), SEQ_DEF_3(KEYCODE_J, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 2)), SEQ_DEF_3(KEYCODE_L, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	MOUSE_X,			"Mouse X 4",			SEQ_DEF_1(INDEXED(MOUSECODE_X, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	MOUSE_X,			"Mouse X 5",			SEQ_DEF_1(INDEXED(MOUSECODE_X, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	MOUSE_X,			"Mouse X 6",			SEQ_DEF_1(INDEXED(MOUSECODE_X, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	MOUSE_X,			"Mouse X 7",			SEQ_DEF_1(INDEXED(MOUSECODE_X, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	MOUSE_X,			"Mouse X 8",			SEQ_DEF_1(INDEXED(MOUSECODE_X, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_LEFT_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_X_RIGHT_SWITCH, 7)) )

	INPUT_PORT_ANALOG_DEF ( 1, IPG_PLAYER1,	MOUSE_Y,			"Mouse Y",				SEQ_DEF_1(INDEXED(MOUSECODE_Y, 0)), SEQ_DEF_3(KEYCODE_UP, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 0)), SEQ_DEF_3(KEYCODE_DOWN, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 0)) )
	INPUT_PORT_ANALOG_DEF ( 2, IPG_PLAYER2,	MOUSE_Y,			"Mouse Y 2",			SEQ_DEF_1(INDEXED(MOUSECODE_Y, 1)), SEQ_DEF_3(KEYCODE_R, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 1)), SEQ_DEF_3(KEYCODE_F, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 1)) )
	INPUT_PORT_ANALOG_DEF ( 3, IPG_PLAYER3,	MOUSE_Y,			"Mouse Y 3",			SEQ_DEF_1(INDEXED(MOUSECODE_Y, 2)), SEQ_DEF_3(KEYCODE_I, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 2)), SEQ_DEF_3(KEYCODE_K, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 2)) )
	INPUT_PORT_ANALOG_DEF ( 4, IPG_PLAYER4,	MOUSE_Y,			"Mouse Y 4",			SEQ_DEF_1(INDEXED(MOUSECODE_Y, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 3)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 3)) )
	INPUT_PORT_ANALOG_DEF ( 5, IPG_PLAYER5,	MOUSE_Y,			"Mouse Y 5",			SEQ_DEF_1(INDEXED(MOUSECODE_Y, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 4)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 4)) )
	INPUT_PORT_ANALOG_DEF ( 6, IPG_PLAYER6,	MOUSE_Y,			"Mouse Y 6",			SEQ_DEF_1(INDEXED(MOUSECODE_Y, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 5)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 5)) )
	INPUT_PORT_ANALOG_DEF ( 7, IPG_PLAYER7,	MOUSE_Y,			"Mouse Y 7",			SEQ_DEF_1(INDEXED(MOUSECODE_Y, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 6)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 6)) )
	INPUT_PORT_ANALOG_DEF ( 8, IPG_PLAYER8,	MOUSE_Y,			"Mouse Y 8",			SEQ_DEF_1(INDEXED(MOUSECODE_Y, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_UP_SWITCH, 7)), SEQ_DEF_1(INDEXED(JOYCODE_Y_DOWN_SWITCH, 7)) )

	INPUT_PORT_DIGITAL_DEF( 0, IPG_OTHER,   KEYBOARD, 			"Keyboard",		   		SEQ_DEF_0 )

#ifdef MAME32PLUSPLUS
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_ON_SCREEN_DISPLAY,"On Screen Display",	SEQ_DEF_1(KEYCODE_O) )
#else
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_ON_SCREEN_DISPLAY,"On Screen Display",	SEQ_DEF_1(KEYCODE_TILDE) )
#endif /* MAME32PLUSPLUS */
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_DEBUG_BREAK,      "Break in Debugger",	SEQ_DEF_1(KEYCODE_TILDE) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_CONFIGURE,        "Config Menu",			SEQ_DEF_1(KEYCODE_TAB) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_PAUSE,            "Pause",				SEQ_DEF_1(KEYCODE_P) )
#ifdef KAILLERA
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_RESET_MACHINE,    "Reset Game",			SEQ_DEF_2(KEYCODE_F3, KEYCODE_RSHIFT) )
#else
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_RESET_MACHINE,    "Reset Game",			SEQ_DEF_2(KEYCODE_F3, KEYCODE_LSHIFT) )
#endif /* KAILLERA */
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_SOFT_RESET,       "Soft Reset",			SEQ_DEF_3(KEYCODE_F3, SEQCODE_NOT, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_SHOW_GFX,         "Show Gfx",			SEQ_DEF_1(KEYCODE_F4) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_FRAMESKIP_DEC,    "Frameskip Dec",		SEQ_DEF_1(KEYCODE_F8) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_FRAMESKIP_INC,    "Frameskip Inc",		SEQ_DEF_1(KEYCODE_F9) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_THROTTLE,         "Throttle",			SEQ_DEF_1(KEYCODE_F10) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_FAST_FORWARD,     "Fast Forward",		SEQ_DEF_1(KEYCODE_INSERT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_SHOW_FPS,         "Show FPS",			SEQ_DEF_5(KEYCODE_F11, SEQCODE_NOT, KEYCODE_LCONTROL, SEQCODE_NOT, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_SNAPSHOT,         "Save Snapshot",		SEQ_DEF_3(KEYCODE_F12, SEQCODE_NOT, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_RECORD_MOVIE,     "Record Movie",		SEQ_DEF_2(KEYCODE_F12, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_TOGGLE_CHEAT,     "Toggle Cheat",		SEQ_DEF_1(KEYCODE_F6) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_CHEAT,            "Show Cheat List",	SEQ_DEF_0 )
#ifdef CMD_LIST
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_COMMAND,          "Show Command",		SEQ_DEF_0 )
#endif /* CMD_LIST */
#ifdef USE_SHOW_TIME
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_TIME,             "Show Current Time",	SEQ_DEF_0 )
#endif /* USE_SHOW_TIME */
#ifdef USE_SHOW_INPUT_LOG
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_SHOW_INPUT_LOG,   "Show Button Input",	SEQ_DEF_0 )
#endif /* USE_SHOW_INPUT_LOG */
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_UP,               "UI Up",				SEQ_DEF_3(KEYCODE_UP, SEQCODE_OR, INDEXED(JOYCODE_Y_UP_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_DOWN,             "UI Down",				SEQ_DEF_3(KEYCODE_DOWN, SEQCODE_OR, INDEXED(JOYCODE_Y_DOWN_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_LEFT,             "UI Left",				SEQ_DEF_3(KEYCODE_LEFT, SEQCODE_OR, INDEXED(JOYCODE_X_LEFT_SWITCH, 0)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_RIGHT,            "UI Right",			SEQ_DEF_3(KEYCODE_RIGHT, SEQCODE_OR, INDEXED(JOYCODE_X_RIGHT_SWITCH, 0)) )
#ifdef KAILLERA
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_HOME,             "UI Home",				SEQ_DEF_2(KEYCODE_HOME, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_END,              "UI End",				SEQ_DEF_2(KEYCODE_END, KEYCODE_LSHIFT) )
#else
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_HOME,             "UI Home",				SEQ_DEF_1(KEYCODE_HOME) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_END,              "UI End",				SEQ_DEF_1(KEYCODE_END) )
#endif /* KAILLERA */
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_PAGE_UP,          "UI Page Up",			SEQ_DEF_1(KEYCODE_PGUP) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_PAGE_DOWN,        "UI Page Down",		SEQ_DEF_1(KEYCODE_PGDN) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_SELECT,           "UI Select",			SEQ_DEF_3(KEYCODE_ENTER, SEQCODE_OR, INDEXED(JOYCODE_BUTTON1, 0)) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_CANCEL,           "UI Cancel",			SEQ_DEF_1(KEYCODE_ESC) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_CLEAR,            "UI Clear",			SEQ_DEF_1(KEYCODE_DEL) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_ZOOM_IN,          "UI Zoom In",			SEQ_DEF_1(KEYCODE_EQUALS) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_ZOOM_OUT,         "UI Zoom Out",			SEQ_DEF_1(KEYCODE_MINUS) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_PREV_GROUP,       "UI Previous Group",	SEQ_DEF_1(KEYCODE_OPENBRACE) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_NEXT_GROUP,       "UI Next Group",		SEQ_DEF_1(KEYCODE_CLOSEBRACE) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_ROTATE,           "UI Rotate",			SEQ_DEF_1(KEYCODE_R) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_SHOW_PROFILER,    "Show Profiler",		SEQ_DEF_2(KEYCODE_F11, KEYCODE_LSHIFT) )
#ifdef MESS
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_TOGGLE_UI,        "UI Toggle",			SEQ_DEF_1(KEYCODE_SCRLOCK) )
#endif
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_TOGGLE_DEBUG,     "Toggle Debugger",		SEQ_DEF_1(KEYCODE_F5) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_SAVE_STATE,       "Save State",			SEQ_DEF_2(KEYCODE_F7, KEYCODE_LSHIFT) )
#ifdef KAILLERA
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_LOAD_STATE,       "Load State",			SEQ_DEF_2(KEYCODE_F7, KEYCODE_RSHIFT) )
#else
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_LOAD_STATE,       "Load State",			SEQ_DEF_3(KEYCODE_F7, SEQCODE_NOT, KEYCODE_LSHIFT) )
#endif /* KAILLERA */
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_ADD_CHEAT,        "Add Cheat",			SEQ_DEF_1(KEYCODE_A) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_DELETE_CHEAT,     "Delete Cheat",		SEQ_DEF_1(KEYCODE_D) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_SAVE_CHEAT,       "Save Cheat",			SEQ_DEF_1(KEYCODE_S) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_WATCH_VALUE,      "Watch Value",			SEQ_DEF_1(KEYCODE_W) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_EDIT_CHEAT,       "Edit Cheat",			SEQ_DEF_1(KEYCODE_E) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_RELOAD_CHEAT,     "Reload Database",		SEQ_DEF_1(KEYCODE_L) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      UI_TOGGLE_CROSSHAIR, "Toggle Crosshair",	SEQ_DEF_1(KEYCODE_F1) )

#ifdef KAILLERA
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_PLAYER_INC,	"Kaiilera Player Shift Up",			SEQ_DEF_2(KEYCODE_F5, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_PLAYER_DEC,	"Kaillera Player Shift Down",		SEQ_DEF_2(KEYCODE_F6, KEYCODE_LSHIFT) )
	//INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_PLAYER_2IN,	"Unused",						SEQ_DEF_0 )
	//INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_PLAYER_POS,	"Unused",						SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_PLAYER_DUP,	"Kaillera Duplicate Position",		SEQ_DEF_2(KEYCODE_F4, KEYCODE_LSHIFT) )

	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST_1,		"Kaillera Auto-save",				SEQ_DEF_2(KEYCODE_F8, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST_2,		"Kaillera time level change",		SEQ_DEF_3(KEYCODE_F9, SEQCODE_NOT, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST_3,		"Kaillera Version Check",			SEQ_DEF_4(KEYCODE_F10, KEYCODE_LSHIFT, SEQCODE_NOT, KEYCODE_RSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST_4,		"Kaillera Send Dipswitches",		SEQ_DEF_2(KEYCODE_F3, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST_7,		"Killera Select Save Slot",			SEQ_DEF_2(KEYCODE_F9, KEYCODE_LSHIFT) )
	//INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST_8,		"Unused",							SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST_9,		"Killera Send Save Data",			SEQ_DEF_3(KEYCODE_F10, KEYCODE_LSHIFT, KEYCODE_RSHIFT) )

	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST1_0,	"Kaillera Overclock",					SEQ_DEF_2(KEYCODE_F11, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST1_1,	"Unused",							SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST1_2,	"Kaillera Sync Check",				SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST1_3,	"Unused",							SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST2_0,	"Unused",							SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST2_1,	"Unused",							SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST2_2,	"Unused",							SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST2_3,	"Unused",							SEQ_DEF_0 )

	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST1_4,	"Unused",							SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST1_5,	"Unused",							SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST1_6,	"Unused",							SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST1_7,	"Unused",							SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_KAILLERA_TEST1_9,	"Clear Chat Log",					SEQ_DEF_2(KEYCODE_BACKSPACE, KEYCODE_LSHIFT) )

	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_CHAT_OPEN,			"Kaillera Chat Open",				SEQ_DEF_3(KEYCODE_HOME, SEQCODE_NOT, KEYCODE_LSHIFT) )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_CHAT_CHANGE_MODE, 	"Kaillera Chat Change Draw Mode",	SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_CHAT_SHOW_LOG,		"Kaillera Chat Show Chat Log",		SEQ_DEF_3(KEYCODE_END, SEQCODE_NOT, KEYCODE_LSHIFT) )
#endif /* KAILLERA */
#ifdef MAME_AVI
	INPUT_PORT_DIGITAL_DEF( 0, IPG_SPECIAL,      UI_RECORD_AVI,     	"AVI Record",						SEQ_DEF_0 )
#endif /* MAME_AVI */

	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_1,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_2,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_3,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_4,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_5,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_6,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_7,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_8,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_9,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_10,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_11,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_12,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_13,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_14,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_15,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_UI,      OSD_16,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_INVALID, UNKNOWN,			NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_INVALID, SPECIAL,			NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_INVALID, OTHER,				NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_INVALID, DIPSWITCH_NAME,		NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_INVALID, CONFIG_NAME,		NULL,					SEQ_DEF_0 )
	INPUT_PORT_DIGITAL_DEF( 0, IPG_INVALID, END,				NULL,					SEQ_DEF_0 )
};


static input_port_default_entry default_ports[ARRAY_LENGTH(default_ports_builtin)];
static input_port_default_entry default_ports_backup[ARRAY_LENGTH(default_ports_builtin)];
static const int input_port_count = ARRAY_LENGTH(default_ports_builtin);
static int default_ports_lookup[__ipt_max][MAX_PLAYERS];



/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

static void setup_playback(running_machine *machine);
static void setup_record(running_machine *machine);
#ifdef KAILLERA
static void setup_playbacksub(running_machine *machine);
static void setup_recordsub(running_machine *machine);
#endif /* KAILLERA */
static void input_port_exit(running_machine *machine);
static void input_port_load(int config_type, xml_data_node *parentnode);
static void input_port_save(int config_type, xml_data_node *parentnode);
static void update_digital_joysticks(void);
static void update_analog_port(int port);
static void interpolate_analog_port(int port);
static void autoselect_device(const input_port_entry *ipt, int type1, int type2, int type3, const char *option, const char *ananame);

static int auto_pressed(input_bit_info *info);


/***************************************************************************
    CORE IMPLEMENTATION
***************************************************************************/

/*************************************
 *
 *  Input port initialize
 *
 *************************************/

void input_port_init(running_machine *machine, const input_port_token *ipt)
{
	int ipnum, player;

	/* add an exit callback */
	add_exit_callback(machine, input_port_exit);

	for (player = 0; player < MAX_PLAYERS; player++)
		autofiredelay[player] = 1;

#ifdef USE_CUSTOM_BUTTON
	memset(custom_button, 0, sizeof(custom_button));
#endif /* USE_CUSTOM_BUTTON */

	/* start with the raw defaults and ask the OSD to customize them in the backup array */
	memcpy(default_ports_backup, default_ports_builtin, sizeof(default_ports_backup));
	osd_customize_inputport_list(default_ports_backup);

	/* propogate these changes forward to the final input list */
	memcpy(default_ports, default_ports_backup, sizeof(default_ports));

	/* make a lookup table mapping type/player to the default port list entry */
	for (ipnum = 0; ipnum < __ipt_max; ipnum++)
		for (player = 0; player < MAX_PLAYERS; player++)
			default_ports_lookup[ipnum][player] = -1;
	for (ipnum = 0; default_ports[ipnum].type != IPT_END; ipnum++)
		default_ports_lookup[default_ports[ipnum].type][default_ports[ipnum].player] = ipnum;

	/* reset the port info */
	memset(port_info, 0, sizeof(port_info));

	/* if we have inputs, process them now */
	if (ipt != NULL)
	{
		const char *joystick_map_default = options_get_string(mame_options(), OPTION_JOYSTICK_MAP);
		input_port_entry *port;
		int portnum;

		/* allocate input ports */
		machine->input_ports = input_port_allocate(ipt, NULL);

		/* allocate default input ports */
		input_ports_default = input_port_allocate(ipt, NULL);

		/* handle autoselection of devices */
		autoselect_device(machine->input_ports, IPT_PADDLE,      IPT_PADDLE_V,     0,              OPTION_PADDLE_DEVICE,     "paddle");
		autoselect_device(machine->input_ports, IPT_AD_STICK_X,  IPT_AD_STICK_Y,   IPT_AD_STICK_Z, OPTION_ADSTICK_DEVICE,    "analog joystick");
		autoselect_device(machine->input_ports, IPT_LIGHTGUN_X,  IPT_LIGHTGUN_Y,   0,              OPTION_LIGHTGUN_DEVICE,   "lightgun");
		autoselect_device(machine->input_ports, IPT_PEDAL,       IPT_PEDAL2,       IPT_PEDAL3,     OPTION_PEDAL_DEVICE,      "pedal");
		autoselect_device(machine->input_ports, IPT_DIAL,        IPT_DIAL_V,       0,              OPTION_DIAL_DEVICE,       "dial");
		autoselect_device(machine->input_ports, IPT_TRACKBALL_X, IPT_TRACKBALL_Y,  0,              OPTION_TRACKBALL_DEVICE,  "trackball");
		autoselect_device(machine->input_ports, IPT_POSITIONAL,  IPT_POSITIONAL_V, 0,              OPTION_POSITIONAL_DEVICE, "positional");
		autoselect_device(machine->input_ports, IPT_MOUSE_X,     IPT_MOUSE_Y,      0,              OPTION_MOUSE_DEVICE,      "mouse");

		/* look for 4-way joysticks and change the default map if we find any */
		if (joystick_map_default[0] == 0 || strcmp(joystick_map_default, "auto") == 0)
			for (port = machine->input_ports; port->type != IPT_END; port++)
				if (IS_DIGITAL_JOYSTICK(port) && port->way == 4)
				{
					input_device_set_joystick_map(-1, port->rotated ? joystick_map_4way_diagonal : joystick_map_4way_sticky);
					break;
				}

		/* identify all the tagged ports up front so the memory system can access them */
		portnum = 0;
		for (port = machine->input_ports; port->type != IPT_END; port++)
			if (port->type == IPT_PORT)
				port_info[portnum++].tag = port->start.tag;

		/* look up all the tags referenced in conditions */
		for (port = machine->input_ports; port->type != IPT_END; port++)
			if (port->condition.tag)
			{
				int tag = port_tag_to_index(port->condition.tag);
				if (tag == -1)
					fatalerror("Conditional port references invalid tag '%s'", port->condition.tag);
				port->condition.portnum = tag;
			}
	}

	/* register callbacks for when we load configurations */
	config_register("input", input_port_load, input_port_save);

	/* open playback and record files if specified */
	setup_playback(machine);
	setup_record(machine);
#ifdef KAILLERA
	setup_playbacksub(machine);
	setup_recordsub(machine);
#endif /* KAILLERA */
}


/*************************************
 *
 *  Set up for playback
 *
 *************************************/

static void setup_playback(running_machine *machine)
{
	const char *filename = options_get_string(mame_options(), OPTION_PLAYBACK);
	inp_header inpheader;
	file_error filerr;

	struct ext_header xheader;
	char check[7];

	/* if no file, nothing to do */
	if (filename[0] == 0)
		return;

	filerr = FILERR_FAILURE;
	if (strlen(filename) > 4 && mame_stricmp(filename + strlen(filename) - 4, ".zip") == 0)
	{
		char *fname = mame_strdup(filename);

		if (fname)
		{
			strcpy(fname + strlen(fname) - 4, ".inp");
			filerr = mame_fopen(SEARCHPATH_INPUTLOG, fname, OPEN_FLAG_READ, &machine->playback_file);
			free(fname);
		}
	}
	if (filerr != FILERR_NONE)
	/* open the playback file */
	filerr = mame_fopen(SEARCHPATH_INPUTLOG, filename, OPEN_FLAG_READ, &machine->playback_file);
	assert_always(filerr == FILERR_NONE, "Failed to open file for playback");

	// read first four bytes to check INP type
	mame_fread(Machine->playback_file, check, 7);
	mame_fseek(Machine->playback_file, 0, SEEK_SET);

	/* Check if input file is an eXtended INP file */
	if (strncmp(check,"XINP\0\0\0",7) != 0)
	{
		no_extended_inp = 1;
		mame_printf_info(_("This INP file is not an extended INP file, extra info not available\n"));

		/* read playback header */
		mame_fread(machine->playback_file, &inpheader, sizeof(inpheader));

		/* if the first byte is not alphanumeric, it's an old INP file with no header */
		if (!isalnum(inpheader.name[0]))
			mame_fseek(machine->playback_file, 0, SEEK_SET);

		/* else verify the header against the current game */
		else if (strcmp(machine->gamedrv->name, inpheader.name) != 0)
			fatalerror(_("Input file is for " GAMENOUN " '%s', not for current " GAMENOUN " '%s'\n"), inpheader.name, machine->gamedrv->name);

		/* otherwise, print a message indicating what's happening */
		else
			mame_printf_info(_("Playing back previously recorded " GAMENOUN " %s\n"), machine->gamedrv->name);
	}
 	else
	{
		no_extended_inp = 0;
		mame_printf_info(_("Extended INP file info:\n"));

		// read header
		mame_fread(Machine->playback_file, &xheader, sizeof(struct ext_header));

		// output info to console
		mame_printf_info(_("Version string: %s\n"),xheader.version);
		mame_printf_info(_("Start time: %s\n"), ctime((const time_t *)&xheader.starttime));

		// verify header against current game
		if (strcmp(machine->gamedrv->name, xheader.shortname) != 0)
			fatalerror(_("Input file is for " GAMENOUN " '%s', not for current " GAMENOUN " '%s'\n"), xheader.shortname, machine->gamedrv->name);
		else
			mame_printf_info(_("Playing back previously recorded " GAMENOUN " %s\n"), machine->gamedrv->name);
	}

#ifdef INP_CAPTION
	if (strlen(filename) > 4)
	{
		char *fname = mame_strdup(filename);

		if (fname)
		{
			strcpy(fname + strlen(fname) - 4, ".cap");
			mame_fopen(SEARCHPATH_INPUTLOG, fname, OPEN_FLAG_READ, &machine->caption_file);
			free(fname);
		}
	}
#endif /* INP_CAPTION */
}

#ifdef KAILLERA
static void setup_playbacksub(running_machine *machine)
{
	extern char Trace_filename[];
	const char *filename = options_get_string(mame_options(), OPTION_PLAYBACKSUB);
	file_error filerr;

	/* if no file, nothing to do */
	if (filename == NULL || filename[0] == 0)
		return;

	// open the playbacksub file
	filerr = mame_fopen(SEARCHPATH_INPUTLOG, filename, OPEN_FLAG_READ, &machine->playback_sub_file);
 	assert_always(filerr == FILERR_NONE, "Failed to open file for playbacksub");
	strcpy(Trace_filename, filename);

	// read playback header
	{
		inpsub_header inpsub_header;
		char option[512];

		/* read playback header */
		mame_fread(machine->playback_sub_file, &inpsub_header, sizeof(inpsub_header));

		mame_fread(machine->playback_sub_file, option, 512);

		mame_fread(machine->playback_sub_file, (short*)&perform_ui_count, 2);
		perform_ui_count = ((perform_ui_count&0xff)<<8) | ((perform_ui_count&0xff00)>>8);
	}
}
#endif /* KAILLERA */


/*************************************
 *
 *  Set up for recording
 *
 *************************************/

static void setup_record(running_machine *machine)
{
	const char *filename = options_get_string(mame_options(), OPTION_RECORD);
	inp_header inpheader;
	file_error filerr;

	/* if no file, nothing to do */
	if (filename[0] == 0)
		return;

	/* open the record file  */
	filerr = mame_fopen(SEARCHPATH_INPUTLOG, filename, OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS, &machine->record_file);
	assert_always(filerr == FILERR_NONE, "Failed to open file for recording");

	/* create a header */
	memset(&inpheader, 0, sizeof(inpheader));
	strcpy(inpheader.name, machine->gamedrv->name);
	mame_fwrite(machine->record_file, &inpheader, sizeof(inpheader));
}

#ifdef KAILLERA
static void setup_recordsub(running_machine *machine)
{
	extern char Trace_filename[];
	const char *filename = options_get_string(mame_options(), OPTION_RECORDSUB);
	file_error filerr;

	/* if no file, nothing to do */
	if (filename == NULL || filename[0] == 0)
		return;

	// open the recordsub file
	filerr = mame_fopen(SEARCHPATH_INPUTLOG, filename, OPEN_FLAG_WRITE | OPEN_FLAG_CREATE, &machine->record_sub_file);
	assert_always(filerr == FILERR_NONE, "Failed to open file for recording sub");
	strcpy(Trace_filename, filename);

	// create a header
	{
		extern int mame32_PlayGameCount;
		inpsub_header inpsub_header;
		char option[512];

		memset(&inpsub_header, '\0', sizeof(inpsub_header));
		strcpy(inpsub_header.name, machine->gamedrv->name);
		strcpy(inpsub_header.str, "EMMAMETRACE");
		inpsub_header.trcversion = INPUTLOG_TRACE_VERSION;
	
		strcpy(inpsub_header.name, machine->gamedrv->name);
		sprintf(inpsub_header.version,"%s %s", MAMENAME "++", build_version);
		
		inpsub_header.playcount = (unsigned char)mame32_PlayGameCount;
		mame_fwrite(machine->record_sub_file, &inpsub_header, sizeof(inpsub_header));

		memset(option, 0, sizeof(option));
		mame_fwrite(machine->record_sub_file, option, 512);

	}
}
#endif /* KAILLERA */



/*************************************
 *
 *  Input port exit
 *
 *************************************/

void input_port_exit(running_machine *machine)
{
	/* close any playback or recording files */
#ifdef INP_CAPTION
	if (machine->caption_file != NULL)
		mame_fclose(machine->caption_file);
	machine->caption_file = NULL;
#endif /* INP_CAPTION */
#ifdef KAILLERA
	if (machine->playback_sub_file != NULL)
		mame_fclose(machine->playback_sub_file);
	machine->playback_sub_file = NULL;
	if (machine->record_sub_file != NULL)
		mame_fclose(machine->record_sub_file);
	machine->record_sub_file = NULL;
#endif /* KAILERA */
	if (machine->playback_file != NULL)
		mame_fclose(machine->playback_file);
	machine->playback_file = NULL;
	if (machine->record_file != NULL)
		mame_fclose(machine->record_file);
	machine->record_file = NULL;
}



/*************************************
 *
 *  Input port initialization
 *
 *************************************/

static void input_port_postload(void)
{
	input_port_entry *port;
	int portnum, bitnum;
	UINT32 mask;

	/* reset the pointers */
	memset(&joystick_info, 0, sizeof(joystick_info));
#ifdef USE_CUSTOM_BUTTON
	memset(custom_button_info, 0, sizeof(custom_button_info));
#endif /* USE_CUSTOM_BUTTON */

	/* loop over the ports and identify all the analog inputs */
	portnum = -1;
	bitnum = 0;
	for (port = Machine->input_ports; port->type != IPT_END; port++)
	{
		/* if this is IPT_PORT, increment the port number */
		if (port->type == IPT_PORT)
		{
			portnum++;
			bitnum = 0;
		}

		/* if this is not a DIP setting or config setting, add it to the list */
		else if (port->type != IPT_DIPSWITCH_SETTING && port->type != IPT_CONFIG_SETTING)
		{
			/* fatal error if we didn't hit an IPT_PORT */
			if (portnum < 0)
				fatalerror("Error in InputPort definition: expecting PORT_START");

			/* fatal error if too many bits */
			if (bitnum >= MAX_BITS_PER_PORT)
				fatalerror("Error in InputPort definition: too many bits for a port (%d max)", MAX_BITS_PER_PORT);

			if (port->type == IPT_TOGGLE_AUTOFIRE)
				autofire_toggle_port = &port_info[portnum];

#ifdef USE_CUSTOM_BUTTON
			else if (port->type >= IPT_CUSTOM1 && port->type < IPT_CUSTOM1 + custom_buttons)
				custom_button_info[port->player][port->type - IPT_CUSTOM1] = &port_info[portnum].bit[bitnum];
#endif /* USE_CUSTOM_BUTTON */

			/* fill in the bit info */
			port_info[portnum].bit[bitnum].port = port;
			port_info[portnum].bit[bitnum].impulse = 0;
			port_info[portnum].bit[bitnum++].last = 0;

			/* if this is a custom input, add it to the list */
			if (port->custom != NULL)
			{
				custom_port_info *info;

				/* allocate memory */
				info = auto_malloc(sizeof(*info));
				memset(info, 0, sizeof(*info));

				/* fill in the data */
				info->port = port;
				for (mask = port->mask; !(mask & 1); mask >>= 1)
					info->shift++;

				/* hook in the list */
				info->next = port_info[portnum].custominfo;
				port_info[portnum].custominfo = info;
			}

			/* if this is an analog port, create an info struct for it */
			else if (IS_ANALOG(port))
			{
				analog_port_info *info;

				/* allocate memory */
				info = auto_malloc(sizeof(*info));
				memset(info, 0, sizeof(*info));

				/* fill in the data */
				info->port = port;
				for (mask = port->mask; !(mask & 1); mask >>= 1)
					info->shift++;
				for ( ; mask & 1; mask >>= 1)
					info->bits++;

				/* based on the port type determine if we need absolute or relative coordinates */
				info->minimum = INPUT_ABSOLUTE_MIN;
				info->maximum = INPUT_ABSOLUTE_MAX;
				info->interpolate = 1;

				/* adjust default, min, and max so they fall in the bitmask range */
				port->default_value = (port->default_value & port->mask) >> info->shift;
				if (port->type != IPT_POSITIONAL && port->type != IPT_POSITIONAL_V)
				{
					port->analog.min = (port->analog.min & port->mask) >> info->shift;
					port->analog.max = (port->analog.max & port->mask) >> info->shift;
				}

				switch (port->type)
				{
					/* pedals start at and autocenter to the min range*/
					case IPT_PEDAL:
					case IPT_PEDAL2:
					case IPT_PEDAL3:
						info->center = INPUT_ABSOLUTE_MIN;
						/* force pedals to start at their scaled minimum */
						info->accum = APPLY_INVERSE_SENSITIVITY(info->center, port->analog.sensitivity);
						/* fall through to complete setup */

					/* pedals, paddles and analog joysticks are absolute and autocenter */
					case IPT_AD_STICK_X:
					case IPT_AD_STICK_Y:
					case IPT_AD_STICK_Z:
					case IPT_PADDLE:
					case IPT_PADDLE_V:
						info->absolute = 1;
						info->autocenter = 1;
						break;

					/* lightguns are absolute as well, but don't autocenter and don't interpolate their values */
					case IPT_LIGHTGUN_X:
					case IPT_LIGHTGUN_Y:
						info->absolute = 1;
						info->interpolate = 0;
						break;

					/* dials, mice and trackballs are relative devices */
					/* these have fixed "min" and "max" values based on how many bits are in the port */
					/* in addition, we set the wrap around min/max values to 512 * the min/max values */
					/* this takes into account the mapping that one mouse unit ~= 512 analog units */
					case IPT_DIAL:
					case IPT_DIAL_V:
					case IPT_MOUSE_X:
					case IPT_MOUSE_Y:
					case IPT_TRACKBALL_X:
					case IPT_TRACKBALL_Y:
						info->absolute = 0;
						info->wraps = 1;
						break;

					/* positional devices are abolute, but can also wrap like relative devices */
					/* set each position to be 512 units */
					case IPT_POSITIONAL:
					case IPT_POSITIONAL_V:
						info->positionalscale = (double)(port->analog.max) / (INPUT_ABSOLUTE_MAX - INPUT_ABSOLUTE_MIN);
						/* force to only use PORT_POSITIONS data */
						port->analog.min = 0;
						port->analog.max--;
						info->autocenter = !port->analog.wraps;
						info->wraps = port->analog.wraps;
						break;

					default:
						fatalerror("Unknown analog port type -- don't know if it is absolute or not");
						break;
				}

				if (info->absolute)
				{
					/* if we are receiving data from the OSD input as absolute,
                     * and the emulated port type is absolute,
                     * and the default port value is between min/max,
                     * we need to scale differently for the +/- directions.
                     * All other absolute types use a 1:1 scale */
					info->single_scale = (port->default_value == port->analog.min) || (port->default_value == port->analog.max);

					if (!info->single_scale)
					{
						/* axis moves in both directions from the default value */

						/* unsigned */
						info->scalepos = ((double)port->analog.max - (double)port->default_value) / (double)(INPUT_ABSOLUTE_MAX - 0);
						info->scaleneg = ((double)port->default_value - (double)port->analog.min) / (double)(0 - INPUT_ABSOLUTE_MIN);

						if (port->analog.min > port->analog.max)
						{
							/* signed */
							info->scaleneg *= -1;
						}

						/* reverse from center */
						info->reverse_val = 0;
					}
					else
					{
						/* single axis that increases from default */
						info->scalepos = (double)(port->analog.max - port->analog.min) / (double)(INPUT_ABSOLUTE_MAX - INPUT_ABSOLUTE_MIN);

						/* move from default */
						if (port->default_value == port->analog.max)
							info->scalepos *= -1;

						/* make the scaling the same for easier coding when we need to scale */
						info->scaleneg = info->scalepos;

						/* reverse from max */
						info->reverse_val = info->maximum;
					}
				}

				/* relative and positional controls all map directly with a 512x scale factor */
				else
				{
					/* The relative code is set up to allow specifing PORT_MINMAX and default values. */
					/* The validity checks are purposely set up to not allow you to use anything other */
					/* a default of 0 and PORT_MINMAX(0,mask).  This is in case the need arises to use */
					/* this feature in the future.  Keeping the code in does not hurt anything. */
					if (port->analog.min > port->analog.max)
						/* adjust for signed */
						port->analog.min *= -1;

					info->minimum = (port->analog.min - port->default_value) * 512;
					info->maximum = (port->analog.max - port->default_value) * 512;

					/* make the scaling the same for easier coding when we need to scale */
					info->scaleneg = info->scalepos = 1.0 / 512.0;

					if (port->analog.reset)
						/* delta values reverse from center */
						info->reverse_val = 0;
					else
					{
						/* positional controls reverse from their max range */
						info->reverse_val = info->maximum + info->minimum;

						/* relative controls reverse from 1 past their max range */
						if (info->positionalscale == 0) info->reverse_val += 512;
					}
				}

				/* compute scale for keypresses */
				info->keyscalepos = 1.0 / info->scalepos;
				info->keyscaleneg = 1.0 / info->scaleneg;

				/* hook in the list */
				info->next = port_info[portnum].analoginfo;
				port_info[portnum].analoginfo = info;
			}

			/* if this is a digital joystick port, update info on it */
			else if (IS_DIGITAL_JOYSTICK(port))
			{
				digital_joystick_info *info = JOYSTICK_INFO_FOR_PORT(port);
				info->port[JOYSTICK_DIR_FOR_PORT(port)] = port;
				info->inuse = 1;
			}
		}
	}

#ifdef KAILLERA
	if (kPlay)
	{
		input_port_entry *in;
		int port;
		int KAnalogCtrl = -1;	//kt	Kaillera時のアナログコントロール使用許可
		int Dipswitch_ServiceMode = -1;	//kt
		int is6button = 0;

		KailleraStartOption.gameinputmin = 0;
		// Sega
		if (!mame_stricmp(Machine->gamedrv->name, "gground"))		KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "ga2j4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ga2u"))		KailleraStartOption.gameinputmin = 4;

		// Namco
		if (!mame_stricmp(Machine->gamedrv->name, "numanath"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "numanatj"))		KailleraStartOption.gameinputmin = 4;

		// Copcom
		if (!mame_stricmp(Machine->gamedrv->name, "captcomm3p"))		KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "captcomu3p"))		KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "captcomj3p"))		KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "captcomm4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "captcomu4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "captcomj4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "avsp3p"))			KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "avspu3p"))			KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "avspj3p"))			KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "avspa3p"))			KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "batcir4p"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "batcirj4p"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddsom4p"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddsomr2_4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddsomu4p"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddsomur1_4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddsomj4p"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddsomjr1_4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddsoma4p"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddtod"))				KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddtodr1"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddtodh"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddtodj"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddtodjr1"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddtodu"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ddtodur1"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "armwarr1"))			KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "armwaru"))			KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "pgear"))				KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "pgearr1"))			KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "wof"))				KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "wofa"))				KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "wofu"))				KailleraStartOption.gameinputmin = 3;
		if (!mame_stricmp(Machine->gamedrv->name, "wofj"))				KailleraStartOption.gameinputmin = 3;
#ifdef MAME32PLUSPLUS
		if (!mame_stricmp(Machine->gamedrv->name, "xmvsfregion4p"))		KailleraStartOption.gameinputmin = 4;
#endif /* MAME32PLUSPLUS */
		if (!mame_stricmp(Machine->gamedrv->name, "mshvsfj4p"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "mvscj4p"))			KailleraStartOption.gameinputmin = 4;

		// Neo-Geo
		if (!mame_stricmp(Machine->gamedrv->name, "lbowling4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "kof98_6p"))			KailleraStartOption.gameinputmin = 6;
		if (!mame_stricmp(Machine->gamedrv->name, "kof95_6p"))			KailleraStartOption.gameinputmin = 6;

		// Konami
		if (!mame_stricmp(Machine->gamedrv->name, "hyprolym4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "hyperspt4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "hpolym84_4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ssriders"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ssrdradd"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ssrdreaa"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ssrdruac"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "ssrdruda"))			KailleraStartOption.gameinputmin = 4;

		// Deco
		if (!mame_stricmp(Machine->gamedrv->name, "thndzone"))			KailleraStartOption.gameinputmin = 4;

#if 1
		// Psikyo
		if (!mame_stricmp(Machine->gamedrv->name, "hotgmck_k"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "hgkairak_k"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "hotgmck3_k"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "hotgm4ev_k"))		KailleraStartOption.gameinputmin = 4;

		// Video System Co.
		if (!mame_stricmp(Machine->gamedrv->name, "fromanc2_k"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "fromancr_k"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "fromanc4_k"))			KailleraStartOption.gameinputmin = 4;
#endif

		// Taito
		if (!mame_stricmp(Machine->gamedrv->source_file+17, "taito_f3.c")) KAnalogCtrl = 0;

		if (!mame_stricmp(Machine->gamedrv->name, "arabianm4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "arabiamj4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "arabiamu4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "dungeonm4p"))		KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "lightbr4p"))			KailleraStartOption.gameinputmin = 4;
		if (!mame_stricmp(Machine->gamedrv->name, "dungenmu4p"))		KailleraStartOption.gameinputmin = 4;

		memset(&kBitsPlayValues,-1,sizeof(kBitsPlayValues));
		memset(&kMahjongPlayValues,-1,sizeof(kMahjongPlayValues));	//kt
		memset(&kDipSwitchValues,-1,sizeof(kDipSwitchValues));		//kt
		memset(&kAnalogPlayValues,-1,sizeof(kAnalogPlayValues));	//kt
		kAnalog_input_port_clear();
		kMaxDipSwitch	= 0;	//kt
		kMaxMahjong		= 0;	//kt

		in = Machine->input_ports;
		in++;

		port = 0;
		while (in->type != IPT_END && port < MAX_INPUT_PORTS)
		{
			while (in->type != IPT_END && in->type != IPT_PORT)
			{
				int player=-1,pos=-1;
				int analog=-1,dipswitch=-1,mahjong=-1; //kt

				if (in->player == 0) player=0;
				if (in->player == 1) player=1;
				if (in->player == 2) player=2;
				if (in->player == 3) player=3;

				switch(in->type)
				{
				case IPT_BUTTON1:						pos=0; break;
				case IPT_BUTTON2:						pos=1; break;
				case IPT_BUTTON3:						pos=2; break;
				case IPT_BUTTON4:						pos=3; break;
				case IPT_BUTTON5:						pos=4; break;
				case IPT_BUTTON6:						pos=5; is6button=1; break;
				case IPT_BUTTON7:						pos=6; is6button=1; break;
				case IPT_JOYSTICK_UP:					pos=7; break;
				case IPT_JOYSTICK_DOWN:					pos=8; break;
				case IPT_JOYSTICK_LEFT:					pos=9; break;
				case IPT_JOYSTICK_RIGHT:				pos=10; break;

				case IPT_COIN1:				player=0;	pos=11; break;
				case IPT_START1:			player=0;	pos=12; break;

				case IPT_COIN2:				player=1;	pos=11; break;
				case IPT_START2:			player=1;	pos=12; break;

				case IPT_COIN3:				player=2;	pos=11; break;
				case IPT_START3:			player=2;	pos=12; break;

				case IPT_COIN4:				player=3;	pos=11; break;
				case IPT_START4:			player=3;	pos=12; break;

				case IPT_UI_SOFT_RESET:		player=0;	pos=13; break;
				case IPT_SERVICE:			player=0;	pos=14; break;
				case IPT_SERVICE1:			player=0;	pos=15; break;

				case IPT_JOYSTICKRIGHT_UP:				pos= 7; break;
				case IPT_JOYSTICKRIGHT_DOWN:			pos= 8; break;
				case IPT_JOYSTICKRIGHT_LEFT:			pos= 9; break;
				case IPT_JOYSTICKRIGHT_RIGHT:			pos=10; break;

				case IPT_JOYSTICKLEFT_UP:				pos= 3; break;
				case IPT_JOYSTICKLEFT_DOWN:				pos= 4; break;
				case IPT_JOYSTICKLEFT_LEFT:				pos= 5; break;
				case IPT_JOYSTICKLEFT_RIGHT:			pos= 6; break;

				case IPT_MAHJONG_A:				pos =  0; mahjong=0; break;
				case IPT_MAHJONG_B:				pos =  1; mahjong=0; break;
				case IPT_MAHJONG_C:				pos =  2; mahjong=0; break;
				case IPT_MAHJONG_D:				pos =  3; mahjong=0; break;
				case IPT_MAHJONG_E:				pos =  4; mahjong=0; break;
				case IPT_MAHJONG_F:				pos =  5; mahjong=0; break;
				case IPT_MAHJONG_G:				pos =  6; mahjong=0; break;
				case IPT_MAHJONG_H:				pos =  7; mahjong=0; break;
				case IPT_MAHJONG_I:				pos =  8; mahjong=0; break;
				case IPT_MAHJONG_J:				pos =  9; mahjong=0; break;
				case IPT_MAHJONG_K:				pos = 10; mahjong=0; break;
				case IPT_MAHJONG_L:				pos = 11; mahjong=0; break;
				case IPT_MAHJONG_M:				pos = 12; mahjong=0; break;
				case IPT_MAHJONG_N:				pos = 13; mahjong=0; break;
				case IPT_MAHJONG_O:				pos = 14; mahjong=0; break;
				case IPT_MAHJONG_P:				pos = 15; mahjong=0; break;
				case IPT_MAHJONG_Q:				pos = 16; mahjong=0; break;
				case IPT_MAHJONG_KAN:			pos = 17; mahjong=0; break;
				case IPT_MAHJONG_PON:			pos = 18; mahjong=0; break;
				case IPT_MAHJONG_CHI:			pos = 19; mahjong=0; break;
				case IPT_MAHJONG_REACH:			pos = 20; mahjong=0; break;
				case IPT_MAHJONG_RON:			pos = 21; mahjong=0; break;
				case IPT_MAHJONG_BET:			pos = 22; mahjong=0; break;
				case IPT_MAHJONG_LAST_CHANCE:	pos = 23; mahjong=0; break;
				case IPT_MAHJONG_SCORE:			pos = 24; mahjong=0; break;
				case IPT_MAHJONG_DOUBLE_UP:		pos = 25; mahjong=0; break;
				case IPT_MAHJONG_FLIP_FLOP:		pos = 26; mahjong=0; break;
				case IPT_MAHJONG_BIG:			pos = 27; mahjong=0; break;
				case IPT_MAHJONG_SMALL:			pos = 28; mahjong=0; break;

				case IPT_PADDLE:				pos =  0; analog=0; break;
				case IPT_PADDLE_V:				pos =  1; analog=0; break;
				case IPT_AD_STICK_X:			pos =  2; analog=0; break;
				case IPT_AD_STICK_Y:			pos =  3; analog=0; break;
				case IPT_AD_STICK_Z:			pos =  4; analog=0; break;
				case IPT_LIGHTGUN_X:			pos =  5; analog=0; break;
				case IPT_LIGHTGUN_Y:			pos =  6; analog=0; break;
				case IPT_PEDAL:					pos =  7; analog=0; break;
				case IPT_PEDAL2:				pos =  8; analog=0; break;
				case IPT_PEDAL3:				pos =  9; analog=0; break;
				case IPT_DIAL:					pos = 10; analog=0; break;
				case IPT_DIAL_V:				pos = 11; analog=0; break;
				case IPT_TRACKBALL_X:			pos = 12; analog=0; break;
				case IPT_TRACKBALL_Y:			pos = 13; analog=0; break;
				case IPT_MOUSE_X:				pos = 14; analog=0; break;
				case IPT_MOUSE_Y:				pos = 15; analog=0; break;

				//case IPT_SERVICE1:								break;
				case IPT_SERVICE2:
					if (!mame_stricmp(Machine->gamedrv->source_file+17, "playch10.c") &&
						!strcmp(in->name, "Channel Select"))	{ player=0;	pos =  3;	break;}
					break;
				case IPT_SERVICE3:
					if (!mame_stricmp(Machine->gamedrv->source_file+17, "playch10.c") &&
						!strcmp(in->name, "Enter"))				{ player=0;	pos =  4;	break;}
					break;
				case IPT_SERVICE4:
					if (!mame_stricmp(Machine->gamedrv->source_file+17, "playch10.c") &&
						!strcmp(in->name, "Reset"))				{ player=0;	pos =  5;	break;}
					break;

				case IPT_OTHER:
					if (!mame_stricmp(Machine->gamedrv->source_file+17, "neogeo.c") ||
						!mame_stricmp(Machine->gamedrv->source_file+17, "neodrvr.c"))
					{
						if (!strcmp(in->name, "Next Game"))		{ player=0;	pos =  4;	break;}
						if (!strcmp(in->name, "Previous Game"))	{ player=1;	pos =  4;	break;}
					}
					break;

				case IPT_DIPSWITCH_NAME:

					if (!mame_stricmp(Machine->gamedrv->source_file+17, "namcona1.c"))
						if (!strcmp(in->name, "Test"))	{ player=0;	pos =  4;	break;}

					if (!strcmp(in->name, input_port_string_from_token((const input_port_token)DEF_STR( Service_Mode ))) &&
						(in->toggle))
					{	//PORT_SERVICE(mask,default)
						player=0;	pos=14;	
						kBitsPlayValues[player][pos][0]=port;
						kBitsPlayValues[player][pos][1]=in->mask;
						if (in->default_value)	kBitsPlayValues[player][pos][2]=IP_ACTIVE_LOW;
						else					kBitsPlayValues[player][pos][2]=IP_ACTIVE_HIGH;
						Dipswitch_ServiceMode = 0;
						player=-1;	pos=-1;
						break;
					}

					player=0;	pos= 0;	dipswitch=0;
					break;
				}

				if (!mame_stricmp(Machine->gamedrv->source_file+17, "fromanc2.c"))
				{
					switch(in->type)
					{
						case IPT_COIN1:				player=0;	pos=11; break;
						case IPT_COIN2:				player=-1;	pos=-1; break;
						case IPT_COIN3:				player=1;	pos=11; break;
						case IPT_COIN4:				player=-1;	pos=-1; break;
					}
				}

				if (!mame_stricmp(Machine->gamedrv->source_file+17, "namcos2.c"))
				{
					//Service Button
					if (player==0 && pos==14 && port<2)	{ player=0; pos=3; }
				}

				if(pos!=-1 && player!=-1)
				{
					if(mahjong!=-1) {
						kMahjongPlayValues[player][pos][0]=port;
						kMahjongPlayValues[player][pos][1]=in->mask;
						kBitsPlayValues[player][pos][2]=in->default_value;
						kMaxMahjong++;

					} else if(dipswitch!=-1) {
						kDipSwitchValues[kMaxDipSwitch][0]=port;
						kDipSwitchValues[kMaxDipSwitch][1]=in->mask;
						kMaxDipSwitch++;

					} else if(analog!=-1) {
						if(KAnalogCtrl) {
							kAnalogPlayValues[player][pos]=port;
							if(player == 0) Kaillera_analog_port++;
						}
						
					} else {
						if (kBitsPlayValues[player][pos][0]==-1)	//	同じボタンが複数ある場合。
						{
							kBitsPlayValues[player][pos][0]=port;
							kBitsPlayValues[player][pos][1]=in->mask;
							if (in->mask != in->default_value)
								kBitsPlayValues[player][pos][2]=in->default_value;
							else
								kBitsPlayValues[player][pos][2]=IP_ACTIVE_LOW;
						} else
						{
							if (kBitsPlayValues[player][pos][0] == port)
							{
								kBitsPlayValues[player][pos][1]|=in->mask;
							} else
							{
								if (kBitsPlayValues[player][pos][0+3]==-1)
								{
									kBitsPlayValues[player][pos][0+3]=port;
									kBitsPlayValues[player][pos][1+3]=in->mask;
									if (in->mask != in->default_value)
										kBitsPlayValues[player][pos][2]=in->default_value;
									else
										kBitsPlayValues[player][pos][2]=IP_ACTIVE_LOW;
								}

							}
						}
					}
				}

				in++;
			}
			port++;
			if (in->type == IPT_PORT) in++;
		}
#if 0
		{
			FILE *fp;
			int i,j;
			fp = fopen("port.txt","a");
			fprintf(fp, "%s%s%s\n\n", "/*-------------- ", Machine->gamedrv->name, " --------------*/");
			for (i=0;i<4;i++)
				for (j=0;j<16;j++)
					if (kBitsPlayValues[i][j][0] != -1)
						fprintf(fp, "(pl:%2d pos:%2d) %08x %08x %08x\n", i,j,kBitsPlayValues[i][j][0],kBitsPlayValues[i][j][1],kBitsPlayValues[i][j][2]);
			fprintf(fp, "\n\n");
			fclose(fp);
		}
#endif
		{
			int i;
			if (is6button && !Kaillera_analog_port && !kMaxMahjong)
				if (kBitsPlayValues[0][15][0]!=-1)
					// clear IPT_SERVICE1
					for (i = 0; i < 5; i++)
						kBitsPlayValues[0][15][i] = -1;
		}
		kFirstMsg=1;
#if 1
		kPlayerDup = 0;
		kValXor = 0;
#else
		{
			int j;
			kPlayerDup = 0;
			kValXor = 0;
			for(j=0; j<16; j++)
			{
				if(kBitsPlayValues[0][j][0]!=-1)
				{
					if(kBitsPlayValues[0][j][2] != IP_ACTIVE_LOW)
						kValXor |= (1<<j);
				}
			}
		}
#endif
	
		{//kt start		未使用ビットを探す。
			int i,j,jj;
			unsigned short mask=0, b=1;
			int *usedkeys = usedkeys_get();
			j=0;jj=0;
			for(i=0; i<16; i++)
			{
				if(kBitsPlayValues[0][i][0]==-1)
				{
					usedkeys[j++] = i;
					if( jj<3 ){
						jj++;
						mask |= b;
					}
				}
				b<<=1;
			}
			usedkeysMask_set(~mask);
			//popmessage("key %x,%x",mask,kValXor );
		}//kt end
	}
	else
	{
		kFirstMsg=1;
	}
#endif /* KAILLERA */


	/* run an initial update */
	input_port_vblank_start();
}



/*************************************
 *
 *  Identifies which port types are
 *  saved/loaded
 *
 *************************************/

static int save_this_port_type(int type)
{
	switch (type)
	{
		case IPT_UNUSED:
		case IPT_END:
		case IPT_PORT:
		case IPT_DIPSWITCH_SETTING:
		case IPT_CONFIG_SETTING:
		case IPT_CATEGORY_SETTING:
		case IPT_VBLANK:
		case IPT_UNKNOWN:
			return 0;
	}
	return 1;
}



/*************************************
 *
 *  Input port configuration read
 *
 *************************************/

INLINE input_code get_default_code(int config_type, int type)
{
	switch (type)
	{
		case IPT_DIPSWITCH_NAME:
		case IPT_CATEGORY_NAME:
			return SEQCODE_END;

		default:
			if (config_type != CONFIG_TYPE_GAME)
				return SEQCODE_END;
			else
				return SEQCODE_DEFAULT;
	}
	return SEQCODE_END;
}


INLINE int string_to_seq_index(const char *string)
{
	int seqindex;

	for (seqindex = 0; seqindex < ARRAY_LENGTH(seqtypestrings); seqindex++)
		if (!mame_stricmp(string, seqtypestrings[seqindex]))
			return seqindex;

	return -1;
}


static void apply_remaps(int count, const input_code *oldtable, const input_code *newtable)
{
	int remapnum;

	/* loop over the remapping table, operating only if something was specified */
	for (remapnum = 0; remapnum < count; remapnum++)
	{
		input_code oldcode = oldtable[remapnum];
		input_code newcode = newtable[remapnum];
		int portnum;

		/* loop over all default ports, remapping the requested keys */
		for (portnum = 0; default_ports[portnum].type != IPT_END; portnum++)
		{
			input_port_default_entry *defport = &default_ports[portnum];
			int seqnum;

			/* remap anything in the default sequences */
			for (seqnum = 0; seqnum < ARRAY_LENGTH(defport->defaultseq.code); seqnum++)
				if (defport->defaultseq.code[seqnum] == oldcode)
					defport->defaultseq.code[seqnum] = newcode;
			for (seqnum = 0; seqnum < ARRAY_LENGTH(defport->defaultdecseq.code); seqnum++)
				if (defport->defaultdecseq.code[seqnum] == oldcode)
					defport->defaultdecseq.code[seqnum] = newcode;
			for (seqnum = 0; seqnum < ARRAY_LENGTH(defport->defaultincseq.code); seqnum++)
				if (defport->defaultincseq.code[seqnum] == oldcode)
					defport->defaultincseq.code[seqnum] = newcode;
		}
	}
}



static int apply_config_to_default(xml_data_node *portnode, int type, int player, input_seq *newseq)
{
	int portnum;

	/* find a matching port in the list */
	for (portnum = 0; portnum < ARRAY_LENGTH(default_ports_backup); portnum++)
	{
		input_port_default_entry *updateport = &default_ports[portnum];
		if (updateport->type == type && updateport->player == player)
		{
			/* copy the sequence(s) that were specified */
			if (input_seq_get_1(&newseq[0]) != INPUT_CODE_INVALID)
				updateport->defaultseq = newseq[0];
			if (input_seq_get_1(&newseq[1]) != INPUT_CODE_INVALID)
				updateport->defaultdecseq = newseq[1];
			if (input_seq_get_1(&newseq[2]) != INPUT_CODE_INVALID)
				updateport->defaultincseq = newseq[2];
			return 1;
		}
	}
	return 0;
}


static int apply_config_to_current(xml_data_node *portnode, int type, int player, input_seq *newseq)
{
	input_port_entry *updateport;
	int mask, defvalue, index;

	/* read the mask, index, and defvalue attributes */
	index = xml_get_attribute_int(portnode, "index", 0);
	mask = xml_get_attribute_int(portnode, "mask", 0);
	defvalue = xml_get_attribute_int(portnode, "defvalue", 0);

	/* find the indexed port; we scan the array to make sure we don't read past the end */
	for (updateport = input_ports_default; updateport->type != IPT_END; updateport++)
		if (index-- == 0)
			break;

	/* verify that it matches */
	if (updateport->type == type && updateport->player == player &&
		updateport->mask == mask && (updateport->default_value & mask) == (defvalue & mask))
	{
		const char *revstring;

		/* point to the real port */
		updateport = Machine->input_ports + (updateport - input_ports_default);

		/* fill in the data from the attributes */
		if (!port_type_is_analog(updateport->type))
			updateport->default_value = xml_get_attribute_int(portnode, "value", updateport->default_value);
		updateport->analog.delta = xml_get_attribute_int(portnode, "keydelta", updateport->analog.delta);
		updateport->analog.centerdelta = xml_get_attribute_int(portnode, "centerdelta", updateport->analog.centerdelta);
		updateport->analog.sensitivity = xml_get_attribute_int(portnode, "sensitivity", updateport->analog.sensitivity);
		revstring = xml_get_attribute_string(portnode, "reverse", NULL);
		if (revstring)
			updateport->analog.reverse = (strcmp(revstring, "yes") == 0);

		if (strcmp(xml_get_attribute_string(portnode, "autofire", "off"), "on") == 0)
			updateport->autofire_setting = AUTOFIRE_ON;
		else if (strcmp(xml_get_attribute_string(portnode, "autofire", "off"), "toggle") == 0)
			updateport->autofire_setting = AUTOFIRE_TOGGLE;
		else
			updateport->autofire_setting = 0;

#ifdef USE_CUSTOM_BUTTON
		if (updateport->type >= IPT_CUSTOM1 && updateport->type < IPT_CUSTOM1 + MAX_CUSTOM_BUTTONS)
			custom_button[updateport->player][updateport->type - IPT_CUSTOM1] = xml_get_attribute_int(portnode, "custom", 0);
#endif /* USE_CUSTOM_BUTTON */

		/* copy the sequence(s) that were specified */
		if (input_seq_get_1(&newseq[0]) != INPUT_CODE_INVALID)
			updateport->seq = newseq[0];
		if (input_seq_get_1(&newseq[1]) != INPUT_CODE_INVALID)
			updateport->analog.decseq = newseq[1];
		if (input_seq_get_1(&newseq[2]) != INPUT_CODE_INVALID)
			updateport->analog.incseq = newseq[2];
		return 1;
	}
	return 0;
}


static void input_port_load(int config_type, xml_data_node *parentnode)
{
	xml_data_node *portnode;
	int seqnum;

	/* in the completion phase, we finish the initialization with the final ports */
	if (config_type == CONFIG_TYPE_FINAL)
		input_port_postload();

	/* early exit if no data to parse */
	if (!parentnode)
		return;

	/* iterate over all the remap nodes for controller configs only */
	if (config_type == CONFIG_TYPE_CONTROLLER)
	{
		input_code *oldtable, *newtable;
		xml_data_node *remapnode;
		int count;

		/* count items first so we can allocate */
		count = 0;
		for (remapnode = xml_get_sibling(parentnode->child, "remap"); remapnode; remapnode = xml_get_sibling(remapnode->next, "remap"))
			count++;

		/* if we have some, deal with them */
		if (count > 0)
		{
			/* allocate tables */
			oldtable = malloc_or_die(count * sizeof(*oldtable));
			newtable = malloc_or_die(count * sizeof(*newtable));

			/* build up the remap table */
			count = 0;
			for (remapnode = xml_get_sibling(parentnode->child, "remap"); remapnode; remapnode = xml_get_sibling(remapnode->next, "remap"))
			{
				input_code origcode = input_code_from_token(xml_get_attribute_string(remapnode, "origcode", ""));
				input_code newcode = input_code_from_token(xml_get_attribute_string(remapnode, "newcode", ""));
				if (origcode != INPUT_CODE_INVALID && newcode != INPUT_CODE_INVALID)
				{
					oldtable[count] = origcode;
					newtable[count] = newcode;
					count++;
				}
			}

			/* apply it then free the tables */
			apply_remaps(count, oldtable, newtable);
			free(oldtable);
			free(newtable);
		}
	}

	/* iterate over all the port nodes */
	for (portnode = xml_get_sibling(parentnode->child, "port"); portnode; portnode = xml_get_sibling(portnode->next, "port"))
	{
		input_seq newseq[3], tempseq;
		xml_data_node *seqnode;
		int type, player;

		/* get the basic port info from the attributes */
		type = token_to_port_type(xml_get_attribute_string(portnode, "type", ""), &player);

		/* initialize sequences to invalid defaults */
		for (seqnum = 0; seqnum < 3; seqnum++)
			input_seq_set_1(&newseq[seqnum], INPUT_CODE_INVALID);

		/* loop over new sequences */
		for (seqnode = xml_get_sibling(portnode->child, "newseq"); seqnode; seqnode = xml_get_sibling(seqnode->next, "newseq"))
		{
			/* with a valid type, parse out the new sequence */
			seqnum = string_to_seq_index(xml_get_attribute_string(seqnode, "type", ""));
			if (seqnum != -1)
				if (seqnode->value != NULL)
				{
					if (strcmp(seqnode->value, "NONE") == 0)
						input_seq_set_0(&newseq[seqnum]);
					else if (input_seq_from_tokens(seqnode->value, &tempseq) != 0)
						newseq[seqnum] = tempseq;
				}
		}

		/* if we're loading default ports, apply to the default_ports */
		if (config_type != CONFIG_TYPE_GAME)
			apply_config_to_default(portnode, type, player, newseq);
		else
			apply_config_to_current(portnode, type, player, newseq);
	}

	if (config_type == CONFIG_TYPE_GAME)
	{
		for (portnode = xml_get_sibling(parentnode->child, "autofire"); portnode; portnode = xml_get_sibling(portnode->next, "autofire"))
		{
			int player = xml_get_attribute_int(portnode, "player", 0);

			if (player > 0 && player <= MAX_PLAYERS)
				autofiredelay[player - 1] = xml_get_attribute_int(portnode, "delay", 1);
		}
	}

	/* after applying the controller config, push that back into the backup, since that is */
	/* what we will diff against */
	if (config_type == CONFIG_TYPE_CONTROLLER)
		memcpy(default_ports_backup, default_ports, sizeof(default_ports_backup));
}



/*************************************
 *
 *  Input port configuration write
 *
 *************************************/

static void add_sequence(xml_data_node *parentnode, int type, int porttype, const input_seq *seq)
{
	astring *seqstring = astring_alloc();
	xml_data_node *seqnode;

	/* get the string for the sequence */
	if (input_seq_get_1(seq) == SEQCODE_END)
		astring_cpyc(seqstring, "NONE");
	else
		input_seq_to_tokens(seqstring, seq);

	/* add the new node */
	seqnode = xml_add_child(parentnode, "newseq", astring_c(seqstring));
	if (seqnode)
		xml_set_attribute(seqnode, "type", seqtypestrings[type]);
	astring_free(seqstring);
}


static void save_default_inputs(xml_data_node *parentnode)
{
	int portnum;

	/* iterate over ports */
	for (portnum = 0; portnum < ARRAY_LENGTH(default_ports_backup); portnum++)
	{
		input_port_default_entry *defport = &default_ports_backup[portnum];
		input_port_default_entry *curport = &default_ports[portnum];

		/* only save if something has changed and this port is a type we save */
		if (save_this_port_type(defport->type) &&
			(input_seq_cmp(&defport->defaultseq, &curport->defaultseq) != 0 ||
			 input_seq_cmp(&defport->defaultdecseq, &curport->defaultdecseq) != 0 ||
			 input_seq_cmp(&defport->defaultincseq, &curport->defaultincseq) != 0))
		{
			/* add a new port node */
			xml_data_node *portnode = xml_add_child(parentnode, "port", NULL);
			if (portnode)
			{
				/* add the port information and attributes */
				xml_set_attribute(portnode, "type", port_type_to_token(defport->type, defport->player));

				/* add only the sequences that have changed from the defaults */
				if (input_seq_cmp(&defport->defaultseq, &curport->defaultseq) != 0)
					add_sequence(portnode, 0, defport->type, &curport->defaultseq);
				if (input_seq_cmp(&defport->defaultdecseq, &curport->defaultdecseq) != 0)
					add_sequence(portnode, 1, defport->type, &curport->defaultdecseq);
				if (input_seq_cmp(&defport->defaultincseq, &curport->defaultincseq) != 0)
					add_sequence(portnode, 2, defport->type, &curport->defaultincseq);
			}
		}
	}
}


static void save_game_inputs(xml_data_node *parentnode)
{
	int portnum;

	/* iterate over ports */
	for (portnum = 0; input_ports_default[portnum].type != IPT_END; portnum++)
	{
		input_port_entry *defport = &input_ports_default[portnum];
		input_port_entry *curport = &Machine->input_ports[portnum];

		/* only save if something has changed and this port is a type we save */
		if (save_this_port_type(defport->type) &&
			((!port_type_is_analog(defport->type) && (defport->default_value & defport->mask) != (curport->default_value & defport->mask)) ||
			 (port_type_is_analog(defport->type) &&
			  (defport->analog.delta != curport->analog.delta ||
			 defport->analog.centerdelta != curport->analog.centerdelta ||
			 defport->analog.sensitivity != curport->analog.sensitivity ||
			   defport->analog.reverse != curport->analog.reverse)) ||
			 curport->autofire_setting ||
#ifdef USE_CUSTOM_BUTTON
			 (defport->type >= IPT_CUSTOM1 && defport->type < IPT_CUSTOM1 + MAX_CUSTOM_BUTTONS &&
			  custom_button[defport->player][defport->type - IPT_CUSTOM1]) ||
#endif /* USE_CUSTOM_BUTTON */
			 input_seq_cmp(&defport->seq, &curport->seq) != 0 ||
			 input_seq_cmp(&defport->analog.decseq, &curport->analog.decseq) != 0 ||
			 input_seq_cmp(&defport->analog.incseq, &curport->analog.incseq) != 0))
		{
			/* add a new port node */
			xml_data_node *portnode = xml_add_child(parentnode, "port", NULL);
			if (portnode)
			{
				/* add the port information and attributes */
				xml_set_attribute(portnode, "type", port_type_to_token(defport->type, defport->player));
				xml_set_attribute_int(portnode, "mask", defport->mask);
				xml_set_attribute_int(portnode, "index", portnum);
				xml_set_attribute_int(portnode, "defvalue", defport->default_value & defport->mask);

				/* if the value has changed, add it as well */
				if (!port_type_is_analog(defport->type) && (defport->default_value & defport->mask) != (curport->default_value & defport->mask))
					xml_set_attribute_int(portnode, "value", curport->default_value & defport->mask);

				/* add analog-specific attributes if they have changed */
				if (port_type_is_analog(defport->type))
				{
					if (defport->analog.delta != curport->analog.delta)
						xml_set_attribute_int(portnode, "keydelta", curport->analog.delta);
					if (defport->analog.centerdelta != curport->analog.centerdelta)
						xml_set_attribute_int(portnode, "centerdelta", curport->analog.centerdelta);
					if (defport->analog.sensitivity != curport->analog.sensitivity)
						xml_set_attribute_int(portnode, "sensitivity", curport->analog.sensitivity);
					if (defport->analog.reverse != curport->analog.reverse)
						xml_set_attribute(portnode, "reverse", curport->analog.reverse ? "yes" : "no");
				}

				/* write digital sequences */
				else
				{
					if (curport->autofire_setting & AUTOFIRE_ON)
						xml_set_attribute(portnode, "autofire", "on");
					else if (curport->autofire_setting & AUTOFIRE_TOGGLE)
						xml_set_attribute(portnode, "autofire", "toggle");

#ifdef USE_CUSTOM_BUTTON
					if (defport->type >= IPT_CUSTOM1 && defport->type < IPT_CUSTOM1 + MAX_CUSTOM_BUTTONS &&
					    custom_button[defport->player][defport->type - IPT_CUSTOM1])
						xml_set_attribute_int(portnode, "custom", custom_button[defport->player][defport->type - IPT_CUSTOM1]);
#endif /* USE_CUSTOM_BUTTON */
				}

				/* add only the sequences that have changed from the defaults */
				if (input_seq_cmp(&defport->seq, &curport->seq) != 0)
					add_sequence(portnode, 0, defport->type, &curport->seq);
				if (input_seq_cmp(&defport->analog.decseq, &curport->analog.decseq) != 0)
					add_sequence(portnode, 1, defport->type, &curport->analog.decseq);
				if (input_seq_cmp(&defport->analog.incseq, &curport->analog.incseq) != 0)
					add_sequence(portnode, 2, defport->type, &curport->analog.incseq);
			}
		}
	}

	for (portnum = 0; portnum < MAX_PLAYERS; portnum++)
	{
		if (autofiredelay[portnum] != 1)
		{
			xml_data_node *childnode = xml_add_child(parentnode, "autofire", NULL);
			if (childnode)
			{
				xml_set_attribute_int(childnode, "player", portnum + 1);
				xml_set_attribute_int(childnode, "delay", autofiredelay[portnum]);
			}
		}
	}
}


static void input_port_save(int config_type, xml_data_node *parentnode)
{
	if (parentnode)
	{
		/* default ports save differently */
		if (config_type == CONFIG_TYPE_DEFAULT)
			save_default_inputs(parentnode);
		else
			save_game_inputs(parentnode);
	}
}



/*************************************
 *
 *  Token to default string
 *
 *************************************/

const char *input_port_string_from_token(const input_port_token token)
{
	int index;

	if (token == 0)
		return NULL;
	if ((FPTR)token >= INPUT_STRING_COUNT)
		return (const char *)token;
	for (index = 0; index < ARRAY_LENGTH(input_port_default_strings); index++)
		if (input_port_default_strings[index].id == (FPTR)token)
			return input_port_default_strings[index].string;
	return "(Unknown Default)";
}



/*************************************
 *
 *  Input port detokenizer
 *
 *************************************/

static void input_port_detokenize(input_port_init_params *param, const input_port_token *ipt)
{
	UINT32 entrytype = INPUT_TOKEN_INVALID;
	input_port_entry *port = NULL;
	const char *modify_tag = NULL;
	int seq_index[3] = {0};

	/* loop over tokens until we hit the end */
	while (entrytype != INPUT_TOKEN_END)
	{
		UINT32 mask, defval, type, val;

		/* the entry is the first of a UINT32 pair */
		/* note that we advance IPT assuming that there is no second half */
		entrytype = INPUT_PORT_PAIR_ITEM(ipt++, 0);
		switch (entrytype)
		{
			/* end */
			case INPUT_TOKEN_END:
				break;

			/* including */
			case INPUT_TOKEN_INCLUDE:
				input_port_detokenize(param, (const input_port_token *)*ipt++);
				break;

			/* start of a new input port */
			case INPUT_TOKEN_START:
			case INPUT_TOKEN_START_TAG:
				modify_tag = NULL;
				port = input_port_initialize(param, IPT_PORT, NULL, 0, 0);
				if (entrytype == INPUT_TOKEN_START_TAG)
					port->start.tag = (const char *)*ipt++;
				break;

			/* modify an existing port */
			case INPUT_TOKEN_MODIFY:
				modify_tag = (const char *)*ipt++;
				break;

			/* input bit definition */
			case INPUT_TOKEN_BIT:
				type = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				mask = INPUT_PORT_PAIR_ITEM(ipt, 0);
				defval = INPUT_PORT_PAIR_ITEM(ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;

				port = input_port_initialize(param, type, modify_tag, mask, defval);
				seq_index[0] = seq_index[1] = seq_index[2] = 0;
				break;

			/* append a code */
			case INPUT_TOKEN_CODE:
				val = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				if (seq_index[0] > 0)
					port->seq.code[seq_index[0]++] = SEQCODE_OR;
				port->seq.code[seq_index[0]++] = val;
				break;

			case INPUT_TOKEN_CODE_DEC:
				val = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				if (seq_index[1] > 0)
					port->analog.decseq.code[seq_index[1]++] = SEQCODE_OR;
				port->analog.decseq.code[seq_index[1]++] = val;
				break;

			case INPUT_TOKEN_CODE_INC:
				val = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				if (seq_index[2] > 0)
					port->analog.incseq.code[seq_index[2]++] = SEQCODE_OR;
				port->analog.incseq.code[seq_index[2]++] = val;
				break;

			/* joystick flags */
			case INPUT_TOKEN_2WAY:
			case INPUT_TOKEN_4WAY:
			case INPUT_TOKEN_8WAY:
			case INPUT_TOKEN_16WAY:
				port->way = 2 << (entrytype - INPUT_TOKEN_2WAY);
				break;

			case INPUT_TOKEN_ROTATED:
				port->rotated = TRUE;
				break;

			/* general flags */
			case INPUT_TOKEN_NAME:
				port->name = input_port_string_from_token(*ipt++);
				break;

			case INPUT_TOKEN_PLAYER1:
			case INPUT_TOKEN_PLAYER2:
			case INPUT_TOKEN_PLAYER3:
			case INPUT_TOKEN_PLAYER4:
			case INPUT_TOKEN_PLAYER5:
			case INPUT_TOKEN_PLAYER6:
			case INPUT_TOKEN_PLAYER7:
			case INPUT_TOKEN_PLAYER8:
				port->player = entrytype - INPUT_TOKEN_PLAYER1;
				break;

			case INPUT_TOKEN_COCKTAIL:
				port->cocktail = TRUE;
				port->player = 1;
				break;

			case INPUT_TOKEN_TOGGLE:
				port->toggle = TRUE;
				break;

			case INPUT_TOKEN_IMPULSE:
				port->impulse = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				break;

			case INPUT_TOKEN_REVERSE:
				port->analog.reverse = TRUE;
				break;

			case INPUT_TOKEN_RESET:
				port->analog.reset = TRUE;
				break;

			case INPUT_TOKEN_UNUSED:
				port->unused = TRUE;
				break;

			/* analog settings */
			case INPUT_TOKEN_MINMAX:
				port->analog.min = INPUT_PORT_PAIR_ITEM(ipt, 0);
				port->analog.max = INPUT_PORT_PAIR_ITEM(ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				break;

			case INPUT_TOKEN_SENSITIVITY:
				port->analog.sensitivity = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				break;

			case INPUT_TOKEN_KEYDELTA:
				port->analog.delta = port->analog.centerdelta = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				break;

			case INPUT_TOKEN_CENTERDELTA:
				port->analog.centerdelta = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				break;

			case INPUT_TOKEN_CROSSHAIR:
				port->analog.crossaxis = INPUT_PORT_PAIR_ITEM(--ipt, 1) & 0xff;
				port->analog.crossaltaxis = (float)((INT32)INPUT_PORT_PAIR_ITEM(ipt, 1) >> 8) / 65536.0f;
				ipt += INPUT_PORT_PAIR_TOKENS;
				port->analog.crossscale = (float)(INT32)INPUT_PORT_PAIR_ITEM(ipt, 0) / 65536.0f;
				port->analog.crossoffset = (float)(INT32)INPUT_PORT_PAIR_ITEM(ipt, 1) / 65536.0f;
				ipt += INPUT_PORT_PAIR_TOKENS;
				break;

			case INPUT_TOKEN_FULL_TURN_COUNT:
				port->analog.full_turn_count = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				break;

			case INPUT_TOKEN_POSITIONS:
				port->analog.max = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				break;

			case INPUT_TOKEN_WRAPS:
				port->analog.wraps = TRUE;
				break;

			case INPUT_TOKEN_REMAP_TABLE:
				port->analog.remap_table = (UINT32 *)*ipt++;
				break;

			case INPUT_TOKEN_INVERT:
				port->analog.invert = TRUE;
				break;

			/* custom callbacks */
			case INPUT_TOKEN_CUSTOM:
				port->custom = (UINT32 (*)(void *))*ipt++;
				port->custom_param = (void *)*ipt++;
				break;

			/* dip switch definition */
			case INPUT_TOKEN_DIPNAME:
				mask = INPUT_PORT_PAIR_ITEM(ipt, 0);
				defval = INPUT_PORT_PAIR_ITEM(ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;

				port = input_port_initialize(param, IPT_DIPSWITCH_NAME, modify_tag, mask, defval);
				seq_index[0] = seq_index[1] = seq_index[2] = 0;
				port->name = input_port_string_from_token(*ipt++);
				break;

			case INPUT_TOKEN_DIPSETTING:
				defval = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;

				port = input_port_initialize(param, IPT_DIPSWITCH_SETTING, modify_tag, 0, defval);
				seq_index[0] = seq_index[1] = seq_index[2] = 0;
				port->name = input_port_string_from_token(*ipt++);
				break;

			/* physical location */
			case INPUT_TOKEN_DIPLOCATION:
				input_port_parse_diplocation(port, (const char *)*ipt++);
				break;

			/* conditionals for dip switch settings */
			case INPUT_TOKEN_CONDITION:
				port->condition.condition = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				port->condition.mask = INPUT_PORT_PAIR_ITEM(ipt, 0);
				port->condition.value = INPUT_PORT_PAIR_ITEM(ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				port->condition.tag = (const char *)*ipt++;
				break;

			/* analog adjuster definition */
			case INPUT_TOKEN_ADJUSTER:
				defval = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;

				port = input_port_initialize(param, IPT_ADJUSTER, modify_tag, 0xff, defval | (defval << 8));
				seq_index[0] = seq_index[1] = seq_index[2] = 0;
				port->name = input_port_string_from_token(*ipt++);
				break;

			/* configuration definition */
			case INPUT_TOKEN_CONFNAME:
				mask = INPUT_PORT_PAIR_ITEM(ipt, 0);
				defval = INPUT_PORT_PAIR_ITEM(ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;

				port = input_port_initialize(param, IPT_CONFIG_NAME, modify_tag, mask, defval);
				seq_index[0] = seq_index[1] = seq_index[2] = 0;
				port->name = input_port_string_from_token(*ipt++);
				break;

			case INPUT_TOKEN_CONFSETTING:
				defval = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;

				port = input_port_initialize(param, IPT_CONFIG_SETTING, modify_tag, 0, defval);
				seq_index[0] = seq_index[1] = seq_index[2] = 0;
				port->name = input_port_string_from_token(*ipt++);
				break;

#ifdef MESS
			case INPUT_TOKEN_CHAR:
				val = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;

				{
					int ch;
					for (ch = 0; port->keyboard.chars[ch] != 0; ch++)
						;
					port->keyboard.chars[ch] = (unicode_char) val;
				}
				break;

			/* category definition */
			case INPUT_TOKEN_CATEGORY:
				port->category = (UINT16) INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;
				break;

			case INPUT_TOKEN_CATEGORY_NAME:
				mask = INPUT_PORT_PAIR_ITEM(ipt, 0);
				defval = INPUT_PORT_PAIR_ITEM(ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;

				port = input_port_initialize(param, IPT_CATEGORY_NAME, modify_tag, mask, defval);
				seq_index[0] = seq_index[1] = seq_index[2] = 0;
				port->name = input_port_string_from_token(*ipt++);
				break;

			case INPUT_TOKEN_CATEGORY_SETTING:
				defval = INPUT_PORT_PAIR_ITEM(--ipt, 1);
				ipt += INPUT_PORT_PAIR_TOKENS;

				port = input_port_initialize(param, IPT_CATEGORY_SETTING, modify_tag, 0, defval);
				seq_index[0] = seq_index[1] = seq_index[2] = 0;
				port->name = input_port_string_from_token(*ipt++);
				break;
#endif /* MESS */

			default:
				fatalerror("unknown port entry type");
				break;
		}
	}
}



/*************************************
 *
 *  Input port construction
 *
 *************************************/

input_port_entry *input_port_initialize(input_port_init_params *iip, UINT32 type, const char *tag, UINT32 mask, UINT32 defval)
{
	/* this function is used within an INPUT_PORT callback to set up a single port */
	input_port_entry *port;
	input_code code;

	/* are we modifying an existing port? */
	if (tag != NULL)
	{
		int portnum, deleting;

		/* find the matching port */
		for (portnum = 0; portnum < iip->current_port; portnum++)
			if (iip->ports[portnum].type == IPT_PORT && iip->ports[portnum].start.tag != NULL && !strcmp(iip->ports[portnum].start.tag, tag))
				break;
		if (portnum >= iip->current_port)
			fatalerror("Could not find port to modify: '%s'", tag);

		/* nuke any matching masks */
		for (portnum++, deleting = 0; portnum < iip->current_port && iip->ports[portnum].type != IPT_PORT; portnum++)
		{
			deleting = (iip->ports[portnum].mask & mask) || (deleting && iip->ports[portnum].mask == 0);
			if (deleting)
			{
				iip->current_port--;
				memmove(&iip->ports[portnum], &iip->ports[portnum + 1], (iip->current_port - portnum) * sizeof(iip->ports[0]));
				portnum--;
			}
		}

		/* allocate space for a new port at the end of this entry */
		if (iip->current_port >= iip->max_ports)
			fatalerror("Too many input ports");
		if (portnum < iip->current_port)
		{
			memmove(&iip->ports[portnum + 1], &iip->ports[portnum], (iip->current_port - portnum) * sizeof(iip->ports[0]));
			iip->current_port++;
			port = &iip->ports[portnum];
		}
		else
			port = &iip->ports[iip->current_port++];
	}

	/* otherwise, just allocate a new one from the end */
	else
	{
		if (iip->current_port >= iip->max_ports)
			fatalerror("Too many input ports");
		port = &iip->ports[iip->current_port++];
	}

	/* set up defaults */
	memset(port, 0, sizeof(*port));
	port->name = IP_NAME_DEFAULT;
	port->type = type;
	port->mask = mask;
	port->default_value = defval;

	/* default min to 0 and max to the mask value */
	/* they can be overwritten by PORT_MINMAX */
	if (port_type_is_analog(port->type) && !port->analog.min && !port->analog.max)
		port->analog.max = port->mask;

	/* sets up default port codes */
	switch (port->type)
	{
		case IPT_DIPSWITCH_NAME:
		case IPT_DIPSWITCH_SETTING:
		case IPT_CONFIG_NAME:
		case IPT_CONFIG_SETTING:
			code = SEQCODE_END;
			break;

		default:
			code = SEQCODE_DEFAULT;
			break;
	}

	/* set the default codes */
	input_seq_set_1(&port->seq, code);
	input_seq_set_1(&port->analog.incseq, code);
	input_seq_set_1(&port->analog.decseq, code);
	return port;
}


input_port_entry *input_port_allocate(const input_port_token *ipt, input_port_entry *memory)
{
	input_port_init_params iip;
	input_port_entry *port;

#ifdef USE_NEOGEO_HACKS
	int remove_neogeo_territory = 0;
	int remove_neogeo_arcade = 0;

	if (Machine && Machine->gamedrv &&
	     (!mame_stricmp(Machine->gamedrv->source_file+17, "neogeo.c")
	      || !mame_stricmp(Machine->gamedrv->source_file+17, "neodrvr.c")))
	{
		int system_bios = determine_bios_rom(mame_options(), Machine->gamedrv->rom);

		/* first mark all items to disable */
		remove_neogeo_territory = 1;
		remove_neogeo_arcade = 1;

		switch (system_bios - 1)
		{
		// enable arcade/console and territory
		case NEOGEO_BIOS_EURO:
			remove_neogeo_arcade = 0;
			remove_neogeo_territory = 0;
			break;

		// enable territory
		case NEOGEO_BIOS_DEBUG:
			remove_neogeo_territory = 0;
		}

		// enable arcade/console and territory
		if (!strcmp(Machine->gamedrv->name,"irrmaze"))
		{
			remove_neogeo_arcade = 0;
			remove_neogeo_territory = 0;
		}
	}
#endif /* USE_NEOGEO_HACKS */

	/* set up the port parameter structure */
	iip.max_ports = MAX_INPUT_PORTS * MAX_BITS_PER_PORT;
	iip.current_port = 0;

	/* add for toggle autofire button */
	iip.max_ports += 1 + MAX_PLAYERS;

#ifdef USE_CUSTOM_BUTTON
	iip.max_ports += MAX_CUSTOM_BUTTONS * MAX_PLAYERS;
#endif /* USE_CUSTOM_BUTTON */

	/* allocate memory for the input ports */
	if (memory == NULL)
		iip.ports = (input_port_entry *)auto_malloc(iip.max_ports * sizeof(*iip.ports));
	else
		iip.ports = memory;
/* avoided this useless initialization: the memory is always ...
 ... cleared first on every input_port_initialize() call !!!
	memset(iip.ports, 0, iip.max_ports * sizeof(*iip.ports));
 */

	/* construct the ports */
	input_port_detokenize(&iip, ipt);

	/* append final IPT_END */
	input_port_initialize(&iip, IPT_END, NULL, 0, 0);

#ifdef USE_NEOGEO_HACKS
	{
		for (port = iip.ports; port->type != IPT_END; port++)
		{
			int n = 0;

			while (port[n].type == IPT_DIPSWITCH_NAME)
			{
				if ((remove_neogeo_territory && !strcmp(port[n].name, "Territory")) ||
				    (remove_neogeo_arcade && (!strcmp(port[n].name, "Machine Mode") ||
				                              !strcmp(port[n].name, "Game Slots"))))
				{
					while (port[++n].type == IPT_DIPSWITCH_SETTING)
						;
				}
				else
					break;
			}

			if (n)
				memmove(port, &port[n], n * sizeof *port);
		}

	}
#endif /* USE_NEOGEO_HACKS */

	// Install buttons if needed
	{
		int p;
		int b;

		players = 0;
		normal_buttons = 0;

		for (port = iip.ports; port->type != IPT_END; port++)
		{
			p = port->player + 1;
			b = port->type - IPT_BUTTON1 + 1;

			if (b < 1 || b > MAX_NORMAL_BUTTONS)
				continue;

			if (p > players)
				players = p;

			if (b > normal_buttons)
				normal_buttons = b;
		}

		if (normal_buttons)
		{
			input_port_entry port_end = *port;

			memset(port, 0, sizeof(*port));
			port->type = IPT_PORT;
			port->name = IP_NAME_DEFAULT;
			input_seq_set_1(&port->seq, SEQCODE_DEFAULT);
			port++;

			for (p = 0; p < players; p++)
			{
				memset(port, 0, sizeof(*port));
				port->name = IP_NAME_DEFAULT;
				port->type = IPT_TOGGLE_AUTOFIRE;
				port->player = p;
				port->mask = 1 << p;
				port->default_value = IP_ACTIVE_HIGH;
				port->toggle = 1;
				input_seq_set_1(&port->seq, SEQCODE_DEFAULT);

				port++;
			}

#ifdef USE_CUSTOM_BUTTON
			if (normal_buttons > 2)
				custom_buttons = 4;
			else if (normal_buttons == 2)
				custom_buttons = 3;
			else
				custom_buttons = 1;

			for (p = 0; p < players; p++)
				for (b = 0; b < custom_buttons; b++)
				{
					memset(port, 0, sizeof(*port));
					port->name = IP_NAME_DEFAULT;
					port->type = IPT_CUSTOM1 + b;
					port->player = p;
					port->mask = 0;
					port->default_value = IP_ACTIVE_LOW;
					input_seq_set_1(&port->seq, SEQCODE_DEFAULT);

					port++;
				}
#endif /* USE_CUSTOM_BUTTON */

			*port = port_end;
		}
	}


#ifdef MESS
	/* process MESS specific extensions to the port */
	inputx_handle_mess_extensions(iip.ports);
#endif

	return iip.ports;
}


void input_port_parse_diplocation(input_port_entry *in, const char *location)
{
	char *curname = NULL, tempbuf[100];
	const char *entry;
	int index, val, bits;
	UINT32 temp;

	/* if nothing present, bail */
	if (!location)
		return;
	memset(in->diploc, 0, sizeof(in->diploc));

	/* parse the string */
	for (index = 0, entry = location; *entry && index < ARRAY_LENGTH(in->diploc); index++)
	{
		const char *comma, *colon, *number;

		/* find the end of this entry */
		comma = strchr(entry, ',');
		if (comma == NULL)
			comma = entry + strlen(entry);

		/* extract it to tempbuf */
		strncpy(tempbuf, entry, comma - entry);
		tempbuf[comma - entry] = 0;

		/* first extract the switch name if present */
		number = tempbuf;
		colon = strchr(tempbuf, ':');
		if (colon != NULL)
		{
			curname = auto_malloc(colon - tempbuf + 1);
			strncpy(curname, tempbuf, colon - tempbuf);
			curname[colon - tempbuf] = 0;
			number = colon + 1;
		}

		/* if we don't have a name by now, we're screwed */
		if (curname == NULL)
			fatalerror("Switch location '%s' missing switch name!", location);

		/* now scan the switch number */
		if (sscanf(number, "%d", &val) != 1)
			fatalerror("Switch location '%s' has invalid format!", location);

		/* fill the entry and bump the index */
		in->diploc[index].swname = curname;
		in->diploc[index].swnum = val;

		/* advance to the next item */
		entry = comma;
		if (*entry)
			entry++;
	}

	/* then verify the number of bits in the mask matches */
	for (bits = 0, temp = in->mask; temp && bits < 32; bits++)
		temp &= temp - 1;
	if (bits != index)
		fatalerror("Switch location '%s' does not describe enough bits for mask %X\n", location, in->mask);
}



/*************************************
 *
 *  List access
 *
 *************************************/

input_port_default_entry *get_input_port_list(void)
{
	return default_ports;
}


const input_port_default_entry *get_input_port_list_defaults(void)
{
	return default_ports_backup;
}



/*************************************
 *
 *  Input port tokens
 *
 *************************************/

const char *port_type_to_token(int type, int player)
{
	static char tempbuf[32];
	int defindex;

	/* look up the port and return the token */
	defindex = default_ports_lookup[type][player];
	if (defindex != -1)
		return default_ports[defindex].token;

	/* if that fails, carry on */
	sprintf(tempbuf, "TYPE_OTHER(%d,%d)", type, player);
	return tempbuf;
}


int token_to_port_type(const char *string, int *player)
{
	int ipnum;

	/* check for our failsafe case first */
	if (sscanf(string, "TYPE_OTHER(%d,%d)", &ipnum, player) == 2)
		return ipnum;

	/* find the token in the list */
	for (ipnum = 0; ipnum < input_port_count; ipnum++)
		if (default_ports[ipnum].token != NULL && !strcmp(default_ports[ipnum].token, string))
		{
			*player = default_ports[ipnum].player;
			return default_ports[ipnum].type;
		}

	/* if we fail, return IPT_UNKNOWN */
	*player = 0;
	return IPT_UNKNOWN;
}



/*************************************
 *
 *  Input port getters
 *
 *************************************/

int input_port_active(const input_port_entry *port)
{
	return (input_port_name(port) != NULL && !port->unused);
}


int port_type_is_analog(int type)
{
	return (type >= __ipt_analog_start && type <= __ipt_analog_end);
}


int port_type_is_analog_absolute(int type)
{
	return (type >= __ipt_analog_absolute_start && type <= __ipt_analog_absolute_end);
}


int port_type_in_use(int type)
{
	input_port_entry *port;
	for (port = Machine->input_ports; port->type != IPT_END; port++)
		if (port->type == type)
			return 1;
	return 0;
}


int port_type_to_group(int type, int player)
{
	int defindex = default_ports_lookup[type][player];
	if (defindex != -1)
		return default_ports[defindex].group;
	return IPG_INVALID;
}


int port_tag_to_index(const char *tag)
{
	int port;

	/* find the matching tag */
	for (port = 0; port < MAX_INPUT_PORTS; port++)
		if (port_info[port].tag != NULL && !strcmp(port_info[port].tag, tag))
			return port;
	return -1;
}


read8_handler port_tag_to_handler8(const char *tag)
{
	int port = port_tag_to_index(tag);
	return (port == -1) ? MRA8_NOP : port_handler8[port];
}


read16_handler port_tag_to_handler16(const char *tag)
{
	int port = port_tag_to_index(tag);
	return (port == -1) ? MRA16_NOP : port_handler16[port];
}


read32_handler port_tag_to_handler32(const char *tag)
{
	int port = port_tag_to_index(tag);
	return (port == -1) ? MRA32_NOP : port_handler32[port];
}


read64_handler port_tag_to_handler64(const char *tag)
{
	return MRA64_NOP;
}


const char *input_port_name(const input_port_entry *port)
{
	int defindex;

	/* if we have a non-default name, use that */
	if (port->name != IP_NAME_DEFAULT)
		return port->name;

	/* if the port exists, return the default name */
	defindex = default_ports_lookup[port->type][port->player];
	if (defindex != -1)
		return default_ports[defindex].name;

	/* should never get here */
	return NULL;
}


input_seq *input_port_seq(input_port_entry *port, int seqtype)
{
	static input_seq ip_none = SEQ_DEF_0;
	input_seq *portseq;

	/* if port is disabled, return no key */
	if (port->unused)
		return &ip_none;

	/* handle the various seq types */
	switch (seqtype)
	{
		case SEQ_TYPE_STANDARD:
			portseq = &port->seq;
			break;

		case SEQ_TYPE_INCREMENT:
			if (!IS_ANALOG(port))
				return &ip_none;
			portseq = &port->analog.incseq;
			break;

		case SEQ_TYPE_DECREMENT:
			if (!IS_ANALOG(port))
				return &ip_none;
			portseq = &port->analog.decseq;
			break;

		default:
			return &ip_none;
	}

	/* does this override the default? if so, return it directly */
	if (input_seq_get_1(portseq) != SEQCODE_DEFAULT)
		return portseq;

	/* otherwise find the default setting */
	return input_port_default_seq(port->type, port->player, seqtype);
}


input_seq *input_port_default_seq(int type, int player, int seqtype)
{
	static input_seq ip_none = SEQ_DEF_0;

	/* find the default setting */
	int defindex = default_ports_lookup[type][player];
	if (defindex != -1)
	{
		switch (seqtype)
		{
			case SEQ_TYPE_STANDARD:
				return &default_ports[defindex].defaultseq;
			case SEQ_TYPE_INCREMENT:
				return &default_ports[defindex].defaultincseq;
			case SEQ_TYPE_DECREMENT:
				return &default_ports[defindex].defaultdecseq;
		}
	}
	return &ip_none;
}


int input_port_condition(const input_port_entry *in)
{
	switch (in->condition.condition)
	{
		case PORTCOND_EQUALS:
			return ((readinputport(in->condition.portnum) & in->condition.mask) == in->condition.value);
		case PORTCOND_NOTEQUALS:
			return ((readinputport(in->condition.portnum) & in->condition.mask) != in->condition.value);
	}
	return 1;
}



/*************************************
 *
 *  Key sequence handlers
 *
 *************************************/

int input_port_type_pressed(int type, int player)
{
	int defindex = default_ports_lookup[type][player];
	if (defindex != -1)
		return input_seq_pressed(&default_ports[defindex].defaultseq);

	return 0;
}


int input_ui_pressed(int code)
{
	int pressed;

profiler_mark(PROFILER_INPUT);

	/* get the status of this key (assumed to be only in the defaults) */
	pressed = input_seq_pressed(input_port_default_seq(code, 0, SEQ_TYPE_STANDARD));

	/* if pressed, handle it specially */
	if (pressed)
	{
		/* if this is the first press, leave pressed = 1 */
		if (ui_memory[code] == 0)
			ui_memory[code] = 1;

		/* otherwise, reset pressed = 0 */
		else
			pressed = 0;
	}

	/* if we're not pressed, reset the memory field */
	else
		ui_memory[code] = 0;

profiler_mark(PROFILER_END);

	return pressed;
}


int input_ui_pressed_repeat(int code, int speed)
{
	static osd_ticks_t lastdown;
	static osd_ticks_t keydelay;
	int pressed;

profiler_mark(PROFILER_INPUT);

	/* get the status of this key (assumed to be only in the defaults) */
	pressed = input_seq_pressed(input_port_default_seq(code, 0, SEQ_TYPE_STANDARD));

	/* if so, handle it specially */
	if (pressed)
	{
		/* if this is the first press, set a 3x delay and leave pressed = 1 */
		if (ui_memory[code] == 0)
		{
			ui_memory[code] = 1;
			lastdown = osd_ticks();
			keydelay = 3 * speed * osd_ticks_per_second() / 60;
		}

		/* if this is an autorepeat case, set a 1x delay and leave pressed = 1 */
		else if (osd_ticks() - lastdown >= keydelay)
		{
			lastdown += keydelay;
			keydelay = 1 * speed * osd_ticks_per_second() / 60;
		}

		/* otherwise, reset pressed = 0 */
		else
			pressed = 0;
	}

	/* if we're not pressed, reset the memory field */
	else
		ui_memory[code] = 0;

profiler_mark(PROFILER_END);

	return pressed;
}



/*************************************
 *
 *  Playback/record helper
 *
 *************************************/

static void update_playback_record(int portnum, UINT32 portvalue)
{
#ifdef MAME_AVI
	int GetAviCapture(void);
#endif /* MAME_AVI */

	/* handle playback */
	if (Machine->playback_file != NULL)
	{
		UINT32 result;

		/* a successful read goes into the playback field which overrides everything else */
		if (mame_fread(Machine->playback_file, &result, sizeof(result)) == sizeof(result))
			portvalue = port_info[portnum].playback = BIG_ENDIANIZE_INT32(result);

		/* a failure causes us to close the playback file and stop playback */
		else
		{
			mame_fclose(Machine->playback_file);
			Machine->playback_file = NULL;

#ifdef MAME_AVI
			if (!GetAviCapture())
			{
#endif /* MAME_AVI */
			if(no_extended_inp)
				popmessage(_("End of playback"));
			else
			{
				popmessage(_("End of playback - %i frames - Average speed %f%%"),framecount,(double)totalspeed/framecount);
				printf("End of playback - %i frames - Average speed %f%%\n",framecount,(double)totalspeed/framecount);
			}
#ifdef MAME_AVI
			}
#endif /* MAME_AVI */

#ifdef AUTO_PAUSE_PLAYBACK
			if (options_get_bool(mame_options(), OPTION_AUTO_PAUSE_PLAYBACK))
				ui_auto_pause();
#endif /* AUTO_PAUSE_PLAYBACK */
		}
	}

	/* handle recording */
	if (Machine->record_file != NULL)
	{
		UINT32 result = BIG_ENDIANIZE_INT32(portvalue);

		/* a successful write just works */
		if (mame_fwrite(Machine->record_file, &result, sizeof(result)) == sizeof(result))
			;

		/* a failure causes us to close the record file and stop recording */
		else
		{
			mame_fclose(Machine->record_file);
			Machine->record_file = NULL;
		}
	}
}



/*************************************
 *
 *  Update default ports
 *
 *************************************/

void input_port_update_defaults(void)
{
	int loopnum, portnum;

	/* two passes to catch conditionals properly */
	for (loopnum = 0; loopnum < 2; loopnum++)

		/* loop over all input ports */
		for (portnum = 0; portnum < MAX_INPUT_PORTS; portnum++)
		{
			input_port_info *portinfo = &port_info[portnum];
			input_bit_info *info;
			int bitnum;

			/* only clear on the first pass */
			if (loopnum == 0)
				portinfo->defvalue = 0;

			/* first compute the default value for the entire port */
			for (bitnum = 0, info = &portinfo->bit[0]; bitnum < MAX_BITS_PER_PORT && info->port; bitnum++, info++)
				if (input_port_condition(info->port))
					portinfo->defvalue = (portinfo->defvalue & ~info->port->mask) | (info->port->default_value & info->port->mask);
		}
}



/*************************************
 *
 *  VBLANK start routine
 *
 *************************************/

#ifdef KAILLERA
static void input_port_value_save(void)
{
	input_port_entry *port;
	int portnum = 0;

	for (port = Machine->input_ports; port->type != IPT_END; port++)
		if (port->type == IPT_PORT)
		{
			kDefValues[portnum] = port_info[portnum].defvalue;
			portnum++;
		}
}

static void input_port_value_load(void)
{
	input_port_entry *port;
	int portnum = 0;

	for (port = Machine->input_ports; port->type != IPT_END; port++)
		if (port->type == IPT_PORT)
		{
			port_info[portnum].defvalue = kDefValues[portnum];
			portnum++;
		}
}

static int readword(mame_file **f,UINT16 *num)
{
	UINT16 res;

	if (*f == NULL) return -1;

	if (mame_fread(*f, &res, sizeof(res)) == sizeof(res))
		*num = BIG_ENDIANIZE_INT16(res);

	else
	{
		mame_fclose(*f);
		*f = NULL;
		return -1;
	}

	return 0;
}

static void writeword(mame_file **f,UINT16 num)
{
	UINT16 res = BIG_ENDIANIZE_INT16(num);

	if (*f == NULL) return;

	if (mame_fwrite(*f, &res, sizeof(res)) != sizeof(res))
	{
		mame_fclose(*f);
		*f = NULL;
	}
}
#endif /* KAILLERA */


void input_port_vblank_start(void)
{
	int ui_visible = ui_is_menu_active() || ui_is_slider_active();
	int portnum, bitnum;
#ifdef KAILLERA
	int perform_ui = 0;
	int perform_ui_dat = 0;
#endif /* KAILLERA */

profiler_mark(PROFILER_INPUT);

	/* update the digital joysticks first */
	update_digital_joysticks();

	/* compute default values for all the ports */
	input_port_update_defaults();

	/* loop over all input ports */
	for (portnum = 0; portnum < MAX_INPUT_PORTS; portnum++)
	{
		input_port_info *portinfo = &port_info[portnum];
		input_bit_info *info;

		/* compute the VBLANK mask */
		portinfo->vblank = 0;
		for (bitnum = 0, info = &portinfo->bit[0]; bitnum < MAX_BITS_PER_PORT && info->port; bitnum++, info++)
			if (info->port->type == IPT_VBLANK)
			{
				portinfo->vblank ^= info->port->mask;
				if (Machine->screen[0].vblank == 0)
					logerror("Warning: you are using IPT_VBLANK with vblank_time = 0. You need to increase vblank_time for IPT_VBLANK to work.\n");
			}

		/* now loop back and modify based on the inputs */
		portinfo->digital = 0;
		for (bitnum = 0, info = &portinfo->bit[0]; bitnum < MAX_BITS_PER_PORT && info->port; bitnum++, info++)
			if (input_port_condition(info->port))
			{
				input_port_entry *port = info->port;

#ifdef USE_CUSTOM_BUTTON
				/* update autofire status */
				if (port->type >= IPT_CUSTOM1 && port->type < IPT_CUSTOM1 + MAX_CUSTOM_BUTTONS)
				{
					if (input_seq_pressed(input_port_seq(info->port, SEQ_TYPE_STANDARD)))
					{
						if (info->autopressed > autofiredelay[port->player])
							info->autopressed = 0;

						info->autopressed++;
					}
					else
						info->autopressed = 0;

					continue;
				}
#endif /* USE_CUSTOM_BUTTON */

				/* handle non-analog types, but only when the UI isn't visible */
				if (port->type != IPT_VBLANK && !IS_ANALOG(port) && !ui_visible)
				{
					/* if the sequence for this port is currently pressed.... */
					if (auto_pressed(info))
					{
#ifdef MESS
						/* (MESS-specific) check for disabled keyboard */
						if (port->type == IPT_KEYBOARD && osd_keyboard_disabled())
							continue;
#endif
						/* skip locked-out coin inputs */
						if (port->type >= IPT_COIN1 && port->type <= IPT_COIN8 && coinlockedout[port->type - IPT_COIN1])
							continue;
						if (port->type >= IPT_SERVICE1 && port->type <= IPT_SERVICE4 && servicecoinlockedout[port->type - IPT_SERVICE1])
							continue;

						/* if this is a downward press and we're an impulse control, reset the count */
						if (port->impulse)
						{
							if (info->last == 0)
								info->impulse = port->impulse;
						}

						/* if this is a downward press and we're a toggle control, toggle the value */
						else if (port->toggle)
						{
							if (info->last == 0)
							{
								port->default_value ^= port->mask;
								portinfo->digital ^= port->mask;
#ifdef MAME32PLUSPLUS
								if (port->type == IPT_TOGGLE_AUTOFIRE && options_get_bool(mame_options(), "disp_autofire_status"))
								{
									char tmp[256];
									
									sprintf(tmp, _("P%d %s"), port->player + 1, (port->default_value) ? _("Autofire On") : _("Autofire Off"));
									popmessage(tmp);
								}
#endif /* MAME32PLUSPLUS */
							}
						}

						/* if this is a digital joystick type, apply either standard or 4-way rules */
						else if (IS_DIGITAL_JOYSTICK(port))
						{
							digital_joystick_info *joyinfo = JOYSTICK_INFO_FOR_PORT(port);
							UINT8 mask;
							switch( port->way )
							{
							case 4:
								mask = joyinfo->current4way;
								break;
							case 16:
								mask = 0xff;
								break;
							default:
								mask = joyinfo->current;
								break;
							}
							if ((mask >> JOYSTICK_DIR_FOR_PORT(port)) & 1)
								portinfo->digital ^= port->mask;
						}

						/* otherwise, just set it raw */
						else
							portinfo->digital ^= port->mask;

						/* track the last value */
						info->last = 1;
					}
					else
						info->last = 0;

					/* handle the impulse countdown */
					if (port->impulse && info->impulse > 0)
					{
						info->impulse--;
						info->last = 1;
						portinfo->digital ^= port->mask;
					}
				}

				/* note that analog ports are handled instantaneously at port read time */
			}
	}

#ifdef MESS
	/* less MESS to MESSy things */
	inputx_update();
#endif

#ifdef KAILLERA
	(*(MemoryHackFunction.Update))();

	if (kPlay)
	{
		input_ui_temp = 0;
		if (kFirstMsg)
		{
			int i,b;
			kFirstMsg=0;
			if( Machine->record_file != NULL )
			{
				popmessage(_("Play Game With Record Input Log As %s"), options_get_string(mame_options(), "auto_record_name"));
			}

			for(i=0;i<4;i++)
			{
				int j;
				b=1;
				for(j=0;j<16;j++)
				{
				  if(kBitsPlayValues[i][j][0]!=-1)
				  {
				    if(kBitsPlayValues[i][j][2] != IP_ACTIVE_LOW) {
						  port_info[kBitsPlayValues[i][j][0]].defvalue &= ~kBitsPlayValues[i][j][1]; //kt
				    } else {
						  port_info[kBitsPlayValues[i][j][0]].defvalue |= kBitsPlayValues[i][j][1]; //kt
					}
				  }
				  b=b<<1;
				}
			}
			
			//memcpy(kDefValues,input_port_value,sizeof(input_port_value));
			input_port_value_save();
		}
		else
		{
			KAILLERA_VAL_INT val[KAILLERA_MAX_PLAYER];
			unsigned char chval[32*KAILLERA_MAX_PLAYER];
			osd_ticks_t st;
			osd_ticks_t st2;	//kt
			int i,j,b,recvLen,recvPlayer,recvPlsub;
			
			int kaillera_ipu_ui = 0;		//kt
			int *usedkeys = usedkeys_get(); //kt
			
			//popmessage( "%4x", ~input_port_value[0]);	//kt test
			val[0]=0;
			b=1;
			for(i=0;i<16;i++)
			{
			  if(kBitsPlayValues[0][i][0]!=-1)
			    if(port_info[kBitsPlayValues[0][i][0]].digital & kBitsPlayValues[0][i][1]) val[0] |= b;
			  b=b<<1;
			}
			
			
			for(j=1; j<KailleraPlayerOption.subplayer+1; j++)
			{
				val[j]=0;
				b=1;
			    for(i=0;i<16;i++)
				{
					if(kBitsPlayValues[j][i][0]!=-1)
						if(port_info[kBitsPlayValues[j][i][0]].digital & kBitsPlayValues[j][i][1]) val[j] |= b;
					b=b<<1;
				}
			}
			
			//popmessage( IntToBit((int)val[0]));	//kt test
			//popmessage( "%08x,%08x,%08x", val[0],port_info[0].defvalue,port_info[0].digital);	//kt test
			{	//kt start
			
				kaillera_ipu_ui = 0;
				if(input_ui_pressed(IPT_UI_SOFT_RESET))		kaillera_ipu_ui = 1;
				if(Kaillera_StateSave_Flags & KAILLERA_FLAGS_RESET_MACHINE) {
					kaillera_ipu_ui = 1;
					Kaillera_StateSave_Flags &= ~KAILLERA_FLAGS_RESET_MACHINE;
				}
				
				if(Kaillera_StateSave_Count <= 0 && Kaillera_StateSave_TimeRemainder <= 0 && (~Kaillera_Send_Flags & 0x1) && KailleraChatdataPreparationcheck.timeremainder <= 0) {
					
					if( Kaillera_StateSave_Flags & KAILLERA_STATESAVE_AUTOSAVE ) {
						if(((~synccount) % KailleraStartOption.autosave_time_interval) == 0 ) kaillera_ipu_ui = 2;				// オートセーブ
					}
					
					
					if(KailleraStartOption.player == 1) { // クリエイトした人のみの操作可\x94\x5c
						int f=0;
						if(input_ui_pressed(IPT_UI_LOAD_STATE))			kaillera_ipu_ui = 3;		// staロード
						if(input_ui_pressed(IPT_UI_KAILLERA_TEST_7))	Kaillera_StateSave_SelectFile = ~Kaillera_StateSave_SelectFile; // ステートセーブ、ロードで使用する拡張子の変更
						if(input_ui_pressed(IPT_UI_KAILLERA_TEST1_0))	Kaillera_Overclock_Flags = ~Kaillera_Overclock_Flags; // オーバークロック
						if(input_ui_pressed(IPT_UI_SAVE_STATE)) {// staセーブ
							char tmp[256];
							kaillera_ipu_ui = 2;
							sprintf(tmp, _("Saved Name As %c"), Kaillera_StateSave_file);
							kailleraChatSendUTF8(tmp);//チャットで全員に\x95\x5c示。
						}
						
						if(input_ui_pressed(IPT_UI_KAILLERA_TEST_4)) {// ディップスイッチの状態を送信
							//int j;
							long dat[64];
							UINT16 *val2 = (UINT16*)&dat[1];
							
							if( kMaxDipSwitch<=0 )
								popmessage(_("Dipswitch Nothing"));
							else if( kMaxDipSwitch>120/4 )
								popmessage(_("Too many Dipswitches"));
							else {
								i=0;
								for(i=0;i<kMaxDipSwitch;i++) {
									(*val2) = (UINT16)(port_info[kDipSwitchValues[i][0]].defvalue & kDipSwitchValues[i][1]);
									val2++;
								}
								if(i&0x1) i++;
								
								KailleraChatdataPreparationcheck.nmb		= 8;
								KailleraChatdataPreparationcheck.str		= (char *)"Send Dipswitch";
								KailleraChatdataPreparationcheck.count		= KailleraPlayerOption.max;
								KailleraChatdataPreparationcheck.timeremainder	= 150;
								KailleraChatdataPreparationcheck.addtime	= 90;
								KailleraChatdataPreparationcheck.maxtime	= 256;
								KailleraChatdataPreparationcheck.Callback	= SendDipSwitch;
								
								dat[0] = KailleraChatdataPreparationcheck.nmb;
								kailleraChatSend(kChatData(&dat[0], (1+i/2)*4));//全員に伝える。
							}
						}
						
						if(input_ui_pressed(IPT_UI_KAILLERA_TEST_9)) { KailleraPlayerOption.sendfilespeed = KailleraStartOption.send_file_speed; f=2; }
						
						if(f>0) {
							file_error filerr;
							mame_file *file;
							UINT32 length, destLen;
							char *temp,*dest;
							char fname[MAX_PATH];
							char name[2];
							int zl;
							name[0] = Kaillera_StateSave_file;
							name[1] = 0;
							{
								//int flag;
								sprintf(fname, "%s/%s-%c.sta", Machine->basename, Machine->gamedrv->name, name[0]);
								filerr = mame_fopen(SEARCHPATH_STATE, fname, OPEN_FLAG_READ, &file);
							}
							if(filerr != FILERR_NONE) popmessage(_("%s-%c.sta is Nothing"), Machine->gamedrv->name, name[0]);
							else {
								length = mame_fsize(file);
								destLen = length + 12;
								temp = malloc(length);
								dest = malloc(destLen);
								mame_fread(file, temp, length);
								Kaillera_Send_CRC = crc32(0, (Bytef *)temp, length);
								Kaillera_Send_Flags &= ~0x4;
								zl = compress2 ((Bytef *)dest, (unsigned long*)&destLen, (Bytef *)temp, length, 9);
								if( zl == Z_OK) {
									long dat[64];
									//Kaillera_Send_Flags			|= 0x1;
									Kaillera_Send_Len			 = destLen;
									Kaillera_Send_DecompressLen  = length;
									lpkChatDatabit				 = dest;
									
									
									KailleraChatdataPreparationcheck.nmb		= 9;
									KailleraChatdataPreparationcheck.str		= (char *)"Send status file";
									KailleraChatdataPreparationcheck.count		= KailleraPlayerOption.max - 1;
									KailleraChatdataPreparationcheck.timeremainder	= 150;
									KailleraChatdataPreparationcheck.addtime	= 90;
									KailleraChatdataPreparationcheck.maxtime	= 256;
									KailleraChatdataPreparationcheck.Callback			= SendStateSaveFile;
									KailleraChatdataPreparationcheck.Callback_Update	= SendStateSaveFile_Update;
									
									if(KailleraPlayerOption.max <= 1) {
										popmessage(_("There is no other party") );
										PreparationcheckClear();
									}
									else {
										// 最初にデータサイズなどを送信
										dat[0] = KailleraChatdataPreparationcheck.nmb;
										dat[1] = 0x00000000;	// 転送速度など。
										dat[2] = Kaillera_Send_Len;
										dat[3] = Kaillera_Send_DecompressLen;
										dat[4] = Kaillera_Send_CRC;
										Kaillera_Send_Pos = 0;
										kailleraChatSend(kChatData(&dat[0], 5*4));
									}
								} else {
									popmessage(_("Transmission failed") );
									free(dest);
								}
								free(temp);
								mame_fclose(file);
							} /* if(file == -1) */
						}
					} /* クリエイトした人のみの操作はここまで */
				
				}
				
				
				if(input_ui_pressed(IPT_UI_KAILLERA_PLAYER_DUP))
					if(KailleraStartOption.player == 1)		kaillera_ipu_ui = 4;
				
				
				if(KailleraPlayerOption.playercontrol) {	//2 Input のプレイヤー位置移動
					int ipu_ui = 0;
					if(input_ui_pressed(IPT_UI_KAILLERA_PLAYER_INC))ipu_ui = 7;
					if(input_ui_pressed(IPT_UI_KAILLERA_PLAYER_DEC))ipu_ui = 6;
					val[KailleraPlayerOption.playercontrol] &= usedkeysMask_get();
					val[KailleraPlayerOption.playercontrol] |=	((ipu_ui & 0x1) << usedkeys[0])		|
																((ipu_ui & 0x2) << (usedkeys[1]-1)) |
																((ipu_ui & 0x4) << (usedkeys[2]-2));
				} else {
					if(input_ui_pressed(IPT_UI_KAILLERA_PLAYER_INC))kaillera_ipu_ui = 7;
					if(input_ui_pressed(IPT_UI_KAILLERA_PLAYER_DEC))kaillera_ipu_ui = 6;
				}
				
				
				if(input_ui_pressed(IPT_UI_KAILLERA_TEST_1) && KailleraStartOption.player == 1) {
					if( Kaillera_StateSave_Flags & KAILLERA_STATESAVE_AUTOSAVE ) {
						Kaillera_StateSave_Flags &= ~KAILLERA_STATESAVE_AUTOSAVE;
						kailleraChatSendUTF8(_("Auto-save Stop"));
					} else { 
						Kaillera_StateSave_Flags |= KAILLERA_STATESAVE_AUTOSAVE;
						kailleraChatSendUTF8(_("Auto-save Start"));
						
						KailleraStartOption.autosave_time_interval	= (int)(69.0 * (double)Machine->drv->screen[0].defstate.refresh + 0.5);
					}
				}
				if(input_ui_pressed(IPT_UI_KAILLERA_TEST_3) && KailleraStartOption.player == 1) {// mameのバージョンチェック
					long dat[64];
					dat[0] = 0x00000005;
					dat[1] = 0;
					kailleraChatSend(kChatData(&dat[0], 8));//全員に伝える。
				}
				
				if(input_ui_pressed(IPT_UI_KAILLERA_TEST_2) && KailleraStartOption.player == 1)	// 時間調節レベル変更
				{
					long dat[64];
					dat[0] = 0x0000000d;
					dat[1] = (KailleraPlayerOption.waittimemode+1) % 5;
					kailleraChatSend(kChatData(&dat[0], 8));//全員に伝える。
				}
				
				
				if(input_ui_pressed(IPT_UI_KAILLERA_TEST1_2) 
					&& KailleraChatdataPreparationcheck.timeremainder <= 0
					//&& !KailleraSyncCheck.count
					)
				{	//同期ズレチェック
					if (KailleraSyncCheck.count)
					{
						if (KailleraChatdataPreparationcheck.nmb == 11 &&
							!strcmp(KailleraChatdataPreparationcheck.str, "Sync Check") )
							popmessage(_("Now Sync Checking") );
						else
							popmessage(_("Other Player Sync Checking") );
					} else
					if(KailleraPlayerOption.max <= 1) {
						popmessage(_("There is no other party for Sync Check") );
					} else
					{
						long dat[64];
						const unsigned long pos = KailleraPlayerOption.chatsend_timelag;
						const unsigned short steppos = 8, count = 8;	// MAX count 10?
						KailleraChatdataPreparationcheck.nmb			= 11;
						KailleraChatdataPreparationcheck.str			= (char *)"Sync Check";
						KailleraChatdataPreparationcheck.count			= KailleraPlayerOption.max;
						KailleraChatdataPreparationcheck.timeremainder	= 150 + pos + steppos*count;
						KailleraChatdataPreparationcheck.addtime		= 90;
						KailleraChatdataPreparationcheck.maxtime		= 256 + pos + steppos*count;
						KailleraChatdataPreparationcheck.flag			= 0;
						KailleraChatdataPreparationcheck.Callback			= SendSyncCheck;
						KailleraChatdataPreparationcheck.Callback_Update	= SendSyncCheck_Update;
						
						dat[0] = KailleraChatdataPreparationcheck.nmb | ((KailleraStartOption.player & 0xff) << 24);
						dat[1] = synccount + pos;	//	チェック開始するポイント
						dat[2] = (steppos) | (count<<16);	//8フレームおきに８回CRCチェック
						kailleraChatSend(kChatData(&dat[0], 3*4));//全員に伝える。
						
						KailleraSyncCheck.basepos		= synccount + pos;
						KailleraSyncCheck.totalcount	= count;			
						KailleraSyncCheck.step			= steppos;
						KailleraSyncCheck.count			= KailleraSyncCheck.totalcount;
						KailleraSyncCheck.pos			= KailleraSyncCheck.basepos + KailleraSyncCheck.step;
						popmessage(_("Start Sync Check") );
					}
				}
				
				
				val[0] &= usedkeysMask_get();
				val[0] |=	((kaillera_ipu_ui & 0x1) << usedkeys[0]) |
							((kaillera_ipu_ui & 0x2) << (usedkeys[1]-1)) |
							((kaillera_ipu_ui & 0x4) << (usedkeys[2]-2));
				
				synccount++;
				if(KAILLERA_VAL_LEN == 4) {
					val[0] &= 0x0000ffff;
					val[0] |= ((synccount & 0xffff) << 16);
				}
				
			}			//kt end
			
			
			{
				//static unsigned short sba = 0,sbb = 0;
				//static unsigned char ia = 0,ib = 0;
				
				
				Kaillera_Val_Len = KAILLERA_VAL_LEN + Kaillera_analog_port * KAILLERA_ANALOG_LEN;
				if( kMaxMahjong )
					Kaillera_Val_Len++;
				
				b=0;
				for(i=0;i<16;i++)
				{
					//analog_port_info *info;
					int port = kAnalogPlayValues[0][i];
					//port = kAnalogPlayValues[0][i];
					if(port != -1) {
						//input_port_entry *in;
						//in = analog_info[port]->port;
						memcpy(&chval[KAILLERA_VAL_LEN + (b++)*KAILLERA_ANALOG_LEN], &kinput_analog_delta[port][0], KAILLERA_ANALOG_LEN);
					}
				}
				
				if(kMaxMahjong)
					for(i=0;i<29;i++)
					{
						chval[KAILLERA_VAL_LEN + (Kaillera_analog_port * KAILLERA_ANALOG_LEN)] = 0;	// 最初にクリアしておく。
						if(kMahjongPlayValues[0][i][0]!=-1)
						{
							if(kMahjongPlayValues[0][i][2] != IP_ACTIVE_LOW) {
								//if( port_info[kMahjongPlayValues[0][i][0]].digital & kMahjongPlayValues[0][i][1] ) {
								if(!(port_info[kMahjongPlayValues[0][i][0]].digital & kMahjongPlayValues[0][i][1]) ) {
									chval[KAILLERA_VAL_LEN + (Kaillera_analog_port * KAILLERA_ANALOG_LEN)] = i+1;
									break;
								}
							} else {
								//if(!(port_info[kMahjongPlayValues[0][i][0]].digital & kMahjongPlayValues[0][i][1]) ) {
								if((port_info[kMahjongPlayValues[0][i][0]].digital & kMahjongPlayValues[0][i][1]) ) {
									chval[KAILLERA_VAL_LEN + (Kaillera_analog_port * KAILLERA_ANALOG_LEN)] = i+1;
									break;
								}
							}
						}
					}
			//popmessage( "%08x,%08x,%08x,%d", val[0],port_info[0].defvalue,port_info[0].digital,chval[KAILLERA_VAL_LEN + (Kaillera_analog_port * KAILLERA_ANALOG_LEN)]);	//kt test
				
				memcpy(&chval[0], &val[0], KAILLERA_VAL_LEN);
				
				for(j=1; j<KailleraPlayerOption.subplayer+1; j++)
					memcpy(&chval[0]+(Kaillera_Val_Len * j) , &val[j], KAILLERA_VAL_LEN);
				
				if (Kaillera_StatusFlag & KAILLERA_STATUSFLAG_LOST_CONNECTION
					&& KailleraStartOption.lost_connection_time )
				{
					st=osd_ticks();
					recvLen=-1;
				} else
				{
					KailleraLostConnectionCheck(0);
					st=osd_ticks();
					recvLen=kailleraModifyPlayValues((void *)chval, Kaillera_Val_Len + (Kaillera_Val_Len * KailleraPlayerOption.subplayer));
					//recvLen=Kaillera_Val_Len + (Kaillera_Val_Len * KailleraPlayerOption.subplayer);
				}
				st2 = (osd_ticks()-st);
				if (Kaillera_Inp_InitSleepTime<=0)
					KailleraLostConnectionCheck(1);
				
				if (recvLen == -1)
					KailleraLostConnection();
				
				//kailleraChatSend_cb_update();
				
				
				if (synccount == 1)
				{
					//extern void SetActivateWindow(void);
					//SetActivateWindow();
				}
			
				if(input_ui_pressed(IPT_UI_KAILLERA_TEST1_6))
				{	//kt test
					//unsigned int size, bankcpu, bankcount;
					//int cpu = get_game_ram_serch_cpu(&size, &bankcpu, &bankcount);
				
					//popmessage( "Search Cpu %d, Ram %uByte, Over %uByte", cpu, size, oversize );
					//popmessage( "Search Cpu %d, Ram %uKB, Bank %u, BankBase %u", cpu, size / 1000, bankcount, bankcpu );
					
				}
				if( Kaillera_Inp_InitSleepTime>0 )
				{
					if (	recvLen == 0 ||
							Kaillera_Inp_InitSleepTime > (KAILLERA_STATESAVE_NORMAL_DELAYTIME / 4 - 1) )
					{
						//const int players = KailleraStartOption.numplayers * (KailleraPlayerOption.subplayer+1);
						int pltemp;
						players = KailleraStartOption.numplayers * (KailleraPlayerOption.subplayer+1);
						pltemp = players;
						if (players < 4) pltemp = 4;
						
						player_val_clear( (char *)chval, kValXor, pltemp, kMaxMahjong);
						recvLen = Kaillera_Val_Len * players;
					}
				}
			
				KailleraWaitSubTick += st2;
				if(KailleraWaitSubTick > ((osd_ticks_t)UCLOCKS_PER_SEC<<7))
					KailleraWaitSubTick = ((osd_ticks_t)UCLOCKS_PER_SEC<<7);
			
				KailleraWaitTick += st2;
			
				{
					addClock(st2);
				}
			
			
				if(recvLen > 255)recvLen = 255;
				if(recvLen <= 0) { recvLen = 0; recvPlayer = recvLen;}
				else recvPlayer = (int)(recvLen/Kaillera_Val_Len);
			
				recvPlsub = recvPlayer;
				if((recvPlayer < KailleraStartOption.gameinputmin) && recvPlayer) {
					recvPlayer = KailleraStartOption.gameinputmin;
					{
						//int i;
						memset(&chval[recvPlsub*Kaillera_Val_Len], 0, (recvPlayer-recvPlsub)*Kaillera_Val_Len);
						for(i=0; i<recvPlayer-recvPlsub; i++)
							*((unsigned short*)&chval[recvPlsub*Kaillera_Val_Len + i*Kaillera_Val_Len]) = kValXor;
					}
				}
			
				if(KailleraPlayerOption.subplayer) recvPlsub >>= 1;
			
				
				if( Kaillera_Inp_InitSleepTime>0 ) {
					Kaillera_Inp_InitSleepTime--;
					if( Kaillera_Inp_InitSleepTime<=0 )
					{
						//extern void SetActivateWindow(void);
						if (Kaillera_StatusFlag & KAILLERA_STATUSFLAG_2INPUTSTART)
						{
							long dat[64];
							dat[0] = 0x0000000f;
							dat[1] = 0x00000101;
							kailleraChatSend(kChatData(&dat[0], 8));
							
							Kaillera_StatusFlag &= ~KAILLERA_STATUSFLAG_2INPUTSTART;
						}
						//SetActivateWindow();
					}
				}
				Kaillera_Inp_CRC = crc32(Kaillera_Inp_CRC, &chval[0], recvLen);
				
				
				for(i=0; i<recvPlayer ; i++)
					memcpy(&val[i], &chval[i*Kaillera_Val_Len], KAILLERA_VAL_LEN);
				
				//ia++;ib--;
			}
			//popmessage( "%08x,%08x,%08x,%d", val[0],port_info[0].defvalue,port_info[0].digital,recvLen);	//kt test
			
			if(kPlay){	//kt start
			
				if(recvLen != maxplayer) { //プレイヤー数が変化したら、プレイヤー位置を初期化。
					maxplayer = recvLen;
					if( Kaillera_StateSave_Flags & KAILLERA_STATESAVE_AUTOSAVE )
						popmessage(_("Autosave discontinued") );
					playernmb_clear();
					playernmb_dup_clear();
					Kaillera_StateSave_Count = 0;
					Kaillera_StateSave_Flags = 0;
					Kaillera_StateSave_Retry = 0;
				}
				
				
				if(Kaillera_StateSave_TimeRemainder == KAILLERA_STATESAVE_NORMAL_DELAYTIME) { // セーブが完了したので、CRCをチェック。
					long dat[64];
					UINT32 crc;
					char name[2];
					name[0] = '@';
					name[1] = 0;
					
					crc = game_ram_serch_crc32_kaillera_state_save(Kaillera_Inp_StateSave_CRC);
					Kaillera_StateSave_CRC		 = crc;
					
					dat[0] = 0x00000001;
					dat[1] = Kaillera_StateSave_CRC;
					kailleraChatSend(kChatData(&dat[0], 8));//全員に伝える。
					
				}
				
				if(Kaillera_Send_Flags & 0x1) { // チャットを使って大きいデータを少しずつ送信
					//unsigned long syslen = 8;
					unsigned long len;
					//int pos = 2;
					//int i;
					
					static unsigned int count;
					unsigned int up;
					int loop;
					
					
					if (KailleraPlayerOption.sendfilespeed < 0)
					{
						loop = -KailleraPlayerOption.sendfilespeed;
						up = 1;
					} else
					{
						loop = 1;
						up = KailleraPlayerOption.sendfilespeed+1;
					}
					
					if((count++ % up) == 0)
					{
						for(i=0; i<loop; i++) {
							char *ch;
							len = 108;
							
							if( len + Kaillera_Send_Pos > Kaillera_Send_Len)
								len = Kaillera_Send_Len - Kaillera_Send_Pos;
							ch = kChatDatabit(lpkChatDatabit + Kaillera_Send_Pos, len);
							kailleraChatSend(ch);
							Kaillera_Send_Pos += len;
							if( Kaillera_Send_Pos >= Kaillera_Send_Len ) {
								free(lpkChatDatabit);
								lpkChatDatabit = 0;
								Kaillera_Send_Flags &= ~0x1;
								Kaillera_StateSave_TimeRemainder = KAILLERA_STATESAVE_NORMAL_DELAYTIME-1;
								popmessage(_("Transmission completed"));
								break;
							}
						}
					}
				
				}
				
				if (KailleraSyncCheck.count)	//同期チェック
				{
					if (synccount == KailleraSyncCheck.pos)
					{
						
						KailleraSyncCheck.crc[KailleraStartOption.player-1][KailleraSyncCheck.totalcount - KailleraSyncCheck.count] = game_ram_serch_crc32_(0);	//ゲームCPUが使用しているメモリのCRCを使う
						KailleraSyncCheck.pos += KailleraSyncCheck.step;
						KailleraSyncCheck.count--;
						if (!KailleraSyncCheck.count)	//全同期チェックが完了したので、チャットでCRCを送信
						{
							//int i;
							long dat[64];
							dat[0] = 0x0000000f | ((KailleraStartOption.player & 0xff) << 24);
							dat[1] = 0x0000000b;
							dat[2] = 0x00000000;
							
							memcpy (&dat[3], &KailleraSyncCheck.crc[KailleraStartOption.player-1][0], sizeof(long) * KailleraSyncCheck.totalcount);
							
							kailleraChatSend(kChatData(&dat[0], (3 + KailleraSyncCheck.totalcount)*sizeof(long)));
						}
					}
					
				}
				
				kaillera_ipu_ui	=	((val[0] >> usedkeys[0]) & 0x1) |
									((val[0] >> (usedkeys[1]-1)) & 0x2) |
									((val[0] >> (usedkeys[2]-2)) & 0x4);
				
				switch( kaillera_ipu_ui )
				{
				case 1:
					if (GetShowSystemMessage())
						popmessage(_("Machine Reset"));
					synccount = 0;
					KailleraSyncCheck.basepos = KailleraSyncCheck.step;
					KailleraSyncCheck.count = 0;
					
					Kaillera_StateSave_Retry = 0;
					Kaillera_Inp_CRC = crc32(0, &chval[0], recvLen);
					{
						//extern int time_to_reset;
						//time_to_reset = 2;
						ResetReplay();
						mame_schedule_soft_reset(Machine);
						perform_ui = 3;
						perform_ui_dat = 0;
					}
					
					break;
				case 2:						// ステートセーブ
					{
						int filen = 0;
						char filename[20];
						if(Kaillera_StateSave_Count > 0) break;
						filen = '@';
						sprintf(filename, "%s-%c", Machine->gamedrv->name, filen);
						mame_schedule_save(Machine, filename);
						perform_ui = 1;
						perform_ui_dat = (int)Kaillera_StateSave_file & 0xff;
						Kaillera_StateSave_TimeRemainder = KAILLERA_STATESAVE_NORMAL_DELAYTIME + 8;
						
						if(recvPlsub < KailleraStartOption.numplayers)
							Kaillera_StateSave_Count	= KailleraStartOption.numplayers;
						else
							Kaillera_StateSave_Count	= recvPlsub;
						
						Kaillera_Inp_StateSave_CRC = Kaillera_Inp_CRC;
					}
					break;
				case 3:
					{
						int file = 0;
						char filename[20];
						//int flag;
						if(Kaillera_StateSave_Count > 0) break;
						file = Kaillera_StateSave_file;
						
						ResetReplay();
						
						sprintf(filename, "%s-%c", Machine->gamedrv->name, file);
						mame_schedule_load(Machine, filename);
						if (GetShowSystemMessage())
							popmessage(_("Load from %s-%c.sta"), Machine->gamedrv->name, file);
						//palette_refresh();
						perform_ui = 2;
						perform_ui_dat = (int)Kaillera_StateSave_file & 0xff;
						Kaillera_StateSave_Count = 0;
						Kaillera_StateSave_Retry = 0;
						Kaillera_Inp_CRC = crc32(0, &chval[0], recvLen);
					}
					break;
				case 4:
					kPlayerDup = !kPlayerDup;
					if(kPlayerDup)	popmessage(_("Duplicate position enabled"));
					else			popmessage(_("Duplicate position disabled"));
			
					break;
				case 5:
					break;
				case 6:
			
				default:;
				}

				if( Kaillera_StateSave_TimeRemainder > 0 ) {
					Kaillera_StateSave_TimeRemainder--;
					if( Kaillera_StateSave_TimeRemainder <= 0) 
						if(Kaillera_StateSave_Count > 0) {
								popmessage(_("Savestate check timeout")); 
							Kaillera_StateSave_Count = 0;
							Kaillera_StateSave_Retry = 0;
							
						}
				}
				if( Kaillera_Send_SleepTime > 0 ) {
					Kaillera_Send_SleepTime--;
					if( Kaillera_Send_SleepTime <= 0) { 
						popmessage(_("Data reception timeout"));
						Kaillera_Send_Flags &= ~0x2;
						if( lpkChatDatabit )
							free( lpkChatDatabit );
						lpkChatDatabit = 0;
					}
				}
				
				if( KailleraChatdataPreparationcheck.timeremainder>0 ) {
					KailleraChatdataPreparationcheck.timeremainder--;
					if( KailleraChatdataPreparationcheck.timeremainder<=0 ) {
						char str[256];
						sprintf(str, _("%s timeout"), _(KailleraChatdataPreparationcheck.str));
						
						PreparationcheckClear();
						popmessage(str);
					}
				}
				
				
				if( 1<recvPlayer )
				{
					if(kPlayerDup)	//重複位置の使用
					{
						for(i=0; i<recvPlayer ; i++)
							memcpy(&val[i], &chval[i*Kaillera_Val_Len], KAILLERA_VAL_LEN);
						
						for(i=0; i<recvPlayer; i++)
						{
							kaillera_ipu_ui	=	((val[i] >> usedkeys[0]) & 0x1) |
												((val[i] >> (usedkeys[1]-1)) & 0x2) |
												((val[i] >> (usedkeys[2]-2)) & 0x4);
							
							switch ( kaillera_ipu_ui )
							{
							case 3:
								if((i % (KailleraPlayerOption.subplayer+1))
									&& KailleraPlayerOption.subplayer )
								{
									//int j;
									char s[256];
									
									j=KailleraStartOption.numplayers;
									playernmb_dup_set(i,j);
									
									getmessage_playernmb_dup(s, recvPlayer, i, _("Change"), 1);
									popmessage(s);
								}
								break;
							case 6:
								{
									//int j;
									char s[256];
									j = playernmb_dup[i];
									if(j<recvPlayer-1) j++;
									playernmb_dup_set(i,j);
							
									getmessage_playernmb_dup(s, recvPlayer, i, _("Retreat"), 0);
									popmessage(s);
								}
								break;
							case 7:
								{
									//int j;
									char s[256];
									j = playernmb_dup[i];
									if(j>0) j--;
									playernmb_dup_set(i,j);
									
									getmessage_playernmb_dup(s, recvPlayer, i, _("Advancement"), 0);
									popmessage(s);
								}
								break;
								
							default:;
							}
							
						}
						
						player_renmb_dup( (char *)chval, kValXor, recvPlayer, kMaxMahjong);
						for(i=0; i<recvPlayer ; i++)
							memcpy(&val[i], &chval[i*Kaillera_Val_Len], KAILLERA_VAL_LEN);
					} else
					{
						player_renmb( chval, Kaillera_Val_Len);
						for(i=0; i<recvPlayer ; i++)
							memcpy(&val[i], &chval[i*Kaillera_Val_Len], KAILLERA_VAL_LEN);
						for(i=0; i<recvPlayer; i++)
						{
							kaillera_ipu_ui	=	((val[i] >> usedkeys[0]) & 0x1) |
												((val[i] >> (usedkeys[1]-1)) & 0x2) |
												((val[i] >> (usedkeys[2]-2)) & 0x4);
							
							switch ( kaillera_ipu_ui )
							{
								int pl;
							case 3:
								pl = playernmb[i];
								
								if((pl % (KailleraPlayerOption.subplayer+1)) == KailleraPlayerOption.subplayer && KailleraPlayerOption.subplayer )
								{
									//int j;
									char s[256];
									j=KailleraStartOption.numplayers;
									playernmb_set(i,j);
									
									getmessage_playernmb(s, recvPlayer, pl, _("Change"), 1);
									popmessage(s);
								}
								break;
							case 6:
								{
									//int j;
									char s[256];
									pl = playernmb[i];
									j=i+1;
									if(j>=recvPlayer) j=recvPlayer-1;
									playernmb_set(i,j);
									
									getmessage_playernmb(s, recvPlayer, pl, _("Retreat"), 0);
									popmessage(s);
									break;
								}
							case 7:
								{
									//int j;
									char s[256];
									pl = playernmb[i];
									j=i-1;
									if(j<0) j=0;
									playernmb_set(i,j);
									
									getmessage_playernmb(s, recvPlayer, pl, _("Advancement"), 0);
									popmessage(s);
								break;
								}
							}
						}
					}
				}
			
				MemoryHack_KailleraInputVal(val, KailleraStartOption.numplayers);
			}			//kt end
			
			
			if(recvLen>0)
			{
				if(recvPlayer > 4) recvPlayer = 4;//kt
				
				//popmessage("%d",*((short*)&chval[0]));
				//popmessage( IntToBit((int)val[0]));
				
				//memcpy(input_port_value,kDefValues,sizeof(input_port_value));
				input_port_value_load();
				for(i=0;i<recvPlayer;i++)
				{
					//int j;
					b=1;
					for(j=0;j<16;j++)
					{
					
					
						if(kBitsPlayValues[i][j][0]!=-1)
						{
							if(kBitsPlayValues[i][j][2] != IP_ACTIVE_LOW) {
								//if(!(val[i] & b))
								if(val[i] & b)
									port_info[kBitsPlayValues[i][j][0]].digital |= kBitsPlayValues[i][j][1];
								else
									port_info[kBitsPlayValues[i][j][0]].digital &= ~kBitsPlayValues[i][j][1]; //kt
							} else {
								//if(val[i] & b)
								if(!(val[i] & b))
									port_info[kBitsPlayValues[i][j][0]].digital &= ~kBitsPlayValues[i][j][1];
								else
									port_info[kBitsPlayValues[i][j][0]].digital |= kBitsPlayValues[i][j][1]; //kt
							}
						}
					
						if(kBitsPlayValues[i][j][0+3]!=-1)
						{
							if(kBitsPlayValues[i][j][2+3] != IP_ACTIVE_LOW) {
								//if(!(val[i] & b))
								if(val[i] & b)
									port_info[kBitsPlayValues[i][j][0+3]].digital |= kBitsPlayValues[i][j][1+3];
								else
									port_info[kBitsPlayValues[i][j][0+3]].digital &= ~kBitsPlayValues[i][j][1+3]; //kt
							} else {
								//if(val[i] & b)
								if(!(val[i] & b))
									port_info[kBitsPlayValues[i][j][0+3]].digital &= ~kBitsPlayValues[i][j][1+3];
								else
									port_info[kBitsPlayValues[i][j][0+3]].digital |= kBitsPlayValues[i][j][1+3]; //kt
							}
						}
						
						b=b<<1;
					}
					//kt s
					//popmessage( "%08x,%08x", val[0],val[1]);	//kt test
				
					b=0;
					for(j=0;j<16;j++) {
						if(kAnalogPlayValues[i][j]!=-1) {
							memcpy(&kinput_analog_delta[kAnalogPlayValues[i][j]][1], &chval[KAILLERA_VAL_LEN+(b++)*KAILLERA_ANALOG_LEN+i*Kaillera_Val_Len], KAILLERA_ANALOG_LEN);
						}
					}
			
					if(kMaxMahjong)
						for(j=0;j<29;j++)
						{
							if(kMahjongPlayValues[i][j][0]!=-1)
							{
								if(kMahjongPlayValues[i][j][2] != IP_ACTIVE_LOW) {
									//if(chval[KAILLERA_VAL_LEN + Kaillera_analog_port*KAILLERA_ANALOG_LEN + i*Kaillera_Val_Len] == j+1)
									if(chval[KAILLERA_VAL_LEN + Kaillera_analog_port*KAILLERA_ANALOG_LEN + i*Kaillera_Val_Len] != j+1)
										port_info[kMahjongPlayValues[i][j][0]].digital |= kMahjongPlayValues[i][j][1];
									else
										port_info[kMahjongPlayValues[i][j][0]].digital &= ~kMahjongPlayValues[i][j][1];
								} else {
									//if(chval[KAILLERA_VAL_LEN + Kaillera_analog_port*KAILLERA_ANALOG_LEN + i*Kaillera_Val_Len] == j+1)
									if(chval[KAILLERA_VAL_LEN + Kaillera_analog_port*KAILLERA_ANALOG_LEN + i*Kaillera_Val_Len] != j+1)
										port_info[kMahjongPlayValues[i][j][0]].digital &= ~kMahjongPlayValues[i][j][1];
									else
										port_info[kMahjongPlayValues[i][j][0]].digital |= kMahjongPlayValues[i][j][1];
								}
							}
						}
						//kt e
				}
			}
#if 0
			if(recvLen==0)
				//memcpy(kDefValues,input_port_value,sizeof(input_port_value));
				input_port_value_save();
			
			if(recvPlayer < 4 && (recvLen>0 || Kaillera_Inp_InitSleepTime>0) )
				for(i=recvPlayer;i<4;i++)
				{
					//int j;
					for(j=0;j<16;j++)
					{
						if(kBitsPlayValues[i][j][0]!=-1)
						{
							if(kBitsPlayValues[i][j][2] != IP_ACTIVE_LOW) {
								port_info[kBitsPlayValues[i][j][0]].digital &= ~kBitsPlayValues[i][j][1]; //kt
							} else {
								port_info[kBitsPlayValues[i][j][0]].digital |= kBitsPlayValues[i][j][1]; //kt
							}
						}
						
						if(kBitsPlayValues[i][j][0+3]!=-1)
						{
							if(kBitsPlayValues[i][j][2+3] != IP_ACTIVE_LOW) {
								port_info[kBitsPlayValues[i][j][0+3]].digital &= ~kBitsPlayValues[i][j][1+3]; //kt
							} else {
								port_info[kBitsPlayValues[i][j][0+3]].digital |= kBitsPlayValues[i][j][1+3]; //kt
						}
					}
				}

			for(j=0;j<16;j++) {
				if(kAnalogPlayValues[i][j]!=-1) {
					kinput_analog_delta[kAnalogPlayValues[i][j]][1] = 0;
				}
			}
			
			if(kMaxMahjong)
				for(j=0;j<27;j++)
				{
					if(kMahjongPlayValues[i][j][0]!=-1)
					{
						if(kMahjongPlayValues[i][j][2] != IP_ACTIVE_LOW) {
							port_info[kMahjongPlayValues[i][j][0]].digital &= ~kMahjongPlayValues[i][j][1];
						} else {
							port_info[kMahjongPlayValues[i][j][0]].digital |= kMahjongPlayValues[i][j][1];
						}
					}
				}
#endif
		}
	}
	else
	{
		if (kFirstMsg)
		{
			const char *stemp = options_get_string(mame_options(), "auto_record_name");
			kFirstMsg=0;
			if( Machine->record_file != NULL && stemp && strlen(stemp) > 0 )
			{
				popmessage(_("Play Game With Record Input Log As %s"), stemp);
			}
		}
	}
#endif /* KAILLERA */

	/* call changed handlers */
	for (portnum = 0; portnum < MAX_INPUT_PORTS; portnum++)
		if (port_info[portnum].change_notify != NULL)
		{
			changed_callback_info *cbinfo;
			UINT32 newvalue = readinputport(portnum);
			UINT32 oldvalue = port_info[portnum].changed_last_value;
			UINT32 delta = newvalue ^ oldvalue;

			/* call all the callbacks whose mask matches the requested mask */
			for (cbinfo = port_info[portnum].change_notify; cbinfo; cbinfo = cbinfo->next)
				if (delta & cbinfo->mask)
					(*cbinfo->callback)(cbinfo->param, oldvalue & cbinfo->mask, newvalue & cbinfo->mask);

			port_info[portnum].changed_last_value = newvalue;
		}

	/* handle playback/record */
	for (portnum = 0; portnum < MAX_INPUT_PORTS; portnum++)
	{
		/* analog ports automatically update on every read */
		if (port_info[portnum].analoginfo)
			readinputport(portnum);

		/* non-analog ports must be manually updated */
		else
			update_playback_record(portnum, readinputport(portnum));
	}


#ifdef KAILLERA
	if (Machine->playback_sub_file)
	{
		input_ui_temp = 0;
		perform_ui_count--;
		if(perform_ui_count <= 0)
		{
			readword(&Machine->playback_sub_file,(short*)&perform_ui);
			readword(&Machine->playback_sub_file,(short*)&perform_ui_dat );
			readword(&Machine->playback_sub_file,((short*)&perform_ui_dat)+1 );
			readword(&Machine->playback_sub_file,(short*)&perform_ui_count);
			perform_ui_count &= 0xffff;
			perform_ui &= 0xffff;
			if (!Machine->playback_sub_file)
			{
				//options.playbacksub = NULL;
				//perform_ui = 0;
			}
			input_ui_temp = perform_ui;
			input_ui_temp_dat[0] = perform_ui_dat;
		}
		//popmessage("pui_count %d",perform_ui_count );
	}

	{
		switch (input_ui_temp)
		{
		case 1:	//ステートセーブ
			{
				//int flag;
				char filename[20];
				sprintf(filename, "%s-%c", Machine->gamedrv->name, input_ui_temp_dat[0]);
				mame_schedule_save(Machine, filename);
				if (!Machine->playback_sub_file && GetShowSystemMessage()) popmessage(_("Saved Name As %c"), input_ui_temp_dat[0]);
			}
			perform_ui = input_ui_temp;
			perform_ui_dat = (int)input_ui_temp_dat[0] & 0xff;
			break;
		case 2:	//ロード
			{
				//int flag;
				char filename[20];

				ResetReplay();

				sprintf(filename, "%s-%c", Machine->gamedrv->name, input_ui_temp_dat[0]);
				mame_schedule_load(Machine, filename);
				//palette_refresh();
				if (GetShowSystemMessage())
					popmessage(_("Load from %s-%c.sta"), Machine->gamedrv->name, input_ui_temp_dat[0]);
			}
			perform_ui = input_ui_temp;
			perform_ui_dat = (int)input_ui_temp_dat[0] & 0xff;
			break;
		case 3:	//リセット
			{
				//extern int time_to_reset;
				//time_to_reset = 2;
				ResetReplay();
				mame_schedule_soft_reset(Machine);
			}
			perform_ui = input_ui_temp;
			perform_ui_dat = 0;
			break;

		default:;
		}
		input_ui_temp = 0;
	}

	if (Machine->record_sub_file)
	{
		perform_ui_count++;
		if(perform_ui || (perform_ui_count >= 0xffff))
		{
			int i;
			writeword(&Machine->record_sub_file,(short)(perform_ui_count&0xffff));
			writeword(&Machine->record_sub_file,(short)(perform_ui&0xffff));
			writeword(&Machine->record_sub_file,(short)(perform_ui_dat&0xffff) );
			writeword(&Machine->record_sub_file,(short)((perform_ui_dat>>16)&0xffff));
			perform_ui_count = 0;

			switch (perform_ui)
			{
			case 1:	//ステートセーブ
				for(i=0; i<perform_ui_statesave_file_size+1; i++)
				{
					if(perform_ui_statesave_file[i] == (char)perform_ui_dat) { i=65535; break;}
				}
				if( i<65535 )
					perform_ui_statesave_file[perform_ui_statesave_file_size++] = (char)perform_ui_dat;
				if(perform_ui_statesave_file_size > 255) perform_ui_statesave_file_size=255;
				break;

			}
		}
	}
#endif

#ifdef USE_SHOW_INPUT_LOG
	/* show input log */
	if (show_input_log && (!Machine->playback_file))
		make_input_log();
#endif /* USE_SHOW_INPUT_LOG */

profiler_mark(PROFILER_END);
}



/*************************************
 *
 *  VBLANK end routine
 *
 *************************************/

void input_port_vblank_end(void)
{
	int ui_visible = ui_is_menu_active() || ui_is_slider_active();
	int port;

profiler_mark(PROFILER_INPUT);

	/* update all analog ports if the UI isn't visible */
	if (!ui_visible)
		for (port = 0; port < MAX_INPUT_PORTS; port++)
			update_analog_port(port);

profiler_mark(PROFILER_END);
}



/*************************************
 *
 *  Digital joystick updating
 *
 *************************************/

static void update_digital_joysticks(void)
{
	int player, joyindex;

	/* loop over all the joysticks */
	for (player = 0; player < MAX_PLAYERS; player++)
		for (joyindex = 0; joyindex < DIGITAL_JOYSTICKS_PER_PLAYER; joyindex++)
		{
			digital_joystick_info *info = &joystick_info[player][joyindex];
			if (info->inuse)
			{
				info->previous = info->current;
				info->current = 0;

				/* read all the associated ports */
				if (info->port[JOYDIR_UP] != NULL && input_seq_pressed(input_port_seq(info->port[JOYDIR_UP], SEQ_TYPE_STANDARD)))
					info->current |= JOYDIR_UP_BIT;
				if (info->port[JOYDIR_DOWN] != NULL && input_seq_pressed(input_port_seq(info->port[JOYDIR_DOWN], SEQ_TYPE_STANDARD)))
					info->current |= JOYDIR_DOWN_BIT;
				if (info->port[JOYDIR_LEFT] != NULL && input_seq_pressed(input_port_seq(info->port[JOYDIR_LEFT], SEQ_TYPE_STANDARD)))
					info->current |= JOYDIR_LEFT_BIT;
				if (info->port[JOYDIR_RIGHT] != NULL && input_seq_pressed(input_port_seq(info->port[JOYDIR_RIGHT], SEQ_TYPE_STANDARD)))
					info->current |= JOYDIR_RIGHT_BIT;

				/* lock out opposing directions (left + right or up + down) */
				if ((info->current & (JOYDIR_UP_BIT | JOYDIR_DOWN_BIT)) == (JOYDIR_UP_BIT | JOYDIR_DOWN_BIT))
					info->current &= ~(JOYDIR_UP_BIT | JOYDIR_DOWN_BIT);
				if ((info->current & (JOYDIR_LEFT_BIT | JOYDIR_RIGHT_BIT)) == (JOYDIR_LEFT_BIT | JOYDIR_RIGHT_BIT))
					info->current &= ~(JOYDIR_LEFT_BIT | JOYDIR_RIGHT_BIT);

				/* only update 4-way case if joystick has moved */
				if (info->current != info->previous)
				{
					info->current4way = info->current;

					/*
                        If joystick is pointing at a diagonal, acknowledge that the player moved
                        the joystick by favoring a direction change.  This minimizes frustration
                        when using a keyboard for input, and maximizes responsiveness.

                        For example, if you are holding "left" then switch to "up" (where both left
                        and up are briefly pressed at the same time), we'll transition immediately
                        to "up."

                        Zero any switches that didn't change from the previous to current state.
                     */
					if ((info->current4way & (JOYDIR_UP_BIT | JOYDIR_DOWN_BIT)) &&
						(info->current4way & (JOYDIR_LEFT_BIT | JOYDIR_RIGHT_BIT)))
					{
						info->current4way ^= info->current4way & info->previous;
					}

					/*
                        If we are still pointing at a diagonal, we are in an indeterminant state.

                        This could happen if the player moved the joystick from the idle position directly
                        to a diagonal, or from one diagonal directly to an extreme diagonal.

                        The chances of this happening with a keyboard are slim, but we still need to
                        constrain this case.

                        For now, just resolve randomly.
                     */
					if ((info->current4way & (JOYDIR_UP_BIT | JOYDIR_DOWN_BIT)) &&
						(info->current4way & (JOYDIR_LEFT_BIT | JOYDIR_RIGHT_BIT)))
					{
						if (mame_rand(Machine) & 1)
							info->current4way &= ~(JOYDIR_LEFT_BIT | JOYDIR_RIGHT_BIT);
						else
							info->current4way &= ~(JOYDIR_UP_BIT | JOYDIR_DOWN_BIT);
					}
				}
			}
		}
}



/*************************************
 *
 *  Analog minimum/maximum clamping
 *
 *************************************/

INLINE INT32 apply_analog_min_max(const analog_port_info *info, double value)
{
	const input_port_entry *port = info->port;
	double adjmax, adjmin, adj1, adjdif;

	/* take the analog minimum and maximum values and apply the inverse of the */
	/* sensitivity so that we can clamp against them before applying sensitivity */
	adjmin = APPLY_INVERSE_SENSITIVITY(info->minimum, port->analog.sensitivity);
	adjmax = APPLY_INVERSE_SENSITIVITY(info->maximum, port->analog.sensitivity);
	adj1   = APPLY_INVERSE_SENSITIVITY(512, port->analog.sensitivity);

	/* for absolute devices, clamp to the bounds absolutely */
	if (!info->wraps)
	{
		if (value > adjmax)
			value = adjmax;
		else if (value < adjmin)
			value = adjmin;
	}

	/* for relative devices, wrap around when we go past the edge */
	else
	{
		adjdif = adjmax - adjmin + adj1;
		if (port->analog.reverse)
		{
			while (value <= adjmin - adj1)
				value += adjdif;
			while (value > adjmax)
				value -= adjdif;
		}
		else
		{
			while (value >= adjmax + adj1)
				value -= adjdif;
			while (value < adjmin)
				value += adjdif;
		}
	}

	return value;
}



/*************************************
 *
 *  Analog port updating
 *
 *************************************/

static void update_analog_port(int portnum)
{
	analog_port_info *info;

	/* loop over all analog ports in this port number */
	for (info = port_info[portnum].analoginfo; info != NULL; info = info->next)
	{
		input_port_entry *port = info->port;
		INT32 rawvalue;
		double delta = 0, keyscale;
		int keypressed = 0;
		input_item_class itemclass;

		/* clamp the previous value to the min/max range and remember it */
		info->previous = info->accum = apply_analog_min_max(info, info->accum);

		/* get the new raw analog value and its type */
		rawvalue = input_seq_axis_value(input_port_seq(port, SEQ_TYPE_STANDARD), &itemclass);

#ifdef KAILLERA
		if (!kPlay)
#endif /* KAILLERA */
		/* if we got an absolute input, it overrides everything else */
		if (itemclass == ITEM_CLASS_ABSOLUTE)
		{
			if (info->previousanalog != rawvalue)
			{
				/* only update if analog value changed */
				info->previousanalog = rawvalue;

				/* apply the inverse of the sensitivity to the raw value so that */
				/* it will still cover the full min->max range requested after */
				/* we apply the sensitivity adjustment */
				if (info->absolute || port->analog.reset)
				{
					/* if port is absolute, then just return the absolute data supplied */
					info->accum = APPLY_INVERSE_SENSITIVITY(rawvalue, port->analog.sensitivity);
				}
				else if (info->positionalscale != 0)
				{
					/* if port is positional, we will take the full analog control and divide it */
					/* into positions, that way as the control is moved full scale, */
					/* it moves through all the positions */
					rawvalue = info->positionalscale * (rawvalue - INPUT_ABSOLUTE_MIN) * 512  + info->minimum;

					/* clamp the high value so it does not roll over */
					if (rawvalue > info->maximum) rawvalue = info->maximum;
					info->accum = APPLY_INVERSE_SENSITIVITY(rawvalue, port->analog.sensitivity);
				}
				else
					/* if port is relative, we use the value to simulate the speed of relative movement */
					/* sensitivity adjustment is allowed for this mode */
					info->accum += rawvalue;

				info->lastdigital = 0;
				/* do not bother with other control types if the analog data is changing */
				return;
			}
			else
			{
				/* we still have to update fake relative from joystick control */
				if (!info->absolute && info->positionalscale == 0) info->accum += rawvalue;
			}
		}

		/* if we got it from a relative device, use that as the starting delta */
		/* also note that the last input was not a digital one */
		if (itemclass == ITEM_CLASS_RELATIVE && rawvalue != 0)
		{
			delta = rawvalue;
			info->lastdigital = 0;
		}

		keyscale = (info->accum >= 0) ? info->keyscalepos : info->keyscaleneg;

		/* if the decrement code sequence is pressed, add the key delta to */
		/* the accumulated delta; also note that the last input was a digital one */
		if (input_seq_pressed(input_port_seq(info->port, SEQ_TYPE_DECREMENT)))
		{
			keypressed = 1;
			if (port->analog.delta)
				delta -= (double)(port->analog.delta) * keyscale;
			else if (info->lastdigital != 1)
				/* decrement only once when first pressed */
				delta -= keyscale;
			info->lastdigital = 1;
		}

		/* same for the increment code sequence */
		if (input_seq_pressed(input_port_seq(info->port, SEQ_TYPE_INCREMENT)))
		{
			keypressed = 1;
			if (port->analog.delta)
				delta += (double)(port->analog.delta) * keyscale;
			else if (info->lastdigital != 2)
				/* increment only once when first pressed */
				delta += keyscale;
			info->lastdigital = 2;
		}

		/* if resetting is requested, clear the accumulated position to 0 before */
		/* applying the deltas so that we only return this frame's delta */
		/* note that centering only works for relative controls */
		/* no need to check if absolute here because it is checked by the validity tests */
		if (port->analog.reset)
			info->accum = 0;

		#if 0 //def KAILLERA
		if(kPlay) { //kt
			//UINT8 portnum = port->condition.portnum;
			#if KAILLERA_ANALOG_LEN == 4
			kinput_analog_delta[portnum][0] = delta;
			delta = kinput_analog_delta[portnum][1];
			#endif
			#if KAILLERA_ANALOG_LEN == 2
			kinput_analog_delta[portnum][0] = (short)(delta & 0xffff);
			delta = ((int)kinput_analog_delta[portnum][1] << 16 >> 16);
			//if (-512 < delta && delta < 512) delta=0;
			//kinput_analog_delta[portnum][0] = (short)((delta >> 9) && 0xffff);
			//delta = ((int)kinput_analog_delta[portnum][1] << 16 >> 7);
			#endif
		}
		#endif

		/* apply the delta to the accumulated value */
		info->accum += delta;

#ifdef KAILLERA
		if (!kPlay)
		{
#endif /* KAILLERA */
		/* if our last movement was due to a digital input, and if this control */
		/* type autocenters, and if neither the increment nor the decrement seq */
		/* was pressed, apply autocentering */
		if (info->autocenter)
		{
			double center = APPLY_INVERSE_SENSITIVITY(info->center, port->analog.sensitivity);
			if (info->lastdigital && !keypressed)
			{
				/* autocenter from positive values */
				if (info->accum >= center)
				{
					info->accum -= (double)(port->analog.centerdelta) * info->keyscalepos;
					if (info->accum < center)
					{
						info->accum = center;
						info->lastdigital = 0;
					}
				}

				/* autocenter from negative values */
				else
				{
					info->accum += (double)(port->analog.centerdelta) * info->keyscaleneg;
					if (info->accum > center)
					{
						info->accum = center;
						info->lastdigital = 0;
					}
				}
			}
		}
		else if (!keypressed)
			info->lastdigital = 0;
#ifdef KAILLERA
		}
#endif /* KAILLERA */
	}
}


UINT32 readinputportbytag_safe(const char *tag, UINT32 defvalue)
{
	int port = port_tag_to_index(tag);
	if (port != -1)
		return readinputport(port);
	return defvalue;
}

/*************************************
 *
 *  Analog port interpolation
 *
 *************************************/

static void interpolate_analog_port(int portnum)
{
	analog_port_info *info;

profiler_mark(PROFILER_INPUT);

	/* set the default mask and value */
	port_info[portnum].analogmask = 0;
	port_info[portnum].analog = 0;

	/* loop over all analog ports in this port number */
	for (info = port_info[portnum].analoginfo; info != NULL; info = info->next)
	{
		input_port_entry *port = info->port;

		if (input_port_condition(info->port))
		{
			double current;
			INT32 value;

			/* interpolate or not */
			if (info->interpolate && !port->analog.reset)
				current = info->previous + cpu_scalebyfcount(info->accum - info->previous);
			else
				current = info->accum;

			/* apply the min/max and then the sensitivity */
			current = apply_analog_min_max(info, current);
			current = APPLY_SENSITIVITY(current, port->analog.sensitivity);

			/* apply reversal if needed */
			if (port->analog.reverse)
					current = info->reverse_val - current;
			else
			if (info->single_scale)
				/* it's a pedal or the default value is equal to min/max */
				/* so we need to adjust the center to the minimum */
				current -= INPUT_ABSOLUTE_MIN;

			/* map differently for positive and negative values */
			if (current >= 0 )
				value = (INT32)(current * info->scalepos);
			else
				value = (INT32)(current * info->scaleneg);
			value += port->default_value;

			/* store croshair position before any remapping */
			info->crosshair_pos = (INT32)value & (port->mask >> info->shift);

			/* remap the value if needed */
			if (port->analog.remap_table)
				value = info->port->analog.remap_table[value];

			/* invert bits if needed */
			if (port->analog.invert)
				value = ~value;

			/* insert into the port */
			port_info[portnum].analogmask |= info->port->mask;
			port_info[portnum].analog = (port_info[portnum].analog & ~port->mask) | ((value << info->shift) & port->mask);
		}
	}

	/* store speed read from INP file, if extended INP */
	if (Machine->playback_file != NULL && !no_extended_inp)
	{
		long dummy;
		mame_fread(Machine->playback_file,&rec_speed,sizeof(double));
		mame_fread(Machine->playback_file,&dummy,sizeof(long));
		framecount++;
		rec_speed *= 100;
		totalspeed += rec_speed;
	}

profiler_mark(PROFILER_END);
}



/*************************************
 *
 *  Input port reading
 *
 *************************************/

UINT32 readinputport(int port)
{
	input_port_info *portinfo = &port_info[port];
	custom_port_info *custom;
	UINT32 result;

	/* interpolate analog values */
	interpolate_analog_port(port);

	/* update custom values */
	for (custom = portinfo->custominfo; custom; custom = custom->next)
		if (input_port_condition(custom->port))
		{
			/* replace the bits with bits from the custom routine */
			input_port_entry *port = custom->port;
			portinfo->digital &= ~port->mask;
			portinfo->digital |= ((*port->custom)(port->custom_param) << custom->shift) & port->mask;
		}

	/* compute the current result: default value XOR the digital, merged with the analog */
	result = ((portinfo->defvalue ^ portinfo->digital) & ~portinfo->analogmask) | portinfo->analog;

	/* if we have analog data, update the recording state */
	if (port_info[port].analoginfo)
		update_playback_record(port, result);

	/* if we're playing back, use the recorded value for inputs instead */
	if (Machine->playback_file != NULL)
		result = portinfo->playback;

	/* handle VBLANK bits after inputs */
	if (portinfo->vblank)
	{
		/* reset the VBLANK bits to their default value, regardless of inputs */
		result = (result & ~portinfo->vblank) | (portinfo->defvalue & portinfo->vblank);

		/* toggle VBLANK if we're in a VBLANK state */
		if (Machine->screen[0].oldstyle_vblank_supplied)
		{
			int cpu_getvblank(void);

			if (cpu_getvblank())
				result ^= portinfo->vblank;
		}
		else
		{
			if (video_screen_get_vblank(0))
				result ^= portinfo->vblank;
		}
	}
	return result;
}


UINT32 readinputportbytag(const char *tag)
{
	int port = port_tag_to_index(tag);
	if (port != -1)
		return readinputport(port);

	/* otherwise fail horribly */
	fatalerror("Unable to locate input port '%s'", tag);
	return -1;
}

static int auto_pressed(input_bit_info *info)
{
/*
	autofire setting:
	 delay,  on, off
	     1,   1,   1
	     2,   2,   1
	     3,   2,   2
	     4,   3,   2
	     5,   3,   3
	     6,   4,   3
*/
	input_port_entry *port = info->port;
	int pressed = input_seq_pressed(input_port_seq(port, SEQ_TYPE_STANDARD));
	int is_auto = IS_AUTOKEY(port);

#ifdef USE_CUSTOM_BUTTON

	if (port->type >= IPT_BUTTON1 && port->type < IPT_BUTTON1 + MAX_NORMAL_BUTTONS)
{
		UINT16 button_mask = 1 << (port->type - IPT_BUTTON1);
		input_bit_info temp;
		int custom;

		for (custom = 0; custom < custom_buttons; custom++)
			if (custom_button[port->player][custom] & button_mask)
			{
				input_bit_info *custom_info = custom_button_info[port->player][custom];

				if (input_seq_pressed(input_port_seq(custom_info->port, SEQ_TYPE_STANDARD)))
				{
					if (IS_AUTOKEY(custom_info->port))
					{
						if (pressed)
							is_auto &= 1;
						else
							is_auto = 1;

						temp = *custom_info;
						info = &temp;
					}
					else
						is_auto = 0;

					pressed = 1;
				}
			}
	}
#endif /* USE_CUSTOM_BUTTON */

	if (is_auto)
	{
		if (pressed)
		{
			if (info->autopressed > autofiredelay[port->player])
				info->autopressed = 0;
			else if (info->autopressed > autofiredelay[port->player] / 2)
				pressed = 0;

			info->autopressed++;
		}
		else
			info->autopressed = 0;
	}

	return pressed;
}

int get_autofiredelay(int player)
{
	return autofiredelay[player];
}

void set_autofiredelay(int player, int delay)
{
	autofiredelay[player] = delay;
}



/*************************************
 *
 *  Input port writing
 *
 *************************************/

void input_port_set_digital_value(int portnum, UINT32 value, UINT32 mask)
{
	input_port_info *portinfo = &port_info[portnum];
	portinfo->digital &= ~mask;
	portinfo->digital |= value;
}



/*************************************
 *
 *  Input port callbacks
 *
 *************************************/

void input_port_set_changed_callback(int port, UINT32 mask, void (*callback)(void *, UINT32, UINT32), void *param)
{
	input_port_info *portinfo = &port_info[port];
	changed_callback_info *cbinfo;

	assert_always(mame_get_phase(Machine) == MAME_PHASE_INIT, "Can only call input_port_set_changed_callback() at init time!");
	assert_always((port >= 0) && (port < MAX_INPUT_PORTS), "Invalid port number passed to input_port_set_changed_callback()!");

	cbinfo = auto_malloc(sizeof(*cbinfo));
	cbinfo->next = portinfo->change_notify;
	cbinfo->mask = mask;
	cbinfo->callback = callback;
	cbinfo->param = param;

	portinfo->change_notify = cbinfo;
}



/*************************************
 *
 *  Return position of crosshair axis
 *
 *************************************/

UINT32 get_crosshair_pos(int port_num, UINT8 player, UINT8 axis)
{
	input_port_info *portinfo = &port_info[port_num];
	analog_port_info *info;
	input_port_entry *port;
	UINT32 result = 0;

	for (info = portinfo->analoginfo; info; info = info->next)
	{
		port = info->port;
		if (port->player == player && port->analog.crossaxis == axis)
		{
			result = info->crosshair_pos;
			break;
		}
	}

	return result;
}


/*-------------------------------------------------
    autoselect_device - autoselect a single
    device based on the input port list passed
    in and the corresponding option
-------------------------------------------------*/

static void autoselect_device(const input_port_entry *ipt, int type1, int type2, int type3, const char *option, const char *ananame)
{
	const char *stemp = options_get_string(mame_options(), option);
	input_device_class autoenable = DEVICE_CLASS_KEYBOARD;
	const char *autostring = "keyboard";

	/* if nothing specified, ignore the option */
	if (stemp[0] == 0)
		return;

	/* extract valid strings */
	if (strcmp(stemp, "mouse") == 0)
	{
		autoenable = DEVICE_CLASS_MOUSE;
		autostring = "mouse";
	}
	else if (strcmp(stemp, "joystick") == 0)
	{
		autoenable = DEVICE_CLASS_JOYSTICK;
		autostring = "joystick";
	}
	else if (strcmp(stemp, "lightgun") == 0)
	{
		autoenable = DEVICE_CLASS_LIGHTGUN;
		autostring = "lightgun";
	}
	else if (strcmp(stemp, "none") == 0)
	{
		/* nothing specified */
		return;
	}
	else if (strcmp(stemp, "keyboard") != 0)
		mame_printf_error(_("Invalid %s value %s; reverting to keyboard\n"), option, stemp);

	/* only scan the list if we haven't already enabled this class of control */
	if (!input_device_class_enabled(autoenable))
		for ( ; ipt->type != IPT_END; ipt++)

			/* if this port type is in use, apply the autoselect criteria */
			if ((type1 != 0 && ipt->type == type1) ||
				(type2 != 0 && ipt->type == type2) ||
				(type3 != 0 && ipt->type == type3))
			{
				mame_printf_verbose(_("Input: Autoenabling %s due to presence of a %s\n"), autostring, ananame);
				input_device_class_enable(autoenable, TRUE);
				break;
			}
}

#ifdef USE_SHOW_INPUT_LOG
INLINE void copy_command_buffer(char log)
{
	char buf[UTF8_CHAR_MAX + 1];
	unicode_char uchar;
	int len;

	for (len = 0; command_buffer[len].code; len++)
		;

	if (len >= ARRAY_LENGTH(command_buffer) - 1)
	{
		int i;

		for (i = 0; command_buffer[i + 1].code; i++)
			command_buffer[i] = command_buffer[i + 1];

		command_buffer[--len].code = '\0';
	}

	buf[0] = '_';
	buf[1] = log;
	buf[2] = '\0';
	convert_command_glyph(buf, ARRAY_LENGTH(buf));

	if (uchar_from_utf8(&uchar, buf, ARRAY_LENGTH(buf)) == -1)
		return;

	command_buffer[len].code = uchar;
	command_buffer[len].time = mame_time_to_double(mame_timer_get_time());
	command_buffer[++len].code = '\0';
}

static void make_input_log(void)
{
	input_port_entry *port = Machine->input_ports;
	int player = 0; /* player 1 */

	if (port == 0)
		return;

	/* loop over all the joysticks for player 1*/
	if (player == 0) /* player 1 */
	{
		int joyindex;
		static int old_dir = -1;
		static int now_dir = 0;

		for (joyindex = 0; joyindex < DIGITAL_JOYSTICKS_PER_PLAYER; joyindex++)
		{
			digital_joystick_info *info = &joystick_info[player][joyindex];

			if (info->inuse)
			{
				/* set the status of neutral (assumed to be only in the defaults) */
				now_dir = 0;

				/* if this is a digital joystick type, apply 4-way rules */
				switch(info->current)
				{
					case JOYDIR_DOWN_BIT:
						now_dir = 2;
						break;
					case JOYDIR_LEFT_BIT:
						now_dir = 4;
						break;
					case JOYDIR_RIGHT_BIT:
						now_dir = 6;
						break;
					case JOYDIR_UP_BIT:
						now_dir = 8;
						break;
				}

				/* if this is a digital joystick type, apply 8-way rules */
				//if (port->way == 8)
				switch(info->current)
				{
					case JOYDIR_DOWN_BIT | JOYDIR_LEFT_BIT:
						now_dir = 1;
						break;
					case JOYDIR_DOWN_BIT | JOYDIR_RIGHT_BIT:
						now_dir = 3;
						break;
					case JOYDIR_UP_BIT | JOYDIR_LEFT_BIT:
						now_dir = 7;
						break;
					case JOYDIR_UP_BIT | JOYDIR_RIGHT_BIT:
						now_dir = 9;
						break;
				}

				/* if we're not pressed, reset old_dir = -1 */
				if (now_dir == 0)
					old_dir = -1;
			}
		}

		/* if this is the first press, show input log */
		if (old_dir != now_dir)
		{
			if (now_dir != 0)
			{
				char colorbutton = '0';
				copy_command_buffer(colorbutton + now_dir);
				old_dir = now_dir;
			}
		}
	}
	/* End of loop over all the joysticks for player 1*/


	/* loop over all the buttons */
	if (normal_buttons > 0)
	{
		int is_neogeo = !mame_stricmp(Machine->gamedrv->source_file+17, "neogeo.c")
		                || !mame_stricmp(Machine->gamedrv->source_file+17, "neodrvr.c");
		static UINT16 old_btn = 0;
		static UINT16 now_btn;
		int is_pressed = 0;

		now_btn = 0;

		for (port = Machine->input_ports; port->type != IPT_END; port++)
		{
			/* if this is current player, read input port */
			if (port->player == player)
			{
				int i;

				/* if this is normal buttons type, apply usable buttons */
				for (i = 0; i < normal_buttons; i++)
					if (input_seq_pressed(input_port_seq(port, SEQ_TYPE_STANDARD)) && port->type == IPT_BUTTON1 + i)
						now_btn |= 1 << (port->type - IPT_BUTTON1);

				/* if this is start button type */
				if ((input_seq_pressed(input_port_seq(port, SEQ_TYPE_STANDARD)) && port->type == IPT_START1) ||
					(input_seq_pressed(input_port_seq(port, SEQ_TYPE_STANDARD)) && port->type == IPT_START))
						now_btn |= 1 << normal_buttons;

				/* if this is select button type (MESS only) */
				if (input_seq_pressed(input_port_seq(port, SEQ_TYPE_STANDARD)) && port->type == IPT_SELECT)
					now_btn |= 1 << (normal_buttons + 1);

#ifdef USE_CUSTOM_BUTTON
				/* if this is custon buttons type, apply usable buttons */
				for (i = 0; i < custom_buttons; i++)
					if (custom_button[0][i] != 0)
					{
						input_bit_info *custom_info = custom_button_info[0][i];

						if (input_seq_pressed(input_port_seq(custom_info->port, SEQ_TYPE_STANDARD)))
							now_btn |= custom_button[0][i];
					}

				/* if buttons press, leave is_pressed = 1 */
				if (now_btn != 0)
					is_pressed |= 1;
#endif /* USE_CUSTOM_BUTTON */
			}
		}

		/* if we're not pressed, reset old_btn = -1 */
		if (!is_pressed)
			old_btn = 1 << (normal_buttons + 2);

		/* if this is the first press, show input log */
		if (old_btn != now_btn)
		{
			if (now_btn != 0)
			{
				/* if this is Neo-Geo games, than alphabetic button type */
				/*                           else numerical  button type */
				char colorbutton = is_neogeo ? 'A' : 'a';
				int n = 1;
				int i;

				for (i = 0; i < normal_buttons; i++, n <<= 1)
				{
					if ((now_btn & n) != 0 && (old_btn & n) == 0)
						copy_command_buffer(colorbutton + i);
				}

				/* if this is start button */
				if (now_btn & 1 << normal_buttons)
					copy_command_buffer('S');

				/* if this is select button (MESS only) */
				if (now_btn & 1 << (normal_buttons + 1))
					copy_command_buffer('s');

				old_btn = now_btn;
				now_btn = 0;
			}
		}
	}
	/* End of loop over all the buttons */
}
#endif /* USE_SHOW_INPUT_LOG */
