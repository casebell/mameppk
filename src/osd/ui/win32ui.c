/***************************************************************************

  M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
  Win32 Portions Copyright (C) 1997-2003 Michael Soderstrom and Chris Kirmse

  This file is part of MAME32, and may only be used, modified and
  distributed under the terms of the MAME license, in "readme.txt".
  By continuing to use, modify or distribute this file you indicate
  that you have read the license and understand and accept it fully.

 ***************************************************************************/

 /***************************************************************************

  win32ui.c

  Win32 GUI code.

  Created 8/12/97 by Christopher Kirmse (ckirmse@ricochet.net)
  Additional code November 1997 by Jeff Miller (miller@aa.net)
  More July 1998 by Mike Haaland (mhaaland@hypertech.com)
  Added Spitters/Property Sheets/Removed Tabs/Added Tree Control in
  Nov/Dec 1998 - Mike Haaland

***************************************************************************/
#ifdef KAILLERA
#define MULTISESSION 1
#else /* KAILLERA */
#define MULTISESSION 0
#endif /* KAILLERA */

#ifdef _MSC_VER
#undef NONAMELESSUNION
#define NONAMELESSUNION
#endif

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <stdio.h>
#include <ctype.h>
#include <io.h>
#include <fcntl.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dlgs.h>
#include <string.h>
#include <wchar.h>
#include <wingdi.h>
#include <time.h>

#include "mame32.h"
#include "driver.h"
#include "osdepend.h"
#include "unzip.h"

#include "resource.h"
#include "resource.hm"

#include "screenshot.h"
#include "M32Util.h"
#include "file.h"
#include "audit32.h"
#include "Directories.h"
#include "Properties.h"
#include "ColumnEdit.h"
#include "picker.h"
#include "tabview.h"
#include "bitmask.h"
#include "TreeView.h"
#include "Splitters.h"
#include "help.h"
#include "history.h"
#include "winuiopt.h"
#include "dialogs.h"
#include "windows/input.h"
#include "winmain.h"
#include "windows/window.h"
#ifdef UI_COLOR_PALETTE
#include "paletteedit.h"
#endif /* UI_COLOR_PALETTE */
#include "translate.h"
#ifdef IMAGE_MENU
#include "imagemenu.h"
#endif /* IMAGE_MENU */

#undef rand

#include "DirectDraw.h"
#include "DirectInput.h"
#include "DIJoystick.h"     /* For DIJoystick avalibility. */

#ifdef MAME_AVI
#include "video.h"
#include "Avi.h"

static struct MAME_AVI_STATUS AviStatus;
#endif /* MAME_AVI */

#ifdef KAILLERA
#include "kailleraclient.h"
#include "KailleraChat.h"
#include "ui_temp.h"
#include "fileio.h"
#include "zlib.h"
#include "ui.h"
#include "emuopts.h"

INLINE void options_set_wstring(core_options *opts, const char *name, const WCHAR *value, int priority)
{
	char *utf8_value = NULL;

	if (value)
		utf8_value = utf8_from_wstring(value);

	options_set_string(opts, name, utf8_value, priority);
}

#ifdef __INTEL_COMPILER
#define BILD_COMPILER "ICC"
#else
#ifdef _MSC_VER
#define BILD_COMPILER "VC"
#else
#define BILD_COMPILER "GCC"
#endif
#endif

int mame32_PlayGameCount = 0;
WCHAR kaillera_recode_filename[MAX_PATH];
WCHAR local_recode_filename[5];
//char Trace_filename[MAX_PATH];

static char kailleraGame_mameVer[512];	//kt
static WCHAR KailleraClientDLL_Name[MAX_PATH];	//kt
int kPlay = 0;
int RePlay = 0;

extern osd_ticks_t	KailleraMaxWait;
static void StartReplay(void);
static void MKInpDir(void);
#endif /* KAILLERA */

#ifndef LVS_EX_LABELTIP
#define LVS_EX_LABELTIP         0x00004000 // listview unfolds partly hidden labels if it does not have infotip text
#endif // LVS_EX_LABELTIP

#if defined(__GNUC__)
/* fix warning: value computed is not used for GCC4 */
#undef ListView_SetImageList
//#define ListView_SetImageList(w,h,i) (HIMAGELIST)(UINT)SNDMSG((w),LVM_SETIMAGELIST,(i),(LPARAM)(h))
static HIMAGELIST ListView_SetImageList(HWND w, HIMAGELIST h, int i)
{
	UINT result;

	result = SNDMSG(w, LVM_SETIMAGELIST, i, (LPARAM)h);
	return (HIMAGELIST)result;
}

/* fix warning: value computed is not used for GCC4 */
#undef TreeView_HitTest
//#define TreeView_HitTest(w,p) (HTREEITEM)SNDMSG((w),TVM_HITTEST,0,(LPARAM)(LPTV_HITTESTINFO)(p))
static HTREEITEM TreeView_HitTest(HWND w, LPTV_HITTESTINFO p)
{
	LRESULT result;

	result = SNDMSG(w, TVM_HITTEST, 0, (LPARAM)p);
	return (HTREEITEM)result;
}

// fix warning: cast does not match function type
#if defined(ListView_CreateDragImage)
#undef ListView_CreateDragImage
#endif
#endif /* defined(__GNUC__) */

#ifndef ListView_CreateDragImage
#define ListView_CreateDragImage(hwnd, i, lpptUpLeft) \
    (HIMAGELIST)(LRESULT)(int)SendMessage((hwnd), LVM_CREATEDRAGIMAGE, (WPARAM)(int)(i), (LPARAM)(LPPOINT)(lpptUpLeft))
#endif // ListView_CreateDragImage

#ifndef TreeView_EditLabel
#define TreeView_EditLabel(w, i) \
    SNDMSG(w,TVM_EDITLABEL,0,(LPARAM)(i))
#endif // TreeView_EditLabel

#ifndef HDF_SORTUP
#define HDF_SORTUP 0x400
#endif // HDF_SORTUP

#ifndef HDF_SORTDOWN
#define HDF_SORTDOWN 0x200
#endif // HDF_SORTDOWN

#ifndef LVM_SETBKIMAGEA
#define LVM_SETBKIMAGEA         (LVM_FIRST + 68)
#endif // LVM_SETBKIMAGEA

#ifndef LVM_SETBKIMAGEW
#define LVM_SETBKIMAGEW         (LVM_FIRST + 138)
#endif // LVM_SETBKIMAGEW

#ifndef LVM_GETBKIMAGEA
#define LVM_GETBKIMAGEA         (LVM_FIRST + 69)
#endif // LVM_GETBKIMAGEA

#ifndef LVM_GETBKIMAGEW
#define LVM_GETBKIMAGEW         (LVM_FIRST + 139)
#endif // LVM_GETBKIMAGEW

#ifndef LVBKIMAGE

typedef struct tagLVBKIMAGEA
{
	ULONG ulFlags;
	HBITMAP hbm;
	LPSTR pszImage;
	UINT cchImageMax;
	int xOffsetPercent;
	int yOffsetPercent;
} LVBKIMAGEA, *LPLVBKIMAGEA;

typedef struct tagLVBKIMAGEW
{
	ULONG ulFlags;
	HBITMAP hbm;
	LPWSTR pszImage;
	UINT cchImageMax;
	int xOffsetPercent;
	int yOffsetPercent;
} LVBKIMAGEW, *LPLVBKIMAGEW;

#ifdef UNICODE
#define LVBKIMAGE               LVBKIMAGEW
#define LPLVBKIMAGE             LPLVBKIMAGEW
#define LVM_SETBKIMAGE          LVM_SETBKIMAGEW
#define LVM_GETBKIMAGE          LVM_GETBKIMAGEW
#else
#define LVBKIMAGE               LVBKIMAGEA
#define LPLVBKIMAGE             LPLVBKIMAGEA
#define LVM_SETBKIMAGE          LVM_SETBKIMAGEA
#define LVM_GETBKIMAGE          LVM_GETBKIMAGEA
#endif
#endif

#ifndef LVBKIF_SOURCE_NONE
#define LVBKIF_SOURCE_NONE      0x00000000
#endif // LVBKIF_SOURCE_NONE

#ifndef LVBKIF_SOURCE_HBITMAP
#define LVBKIF_SOURCE_HBITMAP   0x00000001
#endif

#ifndef LVBKIF_SOURCE_URL
#define LVBKIF_SOURCE_URL       0x00000002
#endif // LVBKIF_SOURCE_URL

#ifndef LVBKIF_SOURCE_MASK
#define LVBKIF_SOURCE_MASK      0x00000003
#endif // LVBKIF_SOURCE_MASK

#ifndef LVBKIF_STYLE_NORMAL
#define LVBKIF_STYLE_NORMAL     0x00000000
#endif // LVBKIF_STYLE_NORMAL

#ifndef LVBKIF_STYLE_TILE
#define LVBKIF_STYLE_TILE       0x00000010
#endif // LVBKIF_STYLE_TILE

#ifndef LVBKIF_STYLE_MASK
#define LVBKIF_STYLE_MASK       0x00000010
#endif // LVBKIF_STYLE_MASK

#ifndef ListView_SetBkImageA
#define ListView_SetBkImageA(hwnd, plvbki) \
    (BOOL)SNDMSG((hwnd), LVM_SETBKIMAGEA, 0, (LPARAM)(plvbki))
#endif // ListView_SetBkImageA

#ifndef ListView_GetBkImageA
#define ListView_GetBkImageA(hwnd, plvbki) \
    (BOOL)SNDMSG((hwnd), LVM_GETBKIMAGEA, 0, (LPARAM)(plvbki))
#endif // ListView_GetBkImageA

#define MM_PLAY_GAME (WM_APP + 15000)

#define JOYGUI_MS 100

#define JOYGUI_TIMER 1
#define SCREENSHOT_TIMER 2
#if MULTISESSION
#define GAMEWND_TIMER 3
#endif

/* Max size of a sub-menu */
#define DBU_MIN_WIDTH  292
#define DBU_MIN_HEIGHT 190

int MIN_WIDTH  = DBU_MIN_WIDTH;
int MIN_HEIGHT = DBU_MIN_HEIGHT;

/* Max number of bkground picture files */
#define MAX_BGFILES 100

#ifdef USE_IPS
#define MAX_PATCHES 128
#define MAX_PATCHNAME 64
#endif /* USE_IPS */

#define NO_FOLDER -1
#define STATESAVE_VERSION 1

enum
{
	FILETYPE_INPUT_FILES = 0,
	FILETYPE_SAVESTATE_FILES,
	FILETYPE_WAVE_FILES,
	FILETYPE_MNG_FILES,
	FILETYPE_IMAGE_FILES,
	FILETYPE_GAMELIST_FILES,
#ifdef MAME_AVI
	FILETYPE_AVI_FILES,
#endif /* MAME_AVI */
	FILETYPE_MAX
};

typedef BOOL (WINAPI *common_file_dialog_procW)(LPOPENFILENAMEW lpofn);
typedef BOOL (WINAPI *common_file_dialog_procA)(LPOPENFILENAMEA lpofn);

#ifdef MAME_AVI
#include "AVI.h"
#include "WAV.h"
static void     MamePlayGameAVI(void);
static void     MamePlayBackGameAVI(void);
static WCHAR	last_directory_avi[MAX_PATH];
int				_nAviNo = 0;
#endif /* MAME_AVI */


/***************************************************************************
 externally defined global variables
 ***************************************************************************/

struct _driverw **driversw;

#if 0
extern const char g_szPlayGameString[] = "&Play %s";
extern const char g_szGameCountString[] = "%d games";
extern const char *history_filename;
extern const char *mameinfo_filename;
#endif


/***************************************************************************
    function prototypes
 ***************************************************************************/

static BOOL             Win32UI_init(HINSTANCE hInstance, LPSTR lpCmdLine, int nCmdShow);
static void             Win32UI_exit(void);

static BOOL             PumpMessage(void);
static BOOL             OnIdle(HWND hWnd);
static void             OnSize(HWND hwnd, UINT state, int width, int height);
static long WINAPI      MameWindowProc(HWND hwnd,UINT message,UINT wParam,LONG lParam);

static void             SetView(int menu_id);
static void             ResetListView(void);
static void             UpdateGameList(BOOL bUpdateRomAudit, BOOL bUpdateSampleAudit);
static void             DestroyIcons(void);
static void             ReloadIcons(void);
static void             PollGUIJoystick(void);
#if 0
static void             PressKey(HWND hwnd, UINT vk);
#endif
static BOOL             MameCommand(HWND hwnd,int id, HWND hwndCtl, UINT codeNotify);
static void             KeyboardKeyDown(int syskey, int vk_code, int special);
static void             KeyboardKeyUp(int syskey, int vk_code, int special);
static void             KeyboardStateClear(void);

static void             UpdateStatusBar(void);
static BOOL             PickerHitTest(HWND hWnd);
static BOOL             TreeViewNotify(NMHDR *nm);

static void             ResetBackground(const WCHAR *szFile);
static void             RandomSelectBackground(void);
static void             LoadBackgroundBitmap(void);
#ifndef USE_VIEW_PCBINFO
static void             PaintBackgroundImage(HWND hWnd, HRGN hRgn, int x, int y);
#endif /* USE_VIEW_PCBINFO */

static int CLIB_DECL    DriverDataCompareFunc(const void *arg1,const void *arg2);
static int              GamePicker_Compare(HWND hwndPicker, int index1, int index2, int sort_subitem);

static void             DisableSelection(void);
static void             EnableSelection(int nGame);

static int              GetSelectedPick(void);
static HICON            GetSelectedPickItemIcon(void);
static void             SetRandomPickItem(void);
static void             PickColor(COLORREF *cDefault);

static LPTREEFOLDER     GetSelectedFolder(void);
static HICON            GetSelectedFolderIcon(void);

static LRESULT CALLBACK HistoryWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK PictureFrameWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK PictureWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static void             ChangeLanguage(int id);
#ifdef IMAGE_MENU
static void             ChangeMenuStyle(int id);
#endif /* IMAGE_MENU */
static void             MamePlayRecordGame(void);
static void             MamePlayBackGame(const WCHAR *fname_playback);
static void             MamePlayRecordWave(void);
static void             MamePlayRecordMNG(void);
static void             MameLoadState(const WCHAR *fname_state);
static BOOL             CommonFileDialogW(BOOL open_for_write, WCHAR *filename, int filetype);
static BOOL             CommonFileDialogA(BOOL open_for_write, WCHAR *filename, int filetype);
static BOOL             CommonFileDialog(BOOL open_for_write, WCHAR *filename, int filetype);
static void             MamePlayGame(void);
static void             MamePlayGameWithOptions(int nGame);
static BOOL             GameCheck(void);
static BOOL             FolderCheck(void);

static void             ToggleScreenShot(void);
static void             AdjustMetrics(void);
static void             EnablePlayOptions(int nIndex, options_type *o);

/* Icon routines */
static DWORD            GetShellLargeIconSize(void);
static DWORD            GetShellSmallIconSize(void);
static void             CreateIcons(void);
static int              GetIconForDriver(int nItem);
static void             AddDriverIcon(int nItem,int default_icon_index);

// Context Menu handlers
static void             UpdateMenu(HMENU hMenu);
static void             InitTreeContextMenu(HMENU hTreeMenu);
static void             ToggleShowFolder(int folder);
static BOOL             HandleTreeContextMenu( HWND hWnd, WPARAM wParam, LPARAM lParam);
static BOOL             HandleScreenShotContextMenu( HWND hWnd, WPARAM wParam, LPARAM lParam);
static void             GamePicker_OnHeaderContextMenu(POINT pt, int nColumn);
static void             GamePicker_OnBodyContextMenu(POINT pt);

static void             InitListView(void);
/* Re/initialize the ListView header columns */
static void             ResetColumnDisplay(BOOL first_time);

static void             CopyToolTipTextW(LPTOOLTIPTEXTW lpttt);
static void             CopyToolTipTextA(LPTOOLTIPTEXTA lpttt);

static void             ProgressBarShow(void);
static void             ProgressBarHide(void);
static void             ResizeProgressBar(void);
static void             ProgressBarStep(void);
static void             ProgressBarStepParam(int iGameIndex, int nGameCount);

static HWND             InitProgressBar(HWND hParent);
static HWND             InitToolbar(HWND hParent);
static HWND             InitStatusBar(HWND hParent);

static LRESULT          Statusbar_MenuSelect(HWND hwnd, WPARAM wParam, LPARAM lParam);

static BOOL             NeedScreenShotImage(void);
static BOOL             NeedHistoryText(void);
static void             UpdateHistory(void);

static void             MameMessageBoxW(const WCHAR *fmt, ...);


void RemoveCurrentGameCustomFolder(void);
void RemoveGameCustomFolder(int driver_index);

void BeginListViewDrag(NM_LISTVIEW *pnmv);
void MouseMoveListViewDrag(POINTS pt);
void ButtonUpListViewDrag(POINTS p);

void CalculateBestScreenShotRect(HWND hWnd, RECT *pRect, BOOL restrict_height);

BOOL MouseHasBeenMoved(void);
void SwitchFullScreenMode(void);

#ifdef KAILLERA
static void KailleraTraceRecordGame(void);
INT_PTR CALLBACK KailleraOptionDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
int WINAPI kGameCallback(char *game, int player, int numplayers);
void WINAPI kChatCallback(char *nick, char *text);
void WINAPI kDropCallback(char *nick, int playernb);
void WINAPI kInfosCallback(char *gamename);
#endif /* KAILLERA */

#ifdef KSERVER
static void MameMessageBoxI(const WCHAR *fmt, ...);
INT_PTR CALLBACK KServerOptionDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
#endif /* KSERVER */
#ifdef EXPORT_GAME_LIST
INT_PTR CALLBACK ExportOptionDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
#endif /*EXPORT_GAME_LIST */

#ifdef USE_SHOW_SPLASH_SCREEN
static LRESULT CALLBACK BackMainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void  CreateBackgroundMain(HINSTANCE hInstance);
static void  DestroyBackgroundMain(void);
#endif /* USE_SHOW_SPLASH_SCREEN */

// Game Window Communication Functions
#if MULTISESSION
BOOL SendMessageToEmulationWindow(UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL SendIconToEmulationWindow(int nGameIndex);
HWND GetGameWindow(void);
#endif
#if !(MULTISESSION) || defined(KAILLERA)
void SendMessageToProcess(LPPROCESS_INFORMATION lpProcessInformation,
                                             UINT Msg, WPARAM wParam, LPARAM lParam);
void SendIconToProcess(LPPROCESS_INFORMATION lpProcessInformation, int nGameIndex);
#ifdef KAILLERA
HWND GetGameWindowA(LPPROCESS_INFORMATION lpProcessInformation);
#else
HWND GetGameWindow(LPPROCESS_INFORMATION lpProcessInformation);
#endif /* KAILLERA */
#endif

static BOOL CALLBACK EnumWindowCallBack(HWND hwnd, LPARAM lParam);

static const WCHAR *GetLastDir(void);

/***************************************************************************
    External variables
 ***************************************************************************/
#ifdef KSERVER
static HWND m_hPro = NULL;

const char *CheckIP(void);
char pszAddr[16];
#endif /* KSERVER */

#ifdef EXPORT_GAME_LIST
int nExportContent=1;
static void ExportTranslationToFile(char *szFile);
static void SaveGameListToFile(char *szFile, int Formatted);
static void SaveRomsListToFile(char *szFile);
#endif /* EXPORT_GAME_LIST */

/***************************************************************************
    Internal structures
 ***************************************************************************/

/*
 * These next two structs represent how the icon information
 * is stored in an ICO file.
 */
typedef struct
{
	BYTE    bWidth;               /* Width of the image */
	BYTE    bHeight;              /* Height of the image (times 2) */
	BYTE    bColorCount;          /* Number of colors in image (0 if >=8bpp) */
	BYTE    bReserved;            /* Reserved */
	WORD    wPlanes;              /* Color Planes */
	WORD    wBitCount;            /* Bits per pixel */
	DWORD   dwBytesInRes;         /* how many bytes in this resource? */
	DWORD   dwImageOffset;        /* where in the file is this image */
} ICONDIRENTRY, *LPICONDIRENTRY;

typedef struct
{
	UINT            Width, Height, Colors; /* Width, Height and bpp */
	LPBYTE          lpBits;                /* ptr to DIB bits */
	DWORD           dwNumBytes;            /* how many bytes? */
	LPBITMAPINFO    lpbi;                  /* ptr to header */
	LPBYTE          lpXOR;                 /* ptr to XOR image bits */
	LPBYTE          lpAND;                 /* ptr to AND image bits */
} ICONIMAGE, *LPICONIMAGE;

/* Which edges of a control are anchored to the corresponding side of the parent window */
#define RA_LEFT     0x01
#define RA_RIGHT    0x02
#define RA_TOP      0x04
#define RA_BOTTOM   0x08
#define RA_ALL      0x0F

#define RA_END  0
#define RA_ID   1
#define RA_HWND 2

typedef struct
{
	int         type;       /* Either RA_ID or RA_HWND, to indicate which member of u is used; or RA_END
				   to signify last entry */
	union                   /* Can identify a child window by control id or by handle */
	{
		int     id;         /* Window control id */
		HWND    hwnd;       /* Window handle */
	} u;
	BOOL		setfont;	/* Do we set this item's font? */
	int         action;     /* What to do when control is resized */
	void        *subwindow; /* Points to a Resize structure for this subwindow; NULL if none */
} ResizeItem;

typedef struct
{
	RECT        rect;       /* Client rect of window; must be initialized before first resize */
	ResizeItem* items;      /* Array of subitems to be resized */
} Resize;

static void ResizeWindow(HWND hParent, Resize *r);
static void SetAllWindowsFont(HWND hParent, const Resize *r, HFONT hFont, BOOL bRedraw);

/* List view Icon defines */
#define LG_ICONMAP_WIDTH    GetSystemMetrics(SM_CXICON)
#define LG_ICONMAP_HEIGHT   GetSystemMetrics(SM_CYICON)
#define ICONMAP_WIDTH       GetSystemMetrics(SM_CXSMICON)
#define ICONMAP_HEIGHT      GetSystemMetrics(SM_CYSMICON)

/*
typedef struct tagPOPUPSTRING
{
	HMENU hMenu;
	UINT uiString;
} POPUPSTRING;

#define MAX_MENUS 3
*/

#define SPLITTER_WIDTH	4
#define MIN_VIEW_WIDTH	10

// Struct needed for Game Window Communication

typedef struct
{
	LPPROCESS_INFORMATION ProcessInfo;
	HWND hwndFound;
} FINDWINDOWHANDLE;

#ifdef IMAGE_MENU
static struct
{
	UINT itemID;
	UINT iconID;
} menu_icon_table[] =
{
#ifdef MAME32PLUSPLUS
	{ ID_VIEW_GROUPED,			IDI_GROUP },
	{ ID_VIEW_DETAIL,			IDI_DETAILS },
	{ ID_VIEW_LIST_MENU,		IDI_LIST },
	{ ID_VIEW_SMALL_ICON,		IDI_SMALL },
	{ ID_VIEW_LARGE_ICON,		IDI_LARGE },
	{ ID_VIEW_PCBINFO,			IDI_PCB },
	{ ID_FILE_PLAY,				IDI_CHECKMARK },
	{ ID_FILE_PLAY_RECORD,		IDI_H_RECORD },
	{ ID_FILE_PLAY_BACK,		IDI_H_PLAYBACK },
	{ ID_FILE_PLAY_RECORD_MNG,	IDI_H_MNG_RECORD },
	{ ID_FILE_PLAY_RECORD_WAVE,	IDI_H_WAVE_RECORD },
	{ ID_FILE_PLAY_WITH_AVI,	IDI_H_AVI_RECORD },
	{ ID_FILE_PLAY_BACK_AVI,	IDI_H_PLAY_BACK_AVI },
	{ ID_FILE_NETPLAY,			IDI_H_NETPLAY },
	{ ID_FILE_LOADSTATE,		IDI_H_LOAD },
	{ ID_CONTEXT_SELECT_RANDOM,	IDI_H_SELECT_RANDOM },
	{ ID_CONTEXT_ADD_CUSTOM,	IDI_H_ADD_CUSTOM },
	{ ID_CONTEXT_REMOVE_CUSTOM,	IDI_H_REMOVE_CUSTOM },
	{ ID_FOLDER_SOURCEPROPERTIES,	IDI_H_DRIVER_SETTING },
	{ ID_GAME_PROPERTIES,		IDI_H_PROPERTIES },
	{ ID_FILE_AUDIT,			IDI_H_AUDIT },
	{ ID_GAME_AUDIT,			IDI_H_AUDIT },
	{ ID_FILE_EXIT,				IDI_H_EXIT },
	{ ID_VIEW_FULLSCREEN,		IDI_H_FULLSCREEN },
	{ ID_UPDATE_GAMELIST,		IDI_H_REFRESH },
	{ ID_CONTEXT_FILTERS,		IDI_H_FILTERS },
	{ ID_CUSTOMIZE_FIELDS,		IDI_H_FIELDS },
	{ ID_OPTIONS_FONT,			IDI_H_FONT },
	{ ID_OPTIONS_CLONE_COLOR,	IDI_H_CLONE_COLOR },
	{ ID_OPTIONS_DIR,			IDI_H_DIR },
	{ ID_OPTIONS_DEFAULTS,		IDI_H_DEFAULTS },
	{ ID_OPTIONS_INTERFACE,		IDI_H_INTERFACE },
	{ ID_OPTIONS_BG,			IDI_H_BACKGROUND },
	{ ID_OPTIONS_PALETTE,		IDI_H_PALETTE },
	{ ID_OPTIONS_RESET_DEFAULTS,	IDI_H_RESET_DEFAULTS },
	{ ID_OPTIONS_KAILLERA_OPTIONS,	IDI_H_KAILLERA_OPTIONS },
	{ ID_OPTIONS_MMO2LST,		IDI_H_LIST },
	{ ID_HELP_CONTENTS,			IDI_H_HELP },
	{ ID_HELP_TROUBLE,			IDI_H_TROUBLE },
	{ ID_HELP_ABOUT,			IDI_H_ABOUT },
#else
	{ ID_HELP_ABOUT,			IDI_MAME32_ICON },
	{ ID_FILE_AUDIT,			IDI_CHECKMARK },
	{ ID_GAME_AUDIT,			IDI_CHECKMARK },
	{ ID_FILE_PLAY,				IDI_WIN_ROMS },
	{ ID_FILE_EXIT,				IDI_WIN_REDX },
	{ ID_FILE_PLAY_RECORD_MNG,	IDI_VIDEO },
	{ ID_FILE_PLAY_RECORD_WAVE,	IDI_SOUND },
	{ ID_FILE_PLAY_RECORD,		IDI_JOYSTICK },
	{ ID_OPTIONS_DIR,			IDI_FOLDER },
	{ ID_VIEW_GROUPED,			IDI_GROUP },
	{ ID_VIEW_DETAIL,			IDI_DETAILS },
	{ ID_VIEW_LIST_MENU,		IDI_LIST },
	{ ID_VIEW_SMALL_ICON,		IDI_SMALL },
	{ ID_VIEW_LARGE_ICON,		IDI_LARGE },
	{ ID_GAME_PROPERTIES,		IDI_PROPERTY },
	{ ID_VIEW_PCBINFO,			IDI_PCB },
#endif /* MAME32PLUSPLUS */
	{ 0 }
};
#endif /* IMAGE_MENU */

/***************************************************************************
    Internal variables
 ***************************************************************************/

static HWND   hMain  = NULL;
static HACCEL hAccel = NULL;

static HWND hwndList  = NULL;
static HWND hTreeView = NULL;
static HWND hProgWnd  = NULL;
static HWND hTabCtrl  = NULL;

static HINSTANCE hInst = NULL;

static HFONT hFont = NULL;     /* Font for list view */

static int game_count = 0;

/* global data--know where to send messages */
static BOOL in_emulation;

/* idle work at startup */
static BOOL idle_work;

static int  game_index;
static int  progBarStep;

static BOOL bDoGameCheck = FALSE;

/* Tree control variables */
static BOOL bShowTree      = 1;
static BOOL bShowToolBar   = 1;
static BOOL bShowStatusBar = 1;
static BOOL bShowTabCtrl   = 1;
static BOOL bProgressShown = FALSE;
static BOOL bListReady     = FALSE;

#ifdef KAILLERA
static BOOL bUseFavorite  = FALSE;
static BOOL bUseImeInChat = FALSE;
BOOL bKailleraMAME32WindowHide = FALSE;
BOOL bMAME32WindowShow = TRUE;
static BOOL bKailleraMAME32WindowOwner = TRUE;
BOOL bKailleraNetPlay	= FALSE;

static kailleraInfos kInfos =
{
    (char *)"",
    (char *)"",
    kGameCallback,
    kChatCallback,
    kDropCallback,
    kInfosCallback
};
#endif /* KAILLERA */

/* use a joystick subsystem in the gui? */
static struct OSDJoystick* g_pJoyGUI = NULL;

/* store current keyboard state (in internal codes) here */
static input_code *keyboard_state;
static int keyboard_state_count;

/* search */
static WCHAR g_SearchText[256];

/* table copied from windows/inputs.c */
// table entry indices
#define MAME_KEY		0
#define DI_KEY			1
#define VIRTUAL_KEY		2
#define ASCII_KEY		3

typedef struct
{
	char		name[40];	    // functionality name (optional)
	input_seq	is;				// the input sequence (the keys pressed)
	UINT		func_id;        // the identifier
	input_seq* (*getiniptr)(void);// pointer to function to get the value from .ini file
} GUISequence;

static GUISequence GUISequenceControl[]=
{
	{"gui_key_up",                SEQ_DEF_0,    ID_UI_UP,           Get_ui_key_up },
	{"gui_key_down",              SEQ_DEF_0,    ID_UI_DOWN,         Get_ui_key_down },
	{"gui_key_left",              SEQ_DEF_0,    ID_UI_LEFT,         Get_ui_key_left },
	{"gui_key_right",             SEQ_DEF_0,    ID_UI_RIGHT,        Get_ui_key_right },
	{"gui_key_start",             SEQ_DEF_0,    ID_UI_START,        Get_ui_key_start },
	{"gui_key_pgup",              SEQ_DEF_0,    ID_UI_PGUP,         Get_ui_key_pgup },
	{"gui_key_pgdwn",             SEQ_DEF_0,    ID_UI_PGDOWN,       Get_ui_key_pgdwn },
	{"gui_key_home",              SEQ_DEF_0,    ID_UI_HOME,         Get_ui_key_home },
	{"gui_key_end",               SEQ_DEF_0,    ID_UI_END,          Get_ui_key_end },
	{"gui_key_ss_change",         SEQ_DEF_0,    IDC_SSFRAME,        Get_ui_key_ss_change },
	{"gui_key_history_up",        SEQ_DEF_0,    ID_UI_HISTORY_UP,   Get_ui_key_history_up },
	{"gui_key_history_down",      SEQ_DEF_0,    ID_UI_HISTORY_DOWN, Get_ui_key_history_down },
	
	{"gui_key_context_filters",    SEQ_DEF_0,    ID_CONTEXT_FILTERS,       Get_ui_key_context_filters },
	{"gui_key_select_random",      SEQ_DEF_0,    ID_CONTEXT_SELECT_RANDOM, Get_ui_key_select_random },
	{"gui_key_game_audit",         SEQ_DEF_0,    ID_GAME_AUDIT,            Get_ui_key_game_audit },
	{"gui_key_game_properties",    SEQ_DEF_0,    ID_GAME_PROPERTIES,       Get_ui_key_game_properties },
	{"gui_key_help_contents",      SEQ_DEF_0,    ID_HELP_CONTENTS,         Get_ui_key_help_contents },
	{"gui_key_update_gamelist",    SEQ_DEF_0,    ID_UPDATE_GAMELIST,       Get_ui_key_update_gamelist },
	{"gui_key_view_folders",       SEQ_DEF_0,    ID_VIEW_FOLDERS,          Get_ui_key_view_folders },
	{"gui_key_view_fullscreen",    SEQ_DEF_0,    ID_VIEW_FULLSCREEN,       Get_ui_key_view_fullscreen },
	{"gui_key_view_pagetab",       SEQ_DEF_0,    ID_VIEW_PAGETAB,          Get_ui_key_view_pagetab },
	{"gui_key_view_picture_area",  SEQ_DEF_0,    ID_VIEW_PICTURE_AREA,     Get_ui_key_view_picture_area },
	{"gui_key_view_status",        SEQ_DEF_0,    ID_VIEW_STATUS,           Get_ui_key_view_status },
	{"gui_key_view_toolbars",      SEQ_DEF_0,    ID_VIEW_TOOLBARS,         Get_ui_key_view_toolbars },

	{"gui_key_view_tab_cabinet",     SEQ_DEF_0,  ID_VIEW_TAB_CABINET,       Get_ui_key_view_tab_cabinet },
	{"gui_key_view_tab_cpanel",      SEQ_DEF_0,  ID_VIEW_TAB_CONTROL_PANEL, Get_ui_key_view_tab_cpanel },
	{"gui_key_view_tab_flyer",       SEQ_DEF_0,  ID_VIEW_TAB_FLYER,         Get_ui_key_view_tab_flyer },
	{"gui_key_view_tab_history",     SEQ_DEF_0,  ID_VIEW_TAB_HISTORY,       Get_ui_key_view_tab_history },
#ifdef STORY_DATAFILE
	{"gui_key_view_tab_story",       SEQ_DEF_0,  ID_VIEW_TAB_STORY,         Get_ui_key_view_tab_story },
#endif /* STORY_DATAFILE */
	{"gui_key_view_tab_marquee",     SEQ_DEF_0,  ID_VIEW_TAB_MARQUEE,       Get_ui_key_view_tab_marquee },
	{"gui_key_view_tab_screenshot",  SEQ_DEF_0,  ID_VIEW_TAB_SCREENSHOT,    Get_ui_key_view_tab_screenshot },
	{"gui_key_view_tab_title",       SEQ_DEF_0,  ID_VIEW_TAB_TITLE,         Get_ui_key_view_tab_title },
	{"gui_key_quit",                 SEQ_DEF_0,  ID_FILE_EXIT,              Get_ui_key_quit },
};


#define NUM_GUI_SEQUENCES ARRAY_LENGTH(GUISequenceControl)


static UINT    lastColumnClick   = 0;
static WNDPROC g_lpHistoryWndProc = NULL;
static WNDPROC g_lpPictureFrameWndProc = NULL;
static WNDPROC g_lpPictureWndProc = NULL;

/*
static POPUPSTRING popstr[MAX_MENUS + 1];
*/

/* Tool and Status bar variables */
static HWND hStatusBar = 0;
static HWND hToolBar   = 0;

/* Column Order as Displayed */
static BOOL oldControl = FALSE;
static BOOL xpControl = FALSE;

/* Used to recalculate the main window layout */
static int  bottomMargin;
static int  topMargin;
static int  have_history = FALSE;
static RECT history_rect;

static BOOL have_selection = FALSE;

static HBITMAP hMissing_bitmap;

/* Icon variables */
static HIMAGELIST   hLarge = NULL;
static HIMAGELIST   hSmall = NULL;
static HIMAGELIST   hHeaderImages = NULL;
static int          *icon_index = NULL; /* for custom per-game icons */

static TBBUTTON tbb[] =
{
	{0, ID_VIEW_FOLDERS,     TBSTATE_ENABLED, TBSTYLE_CHECK,      {0, 0}, 0, 0},
	{1, ID_VIEW_PICTURE_AREA,TBSTATE_ENABLED, TBSTYLE_CHECK,      {0, 0}, 0, 1},
	{0, 0,                   TBSTATE_ENABLED, TBSTYLE_SEP,        {0, 0}, 0, 0},
	{2, ID_VIEW_LARGE_ICON,  TBSTATE_ENABLED, TBSTYLE_CHECKGROUP, {0, 0}, 0, 2},
	{3, ID_VIEW_SMALL_ICON,  TBSTATE_ENABLED, TBSTYLE_CHECKGROUP, {0, 0}, 0, 3},
	{4, ID_VIEW_LIST_MENU,   TBSTATE_ENABLED, TBSTYLE_CHECKGROUP, {0, 0}, 0, 4},
	{5, ID_VIEW_DETAIL,      TBSTATE_ENABLED, TBSTYLE_CHECKGROUP, {0, 0}, 0, 5},
	{6, ID_VIEW_GROUPED,     TBSTATE_ENABLED, TBSTYLE_CHECKGROUP, {0, 0}, 0, 6},
	{0, 0,                   TBSTATE_ENABLED, TBSTYLE_SEP,        {0, 0}, 0, 0},
	{9, IDC_USE_LIST,        TBSTATE_ENABLED, TBSTYLE_CHECK,      {0, 0}, 0, 9},
	{0, 0,                   TBSTATE_ENABLED, TBSTYLE_SEP,        {0, 0}, 0, 0},
#if defined(KAILLERA) || defined(MAME32PLUSPLUS)
	{13, ID_UPDATE_GAMELIST, TBSTATE_ENABLED, TBSTYLE_BUTTON,     {0, 0}, 0, 10},
	{0,  0,                  TBSTATE_ENABLED, TBSTYLE_SEP,        {0, 0}, 0, 0},
	{14, ID_OPTIONS_INTERFACE,TBSTATE_ENABLED, TBSTYLE_BUTTON,   {0, 0}, 0, 11},
	{15, ID_OPTIONS_DEFAULTS, TBSTATE_ENABLED, TBSTYLE_BUTTON,   {0, 0}, 0, 12},
	{0,  0,                   TBSTATE_ENABLED, TBSTYLE_SEP,        {0, 0}, 0, 0},
	{10, ID_MAME_HOMEPAGE,    TBSTATE_ENABLED, TBSTYLE_BUTTON,   {0, 0}, 0, 15},
	{0,  0,                   TBSTATE_ENABLED, TBSTYLE_SEP,        {0, 0}, 0, 0},
#ifdef KAILLERA
	{17, IDC_USE_NETPLAY_FOLDER,     TBSTATE_ENABLED, TBSTYLE_CHECK,      {0, 0}, 0, 13},
	{18, IDC_USE_IME_IN_CHAT,        TBSTATE_ENABLED, TBSTYLE_CHECK,      {0, 0}, 0, 14},
	{0, 0,                   TBSTATE_ENABLED, TBSTYLE_SEP,        {0, 0}, 0, 0},
#endif /* KAILLERA */
#endif
	{7, ID_HELP_ABOUT,       TBSTATE_ENABLED, TBSTYLE_BUTTON,     {0, 0}, 0, 7},
	{8, ID_HELP_CONTENTS,    TBSTATE_ENABLED, TBSTYLE_BUTTON,     {0, 0}, 0, 8}
};

#define NUM_TOOLBUTTONS ARRAY_LENGTH(tbb)

#if defined(KAILLERA) || defined(MAME32PLUSPLUS)
#define NUM_TOOLTIPS 15 + 1
#else
#define NUM_TOOLTIPS 8 + 1
#endif /* KAILLERA */

static WCHAR szTbStrings[NUM_TOOLTIPS + 1][40] =
{
	TEXT("Toggle Folder List"),
	TEXT("Toggle Screen Shot"),
	TEXT("Large Icons"),
	TEXT("Small Icons"),
	TEXT("List"),
	TEXT("Details"),
	TEXT("Grouped"),
	TEXT("About"),
	TEXT("Help"),
	TEXT("Use Local Language Game List")
#if defined(KAILLERA) || defined(MAME32PLUSPLUS)
	,TEXT("Refresh"),
	TEXT("Interface Options"),
	TEXT("Default Game Options"),
    TEXT("Play network game with favorite"),
    TEXT("Use IME in chat"),
    TEXT("MAME Homepage"),
#endif /* KAILLERA */
};

static int CommandToString[] =
{
	ID_VIEW_FOLDERS,
	ID_VIEW_PICTURE_AREA,
	ID_VIEW_LARGE_ICON,
	ID_VIEW_SMALL_ICON,
	ID_VIEW_LIST_MENU,
	ID_VIEW_DETAIL,
	ID_VIEW_GROUPED,
	ID_HELP_ABOUT,
	ID_HELP_CONTENTS,
	IDC_USE_LIST,
#if defined(KAILLERA) || defined(MAME32PLUSPLUS)
	ID_UPDATE_GAMELIST,
	ID_OPTIONS_INTERFACE,
	ID_OPTIONS_DEFAULTS,
	IDC_USE_NETPLAY_FOLDER,
	IDC_USE_IME_IN_CHAT,
	ID_MAME_HOMEPAGE,
#endif /* KAILLERA */
	-1
};

/* How to resize main window */
static ResizeItem main_resize_items[] =
{
	{ RA_HWND, { 0 },            FALSE, RA_LEFT  | RA_RIGHT  | RA_TOP,     NULL },
	{ RA_HWND, { 0 },            FALSE, RA_LEFT  | RA_RIGHT  | RA_BOTTOM,  NULL },
	{ RA_ID,   { IDC_DIVIDER },  FALSE, RA_LEFT  | RA_RIGHT  | RA_TOP,     NULL },
	{ RA_ID,   { IDC_TREE },     TRUE,	RA_LEFT  | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_LIST },     TRUE,	RA_ALL,                            NULL },
	{ RA_ID,   { IDC_SPLITTER }, FALSE,	RA_LEFT  | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_SPLITTER2 },FALSE,	RA_RIGHT | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_SSFRAME },  FALSE,	RA_RIGHT | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_SSPICTURE },FALSE,	RA_RIGHT | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_HISTORY },  TRUE,	RA_RIGHT | RA_BOTTOM | RA_TOP,     NULL },
	{ RA_ID,   { IDC_SSTAB },    FALSE,	RA_RIGHT | RA_TOP,                 NULL },
	{ RA_END,  { 0 },            FALSE, 0,                                 NULL }
};

static Resize main_resize = { {0, 0, 0, 0}, main_resize_items };

/* our dialog/configured options */
static options_type playing_game_options;

/* last directory for common file dialogs */
static WCHAR last_directory[MAX_PATH];

/* system-wide window message sent out with an ATOM of the current game name
   each time it changes */
static UINT g_mame32_message = 0;
static BOOL g_bDoBroadcast   = FALSE;

static BOOL use_gui_romloading = FALSE;

static BOOL g_listview_dragging = FALSE;
HIMAGELIST himl_drag;
int game_dragged; /* which game started the drag */
HTREEITEM prev_drag_drop_target; /* which tree view item we're currently highlighting */

static BOOL g_in_treeview_edit = FALSE;

#ifdef USE_IPS
static WCHAR * g_IPSMenuSelectName;
#endif /* USE_IPS */

typedef struct
{
	const char *name;
	int index;
} driver_data_type;
static driver_data_type *sorted_drivers;

static WCHAR * g_pRecordName = NULL;
static WCHAR * g_pPlayBkName = NULL;
#ifdef KAILLERA
static WCHAR * g_pRecordSubName = NULL;
static WCHAR * g_pPlayBkSubName = NULL;
static WCHAR * g_pAutoRecordName = NULL;
#endif /* KAILLERA */
static WCHAR * g_pSaveStateName = NULL;
static WCHAR * g_pRecordWaveName = NULL;
static WCHAR * g_pRecordMNGName = NULL;
#ifdef MAME_AVI
static WCHAR * g_pRecordAVIName = NULL;
#endif /* MAME_AVI */
static WCHAR * override_playback_directory = NULL;
static WCHAR * override_savestate_directory = NULL;

static struct
{
	const WCHAR *filter;
	const WCHAR *title_load;
	const WCHAR *title_save;
	const WCHAR *(*dir)(void);
	const WCHAR *ext;
} cfg_data[FILETYPE_MAX] =
{
	{
#ifdef KAILLERA
		TEXT(MAMENAME) TEXT(" input files (*.inp,*.trc,*.zip)\0*.inp;*.trc;*.zip\0All files (*.*)\0*.*\0"),
#else
		TEXT(MAMENAME) TEXT(" input files (*.inp,*.zip)\0*.inp;*.zip\0All files (*.*)\0*.*\0"),
#endif /* KAILLERA */
		TEXT("Select a recorded file"),
		TEXT("Select a file to record"),
		GetInpDir,
		TEXT("inp")
	},
	{
		TEXT(MAMENAME) TEXT(" savestate files (*.sta)\0*.sta;\0All files (*.*)\0*.*\0"),
		TEXT("Select a savestate file"),
		NULL,
		GetStateDir,
		TEXT("sta")
	},
	{
		TEXT("Sounds (*.wav)\0*.wav;\0All files (*.*)\0*.*\0"),
		NULL,
		TEXT("Select a sound file to record"),
		GetLastDir,
		TEXT("wav")
	},
	{
		TEXT("Videos (*.mng)\0*.mng;\0All files (*.*)\0*.*\0"),
		NULL,
		TEXT("Select a mng file to record"),
		GetLastDir,
		TEXT("mng")
	},
	{
		TEXT("Image Files (*.png,*.bmp)\0*.png;*.bmp\0"),
		TEXT("Select a Background Image"),
		NULL,
		GetBgDir,
		TEXT("png")
	},
	{
		TEXT("Game List Files (*.lst)\0*.lst\0"),
		TEXT("Select a folder to save game list"),
		NULL,
		GetTranslationDir,
		TEXT("lst")
	},
#ifdef MAME_AVI
	{
		TEXT("AVI files (*.avi)\0*.avi\0All files (*.*)\0*.*\0"),
		TEXT("Select AVI file to record"),
		NULL,
		GetAviDir,
		TEXT("avi")
	},
#endif /* MAME_AVI */
};


/***************************************************************************
    Global variables
 ***************************************************************************/

/* Background Image handles also accessed from TreeView.c */
static HPALETTE         hPALbg   = 0;
static HBITMAP          hBackground  = 0;
static MYBITMAPINFO     bmDesc;

#ifdef USE_SHOW_SPLASH_SCREEN
static HWND             hBackMain = NULL;
static HBITMAP          hSplashBmp = 0;
static HDC              hMemoryDC;
#endif /* USE_SHOW_SPLASH_SCREEN */

/* List view Column text */
const WCHAR* column_names[COLUMN_MAX] =
{
	TEXT("Description"),
	TEXT("ROMs"),
	TEXT("Samples"),
	TEXT("Name"),
	TEXT("Type"),
	TEXT("Trackball"),
	TEXT("Played"),
	TEXT("Manufacturer"),
	TEXT("Year"),
	TEXT("Clone Of"),
	TEXT("Driver"),
	TEXT("Play Time")
};


/***************************************************************************
    Message Macros
 ***************************************************************************/

#ifndef StatusBar_GetItemRect
#define StatusBar_GetItemRect(hWnd, iPart, lpRect) \
    SendMessage(hWnd, SB_GETRECT, (WPARAM) iPart, (LPARAM) (LPRECT) lpRect)
#endif

#ifndef ToolBar_CheckButton
#define ToolBar_CheckButton(hWnd, idButton, fCheck) \
    SendMessage(hWnd, TB_CHECKBUTTON, (WPARAM)idButton, (LPARAM)MAKELONG(fCheck, 0))
#endif

/***************************************************************************
    External functions
 ***************************************************************************/

static BOOL WaitWithMessageLoop(HANDLE hEvent)
{
	DWORD dwRet;
	MSG   msg;

	while (1)
	{
		dwRet = MsgWaitForMultipleObjects(1, &hEvent, FALSE, INFINITE, QS_ALLINPUT);

		if (dwRet == WAIT_OBJECT_0)
			return TRUE;

		if (dwRet != WAIT_OBJECT_0 + 1)
			break;

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0)
				return TRUE;
		}
	}
	return FALSE;
}

static void override_options(void)
{
	if (override_playback_directory)
		set_core_input_directory(override_playback_directory);
	if (override_savestate_directory)
		set_core_state_directory(override_savestate_directory);

	if (g_pSaveStateName)
		set_core_state(g_pSaveStateName);
	if (g_pPlayBkName)
		set_core_playback(g_pPlayBkName);
	if (g_pRecordName)
		set_core_record(g_pRecordName);
	if (g_pRecordMNGName)
		set_core_mngwrite(g_pRecordMNGName);
	if (g_pRecordWaveName)
		set_core_wavwrite(g_pRecordWaveName);
#ifdef MAME_AVI
	if (g_pRecordAVIName)
	{
		options_set_wstring(get_core_options(), "avi_avi_filename", g_pRecordAVIName, OPTION_PRIORITY_INI);

		options_set_float (get_core_options(), "avi_def_fps", AviStatus.def_fps, OPTION_PRIORITY_INI);
		options_set_float (get_core_options(), "avi_fps", AviStatus.fps, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_frame_skip", AviStatus.frame_skip, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "avi_frame_cmp", AviStatus.frame_cmp, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "avi_frame_cmp_pre15", AviStatus.frame_cmp_pre15, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "avi_frame_cmp_few", AviStatus.frame_cmp_few, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_width", AviStatus.width, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_height", AviStatus.height, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_depth", AviStatus.depth, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_orientation", AviStatus.orientation, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_rect_top", AviStatus.rect.m_Top, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_rect_left", AviStatus.rect.m_Left, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_rect_width", AviStatus.rect.m_Width, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_rect_height", AviStatus.rect.m_Height, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "avi_interlace", AviStatus.interlace, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "avi_interlace_odd_field", AviStatus.interlace_odd_number_field, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_filesize", AviStatus.avi_filesize, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "avi_avi_savefile_pause", AviStatus.avi_savefile_pause, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_width", AviStatus.avi_width, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_height", AviStatus.avi_height, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_depth", AviStatus.avi_depth, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_rect_top", AviStatus.avi_rect.m_Top, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_rect_left", AviStatus.avi_rect.m_Left, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_rect_width", AviStatus.avi_rect.m_Width, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_rect_height", AviStatus.avi_rect.m_Height, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "avi_avi_smooth_resize_x", AviStatus.avi_smooth_resize_x, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "avi_avi_smooth_resize_y", AviStatus.avi_smooth_resize_y, OPTION_PRIORITY_INI);

		if (AviStatus.wav_filename && strlen(AviStatus.wav_filename))
			options_set_string(get_core_options(), "avi_wav_filename", AviStatus.wav_filename, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_audio_type", AviStatus.audio_type, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_audio_channel", AviStatus.audio_channel, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_audio_samples_per_sec", AviStatus.audio_samples_per_sec, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_audio_bitrate", AviStatus.audio_bitrate, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_audio_record_type", AviStatus.avi_audio_record_type, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_audio_channel", AviStatus.avi_audio_channel, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_audio_samples_per_sec", AviStatus.avi_audio_samples_per_sec, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_avi_audio_bitrate", AviStatus.avi_audio_bitrate, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "avi_audio_cmp", AviStatus.avi_audio_cmp, OPTION_PRIORITY_INI);
		
		options_set_int   (get_core_options(), "avi_hour", AviStatus.hour, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_minute", AviStatus.minute, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "avi_second", AviStatus.second, OPTION_PRIORITY_INI);
	}
#endif /* MAME_AVI */
#ifdef KAILLERA
	if (g_pPlayBkSubName)
		options_set_wstring(get_core_options(), "pbsub", g_pPlayBkSubName, OPTION_PRIORITY_INI);
	if (g_pRecordSubName)
		options_set_wstring(get_core_options(), "recsub", g_pRecordSubName, OPTION_PRIORITY_INI);
	if (g_pRecordName != NULL && g_pAutoRecordName != NULL)
		options_set_wstring(get_core_options(), "at_rec_name", g_pAutoRecordName, OPTION_PRIORITY_INI);

	// kaillera force options
	if (kPlay)
	{
		options_set_bool  (get_core_options(), "autoframeskip", TRUE, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "throttle", TRUE, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "waitvsync", 0, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "syncrefresh", 0, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "sound", TRUE, OPTION_PRIORITY_INI);
		options_set_int   (get_core_options(), "samplerate", 48000, OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "samples", TRUE, OPTION_PRIORITY_INI);
#if 0
		if (!mame_stricmp(GetDriverFilename(nGameIndex), "neogeo.c") && !mame_stricmp(pOpts->bios, "uni-bios.22"))
			options_set_string(get_core_options(), "bios", pOpts->bios, OPTION_PRIORITY_INI);
		else
#endif
			options_set_string(get_core_options(), "bios", "0", OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "cheat", 0, OPTION_PRIORITY_INI);
		options_set_string(get_core_options(), "m68k_core", "c", OPTION_PRIORITY_INI);
		options_set_bool  (get_core_options(), "multithreading", 0, OPTION_PRIORITY_INI);
		options_set_float (get_core_options(), "speed", 1.0, OPTION_PRIORITY_INI);
	}
#endif /* KAILLERA */
}

static int RunMAME(int nGameIndex)
{
	time_t start, end;
	double elapsedtime;

#if MULTISESSION
#ifdef KAILLERA
if (kPlay) {
#endif /* KAILLERA */
	extern int utf8_main(int argc, char **argv);
	int exit_code = 0;
	int argc = 0;
	const char *argv[256];
	WCHAR *pCmdLine;
	char *utf8_argv;

#if 0
// load settings from ini files
	char pModule[_MAX_PATH];
	char game_name[500];

	SaveOptions();

	GetModuleFileNameA(GetModuleHandle(NULL), pModule, _MAX_PATH);
	argv[0] = pModule;
	strcpy(game_name,drivers[nGameIndex]->name);
	argv[argc++] = pModule;
	argv[argc++] = drivers[nGameIndex]->name;

	if (override_playback_directory != NULL)
	{
		argv[argc++] = "-input_directory";
		argv[argc++] = override_playback_directory;
	}
	if (g_pPlayBkName != NULL)
	{
		argv[argc++] = "-pb";
		argv[argc++] = g_pPlayBkName;
	}
	if (g_pRecordName != NULL)
	{
		argv[argc++] = "-rec";
		argv[argc++] = g_pRecordName;
	}
	if (g_pRecordWaveName != NULL)
	{
		argv[argc++] = "-wavwrite";
		argv[argc++] = g_pRecordWaveName;
	}
	if (g_pRecordMNGName != NULL)
	{
		argv[argc++] = "-mngwrite";
		argv[argc++] = g_pRecordMNGName;
	}
	if (g_pSaveStateName != NULL)
	{
		argv[argc++] = "-state";
		argv[argc++] = g_pSaveStateName;
	}
#else
	char *p;
// pass settings via cmd-line
	pCmdLine = OptionsGetCommandLine(nGameIndex, override_options);
	utf8_argv = utf8_from_wstring(pCmdLine);
	p = utf8_argv;

	while (argc < 256 - 1)
	{
		while (*p == ' ')
			p++;

		if (*p == '\0')
			break;

		if (*p == '\"')
		{
			argv[argc++] = ++p;

			for (; *p != '\0'; p++)
				if (*p == '\"')
					break;

			if (*p != '\0')
				*p++ = '\0';

			continue;
		}

		argv[argc++] = p;

		for (; *p != '\0'; p++)
			if (*p == ' ')
				break;

		if (*p != '\0')
			*p++ = '\0';

		continue;
	}
	*p = '\0';
#endif
	argv[argc] = NULL;

	ShowWindow(hMain, SW_HIDE);

#ifdef KAILLERA
	end_resource_tracking();
	exit_resource_tracking();
#endif /* KAILLERA */

	time(&start);
	SetTimer(hMain, GAMEWND_TIMER, 1000/*1s*/, NULL);
	exit_code = utf8_main(argc, (char **)argv);
	time(&end);

#ifdef KAILLERA
	init_resource_tracking();
	begin_resource_tracking();

	cpuintrf_init(NULL);
	sndintrf_init(NULL);
//	code_init(NULL);
#endif /* KAILLERA */

	/*This is to make sure this timer is killed, if the Game Window was not found
	Should not happen, but you never know... */
	KillTimer(hMain,GAMEWND_TIMER);
	elapsedtime = end - start;

	if (pCmdLine)
		free(pCmdLine);
	
	if (utf8_argv)
		free(utf8_argv);

	if (exit_code == 0)
	{
		// Check the exitcode before incrementing Playtime
		IncrementPlayTime(nGameIndex, elapsedtime);
		ListView_RedrawItems(hwndList, GetSelectedPick(), GetSelectedPick());
	}

	if (GetHideMouseOnStartup())
	{
		ShowCursor(FALSE);
	}
	else
	{
		// recover windows cursor and our main window
		while (1)
		{
			if (ShowCursor(TRUE) >= 0)
				break;
		}
	}
	
#ifdef KAILLERA
	//ChangeLanguage(GetLangcode());

	if( !kPlay || bKailleraMAME32WindowHide == FALSE )
#endif /* KAILLERA */
	ShowWindow(hMain, SW_SHOW);

	// Kludge: Reset the font
	if (hFont != NULL)
		SetAllWindowsFont(hMain, &main_resize, hFont, FALSE);

	return exit_code;

#endif
#if !(MULTISESSION) || defined(KAILLERA)
#ifdef KAILLERA
} else {
#endif /* KAILLERA */
	DWORD               dwExitCode = 0;
	PROCESS_INFORMATION pi;
	WCHAR *pCmdLine;
	HWND hGameWnd = NULL;
	long lGameWndStyle = 0;
	BOOL process_created = FALSE;

	pCmdLine = OptionsGetCommandLine(nGameIndex, override_options);

	ZeroMemory(&pi, sizeof(pi));

	if (OnNT())
	{
		STARTUPINFOW        si;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);

		if (CreateProcessW(NULL,
		                    pCmdLine,
		                    NULL,		  /* Process handle not inheritable. */
		                    NULL,		  /* Thread handle not inheritable. */
		                    TRUE,		  /* Handle inheritance.  */
		                    0,			  /* Creation flags. */
		                    NULL,		  /* Use parent's environment block.  */
		                    NULL,		  /* Use parent's starting directory.  */
		                    &si,		  /* STARTUPINFO */
		                    &pi))		  /* PROCESS_INFORMATION */
			process_created  = TRUE;
	}
	else
	{
		STARTUPINFOA        si;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);

		if (CreateProcessA(NULL,
		                    _String(pCmdLine),
		                    NULL,		  /* Process handle not inheritable. */
		                    NULL,		  /* Thread handle not inheritable. */
		                    TRUE,		  /* Handle inheritance.  */
		                    0,			  /* Creation flags. */
		                    NULL,		  /* Use parent's environment block.  */
		                    NULL,		  /* Use parent's starting directory.  */
		                    &si,		  /* STARTUPINFO */
		                    &pi))		  /* PROCESS_INFORMATION */
			process_created  = TRUE;
	}

	if (!process_created)
	{
		dprintf("CreateProcess failed.");
		dwExitCode = GetLastError();
	}
	else
	{
		ShowWindow(hMain, SW_HIDE);
		SendIconToProcess(&pi, nGameIndex);
		if( ! GetGameCaption() )
		{
#ifdef KAILLERA
			hGameWnd = GetGameWindowA(&pi);
#else
			hGameWnd = GetGameWindow(&pi);
#endif /* KAILLERA */
			if( hGameWnd )
			{
				lGameWndStyle = GetWindowLong(hGameWnd, GWL_STYLE);
				lGameWndStyle = lGameWndStyle & (WS_BORDER ^ 0xffffffff);
				SetWindowLong(hGameWnd, GWL_STYLE, lGameWndStyle);
				SetWindowPos(hGameWnd,0,0,0,0,0,SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
			}
		}
		time(&start);

		// Wait until child process exits.
		WaitWithMessageLoop(pi.hProcess);

		GetExitCodeProcess(pi.hProcess, &dwExitCode);

		time(&end);
		elapsedtime = end - start;
		if( dwExitCode == 0 )
		{
			// Check the exitcode before incrementing Playtime
			IncrementPlayTime(nGameIndex, elapsedtime);
			ListView_RedrawItems(hwndList, GetSelectedPick(), GetSelectedPick());
		}

		ShowWindow(hMain, SW_SHOW);
		// Close process and thread handles.
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	free(pCmdLine);

	return dwExitCode;
#ifdef KAILLERA
}
#endif /* KAILLERA */
#endif
}

#undef WinMainInDLL
#ifndef DONT_USE_DLL
#ifdef _MSC_VER
#define WinMainInDLL
#endif /* _MSC_VER */
#endif /* !DONT_USE_DLL */

#ifdef WinMainInDLL
SHAREDOBJ_FUNC(int) WinMain_(HINSTANCE    hInstance,
#else
int WinMain_(HINSTANCE    hInstance,
#endif
                   HINSTANCE    hPrevInstance,
                   LPSTR        lpCmdLine,
                   int          nCmdShow)
{
	dprintf("MAME32 starting\n");

	use_gui_romloading = TRUE;

	/* set up language for windows */
	assign_msg_catategory(UI_MSG_OSD0, "windows");
	assign_msg_catategory(UI_MSG_OSD1, "ui");

	if (__argc != 1)
		exit(main_(__argc, __argv));

	if (!Win32UI_init(hInstance, lpCmdLine, nCmdShow))
		return 1;

	// pump message, but quit on WM_QUIT
	while(PumpMessage())
		;

	Win32UI_exit();
	return 0;
}


HWND GetMainWindow(void)
{
	return hMain;
}

HWND GetTreeView(void)
{
	return hTreeView;
}

void GetRealColumnOrder(int order[])
{
	int tmpOrder[COLUMN_MAX];
	int nColumnMax;
	int i;

	nColumnMax = Picker_GetNumColumns(hwndList);

	/* Get the Column Order and save it */
	if (!oldControl)
	{
		ListView_GetColumnOrderArray(hwndList, nColumnMax, tmpOrder);

		for (i = 0; i < nColumnMax; i++)
		{
			order[i] = Picker_GetRealColumnFromViewColumn(hwndList, tmpOrder[i]);
		}
	}
}

/*
 * PURPOSE: Format raw data read from an ICO file to an HICON
 * PARAMS:  PBYTE ptrBuffer  - Raw data from an ICO file
 *          UINT nBufferSize - Size of buffer ptrBuffer
 * RETURNS: HICON - handle to the icon, NULL for failure
 * History: July '95 - Created
 *          March '00- Seriously butchered from MSDN for mine own
 *          purposes, sayeth H0ek.
 */
HICON FormatICOInMemoryToHICON(PBYTE ptrBuffer, UINT nBufferSize)
{
	ICONIMAGE           IconImage;
	LPICONDIRENTRY      lpIDE = NULL;
	UINT                nNumImages;
	UINT                nBufferIndex = 0;
	HICON               hIcon = NULL;
	int                 i;

	/* Is there a WORD? */
	if (nBufferSize < sizeof(WORD))
	{
		return NULL;
	}

	/* Was it 'reserved' ?	 (ie 0) */
	if ((WORD)(ptrBuffer[nBufferIndex]) != 0)
	{
		return NULL;
	}

	nBufferIndex += sizeof(WORD);

	/* Is there a WORD? */
	if (nBufferSize - nBufferIndex < sizeof(WORD))
	{
		return NULL;
	}

	/* Was it type 1? */
	if ((WORD)(ptrBuffer[nBufferIndex]) != 1)
	{
		return NULL;
	}

	nBufferIndex += sizeof(WORD);

	/* Is there a WORD? */
	if (nBufferSize - nBufferIndex < sizeof(WORD))
	{
		return NULL;
	}

	/* Then that's the number of images in the ICO file */
	nNumImages = (WORD)(ptrBuffer[nBufferIndex]);

	/* Is there at least one icon in the file? */
	if ( nNumImages < 1 )
	{
		return NULL;
	}

	nBufferIndex += sizeof(WORD);

	/* Is there enough space for the icon directory entries? */
	if ((nBufferIndex + nNumImages * sizeof(ICONDIRENTRY)) > nBufferSize)
	{
		return NULL;
	}

	/* Assign icon directory entries from buffer */
	lpIDE = (LPICONDIRENTRY)(&ptrBuffer[nBufferIndex]);
	nBufferIndex += nNumImages * sizeof (ICONDIRENTRY);

	/* Search large icon index to load */
	for (i = 0; i < nNumImages; i++)
	{
		int width;
		int height;

		IconImage.dwNumBytes = lpIDE[i].dwBytesInRes;

		/* Seek to beginning of this image */
		if ( lpIDE[i].dwImageOffset > nBufferSize )
		{
			return NULL;
		}

		nBufferIndex = lpIDE[i].dwImageOffset;

		/* Read it in */
		if ((nBufferIndex + lpIDE[i].dwBytesInRes) > nBufferSize)
		{
			return NULL;
		}

		IconImage.lpBits = &ptrBuffer[nBufferIndex];
		nBufferIndex += lpIDE[i].dwBytesInRes;

		width = (*(LPBITMAPINFOHEADER)(IconImage.lpBits)).biWidth;
		height = (*(LPBITMAPINFOHEADER)(IconImage.lpBits)).biHeight / 2;

		if (width == GetSystemMetrics(SM_CXCURSOR) && height == GetSystemMetrics(SM_CYCURSOR))
			break;
	}

	/* If not found, use first one */
	if (i == nNumImages)
		i = 0;

	IconImage.dwNumBytes = lpIDE[i].dwBytesInRes;
	nBufferIndex = lpIDE[i].dwImageOffset;

	IconImage.lpBits = &ptrBuffer[nBufferIndex];
	nBufferIndex += lpIDE[i].dwBytesInRes;

	hIcon = CreateIconFromResourceEx(IconImage.lpBits, IconImage.dwNumBytes, TRUE, 0x00030000,
			(*(LPBITMAPINFOHEADER)(IconImage.lpBits)).biWidth, (*(LPBITMAPINFOHEADER)(IconImage.lpBits)).biHeight/2, 0 );

	/* It failed, odds are good we're on NT so try the non-Ex way */
	if (hIcon == NULL)
	{
		/* We would break on NT if we try with a 16bpp image */
		if (((LPBITMAPINFO)IconImage.lpBits)->bmiHeader.biBitCount != 16)
		{
			hIcon = CreateIconFromResource(IconImage.lpBits, IconImage.dwNumBytes, TRUE, 0x00030000);
		}
	}
	return hIcon;
}

static BOOL isFileExist(const WCHAR *fname)
{
	WIN32_FIND_DATAW FindData;
	HANDLE hFile;

	hFile = FindFirstFileW(fname, &FindData);
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	FindClose(hFile);

	return !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

static HICON ExtractIconFromZip(const WCHAR *zipname, const WCHAR *iconname)
{
	zip_file *zip;
	zip_error ziperr;
	const zip_file_header *entry;
	HICON hIcon = NULL;
	char *stemp;

	stemp = utf8_from_wstring(zipname);
	ziperr = zip_file_open(stemp, &zip);
	free(stemp);

	if (ziperr != ZIPERR_NONE)
		return NULL;

	stemp = utf8_from_wstring(iconname);

	for (entry = zip_file_first_file(zip); entry; entry = zip_file_next_file(zip))
		if (mame_stricmp(entry->filename, stemp) == 0)
			break;
	free(stemp);

	if (entry)
	{
		UINT8 *data = (UINT8 *)malloc(entry->uncompressed_length);

		if (data != NULL)
		{
			ziperr = zip_file_decompress(zip, data, entry->uncompressed_length);
			if (ziperr == ZIPERR_NONE)
				hIcon = FormatICOInMemoryToHICON(data, entry->uncompressed_length);

			free(data);
		}
	}

	zip_file_close(zip);

	return hIcon;
}

HICON LoadIconFromFile(const char *iconname)
{
	static const WCHAR* (*GetDirsFunc[])(void) =
	{
		GetIconsDirs,
		GetImgDirs,
		NULL
	};

	WCHAR iconfile[MAX_PATH];
	int is_zipfile;
	int i;

	swprintf(iconfile, TEXT("%s.ico"), _Unicode(iconname));

	for (is_zipfile = 0; is_zipfile < 2; is_zipfile++)
	{
		for (i = 0; GetDirsFunc[i]; i++)
		{
			WCHAR *paths = wcsdup(GetDirsFunc[i]());
			WCHAR *p;

			for (p = wcstok(paths, TEXT(";")); p; p =wcstok(NULL, TEXT(";")))
			{
				WCHAR tmpStr[MAX_PATH];
				HICON hIcon = 0;

				wcscpy(tmpStr, p);
				wcscat(tmpStr, TEXT(PATH_SEPARATOR));

				if (!is_zipfile)
					wcscat(tmpStr, iconfile);
				else
					wcscat(tmpStr, TEXT("icons.zip"));

				if (!isFileExist(tmpStr))
					continue;

				if (!is_zipfile)
					hIcon = ExtractIconW(hInst, tmpStr, 0);
				else
					hIcon = ExtractIconFromZip(tmpStr, iconfile);

				if (hIcon)
				{
					free(paths);
					return hIcon;
				}
			}

			free(paths);
		}
	}

	return NULL;
}

/* Return the number of games currently displayed */
int GetNumGames(void)
{
	return game_count;
}

LPWSTR GetSearchText(void)
{
	return g_SearchText;
}

/* Sets the treeview and listviews sizes in accordance with their visibility and the splitters */
static void ResizeTreeAndListViews(BOOL bResizeHidden)
{
	int i;
	int nLastWidth = 0;
	int nLastWidth2 = 0;
	int nLeftWindowWidth = 0;
	RECT rect;
	BOOL bVisible;

	/* Size the List Control in the Picker */
	GetClientRect(hMain, &rect);

	if (bShowStatusBar)
		rect.bottom -= bottomMargin;
	if (bShowToolBar)
		rect.top += topMargin;

	/* Tree control */
	ShowWindow(GetDlgItem(hMain, IDC_TREE), bShowTree ? SW_SHOW : SW_HIDE);

	for (i = 0; g_splitterInfo[i].nSplitterWindow; i++)
	{
		bVisible = GetWindowLong(GetDlgItem(hMain, g_splitterInfo[i].nLeftWindow), GWL_STYLE) & WS_VISIBLE ? TRUE : FALSE;
		if (bResizeHidden || bVisible)
		{
			nLeftWindowWidth = nSplitterOffset[i] - SPLITTER_WIDTH/2 - nLastWidth;

			/* special case for the rightmost pane when the screenshot is gone */
			if (!GetShowScreenShot() && !g_splitterInfo[i+1].nSplitterWindow)
				nLeftWindowWidth = rect.right - nLastWidth;

			/* woah?  are we overlapping ourselves? */
			if (nLeftWindowWidth < MIN_VIEW_WIDTH)
			{
				nLastWidth = nLastWidth2;
				nLeftWindowWidth = nSplitterOffset[i] - MIN_VIEW_WIDTH - (SPLITTER_WIDTH*3/2) - nLastWidth;
				i--;
			}

			MoveWindow(GetDlgItem(hMain, g_splitterInfo[i].nLeftWindow), nLastWidth, rect.top + 2,
				nLeftWindowWidth, (rect.bottom - rect.top) - 4 , TRUE);

			MoveWindow(GetDlgItem(hMain, g_splitterInfo[i].nSplitterWindow), nSplitterOffset[i], rect.top + 2,
				SPLITTER_WIDTH, (rect.bottom - rect.top) - 4, TRUE);
		}

		if (bVisible)
		{
			nLastWidth2 = nLastWidth;
			nLastWidth += nLeftWindowWidth + SPLITTER_WIDTH; 
		}
	}
}

/* Adjust the list view and screenshot button based on GetShowScreenShot() */
void UpdateScreenShot(void)
{
	RECT rect;
	int  nWidth;
	RECT fRect;
	POINT p = {0, 0};

	/* first time through can't do this stuff */
	if (hwndList == NULL)
		return;

	/* Size the List Control in the Picker */
	GetClientRect(hMain, &rect);

	if (bShowStatusBar)
		rect.bottom -= bottomMargin;
	if (bShowToolBar)
		rect.top += topMargin;

	if (GetShowScreenShot())
	{
		nWidth = nSplitterOffset[GetSplitterCount() - 1];
		CheckMenuItem(GetMenu(hMain),ID_VIEW_PICTURE_AREA, MF_CHECKED);
		ToolBar_CheckButton(hToolBar, ID_VIEW_PICTURE_AREA, MF_CHECKED);
	}
	else
	{
		nWidth = rect.right;
		CheckMenuItem(GetMenu(hMain),ID_VIEW_PICTURE_AREA, MF_UNCHECKED);
		ToolBar_CheckButton(hToolBar, ID_VIEW_PICTURE_AREA, MF_UNCHECKED);
	}

	ResizeTreeAndListViews(FALSE);

	FreeScreenShot();

	if (have_selection)
	{
#ifdef USE_IPS
		// load and set image, or empty it if we don't have one
		if (g_IPSMenuSelectName)
			LoadScreenShot(Picker_GetSelectedItem(hwndList), g_IPSMenuSelectName, TAB_IPS);
		else
#endif /* USE_IPS */
			LoadScreenShot(Picker_GetSelectedItem(hwndList), NULL, TabView_GetCurrentTab(hTabCtrl));
	}

	// figure out if we have a history or not, to place our other windows properly
	UpdateHistory();

	// setup the picture area

	if (GetShowScreenShot())
	{
		DWORD dwStyle;
		DWORD dwStyleEx;
		BOOL showing_history;

		ClientToScreen(hMain, &p);
		GetWindowRect(GetDlgItem(hMain, IDC_SSFRAME), &fRect);
		OffsetRect(&fRect, -p.x, -p.y);

		// show history on this tab IFF
		// - we have history for the game
		// - we're on the first tab
		// - we DON'T have a separate history tab
		showing_history = (have_history && NeedHistoryText());
		CalculateBestScreenShotRect(GetDlgItem(hMain, IDC_SSFRAME), &rect,showing_history);

		dwStyle   = GetWindowLong(GetDlgItem(hMain, IDC_SSPICTURE), GWL_STYLE);
		dwStyleEx = GetWindowLong(GetDlgItem(hMain, IDC_SSPICTURE), GWL_EXSTYLE);

		AdjustWindowRectEx(&rect, dwStyle, FALSE, dwStyleEx);
		MoveWindow(GetDlgItem(hMain, IDC_SSPICTURE),
		           fRect.left  + rect.left,
		           fRect.top   + rect.top,
		           rect.right  - rect.left,
		           rect.bottom - rect.top,
		           TRUE);

		ShowWindow(GetDlgItem(hMain,IDC_SSPICTURE), NeedScreenShotImage() ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(hMain,IDC_SSFRAME),SW_SHOW);
		ShowWindow(GetDlgItem(hMain,IDC_SSTAB),bShowTabCtrl ? SW_SHOW : SW_HIDE);

		InvalidateRect(GetDlgItem(hMain,IDC_SSPICTURE),NULL,FALSE);
	}
	else
	{
		ShowWindow(GetDlgItem(hMain,IDC_SSPICTURE),SW_HIDE);
		ShowWindow(GetDlgItem(hMain,IDC_SSFRAME),SW_HIDE);
		ShowWindow(GetDlgItem(hMain,IDC_SSTAB),SW_HIDE);
	}

}

void ResizePickerControls(HWND hWnd)
{
	RECT frameRect;
	RECT rect;
	int  nListWidth, nScreenShotWidth;
	static BOOL firstTime = TRUE;
	int  doSSControls = TRUE;
	int i, nSplitterCount;

	nSplitterCount = GetSplitterCount();

	/* Size the List Control in the Picker */
	GetClientRect(hWnd, &rect);

	/* Calc the display sizes based on g_splitterInfo */
	if (firstTime)
	{
		RECT rWindow;

		for (i = 0; i < nSplitterCount; i++)
			nSplitterOffset[i] = rect.right * g_splitterInfo[i].dPosition;

		GetWindowRect(hStatusBar, &rWindow);
		bottomMargin = rWindow.bottom - rWindow.top;
		GetWindowRect(hToolBar, &rWindow);
		topMargin = rWindow.bottom - rWindow.top;
		/*buttonMargin = (sRect.bottom + 4); */

		firstTime = FALSE;
	}
	else
	{
		doSSControls = GetShowScreenShot();
	}

	if (bShowStatusBar)
		rect.bottom -= bottomMargin;

	if (bShowToolBar)
		rect.top += topMargin;

	MoveWindow(GetDlgItem(hWnd, IDC_DIVIDER), rect.left, rect.top - 4, rect.right, 2, TRUE);

	ResizeTreeAndListViews(TRUE);

	nListWidth = nSplitterOffset[nSplitterCount-1];
	nScreenShotWidth = (rect.right - nListWidth) - 4;

	/* Screen shot Page tab control */
	if (bShowTabCtrl)
	{
		MoveWindow(GetDlgItem(hWnd, IDC_SSTAB), nListWidth + 4, rect.top + 2,
			nScreenShotWidth - 2, rect.top + 20, doSSControls);
		rect.top += 20;
	}

	/* resize the Screen shot frame */
	MoveWindow(GetDlgItem(hWnd, IDC_SSFRAME), nListWidth + 4, rect.top + 2,
		nScreenShotWidth - 2, (rect.bottom - rect.top) - 4, doSSControls);

	/* The screen shot controls */
	GetClientRect(GetDlgItem(hWnd, IDC_SSFRAME), &frameRect);

	/* Text control - game history */
	history_rect.left = nListWidth + 14;
	history_rect.right = nScreenShotWidth - 22;

	history_rect.top = rect.top;
	history_rect.bottom = rect.bottom;

	/* the other screen shot controls will be properly placed in UpdateScreenshot() */
}

static int modify_separator_len(const char *str)
{
	int n;

	switch (*str)
	{
	case ' ':
		if (!strncmp(str, " - ", 3))
			return 3;

		if ((n = modify_separator_len(str + 1)) != 0)
			return n + 1;

		break;

	case ':':
	case '/':
	case ',':
	case '(':
	case ')':
	case '!':
		for (n = 1; str[n]; n++)
			if (str[n] != ' ')
				break;
		return modify_separator_len(str + n) + n;
	}

	return 0;
}

static char *ModifyThe(const char *str)
{
	static int  bufno = 0;
	static char buffer[4][255];
	int modified = 0;
	char *ret = (char *)str;
	char *t;

	t = buffer[bufno];

	while (*str)
	{
		char *p = t;

		while (!modify_separator_len(str))
		{
			if ((*p++ = *str) == '\0')
				break;
			str++;
		}

		*p = '\0';

		if (strncmp(t, "The ", 4) == 0)
		{
			char temp[255];

			strcpy(temp, t + 4);
			strcat(temp, ", The");

			strcpy(t, temp);
			p++;
			modified = 1;
		}

		t = p + modify_separator_len(str);
		while (p < t)
			*p++ = *str++;
		*p = '\0';
	}

	if (modified)
	{
		ret = buffer[bufno];
		bufno = (bufno + 1) % 4;
	}

	return ret;
}

HBITMAP GetBackgroundBitmap(void)
{
	return hBackground;
}

HPALETTE GetBackgroundPalette(void)
{
	return hPALbg;
}

MYBITMAPINFO * GetBackgroundInfo(void)
{
	return &bmDesc;
}

BOOL GetUseOldControl(void)
{
	return oldControl;
}

BOOL GetUseXPControl(void)
{
	return xpControl;
}

int GetMinimumScreenShotWindowWidth(void)
{
	BITMAP bmp;
	GetObject(hMissing_bitmap,sizeof(BITMAP),&bmp);

	return bmp.bmWidth + 6; // 6 is for a little breathing room
}


int GetDriverIndex(const game_driver *driver)
{
	return GetGameNameIndex(driver->name);
}

int GetParentIndex(const game_driver *driver)
{
	return GetGameNameIndex(driver->parent);
}

int GetParentRomSetIndex(const game_driver *driver)
{
	int nParentIndex = GetGameNameIndex(driver->parent);

	if( nParentIndex >= 0)
	{
		if ((drivers[nParentIndex]->flags & GAME_IS_BIOS_ROOT) == 0)
			return nParentIndex;
	}

	return -1;
}

#if defined(KAILLERA) || defined(MAME32PLUSPLUS)
int GetParentRomSetIndex2(const game_driver *driver)
{
	int nParentIndex = GetGameNameIndex(driver->parent);
	int nParentIndex2 = -1;

	do
	{
		if( nParentIndex >= 0)
		{
			if ((drivers[nParentIndex]->flags & GAME_IS_BIOS_ROOT) == 0)
				nParentIndex2 = nParentIndex;
		} else break;
	} while ((nParentIndex = GetGameNameIndex(drivers[nParentIndex]->parent)) >= 0);

	return nParentIndex2;
}
#endif

int GetGameNameIndex(const char *name)
{
	driver_data_type *driver_index_info;
	driver_data_type key;
	key.name = name;

	// uses our sorted array of driver names to get the index in log time
	driver_index_info = bsearch(&key, sorted_drivers, game_count, sizeof(*sorted_drivers), DriverDataCompareFunc);
	if (driver_index_info == NULL)
		return -1;

	return driver_index_info->index;
}

int GetIndexFromSortedIndex(int sorted_index)
{
	return sorted_drivers[sorted_index].index;
}

/***************************************************************************
    Internal functions
 ***************************************************************************/

typedef struct
{
	int readings;
	int description;
} sort_index_t;

typedef struct
{
	const WCHAR *str;
	const WCHAR *str2;
	int index;
} sort_comapre_t;

static sort_index_t *sort_index;


static int sort_comapre_str(const void *p1, const void *p2)
{
	int result = wcsicmp(((const sort_comapre_t *)p1)->str, ((const sort_comapre_t *)p2)->str);
	if (result)
		return result;

	return wcsicmp(((const sort_comapre_t *)p1)->str2, ((const sort_comapre_t *)p2)->str2);
}

static void build_sort_index(void)
{
	sort_comapre_t *ptemp;
	int i;

	if (!sort_index)
	{
		sort_index = malloc(sizeof (*sort_index) * game_count);
		assert(sort_index);
	}

	ptemp = malloc(sizeof (*ptemp) * game_count);
	assert(ptemp);

	memset(ptemp, 0, sizeof (*ptemp) * game_count);

	// process description
	for (i = 0; i < game_count; i++)
	{
		ptemp[i].index = i;
		ptemp[i].str = driversw[i]->modify_the;
		ptemp[i].str2 = driversw[i]->description;
	}

	qsort(ptemp, game_count, sizeof (*ptemp), sort_comapre_str);

	for (i = 0; i < game_count; i++)
		sort_index[ptemp[i].index].description = i;

	free(ptemp);
}

static void build_sort_readings(void)
{
	sort_comapre_t *ptemp;
	int i;

	ptemp = malloc(sizeof (*ptemp) * game_count);
	assert(ptemp);

	// process readings
	for (i = 0; i < game_count; i++)
	{
		WCHAR *r;
		WCHAR *r2;

		r = _READINGSW(driversw[i]->description);
		if (r != driversw[i]->description)
		{
			r2 = _LSTW(driversw[i]->description);
		}
		else
		{
			r = _LSTW(driversw[i]->description);
			if (r != driversw[i]->description)
			{
				r2 = driversw[i]->modify_the;
			}
			else
			{
				r = driversw[i]->modify_the;
				r2 = driversw[i]->description;
			}
		}

		ptemp[i].index = i;
		ptemp[i].str = r;
		ptemp[i].str2 = r2;
	}

	qsort(ptemp, game_count, sizeof (*ptemp), sort_comapre_str);

	for (i = 0; i < game_count; i++)
		sort_index[ptemp[i].index].readings = i;

	free(ptemp);
}

static void build_driversw(void)
{
	int i;

	driversw = malloc(sizeof (*driversw) * (game_count + 1));
	assert(driversw);

	driversw[game_count] = NULL;
	for (i = 0; i < game_count; i++)
	{
		driversw[i] = malloc(sizeof *driversw[i]);
		assert(driversw[i]);

		driversw[i]->name = wcsdup(_Unicode(drivers[i]->name));
		driversw[i]->description = wcsdup(_Unicode(drivers[i]->description));
		driversw[i]->modify_the = wcsdup(_Unicode(ModifyThe(drivers[i]->description)));
		assert(driversw[i]->name && driversw[i]->description && driversw[i]->modify_the);

		driversw[i]->manufacturer = wcsdup(_Unicode(drivers[i]->manufacturer));
		driversw[i]->year = wcsdup(_Unicode(drivers[i]->year));
		assert(driversw[i]->manufacturer && driversw[i]->year);

		driversw[i]->source_file = wcsdup(_Unicode(drivers[i]->source_file));
		assert(driversw[i]->source_file);
	}
}

static void free_driversw(void)
{
	int i;

	for (i = 0; i < game_count; i++)
	{
		free(driversw[i]);

		free(driversw[i]->name);
		free(driversw[i]->description);
		free(driversw[i]->modify_the);

		free(driversw[i]->manufacturer);
		free(driversw[i]->year);

		free(driversw[i]->source_file);
	}

	free(driversw);
	driversw = NULL;
}

static void ChangeLanguage(int id)
{
	int nGame = Picker_GetSelectedItem(hwndList);
	int i;

	if (id)
		SetLangcode(id - ID_LANGUAGE_ENGLISH_US);

	for (i = 0; i < UI_LANG_MAX; i++)
	{
		UINT cp = ui_lang_info[i].codepage;

		CheckMenuItem(GetMenu(hMain), i + ID_LANGUAGE_ENGLISH_US, i == GetLangcode() ? MF_CHECKED : MF_UNCHECKED);
		if (OnNT())
			EnableMenuItem(GetMenu(hMain), i + ID_LANGUAGE_ENGLISH_US, IsValidCodePage(cp) ? MF_ENABLED : MF_GRAYED);
		else
			EnableMenuItem(GetMenu(hMain), i + ID_LANGUAGE_ENGLISH_US, (i == UI_LANG_EN_US || cp == GetOEMCP()) ? MF_ENABLED : MF_GRAYED);
	}

	if (id)
	{
		LOGFONTW logfont;

		if (hFont != NULL)
			DeleteObject(hFont);

		GetTranslatedFont(&logfont);

		SetListFont(&logfont);

		hFont = TranslateCreateFont(&logfont);
	}

	build_sort_readings();

	if (id && hToolBar != NULL)
	{
		DestroyWindow(hToolBar);
		hToolBar = InitToolbar(hMain);
		main_resize_items[0].u.hwnd = hToolBar;
		ToolBar_CheckButton(hToolBar, ID_VIEW_FOLDERS, (bShowTree) ? MF_CHECKED : MF_UNCHECKED);
		ToolBar_CheckButton(hToolBar, IDC_USE_LIST, UseLangList() ^ (GetLangcode() == UI_LANG_EN_US) ? MF_CHECKED : MF_UNCHECKED);
		ToolBar_CheckButton(hToolBar, ID_VIEW_PICTURE_AREA, GetShowScreenShot() ? MF_CHECKED : MF_UNCHECKED);
		ToolBar_CheckButton(hToolBar, ID_VIEW_STATUS, (bShowStatusBar) ? MF_CHECKED : MF_UNCHECKED);
		ToolBar_CheckButton(hToolBar, ID_VIEW_LARGE_ICON + Picker_GetViewID(hwndList), MF_CHECKED);
#ifdef KAILLERA
		ToolBar_CheckButton(hToolBar, IDC_USE_NETPLAY_FOLDER, GetNetPlayFolder() ? MF_CHECKED : MF_UNCHECKED);
		ToolBar_CheckButton(hToolBar, IDC_USE_IME_IN_CHAT, GetUseImeInChat() ? MF_CHECKED : MF_UNCHECKED);
#endif /* KAILLERA */
		ShowWindow(hToolBar, (bShowToolBar) ? SW_SHOW : SW_HIDE);
	}

	TranslateDialog(hMain, 0, TRUE);
	TranslateMenu(GetMenu(hMain), 0);
	DrawMenuBar(hMain);

	TranslateTreeFolders(hTreeView);

	/* Reset the font */
	if (hFont != NULL)
		SetAllWindowsFont(hMain, &main_resize, hFont, FALSE);

	TabView_Reset(hTabCtrl);
	TabView_UpdateSelection(hTabCtrl);
	UpdateHistory();

	ResetColumnDisplay(FALSE);

	Picker_SetSelectedItem(hwndList, nGame);
}

#ifdef IMAGE_MENU
static void ApplyMenuStyle(HINSTANCE hInst, HWND hwnd, HMENU menuHandle)
{
	if (GetImageMenuStyle() > 0)
	{
		IMITEMIMAGE imi;
		int i;

		ImageMenu_Create(hwnd, menuHandle, TRUE);

		imi.mask = IMIMF_LOADFROMRES | IMIMF_ICON;
		imi.hInst = hInst;

	    for (i = 0; menu_icon_table[i].itemID; i++)
	    {
		    imi.itemID = menu_icon_table[i].itemID;
		    imi.imageStr = MAKEINTRESOURCE(menu_icon_table[i].iconID);
		    ImageMenu_SetItemImage(&imi);
	    }

		ImageMenu_SetStyle(GetImageMenuStyle() - 1);
	}
}

static void ChangeMenuStyle(int id)
{
	if (id)
		SetImageMenuStyle(id - ID_STYLE_NONE);

	CheckMenuRadioItem(GetMenu(hMain), ID_STYLE_NONE, ID_STYLE_NONE + MENU_STYLE_MAX, ID_STYLE_NONE + GetImageMenuStyle(), MF_BYCOMMAND);
	ApplyMenuStyle(hInst, hMain, GetMenu(hMain));
}
#endif /* IMAGE_MENU */

// used for our sorted array of game names
int CLIB_DECL DriverDataCompareFunc(const void *arg1,const void *arg2)
{
	return strcmp( ((driver_data_type *)arg1)->name, ((driver_data_type *)arg2)->name );
}

static void ResetBackground(const WCHAR *szFile)
{
	WCHAR szDestFile[MAX_PATH];

	/* The MAME core load the .png file first, so we only need replace this file */
	wcscpy(szDestFile, GetBgDir());
	wcscat(szDestFile, TEXT("\\bkground.png"));
	SetFileAttributes(szDestFile, FILE_ATTRIBUTE_NORMAL);
	CopyFile(szFile, szDestFile, FALSE);
}

static void RandomSelectBackground(void)
{
	WIN32_FIND_DATAW ffd;
	HANDLE hFile;
	WCHAR szFile[MAX_PATH];
	int count=0;
	const WCHAR *szDir = GetBgDir();
	WCHAR *buf=malloc((_MAX_FNAME * MAX_BGFILES) * sizeof (*buf));

	if (buf == NULL)
		return;

	wcscpy(szFile, szDir);
	wcscat(szFile, TEXT("\\*.bmp"));
	hFile = FindFirstFileW(szFile, &ffd);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		int Done = 0;
		while (!Done && count < MAX_BGFILES)
		{
			memcpy(buf + count * _MAX_FNAME, ffd.cFileName, _MAX_FNAME * sizeof (*buf));
			count++;
			Done = !FindNextFileW(hFile, &ffd);
		}
		FindClose(hFile);
	}

	wcscpy(szFile, szDir);
	wcscat(szFile, TEXT("\\*.png"));
	hFile = FindFirstFileW(szFile, &ffd);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		int Done = 0;
		while (!Done && count < MAX_BGFILES)
		{
			memcpy(buf + count * _MAX_FNAME, ffd.cFileName, _MAX_FNAME * sizeof (*buf));
			count++;
			Done = !FindNextFileW(hFile, &ffd);
		}
		FindClose(hFile);
	}

	if (count)
	{
		srand( (unsigned)time( NULL ) );
		wcscpy(szFile, szDir);
		wcscat(szFile, TEXT("\\"));
		wcscat(szFile, buf + (rand() % count) * _MAX_FNAME);
		ResetBackground(szFile);
	}

	free(buf);
}

void SetMainTitle(void)
{
	char version[50];
	WCHAR buffer[100];
	WCHAR tmp[50];

#ifdef KAILLERA
	swprintf(tmp, TEXT("%s"), TEXT("Plus! Plus!"));
#else
	swprintf(tmp, TEXT("%s"), TEXT("Plus!"));
#endif /* KAILLERA */

	sscanf(build_version,"%s",version);
	swprintf(buffer, TEXT("%s %s %s"),
#ifdef HAZEMD
		TEXT("HazeMD"),
#else
		TEXT(MAME32NAME),
#endif /*HAZEMD */
		tmp,
		_Unicode(version));

	SetWindowText(hMain, buffer);
}

static void TabSelectionChanged(void)
{
#ifdef USE_IPS
	FreeIfAllocatedW(&g_IPSMenuSelectName);
#endif /* USE_IPS */

	UpdateScreenShot();
}

static BOOL Win32UI_init(HINSTANCE hInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASS wndclass;
	RECT     rect;
	int      i;
	int      nSplitterCount;
	LONG     common_control_version = GetCommonControlVersion();

	srand((unsigned)time(NULL));

	init_resource_tracking();
	begin_resource_tracking();

#ifdef DRIVER_SWITCH
	{
		core_options *options;
		mame_file *file;
		file_error filerr;

		options = mame_options_init(mame_win_options);

		filerr = mame_fopen_options(options, SEARCHPATH_RAW, CONFIGNAME ".ini", OPEN_FLAG_READ, &file);
		if (filerr == FILERR_NONE)
		{
			options_parse_ini_file(options, mame_core_file(file), OPTION_PRIORITY_INI);
			mame_fclose(file);
		}

		assign_drivers(options);

		options_free(options);
	}
#endif /* DRIVER_SWITCH */

	// Count the number of games
	game_count = 0;
	while (drivers[game_count] != 0)
		game_count++;

	build_driversw();
	build_sort_index();

	/* custom per-game icons */
	icon_index = malloc(sizeof (*icon_index) * game_count);
	if (!icon_index)
		return FALSE;
	ZeroMemory(icon_index, sizeof (*icon_index) * game_count);

	/* sorted list of drivers by name */
	sorted_drivers = (driver_data_type *) malloc(sizeof (*sorted_drivers) * game_count);
	if (!sorted_drivers)
		return FALSE;
	for (i=0; i<game_count; i++)
	{
		sorted_drivers[i].name = drivers[i]->name;
		sorted_drivers[i].index = i;
	}
	qsort(sorted_drivers, game_count, sizeof (*sorted_drivers), DriverDataCompareFunc);

	/* initialize cpu information */
	cpuintrf_init(NULL);

	/* initialize sound information */
	sndintrf_init(NULL);

	wndclass.style         = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc   = MameWindowProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = DLGWINDOWEXTRA;
	wndclass.hInstance     = hInstance;
	wndclass.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAME32_ICON));
	wndclass.hCursor       = NULL;
	wndclass.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
	wndclass.lpszMenuName  = MAKEINTRESOURCE(IDR_UI_MENU);
	wndclass.lpszClassName = TEXT("MainClass");

	RegisterClass(&wndclass);

	InitCommonControls();

	// Are we using an Old comctl32.dll?
	dprintf("common control version %i.%i",common_control_version >> 16,
	        common_control_version & 0xffff);
			 
	oldControl = (common_control_version < PACKVERSION(4,71));
	xpControl = (common_control_version >= PACKVERSION(6,0));
	if (oldControl)
	{
		char buf[] = MAME32NAME " has detected an old version of comctl32.dll\n\n"
					 "Game Properties, many configuration options and\n"
					 "features are not available without an updated DLL\n\n"
					 "Please install the common control update found at:\n\n"
					 "http://www.microsoft.com/msdownload/ieplatform/ie/comctrlx86.asp\n\n"
					 "Would you like to continue without using the new features?\n";

		if (IDNO == MessageBoxA(0, buf, MAME32NAME " Outdated comctl32.dll Warning", MB_YESNO | MB_ICONWARNING))
			return FALSE;
	}

	dprintf("about to init options");
	OptionsInit();
	dprintf("options loaded");

	datafile_init(get_core_options());

#ifdef USE_SHOW_SPLASH_SCREEN
	// Display splash screen window
	if (GetDisplaySplashScreen() != FALSE)
		CreateBackgroundMain(hInstance);
#endif /* USE_SHOW_SPLASH_SCREEN */

	/* USE LANGUAGE LIST */
	build_sort_readings();

	g_mame32_message = RegisterWindowMessage(TEXT_MAME32NAME);
	g_bDoBroadcast = GetBroadcast();

	HelpInit();

	wcscpy(last_directory, GetInpDir());
#ifdef MAME_AVI
	wcscpy(last_directory_avi, GetAviDir());
#endif /* MAME_AVI */
	hMain = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), 0, NULL);
	if (hMain == NULL)
	{
		dprintf("error creating main dialog, aborting");
		return FALSE;
	}

	SetMainTitle();
	hTabCtrl = GetDlgItem(hMain, IDC_SSTAB);

	{
		struct TabViewOptions opts;

		static struct TabViewCallbacks s_tabviewCallbacks =
		{
			GetShowTabCtrl,			// pfnGetShowTabCtrl
			SetCurrentTab,			// pfnSetCurrentTab
			GetCurrentTab,			// pfnGetCurrentTab
			SetShowTab,				// pfnSetShowTab
			GetShowTab,				// pfnGetShowTab

			GetImageTabShortName,	// pfnGetTabShortName
			GetImageTabLongName,	// pfnGetTabLongName
			TabSelectionChanged		// pfnOnSelectionChanged
		};

		memset(&opts, 0, sizeof(opts));
		opts.pCallbacks = &s_tabviewCallbacks;
		opts.nTabCount = MAX_TAB_TYPES;

		if (!SetupTabView(hTabCtrl, &opts))
			return FALSE;
	}

	/* subclass history window */
	g_lpHistoryWndProc = (WNDPROC)(LONG)(int)GetWindowLong(GetDlgItem(hMain, IDC_HISTORY), GWL_WNDPROC);
	SetWindowLong(GetDlgItem(hMain, IDC_HISTORY), GWL_WNDPROC, (LONG)HistoryWndProc);

	/* subclass picture frame area */
	g_lpPictureFrameWndProc = (WNDPROC)(LONG)(int)GetWindowLong(GetDlgItem(hMain, IDC_SSFRAME), GWL_WNDPROC);
	SetWindowLong(GetDlgItem(hMain, IDC_SSFRAME), GWL_WNDPROC, (LONG)PictureFrameWndProc);

	/* subclass picture area */
	g_lpPictureWndProc = (WNDPROC)(LONG)(int)GetWindowLong(GetDlgItem(hMain, IDC_SSPICTURE), GWL_WNDPROC);
	SetWindowLong(GetDlgItem(hMain, IDC_SSPICTURE), GWL_WNDPROC, (LONG)PictureWndProc);

	/* Load the pic for the default screenshot. */
	hMissing_bitmap = LoadBitmap(GetModuleHandle(NULL),MAKEINTRESOURCE(IDB_ABOUT));

	/* Stash hInstance for later use */
	hInst = hInstance;

	hToolBar   = InitToolbar(hMain);
	hStatusBar = InitStatusBar(hMain);
	hProgWnd   = InitProgressBar(hStatusBar);

	main_resize_items[0].u.hwnd = hToolBar;
	main_resize_items[1].u.hwnd = hStatusBar;

	/* In order to handle 'Large Fonts' as the Windows
	 * default setting, we need to make the dialogs small
	 * enough to fit in our smallest window size with
	 * large fonts, then resize the picker, tab and button
	 * controls to fill the window, no matter which font
	 * is currently set.  This will still look like bad
	 * if the user uses a bigger default font than 125%
	 * (Large Fonts) on the Windows display setting tab.
	 *
	 * NOTE: This has to do with Windows default font size
	 * settings, NOT our picker font size.
	 */

	GetClientRect(hMain, &rect);

	hTreeView = GetDlgItem(hMain, IDC_TREE);
	hwndList  = GetDlgItem(hMain, IDC_LIST);

	//history_filename = mame_strdup(GetHistoryFile());
#ifdef STORY_DATAFILE
	//story_filename = mame_strdup(GetStoryFile());
#endif /* STORY_DATAFILE */
	//mameinfo_filename = mame_strdup(GetMAMEInfoFile());

	if (!InitSplitters())
		return FALSE;

	nSplitterCount = GetSplitterCount();
	for (i = 0; i < nSplitterCount; i++)
	{
		HWND hWnd;
		HWND hWndLeft;
		HWND hWndRight;

		hWnd = GetDlgItem(hMain, g_splitterInfo[i].nSplitterWindow);
		hWndLeft = GetDlgItem(hMain, g_splitterInfo[i].nLeftWindow);
		hWndRight = GetDlgItem(hMain, g_splitterInfo[i].nRightWindow);

		AddSplitter(hWnd, hWndLeft, hWndRight, g_splitterInfo[i].pfnAdjust);
	}

	/* Initial adjustment of controls on the Picker window */
	ResizePickerControls(hMain);

	TabView_UpdateSelection(hTabCtrl);

	bDoGameCheck = GetGameCheck();
	idle_work    = TRUE;
	game_index   = 0;

	bShowTree      = GetShowFolderList();
	bShowToolBar   = GetShowToolBar();
	bShowStatusBar = GetShowStatusBar();
	bShowTabCtrl   = GetShowTabCtrl();
#ifdef KAILLERA
    bUseFavorite   = GetNetPlayFolder();
    bUseImeInChat  = GetUseImeInChat();
    bKailleraMAME32WindowHide  = GetKailleraMAME32WindowHide();
	bKailleraMAME32WindowOwner = GetKailleraMAME32WindowOwner();
	bKailleraNetPlay	= FALSE;
#endif /* KAILLERA */

	CheckMenuRadioItem(GetMenu(hMain), ID_VIEW_BYGAME, ID_VIEW_BYPLAYTIME, GetSortColumn(), MF_CHECKED);

	CheckMenuItem(GetMenu(hMain), ID_VIEW_FOLDERS, (bShowTree) ? MF_CHECKED : MF_UNCHECKED);
	ToolBar_CheckButton(hToolBar, ID_VIEW_FOLDERS, (bShowTree) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(GetMenu(hMain), ID_VIEW_TOOLBARS, (bShowToolBar) ? MF_CHECKED : MF_UNCHECKED);
	ShowWindow(hToolBar, (bShowToolBar) ? SW_SHOW : SW_HIDE);
	CheckMenuItem(GetMenu(hMain), ID_VIEW_STATUS, (bShowStatusBar) ? MF_CHECKED : MF_UNCHECKED);
	ShowWindow(hStatusBar, (bShowStatusBar) ? SW_SHOW : SW_HIDE);
	CheckMenuItem(GetMenu(hMain), ID_VIEW_PAGETAB, (bShowTabCtrl) ? MF_CHECKED : MF_UNCHECKED);
	ToolBar_CheckButton(hToolBar, IDC_USE_LIST, UseLangList() ^ (GetLangcode() == UI_LANG_EN_US) ? MF_CHECKED : MF_UNCHECKED);
#ifdef KAILLERA
	ToolBar_CheckButton(hToolBar, IDC_USE_NETPLAY_FOLDER, GetNetPlayFolder() ? MF_CHECKED : MF_UNCHECKED);
	ToolBar_CheckButton(hToolBar, IDC_USE_IME_IN_CHAT, GetUseImeInChat() ? MF_CHECKED : MF_UNCHECKED);
#endif /* KAILLERA */
	DragAcceptFiles(hMain, TRUE);

	if (oldControl)
	{
		EnableMenuItem(GetMenu(hMain), ID_CUSTOMIZE_FIELDS,  MF_GRAYED);
		EnableMenuItem(GetMenu(hMain), ID_GAME_PROPERTIES,   MF_GRAYED);
		EnableMenuItem(GetMenu(hMain), ID_FOLDER_SOURCEPROPERTIES, MF_GRAYED);
		EnableMenuItem(GetMenu(hMain), ID_BIOS_PROPERTIES, MF_GRAYED);
		EnableMenuItem(GetMenu(hMain), ID_OPTIONS_DEFAULTS,  MF_GRAYED);
	}

	/* Init DirectDraw */
	if (!DirectDraw_Initialize())
	{
		DialogBox(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_DIRECTX), NULL, DirectXDialogProc);
		return FALSE;
	}

	if (GetRandomBackground())
		RandomSelectBackground();

	LoadBackgroundBitmap();

	dprintf("about to init tree");
	InitTree(g_folderData, g_filterList);
	dprintf("did init tree");

	PropertiesInit();

	/* Initialize listview columns */
	InitListView();
	SetFocus(hwndList);

	/* Reset the font */
	{
		LOGFONTW logfont;

		GetListFont(&logfont);
		hFont = TranslateCreateFont(&logfont);
		if (hFont != NULL)
			SetAllWindowsFont(hMain, &main_resize, hFont, FALSE);
	}

	/* Init DirectInput */
	if (!DirectInputInitialize())
	{
		DialogBox(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_DIRECTX), NULL, DirectXDialogProc);
		return FALSE;
	}

	AdjustMetrics();
	UpdateScreenShot();

	hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDA_TAB_KEYS));

	/* initialize keyboard_state */
	{
		keyboard_state_count = 0;
 		for (i = 0; i < wininput_count_key_trans_table(); i++)
			if (keyboard_state_count < win_key_trans_table[i][MAME_KEY] + 1)
				keyboard_state_count = win_key_trans_table[i][MAME_KEY] + 1;
		keyboard_state = malloc(sizeof (*keyboard_state) * keyboard_state_count);

		/* clear keyboard state */
		KeyboardStateClear();
	}

	for (i = 0; i < NUM_GUI_SEQUENCES; i++)
	{
		const input_seq *is1;
		input_seq *is2;
	 	astring *seqstring = astring_alloc();

		is1 = &(GUISequenceControl[i].is);
		is2 = GUISequenceControl[i].getiniptr();
		input_seq_to_tokens(seqstring, is1);
		input_seq_from_tokens(astring_c(seqstring), is2);
		//dprintf("seq =%s is: %4i %4i %4i %4i\n",GUISequenceControl[i].name, (*is1)[0], (*is1)[1], (*is1)[2], (*is1)[3]);
		//dprintf("seq =%s: %s", GUISequenceControl[i].name, astring_c(seqstring));
		astring_free(seqstring);
	}

	if (GetJoyGUI() == TRUE)
	{
		g_pJoyGUI = &DIJoystick;
		if (g_pJoyGUI->init() != 0)
			g_pJoyGUI = NULL;
		else
			SetTimer(hMain, JOYGUI_TIMER, JOYGUI_MS, NULL);
	}
	else
		g_pJoyGUI = NULL;

	ChangeLanguage(0);
#ifdef IMAGE_MENU
	ChangeMenuStyle(0);
#endif /* IMAGE_MENU */

	if (GetHideMouseOnStartup())
	{
		/*  For some reason the mouse is centered when a game is exited, which of
			course causes a WM_MOUSEMOVE event that shows the mouse. So we center
			it now, before the startup coords are initilized, and that way the mouse
			will still be hidden when exiting from a game (i hope) :)
		*/
		SetCursorPos(GetSystemMetrics(SM_CXSCREEN)/2, GetSystemMetrics(SM_CYSCREEN)/2);

		// Then hide it
		ShowCursor(FALSE);
	}

	dprintf("about to show window");

	nCmdShow = GetWindowState();
	if (nCmdShow == SW_HIDE || nCmdShow == SW_MINIMIZE || nCmdShow == SW_SHOWMINIMIZED)
	{
		nCmdShow = SW_RESTORE;
	}

	if (GetRunFullScreen())
	{ 
		LONG lMainStyle;

		// Remove menu
		SetMenu(hMain,NULL); 

		// Frameless dialog (fake fullscreen)
		lMainStyle = GetWindowLong(hMain, GWL_STYLE);
		lMainStyle = lMainStyle & (WS_BORDER ^ 0xffffffff);
		SetWindowLong(hMain, GWL_STYLE, lMainStyle);

		nCmdShow = SW_MAXIMIZE;
	}

#ifdef USE_SHOW_SPLASH_SCREEN
	// Destroy splash screen window
	if (GetDisplaySplashScreen() != FALSE)
		DestroyBackgroundMain();
#endif /* USE_SHOW_SPLASH_SCREEN */

	ShowWindow(hMain, nCmdShow);

#ifdef KSERVER
 	if(GetAutoRun())
	{
	char cCommandLine[32];

	PROCESS_INFORMATION pi;
        STARTUPINFOA si;
	ZeroMemory( &si, sizeof(si) );
	ZeroMemory( &pi, sizeof(pi) );
        si.cb=sizeof(si);
	if(GetShowConsole())si.wShowWindow=SW_SHOW;
	else si.wShowWindow=SW_HIDE;
        si.dwFlags=STARTF_USESHOWWINDOW;
	strcpy(cCommandLine, "kaillera\\kaillerasrv.exe");

        	if (!CreateProcessA(NULL, cCommandLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS|CREATE_NEW_CONSOLE, NULL, "kaillera", &si, &pi))
		{
			MameMessageBoxW(_UIW(TEXT("Start Kaillera Server Failed!\nPlease make sure \"kaillerasrv.exe\" in your \"kaillera\" directory.")));
		}
		else
		{
			m_hPro=pi.hProcess;//Save Current Handle, will be used for Terminate this process.
			MameMessageBoxI(_UIW(TEXT("Start Kaillera Server Succeeded!")));
		}

	}
#endif /* KSERVER */

	switch (GetViewMode())
	{
	case VIEW_LARGE_ICONS :
		SetView(ID_VIEW_LARGE_ICON);
		break;
	case VIEW_SMALL_ICONS :
		SetView(ID_VIEW_SMALL_ICON);
		break;
	case VIEW_INLIST :
		SetView(ID_VIEW_LIST_MENU);
		break;
	case VIEW_REPORT :
		SetView(ID_VIEW_DETAIL);
		break;
	case VIEW_GROUPED :
	default :
		SetView(ID_VIEW_GROUPED);
		break;
	}

	if (GetCycleScreenshot() > 0)
	{
		SetTimer(hMain, SCREENSHOT_TIMER, GetCycleScreenshot()*1000, NULL); //scale to Seconds
	}

#if !defined(KAILLERA) && !defined(MAME32PLUSPLUS)
#ifdef MAME_DEBUG
	if (mame_validitychecks(NULL))
	{
		MessageBoxA(hMain, MAMENAME " has failed its validity checks.  The GUI will "
			"still work, but emulations will fail to execute", MAMENAME, MB_OK);
	}
#endif // MAME_DEBUG
#endif

	return TRUE;
}

static void Win32UI_exit(void)
{
	DragAcceptFiles(hMain, FALSE);

	datafile_exit();

	if (g_bDoBroadcast == TRUE)
	{
        ATOM a = GlobalAddAtomA("");
		SendMessage(HWND_BROADCAST, g_mame32_message, a, a);
		GlobalDeleteAtom(a);
	}

	if (g_pJoyGUI != NULL)
		g_pJoyGUI->exit();

	/* Free GDI resources */

	if (hMissing_bitmap)
	{
		DeleteObject(hMissing_bitmap);
		hMissing_bitmap = NULL;
	}

	if (hBackground)
	{
		DeleteObject(hBackground);
		hBackground = NULL;
	}
	
	if (hPALbg)
	{
		DeleteObject(hPALbg);
		hPALbg = NULL;
	}
	
	if (hFont)
	{
		DeleteObject(hFont);
		hFont = NULL;
	}
	
	DestroyIcons();

	DestroyAcceleratorTable(hAccel);

	if (icon_index != NULL)
	{
		free(icon_index);
		icon_index = NULL;
	}

	free(keyboard_state);
	keyboard_state = NULL;

	DirectInputClose();
	DirectDraw_Close();

	SetSavedFolderPath(GetCurrentFolder()->m_lpPath);

	SaveOptions();

	FreeFolders();

	/* DestroyTree(hTreeView); */

	FreeScreenShot();

	OptionsExit();

	HelpExit();

	if (sorted_drivers != NULL)
	{
		free(sorted_drivers);
		sorted_drivers = NULL;
	}

	free(sort_index);

	free(driversw);

#ifdef DRIVER_SWITCH
	free(drivers);
#endif /* DRIVER_SWITCH */

	end_resource_tracking();
	exit_resource_tracking();
}

static long WINAPI MameWindowProc(HWND hWnd, UINT message, UINT wParam, LONG lParam)
{
	MINMAXINFO	*mminfo;
#if MULTISESSION
	int nGame;
	HWND hGameWnd;
	long lGameWndStyle;
#endif // MULTISESSION


	int 		i;
	char		szClass[128];
	
#ifdef USE_IPS
	static WCHAR patch_name[MAX_PATCHNAME];
#endif /* USE_IPS */

	switch (message)
	{
	case WM_CTLCOLORSTATIC:
		if (hBackground && (HWND)lParam == GetDlgItem(hMain, IDC_HISTORY))
		{
			static HBRUSH hBrush=0;
			HDC hDC=(HDC)wParam;
			LOGBRUSH lb;

			if (hBrush)
				DeleteObject(hBrush);

			if (hBackground)	// Always true?
			{
				lb.lbStyle  = BS_HOLLOW;
				hBrush = CreateBrushIndirect(&lb);
				SetBkMode(hDC, TRANSPARENT);
			}
			else
			{
				hBrush = GetSysColorBrush(COLOR_BTNFACE);
				SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
				SetBkMode(hDC, OPAQUE);
			}
			SetTextColor(hDC, GetListFontColor());
			return (LRESULT) hBrush;
		}
		break;

	case WM_INITDIALOG:
		TranslateDialog(hWnd, lParam, FALSE);

		/* Initialize info for resizing subitems */
		GetClientRect(hWnd, &main_resize.rect);
		return TRUE;

	case WM_SETFOCUS:
		SetFocus(hwndList);
		break;

	case WM_SETTINGCHANGE:
		AdjustMetrics();
		return 0;

	case WM_SIZE:
		OnSize(hWnd, wParam, LOWORD(lParam), HIWORD(wParam));
		return TRUE;

	case WM_MENUSELECT:
#ifdef USE_IPS
		//menu closed, do not UpdateScreenShot() for EditControl scrolling
		if ((int)(HIWORD(wParam)) == 0xFFFF)
		{
			FreeIfAllocatedW(&g_IPSMenuSelectName);
			dprintf("menusele: clear");
			return 0;
		}

		i = (int)(LOWORD(wParam)) - ID_PLAY_PATCH;
		if (i >= 0 && i < MAX_PATCHES && GetPatchFilename(patch_name, driversw[Picker_GetSelectedItem(hwndList)]->name, i))
		{
			FreeIfAllocatedW(&g_IPSMenuSelectName);
			g_IPSMenuSelectName = _wcsdup(patch_name);
			dwprintf(TEXT("menusele: %d %s, updateSS"), (int)(LOWORD(wParam)), patch_name);
			UpdateScreenShot();
		}
		else if (g_IPSMenuSelectName)
		{
			FreeIfAllocatedW(&g_IPSMenuSelectName);
			dwprintf(TEXT("menusele:none, updateSS"));
			UpdateScreenShot();
		}
#endif /* USE_IPS */

		return Statusbar_MenuSelect(hWnd, wParam, lParam);

	case MM_PLAY_GAME:
		MamePlayGame();
		return TRUE;

	case WM_INITMENUPOPUP:
		UpdateMenu(GetMenu(hWnd));
#ifdef IMAGE_MENU
		ApplyMenuStyle(hInst, hWnd, GetMenu(hWnd));
#endif /* IMAGE_MENU */
		break;

	case WM_CONTEXTMENU:
		if (HandleTreeContextMenu(hWnd, wParam, lParam)
		 || HandleScreenShotContextMenu(hWnd, wParam, lParam))
			return FALSE;
		break;

	case WM_COMMAND:
		return MameCommand(hWnd,(int)(LOWORD(wParam)),(HWND)(lParam),(UINT)HIWORD(wParam));

	case WM_GETMINMAXINFO:
		/* Don't let the window get too small; it can break resizing */
		mminfo = (MINMAXINFO *) lParam;
		mminfo->ptMinTrackSize.x = MIN_WIDTH;
		mminfo->ptMinTrackSize.y = MIN_HEIGHT;
		return 0;

	case WM_TIMER:
		switch (wParam)
		{
		case JOYGUI_TIMER:
			PollGUIJoystick();
			break;
		case SCREENSHOT_TIMER:
			TabView_CalculateNextTab(hTabCtrl);
			UpdateScreenShot();
			TabView_UpdateSelection(hTabCtrl);
			break;
#if MULTISESSION
		case GAMEWND_TIMER:
			nGame = Picker_GetSelectedItem(hwndList);
			if( ! GetGameCaption() )
			{
				hGameWnd = GetGameWindow();
				if( hGameWnd )
				{
					lGameWndStyle = GetWindowLong(hGameWnd, GWL_STYLE);
					lGameWndStyle = lGameWndStyle & (WS_BORDER ^ 0xffffffff);
					SetWindowLong(hGameWnd, GWL_STYLE, lGameWndStyle);
					SetWindowPos(hGameWnd,0,0,0,0,0,SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
				}
			}
			if( SendIconToEmulationWindow(nGame)== TRUE);
				KillTimer(hMain, GAMEWND_TIMER);
#endif // MULTISESSION
			break;
		default:
			break;
		}
		return TRUE;

	case WM_CLOSE:
		{
			/* save current item */
			RECT rect;
			AREA area;
			int nItem;
			WINDOWPLACEMENT wndpl;
			UINT state;

#ifdef KAILLERA
			if (bKailleraNetPlay == TRUE)
			{
				{
					MameMessageBoxW(_UIW(TEXT("Close KailleraClient Window")));
					return TRUE;
				}
			}

#endif /* KAILLERA */

#ifdef KSERVER
 		if(m_hPro&&GetAutoClose())
        	{
            	if(!TerminateProcess(m_hPro,0)) //Terminate code is 0
					MameMessageBoxW(_UIW(TEXT("Terminate Kaillera Server ERROR!!!")));
            	else
                    MameMessageBoxI(_UIW(TEXT("Kaillera Server Terminated!")));
                m_hPro=NULL;
        	}
#endif /* KSERVER */

			wndpl.length = sizeof(WINDOWPLACEMENT);
			GetWindowPlacement(hMain, &wndpl);
			state = wndpl.showCmd;

			/* Restore the window before we attempt to save parameters,
			 * This fixed the lost window on startup problem, among other problems
			 */
			if (state == SW_MINIMIZE || state == SW_SHOWMINIMIZED || state == SW_MAXIMIZE)
			{
				if( wndpl.flags & WPF_RESTORETOMAXIMIZED || state == SW_MAXIMIZE)
					state = SW_MAXIMIZE;
				else
					state = SW_RESTORE;
			}
			ShowWindow(hWnd, SW_RESTORE);
			for (i = 0; i < GetSplitterCount(); i++)
				SetSplitterPos(i, nSplitterOffset[i]);
			SetWindowState(state);

			Picker_SaveColumnWidths(hwndList);
#ifdef MESS
			Picker_SaveColumnWidths(GetDlgItem(hMain, IDC_SWLIST));
#endif /* MESS */

			GetWindowRect(hWnd, &rect);
			area.x		= rect.left;
			area.y		= rect.top;
			area.width	= rect.right  - rect.left;
			area.height = rect.bottom - rect.top;
			SetWindowArea(&area);

			/* Save the users current game options and default game */
			nItem = Picker_GetSelectedItem(hwndList);
			SetDefaultGame(drivers[nItem]->name);

			/* hide window to prevent orphan empty rectangles on the taskbar */
			/* ShowWindow(hWnd,SW_HIDE); */
			DestroyWindow( hWnd );

			/* PostQuitMessage(0); */
			break;
		}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_LBUTTONDOWN:
		OnLButtonDown(hWnd, (UINT)wParam, MAKEPOINTS(lParam));
		break;

		/*
		  Check to see if the mouse has been moved by the user since
		  startup. I'd like this checking to be done only in the
		  main WinProc (here), but somehow the WM_MOUSEDOWN messages
		  are eaten up before they reach MameWindowProc. That's why
		  there is one check for each of the subclassed windows too.
    
		  POSSIBLE BUGS:
		  I've included this check in the subclassed windows, but a 
		  mose move in either the title bar, the menu, or the
		  toolbar will not generate a WM_MOUSEOVER message. At least
		  not one that I know how to pick up. A solution could maybe
		  be to subclass those too, but that's too much work :)
		*/

	case WM_MOUSEMOVE:
	{
		if (MouseHasBeenMoved())
			ShowCursor(TRUE);

		if (g_listview_dragging)
			MouseMoveListViewDrag(MAKEPOINTS(lParam));
		else
			/* for splitters */
			OnMouseMove(hWnd, (UINT)wParam, MAKEPOINTS(lParam));
		break;
	}

	case WM_LBUTTONUP:
		if (g_listview_dragging)
			ButtonUpListViewDrag(MAKEPOINTS(lParam));
		else
			/* for splitters */
			OnLButtonUp(hWnd, (UINT)wParam, MAKEPOINTS(lParam));
		break;

	case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP)wParam;
			WCHAR fileName[MAX_PATH];
			WCHAR ext[MAX_PATH];

			if (OnNT())
				DragQueryFileW(hDrop, 0, fileName, MAX_PATH);
			else
			{
				char fileNameA[MAX_PATH];
				DragQueryFileA(hDrop, 0, fileNameA, MAX_PATH);
				wcscpy(fileName, _Unicode(fileNameA));
			}
			DragFinish(hDrop);

			_wsplitpath(fileName, NULL, NULL, NULL, ext);

			DragAcceptFiles(hMain, FALSE);
			SetForegroundWindow(hMain);
			if (!_wcsicmp(ext, TEXT(".sta")))
				MameLoadState(fileName);
			else
				MamePlayBackGame(fileName);
			DragAcceptFiles(hMain, TRUE);

		}
		break;

	case WM_NOTIFY:
		/* Where is this message intended to go */
		{
			LPNMHDR lpNmHdr = (LPNMHDR)lParam;

			/* Fetch tooltip text */
			if (lpNmHdr->code == TTN_NEEDTEXTW)
			{
				LPTOOLTIPTEXTW lpttt = (LPTOOLTIPTEXTW)lParam;
				CopyToolTipTextW(lpttt);
			}
			if (lpNmHdr->code == TTN_NEEDTEXTA)
			{
				LPTOOLTIPTEXTA lpttt = (LPTOOLTIPTEXTA)lParam;
				CopyToolTipTextA(lpttt);
			}

			if (lpNmHdr->hwndFrom == hTreeView)
				return TreeViewNotify(lpNmHdr);

			GetClassNameA(lpNmHdr->hwndFrom, szClass, ARRAY_LENGTH(szClass));
			if (!strcmp(szClass, "SysListView32"))
				return Picker_HandleNotify(lpNmHdr);	
			if (!strcmp(szClass, "SysTabControl32"))
				return TabView_HandleNotify(lpNmHdr);
		}
		break;

	case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT lpDis = (LPDRAWITEMSTRUCT)lParam;

			GetClassNameA(lpDis->hwndItem, szClass, ARRAY_LENGTH(szClass));
			if (!strcmp(szClass, "SysListView32"))
				Picker_HandleDrawItem(GetDlgItem(hMain, lpDis->CtlID), lpDis);
		}
		break;

	case WM_MEASUREITEM :
	{
		if (wParam) // the message was NOT sent by a menu
		{
		    LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT) lParam;

		    // tell the list view that each row (item) should be just taller than our font
    		    //DefWindowProc(hWnd, message, wParam, lParam);
		    //dprintf("default row height calculation gives %u\n",lpmis->itemHeight);

		    TEXTMETRIC tm;
		    HDC hDC = GetDC(NULL);
		    HFONT hFontOld = (HFONT)SelectObject(hDC,hFont);

		    GetTextMetrics(hDC,&tm);

		    lpmis->itemHeight = tm.tmHeight + tm.tmExternalLeading + 1;
		    if (lpmis->itemHeight < 17)
			    lpmis->itemHeight = 17;
		    //dprintf("we would do %u\n",tm.tmHeight + tm.tmExternalLeading + 1);
		    SelectObject(hDC,hFontOld);
		    ReleaseDC(NULL,hDC);

		    return TRUE;
		}
		else
			return FALSE;
	}
	default:

		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

static int HandleKeyboardGUIMessage(HWND hWnd, UINT message, UINT wParam, LONG lParam)
{
	switch (message)
	{
		case WM_CHAR: /* List-View controls use this message for searching the items "as user types" */
			//MessageBox(NULL,"wm_char message arrived","TitleBox",MB_OK);
			return TRUE;

		case WM_KEYDOWN:
			KeyboardKeyDown(0, wParam, lParam);
			return TRUE;

		case WM_KEYUP:
			KeyboardKeyUp(0, wParam, lParam);
			return TRUE;

		case WM_SYSKEYDOWN:
			KeyboardKeyDown(1, wParam, lParam);
			return TRUE;

		case WM_SYSKEYUP:
			KeyboardKeyUp(1, wParam, lParam);
			return TRUE;
	}

	return FALSE;	/* message not processed */
}

static BOOL PumpMessage(void)
{
	MSG msg;

	if (!GetMessage(&msg, NULL, 0, 0))
	{
		return FALSE;
	}

	if (IsWindow(hMain))
	{
		BOOL absorbed_key = FALSE;
		if (GetKeyGUI())
			absorbed_key = HandleKeyboardGUIMessage(msg.hwnd, msg.message, 
			                                        msg.wParam, msg.lParam);
		else
			absorbed_key = TranslateAccelerator(hMain, hAccel, &msg);

		if (!absorbed_key)
		{
			if (!IsDialogMessage(hMain, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	return TRUE;
}

static BOOL FolderCheck(void)
{
	int nGameIndex = 0;
	int i=0;
	int iStep = 0;
	LV_FINDINFO lvfi;
	int nCount = ListView_GetItemCount(hwndList);
	BOOL changed = FALSE;

	MSG msg;
	for(i=0; i<nCount;i++)
	{
		LV_ITEM lvi;

		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.mask	 = LVIF_PARAM;
		ListView_GetItem(hwndList, &lvi);
		nGameIndex  = lvi.lParam;
		SetRomAuditResults(nGameIndex, UNKNOWN);
		SetSampleAuditResults(nGameIndex, UNKNOWN);
	}
	if( nCount > 0)
		ProgressBarShow();
	else
		return FALSE;
	if( nCount < 100 )
		iStep = 100 / nCount;
	else
		iStep = nCount/100;
	UpdateListView();
	UpdateWindow(hMain);
	for(i=0; i<nCount;i++)
	{
		LV_ITEM lvi;

		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.mask	 = LVIF_PARAM;
		ListView_GetItem(hwndList, &lvi);
		nGameIndex  = lvi.lParam;
		if (GetRomAuditResults(nGameIndex) == UNKNOWN)
		{
			Mame32VerifyRomSet(nGameIndex, FALSE);
			changed = TRUE;
		}

		if (GetSampleAuditResults(nGameIndex) == UNKNOWN)
		{
			Mame32VerifySampleSet(nGameIndex, FALSE);
			changed = TRUE;
		}

		lvfi.flags	= LVFI_PARAM;
		lvfi.lParam = nGameIndex;

		i = ListView_FindItem(hwndList, -1, &lvfi);
		if (changed && i != -1);
		{
			ListView_RedrawItems(hwndList, i, i);
			while( PeekMessage( &msg, hwndList, 0, 0, PM_REMOVE ) != 0)
			{
				TranslateMessage(&msg); 
				DispatchMessage(&msg); 
			}
		}
		changed = FALSE;
		if ((i % iStep) == 0)
			ProgressBarStepParam(i, nCount);
	}
	ProgressBarHide();
	SetStatusBarTextW(0, UseLangList() ? _LSTW(driversw[Picker_GetSelectedItem(hwndList)]->description) : driversw[Picker_GetSelectedItem(hwndList)]->modify_the);
	UpdateStatusBar();
	return TRUE;
}

static BOOL GameCheck(void)
{
	LV_FINDINFO lvfi;
	int i;
	BOOL changed = FALSE;

	if (game_index == 0)
		ProgressBarShow();

	if (game_index >= game_count)
	{
		bDoGameCheck = FALSE;
		ProgressBarHide();
		ResetWhichGamesInFolders();
		return FALSE;
	}

	if (GetRomAuditResults(game_index) == UNKNOWN)
	{
		Mame32VerifyRomSet(game_index, FALSE);
		changed = TRUE;
	}

	if (GetSampleAuditResults(game_index) == UNKNOWN)
	{
		Mame32VerifySampleSet(game_index, FALSE);
		changed = TRUE;
	}

	lvfi.flags	= LVFI_PARAM;
	lvfi.lParam = game_index;

	i = ListView_FindItem(hwndList, -1, &lvfi);
	if (changed && i != -1);
		ListView_RedrawItems(hwndList, i, i);
	if ((game_index % progBarStep) == 0)
		ProgressBarStep();
	game_index++;

	return changed;
}

static BOOL OnIdle(HWND hWnd)
{
	static int bFirstTime = TRUE;
	static int bResetList = TRUE;

	int driver_index;

	if (bFirstTime)
	{
		bResetList = FALSE;
		bFirstTime = FALSE;
	}
	if (bDoGameCheck)
	{
		if (GameCheck())
		{
			/* we only reset the View if "available" is the selected folder
			  as it doesn't affect the others*/
			LPTREEFOLDER folder = GetSelectedFolder();

			if (folder && folder->m_nFolderId == FOLDER_AVAILABLE)
				bResetList = TRUE;
		}

		return idle_work;
	}
	// NPW 17-Jun-2003 - Commenting this out because it is redundant
	// and it causes the game to reset back to the original game after an F5 
	// refresh
	//driver_index = GetGameNameIndex(GetDefaultGame());
	//SetSelectedPickItem(driver_index);

	// in case it's not found, get it back
	driver_index = Picker_GetSelectedItem(hwndList);

	SetStatusBarTextW(0, UseLangList() ? _LSTW(driversw[driver_index]->description) : driversw[driver_index]->modify_the);
	if (bResetList || (GetViewMode() == VIEW_LARGE_ICONS))
	{
		ResetWhichGamesInFolders();
		ResetListView();
	}
	idle_work = FALSE;
	UpdateStatusBar();
	bFirstTime = TRUE;

	if (!idle_work)
		PostMessage(GetMainWindow(),WM_COMMAND, MAKEWPARAM(ID_VIEW_LINEUPICONS, TRUE),(LPARAM)NULL);
	return idle_work;
}

static void OnSize(HWND hWnd, UINT nState, int nWidth, int nHeight)
{
	static BOOL firstTime = TRUE;

	if (nState != SIZE_MAXIMIZED && nState != SIZE_RESTORED)
		return;

	ResizeWindow(hWnd, &main_resize);
	ResizeProgressBar();
	if (firstTime == FALSE)
		OnSizeSplitter(hMain);
	//firstTime = FALSE;
	/* Update the splitters structures as appropriate */
	RecalcSplitters();
	if (firstTime == FALSE)
		ResizePickerControls(hMain);
	firstTime = FALSE;
	UpdateScreenShot();
}



static HWND GetResizeItemWindow(HWND hParent, const ResizeItem *ri)
{
	HWND hControl;
	if (ri->type == RA_ID)
		hControl = GetDlgItem(hParent, ri->u.id);
	else
		hControl = ri->u.hwnd;
	return hControl;
}



static void SetAllWindowsFont(HWND hParent, const Resize *r, HFONT hTheFont, BOOL bRedraw)
{
	int i;
	HWND hControl;

	for (i = 0; r->items[i].type != RA_END; i++)
	{
		if (r->items[i].setfont)
		{
			hControl = GetResizeItemWindow(hParent, &r->items[i]);
			SetWindowFont(hControl, hTheFont, bRedraw);
		}
	}

	hControl = GetDlgItem(hwndList, 0);
	if (hControl)
		TranslateControl(hControl);
}



static void ResizeWindow(HWND hParent, Resize *r)
{
	int cmkindex = 0, dx, dy, dx1, dtempx;
	HWND hControl;
	RECT parent_rect, rect;
	ResizeItem *ri;
	POINT p = {0, 0};

	if (hParent == NULL)
		return;

	/* Calculate change in width and height of parent window */
	GetClientRect(hParent, &parent_rect);
	//dx = parent_rect.right - r->rect.right;
	dtempx = parent_rect.right - r->rect.right;
	dy = parent_rect.bottom - r->rect.bottom;
	dx = dtempx/2;
	dx1 = dtempx - dx;
	ClientToScreen(hParent, &p);

	while (r->items[cmkindex].type != RA_END)
	{
		ri = &r->items[cmkindex];
		if (ri->type == RA_ID)
			hControl = GetDlgItem(hParent, ri->u.id);
		else
			hControl = ri->u.hwnd;

		if (hControl == NULL)
		{
			cmkindex++;
			continue;
		}

		/* Get control's rectangle relative to parent */
		GetWindowRect(hControl, &rect);
		OffsetRect(&rect, -p.x, -p.y);

		if (!(ri->action & RA_LEFT))
			rect.left += dx;

		if (!(ri->action & RA_TOP))
			rect.top += dy;

		if (ri->action & RA_RIGHT)
			rect.right += dx;

		if (ri->action & RA_BOTTOM)
			rect.bottom += dy;

		MoveWindow(hControl, rect.left, rect.top,
		           (rect.right - rect.left),
		           (rect.bottom - rect.top), TRUE);

		/* Take care of subcontrols, if appropriate */
		if (ri->subwindow != NULL)
			ResizeWindow(hControl, ri->subwindow);

		cmkindex++;
	}

	/* Record parent window's new location */
	memcpy(&r->rect, &parent_rect, sizeof(RECT));
}

static void ProgressBarShow(void)
{
	RECT rect;
	int  widths[2] = {150, -1};

	if (game_count < 100)
		progBarStep = 100 / game_count;
	else
		progBarStep = (game_count / 100);

	SendMessage(hStatusBar, SB_SETPARTS, (WPARAM)2, (LPARAM)(LPINT)widths);
	SendMessage(hProgWnd, PBM_SETRANGE, 0, (LPARAM)MAKELONG(0, game_count));
	SendMessage(hProgWnd, PBM_SETSTEP, (WPARAM)progBarStep, 0);
	SendMessage(hProgWnd, PBM_SETPOS, 0, 0);

	StatusBar_GetItemRect(hStatusBar, 1, &rect);

	MoveWindow(hProgWnd, rect.left, rect.top,
	           rect.right - rect.left,
	           rect.bottom - rect.top, TRUE);

	bProgressShown = TRUE;
}

static void ProgressBarHide(void)
{
	RECT rect;
	int  widths[4];
	HDC  hDC;
	SIZE size;
	int  numParts = 4;

	if (hProgWnd == NULL)
	{
		return;
	}

	hDC = GetDC(hProgWnd);

	ShowWindow(hProgWnd, SW_HIDE);

	GetTextExtentPoint32A(hDC, "MMX", 3 , &size);
	widths[3] = size.cx;
	GetTextExtentPoint32A(hDC, "MMMM games", 10, &size);
	widths[2] = size.cx;
	//Just specify 24 instead of 30, gives us sufficient space to display the message, and saves some space
	GetTextExtentPoint32(hDC, TEXT("Screen flip support is missing"), 24, &size);
	widths[1] = size.cx;

	ReleaseDC(hProgWnd, hDC);

	widths[0] = -1;
	SendMessage(hStatusBar, SB_SETPARTS, (WPARAM)1, (LPARAM)(LPINT)widths);
	StatusBar_GetItemRect(hStatusBar, 0, &rect);

	widths[0] = (rect.right - rect.left) - (widths[1] + widths[2] + widths[3]);
	widths[1] += widths[0];
	widths[2] += widths[1];
	widths[3] = -1;

	SendMessage(hStatusBar, SB_SETPARTS, (WPARAM)numParts, (LPARAM)(LPINT)widths);
	UpdateStatusBar();

	bProgressShown = FALSE;
}

static void ResizeProgressBar(void)
{
	if (bProgressShown)
	{
		RECT rect;
		int  widths[2] = {150, -1};

		SendMessage(hStatusBar, SB_SETPARTS, (WPARAM)2, (LPARAM)(LPINT)widths);
		StatusBar_GetItemRect(hStatusBar, 1, &rect);
		MoveWindow(hProgWnd, rect.left, rect.top,
		           rect.right  - rect.left,
		           rect.bottom - rect.top, TRUE);
	}
	else
	{
		ProgressBarHide();
	}
}

static void ProgressBarStepParam(int iGameIndex, int nGameCount)
{
	SetStatusBarTextFW(0, _UIW(TEXT("Game search %d%% complete")),
	                  ((iGameIndex + 1) * 100) / nGameCount);
	if (iGameIndex == 0)
		ShowWindow(hProgWnd, SW_SHOW);
	SendMessage(hProgWnd, PBM_STEPIT, 0, 0);
}

static void ProgressBarStep(void)
{
	ProgressBarStepParam(game_index, game_count);
}

static HWND InitProgressBar(HWND hParent)
{
	RECT rect;

	StatusBar_GetItemRect(hStatusBar, 0, &rect);

	rect.left += 150;

	return CreateWindowEx(WS_EX_STATICEDGE,
			PROGRESS_CLASS,
			TEXT("Progress Bar"),
			WS_CHILD | PBS_SMOOTH,
			rect.left,
			rect.top,
			rect.right	- rect.left,
			rect.bottom - rect.top,
			hParent,
			NULL,
			hInst,
			NULL);
}

static void CopyToolTipTextW(LPTOOLTIPTEXTW lpttt)
{
	int   i;
	int   id = lpttt->hdr.idFrom;
	int   iButton = lpttt->hdr.idFrom;
	static WCHAR String[1024];
	BOOL bConverted = FALSE;
	//LPWSTR pDest = lpttt->lpszText;
	/* Map command ID to string index */
	for (i = 0; CommandToString[i] != -1; i++)
	{
		if (CommandToString[i] == id)
		{
			iButton = i;
			bConverted = TRUE;
			break;
		}
	}
	if( bConverted )
	{
		/* Check for valid parameter */
		if (iButton > NUM_TOOLTIPS)
		{
			wcscpy(String, _UIW(TEXT("Invalid Button Index")));
		}
		else
		{
			wcscpy(String, (id==IDC_USE_LIST && GetLangcode()==UI_LANG_EN_US) ?
			       _UIW(TEXT("Modify 'The'")) : _UIW(szTbStrings[iButton]));
		}
	}
	else if (iButton <= 2 )
	{
		//Statusbar
		SendMessage(lpttt->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 200);
		if (iButton != 1)
			SendMessage(hStatusBar, SB_GETTEXTW, (WPARAM)iButton, (LPARAM) &String);
		else
			//for first pane we get the Status directly, to get the line breaks
			wcscpy(String, GameInfoStatus(Picker_GetSelectedItem(hwndList), FALSE));
	}
	else
		wcscpy(String, _UIW(TEXT("Invalid Button Index")));

	wcscpy(lpttt->lpszText, String);
}

static void CopyToolTipTextA(LPTOOLTIPTEXTA lpttt)
{
	int   i;
	int   id = lpttt->hdr.idFrom;
	int   iButton = lpttt->hdr.idFrom;
	static char String[1024];
	BOOL bConverted = FALSE;
	//LPSTR pDest = lpttt->lpszText;
	/* Map command ID to string index */
	for (i = 0; CommandToString[i] != -1; i++)
	{
		if (CommandToString[i] == id)
		{
			iButton = i;
			bConverted = TRUE;
			break;
		}
	}

	if( bConverted )
	{
		/* Check for valid parameter */
		if (iButton > NUM_TOOLTIPS)
		{
			strcpy(String, _String(_UIW(TEXT("Invalid Button Index"))));
		}
		else
		{
			strcpy(String, (id==IDC_USE_LIST && GetLangcode()==UI_LANG_EN_US) ?
			       "Modify 'The'" : _String(_UIW(szTbStrings[iButton])));
		}
	}
	else if (iButton <= 2 )
	{
		//Statusbar
		SendMessage(lpttt->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 140);
		if (iButton != 1)
			SendMessage(hStatusBar, SB_GETTEXTA, (WPARAM)iButton, (LPARAM)(LPSTR) &String);
		else
			//for first pane we get the Status directly, to get the line breaks
			strcpy(String, _String(GameInfoStatus(Picker_GetSelectedItem(hwndList), FALSE)));
	}
	else
		strcpy(String, _String(_UIW(TEXT("Invalid Button Index"))));

	strcpy(lpttt->lpszText, String);
}

static HWND InitToolbar(HWND hParent)
{
	HWND hToolBar = CreateToolbarEx(hParent,
	                       WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
	                       CCS_TOP | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
	                       1,
#if defined(KAILLERA) || defined(MAME32PLUSPLUS)
	                       NUM_TOOLTIPS - 1,
#else
	                       8,
#endif
	                       hInst,
	                       IDB_TOOLBAR_US + GetLangcode(),
	                       tbb,
	                       NUM_TOOLBUTTONS,
	                       16,
	                       16,
	                       0,
	                       0,
	                       sizeof(TBBUTTON));

	RECT rect;
	int idx;
	int iPosX, iPosY, iHeight;

	// get Edit Control position
	idx = SendMessage(hToolBar, TB_BUTTONCOUNT, (WPARAM)0, (LPARAM)0) - 1;
	SendMessage(hToolBar, TB_GETITEMRECT, (WPARAM)idx, (LPARAM)&rect);
	iPosX = rect.right + 10;
	iPosY = rect.top + 1;
	iHeight = rect.bottom - rect.top - 2;

	// create Edit Control
	CreateWindowEx( 0L, TEXT("Edit"), _UIW(TEXT(SEARCH_PROMPT)), WS_CHILD | WS_BORDER | WS_VISIBLE | ES_LEFT, 
					iPosX, iPosY, 200, iHeight, hToolBar, (HMENU)ID_TOOLBAR_EDIT, hInst, NULL );

	return hToolBar;
}

static HWND InitStatusBar(HWND hParent)
{
#if 0
	HMENU hMenu = GetMenu(hParent);

	popstr[0].hMenu    = 0;
	popstr[0].uiString = 0;
	popstr[1].hMenu    = hMenu;
	popstr[1].uiString = IDS_UI_FILE;
	popstr[2].hMenu    = GetSubMenu(hMenu, 1);
	popstr[2].uiString = IDS_UI_TOOLBAR;
	popstr[3].hMenu    = 0;
	popstr[3].uiString = 0;
#endif

	return CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
	                          CCS_BOTTOM | SBARS_SIZEGRIP | SBT_TOOLTIPS,
	                          _UIW(TEXT("Ready")),
	                          hParent,
	                          2);
}


static LRESULT Statusbar_MenuSelect(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
#if 0
	UINT  fuFlags	= (UINT)HIWORD(wParam);
	HMENU hMainMenu = NULL;
	int   iMenu 	= 0;

	/* Handle non-system popup menu descriptions. */
	if (  (fuFlags & MF_POPUP)
	&&	(!(fuFlags & MF_SYSMENU)))
	{
		for (iMenu = 1; iMenu < MAX_MENUS; iMenu++)
		{
			if ((HMENU)lParam == popstr[iMenu].hMenu)
			{
				hMainMenu = (HMENU)lParam;
				break ;
			}
		}
	}

	if (hMainMenu)
	{
		/* Display helpful text in status bar */
		MenuHelp(WM_MENUSELECT, wParam, lParam, hMainMenu, hInst,
				 hStatusBar, (UINT *)&popstr[iMenu]);
	}
	else
	{
		UINT nZero = 0;
		MenuHelp(WM_MENUSELECT, wParam, lParam, NULL, hInst,
				 hStatusBar, &nZero);
	}
#else
	WCHAR *p = TranslateMenuHelp((HMENU)lParam, (UINT)LOWORD(wParam), HIWORD(wParam) & MF_POPUP);
	StatusBarSetTextW(hStatusBar, 0, p);
#endif

	return 0;
}

static void UpdateStatusBar(void)
{
	LPTREEFOLDER lpFolder = GetCurrentFolder();
	int 		 games_shown = 0;
	int 		 i = -1;

	if (!lpFolder)
		return;

	while (1)
	{
		i = FindGame(lpFolder,i+1);
		if (i == -1)
			break;

		if (!GameFiltered(i, lpFolder->m_dwFlags))
			games_shown++;
	}

	/* Show number of games in the current 'View' in the status bar */
	SetStatusBarTextFW(2, _UIW(TEXT("%d games")), games_shown);

	i = Picker_GetSelectedItem(hwndList);

	if (games_shown == 0)
		DisableSelection();
	else
		SetStatusBarTextW(1, GameInfoStatus(i, FALSE));
}

static BOOL NeedScreenShotImage(void)
{
#ifdef USE_IPS
	if (g_IPSMenuSelectName)
		return TRUE;
#endif /* USE_IPS */

	if (TabView_GetCurrentTab(hTabCtrl) == TAB_HISTORY && GetShowTab(TAB_HISTORY))
		return FALSE;

#ifdef STORY_DATAFILE
	if (TabView_GetCurrentTab(hTabCtrl) == TAB_STORY && GetShowTab(TAB_STORY))
		return FALSE;
#endif /* STORY_DATAFILE */

	return TRUE;
}

static BOOL NeedHistoryText(void)
{
#ifdef USE_IPS
	if (g_IPSMenuSelectName)
		return TRUE;
#endif /* USE_IPS */

	if (TabView_GetCurrentTab(hTabCtrl) == TAB_HISTORY)
		return TRUE;
	if (GetShowTab(TAB_HISTORY) == FALSE)
	{
		if (TabView_GetCurrentTab(hTabCtrl) == GetHistoryTab())
			return TRUE;
		if (TAB_ALL == GetHistoryTab())
			return TRUE;
	}

#ifdef STORY_DATAFILE
	if (TabView_GetCurrentTab(hTabCtrl) == TAB_STORY)
		return TRUE;
#endif /* STORY_DATAFILE */

	return FALSE;
}

static void UpdateHistory(void)
{
	HDC hDC;
	RECT rect;
	TEXTMETRIC     tm ;
	int nLines, nLineHeight;
	//DWORD dwStyle = GetWindowLong(GetDlgItem(hMain, IDC_HISTORY), GWL_STYLE);
	have_history = FALSE;

	if (GetSelectedPick() >= 0)
	{
		LPCWSTR histText;
		
#ifdef USE_IPS
		if (g_IPSMenuSelectName)
		{
			WCHAR *p = NULL;
			histText = GetPatchDesc(driversw[Picker_GetSelectedItem(hwndList)]->name, g_IPSMenuSelectName);
			if(histText && (p = wcschr(histText, '/')))	// no category
				histText = p + 1;
		}
		else
#endif /* USE_IPS */
#ifdef STORY_DATAFILE
			if (TabView_GetCurrentTab(hTabCtrl) == TAB_STORY)
				histText = GetGameStory(Picker_GetSelectedItem(hwndList));
			else
#endif /* STORY_DATAFILE */
				histText = GetGameHistory(Picker_GetSelectedItem(hwndList));

		if (histText && histText[0])
		{
			have_history = TRUE;
			Edit_SetText(GetDlgItem(hMain, IDC_HISTORY), histText);
		}
		else
		{
			have_history = FALSE;
			Edit_SetText(GetDlgItem(hMain, IDC_HISTORY), TEXT(""));
		}
	}

	if (have_history && GetShowScreenShot() && NeedHistoryText())
	{
		RECT sRect;

		sRect.left = history_rect.left;
		sRect.right = history_rect.right;

		if (!NeedScreenShotImage())
		{
			// We're using the new mode, with the history filling the entire tab (almost)
			sRect.top = history_rect.top + 14;
			sRect.bottom = (history_rect.bottom - history_rect.top) - 30;   
		}
		else
		{
			// We're using the original mode, with the history beneath the SS picture
			sRect.top = history_rect.top + 264;
			sRect.bottom = (history_rect.bottom - history_rect.top) - 278;
		}

		MoveWindow(GetDlgItem(hMain, IDC_HISTORY),
			sRect.left, sRect.top,
			sRect.right, sRect.bottom, TRUE);

		Edit_GetRect(GetDlgItem(hMain, IDC_HISTORY), &rect);
		nLines = Edit_GetLineCount(GetDlgItem(hMain, IDC_HISTORY));
		hDC = GetDC(GetDlgItem(hMain, IDC_HISTORY));
		GetTextMetrics (hDC, &tm);
		nLineHeight = tm.tmHeight - tm.tmInternalLeading;
		if ((rect.bottom - rect.top) / nLineHeight < nLines)
		{
			//more than one Page, so show Scrollbar
			SetScrollRange(GetDlgItem(hMain, IDC_HISTORY), SB_VERT, 0, nLines, TRUE); 
		}
		else
		{
			//hide Scrollbar
			SetScrollRange(GetDlgItem(hMain, IDC_HISTORY),SB_VERT, 0, 0, TRUE);

		}
 		ShowWindow(GetDlgItem(hMain, IDC_HISTORY), SW_SHOW);
	}
	else
		ShowWindow(GetDlgItem(hMain, IDC_HISTORY), SW_HIDE);

}


static void DisableSelection(void)
{
	MENUITEMINFO	mmi;
	HMENU			hMenu = GetMenu(hMain);
	BOOL			prev_have_selection = have_selection;

	mmi.cbSize         = sizeof(mmi);
	mmi.fMask          = MIIM_TYPE;
	mmi.fType          = MFT_STRING;
	mmi.dwTypeData     = _UIW(TEXT("&Play"));
	mmi.cch            = wcslen(mmi.dwTypeData);
	SetMenuItemInfo(hMenu, ID_FILE_PLAY, FALSE, &mmi);

	mmi.cbSize         = sizeof(mmi);
	mmi.fMask          = MIIM_TYPE;
	mmi.fType          = MFT_STRING;
	mmi.dwTypeData     = _UIW(TEXT("Propert&ies for Driver"));
	mmi.cch            = wcslen(mmi.dwTypeData);
	SetMenuItemInfo(hMenu, ID_FOLDER_SOURCEPROPERTIES, FALSE, &mmi);

	mmi.cbSize         = sizeof(mmi);
	mmi.fMask          = MIIM_TYPE;
	mmi.fType          = MFT_STRING;
	mmi.dwTypeData     = _UIW(TEXT("Properties &for BIOS"));
	mmi.cch            = wcslen(mmi.dwTypeData);
	SetMenuItemInfo(hMenu, ID_BIOS_PROPERTIES, FALSE, &mmi);

	EnableMenuItem(hMenu, ID_FILE_PLAY, 		   MF_GRAYED);
	EnableMenuItem(hMenu, ID_FILE_PLAY_RECORD,	   MF_GRAYED);
	EnableMenuItem(hMenu, ID_GAME_PROPERTIES,	   MF_GRAYED);
	EnableMenuItem(hMenu, ID_FOLDER_SOURCEPROPERTIES,  MF_GRAYED);
	EnableMenuItem(hMenu, ID_BIOS_PROPERTIES,	   MF_GRAYED);
#ifdef USE_VIEW_PCBINFO
	EnableMenuItem(hMenu, ID_VIEW_PCBINFO,		   MF_GRAYED);
#endif /* USE_VIEW_PCBINFO */
#ifdef MAME_AVI
    EnableMenuItem(hMenu, ID_FILE_PLAY_BACK_AVI,   MF_GRAYED);
	EnableMenuItem(hMenu, ID_FILE_PLAY_WITH_AVI,   MF_GRAYED);
#endif /* MAME_AVI */
#ifdef KAILLERA
	EnableMenuItem(hMenu, ID_FILE_NETPLAY, 		   MF_GRAYED);
#endif /* KAILLERA */

	SetStatusBarTextW(0, _UIW(TEXT("No Selection")));
	SetStatusBarTextW(1, TEXT(""));
	SetStatusBarTextW(3, TEXT(""));

	have_selection = FALSE;

	if (prev_have_selection != have_selection)
		UpdateScreenShot();
}

static void EnableSelection(int nGame)
{
	WCHAR		buf[200];
	const WCHAR *	pText;
	MENUITEMINFO	mmi;
	HMENU		hMenu = GetMenu(hMain);
	int             bios_driver;
	
	snwprintf(buf, ARRAY_LENGTH(buf), _UIW(TEXT("&Play %s")),
	         ConvertAmpersandString(UseLangList() ?
	                                _LSTW(driversw[nGame]->description) :
	                                driversw[nGame]->modify_the));
	mmi.cbSize         = sizeof(mmi);
	mmi.fMask          = MIIM_TYPE;
	mmi.fType          = MFT_STRING;
	mmi.dwTypeData     = buf;
	mmi.cch            = wcslen(mmi.dwTypeData);
	SetMenuItemInfo(hMenu, ID_FILE_PLAY, FALSE, &mmi);

	snwprintf(buf, ARRAY_LENGTH(buf),
		_UIW(TEXT("Propert&ies for %s")), GetDriverFilename(nGame));
	mmi.cbSize         = sizeof(mmi);
	mmi.fMask          = MIIM_TYPE;
	mmi.fType          = MFT_STRING;
	mmi.dwTypeData     = buf;
	mmi.cch            = wcslen(mmi.dwTypeData);
	SetMenuItemInfo(hMenu, ID_FOLDER_SOURCEPROPERTIES, FALSE, &mmi);

	bios_driver = DriverBiosIndex(nGame);
	if (bios_driver != -1 && bios_driver != nGame)
	{
		snwprintf(buf, ARRAY_LENGTH(buf),
			_UIW(TEXT("Properties &for %s BIOS")), driversw[bios_driver]->name);
		mmi.dwTypeData = buf;
	}
	else
	{
		EnableMenuItem(hMenu, ID_BIOS_PROPERTIES, MF_GRAYED);
		mmi.dwTypeData = _UIW(TEXT("Properties &for BIOS"));
	}

	mmi.cbSize         = sizeof(mmi);
	mmi.fMask          = MIIM_TYPE;
	mmi.fType          = MFT_STRING;
	mmi.cch            = wcslen(mmi.dwTypeData);
	SetMenuItemInfo(hMenu, ID_BIOS_PROPERTIES, FALSE, &mmi);

	pText = UseLangList() ?
		_LSTW(driversw[nGame]->description) :
		driversw[nGame]->modify_the;
	SetStatusBarTextW(0, pText);
	/* Add this game's status to the status bar */
	SetStatusBarTextW(1, GameInfoStatus(nGame, FALSE));
	SetStatusBarTextW(3, TEXT(""));

	/* If doing updating game status and the game name is NOT pacman.... */

	EnableMenuItem(hMenu, ID_FILE_PLAY, 		   MF_ENABLED);
	EnableMenuItem(hMenu, ID_FILE_PLAY_RECORD,	   MF_ENABLED);
#ifdef USE_VIEW_PCBINFO
	EnableMenuItem(hMenu, ID_VIEW_PCBINFO,		   MF_ENABLED);
#endif /* USE_VIEW_PCBINFO */
#ifdef KAILLERA
	if (bKailleraNetPlay == FALSE)
    	EnableMenuItem(hMenu, ID_FILE_NETPLAY, MF_ENABLED);
	else
		EnableMenuItem(hMenu, ID_FILE_NETPLAY, MF_GRAYED);
#endif /* KAILLERA */

	if (!oldControl)
	{
		EnableMenuItem(hMenu, ID_GAME_PROPERTIES,          MF_ENABLED);
		EnableMenuItem(hMenu, ID_FOLDER_SOURCEPROPERTIES,  MF_ENABLED);
		EnableMenuItem(hMenu, ID_BIOS_PROPERTIES, bios_driver != -1 ? MF_ENABLED : MF_GRAYED);
	}

	if (bProgressShown && bListReady == TRUE)
	{
		SetDefaultGame(drivers[nGame]->name);
	}
	have_selection = TRUE;

	UpdateScreenShot();
}

#ifdef USE_VIEW_PCBINFO
void PaintBackgroundImage(HWND hWnd, HRGN hRgn, int x, int y)
#else /* USE_VIEW_PCBINFO */
static void PaintBackgroundImage(HWND hWnd, HRGN hRgn, int x, int y)
#endif /* USE_VIEW_PCBINFO */
{
	RECT		rcClient;
	HRGN		rgnBitmap;
	HPALETTE	hPAL;
	HDC 		hDC = GetDC(hWnd);
	int 		i, j;
	HDC 		htempDC;
	HBITMAP 	oldBitmap;

	/* x and y are offsets within the background image that should be at 0,0 in hWnd */

	/* So we don't paint over the control's border */
	GetClientRect(hWnd, &rcClient);

	htempDC = CreateCompatibleDC(hDC);
	oldBitmap = SelectObject(htempDC, hBackground);

	if (hRgn == NULL)
	{
		/* create a region of our client area */
		rgnBitmap = CreateRectRgnIndirect(&rcClient);
		SelectClipRgn(hDC, rgnBitmap);
		DeleteObject(rgnBitmap);
	}
	else
	{
		/* use the passed in region */
		SelectClipRgn(hDC, hRgn);
	}

	hPAL = GetBackgroundPalette();
	if (hPAL == NULL)
		hPAL = CreateHalftonePalette(hDC);

	if (GetDeviceCaps(htempDC, RASTERCAPS) & RC_PALETTE && hPAL != NULL)
	{
		SelectPalette(htempDC, hPAL, FALSE);
		RealizePalette(htempDC);
	}

	for (i = rcClient.left-x; i < rcClient.right; i += bmDesc.bmWidth)
		for (j = rcClient.top-y; j < rcClient.bottom; j += bmDesc.bmHeight)
			BitBlt(hDC, i, j, bmDesc.bmWidth, bmDesc.bmHeight, htempDC, 0, 0, SRCCOPY);

	SelectObject(htempDC, oldBitmap);
	DeleteDC(htempDC);

	if (GetBackgroundPalette() == NULL)
	{
		DeleteObject(hPAL);
		hPAL = NULL;
	}

	ReleaseDC(hWnd, hDC);
}

static WCHAR *GetCloneParentName(int nItem)
{
	static WCHAR wstr[] = TEXT("");
	int nParentIndex = -1;

	if (DriverIsClone(nItem) == TRUE)
	{
		nParentIndex = GetParentIndex(drivers[nItem]);
		if (nParentIndex >= 0)
			return  UseLangList() ? 
				_LSTW(driversw[nParentIndex]->description) : driversw[nParentIndex]->modify_the;
	}
	return wstr;
}

static BOOL PickerHitTest(HWND hWnd)
{
	RECT			rect;
	POINTS			p;
	DWORD			res = GetMessagePos();
	LVHITTESTINFO	htInfo;

	ZeroMemory(&htInfo,sizeof(LVHITTESTINFO));
	p = MAKEPOINTS(res);
	GetWindowRect(hWnd, &rect);
	htInfo.pt.x = p.x - rect.left;
	htInfo.pt.y = p.y - rect.top;
	ListView_HitTest(hWnd, &htInfo);

	return (! (htInfo.flags & LVHT_NOWHERE));
}

static BOOL TreeViewNotify(LPNMHDR nm)
{
	switch (nm->code)
	{
	case TVN_SELCHANGEDW :
	case TVN_SELCHANGEDA:
	    {
		HTREEITEM hti = TreeView_GetSelection(hTreeView);
		TVITEM	  tvi;

		tvi.mask  = TVIF_PARAM | TVIF_HANDLE;
		tvi.hItem = hti;

		if (TreeView_GetItem(hTreeView, &tvi))
		{
			SetCurrentFolder((LPTREEFOLDER)tvi.lParam);
			if (bListReady)
			{
				ResetListView();
				UpdateScreenShot();
			}
		}
		return TRUE;
	    }
	case TVN_BEGINLABELEDITW :
	case TVN_BEGINLABELEDITA :
	    {
		TV_DISPINFO *ptvdi = (TV_DISPINFO *)nm;
		LPTREEFOLDER folder = (LPTREEFOLDER)ptvdi->item.lParam;

		if (folder->m_dwFlags & F_CUSTOM)
		{
			// user can edit custom folder names
			g_in_treeview_edit = TRUE;
			return FALSE;
		}
		// user can't edit built in folder names
		return TRUE;
	    }
	case TVN_ENDLABELEDITW :
	    {
		TV_DISPINFOW *ptvdi = (TV_DISPINFOW *)nm;
		LPTREEFOLDER folder = (LPTREEFOLDER)ptvdi->item.lParam;

		g_in_treeview_edit = FALSE;

		if (ptvdi->item.pszText == NULL || wcslen(ptvdi->item.pszText) == 0)
			return FALSE;

		return TryRenameCustomFolder(folder, ptvdi->item.pszText);
	    }
	case TVN_ENDLABELEDITA :
	    {
		TV_DISPINFOA *ptvdi = (TV_DISPINFOA *)nm;
		LPTREEFOLDER folder = (LPTREEFOLDER)ptvdi->item.lParam;

		g_in_treeview_edit = FALSE;

		if (ptvdi->item.pszText == NULL || strlen(ptvdi->item.pszText) == 0)
			return FALSE;

		return TryRenameCustomFolder(folder, _Unicode(ptvdi->item.pszText));
	    }
	}
	return FALSE;
}



static void GamePicker_OnHeaderContextMenu(POINT pt, int nColumn)
{
	// Right button was clicked on header
	HMENU hMenuLoad;
	HMENU hMenu;

	hMenuLoad = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_HEADER));
	hMenu = GetSubMenu(hMenuLoad, 0);
	TranslateMenu(hMenu, ID_SORT_ASCENDING);

#ifdef IMAGE_MENU
	if (GetImageMenuStyle() > 0)
	{
		ImageMenu_CreatePopup(hMain, hMenuLoad);
		ImageMenu_SetStyle(GetImageMenuStyle());
	}
#endif /* IMAGE_MENU */

	lastColumnClick = nColumn;
	TrackPopupMenu(hMenu,TPM_LEFTALIGN | TPM_RIGHTBUTTON,pt.x,pt.y,0,hMain,NULL);

#ifdef IMAGE_MENU
	if (GetImageMenuStyle() > 0)
		ImageMenu_Remove(hMenuLoad);
#endif /* IMAGE_MENU */

	DestroyMenu(hMenuLoad);
}



LPWSTR ConvertAmpersandString(LPCWSTR s)
{
	static WCHAR buf[200];
	LPWSTR ptr;

	ptr = buf;
	while (*s)
	{
		if (*s == '&')
			*ptr++ = *s;
		*ptr++ = *s++;
	}
	*ptr = 0;

	return buf;
}

static int GUI_seq_pressed(input_code *code, int seq_max)
{
	int j;
	int res = 1;
	int invert = 0;
	int count = 0;

	for (j = 0; j < seq_max; j++)
	{
		switch (code[j])
		{
		case SEQCODE_END :
			return res && count;
		case SEQCODE_OR :
			if (res && count)
				return 1;
			res = 1;
			count = 0;
			break;
		case SEQCODE_NOT :
			invert = !invert;
			break;
		default:
			if (res)
			{
				int pressed = keyboard_state[code[j]];
				if ((pressed != 0) == invert)
					res = 0;
			}
			invert = 0;
			++count;
		}
	}
	return res && count;
}

static void check_for_GUI_action(void)
{
	int i;

	for (i = 0; i < NUM_GUI_SEQUENCES; i++)
	{
		input_seq *is = &(GUISequenceControl[i].is);

		if (GUI_seq_pressed(is->code, ARRAY_LENGTH(is->code)))
		{
			dprintf("seq =%s pressed\n", GUISequenceControl[i].name);
			switch (GUISequenceControl[i].func_id)
			{
			case ID_GAME_AUDIT:
			case ID_GAME_PROPERTIES:
			case ID_CONTEXT_FILTERS:
			case ID_UI_START:
				KeyboardStateClear(); /* beacuse whe won't receive KeyUp mesage when we loose focus */
				break;
			default:
				break;
			}
			SendMessage(hMain, WM_COMMAND, GUISequenceControl[i].func_id, 0);
		}
	}
}

static void KeyboardStateClear(void)
{
	int i;

	for (i = 0; i < keyboard_state_count; i++)
		keyboard_state[i] = 0;

	dprintf("keyboard gui state cleared.\n");
}


static void KeyboardKeyDown(int syskey, int vk_code, int special)
{
	int i, found = 0;
	input_code icode = 0;
	int special_code = (special >> 24) & 1;
	int scancode = (special>>16) & 0xff;

	if ((vk_code==VK_MENU) || (vk_code==VK_CONTROL) || (vk_code==VK_SHIFT))
	{
		found = 1;

		/* a hack for right shift - it's better to use Direct X for keyboard input it seems......*/
		if (vk_code==VK_SHIFT)
			if (scancode>0x30) /* on my keyboard left shift scancode is 0x2a, right shift is 0x36 */
				special_code = 1;

		if (special_code) /* right hand keys */
		{
			switch(vk_code)
			{
			case VK_MENU:
				icode = KEYCODE_RALT;
				break;
			case VK_CONTROL:
				icode = KEYCODE_RCONTROL;
				break;
			case VK_SHIFT:
				icode = KEYCODE_RSHIFT;
				break;
			}
		}
		else /* left hand keys */
		{
			switch(vk_code)
			{
			case VK_MENU:
				icode = KEYCODE_LALT;
				break;
			case VK_CONTROL:
				icode = KEYCODE_LCONTROL;
				break;
			case VK_SHIFT:
				icode = KEYCODE_LSHIFT;
				break;
			}
		}
	}
	else
	{
		for (i = 0; i < wininput_count_key_trans_table(); i++)
		{
			if ( vk_code == win_key_trans_table[i][VIRTUAL_KEY])
			{
				icode = win_key_trans_table[i][MAME_KEY];
				found = 1;
				break;
			}
		}
	}
	if (!found)
	{
		dprintf("VK_code pressed not found =  %i\n",vk_code);
		//MessageBox(NULL,"keydown message arrived not processed","TitleBox",MB_OK);
		return;
	}
	dprintf("VK_code pressed found =  %i, sys=%i, mame_keycode=%i special=%08x\n", vk_code, syskey, icode, special);
	keyboard_state[icode] = 1;
	check_for_GUI_action();
}

static void KeyboardKeyUp(int syskey, int vk_code, int special)
{
	int i, found = 0;
	input_code icode = 0;
	int special_code = (special >> 24) & 1;
	int scancode = (special>>16) & 0xff;

	if ((vk_code==VK_MENU) || (vk_code==VK_CONTROL) || (vk_code==VK_SHIFT))
	{
		found = 1;

		/* a hack for right shift - it's better to use Direct X for keyboard input it seems......*/
		if (vk_code==VK_SHIFT)
			if (scancode>0x30) /* on my keyboard left shift scancode is 0x2a, right shift is 0x36 */
				special_code = 1;

		if (special_code) /* right hand keys */
		{
			switch(vk_code)
			{
			case VK_MENU:
				icode = KEYCODE_RALT;
				break;
			case VK_CONTROL:
				icode = KEYCODE_RCONTROL;
				break;
			case VK_SHIFT:
				icode = KEYCODE_RSHIFT;
				break;
			}
		}
		else /* left hand keys */
		{
			switch(vk_code)
			{
			case VK_MENU:
				icode = KEYCODE_LALT;
				break;
			case VK_CONTROL:
				icode = KEYCODE_LCONTROL;
				break;
			case VK_SHIFT:
				icode = KEYCODE_LSHIFT;
				break;
			}
		}
	}
	else
	{
		for (i = 0; i < wininput_count_key_trans_table(); i++)
		{
			if (vk_code == win_key_trans_table[i][VIRTUAL_KEY])
			{
				icode = win_key_trans_table[i][MAME_KEY];
				found = 1;
				break;
			}
		}
	}
	if (!found)
	{
		dprintf("VK_code released not found =  %i",vk_code);
		//MessageBox(NULL,"keyup message arrived not processed","TitleBox",MB_OK);
		return;
	}
	keyboard_state[icode] = 0;
	dprintf("VK_code released found=  %i, sys=%i, mame_keycode=%i special=%08x\n", vk_code, syskey, icode, special );
	check_for_GUI_action();
}

static void PollGUIJoystick(void)
{
	// For the exec timer, will keep track of how long the button has been pressed  
	static int exec_counter = 0;

	if (in_emulation)
		return;

	if (g_pJoyGUI == NULL)
		return;

	g_pJoyGUI->poll_joysticks();


	// User pressed UP
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyUp(0), GetUIJoyUp(1), GetUIJoyUp(2),GetUIJoyUp(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_UP, 0);
	}

	// User pressed DOWN
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyDown(0), GetUIJoyDown(1), GetUIJoyDown(2),GetUIJoyDown(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_DOWN, 0);
	}

	// User pressed LEFT
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyLeft(0), GetUIJoyLeft(1), GetUIJoyLeft(2),GetUIJoyLeft(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_LEFT, 0);
	}

	// User pressed RIGHT
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyRight(0), GetUIJoyRight(1), GetUIJoyRight(2),GetUIJoyRight(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_RIGHT, 0);
	}

	// User pressed START GAME
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyStart(0), GetUIJoyStart(1), GetUIJoyStart(2),GetUIJoyStart(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_START, 0);
	}

	// User pressed PAGE UP
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyPageUp(0), GetUIJoyPageUp(1), GetUIJoyPageUp(2),GetUIJoyPageUp(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_PGUP, 0);
	}

	// User pressed PAGE DOWN
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyPageDown(0), GetUIJoyPageDown(1), GetUIJoyPageDown(2),GetUIJoyPageDown(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_PGDOWN, 0);
	}

	// User pressed HOME
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyHome(0), GetUIJoyHome(1), GetUIJoyHome(2),GetUIJoyHome(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_HOME, 0);
	}

	// User pressed END
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyEnd(0), GetUIJoyEnd(1), GetUIJoyEnd(2),GetUIJoyEnd(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_END, 0);
	}

	// User pressed CHANGE SCREENSHOT
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoySSChange(0), GetUIJoySSChange(1), GetUIJoySSChange(2),GetUIJoySSChange(3))))
	{
		SendMessage(hMain, WM_COMMAND, IDC_SSFRAME, 0);
	}

	// User pressed SCROLL HISTORY UP
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyHistoryUp(0), GetUIJoyHistoryUp(1), GetUIJoyHistoryUp(2),GetUIJoyHistoryUp(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_HISTORY_UP, 0);
	}

	// User pressed SCROLL HISTORY DOWN
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyHistoryDown(0), GetUIJoyHistoryDown(1), GetUIJoyHistoryDown(2),GetUIJoyHistoryDown(3))))
	{
		SendMessage(hMain, WM_COMMAND, ID_UI_HISTORY_DOWN, 0);
	}

	// User pressed EXECUTE COMMANDLINE
	if (g_pJoyGUI->is_joy_pressed(JOYCODE(GetUIJoyExec(0), GetUIJoyExec(1), GetUIJoyExec(2),GetUIJoyExec(3))))
	{
		if (++exec_counter >= GetExecWait()) // Button has been pressed > exec timeout
		{
			PROCESS_INFORMATION pi;
			STARTUPINFOW si;

			// Reset counter
			exec_counter = 0;

			ZeroMemory( &si, sizeof(si) );
			ZeroMemory( &pi, sizeof(pi) );
			si.dwFlags = STARTF_FORCEONFEEDBACK;
			si.cb = sizeof(si);

			CreateProcessW(NULL, GetExecCommand(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

			// We will not wait for the process to finish cause it might be a background task
			// The process won't get closed when MAME32 closes either.

			// But close the handles cause we won't need them anymore. Will not close process.
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}
	else
	{
		// Button has been released within the timeout period, reset the counter
		exec_counter = 0;
	}
}

#if 0
static void PressKey(HWND hwnd, UINT vk)
{
	SendMessage(hwnd, WM_KEYDOWN, vk, 0);
	SendMessage(hwnd, WM_KEYUP,   vk, 0xc0000000);
}
#endif

static void DoSortColumn(int column)
{
	int id;

	SetSortColumn(column);

	for (id = 0; id < COLUMN_MAX; id++)
		CheckMenuItem(GetMenu(hMain), ID_VIEW_BYGAME + id, id == column ? MF_CHECKED : MF_UNCHECKED);
}

static void SetView(int menu_id)
{
	BOOL force_reset = FALSE;

	// first uncheck previous menu item, check new one
	CheckMenuRadioItem(GetMenu(hMain), ID_VIEW_LARGE_ICON, ID_VIEW_GROUPED, menu_id, MF_CHECKED);
	ToolBar_CheckButton(hToolBar, menu_id, MF_CHECKED);

	if (Picker_GetViewID(hwndList) == VIEW_GROUPED || menu_id == ID_VIEW_GROUPED)
	{
		// this changes the sort order, so redo everything
		force_reset = TRUE;
	}

	Picker_SetViewID(hwndList, menu_id - ID_VIEW_LARGE_ICON);

	if (force_reset)
	{
		Picker_Sort(hwndList);
		DoSortColumn(GetSortColumn());
	}
}

static void ResetListView(void)
{
	int 	i;
	int 	current_game;
	LV_ITEM lvi;
	BOOL	no_selection = FALSE;
	LPTREEFOLDER lpFolder = GetCurrentFolder();

	if (!lpFolder)
	{
		return;
	}

	/* If the last folder was empty, no_selection is TRUE */
	if (have_selection == FALSE)
	{
		no_selection = TRUE;
	}

	current_game = Picker_GetSelectedItem(hwndList);

	SetWindowRedraw(hwndList,FALSE);

	ListView_DeleteAllItems(hwndList);

	// hint to have it allocate it all at once
	ListView_SetItemCount(hwndList,game_count);

	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
	lvi.stateMask = 0;

	i = -1;

	do
	{
		/* Add the games that are in this folder */
		if ((i = FindGame(lpFolder, i + 1)) != -1)
		{
			if (GameFiltered(i, lpFolder->m_dwFlags))
				continue;

			lvi.iItem	 = i;
			lvi.iSubItem = 0;
			lvi.lParam	 = i;
			lvi.pszText  = LPSTR_TEXTCALLBACK;
			lvi.iImage	 = I_IMAGECALLBACK;
			ListView_InsertItem(hwndList, &lvi);
		}
	} while (i != -1);

	Picker_Sort(hwndList);
	DoSortColumn(GetSortColumn());

	if (bListReady)
	{
		/* If last folder was empty, select the first item in this folder */
		if (no_selection)
			Picker_SetSelectedPick(hwndList, 0);
		else
			Picker_SetSelectedItem(hwndList, current_game);
	}

	/*RS Instead of the Arrange Call that was here previously on all Views
		 We now need to set the ViewMode for SmallIcon again,
		 for an explanation why, see SetView*/
	if (GetViewMode() == VIEW_SMALL_ICONS)
		SetView(ID_VIEW_SMALL_ICON);

	SetWindowRedraw(hwndList, TRUE);

	UpdateStatusBar();
}

#ifdef EXPORT_GAME_LIST
static int MMO2LST(void)
{
	int i;
	OPENFILENAMEA ofn;
	char szFile[MAX_PATH]   = "\0";
	char buf[256];
	FILE *fp;
	sprintf(szFile, MAME32NAME "%s", ui_lang_info[GetLangcode()].shortname);
	strcpy(szFile, strlwr(szFile));

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner   = hMain;
	ofn.lpstrFile   = szFile;
	ofn.nMaxFile    = sizeof(szFile);
	ofn.Flags       = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

    switch (nExportContent)
    {
    case 1:
	ofn.lpstrFilter = "game list files (*.lst)\0*.lst;\0All files (*.*)\0*.*\0";
	ofn.lpstrTitle  = _String(_UIW(TEXT("Export all game list file")));
	ofn.lpstrDefExt = "lst";

	if (GetSaveFileNameA(&ofn) == 0)
		return 1;

	fp = fopen(szFile, "wt");
	if (fp == NULL)
	{
		sprintf(buf, _String(_UIW(TEXT("Could not create '%s'"))), szFile);
		SetStatusBarTextW(0, _Unicode(buf));
		return 1;
	}

	    for (i = 0; drivers[i]; i++)
	    {
		    const WCHAR *lst = _LSTW(driversw[i]->description);
		    const WCHAR *readings = _READINGSW(driversw[i]->description);
    
		    if (readings == driversw[i]->description)
			    readings = lst;
    
		    fprintf(fp, "%s\t%s\t%s\t%s\n",
			    drivers[i]->name, _String(lst), _String(readings), drivers[i]->manufacturer);
	    }
	fclose(fp);
	break;

    case 2:
	ofn.lpstrFilter = "game list files (*.lst)\0*.lst;\0All files (*.*)\0*.*\0";
	ofn.lpstrTitle  = _String(_UIW(TEXT("Export current folder game list file")));
	ofn.lpstrDefExt = "lst";

	if (GetSaveFileNameA(&ofn) == 0)
		return 1;

	ExportTranslationToFile(szFile);
	break;

    case 3:
	ofn.lpstrFilter = "Standard text file (*.txt)\0*.txt;\0All files (*.*)\0*.*\0";
	ofn.lpstrTitle  = _String(_UIW(TEXT("Export a roms list file")));
	ofn.lpstrDefExt = "txt";

	if (GetSaveFileNameA(&ofn) == 0)
		return 1;

	SaveRomsListToFile(szFile);
	break;

    case 4:
	ofn.lpstrFilter = "Formatted text file (*.txt)\0*.txt\0Tabuled text file (*.txt)\0*.txt\0";
	ofn.lpstrTitle  = _String(_UIW(TEXT("Export detail list file")));
	ofn.lpstrDefExt = "txt";

	if (GetSaveFileNameA(&ofn) == 0)
		return 1;

	SaveGameListToFile(szFile, (ofn.nFilterIndex==2 ? 0 : 1));
	break;
    }
	sprintf(buf, _String(_UIW(TEXT("'%s' created!"))), szFile);
	SetStatusBarTextW(0, _Unicode(buf));
	return 0;
}

static void ExportTranslationToFile(char *szFile)
{
	int nListCount = ListView_GetItemCount(hwndList);
	int nIndex = 0, nGameIndex = 0;
	char Buf[300];
	LV_ITEM lvi;
	FILE* fp = NULL;

	fp = fopen(szFile, "wt");
	if (fp == NULL)
	{
		sprintf(Buf, _String(_UIW(TEXT("Could not create '%s'"))), szFile);
		SetStatusBarTextW(0, _Unicode(Buf));
	}
	else
	{
	// Games
	for ( nIndex=0; nIndex<nListCount; nIndex++ )
	   {
		lvi.iItem = nIndex;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_PARAM;

		if ( ListView_GetItem(hwndList, &lvi) )
		{
			nGameIndex  = lvi.lParam;

			fprintf(fp, "%s\t%s\t%s\t%s\n",
			drivers[nGameIndex]->name,
			UseLangList() ? _String(_LSTW(driversw[nGameIndex]->description)) : ModifyThe(drivers[nGameIndex]->description),
			UseLangList() ? _String(_LSTW(driversw[nGameIndex]->description)) : ModifyThe(drivers[nGameIndex]->description),
			drivers[nGameIndex]->manufacturer);
		}
	   }
	fclose(fp);
	}
}

static void SaveGameListToFile(char *szFile, int Formatted)
{

	int Order[COLUMN_MAX];
	int Size[COLUMN_MAX] = {70, 4, 8, 9, 6, 9, 9, 30, 4, 70, 12, 9};
	int nColumnMax = Picker_GetNumColumns(hwndList);
	int nListCount = ListView_GetItemCount(hwndList);
	int nIndex = 0, nGameIndex = 0;
	int i, j = 0;
	const char *Filters[8] = { "Clones", "Non-Working", "Unvailable", "Vector Graphics", "Raster Graphics", "Originals", "Working", "Available"};

	char *CrLf;
	char Buf[300];

	LPTREEFOLDER lpFolder = GetCurrentFolder();
	LV_ITEM lvi;
	FILE* fp = NULL;

	fp = fopen(szFile, "wt");
	if (fp == NULL)
	{
		sprintf(Buf, _String(_UIW(TEXT("Could not create '%s'"))), szFile);
		SetStatusBarTextW(0, _Unicode(Buf));
	}
	else
	{
	// No interline with tabuled format
	if ( Formatted )
		CrLf = (char *)"\n\n";
	else
		CrLf = (char *)"\n";

	GetRealColumnOrder(Order);

	// Title
	sprintf( Buf, "%s %s.%s", MAME32NAME, GetVersionString(), CrLf );
	fwrite( Buf, strlen(Buf), 1, fp);
	sprintf( Buf, "This is the current list of games, as displayed in the GUI (%s view mode).%s", ((GetViewMode() == VIEW_GROUPED)?"grouped":"detail"), CrLf );
	fwrite( Buf, strlen(Buf), 1, fp);

	// Current folder
	sprintf( Buf, "Current folder : <" );
	if ( lpFolder->m_nParent != -1 )
	{
		// Shows only 2 levels (last and previous)
		LPTREEFOLDER lpF = GetFolder( lpFolder->m_nParent );

		strcat( Buf, _String(lpF->m_lpTitle) );
		strcat( Buf, " / " );
	}

	sprintf( &Buf[strlen(Buf)], "%s>%s.%s", _String(lpFolder->m_lpTitle), (lpFolder->m_dwFlags&F_CUSTOM?" (custom folder)":""), CrLf );
	fwrite( Buf, strlen(Buf), 1, fp);

	// Filters
	sprintf(Buf, "Additional excluding filter(s) : ");
	for (i=0,j=0; i<8; i++ )
	{
		if ( lpFolder->m_dwFlags & (1<<i) )
		{
			if ( j > 0)
			{
				strcat( Buf, ", ");
			}

			strcat(Buf, Filters[i]);

			j++;
		}
	}
	if ( j == 0)
	{
		strcat(Buf, "none");
	}
	strcat(Buf, ".");
	strcat(Buf, CrLf);
	fwrite( Buf, strlen(Buf), 1, fp);

	// Sorting
	if ( GetSortColumn() > 0 )
	{
		sprintf( Buf, "Sorted by <%s> descending order", _String(column_names[GetSortColumn()]) );
	}
	else
	{
		sprintf( Buf, "Sorted by <%s> ascending order", _String(column_names[-GetSortColumn()]) );
	}
	sprintf( &Buf[strlen(Buf)], ", %d game(s) found.%s", nListCount, CrLf );
	fwrite( Buf, strlen(Buf), 1, fp);

	if ( Formatted )
	{
		// Separator
		memset( Buf, '-', sizeof(Buf) );
		Buf[0] = '+';
		for (i=0,j=0; i<nColumnMax; i++ )
		{
			j += Size[Order[i]]+3;
			Buf[j] = '+';
		}
		Buf[j+1] = '\0';
		strcat( Buf, "\n");
		fwrite( Buf, strlen(Buf), 1, fp);

		// Left side of columns title
		Buf[0] = '|';
		Buf[1] = '\0';
	}
	else
		Buf[0] = '\0';

	// Title of columns
	for (i=0; i<nColumnMax; i++ )
	{
		if ( Formatted )
			sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], _String(column_names[Order[i]]) );
		else
		{
			if ( i )
				sprintf( &Buf[strlen(Buf)], "\t%s", _String(column_names[Order[i]]) );
			else
				sprintf( &Buf[strlen(Buf)], "%s", _String(column_names[Order[i]]) );
		}
	}
	strcat( Buf, "\n");
	fwrite( Buf, strlen(Buf), 1, fp);

	// Separator
	if ( Formatted )
	{
		memset( Buf, '-', sizeof(Buf) );
		Buf[0] = '+';
		for (i=0,j=0; i<nColumnMax; i++ )
		{
			// Fixed Real Column Size
			int size = (int)strlen(_String(column_names[Order[i]]));
			if ( Size[Order[i]] < size )
				Size[Order[i]] = size;
			j += Size[Order[i]]+3;
			Buf[j] = '+';
		}
		Buf[j+1] = '\0';
		strcat( Buf, "\n");
		fwrite( Buf, strlen(Buf), 1, fp);
	}

	// Games
	for ( nIndex=0; nIndex<nListCount; nIndex++ )
	{
		lvi.iItem = nIndex;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_PARAM;

		if ( ListView_GetItem(hwndList, &lvi) )
		{
			nGameIndex  = lvi.lParam;

			if ( Formatted )
			{
				Buf[0] = '|';
				Buf[1] = '\0';
			}
			else
				Buf[0] = '\0';

			// lvi.lParam contains the absolute game index
			for (i=0; i<nColumnMax; i++ )
			{
				if ((i > 1) && (! Formatted))
					strcat(&Buf[strlen(Buf)], "\t");

				switch( Order[i] )
				{
					case  0: // Name
						if ( Formatted )
						{
							if ( DriverIsClone(nGameIndex) && (GetViewMode() == VIEW_GROUPED) )
							{
								sprintf( &Buf[strlen(Buf)], "    %-*.*s |", Size[Order[i]]-3, Size[Order[i]]-3, UseLangList() ? _String(_LSTW(driversw[nGameIndex]->description)) : ModifyThe(drivers[nGameIndex]->description));
							}
							else
							{
								sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], UseLangList() ? _String(_LSTW(driversw[nGameIndex]->description)) : ModifyThe(drivers[nGameIndex]->description));
							}
						}
						else
							sprintf( &Buf[strlen(Buf)], "%s", UseLangList() ? _String(_LSTW(driversw[nGameIndex]->description)) : ModifyThe(drivers[nGameIndex]->description));
						break;

					case  1: // ROMs
						if ( Formatted )
							sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], (GetRomAuditResults(nGameIndex)==TRUE?"no":"yes") );
						else
							sprintf( &Buf[strlen(Buf)], "%s", (GetRomAuditResults(nGameIndex)==TRUE?"no":"yes") );
						break;

					case  2: // Samples
						if (DriverUsesSamples(nGameIndex))
						{
							if ( Formatted )
								sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], (GetSampleAuditResults(nGameIndex)?"no":"yes") );
							else
								sprintf( &Buf[strlen(Buf)], "%s", (GetSampleAuditResults(nGameIndex)?"no":"yes") );
						}
						else
						{
							if ( Formatted )
								sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], "" );
						}
						break;

					case  3: // Directory
						if ( Formatted )
							sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], drivers[nGameIndex]->name );
						else
							sprintf( &Buf[strlen(Buf)], "%s", drivers[nGameIndex]->name );
						break;

					case  4: // Type
						if ( Formatted )
							sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], (DriverIsVector(nGameIndex)?"Vector":"Raster") );
						else
							sprintf( &Buf[strlen(Buf)], "%s", (DriverIsVector(nGameIndex)?"Vector":"Raster") );
						break;

					case  5: // Trackball
						if ( Formatted )
							sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], (DriverUsesTrackball(nGameIndex)?"yes":"no") );
						else
							sprintf( &Buf[strlen(Buf)], "%s", (DriverUsesTrackball(nGameIndex)?"yes":"no") );
						break;

					case  6: // Played
						if ( Formatted )
							sprintf( &Buf[strlen(Buf)], " %-*d |", Size[Order[i]], GetPlayCount(nGameIndex) );
						else
							sprintf( &Buf[strlen(Buf)], "%d", GetPlayCount(nGameIndex) );
						break;

					case  7: // Manufacturer
						if ( Formatted )
							sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], drivers[nGameIndex]->manufacturer );
						else
							sprintf( &Buf[strlen(Buf)], "%s", drivers[nGameIndex]->manufacturer );
						break;

					case  8: // Year
						if ( Formatted )
							sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], drivers[nGameIndex]->year );
						else
							sprintf( &Buf[strlen(Buf)], "%s", drivers[nGameIndex]->year );
						break;

					case  9: // Clone of
						if ( Formatted )
							sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], _String(GetCloneParentName(nGameIndex)) );
						else
							sprintf( &Buf[strlen(Buf)], "%s", _String(GetCloneParentName(nGameIndex)) );
						break;

					case 10: // Source
						if ( Formatted )
							sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], _String(GetDriverFilename(nGameIndex)) );
						else
							sprintf( &Buf[strlen(Buf)], "%s", _String(GetDriverFilename(nGameIndex)) );
						break;

					case 11: // Play time
					{
						WCHAR Tmp[20];
						
						GetTextPlayTime(nGameIndex, Tmp);

						if ( Formatted )
							sprintf( &Buf[strlen(Buf)], " %-*.*s |", Size[Order[i]], Size[Order[i]], _String(Tmp) );
						else
							sprintf( &Buf[strlen(Buf)], "%s", _String(Tmp) );

						break;
					}
				}
			}
			strcat( Buf, "\n");
			fwrite( Buf, strlen(Buf), 1, fp);
		}
	}

	// Last separator
	if ( Formatted && (nListCount > 0) )
	{
		memset( Buf, '-', sizeof(Buf) );
		Buf[0] = '+';
		for (i=0,j=0; i<nColumnMax; i++ )
		{
			j += Size[Order[i]]+3;
			Buf[j] = '+';
		}
		Buf[j+1] = '\0';
		strcat( Buf, "\n");
		fwrite( Buf, strlen(Buf), 1, fp);
	}
	fclose(fp);
	}
}

static void SaveRomsListToFile(char *szFile)
{
	int Order[COLUMN_MAX];
	int nColumnMax = Picker_GetNumColumns(hwndList);
	int nListCount = ListView_GetItemCount(hwndList);
	int nIndex = 0, nGameIndex = 0;
	int i = 0;

	char *CrLf;
	char Buf[300];

	LPTREEFOLDER lpFolder = GetCurrentFolder();
	LV_ITEM lvi;
	FILE *fp;

	fp = fopen(szFile, "wt");
	if (fp == NULL)
	{
		sprintf(Buf, _String(_UIW(TEXT("Could not create '%s'"))), szFile);
		SetStatusBarTextW(0, _Unicode(Buf));
	}
	else
	{
	CrLf = (char *)"\n\n";

	GetRealColumnOrder(Order);

	// Title
	sprintf( Buf, "%s %s.%s", MAME32NAME, GetVersionString(), CrLf );
	fwrite( Buf, strlen(Buf), 1, fp);
	sprintf( Buf, "This is the current list of ROMs.%s", CrLf );
	fwrite( Buf, strlen(Buf), 1, fp);

	// Current folder
	sprintf( Buf, "Current folder : <" );
	if ( lpFolder->m_nParent != -1 )
	{
		// Shows only 2 levels (last and previous)
		LPTREEFOLDER lpF = GetFolder( lpFolder->m_nParent );

		strcat( Buf, _String(lpF->m_lpTitle) );
		strcat( Buf, " / " );
	}

	sprintf( &Buf[strlen(Buf)], "%s>%s.%s", _String(lpFolder->m_lpTitle), (lpFolder->m_dwFlags&F_CUSTOM?" (custom folder)":""), CrLf );
	fwrite( Buf, strlen(Buf), 1, fp);

	// Sorting
	if ( GetSortColumn() > 0 )
	{
		sprintf( Buf, "Sorted by <%s> descending order", _String(column_names[GetSortColumn()]) );
	}
	else
	{
		sprintf( Buf, "Sorted by <%s> ascending order", _String(column_names[-GetSortColumn()]) );
	}
	sprintf( &Buf[strlen(Buf)], ", %d game(s) found.%s", nListCount, CrLf );
	fwrite( Buf, strlen(Buf), 1, fp);

	Buf[0] = '\0';

	// Games
	for ( nIndex=0; nIndex<nListCount; nIndex++ )
	{
		lvi.iItem = nIndex;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_PARAM;

		if ( ListView_GetItem(hwndList, &lvi) )
		{
			nGameIndex  = lvi.lParam;
			Buf[0] = '\0';

			// lvi.lParam contains the absolute game index
			for (i=0; i<nColumnMax; i++ )
			{
				switch( Order[i] )
				{
					case  3: // ROMs
						sprintf( &Buf[strlen(Buf)], "%s", drivers[nGameIndex]->name );
						break;
				}
			}
			strcat( Buf, "\n");
			fwrite( Buf, strlen(Buf), 1, fp);
		}
	}
	fclose(fp);
	}
}
#endif /* EXPORT_GAME_LIST */

static void UpdateGameList(BOOL bUpdateRomAudit, BOOL bUpdateSampleAudit)
{
	int i;

	for (i = 0; i < game_count; i++)
	{
		if (bUpdateRomAudit && DriverUsesRoms(i))
		SetRomAuditResults(i, UNKNOWN);
		if (bUpdateSampleAudit && DriverUsesSamples(i))
		SetSampleAuditResults(i, UNKNOWN);
	}

	idle_work    = TRUE;
	bDoGameCheck = TRUE;
	game_index   = 0;

	ReloadIcons();

	// Let REFRESH also load new background if found
	LoadBackgroundBitmap();
	InvalidateRect(hMain,NULL,TRUE);
	Picker_ResetIdle(hwndList);
}

UINT_PTR CALLBACK CFHookProc(
	HWND hdlg,      // handle to dialog box
	UINT uiMsg,     // message identifier
	WPARAM wParam,  // message parameter
	LPARAM lParam   // message parameter
)
{
	int iIndex, i;
	COLORREF cCombo, cList;
	switch (uiMsg)
	{
		case WM_INITDIALOG:
			SendDlgItemMessage(hdlg, cmb4, CB_ADDSTRING, 0, (LPARAM)(const WCHAR *)_UIW(TEXT("Custom")));
			iIndex = SendDlgItemMessage(hdlg, cmb4, CB_GETCOUNT, 0, 0);
			cList = GetListFontColor();
			SendDlgItemMessage(hdlg, cmb4, CB_SETITEMDATA,(WPARAM)iIndex-1,(LPARAM)cList );
			for( i = 0; i< iIndex; i++)
			{
				cCombo = SendDlgItemMessage(hdlg, cmb4, CB_GETITEMDATA,(WPARAM)i,0 );
				if( cList == cCombo)
				{
					SendDlgItemMessage(hdlg, cmb4, CB_SETCURSEL,(WPARAM)i,0 );
					break;
				}
			}
			break;
		case WM_COMMAND:
			if( LOWORD(wParam) == cmb4)
			{
				switch (HIWORD(wParam))
				{
					case CBN_SELCHANGE:  // The color ComboBox changed selection
						iIndex = (int)SendDlgItemMessage(hdlg, cmb4,
													  CB_GETCURSEL, 0, 0L);
						if( iIndex == SendDlgItemMessage(hdlg, cmb4, CB_GETCOUNT, 0, 0)-1)
						{
							//Custom color selected
 							cList = GetListFontColor();
 							PickColor(&cList);
							SendDlgItemMessage(hdlg, cmb4, CB_DELETESTRING, iIndex, 0);
							SendDlgItemMessage(hdlg, cmb4, CB_ADDSTRING, 0, (LPARAM)(const WCHAR *)_UIW(TEXT("Custom")));
							SendDlgItemMessage(hdlg, cmb4, CB_SETITEMDATA,(WPARAM)iIndex,(LPARAM)cList);
							SendDlgItemMessage(hdlg, cmb4, CB_SETCURSEL,(WPARAM)iIndex,0 );
							return TRUE;
						}
				}
			}
			break;
	}
	return FALSE;
}

static void PickFont(void)
{
	LOGFONTW font;
	CHOOSEFONTW cf;
	HWND hWnd;

	GetListFont(&font);
	font.lfQuality = DEFAULT_QUALITY;

	cf.lStructSize = sizeof(cf);
	cf.hwndOwner   = hMain;
	cf.lpLogFont   = &font;
	cf.lpfnHook = &CFHookProc;
	cf.rgbColors   = GetListFontColor();
	cf.Flags	   = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_EFFECTS | CF_ENABLEHOOK;
	if (!ChooseFontW(&cf))
		return;

	SetListFont(&font);
	if (hFont != NULL)
		DeleteObject(hFont);
	hFont = TranslateCreateFont(&font);
	if (hFont != NULL)
	{
		COLORREF textColor = cf.rgbColors;

		if (textColor == RGB(255,255,255))
		{
			textColor = RGB(240, 240, 240);
		}

		SetAllWindowsFont(hMain, &main_resize, hFont, TRUE);

		hWnd = GetWindow(hMain, GW_CHILD);
		while(hWnd)
		{
			char szClass[128];
			if (GetClassNameA(hWnd, szClass, ARRAY_LENGTH(szClass)))
			{
				if (!strcmp(szClass, "SysListView32"))
				{
					ListView_SetTextColor(hWnd, textColor);
				}
				else if (!strcmp(szClass, "SysTreeView32"))
				{
					TreeView_SetTextColor(hTreeView, textColor);
				}
			}
			hWnd = GetWindow(hWnd, GW_HWNDNEXT);
		}
		SetListFontColor(cf.rgbColors);
		ResetListView();
	}
}

static void PickColor(COLORREF *cDefault)
{
	CHOOSECOLOR cc;
	COLORREF choice_colors[16];
	int i;
	
	for (i=0;i<16;i++)
		choice_colors[i] = GetCustomColor(i);
 
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner   = hMain;
	cc.rgbResult   = *cDefault;
	cc.lpCustColors = choice_colors;
	cc.Flags       = CC_ANYCOLOR | CC_RGBINIT | CC_SOLIDCOLOR;
	if (!ChooseColor(&cc))
		return;
	for (i=0;i<16;i++)
		SetCustomColor(i,choice_colors[i]);
	*cDefault = cc.rgbResult;
}

static void PickCloneColor(void)
{
	COLORREF cClonecolor;
	cClonecolor = GetListCloneColor();
	PickColor( &cClonecolor);
	SetListCloneColor(cClonecolor);
	InvalidateRect(hwndList,NULL,FALSE);
}

static BOOL MameCommand(HWND hwnd,int id, HWND hwndCtl, UINT codeNotify)
{
	int i;
#ifdef IMAGE_MENU
	if ((id >= ID_STYLE_NONE) && (id <= ID_STYLE_NONE + MENU_STYLE_MAX))
	{
		ChangeMenuStyle(id);
		return TRUE;
	}
#endif /* IMAGE_MENU */

	if ((id >= ID_LANGUAGE_ENGLISH_US) && (id < ID_LANGUAGE_ENGLISH_US + UI_LANG_MAX) 
		&& ((id - ID_LANGUAGE_ENGLISH_US) != GetLangcode()))
	{
		ChangeLanguage(id);
		return TRUE;
	}

#ifdef USE_IPS
	if ((id >= ID_PLAY_PATCH) && (id < ID_PLAY_PATCH + MAX_PATCHES))
	{
		int  nGame = Picker_GetSelectedItem(hwndList);
		WCHAR patch_filename[MAX_PATCHNAME];

		if (GetPatchFilename(patch_filename, driversw[nGame]->name, id-ID_PLAY_PATCH))
		{
			options_type* pOpts = GetGameOptions(nGame);
			static WCHAR new_opt[MAX_PATCHNAME * MAX_PATCHES];

			new_opt[0] = '\0';

			if (pOpts->ips)
			{
				WCHAR *temp = _wcsdup(pOpts->ips);
				WCHAR *token = NULL;

				if (temp)
					token = wcstok(temp, TEXT(","));

				while (token)
				{
					if (!wcscmp(patch_filename, token))
					{
						dprintf("dup!");
						patch_filename[0] = '\0';
					}
					else
					{
						if (new_opt[0] != '\0')
							wcscat(new_opt, TEXT(","));
						wcscat(new_opt, token);
					}

					token = wcstok(NULL, TEXT(","));
				}

				free(temp);
			}

			if (patch_filename[0] != '\0')
			{
				if (new_opt[0] != '\0')
					wcscat(new_opt, TEXT(","));
				wcscat(new_opt, patch_filename);
			}

			FreeIfAllocatedW(&pOpts->ips);
			if (new_opt[0] != '\0')
				pOpts->ips = wcsdup(new_opt);

			SaveGameOptions(nGame);
		}
		return TRUE;
	}
	else if (g_IPSMenuSelectName && id != IDC_HISTORY)
	{
		FreeIfAllocatedW(&g_IPSMenuSelectName);
		UpdateScreenShot();
	}
#endif /* USE_IPS */

	switch (id)
	{
	case ID_FILE_PLAY:
		MamePlayGame();
		return TRUE;

	case ID_FILE_PLAY_RECORD:
		MamePlayRecordGame();
		return TRUE;

	case ID_FILE_PLAY_BACK:
		MamePlayBackGame(NULL);
		return TRUE;

#ifdef MAME_AVI
    case ID_FILE_PLAY_BACK_AVI:
        MamePlayBackGameAVI();
        return TRUE;

	case ID_FILE_PLAY_WITH_AVI:	
		MamePlayGameAVI();
		return TRUE;
#endif /* MAME_AVI */

	case ID_FILE_PLAY_RECORD_WAVE:
		MamePlayRecordWave();
		return TRUE;

	case ID_FILE_PLAY_RECORD_MNG:
		MamePlayRecordMNG();
		return TRUE;

	case ID_FILE_LOADSTATE :
		MameLoadState(NULL);
		return TRUE;

#ifdef KAILLERA
    case ID_FILE_NETPLAY:
        {
            char mameVer[512];
            char *tmpGames = NULL;
            int gamesSize  = 8192;
            LVITEM i;
            int j, s;
			HMENU hMenu;		//kt

			if (bKailleraNetPlay != FALSE) return FALSE;	//kt
			bKailleraNetPlay = TRUE;
			hMenu = GetMenu(hMain);
			EnableMenuItem(hMenu, ID_FILE_NETPLAY,          MF_GRAYED);


			FreeLibrary_KailleraClient_DLL();	//kt
			{
				const WCHAR *str = GetKailleraClientDLL();
				if ( wcscmp(str, TEXT("\\")) )
					wsprintf(KailleraClientDLL_Name, TEXT("%s\\%s.dll"), GetKailleraDir(), str);
				else
					wcscpy(KailleraClientDLL_Name, TEXT("kailleraclient.dll"));
			}
			if (LoadLibrary_KailleraClient_DLL(_String(KailleraClientDLL_Name)) == FALSE)
			{
				WCHAR str[MAX_PATH + 32];
				wsprintf(str, _UIW(TEXT("Unable to load DLL: %s")), KailleraClientDLL_Name);
				MameMessageBoxW(str);
				bKailleraNetPlay = FALSE;
				EnableMenuItem(hMenu, ID_FILE_NETPLAY, MF_ENABLED);
				return FALSE;
			}
			kailleraInit();	//kt

            tmpGames = (char *)malloc(gamesSize);
            tmpGames[0] = 0;
            tmpGames[1] = 0;
            s = 0;

			if (UseLangList())
			{
				SetUseLangList(!UseLangList());
				ToolBar_CheckButton(hToolBar, IDC_USE_LIST, UseLangList() ^ (GetLangcode() == UI_LANG_EN_US) ? MF_CHECKED : MF_UNCHECKED);
				ResetListView();
				UpdateHistory();
			}

            if (GetNetPlayFolder())
                SetCurrentFolder(GetFolderByName(TEXT("Favorites")));
            else
                SetCurrentFolder(GetFolderById(FOLDER_AVAILABLE));
			SetSortReverse(FALSE);
			SetSortColumn(COLUMN_GAMES);
			SetView(ID_VIEW_DETAIL);
            ResetListView();

           // Get a list of avaliable game titles
           if (!UseLangList())
			{
	            memset(&i, 0, sizeof(LVITEM));

	            for (j = 0; j < ListView_GetItemCount(hwndList); j++)
	            {
	                char *p;
	                char tmptxt[512];
	                int l;
	
	                i.mask       = LVIF_TEXT;
	                i.iItem      = j;
	                i.iSubItem   = 0;
	                i.pszText    = (LPTSTR)tmptxt;
	                i.cchTextMax = 511;
	                ListView_GetItem(hwndList, &i);
	
	                while ((s + strlen(_String(i.pszText)) + 4) > gamesSize)
	                {
	                    gamesSize *= 2;
	                    tmpGames = (char *)realloc(tmpGames, gamesSize);
	                }
	                p = tmpGames+s;
	                strcpy(p, _String(i.pszText));
	                l = strlen(p) + 1;
	                p[l] = 0;
	                s += l;
	            }
			}

            /* StretchMame Feature */
            {
                char *p;
                char tmptxt[512];
                int l;

                strcpy(tmptxt, "*Away (leave messages)");
                while ((s + strlen(tmptxt) + 4) > gamesSize)
                {
                    gamesSize *= 2;
                    tmpGames = (char *)realloc(tmpGames, gamesSize);
                }
                p = tmpGames + s;
                strcpy(p, tmptxt);
                l = strlen(p) + 1;
                p[l] = 0;
                s += l;

                strcpy(tmptxt, "*Chat (not game)");
                while ((s + strlen(tmptxt) + 4) > gamesSize)
                {
                    gamesSize *= 2;
                    tmpGames = (char *)realloc(tmpGames, gamesSize);
                }
                p = tmpGames + s;
                strcpy(p, tmptxt);
                l = strlen(p) + 1;
                p[l] = 0;
                s += l;
            }

#ifdef HAZEMD
            sprintf(mameVer, "%s %s", "HazeMD++", GetVersionString());
#else
            sprintf(mameVer, "%s %s", MAME32NAME "++", GetVersionString());
#endif /* HAZEMD */

            kInfos.appName  = mameVer;
            kInfos.gameList = tmpGames;

			if(bKailleraMAME32WindowHide == TRUE)
			{
				bMAME32WindowShow = FALSE;
				ShowWindow(hMain, SW_HIDE);
				EnableWindow(hMain, FALSE);
			}

            kailleraSetInfos(&kInfos);

			bKailleraMAME32WindowOwner = GetKailleraMAME32WindowOwner();
			if (bKailleraMAME32WindowOwner == FALSE)
				kailleraSelectServerDialog(NULL);
			else 
				kailleraSelectServerDialog(hMain);

			if(bKailleraMAME32WindowHide == TRUE)
			{
				bMAME32WindowShow = TRUE;
				EnableWindow(hMain, TRUE);
				ShowWindow(hMain, SW_SHOW);
			}

            free(tmpGames);

			kailleraShutdown();	//kt
			bKailleraNetPlay = FALSE;

			if (bKailleraMAME32WindowOwner == FALSE)
			{
				ShowWindow(hMain, SW_RESTORE);
				ResetListView();
			}

			EnableMenuItem(hMenu, ID_FILE_NETPLAY, MF_ENABLED);

			if (bKailleraMAME32WindowOwner == FALSE)
				SetForegroundWindow(hMain);
        }
        return TRUE;
#endif /* KAILLERA */

#ifdef KSERVER
	case ID_FILE_SERVER:
	{
	char cCommandLine[32];
	PROCESS_INFORMATION pi;
        STARTUPINFOA si;
	ZeroMemory( &si, sizeof(si) );
	ZeroMemory( &pi, sizeof(pi) );
        si.cb=sizeof(si);
	if(GetShowConsole())si.wShowWindow=SW_SHOW;
	else si.wShowWindow=SW_HIDE;
        si.dwFlags=STARTF_USESHOWWINDOW;
	strcpy(cCommandLine, "kaillera\\kaillerasrv.exe");
	if(m_hPro==NULL)
	   {
        	if (!CreateProcessA(NULL, cCommandLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS|CREATE_NEW_CONSOLE, NULL, "kaillera", &si, &pi))
			{
				MameMessageBoxW(_UIW(TEXT("Failed to start Kaillera Server!\nPlease confirm \"kaillerasrv.exe\" exists in \"kaillera\" directory.")));
			}
			else
			{
				m_hPro=pi.hProcess;//Save Current Handle, will be used for Terminate this process.
				MameMessageBoxI(_UIW(TEXT("Start Kaillera Server Succeeded!")));
			}
	   }

	else
	{
                if(!TerminateProcess(m_hPro,0)) //Terminate code is 0
					MameMessageBoxW(_UIW(TEXT("Terminate Kaillera Server ERROR!!!")));
                else
	                MameMessageBoxI(_UIW(TEXT("Kaillera Server Terminated!")));
                m_hPro=NULL;
        }
	}
	return TRUE;
#endif /* KSERVER */

	case ID_FILE_AUDIT:
		AuditDialog(hMain);
		ResetWhichGamesInFolders();
		ResetListView();
		SetFocus(hwndList);
		return TRUE;

	case ID_FILE_EXIT:
		PostMessage(hMain, WM_CLOSE, 0, 0);
		return TRUE;

	case ID_VIEW_LARGE_ICON:
		SetView(ID_VIEW_LARGE_ICON);
		return TRUE;

	case ID_VIEW_SMALL_ICON:
		SetView(ID_VIEW_SMALL_ICON);
		ResetListView();
		return TRUE;

	case ID_VIEW_LIST_MENU:
		SetView(ID_VIEW_LIST_MENU);
		return TRUE;

	case ID_VIEW_DETAIL:
		SetView(ID_VIEW_DETAIL);
		return TRUE;

	case ID_VIEW_GROUPED:
		SetView(ID_VIEW_GROUPED);
		return TRUE;

	/* Arrange Icons submenu */
	case ID_VIEW_BYGAME:
	case ID_VIEW_BYROMS:
	case ID_VIEW_BYSAMPLES:
	case ID_VIEW_BYDIRECTORY:
	case ID_VIEW_BYTYPE:
	case ID_VIEW_TRACKBALL:
	case ID_VIEW_BYTIMESPLAYED:
	case ID_VIEW_BYMANUFACTURER:
	case ID_VIEW_BYYEAR:
	case ID_VIEW_BYCLONE:
	case ID_VIEW_BYSRCDRIVERS:
	case ID_VIEW_BYPLAYTIME:
		SetSortReverse(FALSE);
		DoSortColumn(id - ID_VIEW_BYGAME);
		Picker_Sort(hwndList);
		break;

	case ID_VIEW_FOLDERS:
		bShowTree = !bShowTree;
		SetShowFolderList(bShowTree);
		CheckMenuItem(GetMenu(hMain), ID_VIEW_FOLDERS, (bShowTree) ? MF_CHECKED : MF_UNCHECKED);
		ToolBar_CheckButton(hToolBar, ID_VIEW_FOLDERS, (bShowTree) ? MF_CHECKED : MF_UNCHECKED);
		UpdateScreenShot();
		break;

	case ID_VIEW_TOOLBARS:
		bShowToolBar = !bShowToolBar;
		SetShowToolBar(bShowToolBar);
		CheckMenuItem(GetMenu(hMain), ID_VIEW_TOOLBARS, (bShowToolBar) ? MF_CHECKED : MF_UNCHECKED);
		ToolBar_CheckButton(hToolBar, ID_VIEW_TOOLBARS, (bShowToolBar) ? MF_CHECKED : MF_UNCHECKED);
		ShowWindow(hToolBar, (bShowToolBar) ? SW_SHOW : SW_HIDE);
		ResizePickerControls(hMain);
		UpdateScreenShot();
		break;

	case ID_VIEW_STATUS:
		bShowStatusBar = !bShowStatusBar;
		SetShowStatusBar(bShowStatusBar);
		CheckMenuItem(GetMenu(hMain), ID_VIEW_STATUS, (bShowStatusBar) ? MF_CHECKED : MF_UNCHECKED);
		ToolBar_CheckButton(hToolBar, ID_VIEW_STATUS, (bShowStatusBar) ? MF_CHECKED : MF_UNCHECKED);
		ShowWindow(hStatusBar, (bShowStatusBar) ? SW_SHOW : SW_HIDE);
		ResizePickerControls(hMain);
		UpdateScreenShot();
		break;

	case ID_VIEW_PAGETAB:
		bShowTabCtrl = !bShowTabCtrl;
		SetShowTabCtrl(bShowTabCtrl);
		ShowWindow(hTabCtrl, (bShowTabCtrl) ? SW_SHOW : SW_HIDE);
		ResizePickerControls(hMain);
		UpdateScreenShot();
		InvalidateRect(hMain,NULL,TRUE);
		break;

		/*
		  Switches to fullscreen mode. No check mark handeling 
		  for this item cause in fullscreen mode the menu won't
		  be visible anyways.
		*/    
	case ID_VIEW_FULLSCREEN:
		SwitchFullScreenMode();
		break;

	case IDC_USE_LIST:
		SetUseLangList(!UseLangList());
		ToolBar_CheckButton(hToolBar, IDC_USE_LIST, UseLangList() ^ (GetLangcode() == UI_LANG_EN_US) ? MF_CHECKED : MF_UNCHECKED);
		ResetListView();
		UpdateHistory();
		break;

#ifdef KAILLERA
	case IDC_USE_NETPLAY_FOLDER:
		SetNetPlayFolder(!GetNetPlayFolder());
		ToolBar_CheckButton(hToolBar, IDC_USE_NETPLAY_FOLDER, GetNetPlayFolder() ? MF_CHECKED : MF_UNCHECKED);
		break;

	case IDC_USE_IME_IN_CHAT:
		SetUseImeInChat(!GetUseImeInChat());
		ToolBar_CheckButton(hToolBar, IDC_USE_IME_IN_CHAT, GetUseImeInChat() ? MF_CHECKED : MF_UNCHECKED);
		break;
#endif /* KAILLERA */

	case ID_TOOLBAR_EDIT:
		{
			WCHAR buf[256];

			Edit_GetText(hwndCtl, buf, ARRAY_LENGTH(buf));
			switch (codeNotify)
			{
			case EN_CHANGE:
				//put search routine here first, add a 200ms timer later.
				if ((!_wcsicmp(buf, _UIW(TEXT(SEARCH_PROMPT))) && !_wcsicmp(g_SearchText, TEXT(""))) ||
				    (!_wcsicmp(g_SearchText, _UIW(TEXT(SEARCH_PROMPT))) && !_wcsicmp(buf, TEXT(""))))
				{
					wcscpy(g_SearchText, buf);
				}
				else
				{
					wcscpy(g_SearchText, buf);
					ResetListView();
				}
				break;
			case EN_SETFOCUS:
				if (!_wcsicmp(buf, _UIW(TEXT(SEARCH_PROMPT))))
					SetWindowTextW(hwndCtl, TEXT(""));
				break;
			case EN_KILLFOCUS:
				if (wcslen(buf) == 0)
					SetWindowTextW(hwndCtl, _UIW(TEXT(SEARCH_PROMPT)));
				break;
			}
		}
		break;

	case ID_GAME_AUDIT:
		InitGameAudit(0);
		if (!oldControl)
		{
			InitPropertyPageToPage(hInst, hwnd, Picker_GetSelectedItem(hwndList), GetSelectedPickItemIcon(), AUDIT_PAGE, NULL);
			SaveGameOptions(Picker_GetSelectedItem(hwndList));
		}
		/* Just in case the toggle MMX on/off */
		UpdateStatusBar();
		break;

	/* ListView Context Menu */
	case ID_CONTEXT_ADD_CUSTOM:
	{
		int  nResult;

		nResult = DialogBoxParam(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_CUSTOM_FILE),
								 hMain,AddCustomFileDialogProc,Picker_GetSelectedItem(hwndList));
		SetFocus(hwndList);
		break;
	}

	case ID_CONTEXT_REMOVE_CUSTOM:
	{
		RemoveCurrentGameCustomFolder();
		break;
	}

	/* Tree Context Menu */
	case ID_CONTEXT_FILTERS:
		if (DialogBox(GetModuleHandle(NULL),
			MAKEINTRESOURCE(IDD_FILTERS), hMain, FilterDialogProc) == TRUE)
			ResetListView();
		SetFocus(hwndList);
		return TRUE;

	// ScreenShot Context Menu
	// select current tab
	case ID_VIEW_TAB_SCREENSHOT :
	case ID_VIEW_TAB_FLYER :
	case ID_VIEW_TAB_CABINET :
	case ID_VIEW_TAB_MARQUEE :
	case ID_VIEW_TAB_TITLE :
	case ID_VIEW_TAB_CONTROL_PANEL :
	case ID_VIEW_TAB_HISTORY :
#ifdef STORY_DATAFILE
	case ID_VIEW_TAB_STORY :
		if ((id == ID_VIEW_TAB_HISTORY || id == ID_VIEW_TAB_STORY) && GetShowTab(TAB_HISTORY) == FALSE)
#else /* STORY_DATAFILE */
		if (id == ID_VIEW_TAB_HISTORY && GetShowTab(TAB_HISTORY) == FALSE)
#endif /* STORY_DATAFILE */
			break;

		TabView_SetCurrentTab(hTabCtrl, id - ID_VIEW_TAB_SCREENSHOT);
		UpdateScreenShot();
		TabView_UpdateSelection(hTabCtrl);
		break;

		// toggle tab's existence
	case ID_TOGGLE_TAB_SCREENSHOT :
	case ID_TOGGLE_TAB_FLYER :
	case ID_TOGGLE_TAB_CABINET :
	case ID_TOGGLE_TAB_MARQUEE :
	case ID_TOGGLE_TAB_TITLE :
	case ID_TOGGLE_TAB_CONTROL_PANEL :
	case ID_TOGGLE_TAB_HISTORY :
#ifdef STORY_DATAFILE
	case ID_TOGGLE_TAB_STORY :
#endif /* STORY_DATAFILE */
	{
		int toggle_flag = id - ID_TOGGLE_TAB_SCREENSHOT;

		if (AllowedToSetShowTab(toggle_flag,!GetShowTab(toggle_flag)) == FALSE)
		{
			// attempt to hide the last tab
			// should show error dialog? hide picture area? or ignore?
			break;
		}

		SetShowTab(toggle_flag,!GetShowTab(toggle_flag));

		TabView_Reset(hTabCtrl);

		if (TabView_GetCurrentTab(hTabCtrl) == toggle_flag && GetShowTab(toggle_flag) == FALSE)
		{
			// we're deleting the tab we're on, so go to the next one
			TabView_CalculateNextTab(hTabCtrl);
		}


		// Resize the controls in case we toggled to another history
		// mode (and the history control needs resizing).

		ResizePickerControls(hMain);
		UpdateScreenShot();

		TabView_UpdateSelection(hTabCtrl);

		break;
	}

	/* Header Context Menu */
	case ID_SORT_ASCENDING:
		SetSortReverse(FALSE);
		SetSortColumn(Picker_GetRealColumnFromViewColumn(hwndList, lastColumnClick));
		Picker_Sort(hwndList);
		break;

	case ID_SORT_DESCENDING:
		SetSortReverse(TRUE);
		SetSortColumn(Picker_GetRealColumnFromViewColumn(hwndList, lastColumnClick));
		Picker_Sort(hwndList);
		break;

	case ID_CUSTOMIZE_FIELDS:
		if (DialogBox(GetModuleHandle(NULL),
			MAKEINTRESOURCE(IDD_COLUMNS), hMain, ColumnDialogProc) == TRUE)
			ResetColumnDisplay(FALSE);
		SetFocus(hwndList);
		return TRUE;

	/* View Menu */
	case ID_VIEW_LINEUPICONS:
		if( codeNotify == FALSE)
			ResetListView();
		else
		{
			/*it was sent after a refresh (F5) was done, we only reset the View if "available" is the selected folder
			  as it doesn't affect the others*/
			LPTREEFOLDER folder = GetSelectedFolder();
			if( folder )
			{
				if (folder->m_nFolderId == FOLDER_AVAILABLE )
					ResetListView();

			}
		}
		break;

	//fixme: inconsistent w/ official
	case ID_GAME_PROPERTIES:
		if (!oldControl)
		{
			InitPropertyPage(hInst, hwnd, Picker_GetSelectedItem(hwndList), GetSelectedPickItemIcon(), NULL);
			SaveGameOptions(Picker_GetSelectedItem(hwndList));
		}
		/* Just in case the toggle MMX on/off */
		UpdateStatusBar();
		break;

	case ID_BIOS_PROPERTIES:
		if (!oldControl)
		{
			int bios_driver = DriverBiosIndex(Picker_GetSelectedItem(hwndList));
			if (bios_driver != -1)
			{
				HICON hIcon = ImageList_GetIcon(NULL, IDI_BIOS, ILD_TRANSPARENT);
				InitPropertyPage(hInst, hwnd, bios_driver, hIcon, NULL);
				SaveGameOptions(bios_driver);
			}
		}
		/* Just in case the toggle MMX on/off */
		UpdateStatusBar();
		break;

	case ID_FOLDER_SOURCEPROPERTIES:
		if (!oldControl)
		{
			LPTREEFOLDER lpFolder = GetSourceFolder(Picker_GetSelectedItem(hwndList));
			int icon_id = GetTreeViewIconIndex(lpFolder->m_nIconId);
			HICON hIcon = ImageList_GetIcon(NULL, icon_id, ILD_TRANSPARENT);
			const WCHAR *name = lpFolder->m_lpTitle;

			if (lpFolder->m_lpOriginalTitle)
				name = lpFolder->m_lpOriginalTitle;

			InitPropertyPage(hInst, hwnd, -2, hIcon, name);
			SaveFolderOptions(name);
		}
		/* Just in case the toggle MMX on/off */
		UpdateStatusBar();
		break;

	//fixme: inconsistent w/ official
	case ID_FOLDER_PROPERTIES:
		if (!oldControl)
		{
			LPTREEFOLDER lpFolder = GetSelectedFolder();
			int bios_driver = GetBiosDriverByFolder(lpFolder);
			if (bios_driver != -1)
			{
				InitPropertyPage(hInst, hwnd, bios_driver, GetSelectedFolderIcon(), NULL);
				SaveGameOptions(bios_driver);
			}
			else
			{
				const WCHAR *name = lpFolder->m_lpTitle;

				if (lpFolder->m_lpOriginalTitle)
					name = lpFolder->m_lpOriginalTitle;

				InitPropertyPage(hInst, hwnd, -2, GetSelectedFolderIcon(), name);
				SaveFolderOptions(name);
			}
		}
		/* Just in case the toggle MMX on/off */
		UpdateStatusBar();
		break;
	case ID_FOLDER_AUDIT:
		FolderCheck();
		/* Just in case the toggle MMX on/off */
		UpdateStatusBar();
		break;

	case ID_VIEW_PICTURE_AREA :
		ToggleScreenShot();
		break;

	case ID_UPDATE_GAMELIST:
		UpdateGameList(TRUE, TRUE);
		break;

	case ID_OPTIONS_FONT:
		PickFont();
		UpdateHistory();
		return TRUE;

	case ID_OPTIONS_CLONE_COLOR:
		PickCloneColor();
		return TRUE;

	case ID_OPTIONS_DEFAULTS:
		/* Check the return value to see if changes were applied */
		if (!oldControl)
		{
			InitDefaultPropertyPage(hInst, hwnd);
			SaveDefaultOptions();
		}
		SetFocus(hwndList);
		return TRUE;

	case ID_OPTIONS_DIR:
		{
			int  nResult;
			BOOL bUpdateRoms;
			BOOL bUpdateSamples;

			nResult = DialogBox(GetModuleHandle(NULL),
			                    MAKEINTRESOURCE(IDD_DIRECTORIES),
			                    hMain,
			                    DirectoriesDialogProc);

			SaveDefaultOptions();

			bUpdateRoms    = ((nResult & DIRDLG_ROMS)	 == DIRDLG_ROMS)	? TRUE : FALSE;
			bUpdateSamples = ((nResult & DIRDLG_SAMPLES) == DIRDLG_SAMPLES) ? TRUE : FALSE;

			/* update game list */
			if (bUpdateRoms == TRUE || bUpdateSamples == TRUE)
				UpdateGameList(bUpdateRoms, bUpdateSamples);

			SetFocus(hwndList);
		}
		return TRUE;

	case ID_OPTIONS_RESET_DEFAULTS:
		if (DialogBox(GetModuleHandle(NULL),
		              MAKEINTRESOURCE(IDD_RESET), hMain, ResetDialogProc) == TRUE)
		{
			// these may have been changed
			SaveDefaultOptions();
			DestroyWindow(hwnd);
			PostQuitMessage(0);
		}
		return TRUE;

	case ID_OPTIONS_INTERFACE:
		DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_INTERFACE_OPTIONS),
		          hMain, InterfaceDialogProc);
		SaveDefaultOptions();

		KillTimer(hMain, SCREENSHOT_TIMER);
		if( GetCycleScreenshot() > 0)
		{
			SetTimer(hMain, SCREENSHOT_TIMER, GetCycleScreenshot()*1000, NULL ); // Scale to seconds
		}

		return TRUE;

#ifdef USE_VIEW_PCBINFO
	case ID_VIEW_PCBINFO:
		DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PCBINFO),
				  hMain, PCBInfoDialogProc);
		SetFocus(hwndList);
		return TRUE;
#endif /* USE_VIEW_PCBINFO */

#ifdef MAME32PLUSPLUS
	case ID_MAME_HOMEPAGE:
		{
			if (MessageBoxW(GetMainWindow(), _UIW(TEXT("Go to MAME Development site?")),
				TEXT_MAME32NAME, MB_ICONQUESTION | MB_YESNO) == IDYES)
			{
				ShellExecuteW(GetMainWindow(), NULL, TEXT("http://www.mamedev.com"),
							 NULL, NULL, SW_SHOWNORMAL);
			return TRUE;
			}
		}
		break;

	case ID_MAME_FAQ:
		{
			if (MessageBox(GetMainWindow(), _UIW(TEXT("Go to MAME FAQ site?")),
				TEXT_MAME32NAME, MB_ICONQUESTION | MB_YESNO) == IDYES)
			{
				ShellExecuteW(GetMainWindow(), NULL, TEXT("http://www.mame.net/mamefaq.html"),
							 NULL, NULL, SW_SHOWNORMAL);
			return TRUE;
			}
		}
		break;

	case ID_VIEW_MAWS:
		{
			int nGame;
			char address[256];
			nGame = Picker_GetSelectedItem(hwndList);
			strcpy(address, "http://www.mameworld.net/maws/romset/");
			strcat(address, drivers[nGame]->name);
			if (MessageBoxW(GetMainWindow(), _UIW(TEXT("Go to MAWS site?")),
				TEXT_MAME32NAME, MB_ICONQUESTION | MB_YESNO) == IDYES)
			{
				ShellExecuteW(GetMainWindow(), NULL, _Unicode(address), NULL, NULL, SW_SHOWNORMAL);
				return TRUE;
			}
		}
		break;
#endif /* MAME32PLUSPLUS */

	case ID_OPTIONS_BG:
		{
			WCHAR filename[MAX_PATH];
			*filename = 0;

			if (CommonFileDialog(FALSE, filename, FILETYPE_IMAGE_FILES))
			{
				ResetBackground(filename);
				LoadBackgroundBitmap();
				InvalidateRect(hMain, NULL, TRUE);
				return TRUE;
			}
		}
		break;

#ifdef UI_COLOR_PALETTE
	case ID_OPTIONS_PALETTE:
		DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PALETTE),
				  hMain, PaletteDialogProc);
		return TRUE;
#endif /* UI_COLOR_PALETTE */

#ifdef KAILLERA
    case ID_OPTIONS_KAILLERA_OPTIONS:
        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_KAILLERA_OPTIONS),
                  hMain, KailleraOptionDialogProc);
        return TRUE;
#endif /* KAILLERA */

#ifdef KSERVER
    case ID_OPTIONS_KSERVER_OPTIONS:
        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SERVER),
                  hMain, KServerOptionDialogProc);
        return TRUE;
#endif

#ifdef EXPORT_GAME_LIST
	case ID_OPTIONS_MMO2LST:
		if(DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_EXPORT),
                  hMain, ExportOptionDialogProc)==TRUE)MMO2LST();
        return TRUE;
#endif

	case ID_HELP_ABOUT:
		DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT),
				  hMain, AboutDialogProc);
		SetFocus(hwndList);
		return TRUE;

	case IDOK :
		/* cmk -- might need to check more codes here, not sure */
		if (codeNotify != EN_CHANGE && codeNotify != EN_UPDATE)
		{
			/* enter key */
			if (g_in_treeview_edit)
			{
				TreeView_EndEditLabelNow(hTreeView, FALSE);
				return TRUE;
			}
			else 
				if (have_selection)
					MamePlayGame();
		}
		break;

	case IDCANCEL : /* esc key */
		if (g_in_treeview_edit)
			TreeView_EndEditLabelNow(hTreeView, TRUE);
		break;

	case IDC_PLAY_GAME :
		if (have_selection)
			MamePlayGame();
		break;

	case ID_UI_START:
		SetFocus(hwndList);
		MamePlayGame();
		break;

	case ID_UI_UP:
		Picker_SetSelectedPick(hwndList, GetSelectedPick() - 1);
		break;

	case ID_UI_DOWN:
		Picker_SetSelectedPick(hwndList, GetSelectedPick() + 1);
		break;

	case ID_UI_PGUP:
		Picker_SetSelectedPick(hwndList, GetSelectedPick() - ListView_GetCountPerPage(hwndList));
		break;

	case ID_UI_PGDOWN:
		if ( (GetSelectedPick() + ListView_GetCountPerPage(hwndList)) < ListView_GetItemCount(hwndList) )
			Picker_SetSelectedPick(hwndList,  GetSelectedPick() + ListView_GetCountPerPage(hwndList) );
		else
			Picker_SetSelectedPick(hwndList,  ListView_GetItemCount(hwndList)-1 );
		break;

	case ID_UI_HOME:
		Picker_SetSelectedPick(hwndList, 0);
		break;

	case ID_UI_END:
		Picker_SetSelectedPick(hwndList,  ListView_GetItemCount(hwndList)-1 );
		break;
	case ID_UI_LEFT:
		/* hmmmmm..... */
		SendMessage(hwndList,WM_HSCROLL, SB_LINELEFT, 0);
		break;

	case ID_UI_RIGHT:
		/* hmmmmm..... */
		SendMessage(hwndList,WM_HSCROLL, SB_LINERIGHT, 0);
		break;
	case ID_UI_HISTORY_UP:
		/* hmmmmm..... */
		{
			HWND hHistory = GetDlgItem(hMain, IDC_HISTORY);
			SendMessage(hHistory, EM_SCROLL, SB_PAGEUP, 0);
		}
		break;

	case ID_UI_HISTORY_DOWN:
		/* hmmmmm..... */
		{
			HWND hHistory = GetDlgItem(hMain, IDC_HISTORY);
			SendMessage(hHistory, EM_SCROLL, SB_PAGEDOWN, 0);
		}
		break;

	case IDC_SSFRAME:
		TabView_CalculateNextTab(hTabCtrl);
		UpdateScreenShot();
		TabView_UpdateSelection(hTabCtrl);
		break;

	case ID_CONTEXT_SELECT_RANDOM:
		SetRandomPickItem();
		break;

	case ID_CONTEXT_RESET_PLAYTIME:
		ResetPlayTime( Picker_GetSelectedItem(hwndList) );
		ListView_RedrawItems(hwndList, GetSelectedPick(), GetSelectedPick());
		break;

	case ID_CONTEXT_RESET_PLAYCOUNT:
		ResetPlayCount( Picker_GetSelectedItem(hwndList) );
		ListView_RedrawItems(hwndList, GetSelectedPick(), GetSelectedPick());
		break;

	case ID_CONTEXT_RENAME_CUSTOM :
		TreeView_EditLabel(hTreeView,TreeView_GetSelection(hTreeView));
		break;

	default:
		if (id >= ID_CONTEXT_SHOW_FOLDER_START && id < ID_CONTEXT_SHOW_FOLDER_END)
		{
			ToggleShowFolder(id - ID_CONTEXT_SHOW_FOLDER_START);
			break;
		}
		for (i = 0; g_helpInfo[i].nMenuItem > 0; i++)
		{
			if (g_helpInfo[i].nMenuItem == id)
			{
				if (g_helpInfo[i].bIsHtmlHelp)
					HelpFunction(hMain, g_helpInfo[i].lpFile, HH_DISPLAY_TOPIC, 0);
				else
					DisplayTextFile(hMain, g_helpInfo[i].lpFile);
				return FALSE;
			}
		}
		break;
	}

	return FALSE;
}

static void LoadBackgroundBitmap(void)
{
	HGLOBAL hDIBbg;
	LPCWSTR	pFileName = 0;

	if (hBackground)
	{
		DeleteObject(hBackground);
		hBackground = 0;
	}

	if (hPALbg)
	{
		DeleteObject(hPALbg);
		hPALbg = 0;
	}

	/* Pick images based on number of colors avaliable. */
	if (GetDepth(hwndList) <= 8)
	{
		pFileName = TEXT("bkgnd16");
		/*nResource = IDB_BKGROUND16;*/
	}
	else
	{
		pFileName = TEXT("bkground");
		/*nResource = IDB_BKGROUND;*/
	}

	if (LoadDIB(pFileName, &hDIBbg, &hPALbg, BACKGROUND))
	{
		HDC hDC = GetDC(hwndList);
		hBackground = DIBToDDB(hDC, hDIBbg, &bmDesc);
		GlobalFree(hDIBbg);
		ReleaseDC(hwndList, hDC);
	}
}

static void ResetColumnDisplay(BOOL first_time)
{
	int driver_index;

	if (!first_time)
		Picker_ResetColumnDisplay(hwndList);

	ResetListView();

	driver_index = GetGameNameIndex(GetDefaultGame());
	Picker_SetSelectedItem(hwndList, driver_index);
}

int GamePicker_GetItemImage(HWND hwndPicker, int nItem)
{
	return GetIconForDriver(nItem);
}

int GamePicker_GetUseBrokenColor(void)
{
	return !GetUseBrokenIcon();
}

const WCHAR *GamePicker_GetItemString(HWND hwndPicker, int nItem, int nColumn)
{
	const WCHAR *s = TEXT("");

	switch(nColumn)
	{
		case COLUMN_GAMES:
			/* Driver description */
			s = UseLangList() ?
				_LSTW(driversw[nItem]->description):
				driversw[nItem]->modify_the;
			break;

		case COLUMN_ROMS:
			/* Has Roms */
			s = GetAuditString(GetRomAuditResults(nItem));
			break;

		case COLUMN_SAMPLES:
			/* Samples */
			if (DriverUsesSamples(nItem))
				s = GetAuditString(GetSampleAuditResults(nItem));
			break;

		case COLUMN_DIRECTORY:
			/* Driver name (directory) */
			s = driversw[nItem]->name;
			break;

		case COLUMN_SRCDRIVERS:
			/* Source drivers */
			s = GetDriverFilename(nItem);
			break;

		case COLUMN_PLAYTIME:
			/* total play time */
			{
				static WCHAR buf[100];

				GetTextPlayTime(nItem, buf);
				s = buf;
			}
			break;

		case COLUMN_TYPE:
		{
			machine_config drv;
			expand_machine_driver(drivers[nItem]->drv,&drv);

			/* Vector/Raster */
			if (drv.video_attributes & VIDEO_TYPE_VECTOR)
				s = _UIW(TEXT("Vector"));
			else
				s = _UIW(TEXT("Raster"));
			break;
		}
		case COLUMN_TRACKBALL:
			/* Trackball */
			if (DriverUsesTrackball(nItem))
				s = _UIW(TEXT("Yes"));
			else
				s = _UIW(TEXT("No"));
			break;

		case COLUMN_PLAYED:
			/* times played */
			{
				static WCHAR buf[100];

				swprintf(buf, TEXT("%i"), GetPlayCount(nItem));
				s = buf;
			}
			break;

		case COLUMN_MANUFACTURER:
			/* Manufacturer */
			if (UseLangList())
				s = _MANUFACTW(driversw[nItem]->manufacturer);
			else
				s = driversw[nItem]->manufacturer;
			break;

		case COLUMN_YEAR:
			/* Year */
			s = driversw[nItem]->year;
			break;

		case COLUMN_CLONE:
			s = GetCloneParentName(nItem);
			break;
	}
	return s;
}

static void GamePicker_LeavingItem(HWND hwndPicker, int nItem)
{
	// leaving item
	// printf("leaving %s\n",drivers[nItem]->name);
#ifdef MESS
	MessWriteMountedSoftware(nItem);
#endif	
}

static void GamePicker_EnteringItem(HWND hwndPicker, int nItem)
{
	// printf("entering %s\n",drivers[nItem]->name);
	if (g_bDoBroadcast == TRUE)
	{
		ATOM a = GlobalAddAtom(driversw[nItem]->description);
		SendMessage(HWND_BROADCAST, g_mame32_message, a, a);
		GlobalDeleteAtom(a);
	}

	EnableSelection(nItem);
#ifdef MESS
	MessReadMountedSoftware(nItem);
#endif
}

static int GamePicker_FindItemParent(HWND hwndPicker, int nItem)
{
#if defined(KAILLERA) || defined(MAME32PLUSPLUS)
	int DriverParentIndex2(int driver_index);
	return DriverParentIndex2(nItem);
#else
	return DriverParentIndex(nItem);
#endif
}

static int GamePicker_CheckItemBroken(HWND hwndPicker, int nItem)
{
	return DriverIsBroken(nItem);
}

/* Initialize the Picker and List controls */
static void InitListView(void)
{
	LVBKIMAGEW bki;
	WCHAR path[MAX_PATH];

	static const struct PickerCallbacks s_gameListCallbacks =
	{
		DoSortColumn,			/* pfnSetSortColumn */
		GetSortColumn,			/* pfnGetSortColumn */
		SetSortReverse,			/* pfnSetSortReverse */
		GetSortReverse,			/* pfnGetSortReverse */
		SetViewMode,			/* pfnSetViewMode */
		GetViewMode,			/* pfnGetViewMode */
		SetColumnWidths,		/* pfnSetColumnWidths */
		GetColumnWidths,		/* pfnGetColumnWidths */
		SetColumnOrder,			/* pfnSetColumnOrder */
		GetColumnOrder,			/* pfnGetColumnOrder */
		SetColumnShown,			/* pfnSetColumnShown */
		GetColumnShown,			/* pfnGetColumnShown */
		GetOffsetClones,		/* pfnGetOffsetChildren */
		GamePicker_GetUseBrokenColor,	/* pfnGetUseBrokenColor */

		GamePicker_Compare,		/* pfnCompare */
		MamePlayGame,			/* pfnDoubleClick */
		GamePicker_GetItemString,	/* pfnGetItemString */
		GamePicker_GetItemImage,	/* pfnGetItemImage */
		GamePicker_LeavingItem,		/* pfnLeavingItem */
		GamePicker_EnteringItem,	/* pfnEnteringItem */
		BeginListViewDrag,		/* pfnBeginListViewDrag */
		GamePicker_FindItemParent,	/* pfnFindItemParent */
		GamePicker_CheckItemBroken,	/* pfnCheckItemBroken */
		OnIdle,				/* pfnIdle */
		GamePicker_OnHeaderContextMenu,	/* pfnOnHeaderContextMenu */
		GamePicker_OnBodyContextMenu	/* pfnOnBodyContextMenu */
	};

	struct PickerOptions opts;

	// subclass the list view
	memset(&opts, 0, sizeof(opts));
	opts.pCallbacks = &s_gameListCallbacks;
	opts.nColumnCount = COLUMN_MAX;
	opts.ppszColumnNames = column_names;
	SetupPicker(hwndList, &opts);

	ListView_SetTextBkColor(hwndList, CLR_NONE);
	ListView_SetBkColor(hwndList, CLR_NONE);
	swprintf(path, TEXT("%s\\bkground"), GetBgDir());
	bki.ulFlags = LVBKIF_SOURCE_URL | LVBKIF_STYLE_TILE;
	bki.pszImage = path;
	if( hBackground )	
		ListView_SetBkImageA(hwndList, &bki);

	CreateIcons();

	ResetColumnDisplay(TRUE);

	// Allow selection to change the default saved game
	bListReady = TRUE;
}

static void AddDriverIcon(int nItem,int default_icon_index)
{
	HICON hIcon = 0;
	int nParentIndex = GetParentIndex(drivers[nItem]);
	char* game_name = (char *)drivers[nItem]->name;

	/* if already set to rom or clone icon, we've been here before */
	if (icon_index[nItem] == 1 || icon_index[nItem] == 3)
		return;

	hIcon = LoadIconFromFile(game_name);
	
	if (hIcon == NULL)
	{
		if (strstr(game_name, "g_") == game_name)
			hIcon = LoadIconFromFile("g_games");
		else if (strstr(game_name, "gg_") == game_name)
			hIcon = LoadIconFromFile("gg_games");
		else if (strstr(game_name, "s_") == game_name)
			hIcon = LoadIconFromFile("s_games");
	}
	
	if (hIcon == NULL && nParentIndex >= 0)
	{
		hIcon = LoadIconFromFile((char *)drivers[nItem]->parent);
		nParentIndex = GetParentIndex(drivers[nParentIndex]);
		if (hIcon == NULL && nParentIndex >= 0)
			hIcon = LoadIconFromFile((char *)drivers[nParentIndex]->parent);
	}

	if (hIcon != NULL)
	{
		int nIconPos = ImageList_AddIcon(hSmall, hIcon);
		ImageList_AddIcon(hLarge, hIcon);
		if (nIconPos != -1)
			icon_index[nItem] = nIconPos;
	}
	if (icon_index[nItem] == 0)
		icon_index[nItem] = default_icon_index;
}

static void DestroyIcons(void)
{
	if (hSmall != NULL)
	{
		//FIXME: ImageList_Destroy(hSmall);
		hSmall = NULL;
	}

	if (icon_index != NULL)
	{
		int i;
		for (i=0;i<game_count;i++)
			icon_index[i] = 0; // these are indices into hSmall
	}

	if (hLarge != NULL)
	{
		//FIXME: ImageList_Destroy(hLarge);
		hLarge = NULL;
	}

	if (hHeaderImages != NULL)
	{
		//FIXME: ImageList_Destroy(hHeaderImages);
		hHeaderImages = NULL;
	}

}

static void ReloadIcons(void)
{
	HICON hIcon;
	INT i;

	// clear out all the images
	ImageList_Remove(hSmall,-1);
	ImageList_Remove(hLarge,-1);

	if (icon_index != NULL)
	{
		for (i=0;i<game_count;i++)
			icon_index[i] = 0; // these are indices into hSmall
	}

	for (i = 0; g_iconData[i].icon_name; i++)
	{
		hIcon = LoadIconFromFile((char *) g_iconData[i].icon_name);
		if (hIcon == NULL)
			hIcon = LoadIcon(hInst, MAKEINTRESOURCE(g_iconData[i].resource));

		ImageList_AddIcon(hSmall, hIcon);
		ImageList_AddIcon(hLarge, hIcon);
	}
}

static DWORD GetShellLargeIconSize(void)
{
	DWORD  dwSize, dwLength = 512, dwType = REG_SZ;
	char   szBuffer[512];
	HKEY   hKey;

	/* Get the Key */
	RegOpenKeyA(HKEY_CURRENT_USER, "Control Panel\\desktop\\WindowMetrics", &hKey);
	/* Save the last size */
	RegQueryValueExA(hKey, "Shell Icon Size", NULL, &dwType, (LPBYTE)szBuffer, &dwLength);
	dwSize = atol(szBuffer);
	if (dwSize < 32)
		dwSize = 32;

	if (dwSize > 48)
		dwSize = 48;

	/* Clean up */
	RegCloseKey(hKey);
	return dwSize;
}

static DWORD GetShellSmallIconSize(void)
{
	DWORD dwSize = ICONMAP_WIDTH;

	if (dwSize < 48)
	{
		if (dwSize < 32)
			dwSize = 16;
		else
			dwSize = 32;
	}
	else
	{
		dwSize = 48;
	}
	return dwSize;
}

// create iconlist for Listview control
static void CreateIcons(void)
{
	DWORD dwLargeIconSize = GetShellLargeIconSize();
	HICON hIcon;
	int icon_count;
	DWORD dwStyle;

	icon_count = 0;
	while(g_iconData[icon_count].icon_name)
		icon_count++;

	// the current window style affects the sizing of the rows when changing
	// between list views, so put it in small icon mode temporarily while we associate
	// our image list
	//
	// using large icon mode instead kills the horizontal scrollbar when doing
	// full refresh, which seems odd (it should recreate the scrollbar when
	// set back to report mode, for example, but it doesn't).

	dwStyle = GetWindowLong(hwndList,GWL_STYLE);
	SetWindowLong(hwndList,GWL_STYLE,(dwStyle & ~LVS_TYPEMASK) | LVS_ICON);

	hSmall = ImageList_Create(GetShellSmallIconSize(),GetShellSmallIconSize(),
	                          ILC_COLORDDB | ILC_MASK, icon_count, 500);
	hLarge = ImageList_Create(dwLargeIconSize, dwLargeIconSize,
	                          ILC_COLORDDB | ILC_MASK, icon_count, 500);

	ReloadIcons();

	// Associate the image lists with the list view control.
	ListView_SetImageList(hwndList, hSmall, LVSIL_SMALL);
	ListView_SetImageList(hwndList, hLarge, LVSIL_NORMAL);

	// restore our view
	SetWindowLong(hwndList,GWL_STYLE,dwStyle);

	// Now set up header specific stuff
	hHeaderImages = ImageList_Create(16,16,ILC_COLORDDB | ILC_MASK,2,2);
	hIcon = LoadIcon(hInst,MAKEINTRESOURCE(IDI_HEADER_UP));
	ImageList_AddIcon(hHeaderImages,hIcon);
	hIcon = LoadIcon(hInst,MAKEINTRESOURCE(IDI_HEADER_DOWN));
	ImageList_AddIcon(hHeaderImages,hIcon);

	Picker_SetHeaderImageList(hwndList, hHeaderImages);
}



static int GamePicker_Compare(HWND hwndPicker, int index1, int index2, int sort_subitem)
{
	int value;
	const WCHAR *name1 = NULL;
	const WCHAR *name2 = NULL;
	int nTemp1, nTemp2;

	machine_config drv1;
	machine_config drv2;
	expand_machine_driver(drivers[index1]->drv, &drv1);
	expand_machine_driver(drivers[index2]->drv, &drv2);

#ifdef DEBUG
	if (strcmp(drivers[index1]->name,"1941") == 0 && strcmp(drivers[index2]->name,"1942") == 0)
	{
		dprintf("comparing 1941, 1942");
	}

	if (strcmp(drivers[index1]->name,"1942") == 0 && strcmp(drivers[index2]->name,"1941") == 0)
	{
		dprintf("comparing 1942, 1941");
	}
#endif

	switch (sort_subitem)
	{
	case COLUMN_GAMES:
		value = 0;

		if (UseLangList())
			value = sort_index[index1].readings - sort_index[index2].readings;

		if (value == 0)
			value = sort_index[index1].description - sort_index[index2].description;

		break;

	case COLUMN_ROMS:
		nTemp1 = GetRomAuditResults(index1);
		nTemp2 = GetRomAuditResults(index2);

		if (IsAuditResultKnown(nTemp1) == FALSE && IsAuditResultKnown(nTemp2) == FALSE)
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		if (IsAuditResultKnown(nTemp1) == FALSE)
		{
			value = 1;
			break;
		}

		if (IsAuditResultKnown(nTemp2) == FALSE)
		{
			value = -1;
			break;
		}

		// ok, both are known

		if (IsAuditResultYes(nTemp1) && IsAuditResultYes(nTemp2))
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);
		
		if (IsAuditResultNo(nTemp1) && IsAuditResultNo(nTemp2))
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		if (IsAuditResultYes(nTemp1) && IsAuditResultNo(nTemp2))
			value = -1;
		else
			value = 1;
		break;

	case COLUMN_SAMPLES:
		nTemp1 = -1;
		if (DriverUsesSamples(index1))
		{
			int audit_result = GetSampleAuditResults(index1);
			if (IsAuditResultKnown(audit_result))
			{
				if (IsAuditResultYes(audit_result))
					nTemp1 = 1;
				else 
					nTemp1 = 0;
			}
			else
				nTemp1 = 2;
		}

		nTemp2 = -1;
		if (DriverUsesSamples(index2))
		{
			int audit_result = GetSampleAuditResults(index2);
			if (IsAuditResultKnown(audit_result))
			{
				if (IsAuditResultYes(audit_result))
					nTemp2 = 1;
				else 
					nTemp2 = 0;
			}
			else
				nTemp2 = 2;
		}

		if (nTemp1 == nTemp2)
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		value = nTemp2 - nTemp1;
		break;

	case COLUMN_DIRECTORY:
		value = stricmp(drivers[index1]->name, drivers[index2]->name);
		break;

	case COLUMN_SRCDRIVERS:
		value = wcscmpi(GetDriverFilename(index1), GetDriverFilename(index2));
		if (value == 0)
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		break;

	case COLUMN_PLAYTIME:
		value = GetPlayTime(index1) - GetPlayTime(index2);
		if (value == 0)
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		break;

	case COLUMN_TYPE:
	{
		if ((drv1.video_attributes & VIDEO_TYPE_VECTOR) ==
			(drv2.video_attributes & VIDEO_TYPE_VECTOR))
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		value = (drv1.video_attributes & VIDEO_TYPE_VECTOR) -
				(drv2.video_attributes & VIDEO_TYPE_VECTOR);
		break;
	}
	case COLUMN_TRACKBALL:
		if (DriverUsesTrackball(index1) == DriverUsesTrackball(index2))
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		value = DriverUsesTrackball(index1) - DriverUsesTrackball(index2);
		break;

	case COLUMN_PLAYED:
		value = GetPlayCount(index1) - GetPlayCount(index2);
		if (value == 0)
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		break;

	case COLUMN_MANUFACTURER:
		if (stricmp(drivers[index1]->manufacturer, drivers[index2]->manufacturer) == 0)
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		value = stricmp(drivers[index1]->manufacturer, drivers[index2]->manufacturer);
		break;

	case COLUMN_YEAR:
		if (stricmp(drivers[index1]->year, drivers[index2]->year) == 0)
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		value = stricmp(drivers[index1]->year, drivers[index2]->year);
		break;

	case COLUMN_CLONE:
		name1 = GetCloneParentName(index1);
		name2 = GetCloneParentName(index2);

		if (*name1 == '\0')
			name1 = NULL;
		if (*name2 == '\0')
			name2 = NULL;

		if (name1 == name2)
			return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);

		if (name2 == NULL)
			value = -1;
		else if (name1 == NULL)
			value = 1;
		else
			value = wcscmpi(name1, name2);
		break;

	default :
		return GamePicker_Compare(hwndPicker, index1, index2, COLUMN_GAMES);
	}

#ifdef DEBUG
	if ((strcmp(drivers[index1]->name,"1941") == 0 && strcmp(drivers[index2]->name,"1942") == 0) ||
		(strcmp(drivers[index1]->name,"1942") == 0 && strcmp(drivers[index2]->name,"1941") == 0))
		dprintf("result: %i",value);
#endif

	return value;
}

static int GetSelectedPick()
{
	/* returns index of listview selected item */
	/* This will return -1 if not found */
	return ListView_GetNextItem(hwndList, -1, LVIS_SELECTED | LVIS_FOCUSED);
}

static HICON GetSelectedPickItemIcon()
{
	LV_ITEM lvi;

	lvi.iItem = GetSelectedPick();
	lvi.iSubItem = 0;
	lvi.mask     = LVIF_IMAGE;
	ListView_GetItem(hwndList, &lvi);

	return ImageList_GetIcon(hLarge, lvi.iImage, ILD_TRANSPARENT);
}

static void SetRandomPickItem()
{
	int nListCount;

	nListCount = ListView_GetItemCount(hwndList);

	if (nListCount > 0)
	{
		Picker_SetSelectedPick(hwndList, rand() % nListCount);
	}
}

static const WCHAR *GetLastDir(void)
{
	return last_directory;
}

static BOOL CommonFileDialogW(BOOL open_for_write, WCHAR *filename, int filetype)
{
	BOOL success;

	OPENFILENAMEW of;
	common_file_dialog_procW cfd;
	WCHAR fn[MAX_PATH];
	WCHAR *p, buf[256];
	const WCHAR *s = NULL;
	WCHAR dir[256];
	WCHAR title[256];
	WCHAR ext[256];

	wcscpy(fn, filename);

	of.lStructSize       = sizeof(of);
	of.hwndOwner         = hMain;
	of.hInstance         = NULL;

	of.lpstrInitialDir   = NULL;
	of.lpstrTitle        = NULL;
	of.lpstrDefExt       = NULL;
	of.Flags             = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

	if (filetype == FILETYPE_GAMELIST_FILES)
		of.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	if (open_for_write)
	{
		cfd = GetSaveFileNameW;

		if (cfg_data[filetype].title_save)
		{
			wcscpy(title, _UIW(cfg_data[filetype].title_save));
			of.lpstrTitle = title;
		}
	}
	else
	{
		cfd = GetOpenFileNameW;
		of.Flags |= OFN_FILEMUSTEXIST;

		if (cfg_data[filetype].title_load)
		{
			wcscpy(title, _UIW(cfg_data[filetype].title_load));
			of.lpstrTitle = title;
		}
	}

	if (cfg_data[filetype].dir)
	{
		wcscpy(dir, cfg_data[filetype].dir());
		of.lpstrInitialDir = dir;
	}

	if (cfg_data[filetype].ext)
	{
		wcscpy(ext, cfg_data[filetype].ext);
		of.lpstrDefExt = ext;
	}

	s = cfg_data[filetype].filter;
	for (p = buf; *s; s += wcslen(s) + 1)
	{
		wcscpy(p, _UIW(s));
		p += wcslen(p) + 1;
	}
	*p = '\0';

	of.lpstrFilter       = buf;
	of.lpstrCustomFilter = NULL;
	of.nMaxCustFilter    = 0;
	of.nFilterIndex      = 1;
	of.lpstrFile         = fn;
	of.nMaxFile          = MAX_PATH;
	of.lpstrFileTitle    = NULL;
	of.nMaxFileTitle     = 0;
	of.nFileOffset       = 0;
	of.nFileExtension    = 0;
	of.lCustData         = 0;
	of.lpfnHook          = NULL;
	of.lpTemplateName    = NULL;

	success = cfd(&of);
	if (success)
	{
		wcscpy(filename, fn);
	}
	return success;
}

static BOOL CommonFileDialogA(BOOL open_for_write, WCHAR *filename, int filetype)
{
	BOOL success;

	OPENFILENAMEA of;
	common_file_dialog_procA cfd;
	char fn[MAX_PATH];
	char *p, buf[256];
	const WCHAR *s = NULL;
	char dir[256];
	char title[256];
	char ext[256];

	strcpy(fn, _String(filename));

	of.lStructSize       = sizeof(of);
	of.hwndOwner         = hMain;
	of.hInstance         = NULL;

	of.lpstrInitialDir   = NULL;
	of.lpstrTitle        = NULL;
	of.lpstrDefExt       = NULL;
	of.Flags             = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

	if (filetype == FILETYPE_GAMELIST_FILES)
		of.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	if (open_for_write)
	{
		cfd = GetSaveFileNameA;

		if (cfg_data[filetype].title_save)
		{
			strcpy(title, _String(_UIW(cfg_data[filetype].title_save)));
			of.lpstrTitle = title;
		}
	}
	else
	{
		cfd = GetOpenFileNameA;
		of.Flags |= OFN_FILEMUSTEXIST;

		if (cfg_data[filetype].title_load)
		{
			strcpy(title, _String(_UIW(cfg_data[filetype].title_load)));
			of.lpstrTitle = title;
		}
	}

	if (cfg_data[filetype].dir)
	{
		strcpy(dir, _String(cfg_data[filetype].dir()));
		of.lpstrInitialDir = dir;
	}

	if (cfg_data[filetype].ext)
	{
		strcpy(ext, _String(cfg_data[filetype].ext));
		of.lpstrDefExt = ext;
	}

	s = cfg_data[filetype].filter;
	for (p = buf; *s; s += wcslen(s) + 1)
	{
		strcpy(p, _String(_UIW(s)));
		p += strlen(p) + 1;
	}
	*p = '\0';

	of.lpstrFilter       = buf;
	of.lpstrCustomFilter = NULL;
	of.nMaxCustFilter    = 0;
	of.nFilterIndex      = 1;
	of.lpstrFile         = fn;
	of.nMaxFile          = MAX_PATH;
	of.lpstrFileTitle    = NULL;
	of.nMaxFileTitle     = 0;
	of.nFileOffset       = 0;
	of.nFileExtension    = 0;
	of.lCustData         = 0;
	of.lpfnHook          = NULL;
	of.lpTemplateName    = NULL;

	success = cfd(&of);
	if (success)
		wcscpy(filename, _Unicode(fn));

	return success;
}

static BOOL CommonFileDialog(BOOL open_for_write, WCHAR *filename, int filetype)
{
	if (OnNT())
		return CommonFileDialogW(open_for_write, filename, filetype);
	else
		return CommonFileDialogA(open_for_write, filename, filetype);
}

#if 0
void SetStatusBarText(int part_index, const char *message)
{
	StatusBarSetTextA(hStatusBar, part_index, message);
}

void SetStatusBarTextF(int part_index, const char *fmt, ...)
{
	char buf[256];
	va_list va;

	va_start(va, fmt);
	vsprintf(buf, fmt, va);
	va_end(va);

	SetStatusBarText(part_index, buf);
}

#endif

void SetStatusBarTextW(int part_index, const WCHAR *message)
{
	StatusBarSetTextW(hStatusBar, part_index, message);
}

void SetStatusBarTextFW(int part_index, const WCHAR *fmt, ...)
{
	WCHAR buf[256];
	va_list va;

	va_start(va, fmt);
	vswprintf(buf, fmt, va);
	va_end(va);

	SetStatusBarTextW(part_index, buf);
}

static void MameMessageBoxUTF8(const char *fmt, ...)
{
	char buf[2048];
	va_list va;

	va_start(va, fmt);
	vsprintf(buf, fmt, va);
	MessageBox(GetMainWindow(), _UTF8Unicode(buf), TEXT_MAME32NAME, MB_OK | MB_ICONERROR);
	va_end(va);
}

#ifdef KAILLERA
static void CopyTrctempStateSaveFile(const WCHAR *fname, inpsub_header *inpsub_header)
{
	file_error filerr;
	mame_file *file_inp, *file_trctemp;
	WCHAR path[_MAX_PATH];
	WCHAR name[_MAX_PATH];
	char *stemp;
	void *buf;
	int fsize,i;
	// 使用するstateファイルをtrctempにコピー
	MKInpDir();
	//_splitpath(Trace_filename, NULL, NULL, fname, NULL);
	trctemp_statesave_file_size = 0;
	memset(trctemp_statesave_file, 0, sizeof(trctemp_statesave_file));
	for(i=0; i<256; i++)
	{
		char fex[2];
		//struct stat s;
		fex[1] = 0;
		if (inpsub_header->usestate[i] == 0) break;

		fex[0] = inpsub_header->usestate[i];
		trctemp_statesave_file[trctemp_statesave_file_size++] = inpsub_header->usestate[i];
		if (trctemp_statesave_file_size>256) trctemp_statesave_file_size = 256;
		wsprintf(name, TEXT("%s-%s.sta"), fname, _Unicode(fex));
		//sprintf(path, "%s\\trctemp", GetInpDir());
		wcscpy(path, TEXT("inp\\trctemp"));
		set_core_state_directory(path);
		stemp = utf8_from_wstring(name);
		filerr = mame_fopen(SEARCHPATH_STATE, stemp, OPEN_FLAG_READ, &file_inp);
		set_core_state_directory(GetStateDir());

		if (!file_inp) 
			delete_file(_String(name));
		else
		{
			filerr = mame_fopen(SEARCHPATH_STATE, stemp, OPEN_FLAG_READ, &file_trctemp);

			fsize = mame_fsize(file_inp);
			buf = malloc(fsize);
			mame_fread(file_inp, buf, fsize);
			mame_fwrite(file_trctemp, buf, fsize);
			free(buf);
			mame_fclose(file_inp);
			mame_fclose(file_trctemp);
		}
		free(stemp);
	}
}

static void DeleteTrctempStateSaveFile(WCHAR *fname)
{
	WCHAR name[_MAX_PATH];
	int          i;
	// trctempにコピーしたstateファイルを削除。
	for(i=0; i<trctemp_statesave_file_size; i++)
	{
		char fex[2];
		fex[1] = 0;
		if (trctemp_statesave_file[i] == 0) break;
		fex[0] = trctemp_statesave_file[i];
		//sprintf(name, "%s\\trctemp\\%s-%s.sta", GetInpDir(), fname, fex);
		wsprintf(name, TEXT("inp\\trctemp\\%s-%s.sta"), fname, _Unicode(fex));
		delete_file(_String(name));
	}
}

static int MamePlayBackTrace(const WCHAR *filename, inpsub_header *inpsub_header)
{
	{
		int r=0;
		char ver[256];   /* mame version */

		if ( strcmp(inpsub_header->str, "EMMAMETRACE")) {r=1; goto ext;}

		if (inpsub_header->trcversion != INPUTLOG_TRACE_VERSION) {r=2; goto ext;}

		sprintf(ver,"%s %s", MAME32NAME "++", build_version);
		if ( strcmp(ver, inpsub_header->version)) {r=3;}
		
		if (inpsub_header->playcount != 0 || mame32_PlayGameCount != 0) {r=3;}

		//r=1;
ext:
		switch (r)
		{
			WCHAR buf[1024];
		case 1:

			wsprintf(buf, _UIW(TEXT("Could not open '%s' as a valid trace file.")), filename);
			MameMessageBoxW(buf);
			return 0;
		case 2:

			wsprintf(buf,   _UIW(TEXT("trc file version is different\ntrc version %d\nmame version %s\nPlay Count %d\n"))
							, (int)inpsub_header->trcversion, _Unicode(inpsub_header->version), (int)inpsub_header->playcount+1);
			MameMessageBoxW(buf);
			return 0;
		case 3:
			wsprintf(buf,   _UIW(TEXT("Since environment differs from the time of creation, it may not reappear well.\nIs replay reproduction carried out?\ntrcversion %d\nmame version %s\nPlay Count %d\n"))
							, (int)inpsub_header->trcversion, _Unicode(inpsub_header->version), (int)inpsub_header->playcount+1);
			if( MessageBox(hMain, buf, TEXT_MAME32NAME TEXT("++"), MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
			{
				return 0;
			}
			break;
		}
	}
	return 2;
}
#endif /* KAILLERA */

static void MameMessageBoxW(const WCHAR *fmt, ...)
{
	WCHAR buf[2048];
	va_list va;

	va_start(va, fmt);
	vswprintf(buf, fmt, va);
#ifdef KAILLERA
	MessageBox(GetMainWindow(), buf, TEXT_MAME32NAME TEXT("++"), MB_OK | MB_ICONERROR);
#else
	MessageBox(GetMainWindow(), buf, TEXT_MAME32NAME, MB_OK | MB_ICONERROR);
#endif /* KAILLERA */
	va_end(va);
}

static void MameMessageBoxI(const WCHAR *fmt, ...)
{
	WCHAR buf[2048];
	va_list va;

	va_start(va, fmt);
	vswprintf(buf, fmt, va);
#ifdef KAILLERA
	MessageBox(GetMainWindow(), buf, TEXT_MAME32NAME TEXT("++"), MB_OK | MB_ICONINFORMATION);
#else
	MessageBox(GetMainWindow(), buf, TEXT_MAME32NAME, MB_OK | MB_ICONINFORMATION);
#endif /* KAILLERA */
	va_end(va);
}

static void MamePlayBackGame(const WCHAR *fname_playback)
{
	int nGame = -1;
	WCHAR filename[MAX_PATH];

	if (fname_playback)
	{
		wcscpy(filename, fname_playback);
	}
	else
	{
		*filename = 0;

		nGame = Picker_GetSelectedItem(hwndList);
		if (nGame != -1)
			wcscpy(filename, driversw[nGame]->name);

		if (!CommonFileDialog(FALSE, filename, FILETYPE_INPUT_FILES)) return;
	}
	
	if (*filename)
	{
		mame_file* pPlayBack;
#ifdef KAILLERA
		mame_file* pPlayBackSub;
#endif /* KAILEERA */
		file_error filerr;
		WCHAR drive[_MAX_DRIVE];
		WCHAR dir[_MAX_DIR];
		WCHAR bare_fname[_MAX_FNAME];
		WCHAR ext[_MAX_EXT];

		WCHAR path[MAX_PATH];
		WCHAR fname[MAX_PATH];
#ifdef KAILLERA
		WCHAR fname2[MAX_PATH];
#endif /* KAILEERA */
		char *stemp;

		_wsplitpath(filename, drive, dir, bare_fname, ext);

		wcscpy(path, drive);
		wcscat(path, dir);
		wcscpy(fname, bare_fname);
		wcscat(fname, TEXT(".inp"));
		if (path[wcslen(path)-1] == TEXT(PATH_SEPARATOR[0]))
			path[wcslen(path)-1] = 0; // take off trailing back slash

		set_core_input_directory(path);
		stemp = utf8_from_wstring(fname);
		filerr = mame_fopen_options(get_core_options(), SEARCHPATH_INPUTLOG, stemp, OPEN_FLAG_READ, &pPlayBack);
		free(stemp);
		set_core_input_directory(GetInpDir());

		if (filerr != FILERR_NONE)
		{
			MameMessageBoxW(_UIW(TEXT("Could not open '%s' as a valid input file.")), filename);
			return;
		}

		// check for game name embedded in .inp header
		if (pPlayBack)
		{
			inp_header inp_header;

#ifdef USE_WOLFMAME_XINP
			{
				inpext_header xheader;
				// read first four bytes to check INP type
				mame_fread(pPlayBack, xheader.header, 7);
				mame_fseek(pPlayBack, 0, SEEK_SET);
				if(strncmp(xheader.header,"XINP\0\0\0",7) != 0)
				{
					// read playback header
					mame_fread(pPlayBack, &inp_header, sizeof(inp_header));
				} else {
					// read header
					mame_fread(pPlayBack, &xheader, sizeof(inpext_header));

					memcpy(inp_header.name, xheader.shortname, sizeof(inp_header.name));
				}
			}
#else
			// read playback header
			mame_fread(pPlayBack, &inp_header, sizeof(inp_header));
#endif /* USE_WOLFMAME_XINP */

			if (!isalnum(inp_header.name[0])) // If first byte is not alpha-numeric
				mame_fseek(pPlayBack, 0, SEEK_SET); // old .inp file - no header
			else
			{
				int i;
				for (i = 0; drivers[i] != 0; i++) // find game and play it
				{
					if (strcmp(drivers[i]->name, inp_header.name) == 0)
					{
						nGame = i;
						break;
					}
				}
			}
		}
		mame_fclose(pPlayBack);

#ifdef KAILLERA
		wsprintf(fname2, TEXT("%s.trc"), bare_fname);
		g_pPlayBkSubName = NULL;
		if ((!wcscmp(ext, TEXT(".zip"))) || (!wcscmp(ext, TEXT(".trc"))))
		{
			set_core_input_directory(path);
			stemp = utf8_from_wstring(fname2);
			filerr = mame_fopen(SEARCHPATH_INPUTLOG, stemp, OPEN_FLAG_READ, &pPlayBackSub);
			free(stemp);
			set_core_input_directory(GetInpDir());
			if (filerr == FILERR_NONE)
			{
				inpsub_header inpsub_header;
	
				// read playbacksub header
				mame_fread(pPlayBackSub, &inpsub_header, sizeof(inpsub_header));
				mame_fclose(pPlayBackSub);
				//wsprintf(Trace_filename, TEXT("%s\\%s"), path, fname2);
	
				if (MamePlayBackTrace(fname2, &inpsub_header) == 2)
				{
					g_pPlayBkSubName = fname2;
					CopyTrctempStateSaveFile(fname2, &inpsub_header);
				}
				else
				{
					g_pPlayBkSubName = NULL;
					g_pPlayBkName = NULL;
					override_playback_directory = NULL;
					return;
				}
			}
		}
#endif /* KAILLERA */

		g_pPlayBkName = fname;
		override_playback_directory = path;
		MamePlayGameWithOptions(nGame);
#ifdef KAILLERA
		if (g_pPlayBkSubName != NULL)
			DeleteTrctempStateSaveFile(g_pPlayBkSubName);
		g_pPlayBkSubName = NULL;
#endif /* KAILLERA */
		g_pPlayBkName = NULL;
		override_playback_directory = NULL;
	}
}

static void MameLoadState(const WCHAR *fname_state)
{
	int nGame = -1;
	WCHAR filename[MAX_PATH];
	WCHAR selected_filename[MAX_PATH];

	if (fname_state)
	{
		WCHAR *cPos=0;
		int  iPos=0;
		int  i;
		WCHAR bare_fname[_MAX_FNAME];

		wcscpy(filename, fname_state);

		_wsplitpath(fname_state, NULL, NULL, bare_fname, NULL);
		cPos = wcschr(bare_fname, TEXT('-'));
		iPos = cPos ? cPos - bare_fname : wcslen(bare_fname);
		wcsncpy(selected_filename, bare_fname, iPos );
		selected_filename[iPos] = '\0';

		for (i = 0; drivers[i] != 0; i++) // find game and play it
			if (!wcscmp(driversw[i]->name, selected_filename))
			{
				nGame = i;
				break;
			}
		if (nGame == -1)
		{
			MameMessageBoxW(_UIW(TEXT("Could not open '%s' as a valid savestate file.")), filename);
			return;
		}
	}
	else
	{
		*filename = 0;

		nGame = Picker_GetSelectedItem(hwndList);
		if (nGame != -1)
		{
			wcscpy(filename, driversw[nGame]->name);
			wcscpy(selected_filename, filename);
		}
		if (!CommonFileDialog(FALSE, filename, FILETYPE_SAVESTATE_FILES)) return;
	}

	if (*filename)
	{
		mame_file* pSaveState;
		file_error filerr;
		WCHAR drive[_MAX_DRIVE];
		WCHAR dir[_MAX_DIR];
		WCHAR ext[_MAX_EXT];

		WCHAR path[MAX_PATH];
		WCHAR fname[MAX_PATH];
		WCHAR bare_fname[_MAX_FNAME];
		WCHAR *state_fname;
		char *stemp;
		int rc;

		_wsplitpath(filename, drive, dir, bare_fname, ext);

		// parse path
		wcscpy(path, drive);
		wcscat(path, dir);
		wcscpy(fname, bare_fname);
		wcscat(fname, TEXT(".sta"));
		if (path[wcslen(path)-1] == TEXT(PATH_SEPARATOR[0]))
			path[wcslen(path)-1] = 0; // take off trailing back slash

#ifdef MESS
		{
			state_fname = filename;
			return;
		}
#else // !MESS
		{
			WCHAR *cPos=0;
			int  iPos=0;
			WCHAR romname[MAX_PATH];

			cPos = wcschr(bare_fname, '-' );
			iPos = cPos ? cPos - bare_fname : wcslen(bare_fname);
			wcsncpy(romname, bare_fname, iPos );
			romname[iPos] = '\0';
			if (wcscmp(selected_filename,romname) != 0)
			{
				MameMessageBoxW(_UIW(TEXT("'%s' is not a valid savestate file for game '%s'.")), filename, selected_filename);
				return;
			}
			set_core_state_directory(path);
			state_fname = fname;
		}
#endif // MESS

		stemp = utf8_from_wstring(state_fname);
		filerr = mame_fopen_options(get_core_options(), SEARCHPATH_STATE, stemp, OPEN_FLAG_READ, &pSaveState);
		free(stemp);
		set_core_state_directory(GetStateDir());
		if (filerr != FILERR_NONE)
		{
			MameMessageBoxW(_UIW(TEXT("Could not open '%s' as a valid savestate file.")), filename);
			return;
		}

		// call the MAME core function to check the save state file
		stemp = utf8_from_wstring(selected_filename);
		rc = state_save_check_file(pSaveState, stemp, TRUE, MameMessageBoxUTF8);
		free(stemp);
		mame_fclose(pSaveState);
		if (rc)
			return;

#ifdef MESS
		g_pSaveStateName = state_fname;
#else
		g_pSaveStateName = state_fname;
		override_savestate_directory = path;
#endif

		MamePlayGameWithOptions(nGame);
		g_pSaveStateName = NULL;
		override_savestate_directory = NULL;
	}
}

static void MamePlayRecordGame(void)
{
	int  nGame;
	WCHAR filename[MAX_PATH];
	*filename = 0;

	nGame = Picker_GetSelectedItem(hwndList);
	wcscpy(filename, driversw[nGame]->name);

	if (CommonFileDialog(TRUE, filename, FILETYPE_INPUT_FILES))
	{
		WCHAR drive[_MAX_DRIVE];
		WCHAR dir[_MAX_DIR];
		WCHAR bare_fname[_MAX_FNAME];
		WCHAR fname[_MAX_FNAME];
#ifdef KAILLERA
		WCHAR fname2[_MAX_FNAME];
#endif /* KAILEERA */
		WCHAR ext[_MAX_EXT];
		WCHAR path[MAX_PATH];

		_wsplitpath(filename, drive, dir, bare_fname, ext);

		wcscpy(path, drive);
		wcscat(path, dir);
		wcscpy(fname, bare_fname);
		wcscat(fname, TEXT(".inp"));
		if (path[wcslen(path)-1] == TEXT(PATH_SEPARATOR[0]))
			path[wcslen(path)-1] = 0; // take off trailing back slash

		g_pRecordName = fname;
#ifdef KAILLERA
		wsprintf(fname2, TEXT("%s.trc"), bare_fname);
		g_pRecordSubName = fname2;
		//wsprintf(Trace_filename, TEXT("%s\\%s"), path, fname2);
		local_recode_filename[0] = 0;
		g_pAutoRecordName = NULL;
#endif /* KAILEERA */
		override_playback_directory = path;
		MamePlayGameWithOptions(nGame);
#ifdef KAILLERA
		g_pRecordSubName = NULL;
		g_pAutoRecordName = NULL;
#endif /* KAILEERA */
		g_pRecordName = NULL;
		override_playback_directory = NULL;
	}
}

static void MamePlayGame(void)
{
	int nGame;
#ifdef KAILLERA
    WCHAR filename[MAX_PATH];
    WCHAR fname[MAX_PATH];
    WCHAR fname2[MAX_PATH];
	static	int	num_record = 0;
	static char oldname[256];
	BOOL record = GetLocalRecordInput();
	if(HIBYTE(GetAsyncKeyState(VK_SHIFT)) && HIBYTE(GetAsyncKeyState(VK_CONTROL)))
		record = !record;
	oldname[255] = 0;
#endif /* KAILLERA */

	nGame = Picker_GetSelectedItem(hwndList);

	g_pPlayBkName = NULL;
	g_pRecordName = NULL;
#ifdef MAME_AVI
	g_pRecordAVIName = NULL;
#endif /* MAME_AVI */

#ifdef KAILLERA
	override_playback_directory = NULL;
	g_pPlayBkSubName = NULL;
	g_pRecordSubName = NULL;
	g_pAutoRecordName = NULL;
	perform_ui_count = 0;
	if (record)
	{

		if ( strcmp(drivers[nGame]->name, oldname) )
		{
			strcpy(oldname, drivers[nGame]->name);
			num_record = 0;
		}
	
		wsprintf(local_recode_filename, TEXT("n%02d"), num_record);
		wsprintf(filename, TEXT("%s_%s"), _Unicode(drivers[nGame]->name), local_recode_filename);
		wsprintf(fname, TEXT("%s.inp"), filename);
		wsprintf(fname2, TEXT("%s.trc"), filename);

		num_record = (num_record + 1) % 100;
		MKInpDir();
		g_pRecordName = fname;
		g_pRecordSubName = fname2;
		g_pAutoRecordName = local_recode_filename;
	}
#endif /* KAILLERA */

	MamePlayGameWithOptions(nGame);
#ifdef KAILLERA
	g_pAutoRecordName = NULL;
	g_pRecordName = NULL;
	override_playback_directory = NULL;
	g_pRecordSubName = NULL;
#endif /* KAILLERA */
}

static void MamePlayRecordWave(void)
{
	int  nGame;
	WCHAR filename[MAX_PATH];
	*filename = 0;

	nGame = Picker_GetSelectedItem(hwndList);
	wcscpy(filename, driversw[nGame]->name);

	if (CommonFileDialog(TRUE, filename, FILETYPE_WAVE_FILES))
	{
		g_pRecordWaveName = filename;
		MamePlayGameWithOptions(nGame);
		g_pRecordWaveName = NULL;
	}	
}

static void MamePlayRecordMNG(void)
{
	int  nGame;
	WCHAR filename[MAX_PATH];
	*filename = 0;

	nGame = Picker_GetSelectedItem(hwndList);
	wcscpy(filename, driversw[nGame]->name);

	if (CommonFileDialog(TRUE, filename, FILETYPE_MNG_FILES))
	{
		g_pRecordMNGName = filename;
		MamePlayGameWithOptions(nGame);
		g_pRecordMNGName = NULL;
	}	
}

static void MamePlayGameWithOptions(int nGame)
{
	memcpy(&playing_game_options, GetGameOptions(nGame), sizeof(playing_game_options));

	/* Deal with options that can be disabled. */
	EnablePlayOptions(nGame, &playing_game_options);
    
#ifdef KAILLERA
	if( g_pRecordSubName != NULL )
	{
		perform_ui_count = 0;
		perform_ui_statesave_file_size	= 0;
		perform_ui_statesave_file_fp	= 0;
		memset(perform_ui_statesave_file, 0, sizeof(perform_ui_statesave_file));
	}

	mame32_PlayGameCount++;
	if(mame32_PlayGameCount > 255) mame32_PlayGameCount = 255;
#endif /* KAILLERA */

	if (g_pJoyGUI != NULL)
		KillTimer(hMain, JOYGUI_TIMER);
	if (GetCycleScreenshot() > 0)
		KillTimer(hMain, SCREENSHOT_TIMER);

	in_emulation = TRUE;

	if (RunMAME(nGame) == 0)
	{
		IncrementPlayCount(nGame);
		ResetWhichGamesInFolders();
		ListView_RedrawItems(hwndList, GetSelectedPick(), GetSelectedPick());
	}
	else
	{
		ShowWindow(hMain, SW_SHOW);
	}

	in_emulation = FALSE;

	// re-sort if sorting on # of times played
	if (GetSortColumn() == COLUMN_PLAYED
	 || GetSortColumn() == COLUMN_PLAYTIME)
		Picker_Sort(hwndList);

#ifdef KAILLERA
	if( kPlay && bKailleraMAME32WindowHide == TRUE )
	{
		bMAME32WindowShow = FALSE;	//kt
		ShowWindow(hMain, SW_HIDE);
		EnableWindow(hMain, FALSE);
	} else
    {
	bMAME32WindowShow = TRUE;	//kt
#endif /* KAILLERA */
	UpdateStatusBar();

	ShowWindow(hMain, SW_SHOW);
#ifdef KAILLERA
	if (!kPlay)
#endif
	SetFocus(hwndList);
#ifdef KAILLERA
	}
#endif

	if (g_pJoyGUI != NULL)
		SetTimer(hMain, JOYGUI_TIMER, JOYGUI_MS, NULL);
	if (GetCycleScreenshot() > 0)
		SetTimer(hMain, SCREENSHOT_TIMER, GetCycleScreenshot()*1000, NULL); //scale to seconds
}

/* Toggle ScreenShot ON/OFF */
static void ToggleScreenShot(void)
{
	BOOL showScreenShot = GetShowScreenShot();

	SetShowScreenShot((showScreenShot) ? FALSE : TRUE);
	UpdateScreenShot();

	/* Redraw list view */
	if (hBackground != NULL && showScreenShot)
		InvalidateRect(hwndList, NULL, FALSE);
}

static void AdjustMetrics(void)
{
	HDC hDC;
	TEXTMETRIC tm;
	int xtraX, xtraY;
	AREA area;
	int  offX, offY;
	int  maxX, maxY;
	COLORREF textColor;
	HWND hWnd;

	/* WM_SETTINGCHANGE also */
	xtraX  = GetSystemMetrics(SM_CXFIXEDFRAME); /* Dialog frame width */
	xtraY  = GetSystemMetrics(SM_CYFIXEDFRAME); /* Dialog frame height */
	xtraY += GetSystemMetrics(SM_CYMENUSIZE);   /* Menu height */
	xtraY += GetSystemMetrics(SM_CYCAPTION);    /* Caption Height */
	maxX   = GetSystemMetrics(SM_CXSCREEN);     /* Screen Width */
	maxY   = GetSystemMetrics(SM_CYSCREEN);     /* Screen Height */

	hDC = GetDC(hMain);
	GetTextMetrics (hDC, &tm);

	/* Convert MIN Width/Height from Dialog Box Units to pixels. */
	MIN_WIDTH  = (int)((tm.tmAveCharWidth / 4.0) * DBU_MIN_WIDTH)  + xtraX;
	MIN_HEIGHT = (int)((tm.tmHeight       / 8.0) * DBU_MIN_HEIGHT) + xtraY;
	ReleaseDC(hMain, hDC);

	if ((textColor = GetListFontColor()) == RGB(255, 255, 255))
		textColor = RGB(240, 240, 240);

	hWnd = GetWindow(hMain, GW_CHILD);
	while(hWnd)
	{
		char szClass[128];

		if (GetClassNameA(hWnd, szClass, ARRAY_LENGTH(szClass)))
		{
			if (!strcmp(szClass, "SysListView32"))
			{
				ListView_SetBkColor(hWnd, GetSysColor(COLOR_WINDOW));
				ListView_SetTextColor(hWnd, textColor);
			}
			else if (!strcmp(szClass, "SysTreeView32"))
			{
				TreeView_SetBkColor(hTreeView, GetSysColor(COLOR_WINDOW));
				TreeView_SetTextColor(hTreeView, textColor);
			}
		}
		hWnd = GetWindow(hWnd, GW_HWNDNEXT);
	}

	GetWindowArea(&area);

	offX = area.x + area.width;
	offY = area.y + area.height;

	if (offX > maxX)
	{
		offX = maxX;
		area.x = (offX - area.width > 0) ? (offX - area.width) : 0;
	}
	if (offY > maxY)
	{
		offY = maxY;
		area.y = (offY - area.height > 0) ? (offY - area.height) : 0;
	}

	SetWindowArea(&area);
	SetWindowPos(hMain, 0, area.x, area.y, area.width, area.height, SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOACTIVATE);
}


/* Adjust options - tune them to the currently selected game */
static void EnablePlayOptions(int nIndex, options_type *o)
{
	if (!DIJoystick.Available())
		o->joystick = FALSE;
}

static int FindIconIndex(int nIconResource)
{
	int i;
	for(i = 0; g_iconData[i].icon_name; i++)
	{
		if (g_iconData[i].resource == nIconResource)
			return i;
	}
	return -1;
}

static BOOL UseBrokenIcon(int type)
{
	//if ((GetViewMode() != VIEW_GROUPED) && (GetViewMode() != VIEW_DETAILS))
	//	return TRUE;
	if (type == 4 && !GetUseBrokenIcon())
		return FALSE;
	return TRUE;
}

static int GetIconForDriver(int nItem)
{
	int iconRoms;

	if (DriverUsesRoms(nItem))
	{
		int audit_result = GetRomAuditResults(nItem);
		if (IsAuditResultKnown(audit_result) == FALSE)
			return 2;

		if (IsAuditResultYes(audit_result))
			iconRoms = 1;
		else
			iconRoms = 0;
	}
	else
		iconRoms = 1;

	// iconRoms is now either 0 (no roms), 1 (roms), or 2 (unknown)

	/* these are indices into icon_names, which maps into our image list
	 * also must match IDI_WIN_NOROMS + iconRoms
	 */

	// Show Red-X if the ROMs are present and flagged as NOT WORKING
	if (iconRoms == 1 && DriverIsBroken(nItem))
		iconRoms = FindIconIndex(IDI_WIN_REDX);

	// show clone icon if we have roms and game is working
	if (iconRoms == 1 && DriverIsClone(nItem))
		iconRoms = FindIconIndex(IDI_WIN_CLONE);

	// if we have the roms, then look for a custom per-game icon to override
	if (iconRoms == 1 || iconRoms == 3 || !UseBrokenIcon(iconRoms))
	{
		if (icon_index[nItem] == 0)
			AddDriverIcon(nItem,iconRoms);
		iconRoms = icon_index[nItem];
	}

	return iconRoms;
}

static BOOL HandleTreeContextMenu(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HMENU hTreeMenu;
	HMENU hMenu;
	TVHITTESTINFO hti;
	POINT pt;

	if ((HWND)wParam != GetDlgItem(hWnd, IDC_TREE))
		return FALSE;

	pt.x = GET_X_LPARAM(lParam);
	pt.y = GET_Y_LPARAM(lParam);
	if (pt.x < 0 && pt.y < 0)
		GetCursorPos(&pt);

	/* select the item that was right clicked or shift-F10'ed */
	hti.pt = pt;
	ScreenToClient(hTreeView,&hti.pt);
	TreeView_HitTest(hTreeView,&hti);
	if ((hti.flags & TVHT_ONITEM) != 0)
		TreeView_SelectItem(hTreeView,hti.hItem);

	hTreeMenu = LoadMenu(hInst,MAKEINTRESOURCE(IDR_CONTEXT_TREE));

	InitTreeContextMenu(hTreeMenu);

	hMenu = GetSubMenu(hTreeMenu, 0);

	TranslateMenu(hMenu, ID_CONTEXT_RENAME_CUSTOM);
	UpdateMenu(hMenu);

	TrackPopupMenu(hMenu,TPM_LEFTALIGN | TPM_RIGHTBUTTON,pt.x,pt.y,0,hWnd,NULL);

	DestroyMenu(hTreeMenu);

	return TRUE;
}



static void GamePicker_OnBodyContextMenu(POINT pt)
{
	HMENU hMenuLoad;
	HMENU hMenu;
	HMENU hSubMenu = NULL;

	int  nGame = Picker_GetSelectedItem(hwndList);
	TPMPARAMS tpmp;
	ZeroMemory(&tpmp,sizeof(tpmp));
	tpmp.cbSize = sizeof(tpmp);
	GetWindowRect(GetDlgItem(hMain, IDC_SSFRAME), &tpmp.rcExclude);

	hMenuLoad = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_MENU));
	hMenu = GetSubMenu(hMenuLoad, 0);
	TranslateMenu(hMenu, ID_FILE_PLAY);

	UpdateMenu(hMenu);

#ifdef USE_IPS
	if (have_selection)
	{
		options_type* pOpts = GetGameOptions(nGame);
		int  patch_count = GetPatchCount(driversw[nGame]->name, TEXT("*"));
		
		if (patch_count > MAX_PATCHES)
			patch_count = MAX_PATCHES;

		while (patch_count--)
		{
			WCHAR patch_filename[MAX_PATCHNAME];

			if (GetPatchFilename(patch_filename, driversw[nGame]->name, patch_count))
			{
				WCHAR wbuf[MAX_PATCHNAME * MAX_PATCHES];
				WCHAR *wp = NULL;
				LPWSTR patch_desc = GetPatchDesc(driversw[nGame]->name, patch_filename);

				if (patch_desc && patch_desc[0])
					//has lang specific ips desc, get the first line as display name
					snwprintf(wbuf, ARRAY_LENGTH(wbuf), TEXT("   %s"), wcstok(patch_desc, TEXT("\r\n")));
				else
					//otherwise, use .dat filename instead
					snwprintf(wbuf, ARRAY_LENGTH(wbuf), TEXT("   %s"), patch_filename);

				// patch_count--, add menu items in reversed order
				if(!(wp = wcschr(wbuf,'/')))	// no category
					InsertMenu(hMenu, 1, MF_BYPOSITION, ID_PLAY_PATCH + patch_count, ConvertAmpersandString(wbuf));
				else	// has category
				{
					int  i;

					*wp = '\0';
					
					for (i=1; i<GetMenuItemCount(hMenu); i++)	// do not create submenu if exists
					{
						hSubMenu = GetSubMenu(hMenu, i);
						if (hSubMenu)
						{
							WCHAR patch_category[128];

							GetMenuString(hMenu, i, patch_category, 127, MF_BYPOSITION);
							if (!wcscmp(patch_category, wbuf))
								break;
							hSubMenu = NULL;
						}
					}
					
					if(!hSubMenu)
					{
						hSubMenu = CreateMenu();
						InsertMenu(hSubMenu, 0, MF_BYPOSITION, ID_PLAY_PATCH + patch_count, ConvertAmpersandString(wp + 1));
						InsertMenu(hMenu, 1, MF_BYPOSITION | MF_POPUP, (UINT)hSubMenu, ConvertAmpersandString(wbuf));
					}
					else
						InsertMenu(hSubMenu, 0, MF_BYPOSITION, ID_PLAY_PATCH + patch_count, ConvertAmpersandString(wp + 1));
				}

				if (pOpts->ips != NULL)
				{
					int  i;

					wcscpy(wbuf, pOpts->ips);
					wp = wcstok(wbuf, TEXT(","));

					for (i = 0; i < MAX_PATCHES && wp; i++)
					{
						if (!wcscmp(patch_filename, wp))
						{
							CheckMenuItem(hMenu,ID_PLAY_PATCH + patch_count, MF_BYCOMMAND | MF_CHECKED);
							break;
						}
						wp = wcstok(NULL, TEXT(","));
					}
				}
			}
		}
	}
#endif /* USE_IPS */

#ifdef IMAGE_MENU
	if (GetImageMenuStyle() > 0)
	{
		ImageMenu_CreatePopup(hMain, hMenu);

		ImageMenu_SetMenuTitleProps(hMenu, driversw[nGame]->modify_the, TRUE, RGB(255,255,255));
		ImageMenu_SetMenuTitleBkProps(hMenu, RGB(255,237,213), RGB(255,186,94), TRUE, TRUE);
	}
#endif /* IMAGE_MENU */

	if (GetShowScreenShot())
	{
		dprintf("%d,%d,%d,%d", tpmp.rcExclude.left,tpmp.rcExclude.right,tpmp.rcExclude.top,tpmp.rcExclude.bottom);
		//the menu should not overlap SSFRAME
		TrackPopupMenuEx(hMenu,TPM_LEFTALIGN | TPM_RIGHTBUTTON,pt.x,pt.y,hMain,&tpmp);
	}
	else
		TrackPopupMenuEx(hMenu,TPM_LEFTALIGN | TPM_RIGHTBUTTON,pt.x,pt.y,hMain,NULL);

#ifdef IMAGE_MENU
	if (GetImageMenuStyle() > 0)
		ImageMenu_Remove(hMenu);
#endif /* IMAGE_MENU */

	DestroyMenu(hMenuLoad);
}



static BOOL HandleScreenShotContextMenu(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HMENU hMenuLoad;
	HMENU hMenu;
	POINT pt;

	if ((HWND)wParam != GetDlgItem(hWnd, IDC_SSPICTURE) && (HWND)wParam != GetDlgItem(hWnd, IDC_SSFRAME))
		return FALSE;

	pt.x = GET_X_LPARAM(lParam);
	pt.y = GET_Y_LPARAM(lParam);
	if (pt.x < 0 && pt.y < 0)
		GetCursorPos(&pt);

	hMenuLoad = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_SCREENSHOT));
	hMenu = GetSubMenu(hMenuLoad, 0);
	TranslateMenu(hMenu, ID_VIEW_PAGETAB);

	UpdateMenu(hMenu);

#ifdef IMAGE_MENU
	if (GetImageMenuStyle() > 0)
	{
		ImageMenu_CreatePopup(hWnd, hMenuLoad);
		ImageMenu_SetStyle(GetImageMenuStyle());
	}
#endif /* IMAGE_MENU */

	TrackPopupMenu(hMenu,TPM_LEFTALIGN | TPM_RIGHTBUTTON,pt.x,pt.y,0,hWnd,NULL);

#ifdef IMAGE_MENU
	if (GetImageMenuStyle() > 0)
		ImageMenu_Remove(hMenuLoad);
#endif /* IMAGE_MENU */

	DestroyMenu(hMenuLoad);

	return TRUE;
}

static void UpdateMenu(HMENU hMenu)
{
	WCHAR		buf[200];
	MENUITEMINFO	mItem;
	int 			nGame = Picker_GetSelectedItem(hwndList);
	LPTREEFOLDER lpFolder = GetCurrentFolder();
	int bios_driver;
	int i;

#ifdef KSERVER
		if(m_hPro==NULL)
		_snwprintf(buf, sizeof(buf) / sizeof(buf[0]), _UIW(TEXT("&Start Kaillera Server")));
		else
		_snwprintf(buf, sizeof(buf) / sizeof(buf[0]), _UIW(TEXT("&Stop Kaillera Server")));
		mItem.cbSize     = sizeof(mItem);
		mItem.fMask      = MIIM_TYPE;
		mItem.fType      = MFT_STRING;
		mItem.dwTypeData = buf;
		mItem.cch        = wcslen(mItem.dwTypeData);
		SetMenuItemInfo(hMenu, ID_FILE_SERVER, FALSE, &mItem);
#endif /* KSERVER */

	if (have_selection)
	{
		snwprintf(buf, ARRAY_LENGTH(buf), _UIW(TEXT("&Play %s")),
		         ConvertAmpersandString(UseLangList() ?
		                                _LSTW(driversw[nGame]->description) :
		                                driversw[nGame]->modify_the));

		mItem.cbSize     = sizeof(mItem);
		mItem.fMask      = MIIM_TYPE;
		mItem.fType      = MFT_STRING;
		mItem.dwTypeData = buf;
		mItem.cch        = wcslen(mItem.dwTypeData);

		SetMenuItemInfo(hMenu, ID_FILE_PLAY, FALSE, &mItem);

		snwprintf(buf, ARRAY_LENGTH(buf),
			_UIW(TEXT("Propert&ies for %s")), GetDriverFilename(nGame));

		mItem.cbSize     = sizeof(mItem);
		mItem.fMask      = MIIM_TYPE;
		mItem.fType      = MFT_STRING;
		mItem.dwTypeData = buf;
		mItem.cch        = wcslen(mItem.dwTypeData);

		SetMenuItemInfo(hMenu, ID_FOLDER_SOURCEPROPERTIES, FALSE, &mItem);
#ifdef MAME_AVI
        EnableMenuItem(hMenu, ID_FILE_PLAY_BACK_AVI,    MF_ENABLED);
        EnableMenuItem(hMenu, ID_FILE_PLAY_WITH_AVI, 	MF_ENABLED);
#endif /* MAME_AVI */

		bios_driver = DriverBiosIndex(nGame);
		if (bios_driver != -1 && bios_driver != nGame)
		{
			snwprintf(buf, ARRAY_LENGTH(buf),
				_UIW(TEXT("Properties &for %s BIOS")), driversw[bios_driver]->name);
			mItem.dwTypeData = buf;
		}
		else
		{
			EnableMenuItem(hMenu, ID_BIOS_PROPERTIES, MF_GRAYED);
			mItem.dwTypeData = _UIW(TEXT("Properties &for BIOS"));
		}

		mItem.cbSize     = sizeof(mItem);
		mItem.fMask      = MIIM_TYPE;
		mItem.fType      = MFT_STRING;
		mItem.cch        = wcslen(mItem.dwTypeData);
		SetMenuItemInfo(hMenu, ID_BIOS_PROPERTIES, FALSE, &mItem);

		EnableMenuItem(hMenu, ID_CONTEXT_SELECT_RANDOM, MF_ENABLED);
	}
	else
	{
		snwprintf(buf, ARRAY_LENGTH(buf), _UIW(TEXT("&Play %s")), TEXT("..."));

		mItem.cbSize     = sizeof(mItem);
		mItem.fMask      = MIIM_TYPE;
		mItem.fType      = MFT_STRING;
		mItem.dwTypeData = buf;
		mItem.cch        = wcslen(mItem.dwTypeData);

		SetMenuItemInfo(hMenu, ID_FILE_PLAY, FALSE, &mItem);

		snwprintf(buf, ARRAY_LENGTH(buf), _UIW(TEXT("Propert&ies for %s")), TEXT("..."));

		mItem.cbSize     = sizeof(mItem);
		mItem.fMask      = MIIM_TYPE;
		mItem.fType      = MFT_STRING;
		mItem.dwTypeData = buf;
		mItem.cch        = wcslen(mItem.dwTypeData);

		SetMenuItemInfo(hMenu, ID_FOLDER_SOURCEPROPERTIES, FALSE, &mItem);

		mItem.cbSize     = sizeof(mItem);
		mItem.fMask      = MIIM_TYPE;
		mItem.fType      = MFT_STRING;
		mItem.dwTypeData = _UIW(TEXT("Properties &for BIOS"));
		mItem.cch        = wcslen(mItem.dwTypeData);
		SetMenuItemInfo(hMenu, ID_BIOS_PROPERTIES, FALSE, &mItem);

		EnableMenuItem(hMenu, ID_FILE_PLAY,             MF_GRAYED);
		EnableMenuItem(hMenu, ID_FILE_PLAY_RECORD,      MF_GRAYED);
		EnableMenuItem(hMenu, ID_GAME_PROPERTIES,       MF_GRAYED);
		EnableMenuItem(hMenu, ID_FOLDER_SOURCEPROPERTIES, MF_GRAYED);
		EnableMenuItem(hMenu, ID_BIOS_PROPERTIES,       MF_GRAYED);
		EnableMenuItem(hMenu, ID_CONTEXT_SELECT_RANDOM, MF_GRAYED);
#ifdef MAME_AVI
        EnableMenuItem(hMenu, ID_FILE_PLAY_WITH_AVI,    MF_GRAYED);	
#endif /* MAME_AVI */
	}

	if (oldControl)
	{
		EnableMenuItem(hMenu, ID_CUSTOMIZE_FIELDS,  MF_GRAYED);
		EnableMenuItem(hMenu, ID_GAME_PROPERTIES,   MF_GRAYED);
		EnableMenuItem(hMenu, ID_FOLDER_SOURCEPROPERTIES, MF_GRAYED);
		EnableMenuItem(hMenu, ID_BIOS_PROPERTIES,   MF_GRAYED);
		EnableMenuItem(hMenu, ID_OPTIONS_DEFAULTS,  MF_GRAYED);
	}

	if (lpFolder->m_dwFlags & F_CUSTOM)
	{
		EnableMenuItem(hMenu,ID_CONTEXT_REMOVE_CUSTOM,MF_ENABLED);
		EnableMenuItem(hMenu,ID_CONTEXT_RENAME_CUSTOM,MF_ENABLED);
	}
	else
	{
		EnableMenuItem(hMenu,ID_CONTEXT_REMOVE_CUSTOM,MF_GRAYED);
		EnableMenuItem(hMenu,ID_CONTEXT_RENAME_CUSTOM,MF_GRAYED);
	}

	if (lpFolder && (IsSourceFolder(lpFolder) || IsVectorFolder(lpFolder) || IsBiosFolder(lpFolder)))
		EnableMenuItem(hMenu,ID_FOLDER_PROPERTIES,MF_ENABLED);
	else
		EnableMenuItem(hMenu,ID_FOLDER_PROPERTIES,MF_GRAYED);

	CheckMenuRadioItem(hMenu, ID_VIEW_TAB_SCREENSHOT, ID_VIEW_TAB_HISTORY,
					   ID_VIEW_TAB_SCREENSHOT + TabView_GetCurrentTab(hTabCtrl), MF_BYCOMMAND);

	// set whether we're showing the tab control or not
	if (bShowTabCtrl)
		CheckMenuItem(hMenu,ID_VIEW_PAGETAB,MF_BYCOMMAND | MF_CHECKED);
	else
		CheckMenuItem(hMenu,ID_VIEW_PAGETAB,MF_BYCOMMAND | MF_UNCHECKED);


	for (i=0; i < MAX_TAB_TYPES; i++)
	{
		// disable menu items for tabs we're not currently showing
		if (GetShowTab(i))
			EnableMenuItem(hMenu,ID_VIEW_TAB_SCREENSHOT + i,MF_BYCOMMAND | MF_ENABLED);
		else
			EnableMenuItem(hMenu,ID_VIEW_TAB_SCREENSHOT + i,MF_BYCOMMAND | MF_GRAYED);

		// check toggle menu items 
		if (GetShowTab(i))
			CheckMenuItem(hMenu, ID_TOGGLE_TAB_SCREENSHOT + i,MF_BYCOMMAND | MF_CHECKED);
		else
			CheckMenuItem(hMenu, ID_TOGGLE_TAB_SCREENSHOT + i,MF_BYCOMMAND | MF_UNCHECKED);
	}

	for (i=0; i < MAX_FOLDERS; i++)
	{
		if (GetShowFolder(i))
			CheckMenuItem(hMenu,ID_CONTEXT_SHOW_FOLDER_START + i,MF_BYCOMMAND | MF_CHECKED);
		else
			CheckMenuItem(hMenu,ID_CONTEXT_SHOW_FOLDER_START + i,MF_BYCOMMAND | MF_UNCHECKED);
	}
}

void InitTreeContextMenu(HMENU hTreeMenu)
{
	MENUITEMINFOW mii;
	HMENU hMenu;
	int i;

	ZeroMemory(&mii,sizeof(mii));
	mii.cbSize = sizeof(mii);

	mii.wID = -1;
	mii.fMask = MIIM_SUBMENU | MIIM_ID;

	hMenu = GetSubMenu(hTreeMenu, 0);

	if (GetMenuItemInfoW(hMenu,3,TRUE,&mii) == FALSE)
	{
		dprintf("can't find show folders context menu");
		return;
	}

	if (mii.hSubMenu == NULL)
	{
		dprintf("can't find submenu for show folders context menu");
		return;
	}

	hMenu = mii.hSubMenu;

	for (i=0; g_folderData[i].m_lpTitle != NULL; i++)
	{
		mii.fMask = MIIM_TYPE | MIIM_ID;
		mii.fType = MFT_STRING;
		mii.dwTypeData = (void *)g_folderData[i].m_lpTitle;
		mii.cch = wcslen(mii.dwTypeData);
		mii.wID = ID_CONTEXT_SHOW_FOLDER_START + g_folderData[i].m_nFolderId;


		// menu in resources has one empty item (needed for the submenu to setup properly)
		// so overwrite this one, append after
		if (i == 0)
			SetMenuItemInfoW(hMenu,ID_CONTEXT_SHOW_FOLDER_START,FALSE,&mii);
		else
			InsertMenuItemW(hMenu,i,FALSE,&mii);
	}

}

void ToggleShowFolder(int folder)
{
	LPTREEFOLDER current_folder = GetCurrentFolder();

	SetWindowRedraw(hwndList,FALSE);

	SetShowFolder(folder,!GetShowFolder(folder));

	ResetTreeViewFolders();
	SelectTreeViewFolder(current_folder);

	SetWindowRedraw(hwndList,TRUE);
}

static LRESULT CALLBACK HistoryWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (hBackground)
	{
		switch (uMsg)
		{
		case WM_MOUSEMOVE:
		{
			if (MouseHasBeenMoved())
				ShowCursor(TRUE);
			break;
		}

		case WM_ERASEBKGND:
			return TRUE;
		case WM_PAINT:
		{
			POINT p = { 0, 0 };
			
			/* get base point of background bitmap */
			MapWindowPoints(hWnd,hTreeView,&p,1);
			PaintBackgroundImage(hWnd, NULL, p.x, p.y);
			/* to ensure our parent procedure repaints the whole client area */
			InvalidateRect(hWnd, NULL, FALSE);
			break;
		}
		}
	}
	return CallWindowProc(g_lpHistoryWndProc, hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK PictureFrameWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_MOUSEMOVE:
	{
		if (MouseHasBeenMoved())
			ShowCursor(TRUE);
		break;
	}

	case WM_NCHITTEST :
	{
		POINT pt;
		RECT  rect;
		HWND hHistory = GetDlgItem(hMain, IDC_HISTORY);

		pt.x = LOWORD(lParam);
		pt.y = HIWORD(lParam);
		GetWindowRect(hHistory, &rect);
		// check if they clicked on the picture area (leave 6 pixel no man's land
		// by the history window to reduce mistaken clicks)
		// no more no man's land, the Cursor changes when Edit control is left, should be enough feedback
		if (have_history && NeedHistoryText() &&
//		        (rect.top - 6) < pt.y && pt.y < (rect.bottom + 6) )
		        PtInRect( &rect, pt ) )
		{
			return HTTRANSPARENT;
		}
		else
		{
			return HTCLIENT;
		}
		break;
	}

	case WM_CONTEXTMENU:
		if ( HandleScreenShotContextMenu(hWnd, wParam, lParam))
			return FALSE;
		break;
	}

	if (hBackground)
	{
		switch (uMsg)
		{
		case WM_ERASEBKGND :
			return TRUE;
		case WM_PAINT :
		{
			RECT rect,nodraw_rect;
			HRGN region,nodraw_region;
			POINT p = { 0, 0 };

			/* get base point of background bitmap */
			MapWindowPoints(hWnd,hTreeView,&p,1);

			/* get big region */
			GetClientRect(hWnd,&rect);
			region = CreateRectRgnIndirect(&rect);

			if (IsWindowVisible(GetDlgItem(hMain,IDC_HISTORY)))
			{
				/* don't draw over this window */
				GetWindowRect(GetDlgItem(hMain,IDC_HISTORY),&nodraw_rect);
				MapWindowPoints(HWND_DESKTOP,hWnd,(LPPOINT)&nodraw_rect,2);
				nodraw_region = CreateRectRgnIndirect(&nodraw_rect);
				CombineRgn(region,region,nodraw_region,RGN_DIFF);
				DeleteObject(nodraw_region);
			}
			if (IsWindowVisible(GetDlgItem(hMain,IDC_SSPICTURE)))
			{
				/* don't draw over this window */
				GetWindowRect(GetDlgItem(hMain,IDC_SSPICTURE),&nodraw_rect);
				MapWindowPoints(HWND_DESKTOP,hWnd,(LPPOINT)&nodraw_rect,2);
				nodraw_region = CreateRectRgnIndirect(&nodraw_rect);
				CombineRgn(region,region,nodraw_region,RGN_DIFF);
				DeleteObject(nodraw_region);
			}

			PaintBackgroundImage(hWnd,region,p.x,p.y);

			DeleteObject(region);

			/* to ensure our parent procedure repaints the whole client area */
			InvalidateRect(hWnd, NULL, FALSE);

			break;
		}
		}
	}
	return CallWindowProc(g_lpPictureFrameWndProc, hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK PictureWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ERASEBKGND :
		return TRUE;
	case WM_PAINT :
	{
		PAINTSTRUCT ps;
		HDC	hdc,hdc_temp;
		RECT rect;
		HBITMAP old_bitmap;

		int width,height;

		RECT rect2;
		HBRUSH hBrush;
		HBRUSH holdBrush;
		HRGN region1, region2;
		int nBordersize;
		nBordersize = GetScreenshotBorderSize();
		hBrush = CreateSolidBrush(GetScreenshotBorderColor());

		hdc = BeginPaint(hWnd,&ps);

		hdc_temp = CreateCompatibleDC(hdc);
		if (ScreenShotLoaded())
		{
			width = GetScreenShotWidth();
			height = GetScreenShotHeight();

			old_bitmap = SelectObject(hdc_temp,GetScreenShotHandle());
		}
		else
		{
			BITMAP bmp;

			GetObject(hMissing_bitmap,sizeof(BITMAP),&bmp);
			width = bmp.bmWidth;
			height = bmp.bmHeight;

			old_bitmap = SelectObject(hdc_temp,hMissing_bitmap);
		}

		GetClientRect(hWnd,&rect);

		rect2 = rect;
		//Configurable Borders around images
		rect.bottom -= nBordersize;
		if( rect.bottom < 0)
			rect.bottom = rect2.bottom;
		rect.right -= nBordersize;
		if( rect.right < 0)
			rect.right = rect2.right;
		rect.top += nBordersize;
		if( rect.top > rect.bottom )
			rect.top = rect2.top;
		rect.left += nBordersize;
		if( rect.left > rect.right )
			rect.left = rect2.left;
		region1 = CreateRectRgnIndirect(&rect);
		region2 = CreateRectRgnIndirect(&rect2);
		CombineRgn(region2,region2,region1,RGN_DIFF);
		holdBrush = SelectObject(hdc, hBrush); 

		FillRgn(hdc,region2, hBrush );
		SelectObject(hdc, holdBrush); 
		DeleteObject(hBrush); 

		SetStretchBltMode(hdc,STRETCH_HALFTONE);
		StretchBlt(hdc,nBordersize,nBordersize,rect.right-rect.left,rect.bottom-rect.top,
		           hdc_temp,0,0,width,height,SRCCOPY);
		SelectObject(hdc_temp,old_bitmap);
		DeleteDC(hdc_temp);

		EndPaint(hWnd,&ps);

		return TRUE;
	}
	}

	return CallWindowProc(g_lpPictureWndProc, hWnd, uMsg, wParam, lParam);
}

void RemoveCurrentGameCustomFolder(void)
{
	RemoveGameCustomFolder(Picker_GetSelectedItem(hwndList));
}

void RemoveGameCustomFolder(int driver_index)
{
	int i;
	TREEFOLDER **folders;
	int num_folders;

	GetFolders(&folders,&num_folders);
	
	for (i=0;i<num_folders;i++)
	{
		if ((folders[i]->m_dwFlags & F_CUSTOM) && folders[i] == GetCurrentFolder())
		{
			int current_pick_index;

			RemoveFromCustomFolder(folders[i],driver_index);

			if (driver_index == Picker_GetSelectedItem(hwndList))
			{
				/* if we just removed the current game,
				  move the current selection so that when we rebuild the listview it
				  leaves the cursor on next or previous one */
			
				current_pick_index = GetSelectedPick();
				Picker_SetSelectedPick(hwndList, GetSelectedPick() + 1);
				if (current_pick_index == GetSelectedPick()) /* we must have deleted the last item */
					Picker_SetSelectedPick(hwndList, GetSelectedPick() - 1);
			}

			ResetListView();
			return;
		}
	}
	MessageBox(GetMainWindow(), _UIW(TEXT("Error searching for custom folder")), TEXT_MAME32NAME, MB_OK | MB_ICONERROR);

}


void BeginListViewDrag(NM_LISTVIEW *pnmv)
{
	LV_ITEM lvi;
	POINT pt;

	lvi.iItem = pnmv->iItem;
	lvi.mask	 = LVIF_PARAM;
	ListView_GetItem(hwndList, &lvi);

	game_dragged = lvi.lParam;

	pt.x = 0;
	pt.y = 0;

	/* Tell the list view control to create an image to use 
	   for dragging. */
	himl_drag = ListView_CreateDragImage(hwndList,pnmv->iItem,&pt);
 
	/* Start the drag operation. */
	ImageList_BeginDrag(himl_drag, 0, 0, 0); 

	pt = pnmv->ptAction;
	ClientToScreen(hwndList,&pt);
	ImageList_DragEnter(GetDesktopWindow(),pt.x,pt.y);

	/* Hide the mouse cursor, and direct mouse input to the 
	   parent window. */
	SetCapture(hMain);

	prev_drag_drop_target = NULL;

	g_listview_dragging = TRUE; 

}

void MouseMoveListViewDrag(POINTS p)
{
	HTREEITEM htiTarget;
	TV_HITTESTINFO tvht;

	POINT pt;
	pt.x = p.x;
	pt.y = p.y;

	ClientToScreen(hMain,&pt);

	ImageList_DragMove(pt.x,pt.y);

	MapWindowPoints(GetDesktopWindow(),hTreeView,&pt,1);

	tvht.pt = pt;
	htiTarget = TreeView_HitTest(hTreeView,&tvht);

	if (htiTarget != prev_drag_drop_target)
	{
		ImageList_DragShowNolock(FALSE);
		if (htiTarget != NULL)
			TreeView_SelectDropTarget(hTreeView,htiTarget);
		else
			TreeView_SelectDropTarget(hTreeView,NULL);
		ImageList_DragShowNolock(TRUE);

		prev_drag_drop_target = htiTarget;
	}
}

void ButtonUpListViewDrag(POINTS p)
{
	POINT pt;
	HTREEITEM htiTarget;
	TV_HITTESTINFO tvht;
	TVITEM tvi;
	
	ReleaseCapture();

	ImageList_DragLeave(hwndList);
	ImageList_EndDrag();
	ImageList_Destroy(himl_drag);

	TreeView_SelectDropTarget(hTreeView,NULL);

	g_listview_dragging = FALSE;

	/* see where the game was dragged */

	pt.x = p.x;
	pt.y = p.y;

	MapWindowPoints(hMain,hTreeView,&pt,1);

	tvht.pt = pt;
	htiTarget = TreeView_HitTest(hTreeView,&tvht);
	if (htiTarget == NULL)
	{
		LVHITTESTINFO lvhtti;
		LPTREEFOLDER folder;
		RECT rcList;

		/* the user dragged a game onto something other than the treeview */
		/* try to remove if we're in a custom folder */

		/* see if it was dragged within the list view; if so, ignore */

		MapWindowPoints(hTreeView,hwndList,&pt,1);
		lvhtti.pt = pt;
		GetWindowRect(hwndList, &rcList);
		ClientToScreen(hwndList, &pt);
		if( PtInRect(&rcList, pt) != 0 )
			return;

		folder = GetCurrentFolder();
		if (folder->m_dwFlags & F_CUSTOM)
		{
			/* dragged out of a custom folder, so let's remove it */
			RemoveCurrentGameCustomFolder();
		}
		return;
	}


	tvi.lParam = 0;
	tvi.mask  = TVIF_PARAM | TVIF_HANDLE;
	tvi.hItem = htiTarget;

	if (TreeView_GetItem(hTreeView, &tvi))
	{
		LPTREEFOLDER folder = (LPTREEFOLDER)tvi.lParam;
		AddToCustomFolder(folder,game_dragged);
	}

}

static LPTREEFOLDER GetSelectedFolder(void)
{
	HTREEITEM htree;
	TVITEM tvi;
	htree = TreeView_GetSelection(hTreeView);
	if(htree != NULL)
	{
		tvi.hItem = htree;
		tvi.mask = TVIF_PARAM;
		TreeView_GetItem(hTreeView,&tvi);
		return (LPTREEFOLDER)tvi.lParam;
	}
	return NULL;
}

static HICON GetSelectedFolderIcon(void)
{
	HTREEITEM htree;
	TVITEM tvi;
	HIMAGELIST hSmall_icon;
	LPTREEFOLDER folder;
	htree = TreeView_GetSelection(hTreeView);

	if (htree != NULL)
	{
		tvi.hItem = htree;
		tvi.mask = TVIF_PARAM;
		TreeView_GetItem(hTreeView,&tvi);
		
		folder = (LPTREEFOLDER)tvi.lParam;
		//hSmall_icon = TreeView_GetImageList(hTreeView,(int)tvi.iImage);
		hSmall_icon = NULL;
		return ImageList_GetIcon(hSmall_icon, tvi.iImage, ILD_TRANSPARENT);
	}
	return NULL;
}

/* Updates all currently displayed Items in the List with the latest Data*/
void UpdateListView(void)
{
	if( (GetViewMode() == VIEW_GROUPED) || (GetViewMode() == VIEW_DETAILS ) )
	    ListView_RedrawItems(hwndList,ListView_GetTopIndex(hwndList),
	                         ListView_GetTopIndex(hwndList)+ ListView_GetCountPerPage(hwndList) );
}

void CalculateBestScreenShotRect(HWND hWnd, RECT *pRect, BOOL restrict_height)
{
	int 	destX, destY;
	int 	destW, destH;
	int	nBorder;
	RECT	rect;
	/* for scaling */		 
	int x, y;
	int rWidth, rHeight;
	double scale;
	BOOL bReduce = FALSE;

	GetClientRect(hWnd, &rect);

	// Scale the bitmap to the frame specified by the passed in hwnd
	if (ScreenShotLoaded())
	{
		x = GetScreenShotWidth();
		y = GetScreenShotHeight();
	}
	else
	{
		BITMAP bmp;
		GetObject(hMissing_bitmap,sizeof(BITMAP),&bmp);

		x = bmp.bmWidth;
		y = bmp.bmHeight;
	}
	rWidth	= (rect.right  - rect.left);
	rHeight = (rect.bottom - rect.top);

	/* Limit the screen shot to max height of 264 */
	if (restrict_height == TRUE && rHeight > 264)
	{
		rect.bottom = rect.top + 264;
		rHeight = 264;
	}

	/* If the bitmap does NOT fit in the screenshot area */
	if (x > rWidth - 10 || y > rHeight - 10)
	{
		rect.right	-= 10;
		rect.bottom -= 10;
		rWidth	-= 10;
		rHeight -= 10;
		bReduce = TRUE;
		/* Try to scale it properly */
		/*	assumes square pixels, doesn't consider aspect ratio */
		if (x > y)
			scale = (double)rWidth / x;
		else
			scale = (double)rHeight / y;

		destW = (int)(x * scale);
		destH = (int)(y * scale);

		/* If it's still too big, scale again */
		if (destW > rWidth || destH > rHeight)
		{
			if (destW > rWidth)
				scale = (double)rWidth	/ destW;
			else
				scale = (double)rHeight / destH;

			destW = (int)(destW * scale);
			destH = (int)(destH * scale);
		}
	}
	else
	{
		if (GetStretchScreenShotLarger())
		{
			rect.right	-= 10;
			rect.bottom -= 10;
			rWidth	-= 10;
			rHeight -= 10;
			bReduce = TRUE;
			// Try to scale it properly
			// assumes square pixels, doesn't consider aspect ratio
			if (x < y)
				scale = (double)rWidth / x;
			else
				scale = (double)rHeight / y;
			
			destW = (int)(x * scale);
			destH = (int)(y * scale);
			
			// If it's too big, scale again
			if (destW > rWidth || destH > rHeight)
			{
				if (destW > rWidth)
					scale = (double)rWidth	/ destW;
				else
					scale = (double)rHeight / destH;
				
				destW = (int)(destW * scale);
				destH = (int)(destH * scale);
			}
		}
		else
		{
			// Use the bitmaps size if it fits
			destW = x;
			destH = y;
		}

	}


	destX = ((rWidth  - destW) / 2);
	destY = ((rHeight - destH) / 2);

	if (bReduce)
	{
		destX += 5;
		destY += 5;
	}
	nBorder = GetScreenshotBorderSize();
	if( destX > nBorder+1)
		pRect->left   = destX - nBorder;
	else
		pRect->left   = 2;
	if( destY > nBorder+1)
		pRect->top    = destY - nBorder;
	else
		pRect->top    = 2;
	if( rWidth >= destX + destW + nBorder)
		pRect->right  = destX + destW + nBorder;
	else
		pRect->right  = rWidth - pRect->left;
	if( rHeight >= destY + destH + nBorder)
		pRect->bottom = destY + destH + nBorder;
	else
		pRect->bottom = rHeight - pRect->top;
}

/*
  Switches to either fullscreen or normal mode, based on the
  current mode.

  POSSIBLE BUGS:
  Removing the menu might cause problems later if some
  function tries to poll info stored in the menu. Don't
  know if you've done that, but this was the only way I
  knew to remove the menu dynamically. 
*/

void SwitchFullScreenMode(void)
{
	LONG lMainStyle;
	int i;

	if (GetRunFullScreen())
	{
		// Return to normal

		// Restore the menu
		SetMenu(hMain, LoadMenu(hInst,MAKEINTRESOURCE(IDR_UI_MENU)));
		TranslateMenu(GetMenu(hMain), 0);
		DrawMenuBar(hMain);

		// Refresh the checkmarks
		CheckMenuItem(GetMenu(hMain), ID_VIEW_FOLDERS, GetShowFolderList() ? MF_CHECKED : MF_UNCHECKED); 
		CheckMenuItem(GetMenu(hMain), ID_VIEW_TOOLBARS, GetShowToolBar() ? MF_CHECKED : MF_UNCHECKED);    
		CheckMenuItem(GetMenu(hMain), ID_VIEW_STATUS, GetShowStatusBar() ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(GetMenu(hMain), ID_VIEW_PAGETAB, GetShowTabCtrl() ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(GetMenu(hMain), ID_VIEW_PICTURE_AREA, GetShowScreenShot() ? MF_CHECKED : MF_UNCHECKED);
		for (i = 0; i < UI_LANG_MAX; i++)
		{
			UINT cp = ui_lang_info[i].codepage;

			CheckMenuItem(GetMenu(hMain), i + ID_LANGUAGE_ENGLISH_US, i == GetLangcode() ? MF_CHECKED : MF_UNCHECKED);
			if (OnNT())
				EnableMenuItem(GetMenu(hMain), i + ID_LANGUAGE_ENGLISH_US, IsValidCodePage(cp) ? MF_ENABLED : MF_GRAYED);
			else
				EnableMenuItem(GetMenu(hMain), i + ID_LANGUAGE_ENGLISH_US, (i == UI_LANG_EN_US || cp == GetOEMCP()) ? MF_ENABLED : MF_GRAYED);
		}

		// Add frame to dialog again
		lMainStyle = GetWindowLong(hMain, GWL_STYLE);
		lMainStyle = lMainStyle | WS_BORDER;
		SetWindowLong(hMain, GWL_STYLE, lMainStyle);

		// Show the window maximized
		if( GetWindowState() == SW_MAXIMIZE )
		{
			ShowWindow(hMain, SW_NORMAL);
			ShowWindow(hMain, SW_MAXIMIZE);
		}
		else
			ShowWindow(hMain, SW_RESTORE);

		SetRunFullScreen(FALSE);
	}
	else
	{
		// Set to fullscreen

		// Remove menu
		SetMenu(hMain,NULL); 

		// Frameless dialog (fake fullscreen)
		lMainStyle = GetWindowLong(hMain, GWL_STYLE);
		lMainStyle = lMainStyle & (WS_BORDER ^ 0xffffffff);
		SetWindowLong(hMain, GWL_STYLE, lMainStyle);
		if( IsMaximized(hMain) )
		{
			ShowWindow(hMain, SW_NORMAL);
			SetWindowState( SW_MAXIMIZE );
		}
		else
		{
			SetWindowState( SW_NORMAL );
		}
		ShowWindow(hMain, SW_MAXIMIZE);

		SetRunFullScreen(TRUE);
	}
}

/*
  Checks to see if the mouse has been moved since this func
  was first called (which is at startup). The reason for 
  storing the startup coordinates of the mouse is that when
  a window is created it generates WM_MOUSEOVER events, even
  though the user didn't actually move the mouse. So we need
  to know when the WM_MOUSEOVER event is user-triggered.

  POSSIBLE BUGS:
  Gets polled at every WM_MOUSEMOVE so it might cause lag,
  but there's probably another way to code this that's 
  way better?
  
*/

BOOL MouseHasBeenMoved(void)
{
	static int mouse_x = -1;
	static int mouse_y = -1;
	POINT p;

	GetCursorPos(&p);

	if (mouse_x == -1) // First time
	{
		mouse_x = p.x;
		mouse_y = p.y;
	}
	
	return (p.x != mouse_x || p.y != mouse_y);       
}

/*
	The following two functions enable us to send Messages to the Game Window
*/
#if MULTISESSION

BOOL SendIconToEmulationWindow(int nGameIndex)
{
	HICON hIcon; 
	int nParentIndex = -1;
	hIcon = LoadIconFromFile(drivers[nGameIndex]->name); 
	if( hIcon == NULL ) 
	{ 
		//Check if clone, if so try parent icon first 
		if( DriverIsClone(nGameIndex) ) 
		{ 
			nParentIndex = GetParentIndex(drivers[nGameIndex]);
			if (nParentIndex >= 0)
				hIcon = LoadIconFromFile(drivers[nParentIndex]->name); 
			if( hIcon == NULL) 
			{ 
				hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAME32_ICON)); 
			} 
		} 
		else 
		{ 
			hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAME32_ICON)); 
		} 
	} 
	if( SendMessageToEmulationWindow( WM_SETICON, ICON_SMALL, (LPARAM)hIcon ) == FALSE)
		return FALSE;
	if( SendMessageToEmulationWindow( WM_SETICON, ICON_BIG, (LPARAM)hIcon ) == FALSE)
		return FALSE;
	return TRUE;
}

BOOL SendMessageToEmulationWindow(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	FINDWINDOWHANDLE fwhs;
	fwhs.ProcessInfo = NULL;
	fwhs.hwndFound  = NULL;

	EnumWindows(EnumWindowCallBack, (LPARAM)&fwhs);
	if( fwhs.hwndFound )
	{
		SendMessage(fwhs.hwndFound, Msg, wParam, lParam);
		//Fix for loosing Focus, we reset the Focus on our own Main window
		SendMessage(fwhs.hwndFound, WM_KILLFOCUS,(WPARAM) hMain, (LPARAM) NULL);
		return TRUE;
	}
	return FALSE;
}


static BOOL CALLBACK EnumWindowCallBack(HWND hwnd, LPARAM lParam) 
{ 
	FINDWINDOWHANDLE * pfwhs = (FINDWINDOWHANDLE * )lParam; 
	DWORD ProcessId, ProcessIdGUI; 
	char buffer[MAX_PATH]; 

	GetWindowThreadProcessId(hwnd, &ProcessId);
	GetWindowThreadProcessId(hMain, &ProcessIdGUI);

	// cmk--I'm not sure I believe this note is necessary
	// note: In order to make sure we have the MainFrame, verify that the title 
	// has Zero-Length. Under Windows 98, sometimes the Client Window ( which doesn't 
	// have a title ) is enumerated before the MainFrame 

	GetWindowTextA(hwnd, buffer, ARRAY_LENGTH(buffer));
	if (ProcessId  == ProcessIdGUI &&
		 strncmp(buffer,MAMENAME,strlen(MAMENAME)) == 0 &&
		 hwnd != hMain) 
	{ 
		pfwhs->hwndFound = hwnd; 
		return FALSE; 
	} 
	else 
	{ 
		// Keep enumerating 
		return TRUE; 
	} 
}

HWND GetGameWindow(void)
{
	FINDWINDOWHANDLE fwhs;
	fwhs.ProcessInfo = NULL;
	fwhs.hwndFound  = NULL;

	EnumWindows(EnumWindowCallBack, (LPARAM)&fwhs);
	return fwhs.hwndFound;
}


#endif
#if !(MULTISESSION) || defined(KAILLERA)

void SendIconToProcess(LPPROCESS_INFORMATION pi, int nGameIndex)
{
	HICON hIcon;
	int nParentIndex = -1;
	hIcon = LoadIconFromFile(drivers[nGameIndex]->name);
	if (hIcon == NULL)
	{
		//Check if clone, if so try parent icon first 
		if (DriverIsClone(nGameIndex))
		{
			nParentIndex = GetParentIndex(drivers[nGameIndex]);
			if( nParentIndex >= 0)
				hIcon = LoadIconFromFile(drivers[nParentIndex]->name); 
			if( hIcon == NULL) 
			{
				hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAME32_ICON));
			}
		}
		else
		{
			hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAME32_ICON));
		}
	}
	WaitForInputIdle( pi->hProcess, INFINITE );
	SendMessageToProcess( pi, WM_SETICON, ICON_SMALL, (LPARAM)hIcon );
	SendMessageToProcess( pi, WM_SETICON, ICON_BIG, (LPARAM)hIcon );
}

void SendMessageToProcess(LPPROCESS_INFORMATION lpProcessInformation, 
                                      UINT Msg, WPARAM wParam, LPARAM lParam)
{
	FINDWINDOWHANDLE fwhs;
	fwhs.ProcessInfo = lpProcessInformation;
	fwhs.hwndFound  = NULL;

	EnumWindows(EnumWindowCallBack, (LPARAM)&fwhs);

	SendMessage(fwhs.hwndFound, Msg, wParam, lParam);
	//Fix for loosing Focus, we reset the Focus on our own Main window
	SendMessage(fwhs.hwndFound, WM_KILLFOCUS,(WPARAM) hMain, (LPARAM) NULL);
}

#ifndef KAILLERA
static BOOL CALLBACK EnumWindowCallBack(HWND hwnd, LPARAM lParam) 
{ 
	FINDWINDOWHANDLE * pfwhs = (FINDWINDOWHANDLE * )lParam; 
	DWORD ProcessId; 
	char buffer[MAX_PATH]; 

	GetWindowThreadProcessId(hwnd, &ProcessId);

	// cmk--I'm not sure I believe this note is necessary
	// note: In order to make sure we have the MainFrame, verify that the title 
	// has Zero-Length. Under Windows 98, sometimes the Client Window ( which doesn't 
	// have a title ) is enumerated before the MainFrame 

	GetWindowTextA(hwnd, buffer, ARRAY_LENGTH(buffer));
	if (ProcessId  == pfwhs->ProcessInfo->dwProcessId &&
		 strncmp(buffer,MAMENAME,strlen(MAMENAME)) == 0) 
	{ 
		pfwhs->hwndFound = hwnd; 
		return FALSE; 
	} 
	else 
	{ 
		// Keep enumerating 
		return TRUE; 
	} 
}
#endif /* KAILLERA */

#ifdef KAILLERA
HWND GetGameWindowA(LPPROCESS_INFORMATION lpProcessInformation)
#else
HWND GetGameWindow(LPPROCESS_INFORMATION lpProcessInformation)
#endif /* KAILLERA */
{
	FINDWINDOWHANDLE fwhs;
	fwhs.ProcessInfo = lpProcessInformation;
	fwhs.hwndFound  = NULL;

	EnumWindows(EnumWindowCallBack, (LPARAM)&fwhs);
	return fwhs.hwndFound;
}

#endif

#ifdef USE_SHOW_SPLASH_SCREEN
static LRESULT CALLBACK BackMainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_ERASEBKGND:
		{
			BITMAP Bitmap;
			GetObject(hSplashBmp, sizeof(BITMAP), &Bitmap);
			BitBlt((HDC)wParam, 0, 0, Bitmap.bmWidth, Bitmap.bmHeight, hMemoryDC, 0, 0, SRCCOPY);
			break;
		}

		default:
			return (DefWindowProc(hWnd, uMsg, wParam, lParam));
	}

	return FALSE;
}

static void CreateBackgroundMain(HINSTANCE hInstance)
{
	static HDC hSplashDC = 0;

	WNDCLASSEX BackMainClass;

	BackMainClass.cbSize        = sizeof(WNDCLASSEX);
	BackMainClass.style         = 0;
	BackMainClass.lpfnWndProc   = (WNDPROC)BackMainWndProc;
	BackMainClass.cbClsExtra    = 0;
	BackMainClass.cbWndExtra    = 0;
	BackMainClass.hInstance     = hInstance;
	BackMainClass.hIcon         = NULL;
	BackMainClass.hIconSm       = NULL;
	BackMainClass.hCursor       = NULL;
	BackMainClass.hbrBackground = NULL;
	BackMainClass.lpszMenuName  = NULL;
	BackMainClass.lpszClassName = TEXT("BackMainWindowClass");

	if ( RegisterClassEx(&BackMainClass) )
	{
		BITMAP Bitmap;
		RECT DesktopRect;

		GetWindowRect(GetDesktopWindow(), &DesktopRect);
		hSplashBmp = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_SPLASH));
		GetObject(hSplashBmp, sizeof(BITMAP), &Bitmap);

		hBackMain = CreateWindowEx(WS_EX_TOOLWINDOW,
					TEXT("BackMainWindowClass"),
					TEXT("Main Backgound windows"),
					WS_POPUP,
					(DesktopRect.right - Bitmap.bmWidth) / 2,
					(DesktopRect.bottom - Bitmap.bmHeight) / 2,
					Bitmap.bmWidth,
					Bitmap.bmHeight,
					NULL,
					NULL,
					hInstance,
					NULL);

		hSplashDC = GetDC(hBackMain);
		hMemoryDC = CreateCompatibleDC(hSplashDC);
		SelectObject(hMemoryDC, (HGDIOBJ)hSplashBmp);

		if (GetDisplaySplashScreen() != FALSE)
			ShowWindow(hBackMain, SW_SHOW);

		UpdateWindow(hBackMain);
	}
}

static void DestroyBackgroundMain(void)
{
	static HDC hSplashDC = 0;

	if ( hBackMain )
	{
		DeleteObject(hSplashBmp);
		ReleaseDC(hBackMain, hSplashDC);
		ReleaseDC(hBackMain, hMemoryDC);
		DestroyWindow(hBackMain);
	}
}
#endif /* USE_SHOW_SPLASH_SCREEN */

#ifdef KAILLERA
/*-------------------------------------------------
    popmessage - pop up a user-visible message
-------------------------------------------------*/

void CLIB_DECL popmessageW(const WCHAR *text, ...)
{
	extern void CLIB_DECL ui_popup(const char *text, ...) ATTR_PRINTF(1,2);
	char *utf8_string;
	va_list arg;

	/* dump to the buffer */
	va_start(arg, text);
	vsnwprintf((WCHAR *)giant_string_buffer, GIANT_STRING_BUFFER_SIZE, text, arg);
	va_end(arg);

	/* pop it in the UI */
	utf8_string = utf8_from_wstring((WCHAR *)giant_string_buffer);
	if (utf8_string)
	{
		strcpy(giant_string_buffer, utf8_string);
		ui_popup("%s", giant_string_buffer);
		free(utf8_string);
	}
}

int WINAPI kGameCallback(char *game, int player, int numplayers)
{
	int nGame;
	LV_FINDINFO lvfi;
	int         i, j;
	//options_type *o;
	WCHAR t_game[1024];

    if (kPlay) return 0;
	Kaillera_StateSave_Count = 0;
	Kaillera_StateSave_Flags = 0;
	Kaillera_StateSave_Retry = 0;

	//kAnalog_input_port_end();
	KailleraStartOption.player = player;
	KailleraStartOption.numplayers = numplayers;
	KailleraPlayerOption.max	   = numplayers;
	memset (KailleraPlayerOption.drop_player, 0, sizeof(KailleraPlayerOption.drop_player));

	KailleraStartOption.auto_end					= GetKailleraAutoEnd();
	KailleraStartOption.send_file_speed				= GetKailleraSendFileSpeed();
	KailleraStartOption.autosave_time_interval		= 65536;// dummy   GetKailleraAutosaveTimeInterval();
	KailleraStartOption.lost_connection_time		= GetKailleraLostConnectionTime();
	KailleraStartOption.lost_connection_operation	= GetKailleraLostConnectionOperation();

	i=0; j=numplayers;
	while ( j-- )
	{
		KailleraPlayerOption.drop_player[i>>3] |= (1<<(i & 0x3));
		i++;
	}
	
	KailleraPlayerOption.waittimemode = 1;
	KailleraMaxWait = (osd_ticks_t)UCLOCKS_PER_SEC>>2;
	KailleraPlayerOption.waittimemode = 2;
	KailleraPlayerOption.waittimemode = 0;

	PreparationcheckReset();
	//kt e

	if (GetNetPlayFolder())
		SetCurrentFolder(GetFolderByName(TEXT("Favorites")));
	else
		SetCurrentFolder(GetFolderById(FOLDER_AVAILABLE));
	SetSortReverse(FALSE);
	SetSortColumn(COLUMN_GAMES);
	SetView(ID_VIEW_GROUPED);
	ResetListView();

	lvfi.flags = LVFI_STRING;
	wcscpy((LPTSTR)t_game, _Unicode(game));
	lvfi.psz   = (LPTSTR)t_game;
	i = ListView_FindItem(hwndList, -1, &lvfi);

	if (i == -1)
	{
		char szMsg[256];

		sprintf( szMsg, "ListView_FindItem returned -1\n\ngame:<%s>\nplayer:%d\nnumplayer:%d",
			game, player, numplayers );
		MessageBox( NULL, _Unicode(szMsg), TEXT("kGameCallback"), MB_OK );
		kailleraEndGame();
		return 0;
	}

	Picker_SetSelectedPick(hwndList, i);
	nGame = Picker_GetSelectedItem(hwndList);

	srand(0xbadc0de);

	{
		char	kailleraver[20];

		memset(kailleraver, 0, 20);
		kailleraGetVersion(kailleraver);
#ifdef HAZEMD
		sprintf(kailleraGame_mameVer,"%dp %s %s Kaillera %s", KailleraStartOption.player, "HazeMD++", build_version, kailleraver);
#else
		sprintf(kailleraGame_mameVer,"%dp %s %s Kaillera %s", KailleraStartOption.player, MAME32NAME "++", build_version, kailleraver);
#endif /* HAZEMD */
		//sprintf(kailleraGame_mameVer,"%dp %s %s %s Kaillera %s", KailleraStartOption.player, MAME32NAME "++", build_version, BUILD_COMPILER, kailleraver);
	}

	//StartReplay();
    kPlay = 1;
	{
		BOOL record = GetKailleraRecordInput();
		if(HIBYTE(GetAsyncKeyState(VK_SHIFT)) && HIBYTE(GetAsyncKeyState(VK_CONTROL)))
			record = !record;

		if(record == TRUE)
			KailleraTraceRecordGame();
		else
		    MamePlayGame();
	}
	Kaillera_Emerald_End();
	if (!(Kaillera_StatusFlag & KAILLERA_STATUSFLAG_LOST_CONNECTION)
		|| !KailleraStartOption.lost_connection_time )
	{
		kailleraEndGame();
	}
    kPlay = 0;
	RePlay = 0;

    return 0;
}

void WINAPI kChatCallback(char *nick, char *text)
{
    char tmp[512];

	//kt start
	if( (*((long*)text)) == 0x41440a0d ) {
		int dat[64];
		int ChatDataLen;

		ChatDataLen = kChatReData(&kChatDataBuf[0], text+4);

		switch ( KAILLERA_CHATDATA_GET_COMMAND( kChatDataBuf[0] ) )
		{
		case 1: // ステートセーブのCRCが送られてきた。
			if(Kaillera_StateSave_Count <= 0) break;

			if( (UINT32)Kaillera_StateSave_CRC != (UINT32)kChatDataBuf[1] ) {
				popmessageW(_UIW(TEXT("Maybe Desync")));
				Kaillera_StateSave_Count = 0;
				break;
			}
			Kaillera_StateSave_Count--;
			if(Kaillera_StateSave_TimeRemainder < KAILLERA_STATESAVE_NORMAL_DELAYTIME/2)
				Kaillera_StateSave_TimeRemainder = KAILLERA_STATESAVE_NORMAL_DELAYTIME/2;


			if(Kaillera_StateSave_Count <= 0) { // CRCが全員一致したので、st@をstaにコピー
				char name[2];
				name[0] = Kaillera_StateSave_file;
				name[1] = 0;
				{
					//int flag;
					WCHAR fname_src[MAX_PATH];
					WCHAR fname_dest[MAX_PATH];
					wsprintf(fname_dest, TEXT("%s/%s/%s-%s.sta"), GetStateDir(), _Unicode(Machine->basename), _Unicode(Machine->gamedrv->name), _Unicode(name));
					name[0] = '@';
					wsprintf(fname_src, TEXT("%s/%s/%s-%s.sta"), GetStateDir(), _Unicode(Machine->basename), _Unicode(Machine->gamedrv->name), _Unicode(name));
					CopyFileW(fname_src, fname_dest, FALSE);
				}

				Kaillera_StateSave_Retry = 0;
			}
			break;
		case 2: // ステートセーブ、ロードで使用する拡張子の変更（初期値は'a'  (.sta)）
			Kaillera_StateSave_file = kChatDataBuf[1];
			popmessageW(_UIW(TEXT("%c-slot is selected")), Kaillera_StateSave_file);

			dat[0] = 0x0000000f;
			dat[1] = 0x00000002;
			kailleraChatSend(kChatData(&dat[0], 8));
			break;
		case 5: // mameのバージョンチェック
			{
				kailleraChatSend(kailleraGame_mameVer);
			}
			break;
		case 6: // mameのバージョンを受信
			break;
		case 7:
			{
				WCHAR tmp[512];
#if 0
				int cpu;
				double overclock;

				overclock = (double)kChatDataBuf[1] * 0.5;
				if (overclock < 0.5 || overclock > 3.0) overclock = 1.0;
				for( cpu = 0; cpu < cpu_gettotalcpu(); cpu++ )
					cpunum_set_clockscale(cpu, overclock);
#endif
				Kaillera_Overclock_Multiple = kChatDataBuf[1];
				if (Kaillera_Overclock_Multiple < 1 || Kaillera_Overclock_Multiple > 8) Kaillera_Overclock_Multiple = 2;
				wsprintf(tmp, _UIW(TEXT("CPUs Overclocked %d %%")), (int)(Kaillera_Overclock_Multiple * 50));
				//kailleraChatSend(tmp);
				popmessageW(tmp);
				dat[0] = 0x0000000f;
				dat[1] = 0x00000007;
				kailleraChatSend(kChatData(&dat[0], 8));
			}
			break;
		case 8: // ディップスイッチの状態を受信
			{
				int i;
				extern int kMaxDipSwitch;
				extern UINT32 kDipSwitchValues[MAX_INPUT_PORTS][2];
				extern UINT32 kDefValues[MAX_INPUT_PORTS];
				unsigned short *val = (UINT16*)&kChatDataBuf[1];
				UINT32 dip, mask;
				
				i=0;
				for(i=0;i<kMaxDipSwitch;i++) {
					mask = kDipSwitchValues[i][1];
					dip = ((UINT32)(*val)) & mask;
#if 0
		if(0){
		FILE *fp;
		fp = fopen("a.txt","a");
					fprintf(fp,"[%d]:mask=%x,dip=%x,def=%x\n", i,mask,dip,kDefValues[kDipSwitchValues[i][0]]);
		fclose(fp);
		}
#endif
					kDefValues[kDipSwitchValues[i][0]] = (kDefValues[kDipSwitchValues[i][0]] & (~mask)) | dip;
					val++;
				}
#if 0
		if(0){
		FILE *fp;
		fp = fopen("a.txt","a");
					fprintf(fp,"kDefValues[0] = %x\n", kDefValues[0]);
					fprintf(fp,"kDefValues[1] = %x\n", kDefValues[1]);
					fprintf(fp,"kDefValues[2] = %x\n", kDefValues[2]);
					fprintf(fp,"kDefValues[3] = %x\n", kDefValues[3]);
					fprintf(fp,"kDefValues[4] = %x\n", kDefValues[4]);
					fprintf(fp,"kDefValues[5] = %x\n\n\n", kDefValues[5]);
		fclose(fp);
		}
#endif

				dat[0] = 0x0000000f;
				dat[1] = 0x00000008;
				kailleraChatSend(kChatData(&dat[0], 8));
			}
			break;
		case 9: // ファイル受信開始。
			if( KailleraStartOption.player == 1 ) {
//				if( Kaillera_Send_Flags &= 0x1 ) popmessage("送信状況 %d％", (int)((double)kChatDataBuf[1]/(double)Kaillera_Send_Len * 100));
				if( Kaillera_Send_Flags & 0x1 ) popmessageW(_UIW(TEXT("Sending %d percent")), (int)((double)kChatDataBuf[1]/(double)Kaillera_Send_Len * 100));
			} else {
				//unsigned long syslen = 8;
				//int pos = 2;
				//int f = 0;
				UINT32 crc = 0;
				UINT64 size = 0;
				Kaillera_Send_Len			= kChatDataBuf[2];
				if( Kaillera_Send_Flags & 0x2 )
				{
					if( Kaillera_Send_Len == 0 )
					{	//	転送中止
						Kaillera_Send_Flags &= ~0x2;
						Kaillera_Send_SleepTime = 0;
						if( lpkChatDatabit )	free( lpkChatDatabit );
						lpkChatDatabit = 0;
						popmessageW(_UIW(TEXT("All members' save data matched")));
					}
					break;
				}
				// データ受信準備
				
				Kaillera_Send_DecompressLen	= kChatDataBuf[3];
				Kaillera_Send_Pos			= 0;
				Kaillera_Send_lpBuf = malloc( Kaillera_Send_Len );
				lpkChatDatabit = Kaillera_Send_lpBuf;
				Kaillera_Send_Flags |= 0x2;
				Kaillera_Send_SleepTime = 600;

				{
					//int flag;
					file_error filerr;
					mame_file *file;
					char filename[MAX_PATH];
					char name[2];
					name[0] = Kaillera_StateSave_file; name[1] = 0;
					sprintf(filename, "%s%s%s-%s.sta", Machine->basename, PATH_SEPARATOR, Machine->gamedrv->name, name);
					filerr = mame_fopen(SEARCHPATH_STATE, filename, OPEN_FLAG_READ, &file);
					if (filerr != FILERR_NONE)
						filerr = mame_fopen(SEARCHPATH_STATE, filename, OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS, &file);
					if (filerr == FILERR_NONE)
						mame_fclose(file);
					checksum_file_crc32(SEARCHPATH_STATE, filename, NULL, &size, &crc);
				}
				
				dat[0] = 0x0000000f;
				dat[1] = 0x00000009;
				dat[2] = 0x00000000;
				dat[3] = KailleraStartOption.send_file_speed;
				dat[4] = crc;
				dat[5] = (int)size;
				kailleraChatSend(kChatData(&dat[0], 6*sizeof(dat[0])));
			}
			break;
		case 10:
			break;
		case 11:	// 同期ズレチェック
			{
				if ( KAILLERA_CHATDATA_GET_PLAYERNMB( kChatDataBuf[0] ) == KailleraStartOption.player)
					break;

				if (kChatDataBuf[1] <= synccount)	//error
				{
					dat[0] = 0x0000000f | ((KailleraStartOption.player & 0xff) << 24);
					dat[1] = 0x0000000b;
					dat[2] = 0x80000000;
					dat[3] = (synccount+1) - kChatDataBuf[1];
					kailleraChatSend(kChatData(&dat[0], 4*4));
				}

				if ( KailleraChatdataPreparationcheck.flag & 0x80000000 )
					break;

				if (KailleraChatdataPreparationcheck.nmb == KAILLERA_CHATDATA_GET_COMMAND( kChatDataBuf[0] ))
				{
					KailleraChatdataPreparationcheck.flag |= 0x80000000;
					popmessageW(_UIW(TEXT("Sync Check Failure")));
				}
				
				if (KailleraSyncCheck.count)
				{
					dat[0] = 0x0000000f | ((KailleraStartOption.player & 0xff) << 24);
					dat[1] = 0x0000000b;
					dat[2] = 0x80000000;
					dat[3] = 0;
					kailleraChatSend(kChatData(&dat[0], 4*4));
					KailleraSyncCheck.count = 0;
					break;
				}
				
				{
					const unsigned short steppos = kChatDataBuf[2] & 0xffff, count = (kChatDataBuf[2] & 0xffff0000)>>16;
					KailleraSyncCheck.basepos		= kChatDataBuf[1];
					KailleraSyncCheck.totalcount	= count;			
					KailleraSyncCheck.step			= steppos;
					KailleraSyncCheck.count			= KailleraSyncCheck.totalcount;
					KailleraSyncCheck.pos			= KailleraSyncCheck.basepos + KailleraSyncCheck.step;
				}

			}
			break;
		case 12:	// ゲーム強制終了
			{
				//extern int quiting;

				if( kChatDataBuf[1] >>(KailleraStartOption.player-1) & 0x1)
				{
					extern void KailleraChatEnd(void);
					quiting = 2;

					KailleraChatEnd();
				}
			}
			break;
		case 13:	// 時間調整レベル変更
			{
				extern void KailleraThrottleChange(int mode);
				int wtm = KailleraPlayerOption.waittimemode;
				KailleraPlayerOption.waittimemode = kChatDataBuf[1] % 5;

				if(wtm == 0 && KailleraPlayerOption.waittimemode > 0)
				{
					KailleraThrottleChange(0);
					//DirectSound_ModeChange(0);
				}

				if(wtm > 0 && KailleraPlayerOption.waittimemode == 0)
				{
					KailleraThrottleChange(1);
					//DirectSound_ModeChange(1);
				}

			}
			popmessageW(_UIW(TEXT("The time regulation level was set as %d")), KailleraPlayerOption.waittimemode);
			break;
		case 14:
			break;
		case 15:
			if( kChatDataBuf[1] != KailleraChatdataPreparationcheck.nmb ||
				KailleraChatdataPreparationcheck.timeremainder<=0 ||
				KailleraChatdataPreparationcheck.count <=	0 ) break;

			KailleraChatdataPreparationcheck.timeremainder += KailleraChatdataPreparationcheck.addtime;
			if( KailleraChatdataPreparationcheck.timeremainder > KailleraChatdataPreparationcheck.maxtime)
				KailleraChatdataPreparationcheck.timeremainder = KailleraChatdataPreparationcheck.maxtime;

			if (ChatDataLen <= 2*4)
			{
				kChatDataBuf[2] = 0;
				kChatDataBuf[3] = 0;
			}

			KailleraChatdataPreparationcheck.count--;

			if( kChatDataBuf[2] & 0x80000000) { //中止
				KailleraChatdataPreparationcheck.flag |= 0x80000000;
				(*KailleraChatdataPreparationcheck.Callback_Update)( 0x80000000, &kChatDataBuf[0] );
				
				if( KailleraChatdataPreparationcheck.count<=0 ) {
					(*KailleraChatdataPreparationcheck.Callback)( 1 );
					KailleraChatdataPreparationcheck.nmb			= 0;
					KailleraChatdataPreparationcheck.timeremainder	= 0;
				}
			} else
			{
				(*KailleraChatdataPreparationcheck.Callback_Update)( 0, &kChatDataBuf[0] );
				if( KailleraChatdataPreparationcheck.count<=0 ) {
					(*KailleraChatdataPreparationcheck.Callback)( 0 );
					KailleraChatdataPreparationcheck.nmb			= 0;
					KailleraChatdataPreparationcheck.timeremainder	= 0;
				}

			}
			break;
		default:;
		}
		return;
	} /* DA */

	if( (*((long*)text)) == 0x42440a0d ) {
		file_error filerr;
		unsigned long len;
		int f = 0;
		
		if( KailleraStartOption.player == 1 )
		{
			//if( Kaillera_Send_Flags &= 0x1 ) popmessage("送信状況 %d％", (int)((double)Kaillera_Send_Pos/(double)Kaillera_Send_Len * 100));
			if( Kaillera_Send_Flags & 0x1 ) popmessageW(_UIW(TEXT("Sending %d percent")), (int)((double)Kaillera_Send_Pos/(double)Kaillera_Send_Len * 100));
			return;
		}
		
		if( lpkChatDatabit == 0 )			return;
		if( ~Kaillera_Send_Flags & 0x2 )	return;
		
		len = kChatReDatabit(lpkChatDatabit + Kaillera_Send_Pos, text+4);
		
		Kaillera_Send_SleepTime += 120;
		if( Kaillera_Send_SleepTime > 600 ) Kaillera_Send_SleepTime = 600;
		Kaillera_Send_Pos += len;
		if( Kaillera_Send_Pos >= Kaillera_Send_Len) {
			f = 1;
		}
		popmessageW(_UIW(TEXT("Receiving %d percent")), (int)((double)Kaillera_Send_Pos/(double)Kaillera_Send_Len * 100));

		if(f == 1) { //最後のデータ受信完了
			char *temp = 0;
			char fname[MAX_PATH];
			int zl,tst = Kaillera_Send_DecompressLen;
			temp = malloc( Kaillera_Send_DecompressLen );
			zl = uncompress((Bytef *)temp, &Kaillera_Send_DecompressLen, (Bytef *)lpkChatDatabit, Kaillera_Send_Len);
			if( zl == Z_OK) {
				mame_file *file;
				char name[2];
				name[0] = Kaillera_StateSave_file;
				name[1] = 0;
				{
					//int flag;
					sprintf(fname, "%s/%s-%c.sta", Machine->basename, Machine->gamedrv->name, name[0]);
					filerr = mame_fopen(SEARCHPATH_STATE, fname, OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS, &file);
				}
				mame_fwrite(file, temp, tst);
				mame_fclose(file);
				popmessageW(_UIW(TEXT("Reception completed. %s-%c.sta successfully saved.")), _Unicode(Machine->gamedrv->name), Kaillera_StateSave_file);
			}
			if( zl == Z_MEM_ERROR) popmessage("Z_MEM_ERROR" );
			if( zl == Z_BUF_ERROR) popmessage("Z_BUF_ERROR" );
			if( zl == Z_DATA_ERROR)popmessage("Z_DATA_ERROR" );
			Kaillera_Send_Flags &= ~0x2;
			Kaillera_Send_SleepTime = 0;
			if( temp )				free( temp );
			if( lpkChatDatabit )	free( lpkChatDatabit );
			lpkChatDatabit = 0;
		}
		return;
	} /* DB */
	//kt end

    sprintf(tmp, "<%s> %s", nick, text);
	tmp[255] = 0;
    KailleraChateReceive(tmp);
}

void WINAPI kDropCallback(char *nick, int playernb)
{
    char tmp[512];

    sprintf(tmp, _String(_UIW(TEXT("* Player %i (%s) dropped from the current game."))), playernb, nick);
    KailleraChateReceive(tmp);
    
	//kt start
	if( Kaillera_StateSave_Flags & KAILLERA_STATESAVE_AUTOSAVE )
		popmessageW(_UIW(TEXT("Autosave was discontinued")));
	Kaillera_StateSave_Count = 0;
	Kaillera_StateSave_Flags = 0;
	Kaillera_StateSave_Retry = 0;
	if(--KailleraPlayerOption.max < 0) KailleraPlayerOption.max = 0;
	
	if (playernb)
	{
		KailleraPlayerOption.drop_player[(playernb-1)>>3] &= ~(1<<((playernb-1) & 0x3));
		
		if (KailleraStartOption.auto_end)
		{
			if (playernb <= KailleraStartOption.auto_end || KailleraStartOption.auto_end == -1)
			{
				extern void KailleraChatEnd(void);
				//extern int quiting;
				quiting = 2;
				KailleraChatEnd();
			}

		}
	}

	//kt end
}

void WINAPI kInfosCallback(char *gamename)
{
}

void __cdecl SendDipSwitch(int flag)
{
	if(flag) {
		return;
	}
	Kaillera_StateSave_Flags |= KAILLERA_FLAGS_RESET_MACHINE;
	kailleraChatSend(_String(_UIW(TEXT("Dipswitches Changed"))));
}

void __cdecl SendOverclockParam(int flag)
{
	//char tmp[512];
	if(flag) {
		return;
	}
	
	//sprintf(tmp, _UI("CPUs Overclocked %d %%"), (int)(Kaillera_Overclock_Multiple * 50));
	Kaillera_StateSave_Flags |= KAILLERA_FLAGS_RESET_MACHINE;
	//kailleraChatSend(tmp);
}

void __cdecl SendSyncCheck(int flag)
{
	int i,j;
	unsigned long crc;
	char desync_player[8];	//8bit x 8 = 最大64人分
	char str[512];
	int PerfectCRC = 0;

	if (KailleraChatdataPreparationcheck.flag & 0x80000000)
	{
		if( KailleraChatdataPreparationcheck.count<=0 ) {
			//(*KailleraChatdataPreparationcheck.Callback)( 1 );
			KailleraChatdataPreparationcheck.nmb			= 0;
			KailleraChatdataPreparationcheck.timeremainder	= 0;
			KailleraChatdataPreparationcheck.flag			= 0;
		}
		popmessageW(_UIW(TEXT("Sync Check Failure")));
		return;
	}

	if(flag == 1) {	//タイムアウト
		return;
	}

	memset (desync_player, 0, sizeof(desync_player));

	for (j=0; j<KailleraSyncCheck.totalcount; j++)
	{
		crc = KailleraSyncCheck.crc[KailleraStartOption.player-1][j];
		for (i=0; i<KailleraStartOption.numplayers; i++)
		{
			if ((KailleraPlayerOption.drop_player[i>>3] & (1<<(i & 0x3))) 
				) //&& i != KailleraStartOption.player )
			{
				if (crc == KailleraSyncCheck.crc[i][j])
					desync_player[i>>3] |= (1<<(i & 0x3));
				else
					PerfectCRC++;
			}
		}
	}

	crc = 0;
	str[0] = 0;
	for (i=0; i<KailleraStartOption.numplayers; i++)
	{
		if ( KailleraPlayerOption.drop_player[i>>3] & (1<<(i & 0x3)) )
		{
			if ( !(desync_player[i>>3] & (1<<(i & 0x3))) )
			{
				char bf[16];
				sprintf(bf, "%up,", i+1);
				crc++;
				strcat(str,bf);
			}
		}
	}
	
	if (crc)
	{
		WCHAR tmp[256];
		str[strlen(str)-1] = 0;
		wsprintf(tmp, _UIW(TEXT("Maybe Desync %s")), _Unicode(str));

		popmessageW(tmp);
	} else
	{
		if (PerfectCRC)
			popmessageW(_UIW(TEXT("Sync Check End")));
		else
			popmessageW(_UIW(TEXT("All members' Synchronization is perfect")));
	}



}

void __cdecl SendSyncCheck_Update(int flag, unsigned long *data)
{
	const int player = KAILLERA_CHATDATA_GET_PLAYERNMB( data[0] );
	if(flag == 0x80000000) {	//中止

		if (data[3] > KailleraPlayerOption.chatsend_timelag)
			KailleraPlayerOption.chatsend_timelag = data[3];
		return;
	}

	if (player != KailleraStartOption.player)
		memcpy ( &KailleraSyncCheck.crc[player-1][0], &data[3], sizeof(long) * KailleraSyncCheck.totalcount);
}

void __cdecl SendStateSaveFile(int flag)
{
	if (flag)
	{
		Kaillera_Send_Flags = 0;
		if( lpkChatDatabit )
			free(lpkChatDatabit);
		lpkChatDatabit = 0;
		Kaillera_Send_SleepTime = 0;
		return;
	}

	if (~Kaillera_Send_Flags & 0x4)	//全員staファイルのCRCが一致。
	{
		int dat[64];
		dat[0] = KailleraChatdataPreparationcheck.nmb;
		dat[1] = 0;
		dat[2] = 0;//	len
		dat[3] = 0;
		dat[4] = 0;
		kailleraChatSend(kChatData(&dat[0], 5*4));

		Kaillera_Send_Flags = 0;
		if( lpkChatDatabit )
			free(lpkChatDatabit);
		lpkChatDatabit = 0;
		Kaillera_Send_SleepTime = 0;
		Kaillera_StateSave_TimeRemainder = KAILLERA_STATESAVE_NORMAL_DELAYTIME-1;
		popmessageW(_UIW(TEXT("All the members' save data was coincidenced")));
		return;
	}
	Kaillera_Send_Flags &= ~0x4;

	Kaillera_Send_Flags |= 0x1;
}

void __cdecl SendStateSaveFile_Update(int flag, unsigned long *data)
{
	if (data[3] > KailleraPlayerOption.sendfilespeed)
		KailleraPlayerOption.sendfilespeed = data[3];

	if (data[4] != Kaillera_Send_CRC ||
		data[5] != Kaillera_Send_DecompressLen )
		Kaillera_Send_Flags |= 0x4;

}

#include <float.h>

unsigned int FloatControlReg = 0;
unsigned int Get_FloatControlReg(void)
{
	return _control87( 0, 0 );
}

static void init_FloatControl(void)
{
   FloatControlReg	=	_control87( _IC_AFFINE , _MCW_IC );
   FloatControlReg	=	_control87( _PC_53 , _MCW_PC );
}

void ResetReplay(void)
{
	if (RePlay)
		srand(0xbadc0de);
}

static void StartReplay(void)
{
	//extern int time_to_reset;

	init_FloatControl();
	RePlay = 1;
	ResetReplay();

	//time_to_reset = 2;
}


static void MKInpDir(void)
{
	const WCHAR* dirname = NULL;
	WCHAR dir[256];

	//dirname = GetInpDir();
	dirname = TEXT("inp");

	CreateDirectoryW(dirname, NULL);

	wsprintf(dir, TEXT("%s\\trctemp"), dirname);
	CreateDirectoryW(dir, NULL);
}

static void KailleraTraceRecordGame(void)
{
	int  nGame;
    WCHAR filename[MAX_PATH];
    WCHAR filename_trc[MAX_PATH];
	static	int	num_record = 0;
	static char oldname[256];
	oldname[255] = 0;

	nGame = Picker_GetSelectedItem(hwndList);

	if ( strcmp(drivers[nGame]->name, oldname) )
	{
		strcpy(oldname, drivers[nGame]->name);
		num_record = 0;
	}

	MKInpDir();
	wsprintf(kaillera_recode_filename, TEXT("k%02d"), num_record);
	wsprintf(filename, TEXT("%s_%s.inp"), _Unicode(drivers[nGame]->name), kaillera_recode_filename);
	wsprintf(filename_trc, TEXT("%s_%s.trc"), _Unicode(drivers[nGame]->name), kaillera_recode_filename);

	num_record = (num_record + 1) % 100;

	g_pRecordName = filename;
	g_pRecordSubName = filename_trc;
	g_pAutoRecordName = kaillera_recode_filename;
	override_playback_directory = NULL;
	MamePlayGameWithOptions(nGame);
	g_pRecordSubName = NULL;
	g_pRecordName = NULL;
	g_pAutoRecordName = NULL;
	override_playback_directory = NULL;
}

#define KAILLERA_AUTOEND_ALL 17

struct FILEDATA
{

	WCHAR pszPathName[MAX_PATH+1];
	BOOL bFolder;

} fd={TEXT(""),FALSE};

INT_PTR CALLBACK KailleraOptionDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HWND hCtrl;
	WCHAR buf[256];
	int j;

    switch (Msg)
    {
    case WM_INITDIALOG:
		TranslateDialog(hDlg, lParam, TRUE);
		hCtrl = GetDlgItem(hDlg, IDC_KAILLERA_AUTO_END);
		if (hCtrl)
		{
			static const WCHAR *autoend_values[]={
				TEXT("None"),
				TEXT("1player"),
				TEXT("1p~2p"),
				TEXT("1p~3p"),
				TEXT("1p~4p"),
				TEXT("1p~5p"),
				TEXT("1p~6p"),
				TEXT("1p~7p"),
				TEXT("1p~8p"),
				TEXT("1p~9p"),
				TEXT("1p~10p"),
				TEXT("1p~11p"),
				TEXT("1p~12p"),
				TEXT("1p~13p"),
				TEXT("1p~14p"),
				TEXT("1p~15p"),
				TEXT("1p~16p"),
				TEXT("All")
			};
			int i = GetKailleraAutoEnd();
			if (i == -1) i = KAILLERA_AUTOEND_ALL;
			for (j = 0; j < 18; j++)
				ComboBox_AddString(hCtrl, _UIW(autoend_values[j]));
			ComboBox_SetCurSel(hCtrl, i);
		}

		hCtrl = GetDlgItem(hDlg, IDC_KAILLERA_SEND_FILE_SPEED);
		if (hCtrl)
		{
			static const WCHAR *filespeed_values[]={
				TEXT("4 times"),
				TEXT("3 times"),
				TEXT("2 times"),
				TEXT("Normal"),
				TEXT("1/2"),
				TEXT("1/3"),
				TEXT("1/4"),
				TEXT("1/5"),
				TEXT("1/6"),
				TEXT("1/7"),
				TEXT("1/8"),
				TEXT("1/9"),
				TEXT("1/10"),
				TEXT("1/11"),
				TEXT("1/12"),
				TEXT("1/13"),
				TEXT("1/14"),
				TEXT("1/15"),
				TEXT("1/16")
			};
			int i = GetKailleraSendFileSpeed();
			if (i==0 || i<-4 || i>=16) i=0;

			if (i < 0) i = 4+i;
			else
				i+=3;
			for (j = 0; j < 19; j++)
				ComboBox_AddString(hCtrl, _UIW(filespeed_values[j]));
			ComboBox_SetCurSel(hCtrl, i);
		}

		hCtrl = GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_TIME);
		if (hCtrl)
		{
			static const WCHAR *conntime_values[]={
				TEXT("0"),
				TEXT("9000"),
				TEXT("10000"),
				TEXT("15000"),
				TEXT("20000"),
				TEXT("30000"),
				TEXT("60000"),
				TEXT("120000"),
				TEXT("180000")
			};
			unsigned int i = GetKailleraLostConnectionTime();
			for (j = 0; j < 9; j++)
				ComboBox_AddString(hCtrl, conntime_values[j]);
			wsprintf(buf, TEXT("%u"), i);
			Edit_SetText(hCtrl,        buf);

			if (i == 0)
			{
				Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_WINDOW_MODE),		FALSE);
				Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END),				FALSE);
				Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END_ALL_PLAYERS),	FALSE);
			} else
			{
				Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_WINDOW_MODE),		TRUE);
				Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END),				TRUE);
				Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END_ALL_PLAYERS),	TRUE);
			}
		}

		hCtrl = GetDlgItem(hDlg, IDC_KAILLERA_AUTOSAVE_TIME_INTERVAL);
		if (hCtrl)
		{
			static const WCHAR *autosave_values[]={
				TEXT("45"),
				TEXT("60"),
				TEXT("69"),
				TEXT("120"),
				TEXT("180"),
				TEXT("240"),
				TEXT("300")
			};
			for (j = 0; j < 7; j++)
				ComboBox_AddString(hCtrl, autosave_values[j]);
			ComboBox_LimitText(hCtrl, 5);
			wsprintf(buf, TEXT("%u"), GetKailleraAutosaveTimeInterval());
			Edit_SetText(hCtrl,        buf);
		}
        

        Button_SetCheck(GetDlgItem(hDlg, IDC_SHOW_SYSTEM_MESSAGE),      GetShowSystemMessage());
  

		hCtrl = GetDlgItem(hDlg, IDC_KAILLERA_MAME32WINDOW_OWNER);
		if (hCtrl)
		{
			BOOL b = GetKailleraMAME32WindowOwner();
			Button_SetCheck(hCtrl,      b);

			if (b == TRUE)
			{
				Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_CHANGES_TO_JAPANESE),		TRUE);
				Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_CHANGES_TO_JAPANESE),      GetKailleraClientChangesToJapanese());
			} else
			{
				Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_CHANGES_TO_JAPANESE),		FALSE);
				Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_CHANGES_TO_JAPANESE),      FALSE);
			}
		}
        Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_MAME32WINDOW_HIDE),      GetKailleraMAME32WindowHide());
		Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_RECORD_INPUT),      GetKailleraRecordInput());
		Button_SetCheck(GetDlgItem(hDlg, IDC_LOCAL_RECORD_INPUT),      GetLocalRecordInput());

		Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_WINDOW_MODE),		(GetKailleraLostConnectionOperation() == KAILLERA_LOST_CONNECTION_OPERATION_WINDOW_MODE)		? TRUE:FALSE);
		Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END),				(GetKailleraLostConnectionOperation() == KAILLERA_LOST_CONNECTION_OPERATION_END)				? TRUE:FALSE);
		Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END_ALL_PLAYERS),	(GetKailleraLostConnectionOperation() == KAILLERA_LOST_CONNECTION_OPERATION_END_ALL_PLAYERS)	? TRUE:FALSE);




		hCtrl = GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_DLL);
		if (hCtrl)
		{
			HANDLE hFile;
			WIN32_FIND_DATA w32FindData;
			WCHAR buf2[_MAX_PATH];
			WCHAR *dirname = (WCHAR *)GetKailleraDir();

			CreateDirectoryW(dirname, NULL);

			wsprintf (buf2, TEXT("%s\\*.dll"), dirname);
			hFile=FindFirstFile(buf2, &w32FindData);
			
			if(hFile!=INVALID_HANDLE_VALUE)
			{
				do
				{
					wcscpy(fd.pszPathName,w32FindData.cFileName);
					fd.bFolder = (w32FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE:FALSE;

					if(fd.bFolder==FALSE)
					{
						//char drive[_MAX_DRIVE];
						//char dir[_MAX_DIR];
						WCHAR fname[_MAX_FNAME];
						//char ext[_MAX_EXT];
						_wsplitpath(fd.pszPathName, NULL, NULL, fname, NULL);
						ComboBox_AddString(hCtrl, fname);
					}
				}

				while(FindNextFile(hFile,&w32FindData));

				FindClose(hFile);
			}

			{
				WCHAR buf[256];
				char kailleraver[16];
				if (Kaillera_GetVersion("kailleraclient.dll", kailleraver) == TRUE)
				{
					wsprintf (buf, TEXT("kailleraclient.dll  %s"), _Unicode(kailleraver));
				} else
				{
					wcscpy (buf, _UIW(TEXT("kailleraclient.dll  (not found)")));
				}
				ComboBox_InsertString(hCtrl, 0, buf);
			}
			{
				const WCHAR *str;
				int nmb;
				str = GetKailleraClientDLL();
				nmb = ComboBox_FindStringExact(hCtrl, 0, str );
				if (nmb == CB_ERR) nmb = 0;
				ComboBox_SetCurSel(hCtrl, nmb);
			}
		}

		return TRUE;

    case WM_HELP:
        break;

    case WM_CONTEXTMENU:
        break;

    case WM_COMMAND :
        switch (GET_WM_COMMAND_ID(wParam, lParam))
        {

		case IDC_KAILLERA_MAME32WINDOW_OWNER:
			{
				const BOOL b = Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_MAME32WINDOW_OWNER));

				if (b == TRUE)
				{
					Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_CHANGES_TO_JAPANESE),		TRUE);
					//Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_CHANGES_TO_JAPANESE),      GetKailleraClientChangesToJapanese());
				} else
				{
					Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_CHANGES_TO_JAPANESE),		FALSE);
					Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_CHANGES_TO_JAPANESE),      FALSE);
				}
			}
			break;

        case IDOK :
			hCtrl = GetDlgItem(hDlg, IDC_KAILLERA_AUTO_END);
			if (hCtrl)
			{
				int i = ComboBox_GetCurSel(hCtrl);
				if (i == KAILLERA_AUTOEND_ALL) i=-1;
				SetKailleraAutoEnd(i);
			}

			hCtrl = GetDlgItem(hDlg, IDC_KAILLERA_SEND_FILE_SPEED);
			if (hCtrl)
			{
				int i = ComboBox_GetCurSel(hCtrl) - 3;
				if (i < 0) i-=1;
				SetKailleraSendFileSpeed(i);
			}

			Edit_GetText(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_TIME), (LPTSTR)buf, 100);
			if (buf[0] != 0)
			{
				unsigned int i;
				sscanf(_String((LPTSTR)buf),"%u", &i);
				if (i<1000 && i!=0 )		i=1000;
				SetKailleraLostConnectionTime(i);
			}

			Edit_GetText(GetDlgItem(hDlg, IDC_KAILLERA_AUTOSAVE_TIME_INTERVAL), (LPTSTR)buf, 100);
			if (buf[0] != 0)
			{
				int i;
				sscanf(_String((LPTSTR)buf),"%d", &i);
				if (i<20)		i=20;
				if (i>65535)	i=65535;
				SetKailleraAutosaveTimeInterval(i);
			}


            SetShowSystemMessage(Button_GetCheck(GetDlgItem(hDlg, IDC_SHOW_SYSTEM_MESSAGE)));
            //SetKailleraMAME32WindowOwner(Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_MAME32WINDOW_OWNER)));
			//SetKailleraClientChangesToJapanese(Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_CHANGES_TO_JAPANESE)));
            {
				BOOL b = Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_MAME32WINDOW_OWNER));
				SetKailleraMAME32WindowOwner(b);
				if (b == TRUE)
				{
					b = Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_CHANGES_TO_JAPANESE));
				} else
				{
					b = FALSE;
				}
				SetKailleraClientChangesToJapanese(b);
			}
			
            SetKailleraMAME32WindowHide(Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_MAME32WINDOW_HIDE)));
			SetKailleraRecordInput(Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_RECORD_INPUT)));
			SetLocalRecordInput(Button_GetCheck(GetDlgItem(hDlg, IDC_LOCAL_RECORD_INPUT)));

			{
				int i = KAILLERA_LOST_CONNECTION_OPERATION_NONE;
				if (Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_WINDOW_MODE)) == TRUE)		i=KAILLERA_LOST_CONNECTION_OPERATION_WINDOW_MODE;
				if (Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END)) == TRUE)				i=KAILLERA_LOST_CONNECTION_OPERATION_END;
				if (Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END_ALL_PLAYERS)) == TRUE)	i=KAILLERA_LOST_CONNECTION_OPERATION_END_ALL_PLAYERS;
				SetKailleraLostConnectionOperation(i);
			}


			hCtrl = GetDlgItem(hDlg, IDC_KAILLERA_CLIENT_DLL);
			if (hCtrl)
			{
				WCHAR buf2[MAX_PATH];
				const int nmb = ComboBox_GetCurSel(hCtrl);
				if (nmb == 0) SetKailleraClientDLL(TEXT("\\"));
				else {
					ComboBox_GetLBText(hCtrl, nmb, buf2);
					SetKailleraClientDLL(buf2);
				}
			}
            
		    bKailleraMAME32WindowHide  = GetKailleraMAME32WindowHide();

			/* Fall through */

        case IDCANCEL :
            EndDialog(hDlg, 0);
            return TRUE;


		case IDC_KAILLERA_LOST_CONNECTION_TIME:
			Edit_GetText(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_TIME), buf, 100);
			if (buf[0] != 0)
			{
				unsigned int i;
				swscanf(buf,TEXT("%u"), &i);
				if (i == 0)
				{
					Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_WINDOW_MODE),		FALSE);
					Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END),				FALSE);
					Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END_ALL_PLAYERS),	FALSE);
				} else
				{
					Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_WINDOW_MODE),		TRUE);
					Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END),				TRUE);
					Button_Enable(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END_ALL_PLAYERS),	TRUE);
				}

			}
			break;

		case IDC_KAILLERA_LOST_CONNECTION_IS_MADE_WINDOW_MODE:
			if (Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_WINDOW_MODE)))
			{
				Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END),      FALSE);
				Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END_ALL_PLAYERS),      FALSE);
			}
			break;
		case IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END:
			if (Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END)))
			{
				Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_WINDOW_MODE),      FALSE);
				Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END_ALL_PLAYERS),      FALSE);
			}
			break;
		case IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END_ALL_PLAYERS:
			if (Button_GetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END_ALL_PLAYERS)))
			{
				Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_END),      FALSE);
				Button_SetCheck(GetDlgItem(hDlg, IDC_KAILLERA_LOST_CONNECTION_IS_MADE_WINDOW_MODE),      FALSE);
			}
			break;
        }
        break;
    }
    return 0;
}
#endif /* KAILLERA */

#ifdef MAME_AVI

#include <math.h>

void AviDialogProcRefresh(HWND hDlg)
{
	if (Button_GetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP)) &&
		AviStatus.depth == 16	)
	{
		Button_Enable(GetDlgItem(hDlg, IDC_FRAME_CMP_PRE), TRUE);
		Button_Enable(GetDlgItem(hDlg, IDC_FRAME_CMP_FEW), TRUE);
	}
	else
	{
		Button_Enable(GetDlgItem(hDlg, IDC_FRAME_CMP_PRE), FALSE);
		Button_Enable(GetDlgItem(hDlg, IDC_FRAME_CMP_FEW), FALSE);
	}

	if (Button_GetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24)))
	{
		Button_Enable(GetDlgItem(hDlg, IDC_INTERLACE), TRUE);
	} else
	{
		Button_Enable(GetDlgItem(hDlg, IDC_INTERLACE), FALSE);
	}

	if (Button_GetCheck(GetDlgItem(hDlg, IDC_INTERLACE)) &&
		Button_GetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24))	)
	{	
		Button_Enable(GetDlgItem(hDlg, IDC_INTERLACE_ODD),	 TRUE);
		Button_Enable(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_X), TRUE);
		Button_Enable(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_Y), TRUE);

		Button_Enable(GetDlgItem(hDlg, IDC_AVISIZE_WIDTH),		TRUE);
		Button_Enable(GetDlgItem(hDlg, IDC_AVISIZE_HEIGHT),		TRUE);
		Button_Enable(GetDlgItem(hDlg, IDC_AVI_LEFT),			TRUE);
		Button_Enable(GetDlgItem(hDlg, IDC_AVI_WIDTH),			TRUE);
		Button_Enable(GetDlgItem(hDlg, IDC_AVI_TOP),			TRUE);
		Button_Enable(GetDlgItem(hDlg, IDC_AVI_HEIGHT),			TRUE);
	
		Button_Enable(GetDlgItem(hDlg, IDC_AVI_POS_CENTER),			TRUE);
		Static_SetText(GetDlgItem(hDlg, IDC_TEXT_FPS_DIV2),        TEXT("/2"));
		Static_SetText(GetDlgItem(hDlg, IDC_TEXT_AVI_TOP_MUL2),    TEXT("x2"));
		Static_SetText(GetDlgItem(hDlg, IDC_TEXT_AVI_HEIGHT_MUL2), TEXT("x2"));
	}
	else
	{
		Button_Enable(GetDlgItem(hDlg, IDC_INTERLACE_ODD), FALSE);
		Button_Enable(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_X), FALSE);
		Button_Enable(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_Y), FALSE);

		Button_Enable(GetDlgItem(hDlg, IDC_AVISIZE_WIDTH),		FALSE);
		Button_Enable(GetDlgItem(hDlg, IDC_AVISIZE_HEIGHT),		FALSE);
		Button_Enable(GetDlgItem(hDlg, IDC_AVI_LEFT),			FALSE);
		Button_Enable(GetDlgItem(hDlg, IDC_AVI_WIDTH),			FALSE);
		Button_Enable(GetDlgItem(hDlg, IDC_AVI_TOP),			FALSE);
		Button_Enable(GetDlgItem(hDlg, IDC_AVI_HEIGHT),			FALSE);

		Button_Enable(GetDlgItem(hDlg, IDC_AVI_POS_CENTER),			FALSE);
		Static_SetText(GetDlgItem(hDlg, IDC_TEXT_FPS_DIV2),        _Unicode(""));
		Static_SetText(GetDlgItem(hDlg, IDC_TEXT_AVI_TOP_MUL2),    _Unicode(""));
		Static_SetText(GetDlgItem(hDlg, IDC_TEXT_AVI_HEIGHT_MUL2), _Unicode(""));
	}
}

INT_PTR CALLBACK AviDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{

    switch (Msg)
    {
    case WM_INITDIALOG:
		{
			WCHAR buf[64];
			HWND hCtrl = GetDlgItem(hDlg, IDC_AVIFRAMESKIP);

			TranslateDialog(hDlg, lParam, TRUE);
			AviStatus = (*(GetAviStatus()));
			if (hCtrl)
			{
				int i;
				WCHAR fmt[64];
				const WCHAR *szFmtStr = TEXT("Skip %d of 12 frames");
				ComboBox_AddString(hCtrl, _UIW(TEXT("Draw every frame  @")));

				for (i = 1; i < 12; i++)
				{
					wsprintf( fmt, _UIW(szFmtStr), i );
					if (i==6 || i==8 || i==9)
						wsprintf(buf, TEXT("%s @"), fmt);
					else
						wsprintf(buf, TEXT("%s"), fmt);

					ComboBox_AddString(hCtrl, buf);
				}
				ComboBox_SetCurSel(hCtrl, AviStatus.frame_skip);
			}

			hCtrl = GetDlgItem(hDlg, IDC_FPS);
			if (hCtrl)
			{
				swprintf(buf, TEXT("%5.6f"), AviStatus.fps);
				ComboBox_AddString(hCtrl, buf);

				ComboBox_AddString(hCtrl, TEXT("60"));
				ComboBox_AddString(hCtrl, TEXT("59.94"));
				ComboBox_AddString(hCtrl, TEXT("53.333333"));
				ComboBox_AddString(hCtrl, TEXT("48"));
				ComboBox_AddString(hCtrl, TEXT("40"));
				ComboBox_AddString(hCtrl, TEXT("30"));
				ComboBox_AddString(hCtrl, TEXT("29.97"));
				ComboBox_AddString(hCtrl, TEXT("24"));
				ComboBox_AddString(hCtrl, TEXT("20"));
				ComboBox_AddString(hCtrl, TEXT("15"));
				ComboBox_AddString(hCtrl, TEXT("12"));
				ComboBox_AddString(hCtrl, TEXT("10"));
				ComboBox_SetCurSel(hCtrl, 0);

				ComboBox_LimitText(hCtrl, 10);
				Edit_SetText(hCtrl, buf);

			}

			hCtrl = GetDlgItem(hDlg, IDC_AVISIZE_WIDTH);
			if (hCtrl)
			{
				wsprintf(buf, TEXT("%u"), AviStatus.avi_width);
				ComboBox_AddString(hCtrl, buf);
				ComboBox_AddString(hCtrl, TEXT("720"));
				ComboBox_AddString(hCtrl, TEXT("640"));
				ComboBox_AddString(hCtrl, TEXT("512"));
				ComboBox_AddString(hCtrl, TEXT("480"));
				ComboBox_AddString(hCtrl, TEXT("384"));
				ComboBox_AddString(hCtrl, TEXT("352"));
				ComboBox_AddString(hCtrl, TEXT("320"));
				ComboBox_AddString(hCtrl, TEXT("304"));
				ComboBox_AddString(hCtrl, TEXT("256"));
				ComboBox_AddString(hCtrl, TEXT("240"));
				ComboBox_SetCurSel(hCtrl, 0);

				ComboBox_LimitText(hCtrl, 5);
				wsprintf(buf, TEXT("%u"), AviStatus.avi_width);
				Edit_SetText(hCtrl, buf);
			}

			hCtrl = GetDlgItem(hDlg, IDC_AVISIZE_HEIGHT);
			if (hCtrl)
			{
				wsprintf(buf, TEXT("%u"), AviStatus.avi_height);
				ComboBox_AddString(hCtrl, buf);
				ComboBox_AddString(hCtrl, TEXT("480"));
				ComboBox_AddString(hCtrl, TEXT("384"));
				ComboBox_AddString(hCtrl, TEXT("320"));
				ComboBox_AddString(hCtrl, TEXT("240"));
				ComboBox_AddString(hCtrl, TEXT("232"));
				ComboBox_AddString(hCtrl, TEXT("224"));
				ComboBox_SetCurSel(hCtrl, 0);

				ComboBox_LimitText(hCtrl, 5);
				wsprintf(buf, TEXT("%u"), AviStatus.avi_height);
				Edit_SetText(hCtrl, buf);
			}

			wsprintf(buf, TEXT("%lu"), AviStatus.avi_rect.m_Left);
			Edit_SetText(GetDlgItem(hDlg, IDC_AVI_LEFT),   buf);
			wsprintf(buf, TEXT("%lu"), AviStatus.avi_rect.m_Top);
			Edit_SetText(GetDlgItem(hDlg, IDC_AVI_TOP),    buf);
			wsprintf(buf, TEXT("%lu"), AviStatus.avi_rect.m_Width);
			Edit_SetText(GetDlgItem(hDlg, IDC_AVI_WIDTH),  buf);
			wsprintf(buf, TEXT("%lu"), AviStatus.avi_rect.m_Height);
			Edit_SetText(GetDlgItem(hDlg, IDC_AVI_HEIGHT), buf);
			
			hCtrl = GetDlgItem(hDlg, IDC_AVI_FILESIZE);
			if (hCtrl)
			{
				wsprintf(buf, TEXT("%u"), AviStatus.avi_filesize);
				ComboBox_AddString(hCtrl, buf);
				ComboBox_AddString(hCtrl, TEXT("2000"));
				ComboBox_AddString(hCtrl, TEXT("1500"));
				ComboBox_AddString(hCtrl, TEXT("1000"));
				ComboBox_AddString(hCtrl, TEXT("500"));
				ComboBox_AddString(hCtrl, TEXT("100"));
				ComboBox_SetCurSel(hCtrl, 0);

				ComboBox_LimitText(hCtrl, 7);
				wsprintf(buf, TEXT("%u"), AviStatus.avi_filesize);
				Edit_SetText(hCtrl, buf);
			}
			hCtrl = GetDlgItem(hDlg, IDC_AVI_FILESIZE_CHECK_FRAME);
			if (hCtrl)
			{
				ComboBox_AddString(hCtrl, TEXT("60"));
				ComboBox_AddString(hCtrl, TEXT("30"));
				ComboBox_AddString(hCtrl, TEXT("20"));
				ComboBox_AddString(hCtrl, TEXT("15"));
				ComboBox_AddString(hCtrl, TEXT("12"));
				ComboBox_AddString(hCtrl, TEXT("10"));
				ComboBox_AddString(hCtrl, TEXT("6"));
				ComboBox_AddString(hCtrl, TEXT("5"));
				ComboBox_AddString(hCtrl, TEXT("4"));
				ComboBox_AddString(hCtrl, TEXT("3"));
				ComboBox_AddString(hCtrl, TEXT("2"));
				ComboBox_AddString(hCtrl, TEXT("1"));
				ComboBox_SetCurSel(hCtrl, 0);

				ComboBox_LimitText(hCtrl, 11);
				wsprintf(buf, TEXT("%u"), AviStatus.avi_filesizecheck_frame);
				Edit_SetText(hCtrl, buf);
			}

			hCtrl = GetDlgItem(hDlg, IDC_AUDIO_RECORD_TYPE);
			if (hCtrl)
			{
				ComboBox_AddString(hCtrl, _UIW(TEXT("Do not record sound")));
				ComboBox_AddString(hCtrl, _UIW(TEXT("Record as WAV file")));
				ComboBox_AddString(hCtrl, _UIW(TEXT("Record to AVI")));

				ComboBox_SetCurSel(hCtrl, AviStatus.avi_audio_record_type);
			}

			Edit_LimitText(	GetDlgItem(hDlg, IDC_HOUR),   3);
			Edit_LimitText(	GetDlgItem(hDlg, IDC_MINUTE), 3);
			Edit_LimitText(	GetDlgItem(hDlg, IDC_SECOND), 3);

			{
				wsprintf(buf, TEXT("%d x %d x %d bit"), AviStatus.width, AviStatus.height, AviStatus.depth);
				Static_SetText(GetDlgItem(hDlg, IDC_BITMAP_SIZE),       buf);

				if (AviStatus.audio_type == 0)
				{

					Static_SetText(GetDlgItem(hDlg, IDC_AUDIO_SRC_FORMAT),TEXT("No sound"));

					Static_SetText(GetDlgItem(hDlg, IDC_AUDIO_DEST_FORMAT),TEXT(""));

					Button_Enable(GetDlgItem(hDlg, IDC_AUDIO_RECORD_TYPE), FALSE);

				}else
				{
                    const WCHAR *lpszStereo = TEXT("Stereo");
					const WCHAR *lpszMono   = TEXT("Mono");				
					wsprintf(buf, TEXT("%uHz %ubit %s"), AviStatus.audio_samples_per_sec, AviStatus.audio_bitrate, (AviStatus.audio_channel == 2) ? lpszStereo:lpszMono);
					Static_SetText(GetDlgItem(hDlg, IDC_AUDIO_SRC_FORMAT),        buf);
					wsprintf(buf, TEXT("%uHz %ubit %s"), AviStatus.avi_audio_samples_per_sec, AviStatus.avi_audio_bitrate, (AviStatus.avi_audio_channel == 2) ? lpszStereo:lpszMono);
					Static_SetText(GetDlgItem(hDlg, IDC_AUDIO_DEST_FORMAT),       buf);
					Button_Enable(GetDlgItem(hDlg, IDC_AUDIO_RECORD_TYPE), TRUE);
				}


				wsprintf(buf, TEXT("%d"), AviStatus.hour);
				Edit_SetText(	GetDlgItem(hDlg, IDC_HOUR), buf);
				wsprintf(buf, TEXT("%d"), AviStatus.minute);
				Edit_SetText(	GetDlgItem(hDlg, IDC_MINUTE), buf);
				wsprintf(buf, TEXT("%d"), AviStatus.second);
				Edit_SetText(	GetDlgItem(hDlg, IDC_SECOND), buf);
			}
			

			Button_SetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP),      AviStatus.frame_cmp);
			Button_SetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP_PRE),  AviStatus.frame_cmp_pre15);
			Button_SetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP_FEW),  AviStatus.frame_cmp_few);
			Button_SetCheck(GetDlgItem(hDlg, IDC_WAVE_RECORD),    AviStatus.wave_record);
			Button_SetCheck(GetDlgItem(hDlg, IDC_INTERLACE),      AviStatus.interlace);
			Button_SetCheck(GetDlgItem(hDlg, IDC_INTERLACE_ODD),	AviStatus.interlace_odd_number_field);

			Button_SetCheck(GetDlgItem(hDlg, IDC_CHECK_FORCEFLIPY),     AviStatus.force_flip_y);

			Button_SetCheck(GetDlgItem(hDlg, IDC_AVI_SAVEFILE_PAUSE),	AviStatus.avi_savefile_pause);
			Button_SetCheck(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_X),		AviStatus.avi_smooth_resize_x);
			Button_SetCheck(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_Y),		AviStatus.avi_smooth_resize_y);
			
			Button_SetCheck(GetDlgItem(hDlg, IDC_AUDIO_16BIT),    (AviStatus.avi_audio_bitrate>8) ? TRUE:FALSE);
			Button_SetCheck(GetDlgItem(hDlg, IDC_AUDIO_STEREO),   (AviStatus.avi_audio_channel==2) ? TRUE:FALSE);

	
			if (AviStatus.depth == 16)
			{
				if (AviStatus.avi_depth == 8)	Button_SetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO8),TRUE);
				else							Button_SetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO8),FALSE);
				if (AviStatus.avi_depth == 24)	Button_SetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24),TRUE);
				else							Button_SetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24),FALSE);
			
				Button_Enable(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO8), TRUE);
				Button_Enable(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24), TRUE);

				Button_Enable(GetDlgItem(hDlg, IDC_INTERLACE), TRUE);
				Button_Enable(GetDlgItem(hDlg, IDC_SET_TV_DISPLAY_SETTING), TRUE);
				

			} else
			{
				Button_SetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO8),FALSE);
				Button_SetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24),FALSE);
				Button_Enable(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO8), FALSE);
				Button_Enable(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24), FALSE);
				Button_Enable(GetDlgItem(hDlg, IDC_INTERLACE), FALSE);
				Button_Enable(GetDlgItem(hDlg, IDC_SET_TV_DISPLAY_SETTING), FALSE);
			}
			

			if (AviStatus.depth == 16)
			{
				Button_Enable(GetDlgItem(hDlg, IDC_FRAME_CMP), TRUE);
				Button_Enable(GetDlgItem(hDlg, IDC_FRAME_CMP_PRE), TRUE);
				Button_Enable(GetDlgItem(hDlg, IDC_FRAME_CMP_FEW), TRUE);
			}else 
			{
				Button_Enable(GetDlgItem(hDlg, IDC_FRAME_CMP), FALSE);
				Button_Enable(GetDlgItem(hDlg, IDC_FRAME_CMP_PRE), FALSE);
				Button_Enable(GetDlgItem(hDlg, IDC_FRAME_CMP_FEW), FALSE);
			}

			Edit_LimitText(GetDlgItem(hDlg, IDC_LEFT), 4);
			Edit_SetText(GetDlgItem(hDlg, IDC_LEFT),	TEXT("0"));
			SendDlgItemMessage(hDlg, IDC_LEFT_SPIN, UDM_SETRANGE, 0,
										(LPARAM)MAKELONG(AviStatus.width-1, 0));
		    SendDlgItemMessage(hDlg, IDC_LEFT_SPIN, UDM_SETPOS, 0,
										(LPARAM)MAKELONG(AviStatus.rect.m_Left, 0));

			Edit_LimitText(GetDlgItem(hDlg, IDC_TOP), 4);
			Edit_SetText(GetDlgItem(hDlg, IDC_TOP),	TEXT("0"));
			SendDlgItemMessage(hDlg, IDC_TOP_SPIN, UDM_SETRANGE, 0,
										(LPARAM)MAKELONG(AviStatus.height-1, 0));
		    SendDlgItemMessage(hDlg, IDC_TOP_SPIN, UDM_SETPOS, 0,
										(LPARAM)MAKELONG(AviStatus.rect.m_Top, 0));

			Edit_LimitText(GetDlgItem(hDlg, IDC_WIDTH), 4);
			SendDlgItemMessage(hDlg, IDC_WIDTH_SPIN, UDM_SETRANGE, 0,
										(LPARAM)MAKELONG(AviStatus.width, 1));
		    SendDlgItemMessage(hDlg, IDC_WIDTH_SPIN, UDM_SETPOS, 0,
										(LPARAM)MAKELONG(AviStatus.rect.m_Width, 0));

			Edit_LimitText(GetDlgItem(hDlg, IDC_HEIGHT), 4);
			SendDlgItemMessage(hDlg, IDC_HEIGHT_SPIN, UDM_SETRANGE, 0,
										(LPARAM)MAKELONG(AviStatus.height, 1));
		    SendDlgItemMessage(hDlg, IDC_HEIGHT_SPIN, UDM_SETPOS, 0,
										(LPARAM)MAKELONG(AviStatus.rect.m_Height, 0));


		}

		AviDialogProcRefresh(hDlg);
        return TRUE;

    case WM_HELP:
        break;

    case WM_CONTEXTMENU:
        break;

    case WM_COMMAND :
        switch (GET_WM_COMMAND_ID(wParam, lParam))
        {
		case IDC_FRAME_CMP:
			AviDialogProcRefresh(hDlg);
			break;
		case IDC_FRAME_CMP_PRE:
			if (Button_GetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP_PRE)))
				Button_SetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP_FEW),      FALSE);
			break;
		case IDC_FRAME_CMP_FEW:
			if (Button_GetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP_FEW)))
				Button_SetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP_PRE),      FALSE);
			break;
		case IDC_INTERLACE:
			if (Button_GetCheck(GetDlgItem(hDlg, IDC_INTERLACE)) == TRUE)
			{	
				char buf[32];
				sprintf(buf, "%lu", AviStatus.rect.m_Height*2);
				Edit_SetTextA(GetDlgItem(hDlg, IDC_AVISIZE_HEIGHT),        buf);
			}
			else
			{
				char buf[32];
				sprintf(buf, "%lu", AviStatus.rect.m_Height);
				Edit_SetTextA(GetDlgItem(hDlg, IDC_AVISIZE_HEIGHT),        buf);
			}
			AviDialogProcRefresh(hDlg);
			break;
		case IDC_COLOR_CNV_16TO8:
			if (Button_GetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO8)))
				Button_SetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24),      FALSE);
			AviDialogProcRefresh(hDlg);
			break;
		case IDC_COLOR_CNV_16TO24:
			if (Button_GetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24)))
			{
				Button_SetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO8),      FALSE);
			}
			AviDialogProcRefresh(hDlg);
			break;
		case IDC_SET_TV_DISPLAY_SETTING:
			{
				char buf[100];
				unsigned int x,y,width,height;
				unsigned int width_src,height_src;

				Button_SetCheck(GetDlgItem(hDlg, IDC_INTERLACE),			TRUE);
				Button_SetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24),		TRUE);
				Button_SetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP),			FALSE);
				Button_SetCheck(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_Y),		TRUE);

				Edit_SetTextA(GetDlgItem(hDlg, IDC_FPS),					"59.94");
				Edit_SetTextA(GetDlgItem(hDlg, IDC_AVISIZE_WIDTH),		"720");
				Edit_SetTextA(GetDlgItem(hDlg, IDC_AVISIZE_HEIGHT),		"480");


				Edit_GetText(GetDlgItem(hDlg, IDC_WIDTH), (LPTSTR)buf, 100);
				sscanf(_String((LPTSTR)buf),"%u", &width);	if (width == 0)		width = AviStatus.rect.m_Width;
				Edit_GetText(GetDlgItem(hDlg, IDC_HEIGHT), (LPTSTR)buf, 100);
				sscanf(_String((LPTSTR)buf),"%u", &height);	if (height == 0)	height = AviStatus.rect.m_Height;

				width_src = width; 
				height_src = height;

				width *= 2;
				if (height > 240) {height = 240; width = 480*0.75;}
				y = (240 - height)/2;
				x = (720 - width)/2;
				if (width > 670) {width = 670; x = 22;}

				sprintf(buf, "%u", x);
				Edit_SetTextA(GetDlgItem(hDlg, IDC_AVI_LEFT),        buf);
				sprintf(buf, "%u", y);
				Edit_SetTextA(GetDlgItem(hDlg, IDC_AVI_TOP),        buf);
				sprintf(buf, "%u", width);
				Edit_SetTextA(GetDlgItem(hDlg, IDC_AVI_WIDTH),        buf);	
				sprintf(buf, "%u", height);
				Edit_SetTextA(GetDlgItem(hDlg, IDC_AVI_HEIGHT),        buf);
				
				if (width % width_src)		AviStatus.avi_smooth_resize_x = TRUE;
				else						AviStatus.avi_smooth_resize_x = FALSE;
				if (height % height_src)	AviStatus.avi_smooth_resize_y = TRUE;
				else						AviStatus.avi_smooth_resize_y = FALSE;

				Button_SetCheck(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_X),		AviStatus.avi_smooth_resize_x);
				Button_SetCheck(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_Y),		AviStatus.avi_smooth_resize_y);
				
				ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_AVIFRAMESKIP), 0);

			}
			AviDialogProcRefresh(hDlg);
			break;
		case IDC_AVI_POS_CENTER:
			{
				char buf[100];
				int x,y,width,height;
				Edit_GetText(GetDlgItem(hDlg, IDC_AVI_WIDTH), (LPTSTR)buf, 100);
				sscanf(_String((LPTSTR)buf),"%u", &width);
				Edit_GetText(GetDlgItem(hDlg, IDC_AVI_HEIGHT), (LPTSTR)buf, 100);
				sscanf(_String((LPTSTR)buf),"%u", &height);

				y = (240 - height)/2;
				x = (720 - width)/2;

				if (x<0) x=0;
				if (y<0) y=0;

				sprintf(buf, "%u", x);
				Edit_SetTextA(GetDlgItem(hDlg, IDC_AVI_LEFT),        buf);
				sprintf(buf, "%u", y);
				Edit_SetTextA(GetDlgItem(hDlg, IDC_AVI_TOP),        buf);
			}
			break;
			
        case IDOK :
			{
				char buf[100];
				char buf2[256];

				Edit_GetText(GetDlgItem(hDlg, IDC_FPS), (LPTSTR)buf2, 100);
				strcpy(buf, _String((LPTSTR)buf2));

				{
					int i,j;
					int di,df;
					int di2,df2,j2;

					AviStatus.fps = 0;
					j=-1;
					for(i=0; i<100; i++)
					{
						if (buf[i] == 0 ) break;
						if (buf[i] == '.')
						{
							buf[i] = 0;
							j=i+1;
							break;
						}
					}
					sscanf(buf,"%d", &di);
					df = 0;
					if (j!=-1) sscanf(&buf[j],"%d", &df);
					j=strlen(&buf[j]);
					AviStatus.fps = (double)df / pow(10,j);
					AviStatus.fps += (double)di;

									
					sprintf(buf, "%5.6f", AviStatus.def_fps);
					j2=-1;
					for(i=0; i<100; i++)
					{
						if (buf[i] == 0 ) break;
						if (buf[i] == '.')
						{
							buf[i] = 0;
							j2=i+1;
							break;
						}
					}
					sscanf(buf,"%d", &di2);
					df2 = 0;
					if (j2!=-1) sscanf(&buf[j2],"%d", &df2);
					j2=strlen(&buf[j2]);
					if ( AviStatus.fps == (double)df2 / pow(10,j2) + (double)di2 ) AviStatus.fps = AviStatus.def_fps;

				}

				if (AviStatus.fps <= 0) AviStatus.fps = AviStatus.def_fps;

				AviStatus.frame_skip = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_AVIFRAMESKIP));

				AviStatus.frame_cmp       = Button_GetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP));
				AviStatus.frame_cmp_pre15 = Button_GetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP_PRE));
				AviStatus.frame_cmp_few   = Button_GetCheck(GetDlgItem(hDlg, IDC_FRAME_CMP_FEW));
				AviStatus.wave_record     = Button_GetCheck(GetDlgItem(hDlg, IDC_WAVE_RECORD));
				AviStatus.interlace       = Button_GetCheck(GetDlgItem(hDlg, IDC_INTERLACE));
				AviStatus.interlace_odd_number_field = Button_GetCheck(GetDlgItem(hDlg, IDC_INTERLACE_ODD));

				AviStatus.force_flip_y    = Button_GetCheck(GetDlgItem(hDlg, IDC_CHECK_FORCEFLIPY));
				
				AviStatus.avi_savefile_pause  = Button_GetCheck(GetDlgItem(hDlg, IDC_AVI_SAVEFILE_PAUSE));
				AviStatus.avi_smooth_resize_x = Button_GetCheck(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_X));
				AviStatus.avi_smooth_resize_y = Button_GetCheck(GetDlgItem(hDlg, IDC_SMOOTH_RESIZE_Y));

				AviStatus.avi_audio_record_type = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_AUDIO_RECORD_TYPE));
				AviStatus.avi_audio_bitrate = (Button_GetCheck(GetDlgItem(hDlg, IDC_AUDIO_16BIT))==TRUE) ? 16:8;
				AviStatus.avi_audio_channel = (Button_GetCheck(GetDlgItem(hDlg, IDC_AUDIO_STEREO))==TRUE) ? 2:1;

				AviStatus.bmp_16to8_cnv = Button_GetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO8));
				AviStatus.avi_depth = AviStatus.depth;
				if (AviStatus.depth == 16)
				{
					if (Button_GetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO8)) == TRUE) AviStatus.avi_depth = 8;
					if (Button_GetCheck(GetDlgItem(hDlg, IDC_COLOR_CNV_16TO24)) == TRUE) AviStatus.avi_depth = 24;
				}


				Edit_GetText(GetDlgItem(hDlg, IDC_AVI_FILESIZE), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%d", &AviStatus.avi_filesize);
				Edit_GetText(GetDlgItem(hDlg, IDC_AVI_FILESIZE_CHECK_FRAME), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%d", &AviStatus.avi_filesizecheck_frame);
				Edit_GetText(GetDlgItem(hDlg, IDC_AVISIZE_WIDTH), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%d", &AviStatus.avi_width);
				Edit_GetText(GetDlgItem(hDlg, IDC_AVISIZE_HEIGHT), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%d", &AviStatus.avi_height);

				Edit_GetText(GetDlgItem(hDlg, IDC_AVI_LEFT), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%lu", &AviStatus.avi_rect.m_Left);
				Edit_GetText(GetDlgItem(hDlg, IDC_AVI_TOP), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%lu", &AviStatus.avi_rect.m_Top);
				Edit_GetText(GetDlgItem(hDlg, IDC_AVI_WIDTH), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%lu", &AviStatus.avi_rect.m_Width);
				Edit_GetText(GetDlgItem(hDlg, IDC_AVI_HEIGHT), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%lu", &AviStatus.avi_rect.m_Height);

				Edit_GetText(GetDlgItem(hDlg, IDC_LEFT), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%lu", &AviStatus.rect.m_Left);
				Edit_GetText(GetDlgItem(hDlg, IDC_TOP), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%lu", &AviStatus.rect.m_Top);
				Edit_GetText(GetDlgItem(hDlg, IDC_WIDTH), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%lu", &AviStatus.rect.m_Width);
				Edit_GetText(GetDlgItem(hDlg, IDC_HEIGHT), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%lu", &AviStatus.rect.m_Height);
				
				Edit_GetText(GetDlgItem(hDlg, IDC_HOUR), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%d", &AviStatus.hour);
				Edit_GetText(GetDlgItem(hDlg, IDC_MINUTE), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%d", &AviStatus.minute);
				Edit_GetText(GetDlgItem(hDlg, IDC_SECOND), (LPTSTR)buf, 100);
				if (buf[0] != 0)	sscanf(_String((LPTSTR)buf),"%d", &AviStatus.second);

				if (AviStatus.rect.m_Width	> AviStatus.width)	AviStatus.rect.m_Width	= AviStatus.width;
				if (AviStatus.rect.m_Height	> AviStatus.height)	AviStatus.rect.m_Height	= AviStatus.height;
				if (AviStatus.rect.m_Left+AviStatus.rect.m_Width	> AviStatus.width)	AviStatus.rect.m_Left	= AviStatus.width - AviStatus.rect.m_Width;
				if (AviStatus.rect.m_Top+AviStatus.rect.m_Height	> AviStatus.height) AviStatus.rect.m_Top	= AviStatus.height - AviStatus.rect.m_Height;

				SetAviStatus(&AviStatus);
			}
            /* Fall through */

			EndDialog(hDlg, 1);
			return TRUE;

        case IDCANCEL :
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }
    return 0;
}


static void set_mame_mixer_wfm(int drvindex)
{
	extern int mame_mixer_wave_cnvnmb;
	extern struct WAV_WAVEFORMAT		mame_mixer_dstwfm, mame_mixer_srcwfm;
	extern struct WAV_SAMPLES_RESIZE	mame_mixer_wsr;
	
	machine_config drv;
    expand_machine_driver(drivers[drvindex]->drv,&drv);

	mame_mixer_srcwfm.samplespersec = playing_game_options.samplerate;
	mame_mixer_srcwfm.channel = 2;		// changed from 0.93 (Aaron's big sound system change) - DarkCoder
	mame_mixer_srcwfm.bitrate = 16;

	mame_mixer_dstwfm = mame_mixer_srcwfm;

	mame_mixer_wave_cnvnmb = wav_convert_select(&mame_mixer_dstwfm, &mame_mixer_srcwfm, 
												&mame_mixer_wsr, NULL ); //&mame_mixer_wsre );
}

static void SetupAviStatus(int nGame)
{
#ifndef HAZEMD
	extern int neogeo_check_lower_resolution( const char *name );
#endif /* HAZEMD */
	struct MAME_AVI_STATUS *OldAviStatus;	//kt
	machine_config drv;
    expand_machine_driver(drivers[nGame]->drv, &drv);

	AviStatus.source_file = (char*)drivers[nGame]->source_file;
	AviStatus.index = nGame + 1;

	AviStatus.def_fps = SUBSECONDS_TO_HZ(drv.screen[0].defstate.refresh); // fps
	AviStatus.fps     = AviStatus.def_fps;
	AviStatus.depth   = 16; //playing_game_options.depth;	// (auto/16bit/32bit)
	AviStatus.flags   = drivers[nGame]->flags;
    AviStatus.orientation = AviStatus.flags & ORIENTATION_MASK;

	if (playing_game_options.ror == TRUE)
	{
		if ((AviStatus.orientation & ROT180) == ORIENTATION_FLIP_X ||
			(AviStatus.orientation & ROT180) == ORIENTATION_FLIP_Y) 
		{
			AviStatus.orientation ^= ROT180;
		}
		AviStatus.orientation ^= ROT90;
	}
	else if(playing_game_options.rol == TRUE)
	{
		if ((AviStatus.orientation & ROT180) == ORIENTATION_FLIP_X ||
			(AviStatus.orientation & ROT180) == ORIENTATION_FLIP_Y) 
		{
			AviStatus.orientation ^= ROT180;
		}
		AviStatus.orientation ^= ROT270;
	}

	if (playing_game_options.flipx)
		AviStatus.orientation ^= ORIENTATION_FLIP_X;
	if (playing_game_options.flipy)
		AviStatus.orientation ^= ORIENTATION_FLIP_Y;

	AviStatus.frame_skip		= 0;
	AviStatus.frame_cmp			= TRUE;
	AviStatus.frame_cmp_pre15	= FALSE;
	AviStatus.frame_cmp_few		= FALSE;
	AviStatus.wave_record		= FALSE;
	AviStatus.bmp_16to8_cnv		= FALSE;

	AviStatus.force_flip_y		= FALSE;

	AviStatus.avi_filesize				= 1800;
	AviStatus.avi_filesizecheck_frame	= 10;
	AviStatus.avi_savefile_pause		= FALSE;


	AviStatus.interlace	= FALSE;
	AviStatus.interlace_odd_number_field = FALSE;

	{
		extern struct WAV_WAVEFORMAT mame_mixer_dstwfm, mame_mixer_srcwfm;
		set_mame_mixer_wfm(nGame);

		AviStatus.audio_channel         = mame_mixer_srcwfm.channel;
		AviStatus.audio_samples_per_sec	= mame_mixer_srcwfm.samplespersec;
		AviStatus.audio_bitrate			= mame_mixer_srcwfm.bitrate;
		
		AviStatus.avi_audio_channel			= mame_mixer_dstwfm.channel;
		AviStatus.avi_audio_samples_per_sec = mame_mixer_dstwfm.samplespersec;
		AviStatus.avi_audio_bitrate			= mame_mixer_dstwfm.bitrate;

		AviStatus.audio_type			= playing_game_options.sound;
		AviStatus.avi_audio_record_type	= (AviStatus.audio_type!=0) ? 2:0;
	}

	AviStatus.hour   = 0;
	AviStatus.minute = 0;
	AviStatus.second = 0;

	if (AviStatus.orientation & ORIENTATION_SWAP_XY)
	{
		AviStatus.width  = drv.screen[0].defstate.height;
		AviStatus.height = drv.screen[0].defstate.width;
		AviStatus.rect.m_Left   = drv.screen[0].defstate.visarea.min_y;
		AviStatus.rect.m_Top    = drv.screen[0].defstate.visarea.min_x;
		AviStatus.rect.m_Width  = drv.screen[0].defstate.visarea.max_y - drv.screen[0].defstate.visarea.min_y + 1;
		AviStatus.rect.m_Height = drv.screen[0].defstate.visarea.max_x - drv.screen[0].defstate.visarea.min_x + 1;
	}
	else
	{
		AviStatus.width  = drv.screen[0].defstate.width;
		AviStatus.height = drv.screen[0].defstate.height;
		AviStatus.rect.m_Left   = drv.screen[0].defstate.visarea.min_x;
		AviStatus.rect.m_Top    = drv.screen[0].defstate.visarea.min_y;
		AviStatus.rect.m_Width  = drv.screen[0].defstate.visarea.max_x - drv.screen[0].defstate.visarea.min_x + 1;
		AviStatus.rect.m_Height = drv.screen[0].defstate.visarea.max_y - drv.screen[0].defstate.visarea.min_y + 1;
	}
	
#if 0
#ifndef HAZEMD
	//neogeo
	if (!strcmp(drivers[nGame]->source_file+17, "neogeo.c") && neogeo_check_lower_resolution(drivers[nGame]->name))
	{
		AviStatus.rect.m_Left   = 1*8;
		AviStatus.rect.m_Top    = drv.screen[0].defstate.visarea.min_y;
		AviStatus.rect.m_Width  = 39*8-1 - 1*8 + 1;
		AviStatus.rect.m_Height = drv.screen[0].defstate.visarea.max_y - drv.screen[0].defstate.visarea.min_y + 1;
	}
#endif /* HAZEMD */
#endif

	AviStatus.avi_width			= AviStatus.rect.m_Width;
	AviStatus.avi_height		= AviStatus.rect.m_Height;
	AviStatus.avi_depth			= 16;
	AviStatus.avi_rect.m_Left	= 0;
	AviStatus.avi_rect.m_Top	= 0;
	AviStatus.avi_rect.m_Width	= AviStatus.rect.m_Width;
	AviStatus.avi_rect.m_Height	= AviStatus.rect.m_Height;
	AviStatus.avi_smooth_resize_x	= FALSE;
	AviStatus.avi_smooth_resize_y	= FALSE;

	if (AviStatus.avi_rect.m_Width < AviStatus.rect.m_Width ||
		(int)((double)(AviStatus.avi_rect.m_Width<<16) / (double)AviStatus.rect.m_Width) & 0xffff)	
		AviStatus.avi_smooth_resize_x	= TRUE;

	if (AviStatus.avi_rect.m_Height < AviStatus.rect.m_Height ||
		(int)((double)(AviStatus.avi_rect.m_Height<<16) / (double)AviStatus.rect.m_Height) & 0xffff)
		AviStatus.avi_smooth_resize_y	= TRUE;		


	OldAviStatus = GetAviStatus();

	if (OldAviStatus->source_file)
	if (!strcmp(AviStatus.source_file, OldAviStatus->source_file))
	{
		if (AviStatus.def_fps == OldAviStatus->def_fps)
			AviStatus.fps = OldAviStatus->fps;

		AviStatus.frame_skip = OldAviStatus->frame_skip;
		AviStatus.frame_cmp = OldAviStatus->frame_cmp;
		AviStatus.frame_cmp_pre15 = OldAviStatus->frame_cmp_pre15;
		AviStatus.frame_cmp_few = OldAviStatus->frame_cmp_few;
		AviStatus.wave_record = OldAviStatus->wave_record;
		AviStatus.bmp_16to8_cnv = OldAviStatus->bmp_16to8_cnv;

		AviStatus.avi_depth = OldAviStatus->avi_depth;
		AviStatus.interlace = OldAviStatus->interlace;
		AviStatus.interlace_odd_number_field = OldAviStatus->interlace_odd_number_field;
		AviStatus.avi_filesize = OldAviStatus->avi_filesize;
		AviStatus.avi_filesizecheck_frame = OldAviStatus->avi_filesizecheck_frame;
		AviStatus.avi_savefile_pause = OldAviStatus->avi_savefile_pause;


		if (AviStatus.audio_type == OldAviStatus->audio_type)
			AviStatus.avi_audio_record_type	= OldAviStatus->avi_audio_record_type;
		
		AviStatus.hour = OldAviStatus->hour;
		AviStatus.minute = OldAviStatus->minute;
		AviStatus.second = OldAviStatus->second;

		if ((AviStatus.flags & ORIENTATION_SWAP_XY) == (OldAviStatus->flags & ORIENTATION_SWAP_XY) &&
				AviStatus.orientation == OldAviStatus->orientation)
		{
			AviStatus.rect = OldAviStatus->rect;
			AviStatus.avi_width = OldAviStatus->avi_width;
			AviStatus.avi_height = OldAviStatus->avi_height;
			AviStatus.avi_rect = OldAviStatus->avi_rect;
			AviStatus.avi_smooth_resize_x = OldAviStatus->avi_smooth_resize_x;
			AviStatus.avi_smooth_resize_y = OldAviStatus->avi_smooth_resize_y;
		}
	}

	SetAviStatus(&AviStatus);
}

void get_autofilename(int nGame, WCHAR *avidir, WCHAR *avifilename, WCHAR *ext)
{
	WCHAR sztmpfile[MAX_PATH];

	wsprintf( sztmpfile, TEXT("%s\\%s.%s"), avidir, _Unicode(drivers[nGame]->name), ext );
	if( _waccess(sztmpfile, 0) != -1 ) {
		do
		{
			wsprintf(sztmpfile, TEXT("%s\\%.4s%04d.%s"), avidir, _Unicode(drivers[nGame]->name), _nAviNo++, ext);
		} while (_waccess(sztmpfile, 0) != -1);
	}

	wcscpy( avifilename, sztmpfile );
}

static void MamePlayGameAVI(void)
{
	int nGame, hr;
	WCHAR filename_avi[MAX_PATH];
	WCHAR filename_wav[MAX_PATH];

	nGame = Picker_GetSelectedItem(hwndList);

	g_pPlayBkName = NULL;
	g_pRecordName = NULL;
#ifdef MAME_AVI
	g_pRecordAVIName = NULL;
#endif /* MAME_AVI */
#ifdef KAILLERA
	g_pPlayBkSubName = NULL;
	g_pRecordSubName = NULL;
#endif

	memcpy(&playing_game_options, GetGameOptions(nGame), sizeof(options_type));

	SetupAviStatus(nGame);
	
	hr = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_AVI_STATUS),
				   hMain, AviDialogProc);

	if (hr != 1) return;

	AviStatus = (*(GetAviStatus()));

	get_autofilename(nGame, last_directory_avi, filename_avi, TEXT("avi") );

	wcscpy(filename_wav, TEXT(""));
	if (AviStatus.avi_audio_record_type)
	{
		extern struct WAV_WAVEFORMAT mame_mixer_dstwfm;

		AviStatus.wav_filename = filename_wav;

		if (AviStatus.avi_audio_record_type == 1)
		{
			WCHAR drive[_MAX_DRIVE];
			WCHAR dir  [_MAX_DIR];
			WCHAR fname[_MAX_FNAME];
			WCHAR ext  [_MAX_EXT];

			_wsplitpath(filename_avi, drive, dir, fname, ext);
			_wmakepath(filename_wav, drive, dir, fname, TEXT("wav"));

			if (wav_start_log_wave(_String(filename_wav), &mame_mixer_dstwfm) == 0)
			{
				wav_stop_log_wave();
			} 
			else
			{
				MameMessageBoxW(_UIW(TEXT("Could not open '%s' as a valid wave file.")), filename_wav);
				hr = 0;
				AviStatus.avi_audio_record_type = 0;
			}
		}
	}

	if ( hr == 1 )
	{
		int width, height, depth;
		tRect rect;
		int avi_depth;
		int fcmp;

		extern int			nAviFrameSkip;
		extern unsigned int	nAviFrameCount;
		extern unsigned int	nAviFrameCountStart;
		extern int			nAviAudioRecord;

		width  = AviStatus.width;
		height = AviStatus.height;
		depth  = AviStatus.depth;

		rect = AviStatus.rect;

		avi_depth = depth;
		if (AviStatus.bmp_16to8_cnv == TRUE) avi_depth = 8;

		fcmp=0;
		if (AviStatus.frame_cmp == TRUE)
		{
			fcmp=3;
			if (AviStatus.frame_cmp_pre15 == TRUE)	fcmp=1;
			if (AviStatus.frame_cmp_few == TRUE)	fcmp=2;
		}
		if (AviStatus.fps == AviStatus.def_fps)		fcmp=0;

		nAviFrameSkip = AviStatus.frame_skip;

		nAviFrameCount = 0;
		nAviFrameCountStart = (unsigned int)(((AviStatus.hour*60 + AviStatus.minute)*60 + AviStatus.second) * AviStatus.def_fps);

		nAviAudioRecord = AviStatus.avi_audio_record_type;

		//if (AviStartCapture(hMain, filename_avi, &AviStatus))
		{
			WCHAR buf[1024];
			wsprintf(buf, _UIW(TEXT("Use 'Record AVI' key to toggle start/stop AVI recording.")));
			MessageBox(hMain, buf, TEXT_MAME32NAME, MB_OK | MB_ICONEXCLAMATION );

			g_pRecordAVIName = filename_avi;
			MamePlayGameWithOptions(nGame);
			g_pRecordAVIName = NULL;
			//AviEndCapture();
		}
		//else 
		{
			
			//if( _nAviNo ) _nAviNo--;
		}
	}
}

static void MamePlayBackGameAVI()
{
	int nGame;
	WCHAR filename[MAX_PATH];
	WCHAR filename_avi[MAX_PATH];
	WCHAR filename_wav[MAX_PATH];
	int	hr;

	*filename = 0;
	g_pRecordAVIName = NULL;

	nGame = Picker_GetSelectedItem(hwndList);
	if (nGame != -1)
		wcscpy(filename, _Unicode(drivers[nGame]->name));

	if (CommonFileDialog(FALSE, filename, FILETYPE_INPUT_FILES))
	{
		file_error filerr;
		mame_file* pPlayBack;
#ifdef KAILLERA
		mame_file* pPlayBackSub;
#endif /* KAILEERA */
		WCHAR drive[_MAX_DRIVE];
		WCHAR dir[_MAX_DIR];
		WCHAR bare_fname[_MAX_FNAME];
		WCHAR ext[_MAX_EXT];

		WCHAR path[MAX_PATH];
		WCHAR fname[MAX_PATH];
#ifdef KAILLERA
		WCHAR fname2[MAX_PATH];
#endif /* KAILEERA */
		char *stemp;

		_wsplitpath(filename, drive, dir, bare_fname, ext);

		wcscpy(path, drive);
		wcscat(path, dir);
		wcscpy(fname, bare_fname);
		wcscat(fname, TEXT(".inp"));
		if (path[wcslen(path)-1] == TEXT(PATH_SEPARATOR[0]))
			path[wcslen(path)-1] = 0; // take off trailing back slash

		set_core_input_directory(path);
		stemp = utf8_from_wstring(fname);
		filerr = mame_fopen(SEARCHPATH_INPUTLOG, stemp, OPEN_FLAG_READ, &pPlayBack);
		free(stemp);
		set_core_input_directory(GetInpDir());

		if (pPlayBack == NULL)
		{
			MameMessageBoxW(_UIW(TEXT("Could not open '%s' as a valid input file.")), filename);
			return;
		}

		// check for game name embedded in .inp header
		if (pPlayBack)
		{
			inp_header inp_header;

#ifdef USE_WOLFMAME_XINP
			{
				inpext_header xheader;
				// read first four bytes to check INP type
				mame_fread(pPlayBack, xheader.header, 7);
				mame_fseek(pPlayBack, 0, SEEK_SET);
				if(strncmp(xheader.header,"XINP\0\0\0",7) != 0)
				{
					// read playback header
					mame_fread(pPlayBack, &inp_header, sizeof(inp_header));
				} else {
					// read header
					mame_fread(pPlayBack, &xheader, sizeof(inpext_header));

					memcpy(inp_header.name, xheader.shortname, sizeof(inp_header.name));
				}
			}
#else
			// read playback header
			mame_fread(pPlayBack, &inp_header, sizeof(inp_header));
#endif /* USE_WOLFMAME_XINP */

			if (!isalnum(inp_header.name[0])) // If first byte is not alpha-numeric
				mame_fseek(pPlayBack, 0, SEEK_SET); // old .inp file - no header
			else
			{
				int i;
				for (i = 0; drivers[i] != 0; i++) // find game and play it
				{
					if (strcmp(drivers[i]->name, inp_header.name) == 0)
					{
						nGame = i;
						break;
					}
				}
			}
		}
		mame_fclose(pPlayBack);

#ifdef KAILLERA
		wsprintf(fname2, TEXT("%s.trc"), bare_fname);
		g_pPlayBkSubName = NULL;
		if ((!wcscmp(ext, TEXT(".zip"))) || (!wcscmp(ext, TEXT(".trc"))))
		{
			//pPlayBackSub = mame_fopen(fname,NULL,FILETYPE_INPUTSUBLOG,0);
			set_core_input_directory(path);
			stemp = utf8_from_wstring(fname2);
			filerr = mame_fopen(SEARCHPATH_INPUTLOG, stemp, OPEN_FLAG_READ, &pPlayBackSub);
			free(stemp);
			set_core_input_directory(GetInpDir());
			if (pPlayBackSub != NULL)
			{
				inpsub_header inpsub_header;

				// read playbacksub header
				mame_fread(pPlayBackSub, &inpsub_header, sizeof(inpsub_header));
				mame_fclose(pPlayBackSub);
				//wsprintf(Trace_filename, TEXT("%s\\%s"), path, fname2);

				if (MamePlayBackTrace(fname2, &inpsub_header) == 2)
				{
					g_pPlayBkSubName = fname2;
					CopyTrctempStateSaveFile(fname2, &inpsub_header);
				}
				else
				{
					g_pPlayBkSubName = NULL;
					g_pPlayBkName = NULL;
					override_playback_directory = NULL;
					return;
				}
			}
		}
#endif

		memcpy(&playing_game_options, GetGameOptions(nGame), sizeof(options_type));

		SetupAviStatus(nGame);
		
		hr = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_AVI_STATUS),
					   hMain, AviDialogProc);

		if (hr == 1)
		{
			AviStatus = (*(GetAviStatus()));

			if (1)
			{
				//char fname[_MAX_FNAME];

				_wsplitpath(filename, NULL, NULL, bare_fname, NULL);
				wcscpy(filename_avi, bare_fname);
			} else
				wcscpy(filename_avi, _Unicode(drivers[nGame]->name));

			if (!CommonFileDialog(TRUE, filename_avi, FILETYPE_AVI_FILES))
				hr = 0;
		}

		if (hr == 1) 
		{
			wcscpy(filename_wav, TEXT(""));
			if (AviStatus.avi_audio_record_type)
			{
				extern struct WAV_WAVEFORMAT mame_mixer_dstwfm;

				AviStatus.wav_filename = filename_wav;

				if (AviStatus.avi_audio_record_type == 1)
				{
					WCHAR drive[_MAX_DRIVE];
					WCHAR dir[_MAX_DIR];
					WCHAR fname[_MAX_FNAME];
					WCHAR ext[_MAX_EXT];

					_wsplitpath(filename_avi, drive, dir, fname, ext);
					_wmakepath(filename_wav, drive, dir, fname, TEXT("wav"));

					if (wav_start_log_wave(_String(filename_wav), &mame_mixer_dstwfm) == 0)
					{
						wav_stop_log_wave();
					} 
					else
					{
						MameMessageBoxW(_UIW(TEXT("Could not open '%s' as a valid wave file.")), filename_wav);
						hr = 0;
						AviStatus.avi_audio_record_type = 0;
					}
				}
			}
		}

		if ( hr == 1 )
		{
			int width, height, depth;
			tRect rect;
			int avi_depth;
			int fcmp;
			extern int	nAviFrameSkip;
			extern unsigned int				nAviFrameCount;
			extern unsigned int				nAviFrameCountStart;
			extern int						nAviAudioRecord;


			width  = AviStatus.width;
			height = AviStatus.height;
			depth  = AviStatus.depth;

			rect = AviStatus.rect;

			avi_depth = depth;
			if (AviStatus.bmp_16to8_cnv == TRUE) avi_depth = 8;

			fcmp=0;
			if (AviStatus.frame_cmp == TRUE)
			{
				fcmp=3;
				if (AviStatus.frame_cmp_pre15 == TRUE)	fcmp=1;
				if (AviStatus.frame_cmp_few == TRUE)	fcmp=2;
			}
			if (AviStatus.fps == AviStatus.def_fps)		fcmp=0;

			nAviFrameSkip = AviStatus.frame_skip;

			nAviFrameCount = 0;
			nAviFrameCountStart = (unsigned int)(((AviStatus.hour*60 + AviStatus.minute)*60 + AviStatus.second) * AviStatus.def_fps);

			nAviAudioRecord = AviStatus.avi_audio_record_type;

			//if (AviStartCapture(hMain, filename_avi, &AviStatus))
			{
				
				WCHAR buf[1024];
				wsprintf(buf, _UIW(TEXT("Use 'Record AVI' key to toggle start/stop AVI recording.")));
				MessageBox(hMain, buf, TEXT_MAME32NAME, MB_OK | MB_ICONEXCLAMATION );

				g_pPlayBkName = fname;
				g_pRecordAVIName = filename_avi;
				override_playback_directory = path;
				MamePlayGameWithOptions(nGame);
				//AviEndCapture();
			}
		}

#ifdef KAILLERA
		if (g_pPlayBkSubName != NULL)
			DeleteTrctempStateSaveFile(g_pPlayBkSubName);
		g_pPlayBkSubName = NULL;
#endif /* KAILLERA */
		g_pPlayBkName = NULL;
		override_playback_directory = NULL;
		g_pRecordAVIName = NULL;
	}
}
#endif /* MAME_AVI */

#ifdef KSERVER
INT_PTR CALLBACK KServerOptionDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    HWND hCtrl;

    switch (Msg)
    {
    case WM_INITDIALOG:
	TranslateDialog(hDlg, lParam, TRUE);

	CenterWindow(hDlg);
	hCtrl = GetDlgItem(hDlg, IDC_SERVER_NAME);
	if (hCtrl)
	{
	Edit_LimitText(hCtrl, 64);
	Edit_SetText(hCtrl, GetServerName());
	}

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_PAGE);
	if (hCtrl)
	{
	Edit_LimitText(hCtrl, 128);
	Edit_SetText(hCtrl, GetServerPage());
	}

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_PORT);
	if (hCtrl)
	{
	WCHAR buf[200];
	wsprintf(buf, TEXT("%u"), GetServerPort());
	Edit_LimitText(hCtrl, 8);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_LOCATION);
	if (hCtrl)
	{
	Edit_LimitText(hCtrl, 64);
	Edit_SetText(hCtrl, GetServerLocation());
	}

	hCtrl = GetDlgItem(hDlg, IDC_MAX_USER);
	if (hCtrl)
	{
	WCHAR buf[200];
	wsprintf(buf, TEXT("%d"), GetMaxUser());
	Edit_LimitText(hCtrl, 4);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_IP);
	if (hCtrl)
	{
	Edit_LimitText(hCtrl, 15);
	Edit_SetText(hCtrl, GetServerIP());
	}

	hCtrl = GetDlgItem(hDlg, IDC_MIN_PING);
	if (hCtrl)
	{
	WCHAR buf[200];
	wsprintf(buf, TEXT("%d"), GetMinPing());
	Edit_LimitText(hCtrl, 4);
	Edit_SetText(hCtrl, buf);
	}

	Button_SetCheck(GetDlgItem(hDlg, IDC_SERVER_LAN),GetLan());
	Button_SetCheck(GetDlgItem(hDlg, IDC_SERVER_INTERNET),GetInternet());

	hCtrl = GetDlgItem(hDlg, IDC_LIMIT_MESSAGE);
	if (hCtrl)
	{
	WCHAR buf[200];
	wsprintf(buf, TEXT("%d"), GetLimitMsg());
	Edit_LimitText(hCtrl, 3);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_LIMIT_SECOND);
	if (hCtrl)
	{
	WCHAR buf[200];
	wsprintf(buf, TEXT("%d"), GetLimitSec());
	Edit_LimitText(hCtrl, 3);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE1);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg1(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg1());
	else
	wcscpy(buf, TEXT(""));
	Edit_LimitText(hCtrl, 255);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE2);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg2(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg2());
	else
	wcscpy(buf, TEXT(""));
	Edit_LimitText(hCtrl, 255);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE3);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg3(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg3());
	else
	wcscpy(buf, TEXT(""));
	Edit_LimitText(hCtrl, 255);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE4);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg4(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg4());
	else
	wcscpy(buf, TEXT(""));
	Edit_LimitText(hCtrl, 255);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE5);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg5(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg5());
	else
	wcscpy(buf, TEXT(""));
	Edit_LimitText(hCtrl, 255);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE6);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg6(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg6());
	else
	wcscpy(buf, TEXT(""));
	Edit_LimitText(hCtrl, 255);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE7);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg7(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg7());
	else
	wcscpy(buf, TEXT(""));
	Edit_LimitText(hCtrl, 255);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE8);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg8(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg8());
	else
	wcscpy(buf, TEXT(""));
	Edit_LimitText(hCtrl, 255);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_SHOW_STATUS);
	if (hCtrl)
	{
	if(m_hPro==NULL)
	Edit_SetText(hCtrl, _UIW(TEXT("Kaillera Server is not running.")));
	else
	Edit_SetText(hCtrl, _UIW(TEXT("Kaillera Server is running...")));
	}

	Button_SetCheck(GetDlgItem(hDlg, IDC_DISABLED),GetDisable());
	Button_SetCheck(GetDlgItem(hDlg, IDC_BAD),GetBad());
	Button_SetCheck(GetDlgItem(hDlg, IDC_LOW),GetLow());
	Button_SetCheck(GetDlgItem(hDlg, IDC_AVERAGE),GetAverage());
	Button_SetCheck(GetDlgItem(hDlg, IDC_GOOD),GetGood());
	Button_SetCheck(GetDlgItem(hDlg, IDC_EXCELLENT),GetExcellent());
	Button_SetCheck(GetDlgItem(hDlg, IDC_LAN),GetLimitLan());
	Button_SetCheck(GetDlgItem(hDlg, IDC_ALLOW_WEBACCESS),GetAllowWebAccess());
	Button_SetCheck(GetDlgItem(hDlg, IDC_AUTORUN),GetAutoRun());
	Button_SetCheck(GetDlgItem(hDlg, IDC_AUTOCLOSE),GetAutoClose());
	Button_SetCheck(GetDlgItem(hDlg, IDC_SHOW_CONSOLE),GetShowConsole());

	return TRUE;

    case WM_HELP:
        break;

    case WM_CONTEXTMENU:
        break;

    case WM_COMMAND :
        switch (GET_WM_COMMAND_ID(wParam, lParam))
        {
	FILE *fp;

        case IDOK :
	SendMessage(m_hPro, SW_HIDE, 0 ,0);
	fp = fopen( "kaillera\\kaillerasrv.conf", "wt" );
	if( fp == NULL ) {
	MameMessageBoxW(_UIW(TEXT("Unable to create kaillera server configuration file.\n\"kaillera\" directory is not exist or you are not authenticated to use this directory!")));
			}
		else {
	
	fprintf( fp, "; Kaillera Server settings.\n" );
	fprintf( fp, "; Auto Generated by EMU_MAX.\n\n" );

	hCtrl  = GetDlgItem(hDlg, IDC_SERVER_NAME);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		SetServerName(buffer);
		fprintf( fp, "ServerName=%s\n", _String(buffer) );
	}


	hCtrl = GetDlgItem(hDlg, IDC_SERVER_LOCATION);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		SetServerLocation(buffer);
		fprintf( fp, "Location=%s\n", _String(buffer) );
	}

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_PAGE);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		SetServerPage(buffer);
		fprintf( fp, "URL=%s\n", _String(buffer) );
	}

	hCtrl = GetDlgItem(hDlg, IDC_MAX_USER);
	if (hCtrl)
	{
		int n = 0;
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		swscanf(buffer,TEXT("%d"),&n);
		SetMaxUser(n);
		fprintf( fp, "MaxUsers=%d\n", n );
	}

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_PORT);
	if (hCtrl)
	{
		int n = 0;
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		swscanf(buffer,TEXT("%d"),&n);
		SetServerPort(n);
		fprintf( fp, "Port=%d\n", n );
	}

	SetLan(Button_GetCheck(GetDlgItem(hDlg, IDC_SERVER_LAN)));
	SetInternet(Button_GetCheck(GetDlgItem(hDlg, IDC_SERVER_INTERNET)));
	fprintf( fp, "Public=%d\n", Button_GetCheck(GetDlgItem(hDlg, IDC_SERVER_INTERNET)));

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_IP);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		SetServerIP(buffer);
		fprintf( fp, "IP=%s\n", _String(buffer) );
	}

	hCtrl = GetDlgItem(hDlg, IDC_LIMIT_MESSAGE);
	if (hCtrl)
	{
		int n = 0;
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		swscanf(buffer,TEXT("%d"),&n);
		SetLimitMsg(n);
		fprintf( fp, "FloodMsgNb=%d\n", n );
	}

	hCtrl = GetDlgItem(hDlg, IDC_LIMIT_SECOND);
	if (hCtrl)
	{
		int n = 0;
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		swscanf(buffer,TEXT("%d"),&n);
		SetLimitSec(n);
		fprintf( fp, "FloodMsgTime=%d\n", n );
	}

	hCtrl = GetDlgItem(hDlg, IDC_MIN_PING);
	if (hCtrl)
	{
		int n = 0;
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		swscanf(buffer,TEXT("%d"),&n);
		SetMinPing(n);
		fprintf( fp, "MinPing=%d\n", n );
	}


	SetDisable(Button_GetCheck(GetDlgItem(hDlg, IDC_DISABLED)));
	SetBad(Button_GetCheck(GetDlgItem(hDlg, IDC_BAD)));
	SetLow(Button_GetCheck(GetDlgItem(hDlg, IDC_LOW)));
	SetAverage(Button_GetCheck(GetDlgItem(hDlg, IDC_AVERAGE)));
	SetGood(Button_GetCheck(GetDlgItem(hDlg, IDC_GOOD)));
	SetExcellent(Button_GetCheck(GetDlgItem(hDlg, IDC_EXCELLENT)));
	SetLimitLan(Button_GetCheck(GetDlgItem(hDlg, IDC_LAN)));

	if(Button_GetCheck(GetDlgItem(hDlg, IDC_DISABLED)))fprintf( fp, "MaxConnSet=0\n" );
	else if(Button_GetCheck(GetDlgItem(hDlg, IDC_BAD)))fprintf( fp, "MaxConnSet=1\n" );
	else if(Button_GetCheck(GetDlgItem(hDlg, IDC_LOW)))fprintf( fp, "MaxConnSet=2\n" );
	else if(Button_GetCheck(GetDlgItem(hDlg, IDC_AVERAGE)))fprintf( fp, "MaxConnSet=3\n" );
	else if(Button_GetCheck(GetDlgItem(hDlg, IDC_GOOD)))fprintf( fp, "MaxConnSet=4\n" );
	else if(Button_GetCheck(GetDlgItem(hDlg, IDC_EXCELLENT)))fprintf( fp, "MaxConnSet=5\n" );
	else if(Button_GetCheck(GetDlgItem(hDlg, IDC_LAN)))fprintf( fp, "MaxConnSet=6\n" );

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE1);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		if(wcscmp(buffer, TEXT(""))!=0)
		SetMsg1(buffer);
		else SetMsg1(TEXT("<NULL>"));
		if(wcslen(buffer)!=0)fprintf( fp, "MotdLine=%s\n", _String(buffer) );
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE2);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		if(wcscmp(buffer, TEXT(""))!=0)
		SetMsg2(buffer);
		else SetMsg2(TEXT("<NULL>"));
		if(wcslen(buffer)!=0)fprintf( fp, "MotdLine=%s\n", _String(buffer) );
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE3);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		if(wcscmp(buffer, TEXT(""))!=0)
		SetMsg3(buffer);
		else SetMsg3(TEXT("<NULL>"));
		if(wcslen(buffer)!=0)fprintf( fp, "MotdLine=%s\n", _String(buffer) );
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE4);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		if(wcscmp(buffer, TEXT(""))!=0)
		SetMsg4(buffer);
		else SetMsg4(TEXT("<NULL>"));
		if(wcslen(buffer)!=0)fprintf( fp, "MotdLine=%s\n", _String(buffer) );
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE5);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		if(wcscmp(buffer, TEXT(""))!=0)
		SetMsg5(buffer);
		else SetMsg5(TEXT("<NULL>"));
		if(wcslen(buffer)!=0)fprintf( fp, "MotdLine=%s\n", _String(buffer) );
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE6);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		if(wcscmp(buffer, TEXT(""))!=0)
		SetMsg6(buffer);
		else SetMsg6(TEXT("<NULL>"));
		if(wcslen(buffer)!=0)fprintf( fp, "MotdLine=%s\n", _String(buffer) );
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE7);
	if (hCtrl)
	{
		WCHAR buffer[200];

		Edit_GetText(hCtrl,buffer,sizeof(buffer));
		if(wcscmp(buffer, TEXT(""))!=0)
		SetMsg7(buffer);
		else SetMsg7(TEXT("<NULL>"));
		if(wcslen(buffer)!=0)fprintf( fp, "MotdLine=%s\n", _String(buffer) );
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE8);
	if (hCtrl)
	{
	WCHAR buffer[200];
	Edit_GetText(hCtrl,buffer,sizeof(buffer));
	if(wcscmp(buffer, TEXT(""))!=0)
	SetMsg8(buffer);
	else SetMsg8(TEXT("<NULL>"));
	if(wcslen(buffer)!=0)fprintf( fp, "MotdLine=%s\n", _String(buffer) );
	}

	SetAllowWebAccess(Button_GetCheck(GetDlgItem(hDlg, IDC_ALLOW_WEBACCESS)));
	fprintf( fp, "AllowWebAccess=%d\n", Button_GetCheck(GetDlgItem(hDlg, IDC_ALLOW_WEBACCESS)));

	SetAutoRun(Button_GetCheck(GetDlgItem(hDlg, IDC_AUTORUN)));
	SetAutoClose(Button_GetCheck(GetDlgItem(hDlg, IDC_AUTOCLOSE)));
	SetShowConsole(Button_GetCheck(GetDlgItem(hDlg, IDC_SHOW_CONSOLE)));

	fclose(fp);
	}

        case IDCANCEL :
        EndDialog(hDlg, 0);
        return TRUE;

        case IDC_CHECK_IP :
	{
	char buffer[16];
	strcpy(buffer,CheckIP());
	if(strcmp(buffer, "")!=0)
	Edit_SetText(GetDlgItem(hDlg, IDC_SERVER_IP), _Unicode(buffer));
        return TRUE;
	}

        case IDC_PROP_RESET :
	{
	hCtrl = GetDlgItem(hDlg, IDC_SERVER_NAME);
	if (hCtrl)
	Edit_SetText(hCtrl, GetServerName());

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_PAGE);
	if (hCtrl)
	Edit_SetText(hCtrl, GetServerPage());

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_PORT);
	if (hCtrl)
	{
	unsigned i;
	char buf[200];
	i = GetServerPort();
	sprintf(buf, "%u", i);
	Edit_SetTextA(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_LOCATION);
	if (hCtrl)
	Edit_SetText(hCtrl, GetServerLocation());

	hCtrl = GetDlgItem(hDlg, IDC_MAX_USER);
	if (hCtrl)
	{
	int i;
	WCHAR buf[200];
	i = GetMaxUser();
	wsprintf(buf, TEXT("%d"), i);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_IP);
	if (hCtrl)
	Edit_SetText(hCtrl, GetServerIP());

	hCtrl = GetDlgItem(hDlg, IDC_MIN_PING);
	if (hCtrl)
	{
	int i;
	WCHAR buf[200];
	i = GetMinPing();
	wsprintf(buf, TEXT("%d"), i);
	Edit_SetText(hCtrl, buf);
	}

	Button_SetCheck(GetDlgItem(hDlg, IDC_SERVER_LAN),GetLan());
	Button_SetCheck(GetDlgItem(hDlg, IDC_SERVER_INTERNET),GetInternet());

	hCtrl = GetDlgItem(hDlg, IDC_LIMIT_MESSAGE);
	if (hCtrl)
	{
	int i;
	WCHAR buf[200];
	i = GetLimitMsg();
	wsprintf(buf, TEXT("%d"), i);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_LIMIT_SECOND);
	if (hCtrl)
	{
	int i;
	WCHAR buf[200];
	i = GetLimitSec();
	wsprintf(buf, TEXT("%d"), i);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE1);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg1(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg1());
	else
	wcscpy(buf, TEXT(""));
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE2);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg2(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg2());
	else
	wcscpy(buf, TEXT(""));
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE3);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg3(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg3());
	else
	wcscpy(buf, TEXT(""));
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE4);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg4(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg4());
	else
	wcscpy(buf, TEXT(""));
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE5);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg5(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg5());
	else
	wcscpy(buf, TEXT(""));
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE6);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg6(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg6());
	else
	wcscpy(buf, TEXT(""));
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE7);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg7(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg8());
	else
	wcscpy(buf, TEXT(""));
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE8);
	if (hCtrl)
	{
	WCHAR buf[255];
	if(wcscmp(GetMsg8(), TEXT("<NULL>"))!=0)
	wsprintf(buf, TEXT("%s"), GetMsg8());
	else
	wcscpy(buf, TEXT(""));
	Edit_SetText(hCtrl, buf);
	}

	Button_SetCheck(GetDlgItem(hDlg, IDC_DISABLED),GetDisable());
	Button_SetCheck(GetDlgItem(hDlg, IDC_BAD),GetBad());
	Button_SetCheck(GetDlgItem(hDlg, IDC_LOW),GetLow());
	Button_SetCheck(GetDlgItem(hDlg, IDC_AVERAGE),GetAverage());
	Button_SetCheck(GetDlgItem(hDlg, IDC_GOOD),GetGood());
	Button_SetCheck(GetDlgItem(hDlg, IDC_EXCELLENT),GetExcellent());
	Button_SetCheck(GetDlgItem(hDlg, IDC_LAN),GetLimitLan());
	Button_SetCheck(GetDlgItem(hDlg, IDC_ALLOW_WEBACCESS),GetAllowWebAccess());
	Button_SetCheck(GetDlgItem(hDlg, IDC_AUTORUN),GetAutoRun());
	Button_SetCheck(GetDlgItem(hDlg, IDC_AUTOCLOSE),GetAutoClose());
	Button_SetCheck(GetDlgItem(hDlg, IDC_SHOW_CONSOLE),GetShowConsole());
        return TRUE;
	}

        case IDC_USE_DEFAULT :
	{
	hCtrl = GetDlgItem(hDlg, IDC_SERVER_NAME);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT("Unknown serv0r"));

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_PAGE);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT("http://"));

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_PORT);
	if (hCtrl)
	{
	WCHAR buf[200];
	wsprintf(buf, TEXT("%d"), 27888);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_LOCATION);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT("Unknown location"));

	hCtrl = GetDlgItem(hDlg, IDC_MAX_USER);
	if (hCtrl)
	{
	WCHAR buf[200];
	wsprintf(buf, TEXT("%d"), 50);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_SERVER_IP);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT("127.0.0.1"));

	hCtrl = GetDlgItem(hDlg, IDC_MIN_PING);
	if (hCtrl)
	{
	WCHAR buf[200];
	wsprintf(buf, TEXT("%d"), 60);
	Edit_SetText(hCtrl, buf);
	}

	Button_SetCheck(GetDlgItem(hDlg, IDC_SERVER_LAN),TRUE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_SERVER_INTERNET),FALSE);

	hCtrl = GetDlgItem(hDlg, IDC_LIMIT_MESSAGE);
	if (hCtrl)
	{
	WCHAR buf[200];
	wsprintf(buf, TEXT("%d"), 5);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_LIMIT_SECOND);
	if (hCtrl)
	{
	WCHAR buf[200];
	wsprintf(buf, TEXT("%d"), 3);
	Edit_SetText(hCtrl, buf);
	}

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE1);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT("Welcome to unknown serv0r! You can"));

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE2);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT("see our website at http://web.site/"));

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE3);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT(""));

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE4);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT(""));

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE5);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT(""));

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE6);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT(""));

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE7);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT(""));

	hCtrl = GetDlgItem(hDlg, IDC_MESSAGE8);
	if (hCtrl)
	Edit_SetText(hCtrl, TEXT(""));

	Button_SetCheck(GetDlgItem(hDlg, IDC_DISABLED),TRUE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_BAD),FALSE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_LOW),FALSE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_AVERAGE),FALSE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_GOOD),FALSE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_EXCELLENT),FALSE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_LAN),FALSE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_ALLOW_WEBACCESS),FALSE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_AUTORUN),FALSE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_AUTOCLOSE),TRUE);
	Button_SetCheck(GetDlgItem(hDlg, IDC_SHOW_CONSOLE),FALSE);
        return TRUE;
		}
	}
	break;
    }
    return 0;
}


#include <winsock2.h>
const char *CheckIP(void)
{
 WSADATA wsaData;
 char host_name[255];
 PHOSTENT hostinfo; 

 if ( WSAStartup( MAKEWORD(2,0), &wsaData ) == 0 )
  {
   if( gethostname ( host_name, sizeof(host_name)) == 0)
	{
   	if((hostinfo = gethostbyname(host_name)) != NULL)
		{
		strcpy(pszAddr,inet_ntoa (*(struct in_addr *)*hostinfo->h_addr_list));
   		}
  	}
  WSACleanup();
  }
  else
  	MameMessageBoxW(_UIW(TEXT("Winsock Error:\nUnable to check local IP address!")));//Error message
 	return pszAddr;
}
#endif /* KSERVER */

#ifdef EXPORT_GAME_LIST
INT_PTR CALLBACK ExportOptionDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    HWND hCtrl;

    switch (Msg)
    {
    case WM_INITDIALOG:
	TranslateDialog(hDlg, lParam, TRUE);
	CenterWindow(hDlg);
	if(nExportContent==1)Button_SetCheck(GetDlgItem(hDlg, IDC_EXPORT_ALL), TRUE);
	if(nExportContent==2)Button_SetCheck(GetDlgItem(hDlg, IDC_EXPORT_CURRENT), TRUE);
	if(nExportContent==3)Button_SetCheck(GetDlgItem(hDlg, IDC_EXPORT_ROM), TRUE);
	if(nExportContent==4)Button_SetCheck(GetDlgItem(hDlg, IDC_EXPORT_GAME), TRUE);
	return TRUE;

    case WM_HELP:
        break;

    case WM_CONTEXTMENU:
        break;

    case WM_COMMAND :
        switch (GET_WM_COMMAND_ID(wParam, lParam))
        {
        case IDOK :
	SendMessage(m_hPro, SW_HIDE, 0 ,0);

	hCtrl = GetDlgItem(hDlg, IDC_EXPORT_ALL);
	if( Button_GetCheck(hCtrl) )
	{
	nExportContent=1;
	}

	hCtrl = GetDlgItem(hDlg, IDC_EXPORT_CURRENT);
	if( Button_GetCheck(hCtrl) )
	{
	nExportContent=2;
	}

	hCtrl = GetDlgItem(hDlg, IDC_EXPORT_ROM);
	if( Button_GetCheck(hCtrl) )
	{
	nExportContent=3;
	}

	hCtrl = GetDlgItem(hDlg, IDC_EXPORT_GAME);
	if( Button_GetCheck(hCtrl) )
	{
	nExportContent=4;
	}
            EndDialog(hDlg, 1);
            return TRUE;

        case IDCANCEL :
            EndDialog(hDlg, 0);
            return TRUE;
	}
	break;
    }
    return 0;
}
#endif /* EXPORT_GAME_LIST */

/* End of source file */
