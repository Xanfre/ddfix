#include "FakeVidSurface.h"

FakeVidSurface::FakeVidSurface(IDirectDrawSurface4* real, OverridenTex* _ot) {
	Refs=1;
	Real=real;
	ot=_ot;
	if(ot) AddTextureToVRam(ot, this);
}
// IUnknown methods
HRESULT _stdcall FakeVidSurface::QueryInterface(REFIID a, LPVOID * b) {
	if(a==IID_IDirect3DTexture2) {
		HRESULT hr=Real->QueryInterface(a,b);
		if(FAILED(hr)) return hr;
		*b=new FakeVidTexture((IDirect3DTexture2*)*b, ot, this);
		return DD_OK;
	} else UNUSEDFUNCTION;
}
ULONG _stdcall FakeVidSurface::AddRef()  { return ++Refs; }
ULONG _stdcall FakeVidSurface::Release() {
	if(!--Refs) {
		if(ot) RemoveTextureFromVRam(ot);
		Real->Release();
		delete this;
		return 0;
	} else return Refs;
}
// IDirectDrawSurface methods
HRESULT _stdcall FakeVidSurface::AddAttachedSurface(LPDIRECTDRAWSURFACE4) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::AddOverlayDirtyRect(LPRECT) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::Blt(LPRECT,LPDIRECTDRAWSURFACE4, LPRECT,DWORD, LPDDBLTFX) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::BltBatch(LPDDBLTBATCH, DWORD, DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::BltFast(DWORD,DWORD,LPDIRECTDRAWSURFACE4, LPRECT,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::DeleteAttachedSurface(DWORD,LPDIRECTDRAWSURFACE4) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::EnumAttachedSurfaces(LPVOID,LPDDENUMSURFACESCALLBACK2) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::EnumOverlayZOrders(DWORD,LPVOID,LPDDENUMSURFACESCALLBACK2) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::Flip(LPDIRECTDRAWSURFACE4, DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetAttachedSurface(LPDDSCAPS2, LPDIRECTDRAWSURFACE4 *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetBltStatus(DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetCaps(LPDDSCAPS2) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetClipper(LPDIRECTDRAWCLIPPER *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetColorKey(DWORD, LPDDCOLORKEY) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetDC(HDC *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetFlipStatus(DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetOverlayPosition(LPLONG, LPLONG) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetPalette(LPDIRECTDRAWPALETTE *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetPixelFormat(LPDDPIXELFORMAT) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetSurfaceDesc(LPDDSURFACEDESC2) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::Initialize(LPDIRECTDRAW, LPDDSURFACEDESC2) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::IsLost() {
	return Real->IsLost();
}
HRESULT _stdcall FakeVidSurface::Lock(LPRECT a,LPDDSURFACEDESC2 b,DWORD c,HANDLE d) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::ReleaseDC(HDC) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::Restore() {
	return Real->Restore();
}
HRESULT _stdcall FakeVidSurface::SetClipper(LPDIRECTDRAWCLIPPER) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::SetColorKey(DWORD, LPDDCOLORKEY) { UNUSEDFUNCTION;	}
HRESULT _stdcall FakeVidSurface::SetOverlayPosition(LONG, LONG) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::SetPalette(LPDIRECTDRAWPALETTE) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::Unlock(LPRECT a) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::UpdateOverlay(LPRECT, LPDIRECTDRAWSURFACE4,LPRECT,DWORD, LPDDOVERLAYFX) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::UpdateOverlayDisplay(DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::UpdateOverlayZOrder(DWORD, LPDIRECTDRAWSURFACE4) { UNUSEDFUNCTION; }
//*** Added in the v2 interface ***
HRESULT _stdcall FakeVidSurface::GetDDInterface(LPVOID*) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::PageLock(DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::PageUnlock(DWORD) { UNUSEDFUNCTION; }
//*** Added in the v3 interface ***
HRESULT _stdcall FakeVidSurface::SetSurfaceDesc(LPDDSURFACEDESC2, DWORD) { UNUSEDFUNCTION; }
//*** Added in the v4 interface ***
HRESULT _stdcall FakeVidSurface::SetPrivateData(REFGUID, LPVOID, DWORD, DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetPrivateData(REFGUID, LPVOID, LPDWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::FreePrivateData(REFGUID) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::GetUniquenessValue(LPDWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidSurface::ChangeUniquenessValue() { UNUSEDFUNCTION; }

//XXXXXXXXXXXXX
//XX Texture XX
//XXXXXXXXXXXXX
FakeVidTexture::FakeVidTexture(IDirect3DTexture2* tex, OverridenTex* _ot, FakeVidSurface *_surf) {
	Refs=1;
	Real=tex;
	ot=_ot;
	loaded=false;
	surf=_surf;
}
IDirect3DTexture2* FakeVidTexture::QueryReal() { return Real; }
/*** IUnknown methods ***/
HRESULT _stdcall FakeVidTexture::QueryInterface(REFIID, LPVOID *) { UNUSEDFUNCTION; }
ULONG _stdcall FakeVidTexture::AddRef() { return ++Refs; }
ULONG _stdcall FakeVidTexture::Release() {
	if(!--Refs) {
		Real->Release();
		delete this;
		return 0;
	} else return Refs;
}
/*** IDirect3DTexture2 methods ***/
HRESULT _stdcall FakeVidTexture::GetHandle(LPDIRECT3DDEVICE2,LPD3DTEXTUREHANDLE) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidTexture::PaletteChanged(DWORD,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeVidTexture::Load(LPDIRECT3DTEXTURE2 a) {
	//if(loaded) return D3DERR_TEXTURE_LOAD_FAILED;
	loaded=true;
	if(ot) return Real->Load(ot->tex);
	else return Real->Load(a);
}
FakeVidSurface *FakeVidTexture::QuerySurf() {
	return surf;
}
