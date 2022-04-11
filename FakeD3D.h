#pragma once
#include "FakeCommon.h"

class FakeD3D : IDirect3D3 {
private:
	ULONG Refs;
	IDirect3D3* Real;
public:
	FakeD3D(IDirect3D3* real);
    /*** IUnknown methods ***/
	HRESULT _stdcall QueryInterface(REFIID riid, LPVOID * ppvObj);
	ULONG _stdcall AddRef();
	ULONG _stdcall Release();
    /*** IDirect3D3 methods ***/
    HRESULT _stdcall EnumDevices(LPD3DENUMDEVICESCALLBACK a,LPVOID b);
    HRESULT _stdcall CreateLight(LPDIRECT3DLIGHT*,LPUNKNOWN);
    HRESULT _stdcall CreateMaterial(LPDIRECT3DMATERIAL3* a,LPUNKNOWN b);
    HRESULT _stdcall CreateViewport(LPDIRECT3DVIEWPORT3* a,LPUNKNOWN b);
    HRESULT _stdcall FindDevice(LPD3DFINDDEVICESEARCH,LPD3DFINDDEVICERESULT);
    HRESULT _stdcall CreateDevice(REFCLSID a,LPDIRECTDRAWSURFACE4 b,LPDIRECT3DDEVICE3* c,LPUNKNOWN d);
    HRESULT _stdcall CreateVertexBuffer(LPD3DVERTEXBUFFERDESC,LPDIRECT3DVERTEXBUFFER*,DWORD,LPUNKNOWN);
    HRESULT _stdcall EnumZBufferFormats(REFCLSID a,LPD3DENUMPIXELFORMATSCALLBACK b,LPVOID c);
    HRESULT _stdcall EvictManagedTextures();
};
