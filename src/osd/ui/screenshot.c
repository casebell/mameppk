/***************************************************************************

  M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
  Win32 Portions Copyright (C) 1997-2003 Michael Soderstrom and Chris Kirmse

  This file is part of MAME32, and may only be used, modified and
  distributed under the terms of the MAME license, in "readme.txt".
  By continuing to use, modify or distribute this file you indicate
  that you have read the license and understand and accept it fully.
    
***************************************************************************/

/***************************************************************************

  Screenshot.c
  
    Win32 DIB handling.
    
      Created 7/1/98 by Mike Haaland (mhaaland@hypertech.com)
      
***************************************************************************/

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include "mame32.h"
#include "driver.h"
#include "png.h"
#include "osdepend.h"
#include "screenshot.h"
#include "file.h"
#include "bitmask.h"
#include "winuiopt.h"
#include "m32util.h"
#include "win32ui.h"
#include "translate.h"

/***************************************************************************
    function prototypes
***************************************************************************/

static BOOL     AllocatePNG(png_info *p, HGLOBAL *phDIB, HPALETTE* pPal);

static int png_read_bitmap_gui(LPVOID mfile, HGLOBAL *phDIB, HPALETTE *pPAL);
/***************************************************************************
    Static global variables
***************************************************************************/

/* these refer to the single image currently loaded by the ScreenShot functions */
static HGLOBAL   m_hDIB = NULL;
static HPALETTE  m_hPal = NULL;
static HANDLE m_hDDB = NULL;
static int current_image_game = -1;
static int current_image_type = -1;

#define WIDTHBYTES(width) ((width) / 8)

/* PNG variables */

static int   copy_size = 0;
static char* pixel_ptr = 0;
static int   row = 0;
static int   effWidth;

/***************************************************************************
    Functions
***************************************************************************/

BOOL ScreenShotLoaded(void)
{
	return m_hDDB != NULL;
}

#ifdef MESS
static BOOL LoadSoftwareScreenShot(const WCHAR *drv_name, LPCWSTR lpSoftwareName, int nType)
{
	WCHAR *s = alloca((wcslen(drv_name) + 1 + wcslen(lpSoftwareName) + 5) * sizeof (*s));
	swprintf(s, TEXT("%s/%s.png"), drv_name, lpSoftwareName);
	return LoadDIB(s, &m_hDIB, &m_hPal, nType);
}
#endif /* MESS */

/* Allow us to pre-load the DIB once for future draws */
#ifdef MESS
BOOL LoadScreenShotEx(int nGame, LPCWSTR lpSoftwareName, int nType)
#else /* !MESS */
#ifdef USE_IPS
BOOL LoadScreenShot(int nGame, LPCWSTR lpIPSName, int nType)
#else /* USE_IPS */
BOOL LoadScreenShot(int nGame, int nType)
#endif /* USE_IPS */
#endif /* MESS */
{
	BOOL loaded = FALSE;
	int nParentIndex = GetParentIndex(drivers[nGame]);
	WCHAR buf[MAX_PATH];

	/* No need to reload the same one again */
#ifndef MESS
	if (nGame == current_image_game && nType == current_image_type)
		return TRUE;
#endif

	/* Delete the last ones */
	FreeScreenShot();

	/* Load the DIB */
#ifdef MESS
	if (lpSoftwareName)
	{
		loaded = LoadSoftwareScreenShot(driversw[nGame]->name, lpSoftwareName, nType);
		if (!loaded && DriverIsClone(nGame) == TRUE)
		{
			loaded = LoadSoftwareScreenShot(driversw[nParentIndex]->name, lpSoftwareName, nType);
		}
	}
	if (!loaded)
#endif /* MESS */
	{
		WCHAR *wdrv = driversw[nGame]->name;

#ifdef USE_IPS
		if (lpIPSName)
		{
			wsprintf(buf, TEXT("%s/%s"), wdrv, lpIPSName);
			dwprintf(TEXT("found ipsname: %s"), buf);
		}
		else
#endif /* USE_IPS */
		{
			wcscpy(buf, wdrv);
			dwprintf(TEXT("not found ipsname: %s"), buf);
		}

		loaded = LoadDIB(buf, &m_hDIB, &m_hPal, nType);
	}

	/* If not loaded, see if there is a clone and try that */
	if (!loaded && nParentIndex >= 0)
	{
		WCHAR *wdrv = driversw[nParentIndex]->name;

#ifdef USE_IPS
		if (lpIPSName)
		{
			wsprintf(buf, TEXT("%s/%s"), wdrv, lpIPSName);
			dwprintf(TEXT("found clone ipsname: %s"), buf);
		}
		else
#endif /* USE_IPS */
			wcscpy(buf, wdrv);

		loaded = LoadDIB(buf, &m_hDIB, &m_hPal, nType);
		nParentIndex = GetParentIndex(drivers[nParentIndex]);
		if (!loaded && nParentIndex >= 0)
		{
			wdrv = driversw[nParentIndex]->name;

#ifdef USE_IPS
			if (lpIPSName)
				swprintf(buf, TEXT("%s/%s"), wdrv, lpIPSName);
			else
#endif /* USE_IPS */
				wcscpy(buf, wdrv);

			loaded = LoadDIB(buf, &m_hDIB, &m_hPal, nType);
		}
	}

	if (loaded)
	{
		HDC hdc = GetDC(GetMainWindow());
		m_hDDB = DIBToDDB(hdc, m_hDIB, NULL);
		ReleaseDC(GetMainWindow(),hdc);
		
		current_image_game = nGame;
		current_image_type = nType;

	}

	return (loaded) ? TRUE : FALSE;
}

HANDLE GetScreenShotHandle()
{
	return m_hDDB;
}

int GetScreenShotWidth(void)
{
	return ((LPBITMAPINFO)m_hDIB)->bmiHeader.biWidth;
}

int GetScreenShotHeight(void)
{
	return ((LPBITMAPINFO)m_hDIB)->bmiHeader.biHeight;
}

/* Delete the HPALETTE and Free the HDIB memory */
void FreeScreenShot(void)
{
	if (m_hDIB != NULL)
		GlobalFree(m_hDIB);
	m_hDIB = NULL;

	if (m_hPal != NULL)
		DeleteObject(m_hPal);
	m_hPal = NULL;

	if (m_hDDB != NULL)
		DeleteObject(m_hDDB);
	m_hDDB = NULL;

	current_image_game = -1;
	current_image_type = -1;
}

BOOL LoadDIB(const WCHAR *filename, HGLOBAL *phDIB, HPALETTE *pPal, int pic_type)
{
	mame_file *mfile;
	file_error filerr;
	BOOL success;
	const WCHAR *zip_name = NULL;
	const WCHAR *basedir = NULL;
	astring *fname;
	char *utf8filename;

	switch (pic_type)
	{
	case TAB_SCREENSHOT :
		basedir = GetImgDirs();
		zip_name = TEXT("snap");
		break;
	case TAB_FLYER :
		basedir = GetFlyerDirs();
		zip_name = TEXT("flyers");
		break;
	case TAB_CABINET :
		basedir = GetCabinetDirs();
		zip_name = TEXT("cabinets");
		break;
	case TAB_MARQUEE :
		basedir = GetMarqueeDirs();
		zip_name =TEXT( "marquees");
		break;
	case TAB_TITLE :
		basedir = GetTitlesDirs();
		zip_name = TEXT("titles");
		break;
	case TAB_CONTROL_PANEL :
		basedir = GetControlPanelDirs();
		zip_name = TEXT("cpanel");
		break;
	case BACKGROUND :
		basedir = GetBgDir();
		zip_name = TEXT("bkground");
		break;
#ifdef USE_IPS
	case TAB_IPS :
		basedir = GetPatchDir();
		zip_name = TEXT("ips");
		break;
#endif /* USE_IPS */
	default :
		// in case a non-image tab gets here, which can happen
		return FALSE;
	}

	set_core_snapshot_directory(basedir);

	utf8filename = utf8_from_wstring(filename);

	// look for the raw file
	fname = astring_assemble_2(astring_alloc(), utf8filename, ".png");
	filerr = mame_fopen_options(get_core_options(), SEARCHPATH_SCREENSHOT, astring_c(fname), OPEN_FLAG_READ, &mfile);
	astring_free(fname);

	if (filerr != FILERR_NONE)
	{
		// and look for the zip
		fname = astring_assemble_4(astring_alloc(), utf8filename, PATH_SEPARATOR, utf8filename, ".png");
		filerr = mame_fopen_options(get_core_options(), SEARCHPATH_SCREENSHOT, astring_c(fname), OPEN_FLAG_READ, &mfile);
		astring_free(fname);
	}

	if (filerr != FILERR_NONE)
	{
		// and look for the zip
		char *utf8zip_name = utf8_from_wstring(zip_name);
		fname = astring_assemble_4(astring_alloc(), utf8zip_name, PATH_SEPARATOR, utf8filename, ".png");
		filerr = mame_fopen_options(get_core_options(), SEARCHPATH_SCREENSHOT, astring_c(fname), OPEN_FLAG_READ, &mfile);
		astring_free(fname);
		free(utf8zip_name);
	}

#ifdef MAME32PLUSPLUS
	if (filerr != FILERR_NONE)
	{
		fname = astring_assemble_3(astring_alloc(), utf8filename, PATH_SEPARATOR, "0000.png");
		filerr = mame_fopen_options(get_core_options(), SEARCHPATH_SCREENSHOT, astring_c(fname), OPEN_FLAG_READ, &mfile);
		astring_free(fname);
	}

	if (filerr != FILERR_NONE)
	{
		// and look for the zip
		fname = astring_assemble_5(astring_alloc(), utf8filename, PATH_SEPARATOR, utf8filename, PATH_SEPARATOR, "0000.png");
		filerr = mame_fopen_options(get_core_options(), SEARCHPATH_SCREENSHOT, astring_c(fname), OPEN_FLAG_READ, &mfile);
		astring_free(fname);
	}

	if (filerr != FILERR_NONE)
	{
		// and look for the zip
		char *utf8zip_name = utf8_from_wstring(zip_name);
		fname = astring_assemble_5(astring_alloc(), utf8zip_name, PATH_SEPARATOR, utf8filename, PATH_SEPARATOR, "0000.png");
		filerr = mame_fopen_options(get_core_options(), SEARCHPATH_SCREENSHOT, astring_c(fname), OPEN_FLAG_READ, &mfile);
		astring_free(fname);
		free(utf8zip_name);
	}
#endif /* MAME32PLUSPLUS */

	free(utf8filename);

	if (filerr != FILERR_NONE)
		return FALSE;

	success = png_read_bitmap_gui(mame_core_file(mfile), phDIB, pPal);

	mame_fclose(mfile);

	return success;
}

HBITMAP DIBToDDB(HDC hDC, HANDLE hDIB, LPMYBITMAPINFO desc)
{
	LPBITMAPINFOHEADER	lpbi;
	HBITMAP 			hBM;
	int 				nColors;
	BITMAPINFO *		bmInfo = (LPBITMAPINFO)hDIB;
	LPVOID				lpDIBBits;

	if (hDIB == NULL)
		return NULL;

	lpbi = (LPBITMAPINFOHEADER)hDIB;
	nColors = lpbi->biClrUsed ? lpbi->biClrUsed : 1 << lpbi->biBitCount;

	if (bmInfo->bmiHeader.biBitCount > 8)
		lpDIBBits = (LPVOID)((LPDWORD)(bmInfo->bmiColors + 
			bmInfo->bmiHeader.biClrUsed) + 
			((bmInfo->bmiHeader.biCompression == BI_BITFIELDS) ? 3 : 0));
	else
		lpDIBBits = (LPVOID)(bmInfo->bmiColors + nColors);

	if (desc != 0)
	{
		/* Store for easy retrieval later */
		desc->bmWidth  = bmInfo->bmiHeader.biWidth;
		desc->bmHeight = bmInfo->bmiHeader.biHeight;
		desc->bmColors = (nColors <= 256) ? nColors : 0;
	}

	hBM = CreateDIBitmap(hDC,					  /* handle to device context */
	                    (LPBITMAPINFOHEADER)lpbi, /* pointer to bitmap info header	*/
	                    (LONG)CBM_INIT, 		  /* initialization flag */
	                    lpDIBBits,				  /* pointer to initialization data  */
	                    (LPBITMAPINFO)lpbi, 	  /* pointer to bitmap info */
	                    DIB_RGB_COLORS);		  /* color-data usage  */

	return hBM;
}


/***************************************************************************
    PNG graphics handling functions
***************************************************************************/

static void store_pixels(UINT8 *buf, int len)
{
	if (pixel_ptr && copy_size)
	{
		memcpy(&pixel_ptr[row * effWidth], buf, len);
		row--;
		copy_size -= len;
	}
}

BOOL AllocatePNG(png_info *p, HGLOBAL *phDIB, HPALETTE *pPal)
{
	int 				dibSize;
	HGLOBAL 			hDIB;
	BITMAPINFOHEADER	bi;
	LPBITMAPINFOHEADER	lpbi;
	LPBITMAPINFO		bmInfo;
	LPVOID				lpDIBBits = 0;
	int 				lineWidth = 0;
	int 				nColors = 0;
	RGBQUAD*			pRgb;
	copy_size = 0;
	pixel_ptr = 0;
	row 	  = p->height - 1;
	lineWidth = p->width;
	
	if (p->color_type != 2 && p->num_palette <= 256)
		nColors =  p->num_palette;

	bi.biSize			= sizeof(BITMAPINFOHEADER); 
	bi.biWidth			= p->width;
	bi.biHeight 		= p->height;
	bi.biPlanes 		= 1;
	bi.biBitCount		= (p->color_type == 3) ? 8 : 24; /* bit_depth; */
	
	bi.biCompression	= BI_RGB; 
	bi.biSizeImage		= 0; 
	bi.biXPelsPerMeter	= 0; 
	bi.biYPelsPerMeter	= 0; 
	bi.biClrUsed		= nColors;
	bi.biClrImportant	= nColors;
	
	effWidth = (long)(((long)lineWidth*bi.biBitCount + 31) / 32) * 4;
	
	dibSize = (effWidth * bi.biHeight);
	hDIB = GlobalAlloc(GMEM_FIXED, bi.biSize + (nColors * sizeof(RGBQUAD)) + dibSize);
	
	if (!hDIB)
	{
		return FALSE;
	}

	lpbi = (LPVOID)hDIB;
	memcpy(lpbi, &bi, sizeof(BITMAPINFOHEADER));
	pRgb = (RGBQUAD*)((LPSTR)lpbi + bi.biSize);
	lpDIBBits = (LPVOID)((LPSTR)lpbi + bi.biSize + (nColors * sizeof(RGBQUAD)));
	if (nColors)
	{
		int i;
		/*
		  Convert a PNG palette (3 byte RGBTRIPLEs) to a new
		  color table (4 byte RGBQUADs)
		*/
		for (i = 0; i < nColors; i++)
		{
			RGBQUAD rgb;
			
			rgb.rgbRed		= p->palette[i * 3 + 0];
			rgb.rgbGreen	= p->palette[i * 3 + 1];
			rgb.rgbBlue 	= p->palette[i * 3 + 2];
			rgb.rgbReserved = (BYTE)0;
			
			pRgb[i] = rgb; 
		} 
	} 
	
	bmInfo = (LPBITMAPINFO)hDIB;
	
	/* Create a halftone palette if colors > 256. */
	if (0 == nColors || nColors > 256)
	{
		HDC hDC = CreateCompatibleDC(0); /* Desktop DC */
		*pPal = CreateHalftonePalette(hDC);
		DeleteDC(hDC);
	}
	else
	{
		UINT nSize = sizeof(LOGPALETTE) + (sizeof(PALETTEENTRY) * nColors);
		LOGPALETTE *pLP = (LOGPALETTE *)malloc(nSize);
		int  i;
		
		pLP->palVersion 	= 0x300;
		pLP->palNumEntries	= nColors;
		
		for (i = 0; i < nColors; i++)
		{
			pLP->palPalEntry[i].peRed	= bmInfo->bmiColors[i].rgbRed;
			pLP->palPalEntry[i].peGreen = bmInfo->bmiColors[i].rgbGreen; 
			pLP->palPalEntry[i].peBlue	= bmInfo->bmiColors[i].rgbBlue;
			pLP->palPalEntry[i].peFlags = 0;
		}
		
		*pPal = CreatePalette(pLP);
		
		free (pLP);
	}
	
	copy_size = dibSize;
	pixel_ptr = lpDIBBits;
	*phDIB = hDIB;
	return TRUE;
}

/* Copied and modified from png.c */
static int png_read_bitmap_gui(LPVOID mfile, HGLOBAL *phDIB, HPALETTE *pPAL)
{
	png_info p;
	UINT32 i;
	int bytespp;
	
	if (png_read_file(mfile, &p) != PNGERR_NONE)
		return 0;

	if (p.color_type != 3 && p.color_type != 2)
	{
		logerror("Unsupported color type %i (has to be 3)\n", p.color_type);
		free(p.image);
		return 0;
	}
	if (p.interlace_method != 0)
	{
		logerror("Interlace unsupported\n");
		free (p.image);
		return 0;
	}
 
	/* Convert < 8 bit to 8 bit */
	png_expand_buffer_8bit(&p);
	
	if (!AllocatePNG(&p, phDIB, pPAL))
	{
		logerror("Unable to allocate memory for artwork\n");
		free(p.image);
		return 0;
	}

	bytespp = (p.color_type == 2) ? 3 : 1;

	for (i = 0; i < p.height; i++)
	{
		UINT8 *ptr = p.image + i * (p.width * bytespp);

		if (p.color_type == 2) /*(p->bit_depth > 8) */
		{
			int j;
			UINT8 bTmp;

			for (j = 0; j < p.width; j++)
			{
				bTmp = ptr[0];
				ptr[0] = ptr[2];
				ptr[2] = bTmp;
				ptr += 3;
			}
		}	
		store_pixels(p.image + i * (p.width * bytespp), p.width * bytespp);
	}

	free(p.image);

	return 1;
}

#ifdef MESS
BOOL LoadScreenShot(int nGame, int nType)
{
	return LoadScreenShotEx(nGame, NULL, nType);
}
#endif

/* End of source */
