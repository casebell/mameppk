/******************************************************************************

    mamedriv.c

    Copyright (c) 1996-2006, Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

    The list of all available drivers. Drivers have to be included here to be
    recognized by the executable.

    To save some typing, we use a hack here. This file is recursively #included
    twice, with different definitions of the DRIVER() macro. The first one
    declares external references to the drivers; the second one builds an array
    storing all the drivers.

******************************************************************************/

#include "driver.h"

#ifndef DRIVER_RECURSIVE

#define DRIVER_RECURSIVE

/* step 1: declare all external references */
#define DRIVER(NAME) extern game_driver driver_##NAME;
#include "mameplusdriv.c"

/* step 2: define the drivers[] array */
#undef DRIVER
#define DRIVER(NAME) &driver_##NAME,
const game_driver * const plusdrivers[] =
{
#include "mameplusdriv.c"
	0	/* end of array */
};

#else	/* DRIVER_RECURSIVE */

	DRIVER( knightsh )	/* hack */
	DRIVER( knightsb )	/* bootleg */
	DRIVER( sf2m8 )		/* hack */
	DRIVER( sf2m13 )	/* hack */
	DRIVER( sf2tlona )	/* hack, Tu Long set 1 */
	DRIVER( sf2tlonb )	/* hack, Tu Long set 2 */
	DRIVER( wofb )		/* bootleg */
	DRIVER( wofsj )		/* 1995  Holy Sword Three Kingdoms / Sheng Jian San Guo */
	DRIVER( wofsja )	/* 1995  Holy Sword Three Kingdoms / Sheng Jian San Guo */
	DRIVER( wofsjb )	/* 1995  Holy Sword Three Kingdoms / Sheng Jian San Guo */
	DRIVER( wof3js )	/* 1997  Three Sword Masters / San Jian Sheng */
	DRIVER( wof3sj )	/* 1997  Three Holy Swords / San Sheng Jian */
	DRIVER( wof3sja )	/* 1997  Three Holy Swords / San Sheng Jian */
	DRIVER( wofh ) 		/* 1999  Legend of Three Kingdoms' Heroes / Sanguo Yingxiong Zhuan */
	DRIVER( wofha ) 	/* 1999  Legend of Three Kingdoms' Heroes / Sanguo Yingxiong Zhuan */
	DRIVER( dinob )		/* bootleg */
	DRIVER( dinoh )		/* hack */
	DRIVER( dinoha )	/* hack */
	DRIVER( dinohb )	/* hack */
	DRIVER( sfzch )		/* 1995  Street Fighter Zero (CPS Changer) */
	DRIVER( sfach )		/* 1995  Street Fighter Alpha (Publicity CPS Changer) */

	DRIVER( ddtodd )	/* 12/04/1994 (c) 1993 (Euro) Phoenix Edition */
	DRIVER( avspd )		/* 20/05/1994 (c) 1994 (Euro) Phoenix Edition */
	DRIVER( ringdstd )	/* 02/09/1994 (c) 1994 (Euro) Phoenix Edition */
	DRIVER( xmcotad )	/* 05/01/1995 (c) 1994 (Euro) Phoenix Edition */
	DRIVER( nwarrud )	/* 06/04/1995 (c) 1995 (US) Phoenix Edition */
	DRIVER( sfad )		/* 27/07/1995 (c) 1995 (Euro) Phoenix Edition */
	DRIVER( 19xxd )		/* 07/12/1995 (c) 1996 (US) Phoenix Edition */
	DRIVER( ddsomud )	/* 19/06/1996 (c) 1996 (US) Phoenix Edition */
	DRIVER( spf2xjd )	/* 31/05/1996 (c) 1996 (Japan) Phoenix Edition */
	DRIVER( megamn2d )	/* 08/07/1996 (c) 1996 (US) Phoenix Edition */
	DRIVER( sfz2aad )	/* 26/08/1996 (c) 1996 (Asia) Phoenix Edition */
	DRIVER( xmvsfu1d )	/* 04/10/1996 (c) 1996 (US) Phoenix Edition */
	DRIVER( batcird )	/* 19/03/1997 (c) 1997 (Euro) Phoenix Edition */
	DRIVER( vsavd )		/* 19/05/1997 (c) 1997 (Euro) Phoenix Edition */
	DRIVER( mvscud )	/* 23/01/1998 (c) 1998 (US) Phoenix Edition */
	DRIVER( sfa3ud )	/* 04/09/1998 (c) 1998 (US) Phoenix Edition */
	DRIVER( gwingjd )	/* 23/02/1999 (c) 1999 Takumi (Japan) Phoenix Edition */
	DRIVER( 1944d )		/* 20/06/2000 (c) 2000 Eighting/Raizing (US) Phoenix Edition */
	DRIVER( hsf2d )	/* 02/02/2004 (c) 2004 (Asia) Phoenix Edition */
	DRIVER( dstlku1d )	/* 05/07/1994 (c) 1994 (Phoenix Edition, US 940705) */

	DRIVER( kof96ep )	/* 0214 bootleg */
	DRIVER( kof97pla )	/* 0232 (c) 2003 bootleg */
	DRIVER( kf2k1pls )	/* 0262 (c) 2001 bootleg */
	DRIVER( kf2k1pa )	/* 0262 (c) 2001 bootleg */
	DRIVER( cthd2k3a )	/* bootleg of kof2001*/
	DRIVER( kof2002b )	/* 0265 (c) 2002 bootleg */
	DRIVER( kf2k2plb )	/* bootleg */
	DRIVER( kf2k2plc )	/* bootleg */
	DRIVER( kf2k4pls )	/* bootleg of kof2002 */
	DRIVER( matrimbl )	/* 0266 (c) 2002 bootleg */
	DRIVER( mslug5b )

	/* CD to MVS Conversion */
	DRIVER( zintrkcd )	/* 0211 hack - CD to MVS Conversion by Razoola */
	DRIVER( fr2ch )

#ifdef MAME32PLUSPLUS
#ifndef NEOCPSMAME
	/* Psikyo games */
	DRIVER( tgm2 )		/* (c) 2000 */
	DRIVER( tgm2p )		/* (c) 2000 */

	/* Konami "Nemesis hardware" games */
	DRIVER( spclone )	/* GX587 (c) 1986 */
#endif /* NEOCPSMAME */

	/* Capcom CPS2 games */
	DRIVER( hsf2nb )			/* 02/02/2004 (c) 2004 (Asia) Phoenix Edition */

	/* Capcom CPS3 games */
	DRIVER( jojobap )	/* 13/09/1999 (c) 1999 Capcom */

	/* Neo Geo games */
	DRIVER( ssh5spnd )	/* 0272 (c) 2004 Yuki Enterprise / SNK Playmore */
#endif /* MAME32PLUSPLUS */

#ifdef KAILLERA
	/* Capcom CPS1 games */
	DRIVER( captcomm3p )	/* 14/10/1991 (c) 1991 (World) */
	DRIVER( captcomu3p )	/* 28/ 9/1991 (c) 1991 (US)    */
	DRIVER( captcomj3p )	/* 02/12/1991 (c) 1991 (Japan) */
	DRIVER( captcomm4p )	/* 14/10/1991 (c) 1991 (World) */
	DRIVER( captcomu4p )	/* 28/ 9/1991 (c) 1991 (US)    */
	DRIVER( captcomj4p )	/* 02/12/1991 (c) 1991 (Japan) */

	/* Capcom CPS2 games */
	DRIVER( avsp3p )		/* 20/05/1994 (c) 1994 (Euro) */
	DRIVER( avspu3p )		/* 20/05/1994 (c) 1994 (US) */
	DRIVER( avspj3p )		/* 20/05/1994 (c) 1994 (Japan) */
	DRIVER( avspa3p )		/* 20/05/1994 (c) 1994 (Asia) */
	DRIVER( batcir4p )		/* 19/03/1997 (c) 1997 (Euro) */
	DRIVER( batcirj4p )		/* 19/03/1997 (c) 1997 (Japan) */
	DRIVER( ddsom4p )		/* 19/06/1996 (c) 1996 (Euro) */
	DRIVER( ddsomr2_4p )	/* 09/02/1996 (c) 1996 (Euro) */
	DRIVER( ddsomu4p )		/* 19/06/1996 (c) 1996 (US) */
	DRIVER( ddsomur1_4p )	/* 09/02/1996 (c) 1996 (US) */
	DRIVER( ddsomjr1_4p )	/* 06/02/1996 (c) 1996 (Japan) */
	DRIVER( ddsomj4p )		/* 19/06/1996 (c) 1996 (Japan) */
	DRIVER( ddsoma4p )		/* 19/06/1996 (c) 1996 (Asia) */
	DRIVER( sfa3p )			/* 04/09/1998 (c) 1998 (US) */
	DRIVER( sfa3up )		/* 04/09/1998 (c) 1998 (US) */
	DRIVER( sfa3ur1p )		/* 29/06/1998 (c) 1998 (US) */
	DRIVER( sfz3jp )		/* 04/09/1998 (c) 1998 (Japan) */
	DRIVER( sfz3jr1p )		/* 27/07/1998 (c) 1998 (Japan) */
	DRIVER( sfz3jr2p )		/* 29/06/1998 (c) 1998 (Japan) */
	DRIVER( sfz3ar1p )		/* 01/07/1998 (c) 1998 (Asia) */
#ifdef MAME32PLUSPLUS
	DRIVER( xmvsfregion4p )	/* 23/10/1996 (c) 1996 (US) */
#endif /* MAME32PLUSPLUS */
	DRIVER( mshvsfj4p )		/* 07/07/1997 (c) 1997 (Japan) */
	DRIVER( mvscj4p )		/* 23/01/1998 (c) 1998 (Japan) */

	/* Neo Geo games */
	DRIVER( lbowling4p )	/* 0019 (c) 1990 SNK */
	DRIVER( kof95_6p )		/* 0084 (c) 1995 SNK */
	DRIVER( kof98_6p )		/* 0242 (c) 1998 SNK */

#ifndef NEOCPSMAME
	/* Video System Co. games */
	DRIVER( fromanc2_k )	/* (c) 1995 Video System Co. (Japan) */
	DRIVER( fromancr_k )	/* (c) 1995 Video System Co. (Japan) */
	DRIVER( fromanc4_k )	/* (c) 1998 Video System Co. (Japan) */

	/* Namco System 86 games */
	DRIVER( roishtar2p )	/* (c) 1986 */

	/* Psikyo games */
	DRIVER( hotgmck_k )		/* (c) 1997 */
	DRIVER( hgkairak_k )	/* (c) 1998 */
	DRIVER( hotgmck3_k )	/* (c) 1999 */
	DRIVER( hotgm4ev_k )	/* (c) 2000 */
	DRIVER( loderndf_k )	/* (c) 2000 */
	DRIVER( loderndf_vs )	/* (c) 2000 */
	DRIVER( hotdebut_k )	/* (c) 2000 */

	/* Konami games */
	DRIVER( hyprolym4p )	/* GX361 (c) 1983 */
	DRIVER( hyperspt4p )	/* GX330 (c) 1984 + Centuri */
	DRIVER( hpolym84_4p )	/* GX330 (c) 1984 */

	/* Sega System 32 games */
	DRIVER( ga2j4p )		/* (c) 1992 (Japan) */

	/* Taito F3 games */
	DRIVER( arabianm4p )	/* D29 (c) 1992 Taito Corporation Japan (World) */
	DRIVER( arabiamj4p )	/* D29 (c) 1992 Taito Corporation (Japan) */
	DRIVER( arabiamu4p )	/* D29 (c) 1992 Taito America Corporation (US) */
	DRIVER( dungeonm4p )	/* D69 (c) 1993 Taito Corporation Japan (World) */
	DRIVER( lightbr4p )		/* D69 (c) 1993 Taito Corporation (Japan) */
	DRIVER( dungenmu4p )	/* D69 (c) 1993 Taito America Corporation (US) */
#endif /* NEOCPSMAME */
#endif /* KAILLERA */

#endif	/* DRIVER_RECURSIVE */
