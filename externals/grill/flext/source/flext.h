/* 

flext - C++ layer for Max/MSP and pd (pure data) externals

Copyright (c) 2001-2003 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

/*! \file flext.h 
    \brief This is the main flext include file.
    
    The basic definitions are set here and the necessary header files are included
*/

#ifndef __FLEXT_H
#define __FLEXT_H


/*!	\defgroup FLEXT_GLOBAL Flext global definitions
	@{
*/

//! \brief flext version number
#define FLEXT_VERSION 405

//! \brief flext version string
#define FLEXT_VERSTR "0.4.5"

//! @}


// determine System/OS/CPU
#include "flprefix.h"

// include headers necessary for multi-threading
#ifdef FLEXT_THREADS
	#if FLEXT_THREADS == FLEXT_THR_POSIX
		extern "C" {
			#include <pthread.h>
			#include <sched.h>
		}
	#elif FLEXT_THREADS == FLEXT_THR_MP
		#include <multiprocessing.h>
	#elif FLEXT_THREADS == FLEXT_THR_WIN32
        #define _WIN32_WINNT 0x500 // must be WIN2000 at least!
		#include <windows.h>
        #include <process.h>
	#else
		#error "Thread model not supported"
	#endif
#endif


#if FLEXT_SYS == FLEXT_SYS_MAX && FLEXT_OS == FLEXT_OS_WIN
// for wmax alignment must be 2 bytes!
#pragma pack(2)
#endif


// include all the flext interface definitions
#include "fldefs.h"

// include the basic flext object classes
#include "flclass.h"

// include the flext dsp class
#include "fldsp.h"

/*
#if FLEXT_SYS == FLEXT_SYS_MAX && FLEXT_OS == FLEXT_OS_WIN
#pragma pack(pop)
#endif
*/
#endif // FLEXT_H
