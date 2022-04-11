#include "main.h"

#include <shlobj.h>

#include "SafeWrite.h"
#include "FakeDirectDraw.h"
#include "BitmapSaver.h"
#include "TexOverride.h"

//TODO: windowed mode
//TODO: non-exclusive fullscreen mode

DWORD SafeMode;
DWORD FUMode;

DWORD gWidth;
DWORD gHeight;
float FUxmul;
float FUymul;
DWORD ZBufferBitDepth;
DWORD UseSysMemOverlay;
DWORD OverlayColourKey;
DWORD LayeredOverlay;
DWORD TextureReplacement;
DWORD AnisotropicFiltering;
DWORD RefreshRate;
char TexturePath[MAX_PATH];

DWORD TripleBuffer;
DWORD FlipVSync;
DWORD FlipInterval;
float FrameRateLimit;
DWORD MenuUpdateDelay;
float MenuScreenAspect;
DWORD EnableMissionIni;
DWORD EnableGamma;
DWORD FovModification;
DWORD ItemSize;
DWORD LimbZBufferFix;
DWORD WaterAlphaFix;
DWORD ModulateShift;

struct PPParams pp;
struct FogParams fogp={0};
struct MemAddrParams memaddrp;

static FakeDirectDraw* SavedDirectDraw;

static HHOOK g_hKeyboardHook;

FILE *logfp=NULL;

#ifdef CHECKEDBUILD
static HANDLE LogFile;
#endif

//static HANDLE hfile=0;
//static int ccount=0;

static LRESULT CALLBACK LowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    if (nCode < 0 || nCode != HC_ACTION )  // do not process message 
        return CallNextHookEx( g_hKeyboardHook, nCode, wParam, lParam); 
 
    bool bEatKeystroke = false;
    KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam; 
    switch (wParam) 
    {
		case WM_KEYDOWN:
        case WM_KEYUP:    
        {
            bEatKeystroke = (((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN)));
            break;
        }
    }
 
    if( bEatKeystroke )
        return 1;
    else
        return CallNextHookEx( g_hKeyboardHook, nCode, wParam, lParam );
}

static DWORD BloomUIDef=1;

void readIni(const char *fname, bool defaults)
{
	if(defaults) {
		fogp.Enable=1;
		fogp.Global=1;
		pp.Enable=0;
		pp.Base=0xffffff;
		pp.Saturation=0xc0;
		pp.Bloom=0xff;
		pp.BloomUI=BloomUIDef;
		pp.BloomPersistence=0x404040;
		pp.BloomSaturation=0xc0;
		pp.BloomSpread=0;
		pp.BloomThreshold=0x202020;
		pp.BloomTrail=0x80ffffff;
		pp.BloomShift=0;
		pp.BloomShiftRemodulate=0;
		pp.DynamicThreshold=0;
		pp.DynamicGamma=0;
		pp.DynamicDecay=.5f;
		pp.DynamicMult=1.5f;
		pp.DynamicAdd=8.0f;
		if(FUMode) {
			pp.BloomSaturation=128;
			pp.BloomTrail=0;
			pp.DynamicThreshold=1;
			pp.DynamicDecay=.2;
			pp.DynamicMult=3;
			pp.DynamicAdd=16;
		}
	}

	fogp.Enable=GetPrivateProfileInt("Fog", "Enable", fogp.Enable, fname);
	fogp.Global=GetPrivateProfileInt("Fog", "Global", fogp.Global, fname);

	pp.Enable=GetPrivateProfileInt("PostProcessing", "Enable", pp.Enable, fname);
	pp.Base=GetPrivateProfileInt("PostProcessing", "Base", pp.Base, fname);
	pp.Saturation=GetPrivateProfileInt("PostProcessing", "Saturation", pp.Saturation, fname);
	pp.Bloom=GetPrivateProfileInt("PostProcessing", "Bloom", pp.Bloom, fname);
	pp.BloomUI=GetPrivateProfileInt("PostProcessing", "BloomUI", pp.BloomUI, fname);
	pp.BloomPersistence=GetPrivateProfileInt("PostProcessing", "BloomPersistence", pp.BloomPersistence, fname);
	pp.BloomSaturation=GetPrivateProfileInt("PostProcessing", "BloomSaturation", pp.BloomSaturation, fname);
	pp.BloomSpread=GetPrivateProfileInt("PostProcessing", "BloomSpread", pp.BloomSpread, fname);
	pp.BloomThreshold=GetPrivateProfileInt("PostProcessing", "BloomThreshold", pp.BloomThreshold, fname);
	pp.BloomTrail=GetPrivateProfileInt("PostProcessing", "BloomTrail", pp.BloomTrail, fname);
	pp.BloomShift=GetPrivateProfileInt("PostProcessing", "BloomShift", pp.BloomShift, fname);
	if(pp.BloomShift<0) pp.BloomShift=0;
	if(pp.BloomShift>8) pp.BloomShift=8;
	pp.BloomShiftRemodulate=GetPrivateProfileInt("PostProcessing", "BloomShiftRemodulate", pp.BloomShiftRemodulate, fname);
	pp.DynamicThreshold=GetPrivateProfileInt("PostProcessing", "DynamicThreshold", pp.DynamicThreshold, fname);
	char str[MAX_PATH];
	if(GetPrivateProfileString("PostProcessing", "DynamicGamma", "", str, MAX_PATH, fname)>0) sscanf(str, "%g", &pp.DynamicGamma);
	if(!EnableGamma) pp.DynamicGamma=0;
	if(GetPrivateProfileString("PostProcessing", "DynamicDecay", "", str, MAX_PATH, fname)>0) sscanf(str, "%g", &pp.DynamicDecay);
	if(pp.DynamicDecay<0) pp.DynamicDecay=0;
	if(pp.DynamicDecay>.99f) pp.DynamicDecay=.99f;
	if(GetPrivateProfileString("PostProcessing", "DynamicMult", "", str, MAX_PATH, fname)>0) sscanf(str, "%g", &pp.DynamicMult);
	if(GetPrivateProfileString("PostProcessing", "DynamicAdd", "", str, MAX_PATH, fname)>0) sscanf(str, "%g", &pp.DynamicAdd);
}

#ifdef NONEXCLUSIVE
bool _stdcall SetWindowPosHook(HWND, HWND, int, int, int, int, int) { return true; }
bool _stdcall SetWindowLongHook(HWND, int, int) { return true; }
bool _stdcall ShowWindowHook(HWND, int) { return true; }
#endif

bool _stdcall DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID  lpreserved) {
	if(dwReason==DLL_PROCESS_ATTACH) {

#ifdef NONEXCLUSIVE
		SafeWrite32(0x00600304, (DWORD)SetWindowPosHook);
		SafeWrite32(0x0060025C, (DWORD)SetWindowLongHook);
		SafeWrite32(0x0060028C, (DWORD)ShowWindowHook);
#endif

		SafeMode=GetPrivateProfileInt("Main", "SafeMode", 0, ".\\ddfix.ini");

		gWidth=0;
		gHeight=0;

		FILE* cam=fopen("cam.cfg", "r");
		if(cam) {
			char buf[256];
			while(fgets(buf,256,cam)) {
				char* p;
				p=strchr(buf, ';');
				if(p) *p=0;
				if(!(p=strstr(buf, "game_screen_size"))) continue;
				if(!(p=strchr(p, ' '))) continue;
				sscanf(p, "%d %d", &gWidth, &gHeight);
			}
			fclose(cam);
		}

		if(gWidth<=640 || gHeight<=480) {
			MessageBox(0, "Invalid resolution in cam.cfg.", "Error", MB_OK);
			ExitProcess(1);
		}

		FUMode=0;
		FUxmul=(float)gWidth/1024.0f;
		FUymul=(float)gHeight/768.0f;
		if(*(DWORD*)0x005BAA39==0x6b636162 && *(WORD*)0x005BAA3E==0x6d79 && *(DWORD*)0x0041D948==0x52501c24)
			FUMode=1;
		FUMode=GetPrivateProfileInt("Main", "FUMode", FUMode, ".\\ddfix.ini");

		char str[MAX_PATH];
		EnableMissionIni=GetPrivateProfileInt("Main", "EnableMissionIni", 1, ".\\ddfix.ini");
		if(SafeMode || GetPrivateProfileInt("Main", "MultiCoreFix", 1, ".\\ddfix.ini")) {
			HANDLE process=GetCurrentProcess();
			SetProcessAffinityMask(process, 1);
		}
		if(!SafeMode && !FUMode && GetPrivateProfileInt("Main", "VideoFix", 1, ".\\ddfix.ini")) {
			PROCESS_INFORMATION p;
			STARTUPINFO s;
			memset(&s, 0, sizeof(STARTUPINFO));
			s.cb=sizeof(STARTUPINFO);
			if(CreateProcessA(0, "regsvr32 /s LGVID.AX", 0, 0, false, 0, 0, 0, &s, &p)) {
				CloseHandle(p.hThread);
				WaitForInputIdle(p.hProcess, 100);
				CloseHandle(p.hProcess);
			}
		}
		if(!SafeMode && GetPrivateProfileInt("Main", "DisableWindowsKey", 1, ".\\ddfix.ini")) {
			g_hKeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(NULL), 0 );
		}
		RefreshRate=GetPrivateProfileInt("Main", "RefreshRate", 0, ".\\ddfix.ini");
		FlipVSync=GetPrivateProfileInt("Main", "FlipVSync", 1, ".\\ddfix.ini");
		FlipInterval=GetPrivateProfileInt("Main", "FlipInterval", 1, ".\\ddfix.ini");
		switch(FlipInterval) {
			case 2: FlipInterval=DDFLIP_INTERVAL2; break;
			case 3: FlipInterval=DDFLIP_INTERVAL3; break;
			case 4: FlipInterval=DDFLIP_INTERVAL4; break;
			default: FlipInterval=0;
		}
		FrameRateLimit=0;
		if(GetPrivateProfileString("Main", "FrameRateLimit", "", str, MAX_PATH, ".\\ddfix.ini")>0) sscanf(str, "%g", &FrameRateLimit);
		MenuUpdateDelay=GetPrivateProfileInt("Main", "MenuUpdateDelay", 0, ".\\ddfix.ini");
		MenuScreenAspect=4.0f/3.0f;
		if(GetPrivateProfileString("Main", "MenuScreenAspect", "", str, MAX_PATH, ".\\ddfix.ini")>0) sscanf(str, "%g", &MenuScreenAspect);
		AnisotropicFiltering=GetPrivateProfileInt("Main", "AnisotropicFiltering", 1, ".\\ddfix.ini");
		TripleBuffer=GetPrivateProfileInt("Main", "TripleBuffer", 1, ".\\ddfix.ini");
		ZBufferBitDepth=GetPrivateProfileInt("Main", "ZBufferBitDepth", 24, ".\\ddfix.ini");
		EnableGamma=GetPrivateProfileInt("Main", "EnableGamma", 1, ".\\ddfix.ini");
		FovModification=GetPrivateProfileInt("Main", "FovModification", 100, ".\\ddfix.ini");
		ItemSize=GetPrivateProfileInt("Main", "ItemSize", 20, ".\\ddfix.ini");
		LimbZBufferFix=0;
		WaterAlphaFix=0;
		ModulateShift=GetPrivateProfileInt("Main", "ModulateShift", 0, ".\\ddfix.ini");
		if(ModulateShift<0) ModulateShift=0;
		if(ModulateShift>2) ModulateShift=2;
		UseSysMemOverlay=GetPrivateProfileInt("Main", "UseSysMemOverlay", 0, ".\\ddfix.ini");
		OverlayColourKey=GetPrivateProfileInt("Main", "OverlayColourKey", 0xffdf, ".\\ddfix.ini");
		LayeredOverlay=0;
		TextureReplacement=GetPrivateProfileInt("Main", "TextureReplacement", 1, ".\\ddfix.ini");
		if(!GetPrivateProfileString("Main", "TexturePath", "", TexturePath, MAX_PATH, ".\\ddfix.ini")) strcpy_s(TexturePath, "res\\ddfix\\");
		
		MemAddrParams def={0};

		if(*(DWORD*)0x005BAA39==0x47391875 && *(WORD*)0x005BAA3E==0x1375 && *(DWORD*)0x0041D948==0xfffffba4) {
			/* T2 v1.18 */
			LimbZBufferFix=1;
			WaterAlphaFix=1;
			BloomUIDef=0;
			def.CopyTex=0x41D948;
			def.MipMap=0x41D96B;
			def.CreateVidTexture=0x62A67C;
			def.TextureExists=0x5BD427;
			def.CreateFile=0x60015C;
			def.BltPrimary=0x591440;
			// From weak-ling's DarkWidescreen patch.
			def.Resolution=0x6781F4;
			def.VertResLimit=0x565983;
			def.FovModification=0x4393ED;
			def.AspectCorrection=0x5ABE2A;
			def.ItemSize=0x561315;

			def.OrbAspectCorrection=0x550915;
			def.MaxSkyIntensity=0x43B37E;

			//Remove check for dodgy drivers
			SafeWrite16(0x005BAA39, 0x9090);
			SafeWrite16(0x005BAA3E, 0x9090);
		} else if(*(DWORD*)0x005BAA39==0xe0004c2b && *(WORD*)0x005BAA3E==0x42 && *(DWORD*)0x0041D948==0xc4830000) {
			/* TG v1.37 */
			BloomUIDef=0;
			/* From Assidragon in this post:
			 * http://www.ttlg.com/forums/showthread.php?t=117616&p=1719806&viewfull=1#post1719806
			 */
			def.CopyTex=0x418BEE;
			def.MipMap=0x418C11;
			def.CreateVidTexture=0x5DA9B4;
			def.TextureExists=0x576978;
			def.CreateFile=0x5B614C;
			def.BltPrimary=0x548480;
			// From weak-ling's DarkWidescreen patch.
			def.Resolution=0x616118;
			def.VertResLimit=0x51D114;
			def.FovModification=0x43359B;
			def.AspectCorrection=0x56303A;
			def.ItemSize=0x519265;
		} else if(*(DWORD*)0x005BAA39==0xc53b0000 && *(WORD*)0x005BAA3E==0x246c && *(DWORD*)0x0041D948==0xb7815ff) {
			/* SS2 v2.3 */
			LayeredOverlay=1;
			BloomUIDef=0;
			/* From Assidragon in this post:
			 * http://www.ttlg.com/forums/showthread.php?t=117616&p=1719785&viewfull=1#post1719785
			 */
			def.CopyTex=0x419B5E;
			def.MipMap=0x419B81;
			def.CreateVidTexture=0x65A768;
			def.TextureExists=0x5E3451;
			def.SS2CreateFile=0x61DD99;
			def.BltPrimary=0x5B89C0;
			// From weak-ling's DarkWidescreen patch.
			def.Resolution=0x6B17AC;
			def.VertResLimit=0x569FB0;
			def.FovModification=0x434464;
			def.AspectCorrection=0x5D332A;
			def.ItemSize=0;
		} else if(*(DWORD*)0x005BAA39==0x90909090 && *(WORD*)0x005BAA3E==0x9090 && *(DWORD*)0x0041D948==0xa1e8cf8b) {
			/* Dromed2 v1.18 */
			def.CopyTex=0x425A18;
			def.MipMap=0x425A3B;
			def.CreateVidTexture=0x6DB2E4;
			def.CreateFile=0x6B00D8;
			def.BltPrimary=0x62F870;
		} else if(*(DWORD*)0x005BAA39==0x68000000 && *(WORD*)0x005BAA3E==0x681d && *(DWORD*)0x0041D948==0x90909090) {
			/* Dromed v1.37 */
			def.CopyTex=0x42039E;
			def.MipMap=0x4203C1;
			def.CreateVidTexture=0x699D34;
			def.CreateFile=0x66E1AC;
			def.BltPrimary=0x5EF960;
		} else if(*(DWORD*)0x005BAA39==0x909090c3 && *(WORD*)0x005BAA3E==0x9090 && *(DWORD*)0x0041D948==0x90909090) {
			/* ShockEd v2.12 */
			LayeredOverlay=1;
			def.CopyTex=0x42105E;
			def.MipMap=0x421081;
			def.CreateVidTexture=0x72310c;
			def.SS2CreateFile=0x6E14F9;
			def.BltPrimary=0x66C250;
		}

		if(FUMode) {
			OverlayColourKey=0xe01c;
			BloomUIDef=0;
			def.CopyTex=0;
			def.MipMap=0;
			def.CreateVidTexture=0;
			def.TextureExists=0;
			def.CreateFile=0;
			def.SS2CreateFile=0;
			def.BltPrimary=0;
			def.Resolution=0;
			def.VertResLimit=0;
			def.FovModification=0;
			def.AspectCorrection=0x53C58A;
			def.ItemSize=0;
			def.OrbAspectCorrection=0;
			def.MaxSkyIntensity=0;
		}

		memaddrp.CopyTex=GetPrivateProfileInt("MemAddr", "CopyTex", def.CopyTex, ".\\ddfix.ini");
		memaddrp.MipMap=GetPrivateProfileInt("MemAddr", "MipMap", def.MipMap, ".\\ddfix.ini");
		memaddrp.CreateVidTexture=GetPrivateProfileInt("MemAddr", "CreateVidTexture", def.CreateVidTexture, ".\\ddfix.ini");
		memaddrp.TextureExists=GetPrivateProfileInt("MemAddr", "TextureExists", def.TextureExists, ".\\ddfix.ini");
		memaddrp.CreateFile=GetPrivateProfileInt("MemAddr", "CreateFile", def.CreateFile, ".\\ddfix.ini");
		memaddrp.SS2CreateFile=GetPrivateProfileInt("MemAddr", "SS2CreateFile", def.SS2CreateFile, ".\\ddfix.ini");
		memaddrp.BltPrimary=GetPrivateProfileInt("MemAddr", "BltPrimary", def.BltPrimary, ".\\ddfix.ini");

		memaddrp.Resolution=GetPrivateProfileInt("MemAddr", "Resolution", def.Resolution, ".\\ddfix.ini");
		memaddrp.VertResLimit=GetPrivateProfileInt("MemAddr", "VertResLimit", def.VertResLimit, ".\\ddfix.ini");
		memaddrp.FovModification=GetPrivateProfileInt("MemAddr", "FovModification", def.FovModification, ".\\ddfix.ini");
		memaddrp.AspectCorrection=GetPrivateProfileInt("MemAddr", "AspectCorrection", def.AspectCorrection, ".\\ddfix.ini");
		memaddrp.ItemSize=GetPrivateProfileInt("MemAddr", "ItemSize", def.ItemSize, ".\\ddfix.ini");

		memaddrp.OrbAspectCorrection=GetPrivateProfileInt("MemAddr", "OrbAspectCorrection", def.OrbAspectCorrection, ".\\ddfix.ini");
		memaddrp.MaxSkyIntensity=GetPrivateProfileInt("MemAddr", "MaxSkyIntensity", def.MaxSkyIntensity, ".\\ddfix.ini");

		LimbZBufferFix=GetPrivateProfileInt("Main", "LimbZBufferFix", LimbZBufferFix, ".\\ddfix.ini");
		WaterAlphaFix=GetPrivateProfileInt("Main", "WaterAlphaFix", WaterAlphaFix, ".\\ddfix.ini");
		LayeredOverlay=GetPrivateProfileInt("Main", "LayeredOverlay", LayeredOverlay, ".\\ddfix.ini");

		readIni(".\\ddfix.ini");

		if(SafeMode) {
			EnableMissionIni=0;
			RefreshRate=0;
			FlipVSync=1;
			FlipInterval=0;
			FrameRateLimit=0;
			MenuUpdateDelay=0;
			MenuScreenAspect=4.0f/3.0f;
			AnisotropicFiltering=0;
			TripleBuffer=0;
			ZBufferBitDepth=16;
			EnableGamma=1;
			LimbZBufferFix=0;
			WaterAlphaFix=0;
			UseSysMemOverlay=0;
			OverlayColourKey=0xffdf;
			LayeredOverlay=0;
			TextureReplacement=0;
			memset(&memaddrp, 0, sizeof(memaddrp));
			fogp.Enable=0;
			pp.Enable=0;
		}

		if(TextureReplacement) OverrideSetup();

		WidescreenSetup();

		if(memaddrp.MaxSkyIntensity) {
			/*
			 * Ignore max sky intensity by replacing FSUBR with FLD.
			 * 0043B37E  |. |D82D BC036000 |FSUBR DWORD PTR DS:[6003BC] ; FLOAT 1.000000
			 */
			SafeWrite16(memaddrp.MaxSkyIntensity, 0x05d9);
		}

	} else if(dwReason==DLL_PROCESS_DETACH) {
		UnhookWindowsHookEx( g_hKeyboardHook );
		SavedDirectDraw->Release();
#ifdef CHECKEDBUILD
		CloseHandle(LogFile);
#endif
	}
	return true;
}

#ifdef CHECKEDBUILD
void Log(char* msg) {
	DWORD unused;
	WriteFile(LogFile, msg, strlen(msg), &unused, 0);
}
#endif

typedef HRESULT (_stdcall *DDrawCreateProc)(void* a, void* b, void* c);
typedef HRESULT (_stdcall *DDrawEnumerateProc)(void* callback, void* context);
typedef void (_stdcall *DDrawMiscProc)();
static DDrawCreateProc DDrawCreate=0;
static DDrawEnumerateProc DDrawEnumerate=0;
static DDrawMiscProc AcquireLock;
static DDrawMiscProc ParseUnknown;
static DDrawMiscProc InternalLock;
static DDrawMiscProc InternalUnlock;
static DDrawMiscProc ReleaseLock;

static void LoadDLL() {
	char path[MAX_PATH];
	GetSystemDirectoryA(path,MAX_PATH);
	strcat_s(path, "\\ddraw.dll");
	HMODULE ddrawdll=LoadLibraryA(path);
	DDrawCreate=(DDrawCreateProc)GetProcAddress(ddrawdll, "DirectDrawCreate");
	DDrawEnumerate=(DDrawEnumerateProc)GetProcAddress(ddrawdll, "DirectDrawEnumerateA");

	AcquireLock=(DDrawMiscProc)GetProcAddress(ddrawdll, "AcquireDDThreadLock");
	ParseUnknown=(DDrawMiscProc)GetProcAddress(ddrawdll, "D3DParseUnknownCommand");
	InternalLock=(DDrawMiscProc)GetProcAddress(ddrawdll, "DDInternalLock");
	InternalUnlock=(DDrawMiscProc)GetProcAddress(ddrawdll, "DDInternalUnlock");
	ReleaseLock=(DDrawMiscProc)GetProcAddress(ddrawdll, "ReleaseDDThreadLock");
}

void __declspec(naked) FakeAcquireLock() { 
	_asm jmp AcquireLock;
}
void __declspec(naked) FakeParseUnknown() { 
	_asm jmp ParseUnknown;
}
void __declspec(naked) FakeInternalLock() {
	_asm jmp InternalLock;
}
void __declspec(naked) FakeInternalUnlock() {
	_asm jmp InternalUnlock;
}
void __declspec(naked) FakeReleaseLock() {
	_asm jmp ReleaseLock;
}

HRESULT _stdcall FakeDirectDrawCreate(GUID* a, IDirectDraw** b, IUnknown* c) {
	if(SavedDirectDraw) {
		*b=(IDirectDraw*)SavedDirectDraw;
		SavedDirectDraw->AddRef();
		return 0;
	}
	if(!DDrawCreate) LoadDLL();
	HRESULT hr=DDrawCreate(a,b,c);
	if(FAILED(hr)) return hr;
	SavedDirectDraw=new FakeDirectDraw(*b);
	*b=(IDirectDraw*)SavedDirectDraw;
	SavedDirectDraw->AddRef();
	return 0;
}

HRESULT _stdcall FakeDirectDrawEnumerate(void* lpCallback, void* lpContext) {
	if(!DDrawEnumerate) LoadDLL();
	return DDrawEnumerate(lpCallback, lpContext);
}
