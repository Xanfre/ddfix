#include "FakeD3D.h"

#include "FakeD3DDevice.h"
#include "FakePrimarySurface.h"
#include "FakeMaterial.h"

FakeD3D::FakeD3D(IDirect3D3* real) {
	Refs=1;
	Real=real;
}
/*** IUnknown methods ***/
HRESULT _stdcall FakeD3D::QueryInterface(REFIID, LPVOID *) { UNUSEDFUNCTION; }
ULONG _stdcall FakeD3D::AddRef() { return ++Refs; }
ULONG _stdcall FakeD3D::Release() { COMMONRELEASE }
/*** IDirect3D3 methods ***/
HRESULT _stdcall FakeD3D::EnumDevices(LPD3DENUMDEVICESCALLBACK a,LPVOID b) {
	return Real->EnumDevices(a,b);
}
HRESULT _stdcall FakeD3D::CreateLight(LPDIRECT3DLIGHT*,LPUNKNOWN) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3D::CreateMaterial(LPDIRECT3DMATERIAL3* a,LPUNKNOWN b) {
	HRESULT hr=Real->CreateMaterial(a,b);
	CHECKFAIL(hr);
	if(FAILED(hr)) return hr;
	*a=(IDirect3DMaterial3*)new FakeMaterial(*a);
	return DD_OK;
}
HRESULT _stdcall FakeD3D::CreateViewport(LPDIRECT3DVIEWPORT3* a,LPUNKNOWN b) {
	HRESULT hr=Real->CreateViewport(a,b);
	CHECKFAIL(hr);
	return hr;
}
HRESULT _stdcall FakeD3D::FindDevice(LPD3DFINDDEVICESEARCH,LPD3DFINDDEVICERESULT) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3D::CreateDevice(REFCLSID a,LPDIRECTDRAWSURFACE4 b,LPDIRECT3DDEVICE3* c,LPUNKNOWN d) {
	b=((FakePrimarySurface*)b)->QueryReal();
	HRESULT hr=Real->CreateDevice(a,b,c,d);
	CHECKFAIL(hr);
	if(FAILED(hr)) return hr;
	LOG("Device created.");
	*c=(IDirect3DDevice3*)new FakeD3DDevice(*c);
	return DD_OK;
}
HRESULT _stdcall FakeD3D::CreateVertexBuffer(LPD3DVERTEXBUFFERDESC,LPDIRECT3DVERTEXBUFFER*,DWORD,LPUNKNOWN) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3D::EnumZBufferFormats(REFCLSID a,LPD3DENUMPIXELFORMATSCALLBACK b,LPVOID c) {
	DDPIXELFORMAT pp;
	memset(&pp, 0, sizeof(DDPIXELFORMAT));
	pp.dwSize=sizeof(DDPIXELFORMAT);
	pp.dwFlags=0x400;
	pp.dwZBufferBitDepth=16;
	pp.dwZBitMask=0xffff;
	b(&pp, c);
	return DD_OK;
	//return Real->EnumZBufferFormats(a,b,c);
}
HRESULT _stdcall FakeD3D::EvictManagedTextures() { UNUSEDFUNCTION; }