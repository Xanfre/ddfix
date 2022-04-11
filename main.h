#pragma once

//#define CHECKEDBUILD

#define D3D_OVERLOADS
#define STRICT
#define WIN32_LEAN_AND_MEAN
#define INITGUID
#define DIRECTDRAW_VERSION 0x0600
#define DIRECT3D_VERSION   0x0600
#define WINVER 0x0500
#define _WIN32_WINNT 0x500
#define NTDDI_VERSION NTDDI_WIN2K

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdio.h>

extern DWORD SafeMode;
extern DWORD FUMode;
extern DWORD scene;

extern DWORD gWidth;
extern DWORD gHeight;
extern float FUxmul;
extern float FUymul;

extern DWORD ZBufferBitDepth;
extern DWORD UseSysMemOverlay;
extern DWORD OverlayColourKey;
extern DWORD LayeredOverlay;
extern DWORD TextureReplacement;
extern DWORD AnisotropicFiltering;
extern DWORD RefreshRate;
extern char TexturePath[MAX_PATH];

extern DWORD TripleBuffer;
extern DWORD FlipVSync;
extern DWORD FlipInterval;
extern float FrameRateLimit;
extern DWORD MenuUpdateDelay;
extern float MenuScreenAspect;
extern DWORD EnableMissionIni;
extern DWORD EnableGamma;
extern DWORD FovModification;
extern DWORD ItemSize;
extern DWORD LimbZBufferFix;
extern DWORD WaterAlphaFix;
extern DWORD ModulateShift;

extern LPDIRECT3DDEVICE3 realIDirect3DDevice;

struct renderinfo_t {
	IDirect3DTexture2 *suntex;
	D3DVECTOR pos;
	float size;
	RECT olr;
};
extern renderinfo_t renderinfo;

struct PPParams {
	DWORD Enable;
	DWORD Base;
	DWORD Saturation;
	DWORD Bloom;
	DWORD BloomUI;
	DWORD BloomPersistence;
	DWORD BloomSaturation;
	DWORD BloomSpread;
	DWORD BloomThreshold;
	DWORD BloomTrail;
	DWORD BloomShift;
	DWORD BloomShiftRemodulate;
	DWORD DynamicThreshold;
	float DynamicGamma;
	float DynamicDecay;
	float DynamicMult;
	float DynamicAdd;
} extern pp;

struct FogParams {
	DWORD Enable;
	DWORD Global;
	DWORD Detected;
} extern fogp;

struct MemAddrParams {
	DWORD CopyTex;
	DWORD MipMap;
	DWORD CreateVidTexture;
	DWORD TextureExists;
	DWORD CreateFile;
	DWORD SS2CreateFile;
	DWORD BltPrimary;
	DWORD Resolution;
	DWORD VertResLimit;
	DWORD FovModification;
	DWORD AspectCorrection;
	DWORD ItemSize;
	DWORD OrbAspectCorrection;
	DWORD MaxSkyIntensity;
} extern memaddrp;

void readIni(const char *fname, bool defaults=true);
void WidescreenSetMenuMode(bool MenuMode);
void WidescreenSetup();

static inline __int64 getFreq() {
	static __int64 freq=0;
	if(!freq) QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
	return freq;
}

static inline __int64 getCount() {
	__int64 count;
	QueryPerformanceCounter((LARGE_INTEGER *)&count);
	return count;
}

#ifdef _DEBUG
#define LOG(format, ...) \
	do { \
	char str[256]; \
	_snprintf(str, 256, "%s(%d) %s: " format "\n", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); \
	OutputDebugStringA(str); \
	} while(0)
#else
#define LOG(format, ...)
#endif

#if 1
extern FILE *logfp;
#define LOGF(format, ...) \
	do { \
	if(!logfp) logfp=fopen("ddfix.log", "a+"); \
	fprintf(logfp, "%s(%d) %s: " format "\n", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); \
	fflush(logfp); \
	} while(0)
#else
#define LOGF(format, ...)
#endif
