#pragma once
#include "FakeCommon.h"

class FakeD3DDevice : IDirect3DDevice3 {
private:
	ULONG Refs;
	IDirect3DDevice3* Real;
public:
	FakeD3DDevice(IDirect3DDevice3* real);
	IDirect3DDevice3* QueryReal();
    /*** IUnknown methods ***/
    HRESULT _stdcall QueryInterface(REFIID riid, LPVOID * ppvObj);
    ULONG _stdcall AddRef();
    ULONG _stdcall Release();
    /*** IDirect3DDevice3 methods ***/
    HRESULT _stdcall GetCaps(LPD3DDEVICEDESC a,LPD3DDEVICEDESC b);
    HRESULT _stdcall GetStats(LPD3DSTATS);
    HRESULT _stdcall AddViewport(LPDIRECT3DVIEWPORT3 a);
    HRESULT _stdcall DeleteViewport(LPDIRECT3DVIEWPORT3);
    HRESULT _stdcall NextViewport(LPDIRECT3DVIEWPORT3,LPDIRECT3DVIEWPORT3*,DWORD);
    HRESULT _stdcall EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK a,LPVOID b);
    HRESULT _stdcall BeginScene();
    HRESULT _stdcall EndScene();
    HRESULT _stdcall GetDirect3D(LPDIRECT3D3*);
    HRESULT _stdcall SetCurrentViewport(LPDIRECT3DVIEWPORT3 a);
    HRESULT _stdcall GetCurrentViewport(LPDIRECT3DVIEWPORT3 *);
    HRESULT _stdcall SetRenderTarget(LPDIRECTDRAWSURFACE4,DWORD);
    HRESULT _stdcall GetRenderTarget(LPDIRECTDRAWSURFACE4 *);
    HRESULT _stdcall Begin(D3DPRIMITIVETYPE,DWORD,DWORD);
    HRESULT _stdcall BeginIndexed(D3DPRIMITIVETYPE,DWORD,LPVOID,DWORD,DWORD);
    HRESULT _stdcall Vertex(LPVOID);
    HRESULT _stdcall Index(WORD);
    HRESULT _stdcall End(DWORD);
    HRESULT _stdcall GetRenderState(D3DRENDERSTATETYPE,LPDWORD);
    HRESULT _stdcall SetRenderState(D3DRENDERSTATETYPE a,DWORD b);
    HRESULT _stdcall GetLightState(D3DLIGHTSTATETYPE,LPDWORD);
    HRESULT _stdcall SetLightState(D3DLIGHTSTATETYPE,DWORD);
    HRESULT _stdcall SetTransform(D3DTRANSFORMSTATETYPE,LPD3DMATRIX);
    HRESULT _stdcall GetTransform(D3DTRANSFORMSTATETYPE,LPD3DMATRIX);
    HRESULT _stdcall MultiplyTransform(D3DTRANSFORMSTATETYPE,LPD3DMATRIX);
    HRESULT _stdcall DrawPrimitive(D3DPRIMITIVETYPE a,DWORD b,LPVOID c,DWORD d,DWORD e);
    HRESULT _stdcall DrawIndexedPrimitive(D3DPRIMITIVETYPE a,DWORD b,LPVOID c,DWORD d,LPWORD e,DWORD f,DWORD g);
    HRESULT _stdcall SetClipStatus(LPD3DCLIPSTATUS);
    HRESULT _stdcall GetClipStatus(LPD3DCLIPSTATUS);
    HRESULT _stdcall DrawPrimitiveStrided(D3DPRIMITIVETYPE,DWORD,LPD3DDRAWPRIMITIVESTRIDEDDATA,DWORD,DWORD);
    HRESULT _stdcall DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE,DWORD,LPD3DDRAWPRIMITIVESTRIDEDDATA,DWORD,LPWORD,DWORD,DWORD);
    HRESULT _stdcall DrawPrimitiveVB(D3DPRIMITIVETYPE,LPDIRECT3DVERTEXBUFFER,DWORD,DWORD,DWORD);
    HRESULT _stdcall DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE,LPDIRECT3DVERTEXBUFFER,LPWORD,DWORD,DWORD);
    HRESULT _stdcall ComputeSphereVisibility(LPD3DVECTOR,LPD3DVALUE,DWORD,DWORD,LPDWORD);
    HRESULT _stdcall GetTexture(DWORD,LPDIRECT3DTEXTURE2 *);
    HRESULT _stdcall SetTexture(DWORD a,LPDIRECT3DTEXTURE2 b);
    HRESULT _stdcall GetTextureStageState(DWORD,D3DTEXTURESTAGESTATETYPE,LPDWORD);
    HRESULT _stdcall SetTextureStageState(DWORD a,D3DTEXTURESTAGESTATETYPE b,DWORD c);
    HRESULT _stdcall ValidateDevice(LPDWORD a);
};
