/***************************************************************************

	PSX SPU using external SPU Plugins by DarkCoder.

    function skeleton borrowed from smf's preliminary SPU source.
	Thanks to smf.

  ------------------------------------------------------------------------
  In fact, I don't know the psx things(GPU, SPU, etc...). So it may have 
  small/serious bugs. :) - DarkCoder

  Change Log
  ----------
	2004/06/10 - First draft
***************************************************************************/

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <io.h>

#include "driver.h"
#include "state.h"
#include "includes/psx.h"

#include "window.h"
#define win_video_window		win_window_list->hwnd


#define MAX_CHANNEL ( 24 )
#define SPU_RAM_SIZE ( 512 * 1024 )
#define SAMPLES_PER_BLOCK ( 28 )
#define PITCH_SHIFT ( 12 )

//================================================================
// function type definitions for external SPU Plugin interface
//================================================================
typedef long			(CALLBACK *PROC_SPUTEST					)(void);
typedef long			(CALLBACK *PROC_SPUCLOSE				)(void);
typedef long			(CALLBACK *PROC_SPUFREEZE				)(unsigned long ulFreezeMode,void * pF);
typedef long			(CALLBACK *PROC_SPUOPEN					)(HWND hW);
typedef long			(CALLBACK *PROC_SPUQSOUND				)(void (CALLBACK *callback)(unsigned char *,long *,long));
typedef unsigned short	(CALLBACK *PROC_SPUREADREGISTER			)(unsigned long reg);
typedef void			(CALLBACK *PROC_SPUREGISTERCALLBACK		)(void (CALLBACK *callback)(void));
typedef void			(CALLBACK *PROC_SPUWRITEREGISTER		)(unsigned long reg, unsigned short val);

typedef void (CALLBACK *PROC_CB_SPUQSOUND)(unsigned char *,long *,long);

//================================================================
// struct for external SPU library management
//================================================================
typedef struct _PSXSPULIB {
	BOOL	bIsLoaded;
	HMODULE hLib;

	// function pointer
	PROC_SPUCLOSE				lpfnSPUclose;
	PROC_SPUFREEZE				lpfnSPUfreeze;
	PROC_SPUOPEN				lpfnSPUopen;
	PROC_SPUQSOUND				lpfnSPUqsound;
	PROC_SPUREADREGISTER		lpfnSPUreadRegister;
//	PROC_SPUREGISTERCALLBACK	lpfnSPUregisterCallback;
	PROC_SPUWRITEREGISTER		lpfnSPUwriteRegister;
//	PROC_SPUTEST				lpfnSPUtest;
} PSXSPULIB, *LPPSXSPULIB; 

PSXSPULIB	_psxSPULib = { 0, };

//================================================================
// global/external variables & definition
//================================================================
extern HWND	m_hPSXWnd;

void logmsg(const char*lpszFmt,...);


//================================================================
// Function Implementation Start
//================================================================
void LoadSpuLibrary( const char *lpszFileName, LPPSXSPULIB lpSpu )
{
	HMODULE hLib;

	lpSpu->bIsLoaded = FALSE;
	lpSpu->hLib		 = NULL;

	hLib = LoadLibrary( lpszFileName );
	if( hLib == NULL ) {
		logmsg( "Unable to load external SPU plugin '%s'...\n", lpszFileName );
		return;
	}

	lpSpu->lpfnSPUclose				= (PROC_SPUCLOSE			)GetProcAddress( hLib, "ZN_SPUclose"			);
	lpSpu->lpfnSPUfreeze			= (PROC_SPUFREEZE			)GetProcAddress( hLib, "ZN_SPUfreeze"			);
	lpSpu->lpfnSPUopen				= (PROC_SPUOPEN				)GetProcAddress( hLib, "ZN_SPUopen"				);
	lpSpu->lpfnSPUqsound			= (PROC_SPUQSOUND			)GetProcAddress( hLib, "ZN_SPUqsound"			);
	lpSpu->lpfnSPUreadRegister		= (PROC_SPUREADREGISTER		)GetProcAddress( hLib, "ZN_SPUreadRegister"		);
	//lpSpu->lpfnSPUregisterCallback	= (PROC_SPUREGISTERCALLBACK	)GetProcAddress( hLib, "ZN_SPUregisterCallback"	);
	lpSpu->lpfnSPUwriteRegister		= (PROC_SPUWRITEREGISTER	)GetProcAddress( hLib, "ZN_SPUwriteRegister"	);
//	lpSpu->lpfnSPUtest				= (PROC_SPUTEST				)GetProcAddress( hLib, "SPUtest"				);

	if( !lpSpu->lpfnSPUclose			||	
		!lpSpu->lpfnSPUfreeze			||
		!lpSpu->lpfnSPUopen				||
		!lpSpu->lpfnSPUqsound			||
		!lpSpu->lpfnSPUreadRegister		||
		//!lpSpu->lpfnSPUregisterCallback	||
		!lpSpu->lpfnSPUwriteRegister ) 
	{
		logmsg( "ExtSPU:Unable to find some procedures...\n" );
		FreeLibrary( hLib );
		return;
	}

//	if( (GetUserDefaultLangID() != 0x0412) && !lpSpu->lpfnSPUtest ) {
//		logmsg( "ExtSPU:Unable to find some procedures...\n" );
//		FreeLibrary( hLib );
//		return;
//	}

	lpSpu->bIsLoaded = TRUE;
	lpSpu->hLib = hLib;
}

static const int fdata[ 5 ][ 2 ] =
{
	{   0,   0  },
	{  60,   0  },
	{ 115, -52  },
	{  98, -55  },
	{ 122, -60  }
};

/*
--------------------------------------------------------------
VAG 샘플 그룹 (Sony PlayStation compressed ADPCM sound format)

샘플 그룹은 16바이트단위로 이루어짐

typedef struct {
    unsigned char pack_info;
    unsigned char flags;
    unsigned char packed[14];
}

pack_info: 
 상위4비트:Predictor number 
 하위4비트:Shiftfactor 

flags: 
 0x07: 샘플 종료
 0x02: 반복 구간에 속하는 샘플
 0x06: 반복 시작
 0x04: 반복 위치(Repeat Point)
 0x03: 샘플 종료, 반복 위치부터 시작
--------------------------------------------------------------
*/

void CALLBACK psx_qsound_callback(unsigned char *pSpuBuffer,long *XAPlay, long length)
{
	struct psxinfo *chip = sndti_token(SOUND_PSXSPU, 0);
	int v;
	int voll;
	int volr;
	int n_channel;
	int n_sample;
	int n_word;
	int n_shift;
	int n_predict;
	int n_flags;
	int n_nibble;
	int n_packed;
	int n_unpacked;
	int s1, s2;
	UINT32 offs, blkaddr;
	UINT16* tbuffer;
    long*  pl=(long *)XAPlay;
    short* ps=(short *)pSpuBuffer;

	memset( pl, 0, length * sizeof(long)  );
	memset( ps, 0, length * sizeof(short) );

	for( n_channel = 0; n_channel < MAX_CHANNEL; n_channel++ )
	{
		voll = volume( chip->m_p_n_volumeleft[ n_channel ] );
		volr = volume( chip->m_p_n_volumeright[ n_channel ] );

		offs = 0;
		for( n_sample = 0; n_sample < length; n_sample++ )
		{
			if( chip->m_p_n_blockoffset[ n_channel ] >= ( SAMPLES_PER_BLOCK << PITCH_SHIFT ) )
			{
				if( !chip->m_p_n_blockstatus[ n_channel ] )
				{
					break;
				}
				if( ( chip->m_n_spucontrol & 0x40 ) != 0 &&
					( chip->m_n_irqaddress * 4 ) >= chip->m_p_n_blockaddress[ n_channel ] &&
					( chip->m_n_irqaddress * 4 ) <= chip->m_p_n_blockaddress[ n_channel ] + 7 )
				{
					chip->intf->irq_set( 0x0200 );
				}


				s2 = chip->m_p_n_s2[ n_channel ];
				s1 = chip->m_p_n_s1[ n_channel ];
				blkaddr = chip->m_p_n_blockaddress[ n_channel ];
				tbuffer = &chip->m_p_n_blockbuffer[ ( n_channel * SAMPLES_PER_BLOCK ) ];

				n_shift   = ( chip->m_p_n_spuram[ blkaddr ] >> 0 ) & 0x0f;
				n_predict = ( chip->m_p_n_spuram[ blkaddr ] >> 4 ) & 0x0f;
				n_flags   = ( chip->m_p_n_spuram[ blkaddr ] >> 8 ) & 0xff;

				// 정상적인 VAG블럭에서는 n_predict가 4보다 큰 경우가 절대 일어나지 않음 
				if( n_predict > 4 ) n_predict = 0;

				//if( n_flags == 0x04 )
				if( ( n_flags & 4 ) != 0 )
				{
					// 0x04: 반복 위치(Repeat Point)
					chip->m_n_loop[ n_channel ] = blkaddr;
					//m_p_n_s2[ n_channel ] = m_p_n_s1[ n_channel ] = 0;
				}

				blkaddr++;
				if(blkaddr == (SPU_RAM_SIZE / 2)) blkaddr = 0;

				// Decode VAG sound data
				for( n_word = 0; n_word < 7; n_word++ )
				{
					n_packed = chip->m_p_n_spuram[ blkaddr ];
					blkaddr++;
					if(blkaddr == (SPU_RAM_SIZE / 2)) blkaddr = 0;

					for( n_nibble = 0; n_nibble < 4; n_nibble++ )
					{
						n_unpacked = ( ( n_packed & 0xf ) << 12 );
						if( ( n_unpacked & 0x8000 ) != 0 )
						{
							n_unpacked |= 0xffff0000;
						}
						n_unpacked = ( n_unpacked >> n_shift );
						
						n_unpacked += ((s1*fdata[n_predict][0])>>6) + ((s2*fdata[n_predict][1])>>6);
						s2 = s1; s1 = n_unpacked;

						*tbuffer = n_unpacked;
						tbuffer++;

						n_packed >>= 4;
					}
				}

				chip->m_p_n_s2[ n_channel ] = s2;
				chip->m_p_n_s1[ n_channel ] = s1;
				chip->m_p_n_blockaddress[ n_channel ] = blkaddr;

				// check flags 0x07 or 0x03
				/*
				if( n_flags == 0x07 )	
				{
					// 0x07: end of samples
					m_p_n_blockstatus[ n_channel ] = 0;
					//m_p_n_s2[ n_channel ] = m_p_n_s1[ n_channel ] = 0;
				}
				else if( n_flags == 0x03 )	
				{
					// 0x03: 반복 위치를 다음 시작점으로 세트
					m_p_n_blockaddress[ n_channel ] = m_n_loop[ n_channel ];
				}
				*/

				if( ( n_flags & 1 ) != 0 )	
				{
					if( n_flags != 3 )
					{
						// 0x07: end of samples
						chip->m_p_n_blockstatus[ n_channel ] = 0;
						//m_p_n_s2[ n_channel ] = m_p_n_s1[ n_channel ] = 0;
					}
					else
					{
						// 0x03: 반복 위치를 다음 시작점으로 세트
						chip->m_p_n_blockaddress[ n_channel ] = chip->m_n_loop[ n_channel ];
					}
				}
				
				chip->m_p_n_blockoffset[ n_channel ] %= ( SAMPLES_PER_BLOCK << PITCH_SHIFT );
			}

			v = chip->m_p_n_blockbuffer[ ( n_channel * SAMPLES_PER_BLOCK ) + ( chip->m_p_n_blockoffset[ n_channel ] >> PITCH_SHIFT ) ];
			chip->m_p_n_blockoffset[ n_channel ] += chip->m_p_n_pitch[ n_channel ];

			pl[offs] += (( v * voll ) / 0x4000); 
			if( pl[offs] < -32767 ) pl[offs] = -32767;
			if( pl[offs] >  32767 ) pl[offs] =  32767;
			ps[offs] = (short)pl[offs]; offs++;

			pl[offs] += (( v * volr ) / 0x4000);
			if( pl[offs] < -32767 ) pl[offs] = -32767;
			if( pl[offs] >  32767 ) pl[offs] =  32767;
			ps[offs] = (short)pl[offs]; offs++;
		}
	}
	//memset( pl, 0, length * sizeof(long)  );

}


static void psx_extspu_read( UINT32 n_address, INT32 n_size )
{
	struct psxinfo *chip = sndti_token(SOUND_PSXSPU, 0);
	//verboselog( 1, "spu_read( %08x, %08x )\n", n_address, n_size );
	while( n_size > 0 )
	{
		chip->g_p_n_psxram[ n_address / 4 ] = 
			( chip->m_p_n_spuram[ chip->m_n_spuoffset + 0 ] << 0 ) |
			( chip->m_p_n_spuram[ chip->m_n_spuoffset + 1 ] << 16 );
		//verboselog( 2, "%08x > %04x\n", m_n_spuoffset + 0, m_p_n_spuram[ m_n_spuoffset + 0 ] );
		//verboselog( 2, "%08x > %04x\n", m_n_spuoffset + 1, m_p_n_spuram[ m_n_spuoffset + 1 ] );
		chip->m_n_spuoffset += 2;
		chip->m_n_spuoffset %= ( SPU_RAM_SIZE / 2 );
		n_address += 4;
		n_size--;
	}
}

static void psx_extspu_write( UINT32 n_address, INT32 n_size )
{
	struct psxinfo *chip = sndti_token(SOUND_PSXSPU, 0);
	//verboselog( 1, "spu_write( %08x, %08x )\n", n_address, n_size );
	while( n_size > 0 )
	{
		chip->m_p_n_spuram[ chip->m_n_spuoffset + 0 ] = ( chip->g_p_n_psxram[ n_address / 4 ] >> 0 );
		chip->m_p_n_spuram[ chip->m_n_spuoffset + 1 ] = ( chip->g_p_n_psxram[ n_address / 4 ] >> 16 );
		//verboselog( 2, "%08x < %04x\n", m_n_spuoffset + 0, m_p_n_spuram[ m_n_spuoffset + 0 ] );
		//verboselog( 2, "%08x < %04x\n", m_n_spuoffset + 1, m_p_n_spuram[ m_n_spuoffset + 1 ] );
		chip->m_n_spuoffset += 2;
		chip->m_n_spuoffset %= ( SPU_RAM_SIZE / 2 );
		n_address += 4;
		n_size--;
	}
}

int psx_extspu_start(int sndindex, int clock, const void *config)
{
	HWND hWnd;
	char szPlugin[256];

	/*
	if(_psxSPULib.bIsLoaded && _psxSPULib.hLib) {
		_psxSPULib.lpfnSPUclose();
		FreeLibrary( _psxSPULib.hLib );
	}
	*/
	
	_psxSPULib.hLib = NULL;
	_psxSPULib.bIsLoaded = FALSE;

	if( !options_get_bool(mame_options(), "use_spu_plugin") || !options_get_string(mame_options(), "spu_plugin_name") ) return 0;

	sprintf( szPlugin, "plugins\\%s", options_get_string(mame_options(), "spu_plugin_name") );
	//logmsg( "[psx_extspu_PSX_sh_start] Loading External SPU Plugins '%s'...\n", szPlugin );
	LoadSpuLibrary( szPlugin, &_psxSPULib );
	if( _psxSPULib.bIsLoaded == FALSE ) return 1;

	hWnd = (m_hPSXWnd) ? m_hPSXWnd : win_video_window;
	_psxSPULib.lpfnSPUopen( hWnd );
	_psxSPULib.lpfnSPUqsound( psx_qsound_callback );

	return 0;
}

void psx_extspu_stop( void )
{
	if( _psxSPULib.bIsLoaded != FALSE ) {
		_psxSPULib.lpfnSPUclose();

		FreeLibrary( _psxSPULib.hLib );
	}

	_psxSPULib.hLib = NULL;
	_psxSPULib.bIsLoaded = FALSE;
}

READ32_HANDLER( psx_extspu_delay_r )
{
	//verboselog( 1, "psx_spu_delay_r()\n" );
	return 0;
}

READ32_HANDLER( psx_extspu_r )
{
	int iRet = 0;

	if( _psxSPULib.bIsLoaded != FALSE ) {
		UINT32 reg;
		reg = (offset * 4) + 0x1f801c00;
		if( (mem_mask & 0xffff) == 0xffff) {
			reg += 2;
		}
		iRet = _psxSPULib.lpfnSPUreadRegister(reg);
		//logmsg("    ZN_SPUreadRegister(%08x, %08x) - iRet:%08x\n", reg, mem_mask, iRet );
	}

	return iRet;
}

WRITE32_HANDLER( psx_extspu_w )
{
	if( _psxSPULib.bIsLoaded != FALSE ) {
		UINT32 reg;
		reg = (offset * 4) + 0x1f801c00;


		if( (mem_mask & 0xffff) == 0xffff) {
			data >>= 16;
			reg += 2;
		}

		//logmsg("ZN_SPUwriteRegister(%08x, %08x, %08x)\n", reg, data, mem_mask );

		_psxSPULib.lpfnSPUwriteRegister(reg, data);
	}
}
