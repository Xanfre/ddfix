#include "FakeDirectDraw.h"

#include "main.h"
#include "FakeD3D.h"
#include "FakePrimarySurface.h"
#include "FakeVidSurface.h"
#include "TexOverride.h"

IDirectDraw4* rDDraw=0;

FakeDirectDraw::FakeDirectDraw(IDirectDraw* ddraw) {
	Refs=1;
	savedWidth=640;
	savedHeight=480;
	dd1=ddraw;
	HRESULT hr=ddraw->QueryInterface(IID_IDirectDraw4, (void**)&Real);
	rDDraw=Real;
	CHECKFAIL(hr);
	primary=0;
	texcount=0;
	Real->SetCooperativeLevel(NULL,DDSCL_NORMAL);
}
// IUnknown methods
HRESULT _stdcall FakeDirectDraw::QueryInterface(REFIID a, LPVOID* b) {
	//bb223240 = IID_IDirect3D3
	if(a==IID_IDirectDraw4) {
		*b=this;
		AddRef();
		return DD_OK;
	} else if(a==IID_IDirect3D3) {
		HRESULT hr=Real->QueryInterface(a,b);
		CHECKFAIL(hr);
		if(FAILED(hr)) return hr;
		*b=new FakeD3D((IDirect3D3*)*b);
		return DD_OK;
	} else UNUSEDFUNCTION;
}
ULONG _stdcall FakeDirectDraw::AddRef()  { return ++Refs; }
ULONG _stdcall FakeDirectDraw::Release() {
	if(--Refs==0) {
		Real->Release();
		dd1->Release();
		delete this;
		return 0;
	} else return Refs; }
// IDirectDraw methods
HRESULT _stdcall FakeDirectDraw::Compact() { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::CreateClipper(DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::CreatePalette(DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE*, IUnknown*) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::CreateSurface(LPDDSURFACEDESC2 a, LPDIRECTDRAWSURFACE4 * b, IUnknown * c) {
	if((a->dwFlags&DDSD_CAPS) && (a->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE))  {
		primary->AddRef();
		*b=(IDirectDrawSurface4*)primary;
		return 0;
	} else if(a->ddsCaps.dwCaps&DDSCAPS_ZBUFFER) {
		if(FUMode) {
			a->dwWidth=gWidth; 
			a->dwHeight=gHeight;
		}
		if(ZBufferBitDepth>=24) {
			a->ddpfPixelFormat.dwZBufferBitDepth=32;
			a->ddpfPixelFormat.dwZBitMask=0xffffff00;
			if(Real->CreateSurface(a,b,c)==DD_OK) return DD_OK;
			a->ddpfPixelFormat.dwZBufferBitDepth=24;
			a->ddpfPixelFormat.dwZBitMask=0xffffff;
		}
		return Real->CreateSurface(a,b,c);
		//CHECKFAIL(hr);
		/*if(FAILED(hr)) {
			CHECKFAIL(hr);
			LOG("%dx%d Caps: 0x%x", a->dwWidth, a->dwHeight, a->ddsCaps.dwCaps);
		}*/
	} else if(TextureReplacement&&a->ddsCaps.dwCaps&DDSCAPS_TEXTURE&&a->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY) {
		OverridenTex* ot=GetOverride();
		HRESULT hr;
		if(!ot) {
			hr=Real->CreateSurface(a,b,c);
			CHECKFAIL(hr);
			if(FAILED(hr)) return hr;
			*b=new FakeVidSurface(*b, 0);
			return DD_OK;
		}
		*b=IsTextureInVRam(ot);
		if(*b) {
			(*b)->AddRef();
		} else {
			hr=Real->CreateSurface(&ot->desc,b,c);
			CHECKFAIL(hr);
			if(FAILED(hr)) return hr;
			*b=new FakeVidSurface(*b, ot);
		}
		return DD_OK;
	} else return Real->CreateSurface(a,b,c);
}
HRESULT _stdcall FakeDirectDraw::DuplicateSurface(LPDIRECTDRAWSURFACE4, LPDIRECTDRAWSURFACE4 *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::EnumDisplayModes(DWORD, LPDDSURFACEDESC2, LPVOID c, LPDDENUMMODESCALLBACK2 d) {
	DDSURFACEDESC2 desc;
	memset(&desc,0, sizeof(desc));
	desc.dwSize=sizeof(desc);
	desc.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
	desc.dwFlags=0x41006;//DDSD_HEIGHT, WIDTH, PIXELFORMAT, REFRESHRATE
	desc.dwRefreshRate=50;
	desc.ddpfPixelFormat.dwFlags=0x40;
	desc.ddpfPixelFormat.dwRGBBitCount=16;
	desc.ddpfPixelFormat.dwRBitMask=0xf800;
	desc.ddpfPixelFormat.dwGBitMask=0x7e0;
	desc.ddpfPixelFormat.dwBBitMask=0x1f;

	desc.dwWidth=640;
	desc.dwHeight=480;
	d(&desc, c);

	if(FUMode) {
		desc.dwWidth=1024;
		desc.dwHeight=768;
		d(&desc, c);
	} else {
		desc.dwWidth=gWidth;
		desc.dwHeight=gHeight;
		d(&desc, c);
	}

	return DD_OK;
}
HRESULT _stdcall FakeDirectDraw::EnumSurfaces(DWORD, LPDDSURFACEDESC2, LPVOID,LPDDENUMSURFACESCALLBACK2) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::FlipToGDISurface() {
	return Real->FlipToGDISurface();
}
HRESULT _stdcall FakeDirectDraw::GetCaps(LPDDCAPS a, LPDDCAPS b) {
	return Real->GetCaps(a,b);
}
HRESULT _stdcall FakeDirectDraw::GetDisplayMode(LPDDSURFACEDESC2 a) {
	a->dwFlags=DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT;
	a->dwHeight=savedHeight;
	a->dwWidth=savedWidth;
	a->ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
	a->ddpfPixelFormat.dwFlags=DDPF_RGB;
	a->ddpfPixelFormat.dwRBitMask=0xf800;
	a->ddpfPixelFormat.dwGBitMask=0x7e0;
	a->ddpfPixelFormat.dwBBitMask=0x1f;
	a->ddpfPixelFormat.dwRGBBitCount=16;
	return DD_OK;
}
HRESULT _stdcall FakeDirectDraw::GetFourCCCodes(LPDWORD,LPDWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::GetGDISurface(LPDIRECTDRAWSURFACE4 *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::GetMonitorFrequency(LPDWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::GetScanLine(LPDWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::GetVerticalBlankStatus(LPBOOL) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::Initialize(GUID *) { UNUSEDFUNCTION; }
DDSURFACEDESC2 dtdesc;
HRESULT _stdcall FakeDirectDraw::RestoreDisplayMode() {
	SAFERELEASE(primary);
	Real->SetDisplayMode(dtdesc.dwWidth, dtdesc.dwHeight, dtdesc.ddpfPixelFormat.dwRGBBitCount, dtdesc.dwRefreshRate, 0);
	//CHECKFAIL(-200);
	return DD_OK;
}
HRESULT _stdcall FakeDirectDraw::SetCooperativeLevel(HWND a, DWORD b) {
	if(b&DDSCL_EXCLUSIVE && !primary) {
#ifdef NONEXCLUSIVE
		Real->SetCooperativeLevel(a,DDSCL_NORMAL);
#else
		memset(&dtdesc, 0, sizeof(dtdesc));
		dtdesc.dwSize=sizeof(dtdesc);
		Real->GetDisplayMode(&dtdesc);
		Real->SetCooperativeLevel(a,b);
		Real->SetDisplayMode(gWidth,gHeight,32,RefreshRate,0);
#endif
		primary=new FakePrimarySurface(Real);
	}
	SetWindowLong(a, GWL_STYLE, 0);
	RECT r;
	r.left=0;
	r.right=gWidth;
	r.top=0;
	r.bottom=gHeight;
	AdjustWindowRect(&r, 0, false);
	r.right-=r.left;
	r.left=0;
	r.bottom-=r.top;
	r.top=0;
#ifdef NONEXCLUSIVE
	SetWindowPos(a, HWND_TOP, 0, 0, r.right, r.bottom,SWP_SHOWWINDOW);
#else
	SetWindowPos(a, HWND_NOTOPMOST, 0, 0, r.right, r.bottom,SWP_SHOWWINDOW);
#endif
	return DD_OK;
}
HRESULT _stdcall FakeDirectDraw::SetDisplayMode(DWORD a, DWORD b, DWORD c, DWORD d, DWORD e) {
	LOG("SetDisplayMode: %dx%dx%d", a, b, c);
	savedHeight=b;
	savedWidth=a;
	primary->SetMenuMode(a==640&&b==480);
	texcount=0;
	return DD_OK;
}
HRESULT _stdcall FakeDirectDraw::WaitForVerticalBlank(DWORD, HANDLE) { UNUSEDFUNCTION; }
//*** Added in the V4 Interface ***
HRESULT _stdcall FakeDirectDraw::GetAvailableVidMem(LPDDSCAPS2, LPDWORD b, LPDWORD c) {
	*b=1024*1024*256;
	*c=1024*1024*256 - texcount*1024;
	texcount++;
	return DD_OK;
}
//*** Added in the V4 Interface ***
HRESULT _stdcall FakeDirectDraw::GetSurfaceFromDC(HDC, LPDIRECTDRAWSURFACE4 *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::RestoreAllSurfaces() { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::TestCooperativeLevel() { UNUSEDFUNCTION; }
HRESULT _stdcall FakeDirectDraw::GetDeviceIdentifier(LPDDDEVICEIDENTIFIER, DWORD ) { UNUSEDFUNCTION; }
