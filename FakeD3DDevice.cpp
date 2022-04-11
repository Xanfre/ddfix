#include "FakeD3DDevice.h"
#include "FakeVidSurface.h"
#include "TexOverride.h"
#include "PostProcess.h"

#include "main.h"

struct ThiefVertex {
	float x,y,z,w;
	DWORD diffuse;
	DWORD spec;
	float u1, v1;
	float u2, v2;
};

/*HRESULT _stdcall tex_fmt(LPDDPIXELFORMAT lpDDPixFmt, LPVOID lpContext) {
	return 1;
}*/

FakeD3DDevice::FakeD3DDevice(IDirect3DDevice3* real) {
	Refs=1;
	Real=realIDirect3DDevice=real;
	real->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTFP_LINEAR);
	real->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
	real->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
	real->SetTextureStageState(1, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
	real->SetTextureStageState(1, D3DTSS_MINFILTER, D3DTFN_LINEAR);
	if(AnisotropicFiltering) {
		real->SetTextureStageState(0, D3DTSS_MAXANISOTROPY, 16);
		real->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_ANISOTROPIC);
		real->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_ANISOTROPIC);
		if(FUMode) {
			real->SetTextureStageState(1, D3DTSS_MAXANISOTROPY, 16);
			real->SetTextureStageState(1, D3DTSS_MAGFILTER, D3DTFG_ANISOTROPIC);
			real->SetTextureStageState(1, D3DTSS_MINFILTER, D3DTFN_ANISOTROPIC);
			real->SetTextureStageState(1, D3DTSS_MIPFILTER, D3DTFP_LINEAR);
		}
	}
}
IDirect3DDevice3* FakeD3DDevice::QueryReal() { return Real; }
/*** IUnknown methods ***/
HRESULT _stdcall FakeD3DDevice::QueryInterface(REFIID, LPVOID *) { UNUSEDFUNCTION; }
ULONG _stdcall FakeD3DDevice::AddRef() { return ++Refs; }
ULONG _stdcall FakeD3DDevice::Release() { realIDirect3DDevice=NULL; COMMONRELEASE; }
/*** IDirect3DDevice3 methods ***/
static DWORD firstcall=0;
#define STACKOFFSET \
	DWORD offset; \
	if(!firstcall) firstcall=(DWORD)&offset; \
	offset=(DWORD)&offset-firstcall;
HRESULT _stdcall FakeD3DDevice::GetCaps(LPD3DDEVICEDESC a,LPD3DDEVICEDESC b) {
	STACKOFFSET;
	CHECKFUNC(Real->GetCaps(a,b));
}
HRESULT _stdcall FakeD3DDevice::GetStats(LPD3DSTATS) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::AddViewport(LPDIRECT3DVIEWPORT3 a) {
	CHECKFUNC(Real->AddViewport(a));
}
HRESULT _stdcall FakeD3DDevice::DeleteViewport(LPDIRECT3DVIEWPORT3) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::NextViewport(LPDIRECT3DVIEWPORT3,LPDIRECT3DVIEWPORT3*,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK a,LPVOID b) {
	//Real->EnumTextureFormats(tex_fmt, 0);
	CHECKFUNC(Real->EnumTextureFormats(a,b));
}
static bool limbRendered=false, zCleared=false;
DWORD scene=0;
HRESULT _stdcall FakeD3DDevice::BeginScene() {
	scene++;
	limbRendered=zCleared=false;
	postProcess.beginScene();

	// SiO2's idea: fog requires w-friendly projection transform.
	D3DMATRIX m(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0);
	Real->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &m);

	updateAviTextures();

	CHECKFUNC(Real->BeginScene());
}
HRESULT _stdcall FakeD3DDevice::EndScene() {
	CHECKFUNC(Real->EndScene());
}
HRESULT _stdcall FakeD3DDevice::GetDirect3D(LPDIRECT3D3*) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::SetCurrentViewport(LPDIRECT3DVIEWPORT3 a) {
	if(FUMode) {
		D3DVIEWPORT2 vp;
		memset(&vp, 0, sizeof(vp));
		vp.dwSize=sizeof(vp);
		a->GetViewport2(&vp);
		vp.dwWidth=gWidth;
		vp.dwHeight=gHeight;
		a->SetViewport2(&vp);
	}
	CHECKFUNC(Real->SetCurrentViewport(a));
}
HRESULT _stdcall FakeD3DDevice::GetCurrentViewport(LPDIRECT3DVIEWPORT3 *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::SetRenderTarget(LPDIRECTDRAWSURFACE4,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::GetRenderTarget(LPDIRECTDRAWSURFACE4 *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::Begin(D3DPRIMITIVETYPE,DWORD,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::BeginIndexed(D3DPRIMITIVETYPE,DWORD,LPVOID,DWORD,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::Vertex(LPVOID) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::Index(WORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::End(DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::GetRenderState(D3DRENDERSTATETYPE,LPDWORD) { UNUSEDFUNCTION; }
static inline DWORD colorShl(DWORD color, DWORD shift) {
	for(DWORD i=0; i<3; i++) {
		BYTE *bp=(BYTE *)&color;
		DWORD dw=bp[i];
		dw<<=shift;
		if(dw&0xffffff00) dw=255;
		dw&=0xff;
		bp[i]=(BYTE)dw;
	}
	return color;
}
HRESULT _stdcall FakeD3DDevice::SetRenderState(D3DRENDERSTATETYPE a,DWORD b) {

	// Force global fog. That is, prevent Dark from disabling fog.
	if(a==D3DRENDERSTATE_FOGENABLE && !b && fogp.Enable && ((fogp.Global==1 && fogp.Detected) || fogp.Global==2)) {
		DWORD color=0;
		Real->GetRenderState(D3DRENDERSTATE_FOGCOLOR, &color);
		if(color) b=1;
	}

	if(ModulateShift && a==D3DRENDERSTATE_FOGCOLOR) b=colorShl(b, ModulateShift);

	CHECKFUNC(Real->SetRenderState(a,b));
}
HRESULT _stdcall FakeD3DDevice::GetLightState(D3DLIGHTSTATETYPE,LPDWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::SetLightState(D3DLIGHTSTATETYPE,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::SetTransform(D3DTRANSFORMSTATETYPE,LPD3DMATRIX) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::GetTransform(D3DTRANSFORMSTATETYPE,LPD3DMATRIX) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::MultiplyTransform(D3DTRANSFORMSTATETYPE,LPD3DMATRIX) { UNUSEDFUNCTION; }
static void clearZBuffer(IDirect3DDevice3* Real) {
	postProcess.stateSave();
	Real->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS);
	Real->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, 1);
	D3DTLVERTEX v[4];
	v[0]=D3DTLVERTEX(D3DVECTOR(-1, (float)(gHeight), 1 ), 1, 0, 0, 0, 1 );
	v[1]=D3DTLVERTEX(D3DVECTOR(-1, -1, 1 ), 1, 0, 0, 0, 0 );
	v[2]=D3DTLVERTEX(D3DVECTOR((float)(gWidth), (float)(gHeight), 1 ), 1, 0, 0, 1, 1 );
	v[3]=D3DTLVERTEX(D3DVECTOR((float)(gWidth), -1, 1 ), 1, 0, 0, 1, 0 );
	Real->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
	Real->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
	Real->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
	Real->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2);
	Real->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	Real->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
	Real->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	Real->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
	Real->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
	postProcess.stateRestore();
}
HRESULT _stdcall FakeD3DDevice::DrawPrimitive(D3DPRIMITIVETYPE a,DWORD fvf,LPVOID ptr,DWORD count,DWORD e) {
	STACKOFFSET;
	DWORD zfunc;
	Real->GetRenderState(D3DRENDERSTATE_ZFUNC, &zfunc);
	ThiefVertex* tv=(ThiefVertex*)ptr;

	// This makes it easier to adjust all the offsets.
	const DWORD osb=0x2bc;

	// These offsets are hit when drawing UI elements.
	if(!pp.BloomUI && (offset==osb+0x28/*T2*/||offset==osb/*TG*/) && zfunc==D3DCMP_LESSEQUAL && (tv->z*tv->w<.2)) {
		postProcess.bloom();
	}
	// Self-illuminating limb model.
	if(LimbZBufferFix && offset==osb+0x50/*T2*/) {
		if(!zCleared) {
			clearZBuffer(Real);
			zCleared=true;
		}
		Real->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
	}
	/*
	 * In T2, if the limb was rendered and we're back here in DrawPrimitive, the z buffer 
	 * has to be cleared again to render the arrow on top of the bow and the arm.
	 */
	if(limbRendered) {
		clearZBuffer(Real);
		limbRendered=false;
	}
	DWORD stride=32;
	if(fvf&D3DFVF_TEX2) stride=40;
#define VERT(x) ((ThiefVertex*)((DWORD)ptr+x*stride))
	if(FUMode) {
		/*
		 * Track where primitives are rendered so that the corresponding part 
		 * of the overlay can be cleared later.
		 */
		for(DWORD i=0;i<count;i++) {
			tv=VERT(i);
			renderinfo.olr.left=min(renderinfo.olr.left, (LONG)tv->x);
			renderinfo.olr.right=max(renderinfo.olr.right, (LONG)tv->x);
			renderinfo.olr.top=min(renderinfo.olr.top, (LONG)tv->y);
			renderinfo.olr.bottom=max(renderinfo.olr.bottom, (LONG)tv->y);
		}
	} else {
		for(DWORD i=0;i<count;i++) {
			tv=(ThiefVertex*)((DWORD)ptr+i*stride);
			if(tv->z!=.01f && tv->w!=100.0f) {
				tv->x-=.5f;
				tv->y-=.5f;
			}
			if(tv->z>1) tv->z=1.0f;
			if(WaterAlphaFix && offset==osb+0x140) {
				tv->diffuse|=0xff000000;
			}
		}
	}
	if(ModulateShift) {
		DWORD ZWRITEENABLE, ALPHABLENDENABLE;
		Real->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE, &ZWRITEENABLE);
		Real->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, &ALPHABLENDENABLE);
		if(!ZWRITEENABLE) {
			if(ALPHABLENDENABLE) {
				ThiefVertex *tv0=(ThiefVertex*)((DWORD)ptr+0*stride);
				ThiefVertex *tv1=(ThiefVertex*)((DWORD)ptr+1*stride);
				ThiefVertex *tv2=(ThiefVertex*)((DWORD)ptr+2*stride);
				ThiefVertex *tv3=(ThiefVertex*)((DWORD)ptr+3*stride);
				if((tv0->diffuse&0xff000000) && tv0->x==tv3->x && tv1->x==tv2->x && tv0->y==tv1->y && tv2->y==tv3->y) {
					if((tv0->diffuse&0xffffff)==0xffffff) {
						// Star.
						DWORD alpha=(tv0->diffuse>>24)<<ModulateShift;
						if(alpha>255) {
							if(tv0->z!=.01f && tv0->w!=100.0f) {
								float r=(sqrtf((float)alpha/255.0f)-1.0f)*1.5f;
								tv0->x-=r; tv0->y-=r;
								tv1->x+=r; tv1->y-=r;
								tv2->x+=r; tv2->y+=r;
								tv3->x-=r; tv3->y+=r;
							}
							alpha=255;
						}
						alpha<<=24;
						tv0->diffuse=tv1->diffuse=tv2->diffuse=tv3->diffuse=alpha|(tv0->diffuse&0xffffff);
					} else {
						LPDIRECT3DTEXTURE2 tex=NULL;
						Real->GetTexture(0, &tex);
						if(!tex) {
							// Underwater.
							tv0->diffuse=tv1->diffuse=tv2->diffuse=tv3->diffuse=colorShl(tv0->diffuse, ModulateShift);
						}
					}
				}
			} else {
				LPDIRECT3DTEXTURE2 tex;
				DWORD OP, ARG1, ARG2, AOP;
				Real->GetTexture(0, &tex);
				Real->GetTextureStageState(0, D3DTSS_COLOROP, &OP);
				Real->GetTextureStageState(0, D3DTSS_COLORARG1, &ARG1);
				Real->GetTextureStageState(0, D3DTSS_COLORARG2, &ARG2);
				Real->GetTextureStageState(0, D3DTSS_ALPHAOP, &AOP);
				if(!tex && OP==D3DTOP_MODULATE+ModulateShift && ARG1==2 && ARG2==0 && AOP==4) {
					Real->GetTexture(1, &tex);
					Real->GetTextureStageState(1, D3DTSS_COLOROP, &OP);
					Real->GetTextureStageState(1, D3DTSS_COLORARG1, &ARG1);
					Real->GetTextureStageState(1, D3DTSS_COLORARG2, &ARG2);
					Real->GetTextureStageState(1, D3DTSS_ALPHAOP, &AOP);
					if(!tex && OP==1 && ARG1==2 && ARG2==1 && AOP==1) {
						// Sky.
						for(DWORD i=0;i<count;i++) {
							tv=(ThiefVertex*)((DWORD)ptr+i*stride);
							tv->diffuse=colorShl(tv->diffuse, ModulateShift);
						}
					}
				}
			}
		}
	}
	if(FUMode) {
		// Skip lens flare.
		if(scene==4) return DD_OK;
		// Scale vertices to real resolution.
		for(DWORD i=0;i<count;i++) {
			tv=VERT(i);
			tv->x*=FUxmul;
			tv->y*=FUymul;
			tv->x-=.5f;
			tv->y-=.5f;
		}
		DWORD zwrite=1;
		// The sun should be z=0, but nothing small seems to work here.
		const float tol=.5f;
		if(count==4 && VERT(0)->z<tol && VERT(1)->z<tol && VERT(2)->z<tol && VERT(3)->z<tol) {
			Real->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE, &zwrite);
			if(!zwrite) {
				IDirect3DTexture2 *t;
				Real->GetTexture(0, &t);
				if(t) {
					if(offset-osb!=0x80) {
						// Not the sun.
						DWORD addr;
						Real->GetTextureStageState(0, D3DTSS_ADDRESS, &addr);
						Real->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP);
						HRESULT hr=Real->DrawPrimitive(a,fvf,ptr,count,e);
						Real->SetTextureStageState(0, D3DTSS_ADDRESS, addr);
						CHECKFAIL(hr);
						return hr;
					}

					// The sun.
					float ax=(VERT(0)->x+VERT(1)->x+VERT(2)->x+VERT(3)->x)*.25f,
						ay=(VERT(0)->y+VERT(1)->y+VERT(2)->y+VERT(3)->y)*.25f;
					for(DWORD i=0; i<4; i++) {
						VERT(i)->y=(VERT(i)->y-ay)*(FUxmul/FUymul)+ay;
					}
					// Track the position, for the lens flare.
					renderinfo.pos.x=ax;
					renderinfo.pos.y=ay;
					renderinfo.size=max(fabs(VERT(0)->x-ax), fabs(VERT(0)->y-ay));
					// Replace the sun texture.
					if(renderinfo.suntex) {
						VERT(0)->diffuse=VERT(1)->diffuse=VERT(2)->diffuse=VERT(3)->diffuse=0xffffffff;
						postProcess.stateSave();
						Real->SetTexture(0, renderinfo.suntex);
						Real->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
						Real->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
						Real->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
						Real->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
						Real->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
						Real->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
						HRESULT hr=Real->DrawPrimitive(a,fvf,ptr,count,e);
						postProcess.stateRestore();
						CHECKFAIL(hr);
						return hr;
					}
				}
			}
		}
	}
	CHECKFUNC(Real->DrawPrimitive(a,fvf,ptr,count,e));
}
HRESULT _stdcall FakeD3DDevice::DrawIndexedPrimitive(D3DPRIMITIVETYPE type,DWORD fvf,LPVOID ptr,DWORD count,LPWORD e,DWORD f,DWORD g) {
	STACKOFFSET;
	if(LimbZBufferFix) {
		/*
		 * These offsets cover sword, blackjack, bow, carried body and carrying arm. T2 only.
		 */
		if(!limbRendered && (offset==0x2f0 || offset==0x300 || offset==0x388)) {
			clearZBuffer(Real);
			limbRendered=true;
		}
		Real->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
		Real->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, 1);
	}
	if(FUMode) {
		// Plane shadow.
		ThiefVertex* tv=(ThiefVertex*)ptr;
		DWORD stride=32;
		if(fvf&D3DFVF_TEX2) stride=40;
		for(DWORD i=0;i<count;i++) {
			tv=(ThiefVertex*)((DWORD)ptr+i*stride);
			tv->x*=FUxmul;
			tv->y*=FUymul;
			tv->z-=.001f;
		}
		DWORD addr;
		Real->GetTextureStageState(0, D3DTSS_ADDRESS, &addr);
		Real->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP);
		HRESULT hr=Real->DrawIndexedPrimitive(type,fvf,ptr,count,e,f,g);
		Real->SetTextureStageState(0, D3DTSS_ADDRESS, addr);
		CHECKFAIL(hr);
		return hr;
	}
	CHECKFUNC(Real->DrawIndexedPrimitive(type,fvf,ptr,count,e,f,g));
}
HRESULT _stdcall FakeD3DDevice::SetClipStatus(LPD3DCLIPSTATUS) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::GetClipStatus(LPD3DCLIPSTATUS) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::DrawPrimitiveStrided(D3DPRIMITIVETYPE,DWORD,LPD3DDRAWPRIMITIVESTRIDEDDATA,DWORD,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE,DWORD,LPD3DDRAWPRIMITIVESTRIDEDDATA,DWORD,LPWORD,DWORD,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::DrawPrimitiveVB(D3DPRIMITIVETYPE,LPDIRECT3DVERTEXBUFFER,DWORD,DWORD,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE,LPDIRECT3DVERTEXBUFFER,LPWORD,DWORD,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::ComputeSphereVisibility(LPD3DVECTOR,LPD3DVALUE,DWORD,DWORD,LPDWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::GetTexture(DWORD,LPDIRECT3DTEXTURE2 *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::SetTexture(DWORD a,LPDIRECT3DTEXTURE2 b) {
	if(!TextureReplacement) { CHECKFUNC(Real->SetTexture(a,b)); }
	else { CHECKFUNC(Real->SetTexture(a,(b&&((FakeVidTexture*)b)->Refs)?((FakeVidTexture*)b)->QueryReal():0)); }
}
HRESULT _stdcall FakeD3DDevice::GetTextureStageState(DWORD,D3DTEXTURESTAGESTATETYPE,LPDWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakeD3DDevice::SetTextureStageState(DWORD a,D3DTEXTURESTAGESTATETYPE b,DWORD c) {
	if(FUMode&&AnisotropicFiltering&&b==D3DTSS_MIPFILTER) return DD_OK;
	if(AnisotropicFiltering&&(b==D3DTSS_MAXANISOTROPY||b==D3DTSS_MAGFILTER||b==D3DTSS_MINFILTER)) return DD_OK;
	if(ModulateShift && b==D3DTSS_COLOROP && c==D3DTOP_MODULATE) c=D3DTOP_MODULATE+ModulateShift;
	CHECKFUNC(Real->SetTextureStageState(a,b,c));
}
HRESULT _stdcall FakeD3DDevice::ValidateDevice(LPDWORD a) {
	return Real->ValidateDevice(a);
}
