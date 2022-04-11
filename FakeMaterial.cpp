#include "FakeMaterial.h"

#include "FakeD3DDevice.h"

FakeMaterial::FakeMaterial(IDirect3DMaterial3* _real) {
	Refs=1;
	Real=_real;
}
/*** IUnknown methods ***/
HRESULT _stdcall FakeMaterial::QueryInterface(REFIID, LPVOID *) { UNUSEDFUNCTION; }
ULONG _stdcall FakeMaterial::AddRef() { return ++Refs; }
ULONG _stdcall FakeMaterial::Release() { COMMONRELEASE }
/*** IDirect3DMaterial3 methods ***/
HRESULT _stdcall FakeMaterial::SetMaterial(LPD3DMATERIAL a) {
	return Real->SetMaterial(a);
}
HRESULT _stdcall FakeMaterial::GetMaterial(LPD3DMATERIAL) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeMaterial::GetHandle(LPDIRECT3DDEVICE3 a,LPD3DMATERIALHANDLE b) {
	return Real->GetHandle(((FakeD3DDevice*)a)->QueryReal(), b);
}