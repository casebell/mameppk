/***************************************************************************

	PSX GPU using external GPU Plugins by DarkCoder.

    function skeleton borrowed from smf's software renderer.
	Thanks to smf.

  ------------------------------------------------------------------------
  P.E.Op.S. page on sourceforge: https://sourceforge.net/projects/peops/

  In fact, I don't know the psx things(GPU, SPU, etc...). So it may have 
  small/serious bugs. :) - DarkCoder

  Change Log
  ----------
    2004/11/03 - Support Pete's OpenGL2 PSX GPU

    2004/10/22 - cryptklr GPU Fix

	2004/08/29 - Support GPU option UI. create cfg file on the fly.

  	2004/06/09 - Support dmaChain transfer for sprite invisible problem
	             with various game.
			   - Small change in screen update routine.
  
	2004/06/04 - Fix gputype mismatch with tekken/tekken2 (test3)

	2004/06/03 - Fix screen flicking problem with doapp. (test2)

	2004/06/02 - First draft.

***************************************************************************/

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <io.h>

#include "window.h"
#define win_video_window		win_window_list->hwnd

// window styles for psx game window
#define PSX_WINDOW_STYLE			(WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_MINIMIZEBOX)
#define PSX_WINDOW_STYLE_EX			0

// full screen window styles
#define PSX_FULLSCREEN_STYLE		WS_POPUP
#define PSX_FULLSCREEN_STYLE_EX		WS_EX_TOPMOST

// GPUSTATUS FLAGs : from P.E.Op.S. (soft gpu plugin based on Pete's soft gpus)
#define GPUSTATUS_DISPLAYDISABLED     0x00800000

typedef struct GPUOTAG
{
	unsigned long  Version;        // Version of structure - currently 1
	long           hWnd;           // Window handle
	unsigned long  ScreenRotation; // 0 = 0CW, 1 = 90CW, 2 = 180CW, 3 = 270CW = 90CCW
	unsigned long  GPUVersion;     // 0 = a, 1 = b, 2 = c
	const char*    GameName;       // NULL terminated string
	const char*    CfgFile;        // NULL terminated string
} GPUConfiguration_t;

typedef struct GPUFREEZETAG
{
 unsigned long ulFreezeVersion;      // should be always 1 for now (set by main emu)
 unsigned long ulStatus;             // current gpu status
 unsigned long ulControl[256];       // latest control register values
 unsigned char psxVRam[1024*1024*2]; // current VRam image (full 2 MB for ZN)
} GPUFreeze_t;


//================================================================
// internal variables
//================================================================
static int		classes_created = 0;
static int		bUseGPUPlugin   = 0;
static int		bMakeGameWindow = 0;
static int		bIsInterlaced	= 0;
static int		bDisplayEnabled	= 0;
static int		bVblankSignal	= 0;
static char		szGPUPlugin[MAX_PATH];
static char		szGameName [MAX_PATH];
static char		szCfgName  [MAX_PATH];
//static GPUFreeze_t	_gpuPF;

// Some game identifier (for hack routine)
#define ROM_RVSCHOOL	1	// Rival Schools
static char		iRomId			= 0;
static char		ihackLevel		= 0;

static unsigned int		igpuWriteCounter	= 0;
static unsigned int		igpuReadCounter		= 0;

unsigned long*	_dmaChainData	  = NULL;
unsigned long	_dmaChainOffs	  = 0;
unsigned long	_dmaChainLastAddr = 0;


//================================================================
// global/external variables & definition
//================================================================

HWND			m_hPSXWnd = NULL;
int				m_iPSXFullScreen = 1;

// from windows/window.c
extern	int		nUseExternalDraw;
void logmsg(const char*lpszFmt,...);

//================================================================
// function type definitions for GPU plugin interface
//================================================================
typedef long	(CALLBACK *PROC_ZNGPUCLOSE				)(void);
typedef long	(CALLBACK *PROC_ZNGPUDMACHAIN			)(unsigned long * baseAddrL, unsigned long addr);
typedef long	(CALLBACK *PROC_ZNGPUDMASLICEIN			)(unsigned long *baseAddrL, unsigned long addr, unsigned long iSize);
typedef long	(CALLBACK *PROC_ZNGPUDMASLICEOUT		)(unsigned long *baseAddrL, unsigned long addr, unsigned long iSize);
typedef long	(CALLBACK *PROC_ZNGPUFREEZE				)(unsigned long ulGetFreezeData,void * pF);
typedef long	(CALLBACK *PROC_ZNGPUGETMODE			)(void);
typedef long	(CALLBACK *PROC_ZNGPUINIT				)(void);
typedef void	(CALLBACK *PROC_ZNGPUMAKESNAPSHOT		)(void);
typedef long	(CALLBACK *PROC_ZNGPUOPEN				)(void * vcfg);
typedef unsigned long (CALLBACK *PROC_ZNGPUREADDATA		)(void);
typedef unsigned long (CALLBACK *PROC_ZNGPUREADSTATUS	)(void);
typedef void	(CALLBACK *PROC_ZNGPUSETMODE			)(unsigned long gdata);
typedef long	(CALLBACK *PROC_ZNGPUSHUTDOWN			)(void);
typedef void	(CALLBACK *PROC_ZNGPUUPDATELACE			)(void);		// update lace is called evry VSync
typedef void	(CALLBACK *PROC_ZNGPUWRITEDATA			)(unsigned long gdata);
typedef void	(CALLBACK *PROC_ZNGPUWRITESTATUS		)(unsigned long gdata);

typedef long	(CALLBACK *PROC_GPUCONFIGURE			)(void);	// OpenGL2

//================================================================
// struct for GPU plugin library management
//================================================================
typedef struct _tagPSXGPULIB {
	BOOL	bIsLoaded;
	HMODULE hLib;

	// function pointer
	PROC_ZNGPUCLOSE			lpfnGPUclose;
	PROC_ZNGPUDMACHAIN		lpfnGPUdmaChain;
	PROC_ZNGPUDMASLICEIN	lpfnGPUdmaSliceIn;
	PROC_ZNGPUDMASLICEOUT	lpfnGPUdmaSliceOut;
	PROC_ZNGPUFREEZE		lpfnGPUfreeze;
	PROC_ZNGPUGETMODE		lpfnGPUgetMode;
	PROC_ZNGPUINIT			lpfnGPUinit;
	PROC_ZNGPUMAKESNAPSHOT	lpfnGPUmakeSnapshot;
	PROC_ZNGPUOPEN			lpfnGPUopen;
	PROC_ZNGPUREADDATA		lpfnGPUreadData;
	PROC_ZNGPUREADSTATUS    lpfnGPUreadStatus;
	PROC_ZNGPUSETMODE		lpfnGPUsetMode;
	PROC_ZNGPUSHUTDOWN		lpfnGPUshutdown;
	PROC_ZNGPUUPDATELACE	lpfnGPUupdateLace;
	PROC_ZNGPUWRITEDATA		lpfnGPUwriteData;
	PROC_ZNGPUWRITESTATUS	lpfnGPUwriteStatus;
//	PROC_ZNGPUDMACHAIN		lpfnGPUdmaChainTest;
//	PROC_ZNGPUINIT			lpfnGPUtest;
	PROC_GPUCONFIGURE		lpfnGPUconfig;		// OpenGL2
} PSXGPULIB, *LPPSXGPULIB;

PSXGPULIB	_psxGPULib = { 0, };


//================================================================
// Function Implementation Start
//================================================================
int get_gpu_ini_option( FILE *fp, char *lpOptName, char *lpOptValue )
{
   char buf[512];
   int line = 0;

   lpOptName [0] = 0;
   lpOptValue[0] = 0;

   // 한줄씩 읽기
   while( fgets(buf, 512, fp) )
   {
      char *name, *arg = NULL;

      line ++;

      /* get option name */
      if(!(name = strtok(buf, " \t\r\n")))
         continue;
      if(name[0] == '#' || name[0] == 0)
         continue;

	  strcpy( lpOptName, name );

      /* get complete rest of line */
      arg = strtok(NULL, "\r\n");

      if (arg)
      {
		  /* ignore white space */
		  for (; (*arg == '\t' || *arg == ' '); arg++) {}

		  /* deal with quotations */
		  if (arg[0] == '"')
			 arg = strtok (arg, "\"");
		  else if (arg[0] == '\'')
			 arg = strtok (arg, "'");
		  else
			 arg = strtok (arg, " \t\r\n");

		  strcpy( lpOptValue, arg );
      }

	  return 1; // ok
   }

   return 0; //error
}

void init_extgpu_env( const char *szName )
{
	// 초기값 : INI파일이 없거나 안에 옵션값이 정의되지 않은 경우의 디폴트값
	bUseGPUPlugin   = 0;
	bMakeGameWindow = 0;
	sprintf( szGPUPlugin,	"plugins\\%s",		  "gpuPeopsSoft.dll"	);
	sprintf( szGameName,	"%s",				  szName				);
	szCfgName[0] = 0;

	// ---------------------------------------------------
	bUseGPUPlugin	= (options_get_bool(mame_options(), "use_gpu_plugin")) ? 1:0;
	bMakeGameWindow = (options_get_bool(mame_options(), "make_gpu_gamewin")) ? 1:0;
	if( options_get_string(mame_options(), "gpu_plugin_name") ) {
		sprintf( szGPUPlugin, "plugins\\%s", options_get_string(mame_options(), "gpu_plugin_name") );
	}
}

void load_gpu_ini_option( const char *lpszIniFile, const char *szName )
{
	if( _psxGPULib.bIsLoaded == FALSE ) return;
	Machine->gpu_plugin_loaded=1;
	if( _psxGPULib.lpfnGPUconfig != NULL ) {
		// GPU플러그인에서 자체적인 설정기능(GPUconfiguration함수)이 지원되는 경우에는
		// 별도의 외부 CFG파일을 설정할 필요가 없음
		szCfgName[0] = 0;
		return;
	}
	
	//===================================================
	sprintf( szCfgName,		"cfg\\renderer.cfg" );
	{
		FILE *fp;
		fp = fopen( szCfgName, "wt" );
		if( fp == NULL ) {
			MessageBox( NULL, "Unable to create psxgpu configuration file.", "ERROR", MB_OK );
		}
		else {
			int iScrX, iScrY;
			if (options_get_int(mame_options(), "gpu_screen_ctm"))
			{
			iScrX = options_get_int(mame_options(), "gpu_screen_x");
			iScrY = options_get_int(mame_options(), "gpu_screen_y");
			}
			else
			{
			switch(options_get_int(mame_options(), "gpu_screen_size")) {
			default:
			case 0:	iScrX = 640;  iScrY = 480; break;	//GPUSCR_640x480:
			case 1:	iScrX = 800;  iScrY = 600; break;	//GPUSCR_800x600:
			case 2:	iScrX = 1024; iScrY = 768; break;	//GPUSCR_1024x768:
			case 3:	iScrX = 1152; iScrY = 864; break;	//GPUSCR_1152x864:
							}
			}
			fprintf( fp, "; PSX GPU renderer settings.\n" );
			fprintf( fp, "; Automated by MAME\n\n" );

			fprintf( fp, "XSize = %d	; Window/fullscreen X size\n", iScrX );
			fprintf( fp, "YSize = %d	; Window/fullscreen Y size\n", iScrY );
			fprintf( fp, "FullScreen = %d    ; Fullscreen mode: 0/1\n", options_get_bool(mame_options(), "gpu_fullscreen") );
			fprintf( fp, "ColorDepth = %d   ; Fullscreen color depth: 16/32\n", (options_get_bool(mame_options(), "gpu_32bit") == 0) ? 16 : 32 );
			fprintf( fp, "ScanLines  = %d    ; Scanlines: 0=none, 1=black, 2=bright\n", options_get_int(mame_options(), "gpu_scanline") );
			fprintf( fp, "Filtering  = %d    ; Texture filtering: 0-3 (filtering causes glitches!)\n", options_get_int(mame_options(), "gpu_filtering") );
			fprintf( fp, "Blending   = %d    ; Enhanced color blend: ogl: 0/1; D3D: 0-2\n", options_get_int(mame_options(), "gpu_blending") );
			fprintf( fp, "Dithering  = %d    ; Dithering: 0/1 (only needed in 16 bit color depths)\n", options_get_bool(mame_options(), "gpu_dithering") );
			fprintf( fp, "ShowFPS    = %d    ; FPS display on startup: 0/1\n", options_get_bool(mame_options(), "gpu_showfps") );
			fprintf( fp, "FrameLimitation = %d    ; Frame limit: 0/1\n", options_get_bool(mame_options(), "gpu_frame_limit") );
			fprintf( fp, "FrameSkipping   = %d    ; Frame skip: 0/1\n", options_get_bool(mame_options(), "gpu_frame_skip") );
			fprintf( fp, "FramerateDetection = %d ; Auto framerate detection: 0/1\n", options_get_bool(mame_options(), "gpu_detection") );
			fprintf( fp, "FramerateManual = %d  ; Manual framerate: 0-1000\n", options_get_int(mame_options(), "gpu_frame_rate") );
			fprintf( fp, "TextureType     = %d    ; Textures: 0=card's default, 1=4 bit, 2=5bit, 3=8bit\n", options_get_int(mame_options(), "gpu_quality") );
			fprintf( fp, "TextureCaching  = %d    ; Caching type: 0-3, def=2, mode 3 is not available on most cards\n", options_get_int(mame_options(), "gpu_caching") );
			fprintf( fp, "EnableKeys      = 0    ; Enable renderer keys: 0/1, def=1 (enables keys for the fps menu/pause)\n" );
			fprintf( fp, "FastExcel       = 0    ; Speed hack for SF 'excel' modes. Will cause glitches if enabled!\n" );
				
			fclose(fp);
		}
	}
	//===================================================

	if( _access(szCfgName,0) == -1 ) {
		sprintf( szCfgName,	"cfg\\default.cfg"	);
		if( _access(szCfgName,0) == -1 ) {
			szCfgName[0] = 0;
		}
	}
}

void dispatch_cfg_per_game( const char *szname, GPUConfiguration_t *cfg )
{
	if( 
		!mame_stricmp(szname, "tekken") ||
		!mame_stricmp(szname, "tekkena") ||
		!mame_stricmp(szname, "tekkenb") ||
		!mame_stricmp(szname, "tekken2") ||
		!mame_stricmp(szname, "tekken2b") ||
		!mame_stricmp(szname, "cryptklr") 	// 2004.10.22 GPUFix [DarkCoder]
	) 
	{
		cfg->GPUVersion = 2;
	}

	iRomId		= 0;
	ihackLevel	= 0;
}

void LoadGpuLibrary( const char *lpszFileName, LPPSXGPULIB lpGpu )
{
	HMODULE hLib;

	lpGpu->bIsLoaded = FALSE;
	lpGpu->hLib		 = NULL;

	hLib = LoadLibrary( lpszFileName );
	if( hLib == NULL ) {
		logmsg( "Unable to load znc library '%s'...\n", lpszFileName );
		return;
	}

	lpGpu->lpfnGPUclose			= (PROC_ZNGPUCLOSE			)GetProcAddress( hLib, "ZN_GPUclose"			);
	lpGpu->lpfnGPUdmaChain		= (PROC_ZNGPUDMACHAIN		)GetProcAddress( hLib, "ZN_GPUdmaChain"			);
	lpGpu->lpfnGPUdmaSliceIn	= (PROC_ZNGPUDMASLICEIN		)GetProcAddress( hLib, "ZN_GPUdmaSliceIn"		);
	lpGpu->lpfnGPUdmaSliceOut	= (PROC_ZNGPUDMASLICEOUT	)GetProcAddress( hLib, "ZN_GPUdmaSliceOut"		);
	lpGpu->lpfnGPUfreeze		= (PROC_ZNGPUFREEZE			)GetProcAddress( hLib, "ZN_GPUfreeze"			);
	lpGpu->lpfnGPUgetMode		= (PROC_ZNGPUGETMODE		)GetProcAddress( hLib, "ZN_GPUgetMode"			);
	lpGpu->lpfnGPUinit			= (PROC_ZNGPUINIT			)GetProcAddress( hLib, "ZN_GPUinit"				);
	lpGpu->lpfnGPUmakeSnapshot	= (PROC_ZNGPUMAKESNAPSHOT	)GetProcAddress( hLib, "ZN_GPUmakeSnapshot"		);
	lpGpu->lpfnGPUopen			= (PROC_ZNGPUOPEN			)GetProcAddress( hLib, "ZN_GPUopen"				);
	lpGpu->lpfnGPUreadData		= (PROC_ZNGPUREADDATA		)GetProcAddress( hLib, "ZN_GPUreadData"			);
	lpGpu->lpfnGPUreadStatus	= (PROC_ZNGPUREADSTATUS		)GetProcAddress( hLib, "ZN_GPUreadStatus"		);
	lpGpu->lpfnGPUsetMode		= (PROC_ZNGPUSETMODE		)GetProcAddress( hLib, "ZN_GPUsetMode"			);
	lpGpu->lpfnGPUshutdown		= (PROC_ZNGPUSHUTDOWN		)GetProcAddress( hLib, "ZN_GPUshutdown"			);
	lpGpu->lpfnGPUupdateLace	= (PROC_ZNGPUUPDATELACE		)GetProcAddress( hLib, "ZN_GPUupdateLace"		);
	lpGpu->lpfnGPUwriteData		= (PROC_ZNGPUWRITEDATA		)GetProcAddress( hLib, "ZN_GPUwriteData"		);
	lpGpu->lpfnGPUwriteStatus	= (PROC_ZNGPUWRITESTATUS	)GetProcAddress( hLib, "ZN_GPUwriteStatus"		);
//	lpGpu->lpfnGPUdmaChainTest	= (PROC_ZNGPUDMACHAIN		)GetProcAddress( hLib, "ZN_GPUdmaChainTest"		);
//	lpGpu->lpfnGPUtest			= (PROC_ZNGPUINIT			)GetProcAddress( hLib, "GPUtest"				);

	lpGpu->lpfnGPUconfig		= (PROC_GPUCONFIGURE		)GetProcAddress( hLib, "GPUconfigure"			);

	if( !lpGpu->lpfnGPUclose		||	
		!lpGpu->lpfnGPUdmaChain		||
		!lpGpu->lpfnGPUdmaSliceIn	||
		!lpGpu->lpfnGPUdmaSliceOut	||
		!lpGpu->lpfnGPUfreeze		||
		!lpGpu->lpfnGPUgetMode		||
		!lpGpu->lpfnGPUinit			||
		!lpGpu->lpfnGPUmakeSnapshot	||
		!lpGpu->lpfnGPUopen			||
		!lpGpu->lpfnGPUreadData		||
		!lpGpu->lpfnGPUreadStatus	||
		!lpGpu->lpfnGPUsetMode		||
		!lpGpu->lpfnGPUshutdown		||
		!lpGpu->lpfnGPUupdateLace	||
		!lpGpu->lpfnGPUwriteData	||	
		!lpGpu->lpfnGPUwriteStatus	) 
	{
		logmsg( "Unable to find some procedure...\n" );
		FreeLibrary( hLib );
		return;
	}

//	if( (GetUserDefaultLangID() != 0x0412) && !lpGpu->lpfnGPUtest ) {
//		logmsg( "Unable to find some procedure...\n" );
//		FreeLibrary( hLib );
//		return;
//	}

	lpGpu->bIsLoaded = TRUE;
	lpGpu->hLib = hLib;
}


LRESULT CALLBACK psx_video_window_proc(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// handle a few messages
	switch (message)
	{
		// non-client paint: punt if full screen
		case WM_NCPAINT:
			return DefWindowProc(wnd, message, wparam, lparam);
			break;

		case WM_CLOSE:
			break;

		// destroy: close down the app
		//case WM_DESTROY:
		//	break;

		// everything else: defaults
		default:
			return DefWindowProc(wnd, message, wparam, lparam);
	}

	return 0;
}

HWND psx_create_gpu_video_window(int iFullScreenMode)
{
	if( bMakeGameWindow ) {
		TCHAR	title[256];
		HWND	hWnd;

		if (!classes_created)
		{
			WNDCLASS wc = { 0 };

			// initialize the description of the window class
			wc.lpszClassName 	= TEXT("MAME_PSX");
			wc.hInstance 		= GetModuleHandle(NULL);
			wc.lpfnWndProc		= psx_video_window_proc;
			wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
			wc.hIcon			= LoadIcon(NULL, IDI_APPLICATION);
			wc.lpszMenuName		= NULL;
			wc.hbrBackground	= NULL;
			wc.style			= 0;
			wc.cbClsExtra		= 0;
			wc.cbWndExtra		= 0;

			// register the class; fail if we can't
			if (!RegisterClass(&wc))
				return NULL;

			classes_created = 1;

		}

		// make the window title
		sprintf(title, "GMAME: %s [%s]", Machine->gamedrv->description, Machine->gamedrv->name);

		hWnd = CreateWindowEx( (iFullScreenMode)? PSX_FULLSCREEN_STYLE_EX:PSX_WINDOW_STYLE_EX,
							   TEXT("MAME_PSX"), 
							   title, 
							   (iFullScreenMode)? PSX_FULLSCREEN_STYLE:PSX_WINDOW_STYLE,
							   20, 20, 640, 480, 
							   NULL,	// hWndParent
							   NULL,	// hMenu
							   GetModuleHandle(NULL), NULL	);

		return hWnd;
	}

	return NULL;
}

extern void win_shutdown_joystick(void);
extern void win_init_joystick(HWND hWnd);

static int psx_extgpu_init(void)
{
	int		n_level;
	GPUConfiguration_t cfg;

	// Initialize
	init_extgpu_env( Machine->gamedrv->name );
	
	if( !bUseGPUPlugin ) {
		_psxGPULib.bIsLoaded = FALSE;
		return 0;
	}
	
	// Create window for psx game
	m_hPSXWnd = psx_create_gpu_video_window( 0 );

	memset( &_psxGPULib, 0, sizeof(PSXGPULIB) );

	//logmsg( "Loading '%s'...\n", szGPUPlugin );
	LoadGpuLibrary( szGPUPlugin, &_psxGPULib );
	if( _psxGPULib.bIsLoaded == FALSE ) {
		logmsg( "ERROR: Unable to load external gpu plugins. '%s'!\n", szGPUPlugin );
		return 1;
	}

	// Create cfg for psx game (on the fly)
	load_gpu_ini_option( "gpu_cfg.ini", Machine->gamedrv->name );

	// Build GPUConfiguration Block
	memset(&cfg, 0, sizeof(GPUConfiguration_t) );
	cfg.Version			= 1;					// Version of structure - currently 1
	if( m_hPSXWnd == NULL )
		cfg.hWnd		= (long)win_video_window;     // Window handle	
	else
		cfg.hWnd		= (long)m_hPSXWnd;

	// 0 = 0CW, 1 = 90CW, 2 = 180CW, 3 = 270CW = 90CCW
	if((Machine->gamedrv->flags & ROT90) == ROT90)			cfg.ScreenRotation	= 1;
	else if((Machine->gamedrv->flags & ROT180) == ROT180)	cfg.ScreenRotation	= 2;
	else if((Machine->gamedrv->flags & ROT270) == ROT270)	cfg.ScreenRotation	= 3;
	else
		cfg.ScreenRotation	= 0;

	cfg.GPUVersion = 0;		  // 0 = a, 1 = b, 2 = c
	
	cfg.GameName		= szGameName; // NULL terminated string
	cfg.CfgFile			= szCfgName;  // NULL terminated string
	if( szCfgName[0] == 0 ) cfg.CfgFile = NULL;

	dispatch_cfg_per_game( Machine->gamedrv->name, &cfg );

	/*
	{
		logmsg( "WINDOW SPEC--------------------\n" );
		logmsg( "Handle:<%08x> %s\n", win_video_window, IsWindow(win_video_window) ? "IS_WINDOW" : "NOT_WINDOW" );
		if( IsWindow(win_video_window) ) {
			RECT rc;
			GetWindowRect( win_video_window, &rc );
			logmsg( "size : %d, %d - %d, %d W:%d H:%d\n", rc.left, rc.top, rc.right, rc.bottom, rc.right-rc.left, rc.bottom-rc.top );
			logmsg( "Visible: %s\n", IsWindowVisible(win_video_window) ? "Yes" : "No" );
		}
		logmsg( "WINDOW SPEC--------------------\n" );
	}
	*/
	

	//logmsg( "Calling ZN_GPUinit()...\n" );
	if( _psxGPULib.lpfnGPUinit() != 0 ) {
		logmsg( "ZN_GPUInit() call error!\n" );
		return 1;
	}

	//logmsg( "Calling ZN_GPUopen() <HWND:%08x>...\n", win_video_window );
	win_shutdown_joystick();
	if( _psxGPULib.lpfnGPUopen( (void*)&cfg ) != 0 ) {
		logmsg( "ZN_GPUopen() call error!\n" );
		return 1;
	}
	win_init_joystick( (HWND)cfg.hWnd );
	
	// If we created another window for psx game, let's move main mame window
	// a little to see psx game window.
	if( m_hPSXWnd != NULL ) {
		extern void win_reload_joystick(HWND hWnd);
		long exStyle;

		exStyle = GetWindowLong(m_hPSXWnd, GWL_EXSTYLE);
		m_iPSXFullScreen = ( exStyle == WS_EX_TOPMOST ) ? 1 : 0;

		if( !m_iPSXFullScreen ) {
			RECT rc;
			GetWindowRect( m_hPSXWnd, &rc );

			rc.left -= 20;
			rc.top  -= 20;
			SetWindowPos( m_hPSXWnd, HWND_TOP, rc.left, rc.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER );
		}

		//win_reload_joystick(m_hPSXWnd);
	}

	igpuWriteCounter	= 0;
	igpuReadCounter		= 0;

	m_b_reverseflag		= 0;
	m_n_displaystartx	= 0;
	m_n_displaystarty	= 0;
	m_n_screenwidth		= 256;
	m_n_screenheight	= 240;
	m_n_vert_disstart	= 0x010;
	m_n_vert_disend		= 0x100;
	bIsInterlaced	    = 0;
	bDisplayEnabled		= 1;
	bVblankSignal		= 0;

	_dmaChainOffs	  = 0;
	_dmaChainLastAddr = 0;
	_dmaChainData	  = (unsigned long *)malloc( 16384 * sizeof(unsigned long));

	for( n_level = 0; n_level < 0x10000; n_level++ )
	{
		/* 24bit to 15 bit conversion */
		m_p_n_g0r0[ n_level ] = ( ( ( n_level >> 11 ) & ( MAX_LEVEL - 1 ) ) << 5 ) | ( ( ( n_level >> 3 ) & ( MAX_LEVEL - 1 ) ) << 0 );
		m_p_n_b0  [ n_level ] = ( ( n_level >> 3 ) & ( MAX_LEVEL - 1 ) ) << 10;
		m_p_n_r1  [ n_level ] = ( ( n_level >> 11 ) & ( MAX_LEVEL - 1 ) ) << 0;
		m_p_n_b1g1[ n_level ] = ( ( ( n_level >> 11 ) & ( MAX_LEVEL - 1 ) ) << 10 ) | ( ( ( n_level >> 3 ) & ( MAX_LEVEL - 1 ) ) << 5 );
	}

	return 0;
}

void video_stop_psx_extgpu(void)
{
	nUseExternalDraw = 0;

	if( _psxGPULib.bIsLoaded != FALSE ) {

		if( m_hPSXWnd != NULL ) 
			ShowWindow( m_hPSXWnd, SW_HIDE );
		else 
			ShowWindow( win_video_window, SW_HIDE );

		_psxGPULib.lpfnGPUclose();
		_psxGPULib.lpfnGPUshutdown();

		FreeLibrary( _psxGPULib.hLib );
	}
	memset( &_psxGPULib, 0, sizeof(PSXGPULIB) );

	if( m_hPSXWnd != NULL ) DestroyWindow( m_hPSXWnd );
	m_hPSXWnd = NULL;

	if( _dmaChainData ) free(_dmaChainData );
	_dmaChainData = NULL;
}

#define OVERSCAN_TOP ( 16 )

VIDEO_UPDATE( psx_extgpu )
{
	if( !nUseExternalDraw && (m_hPSXWnd!=NULL) ) {
		SetWindowPos( m_hPSXWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
	}

	nUseExternalDraw = 1;

	if(!bVblankSignal) return UPDATE_HAS_NOT_CHANGED;

	if( _psxGPULib.bIsLoaded != FALSE ) {
		// get data
		UINT32 gpuStatus;

		// D3D_RENDERER: GPUSTATUS_ODDLINES 플래그의 유지를 위해 두번 호출
		gpuStatus = _psxGPULib.lpfnGPUreadStatus();
		gpuStatus = _psxGPULib.lpfnGPUreadStatus();

		if( gpuStatus & GPUSTATUS_DISPLAYDISABLED ) {
			//_psxGPULib.lpfnGPUupdateLace();
		}
		else {
			_psxGPULib.lpfnGPUupdateLace();
		}
	}
	bVblankSignal = 0;

	//set_visible_area( 0, m_n_screenwidth - 1, 0, m_n_screenheight - 1 );

	/*
	UINT32 gpustatus;
	UINT32 n_x;
	UINT32 n_y;
	int n_top;
	int n_lines;
	static frmcnt = 0;

	if( _psxGPULib.bIsLoaded == FALSE ) return;

	_gpuPF.ulFreezeVersion = 1;	// should be always 1 for now (set by main emu)
	
	// get data
	_psxGPULib.lpfnGPUfreeze( 1, &_gpuPF );

	gpustatus = _gpuPF.ulStatus;

	set_visible_area( 0, m_n_screenwidth - 1, 0, m_n_screenheight - 1 );
	
	if( ( gpustatus & ( 1 << 0x17 ) ) != 0 )
	{
		// todo: only draw to necessary area
		fillbitmap( bitmap, 0, cliprect );
	}
	else
	{
		// todo: clear border
		if( m_b_reverseflag )
		{
			n_x = ( 1023 - m_n_displaystartx );
			// todo: make this flip the screen, in the meantime.. 
			n_x -= ( m_n_screenwidth - 1 );
		}
		else
		{
			n_x = m_n_displaystartx;
		}

		n_top = m_n_vert_disstart - OVERSCAN_TOP;
		if( n_top < 0 )
		{
			n_y = -n_top;
			n_top = 0;
			// todo: draw top border
		}
		else
		{
			n_y = 0;
		}
		n_lines = ( m_n_vert_disend - OVERSCAN_TOP ) - n_top;
		if( ( gpustatus & ( 1 << 0x16 ) ) != 0 )
		{
			// interlaced
			n_lines *= 2;
		}
		if( n_lines < m_n_screenheight - n_y )
		{
			// todo: draw bottom border
		}
		else
		{
			n_lines = m_n_screenheight - n_y;
		}

		if( ( gpustatus & ( 1 << 0x15 ) ) != 0 )
		{
			UINT32 offs;
			data16_t *p_n_src;
			data16_t *p_n_dest;

			// 24bit
			while( n_y < n_lines )
			{
				offs = 1024 * (n_y + m_n_displaystarty) * 2; // 2048: bytes per line

				p_n_src  = (data16_t *)&_gpuPF.psxVRam[ offs ];
				p_n_dest = &( (data16_t *)bitmap->line[ n_y + n_top ] )[ 0 ];

				for( n_x = 0; n_x < m_n_screenwidth / 2; n_x++ )
				{
					data32_t n_g0r0 = *( p_n_src++ );
					data32_t n_r1b0 = *( p_n_src++ );
					data32_t n_b1g1 = *( p_n_src++ );

					*( p_n_dest++ ) = m_p_n_g0r0[ n_g0r0 ] | m_p_n_b0[ n_r1b0 ];
					*( p_n_dest++ ) = m_p_n_r1[ n_r1b0 ] | m_p_n_b1g1[ n_b1g1 ];
				}
				n_y++;
			}
		}
		else
		{
			UINT32 offs;
			data16_t *p_n_src;

			offs = 1024 * (n_y + m_n_displaystarty) * 2; // 2048: bytes per line
			p_n_src  = (data16_t *)&_gpuPF.psxVRam[ offs ];

			// 15bit
			while( n_y < n_lines )
			{
				draw_scanline16( bitmap, 0, n_y + n_top, m_n_screenwidth, p_n_src + n_x, Machine->pens, -1 );
				n_y++;
			}
		}
	}
	*/
	return UPDATE_HAS_NOT_CHANGED;
}

int psx_extgpu_dmaChain_exist(void)
{
	return _dmaChainOffs;
}

void psx_extgpu_add_dmaChain( UINT32 *p_ram, INT32 size )
{
	unsigned long addrcnt = 0;

	if( (_dmaChainData==NULL) || (size > 0xff) || (_dmaChainOffs+size) >= 16384 ) return;

	addrcnt = _dmaChainData[ _dmaChainLastAddr ];
	addrcnt &= 0xff000000; // clear addr room
	addrcnt |= ((_dmaChainOffs * 4) & 0xffffff);
	_dmaChainData[ _dmaChainLastAddr ] = addrcnt;

	//logmsg( "psx_add_dmaChain: size:%d lastaddr:%08x(%d) (%08x)\n", size, _dmaChainData[ _dmaChainLastAddr ], _dmaChainLastAddr, addrcnt );
	
	_dmaChainData[ _dmaChainOffs ] = (size << 24) | 0xffffff; // add new end chain
	_dmaChainLastAddr = _dmaChainOffs;
	_dmaChainOffs++;

	// append data
	memcpy( &_dmaChainData[ _dmaChainOffs ], p_ram, sizeof(UINT32) * size );
	_dmaChainOffs += size;
}

// flush stacked dmaChain data if exist.
void psx_extgpu_send_dmaChain(void)
{
	if(_psxGPULib.bIsLoaded != FALSE) {
		//logmsg( "psx_send_dmaChain: _psxGPULib.lpfnGPUdmaChainTest = (%d)\n", _dmaChainOffs );
		//if( _psxGPULib.lpfnGPUdmaChainTest != NULL ) {
		//	_psxGPULib.lpfnGPUdmaChainTest(_dmaChainData, 0);	// Debug function
		//}
		if( _dmaChainOffs ) _psxGPULib.lpfnGPUdmaChain(_dmaChainData, 0);
	}
	
	_dmaChainOffs = 0;
	_dmaChainLastAddr = 0;
}

void psx_extgpu_write( UINT32 *p_ram, INT32 n_size )
{
	if( _psxGPULib.bIsLoaded == FALSE ) return;

	if( n_size > 0 ) {
		igpuWriteCounter++;

		if(n_size <= 10) psx_extgpu_add_dmaChain(p_ram, n_size);
		else {
			// flush stacked dmaChain data if exist.
			if(psx_extgpu_dmaChain_exist()) psx_extgpu_send_dmaChain();

			_psxGPULib.lpfnGPUdmaSliceIn( (unsigned long *)p_ram, 0, n_size );
		}
	}
}

void psx_extgpu_read( UINT32 *p_ram, INT32 n_size )
{
	if( _psxGPULib.bIsLoaded == FALSE ) return;

	if(psx_extgpu_dmaChain_exist()) psx_extgpu_send_dmaChain();

	if( n_size > 0 ) {
		igpuReadCounter++;
		_psxGPULib.lpfnGPUdmaSliceOut( (unsigned long *)p_ram, 0, n_size );
	}
}

WRITE32_HANDLER( psx_extgpu_w )
{
	if( _psxGPULib.bIsLoaded == FALSE ) return;

	// flush stacked dmaChain data if exist.
	if(psx_extgpu_dmaChain_exist()) psx_extgpu_send_dmaChain();

	switch( offset )
	{
	case 0x00:
		_psxGPULib.lpfnGPUwriteData( data );
		break;
	case 0x01:
		_psxGPULib.lpfnGPUwriteStatus(data);

		switch( data >> 24 )
		{
		case 0x00:	// reset
			m_n_displaystartx = 0;
			m_n_displaystarty = 0;
			m_n_vert_disstart = 0x010;
			m_n_vert_disend	  = 0x100;
			m_n_screenwidth   = 256;
			m_n_screenheight  = 240;
			bIsInterlaced	  = 0;
			bDisplayEnabled   = 1;
			bVblankSignal	  = 0;

			_dmaChainOffs	  = 0;
			_dmaChainLastAddr = 0;
			//psx_extgpu_write( &data, 1 );

			break;
		case 0x03:	// dis/enable display
			// if  data & 1 == 1, disabled
			bDisplayEnabled	= (data&1) ? 0:1;
			if( !(data & 1) ) { // enabling
				//UINT32 gpuStatus;
				//gpuStatus = _psxGPULib.lpfnGPUreadStatus();
				//while(bIsInterlaced && !(gpuStatus & 0x80000000)) gpuStatus = _psxGPULib.lpfnGPUreadStatus();
			}
		case 0x05:
			m_n_displaystartx = data & 1023;
			if( m_n_gputype == 2 )
				m_n_displaystarty = ( data >> 10 ) & 1023;
			else
				m_n_displaystarty = ( data >> 12 ) & 1023;
			//verboselog( 1, "start of display area %d %d\n", m_n_displaystartx, m_n_displaystarty );
			break;
		case 0x07:
			m_n_vert_disstart = data & 1023;
			m_n_vert_disend   = ( data >> 10 ) & 2047;
			//verboselog( 1, "vertical display range %d %d\n", m_n_vert_disstart, m_n_vert_disend );
			break;
		case 0x08:
			//verboselog( 1, "display mode %02x\n", data & 0xff );
			m_b_reverseflag = ( data >> 7 ) & 1;
			bIsInterlaced	= ( data >> 5 ) & 1;

			// Check Video Mode (PAL/NTSC)
			if( (data & 0x08) != 0 )
			{
				// pal
				set_refresh_rate( 0, 50 );
				switch( (data >> 2) & 1 )	// Check 'Height' Bits (Double or not)
				{
				case 0:	m_n_screenheight = 256;	break;
				case 1:	m_n_screenheight = 512;	break;
				}
			}
			else
			{
				// ntsc
				set_refresh_rate( 0, 60 );
				switch( (data >> 2) & 1 )	// Check 'Height' Bits (Double or not)
				{
				case 0:	m_n_screenheight = 240;	break;
				case 1:	m_n_screenheight = 480;	break;
				}
			}

			// Check 'Width 0' Bits (2bits)
			switch(data & 0x03)
			{
			case 0:
				switch((data >> 6) & 1)	// Check 'Width 1' bit
				{
				case 0:	m_n_screenwidth = 256;	break;
				case 1:	m_n_screenwidth = 368;	break;
				}
				break;
			case 1:
				switch((data >> 6) & 1)	// Check 'Width 1' bit
				{
				case 0:	m_n_screenwidth = 320;	break;
				case 1:	m_n_screenwidth = 384;	break;
				}
				break;
			case 2:	m_n_screenwidth = 512;	break;
			case 3:	m_n_screenwidth = 640;	break;
			}
			break;
		default:
			break;
		}

		break;
	default:
		//verboselog( 0, "gpu_w( %08x, %08x, %08x ) unknown register\n", offset, data, mem_mask );
		break;
	}
}

READ32_HANDLER( psx_extgpu_r )
{
	UINT32 data;

	if( _psxGPULib.bIsLoaded == FALSE ) return 0;
	
	// flush stacked dmaChain data if exist.
	if(psx_extgpu_dmaChain_exist()) psx_extgpu_send_dmaChain();

	switch( offset )
	{
	case 0x00:
		data = _psxGPULib.lpfnGPUreadData();
		break;
	case 0x01:
		// D3D_RENDERER: GPUSTATUS_ODDLINES 플래그의 유지를 위해 두번 호출
		data = _psxGPULib.lpfnGPUreadStatus();
		data = _psxGPULib.lpfnGPUreadStatus();
		break;
	default:
		//verboselog( 0, "gpu_r( %08x, %08x ) unknown register\n", offset, mem_mask );
		data = 0;
		break;
	}
	return data;
}

INTERRUPT_GEN( psx_extgpu_vblank )
{
	// D3D_RENDERER.ZNC --------------------------------------------
	//Exported fn(): ZN_GPUreadStatus - Ord:0009h
	//100080B0 A1309D3210              mov eax, dword ptr [10329D30]
	//100080B5 3500000080              xor eax, 80000000
	//100080BA A3309D3210              mov dword ptr [10329D30], eax
	//100080BF C3                      ret
	//*
	if( _psxGPULib.bIsLoaded != FALSE ) {
		if(psx_extgpu_dmaChain_exist()) psx_extgpu_send_dmaChain();
		_psxGPULib.lpfnGPUreadStatus();
	}
	bVblankSignal = 1;
	
	psx_irq_set( 0x0001 );
}

void psx_extgpu_reset( void )
{
	 psx_extgpu_w( 1, 0, 0 );
}

/*-------------------------------------------------
	save_screen_snapshot
-------------------------------------------------*/
void psx_extgpu_snapshot( void )
{
	if( _psxGPULib.bIsLoaded == FALSE ) return;
	_psxGPULib.lpfnGPUmakeSnapshot();
}
