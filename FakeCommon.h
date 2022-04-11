#pragma once

#include "main.h"

#ifdef _DEBUG
#ifdef CHECKEDBUILD
#define DEBUGMESS(a) { OutputDebugStringA(a); }
#else
#define DEBUGMESS(a) OutputDebugStringA(a)
#endif
#else
#ifdef CHECKEDBUILD
#define DEBUGMESS(a) { Log(a); }
#else
#define DEBUGMESS(a)
#endif
#endif

#define UNUSEDFUNCTION { DEBUGMESS("Unused function called: " __FUNCTION__ "\r\n"); return DDERR_GENERIC; }
#define SAFERELEASE(a) { if(a) { a->Release(); a=0; } }

#define COMMONRELEASE \
	if(--Refs==0) { \
		Real->Release(); \
		delete this; \
		return 0; \
	} else return Refs;

#ifdef CHECKEDBUILD
#define CHECKFUNC(a) { HRESULT hr=a; CHECKFAIL(hr); return hr; }
#define CHECKFAIL(a) { if(FAILED(a)) { LOG("HRESULT=%x (FAIL !!!)", a); } else { LOG("HRESULT=%x", a); } }
#else
#define CHECKFUNC(a) { return a; }
#define CHECKFAIL(a)
#endif

extern IDirectDraw4* rDDraw;