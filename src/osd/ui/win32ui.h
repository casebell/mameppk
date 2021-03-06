/***************************************************************************

  M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
  Win32 Portions Copyright (C) 1997-2003 Michael Soderstrom and Chris Kirmse
    
  This file is part of MAME32, and may only be used, modified and
  distributed under the terms of the MAME license, in "readme.txt".
  By continuing to use, modify or distribute this file you indicate
  that you have read the license and understand and accept it fully.

 ***************************************************************************/

#ifndef WIN32UI_H
#define WIN32UI_H

#include "MAME32.h"	// include this first
#include <driver.h>

#if !defined(SEARCH_PROMPT)
#define SEARCH_PROMPT "type a keyword"
#endif

enum
{
	TAB_PICKER = 0,
	TAB_DISPLAY,
	TAB_MISC,
	NUM_TABS
};

typedef struct
{
	INT resource;
	const char *icon_name;
} ICONDATA;

struct _driverw
{
	WCHAR *name;
	WCHAR *description;
	WCHAR *modify_the;
	WCHAR *manufacturer;
	WCHAR *year;
	WCHAR *source_file;
};

/* in win32ui.c */
extern struct _driverw **driversw;

/* in layout.c */
extern const ICONDATA g_iconData[];


HWND GetMainWindow(void);
HWND GetTreeView(void);
int GetNumGames(void);
void GetRealColumnOrder(int order[]);
HICON LoadIconFromFile(const char *iconname);
void UpdateScreenShot(void);
void ResizePickerControls(HWND hWnd);
LPWSTR GetSearchText(void);
#ifdef USE_VIEW_PCBINFO
void PaintBackgroundImage(HWND hWnd, HRGN hRgn, int x, int y);
#endif /* USE_VIEW_PCBINFO */

void UpdateListView(void);

// Convert Ampersand so it can display in a static control
LPWSTR ConvertAmpersandString(LPCWSTR s);

// globalized for painting tree control
HBITMAP GetBackgroundBitmap(void);
HPALETTE GetBackgroundPalette(void);
MYBITMAPINFO * GetBackgroundInfo(void);
BOOL GetUseOldControl(void);
BOOL GetUseXPControl(void);

int GetMinimumScreenShotWindowWidth(void);

// we maintain an array of drivers sorted by name, useful all around
int GetDriverIndex(const game_driver *driver);
int GetParentIndex(const game_driver *driver);
int GetParentRomSetIndex(const game_driver *driver);
#if defined(KAILLERA) || defined(MAME32PLUSPLUS)
int GetParentRomSetIndex2(const game_driver *driver);
#endif
int GetGameNameIndex(const char *name);
int GetIndexFromSortedIndex(int sorted_index);

// sets text in part of the status bar on the main window
void SetStatusBarTextW(int part_index, const WCHAR *message);
void SetStatusBarTextFW(int part_index, const WCHAR *fmt, ...);

BOOL MouseHasBeenMoved(void);
#endif
