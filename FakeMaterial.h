#pragma once
#include "FakeCommon.h"

class FakeMaterial : IDirect3DMaterial3 {
private:
	ULONG Refs;
	IDirect3DMaterial3* Real;
public:
	FakeMaterial(IDirect3DMaterial3* _real);
    /*** IUnknown methods ***/
    HRESULT _stdcall QueryInterface(REFIID riid, LPVOID * ppvObj);
    ULONG _stdcall AddRef();
    ULONG _stdcall Release();
    /*** IDirect3DMaterial3 methods ***/
    HRESULT _stdcall SetMaterial(LPD3DMATERIAL a);
    HRESULT _stdcall GetMaterial(LPD3DMATERIAL);
    HRESULT _stdcall GetHandle(LPDIRECT3DDEVICE3 a,LPD3DMATERIALHANDLE b);
};
