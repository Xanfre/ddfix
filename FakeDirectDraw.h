#pragma once
#include "FakeCommon.h"
#include "FakePrimarySurface.h"

class FakeDirectDraw : IDirectDraw4 {
private:
	ULONG Refs;
	DWORD savedWidth;
	DWORD savedHeight;
	IDirectDraw4* Real;
	IDirectDraw* dd1;
	FakePrimarySurface* primary;
	DWORD texcount;
public:
	FakeDirectDraw(IDirectDraw* ddraw);
    // IUnknown methods
    HRESULT _stdcall QueryInterface(REFIID a, LPVOID* b);
    ULONG _stdcall AddRef();
    ULONG _stdcall Release();
    // IDirectDraw methods
    HRESULT _stdcall Compact();
    HRESULT _stdcall CreateClipper(DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*);
	HRESULT _stdcall CreatePalette(DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE* c, IUnknown*);
	HRESULT _stdcall CreateSurface(LPDDSURFACEDESC2 a, LPDIRECTDRAWSURFACE4 * b, IUnknown * c);
    HRESULT _stdcall DuplicateSurface(LPDIRECTDRAWSURFACE4, LPDIRECTDRAWSURFACE4 *);
    HRESULT _stdcall EnumDisplayModes(DWORD a, LPDDSURFACEDESC2 b, LPVOID c, LPDDENUMMODESCALLBACK2 d);
    HRESULT _stdcall EnumSurfaces(DWORD, LPDDSURFACEDESC2, LPVOID,LPDDENUMSURFACESCALLBACK2);
	HRESULT _stdcall FlipToGDISurface();
    HRESULT _stdcall GetCaps(LPDDCAPS a, LPDDCAPS b);
    HRESULT _stdcall GetDisplayMode(LPDDSURFACEDESC2 a);
    HRESULT _stdcall GetFourCCCodes(LPDWORD,LPDWORD);
    HRESULT _stdcall GetGDISurface(LPDIRECTDRAWSURFACE4 *);
    HRESULT _stdcall GetMonitorFrequency(LPDWORD);
    HRESULT _stdcall GetScanLine(LPDWORD);
    HRESULT _stdcall GetVerticalBlankStatus(LPBOOL);
    HRESULT _stdcall Initialize(GUID *);
    HRESULT _stdcall RestoreDisplayMode();
	HRESULT _stdcall SetCooperativeLevel(HWND a, DWORD b);
    HRESULT _stdcall SetDisplayMode(DWORD a, DWORD b, DWORD c, DWORD d, DWORD e);
    HRESULT _stdcall WaitForVerticalBlank(DWORD, HANDLE);
	//*** Added in the V4 Interface ***
	HRESULT _stdcall GetAvailableVidMem(LPDDSCAPS2 a, LPDWORD b, LPDWORD c);
    //*** Added in the V4 Interface ***
    HRESULT _stdcall GetSurfaceFromDC(HDC, LPDIRECTDRAWSURFACE4 *);
    HRESULT _stdcall RestoreAllSurfaces();
    HRESULT _stdcall TestCooperativeLevel();
    HRESULT _stdcall GetDeviceIdentifier(LPDDDEVICEIDENTIFIER, DWORD );
};
