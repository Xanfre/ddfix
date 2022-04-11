#pragma once
#include "FakeCommon.h"

class FakePrimarySurface : IDirectDrawSurface4 {
private:
	ULONG Refs;
	DDSURFACEDESC2 rDesc;
	DDSURFACEDESC2 mDesc;
	DDSURFACEDESC2 oDesc;
	IDirectDrawSurface4* Real;
	IDirectDrawSurface4* Real2;
	IDirectDrawSurface4* menu;
	IDirectDrawSurface4* overlay[2];
	IDirectDrawSurface4 *fakebb, *olc, *ola;
	bool MenuMode;

	void ClearOverlay(LPRECT dr=NULL);
	void BltOverlay();
	void getFlareAmount();
public:
	FakePrimarySurface(IDirectDraw4* ddraw);
	void SetMenuMode(bool b);
	IDirectDrawSurface4* QueryReal();
    // IUnknown methods
    HRESULT _stdcall QueryInterface(REFIID riid, LPVOID * ppvObj);
    ULONG _stdcall AddRef();
    ULONG _stdcall Release();
    // IDirectDrawSurface methods
	HRESULT _stdcall AddAttachedSurface(LPDIRECTDRAWSURFACE4 a);
	HRESULT _stdcall AddOverlayDirtyRect(LPRECT);
    HRESULT _stdcall Blt(LPRECT a,LPDIRECTDRAWSURFACE4 b, LPRECT c,DWORD d, LPDDBLTFX e);
    HRESULT _stdcall BltBatch(LPDDBLTBATCH, DWORD, DWORD);
    HRESULT _stdcall BltFast(DWORD,DWORD,LPDIRECTDRAWSURFACE4, LPRECT,DWORD);
    HRESULT _stdcall DeleteAttachedSurface(DWORD a,LPDIRECTDRAWSURFACE4 b);
    HRESULT _stdcall EnumAttachedSurfaces(LPVOID,LPDDENUMSURFACESCALLBACK2);
    HRESULT _stdcall EnumOverlayZOrders(DWORD,LPVOID,LPDDENUMSURFACESCALLBACK2);
    HRESULT _stdcall Flip(LPDIRECTDRAWSURFACE4 a, DWORD b);
    HRESULT _stdcall GetAttachedSurface(LPDDSCAPS2 a, LPDIRECTDRAWSURFACE4 * b);
    HRESULT _stdcall GetBltStatus(DWORD);
    HRESULT _stdcall GetCaps(LPDDSCAPS2);
    HRESULT _stdcall GetClipper(LPDIRECTDRAWCLIPPER *);
    HRESULT _stdcall GetColorKey(DWORD, LPDDCOLORKEY);
    HRESULT _stdcall GetDC(HDC *);
    HRESULT _stdcall GetFlipStatus(DWORD);
    HRESULT _stdcall GetOverlayPosition(LPLONG, LPLONG);
    HRESULT _stdcall GetPalette(LPDIRECTDRAWPALETTE *);
    HRESULT _stdcall GetPixelFormat(LPDDPIXELFORMAT a);
    HRESULT _stdcall GetSurfaceDesc(LPDDSURFACEDESC2 a);
    HRESULT _stdcall Initialize(LPDIRECTDRAW, LPDDSURFACEDESC2);
    HRESULT _stdcall IsLost();
    HRESULT _stdcall Lock(LPRECT a,LPDDSURFACEDESC2 b,DWORD c,HANDLE d);
    HRESULT _stdcall ReleaseDC(HDC);
    HRESULT _stdcall Restore();
    HRESULT _stdcall SetClipper(LPDIRECTDRAWCLIPPER);
    HRESULT _stdcall SetColorKey(DWORD, LPDDCOLORKEY);
    HRESULT _stdcall SetOverlayPosition(LONG, LONG);
    HRESULT _stdcall SetPalette(LPDIRECTDRAWPALETTE a);
    HRESULT _stdcall Unlock(LPRECT a);
    HRESULT _stdcall UpdateOverlay(LPRECT, LPDIRECTDRAWSURFACE4,LPRECT,DWORD, LPDDOVERLAYFX);
    HRESULT _stdcall UpdateOverlayDisplay(DWORD);
    HRESULT _stdcall UpdateOverlayZOrder(DWORD, LPDIRECTDRAWSURFACE4);
	//*** Added in the v2 interface ***
    HRESULT _stdcall GetDDInterface(LPVOID*);
    HRESULT _stdcall PageLock(DWORD);
    HRESULT _stdcall PageUnlock(DWORD);
    //*** Added in the v3 interface ***
    HRESULT _stdcall SetSurfaceDesc(LPDDSURFACEDESC2, DWORD);
    //*** Added in the v4 interface ***
    HRESULT _stdcall SetPrivateData(REFGUID, LPVOID, DWORD, DWORD);
    HRESULT _stdcall GetPrivateData(REFGUID, LPVOID, LPDWORD);
    HRESULT _stdcall FreePrivateData(REFGUID);
    HRESULT _stdcall GetUniquenessValue(LPDWORD);
    HRESULT _stdcall ChangeUniquenessValue();
};

extern FakePrimarySurface* fakePrimary;
