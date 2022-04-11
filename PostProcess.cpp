#include "PostProcess.h"
#include "FakeCommon.h"
#include "FakePrimarySurface.h"

static inline void mkQuad(LPD3DTLVERTEX v, int color=0, int shift=0) {
	float w=(float)(gWidth>>shift), h=(float)(gHeight>>shift);
	v[0]=D3DTLVERTEX(D3DVECTOR(-1, h, 0 ), 0.5, color, 0, 0, 1 );
	v[1]=D3DTLVERTEX(D3DVECTOR(-1, -1, 0 ), 0.5, color, 0, 0, 0 );
	v[2]=D3DTLVERTEX(D3DVECTOR(w, h, 0 ), 0.5, color, 0, 1, 1 );
	v[3]=D3DTLVERTEX(D3DVECTOR(w, -1, 0 ), 0.5, color, 0, 1, 0 );
}

static inline void drawQuad(LPDIRECT3DDEVICE3 d, DWORD color=0, int shift=0, LPDIRECTDRAWSURFACE4 surf=NULL, DWORD alphaenable=1, DWORD srcblend=D3DBLEND_SRCALPHA, DWORD destblend=D3DBLEND_INVSRCALPHA, 
							DWORD colorop=D3DTOP_MODULATE, DWORD alphaop=D3DTOP_SELECTARG2, DWORD magfilter=D3DTFG_POINT, DWORD minfilter=D3DTFN_POINT) {
	LPDIRECT3DTEXTURE2 tex=NULL;
	if(surf) surf->QueryInterface(IID_IDirect3DTexture2, (LPVOID *)&tex);
	d->SetTexture(0, tex);
	d->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, alphaenable);
	d->SetRenderState(D3DRENDERSTATE_SRCBLEND, srcblend);
	d->SetRenderState(D3DRENDERSTATE_DESTBLEND, destblend);
	d->SetTextureStageState(0, D3DTSS_MAGFILTER, magfilter);
	d->SetTextureStageState(0, D3DTSS_MINFILTER, minfilter);
	d->SetTextureStageState(0, D3DTSS_COLOROP,   colorop);
	d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	d->SetTextureStageState(0, D3DTSS_ALPHAOP,   alphaop);
	d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	d->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
	D3DTLVERTEX v[4];
	mkQuad(v, color, shift);
	d->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
}

PostProcess postProcess;

float PostProcess::currentGamma=1.0;

PostProcess::PostProcess() {
	maps=0;
	memset(blooml, 0, sizeof(blooml));
	base=grey=trail=NULL;
	haveScene=doneBloom=doneSaturation=false;
}

void PostProcess::init() {
	if(!fakePrimary) return;
	LPDIRECTDRAWSURFACE4 Real2=fakePrimary->QueryReal();
	if(!Real2 || !haveScene) return;

	if(!maps) {
		DDSURFACEDESC2 desc;

		memset(&desc, 0, sizeof(DDSURFACEDESC2));
		desc.dwSize=sizeof(DDSURFACEDESC2);
		Real2->GetSurfaceDesc(&desc);
		desc.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;

		rDDraw->CreateSurface(&desc, &base, 0);
		rDDraw->CreateSurface(&desc, &trail, 0);

		int i;
		for(i=0; i<maxmaps && desc.dwWidth>=8 && desc.dwHeight>=8; i++, desc.dwWidth/=2, desc.dwHeight/=2)
			rDDraw->CreateSurface(&desc, &blooml[i], 0);
		maps=i;
		
		memset(&desc, 0, sizeof(DDSURFACEDESC2));
		desc.dwSize=sizeof(DDSURFACEDESC2);
		Real2->GetSurfaceDesc(&desc);
		desc.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;
		memset(&desc.ddpfPixelFormat, 0, sizeof(desc.ddpfPixelFormat));
		desc.ddpfPixelFormat.dwSize=sizeof(desc.ddpfPixelFormat);
		desc.ddpfPixelFormat.dwLuminanceBitCount=8;
		desc.ddpfPixelFormat.dwLuminanceBitMask=0xff;
		desc.ddpfPixelFormat.dwFlags=DDPF_LUMINANCE;
		
		rDDraw->CreateSurface(&desc, &grey, 0);
	} else {
		if(base->IsLost()) base->Restore();
		if(trail->IsLost()) trail->Restore();
		if(grey->IsLost()) grey->Restore();
		for(int i=0; i<maps; i++)
			if(blooml[i]->IsLost()) blooml[i]->Restore();
	}
}

void PostProcess::clearTrail(float newAverage) {
	if(trail) {
		DDBLTFX fx;
		memset(&fx, 0, sizeof(DDBLTFX));
		fx.dwSize=sizeof(DDBLTFX);
		trail->Blt(0, 0, 0, DDBLT_WAIT|DDBLT_COLORFILL, &fx);
	}
	averageLevel=newAverage;
}

void PostProcess::stateSave() {
	LPDIRECT3DDEVICE3 real=realIDirect3DDevice;
	memset(&state, 0, sizeof(state));
	real->GetRenderState(D3DRENDERSTATE_ZENABLE, &state.D3DRENDERSTATE_ZENABLE);
	real->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE, &state.D3DRENDERSTATE_ZWRITEENABLE);
	real->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, &state.D3DRENDERSTATE_ALPHABLENDENABLE);
	real->GetRenderState(D3DRENDERSTATE_SRCBLEND, &state.D3DRENDERSTATE_SRCBLEND);
	real->GetRenderState(D3DRENDERSTATE_DESTBLEND, &state.D3DRENDERSTATE_DESTBLEND);
	real->GetRenderState(D3DRENDERSTATE_TEXTUREFACTOR, &state.D3DRENDERSTATE_TEXTUREFACTOR);
	for(int c=0; c<savestages; c++) {
		real->GetTexture(c, &state.stage[c].tex);
		real->GetTextureStageState(c, D3DTSS_MAGFILTER, &state.stage[c].D3DTSS_MAGFILTER);
		real->GetTextureStageState(c, D3DTSS_MINFILTER, &state.stage[c].D3DTSS_MINFILTER);
		real->GetTextureStageState(c, D3DTSS_COLOROP, &state.stage[c].D3DTSS_COLOROP);
		real->GetTextureStageState(c, D3DTSS_COLORARG1, &state.stage[c].D3DTSS_COLORARG1);
		real->GetTextureStageState(c, D3DTSS_COLORARG2, &state.stage[c].D3DTSS_COLORARG2);
		real->GetTextureStageState(c, D3DTSS_ALPHAOP, &state.stage[c].D3DTSS_ALPHAOP);
		real->GetTextureStageState(c, D3DTSS_ALPHAARG1, &state.stage[c].D3DTSS_ALPHAARG1);
		real->GetTextureStageState(c, D3DTSS_ALPHAARG2, &state.stage[c].D3DTSS_ALPHAARG2);
		real->GetTextureStageState(c, D3DTSS_TEXCOORDINDEX, &state.stage[c].D3DTSS_TEXCOORDINDEX);
		real->GetTextureStageState(c, D3DTSS_ADDRESS, &state.stage[c].D3DTSS_ADDRESS);
	}
}

void PostProcess::stateRestore() {
	LPDIRECT3DDEVICE3 real=realIDirect3DDevice;
	real->SetRenderState(D3DRENDERSTATE_ZENABLE, state.D3DRENDERSTATE_ZENABLE);
	real->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, state.D3DRENDERSTATE_ZWRITEENABLE);
	real->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, state.D3DRENDERSTATE_ALPHABLENDENABLE);
	real->SetRenderState(D3DRENDERSTATE_SRCBLEND, state.D3DRENDERSTATE_SRCBLEND);
	real->SetRenderState(D3DRENDERSTATE_DESTBLEND, state.D3DRENDERSTATE_DESTBLEND);
	real->SetRenderState(D3DRENDERSTATE_TEXTUREFACTOR, state.D3DRENDERSTATE_TEXTUREFACTOR);
	for(int c=0; c<savestages; c++) {
		real->SetTexture(c, state.stage[c].tex);
		real->SetTextureStageState(c, D3DTSS_MAGFILTER, state.stage[c].D3DTSS_MAGFILTER);
		real->SetTextureStageState(c, D3DTSS_MINFILTER, state.stage[c].D3DTSS_MINFILTER);
		real->SetTextureStageState(c, D3DTSS_COLOROP, state.stage[c].D3DTSS_COLOROP);
		real->SetTextureStageState(c, D3DTSS_COLORARG1, state.stage[c].D3DTSS_COLORARG1);
		real->SetTextureStageState(c, D3DTSS_COLORARG2, state.stage[c].D3DTSS_COLORARG2);
		real->SetTextureStageState(c, D3DTSS_ALPHAOP, state.stage[c].D3DTSS_ALPHAOP);
		real->SetTextureStageState(c, D3DTSS_ALPHAARG1, state.stage[c].D3DTSS_ALPHAARG1);
		real->SetTextureStageState(c, D3DTSS_ALPHAARG2, state.stage[c].D3DTSS_ALPHAARG2);
		real->SetTextureStageState(c, D3DTSS_TEXCOORDINDEX, state.stage[c].D3DTSS_TEXCOORDINDEX);
		real->SetTextureStageState(c, D3DTSS_ADDRESS, state.stage[c].D3DTSS_ADDRESS);
	}
}

void PostProcess::stateLog() {
	LOGF("D3DRENDERSTATE_ZENABLE=%x", state.D3DRENDERSTATE_ZENABLE);
	LOGF("D3DRENDERSTATE_ZWRITEENABLE=%x", state.D3DRENDERSTATE_ZWRITEENABLE);
	LOGF("D3DRENDERSTATE_ALPHABLENDENABLE=%x", state.D3DRENDERSTATE_ALPHABLENDENABLE);
	LOGF("D3DRENDERSTATE_SRCBLEND=%x", state.D3DRENDERSTATE_SRCBLEND);
	LOGF("D3DRENDERSTATE_DESTBLEND=%x", state.D3DRENDERSTATE_DESTBLEND);
	LOGF("D3DRENDERSTATE_TEXTUREFACTOR=%x", state.D3DRENDERSTATE_TEXTUREFACTOR);
	for(int c=0; c<savestages; c++) {
		LOGF("stage %d:", c);
		LOGF("tex=%p", state.stage[c].tex);
		LOGF("D3DTSS_MAGFILTER=%x", state.stage[c].D3DTSS_MAGFILTER);
		LOGF("D3DTSS_MINFILTER=%x", state.stage[c].D3DTSS_MINFILTER);
		LOGF("D3DTSS_COLOROP=%x", state.stage[c].D3DTSS_COLOROP);
		LOGF("D3DTSS_COLORARG1=%x", state.stage[c].D3DTSS_COLORARG1);
		LOGF("D3DTSS_COLORARG2=%x", state.stage[c].D3DTSS_COLORARG2);
		LOGF("D3DTSS_ALPHAOP=%x", state.stage[c].D3DTSS_ALPHAOP);
		LOGF("D3DTSS_ALPHAARG1=%x", state.stage[c].D3DTSS_ALPHAARG1);
		LOGF("D3DTSS_ALPHAARG2=%x", state.stage[c].D3DTSS_ALPHAARG2);
		LOGF("D3DTSS_TEXCOORDINDEX=%x", state.stage[c].D3DTSS_TEXCOORDINDEX);
		LOGF("D3DTSS_ADDRESS=%x", state.stage[c].D3DTSS_ADDRESS);
	}
}

void PostProcess::updateLevel() {
	if(pp.Enable && fakePrimary && !doneLevel && (pp.DynamicThreshold || pp.DynamicGamma)) {
		doneLevel=true;
		LPDIRECTDRAWSURFACE4 Real2=fakePrimary->QueryReal();
		if(!Real2 || !grey) return;
		HRESULT hr=grey->BltFast(0, 0, Real2, NULL, DDBLTFAST_WAIT);
		CHECKFAIL(hr);
		if(hr) return;
		DDSURFACEDESC2 desc;
		memset(&desc, 0, sizeof(desc));
		desc.dwSize=sizeof(desc);
		hr=grey->Lock(0, &desc, DDLOCK_READONLY|DDLOCK_WAIT, 0);
		CHECKFAIL(hr);
		if(hr) return;
		DWORD x, y, c=0, v=0, r=rand();
		for(y=desc.dwHeight/8+(r%(desc.dwHeight/16)); y<=desc.dwHeight-desc.dwHeight/8; y+=desc.dwHeight/16) {
			for(x=desc.dwWidth/8+((r>>7)%(desc.dwHeight/16)); x<=desc.dwWidth-desc.dwWidth/8; x+=desc.dwHeight/16) {
				v+=*((BYTE *)desc.lpSurface+y*desc.lPitch+x);
				c++;
			}
		}
		grey->Unlock(NULL);
		static __int64 lcount=0;
		__int64 count=getCount();
		float fps=(float)getFreq()/(float)(count-lcount);
		if(fps<2.0f) fps=2.0f;
		lcount=count;
		float amount=powf(1.0f-pp.DynamicDecay, 1.0f/fps);
		averageLevel=averageLevel*amount+(float)v/(float)c*(1.0f-amount);
	}
}
void PostProcess::dynamicGamma(LPDIRECTDRAWSURFACE4 s) {
	if(pp.Enable && pp.DynamicGamma) {
		if(averageLevel<1.0f) averageLevel=1.0f;
		float exp=pp.DynamicGamma*(logf(averageLevel)-3.0f)+currentGamma;
		if(exp<.5f) exp=.5f;
		if(exp>1.5f) exp=1.5f;
		LPDIRECTDRAWGAMMACONTROL pgc=NULL;
		HRESULT hr=s->QueryInterface(IID_IDirectDrawGammaControl, (LPVOID *)&pgc);
		CHECKFAIL(hr);
		if(hr) return;
		DDGAMMARAMP ramp;
		for(int c=0; c<256; c++) {
			ramp.red[c]=ramp.green[c]=ramp.blue[c]=(WORD)(powf((float)c/256.0f, exp)*65536.0f);
		}
		hr=pgc->SetGammaRamp(0, &ramp);
		CHECKFAIL(hr);
		pgc->Release();
	}
}
void PostProcess::bloom() {
	updateLevel();
	if(!pp.Enable || !pp.Bloom || doneBloom || !fakePrimary) return;
	LPDIRECT3DDEVICE3 real=realIDirect3DDevice;
	LPDIRECTDRAWSURFACE4 Real2=fakePrimary->QueryReal();
	if(!real || !Real2 || !haveScene) return;
	doneBloom=true;

	D3DTLVERTEX vert[4];
	DWORD color;
	LPDIRECT3DTEXTURE2 tex=NULL;

	real->EndScene();

	init();
	stateSave();

	DWORD BloomThreshold=pp.BloomThreshold;
	if(pp.DynamicThreshold) {
		float currentLevel=averageLevel*pp.DynamicMult+pp.DynamicAdd;
		if(currentLevel<0) BloomThreshold=0;
		else if(currentLevel>255.0f) BloomThreshold=0xff;
		else BloomThreshold=(DWORD)currentLevel;
		BloomThreshold|=(BloomThreshold<<16)|(BloomThreshold<<8);
	}
	
	real->SetRenderState(D3DRENDERSTATE_ZENABLE, 0);
	// Texture addressing is clamped at the edges.
	real->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP);

	// Copy rgb (base) map out of the current backbuffer.
	base->BltFast(0, 0, Real2, NULL, DDBLTFAST_WAIT);

	// Threshold. Subtract a constant color from the backbuffer.
	real->BeginScene();
	drawQuad(real, 0xff000000|BloomThreshold, 0, base, 0, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA, D3DTOP_SUBTRACT);
	real->EndScene();

	// Copy the result out to a luminance map.
	grey->BltFast(0, 0, Real2, NULL, DDBLTFAST_WAIT);

	if(pp.BloomSaturation<0xff) {
		// Desaturate by blending the luminance map back in.
		real->BeginScene();
		drawQuad(real, ((0xff-pp.BloomSaturation)<<24)|0xffffff, 0, grey);
		real->EndScene();
	}

	if(pp.BloomShift) {
		blooml[0]->BltFast(0, 0, Real2, NULL, DDBLTFAST_WAIT);
		real->BeginScene();
		LPDIRECT3DTEXTURE2 tex=NULL;
		blooml[0]->QueryInterface(IID_IDirect3DTexture2, (LPVOID *)&tex);
		real->SetTexture(0, tex);
		real->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, 0);
		real->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_POINT);
		real->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_POINT);
		real->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		real->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

		real->SetTextureStageState(0, D3DTSS_COLOROP, pp.BloomShift<2 ? D3DTOP_MODULATE2X : D3DTOP_MODULATE4X);

		int stage=1, shift=pp.BloomShift-2;

		while(shift>0) {
			real->SetTextureStageState(stage, D3DTSS_COLOROP, shift<2 ? D3DTOP_MODULATE2X : D3DTOP_MODULATE4X);
			real->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_CURRENT);
			real->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			stage++; shift-=2;
		}

		if(pp.BloomShiftRemodulate) {
			base->QueryInterface(IID_IDirect3DTexture2, (LPVOID *)&tex);
			real->SetTexture(stage, tex);
			real->SetTextureStageState(stage, D3DTSS_COLOROP,   D3DTOP_MODULATE);
			real->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_CURRENT);
			real->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		}

		D3DTLVERTEX v[4];
		mkQuad(v, 0xffffffff);
		real->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
		real->EndScene();
		while(stage--) real->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_DISABLE);
		blooml[0]->BltFast(0, 0, Real2, NULL, DDBLTFAST_WAIT);
	}

	// Adjust bloom intensity by blending in some black.
	drawQuad(real, (0xff-(pp.Bloom&0xff))<<24, 0, NULL, 1, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA, D3DTOP_DISABLE, D3DTOP_DISABLE);

	// Copy the result out to bloom level zero.
	blooml[0]->BltFast(0, 0, Real2, NULL, DDBLTFAST_WAIT);

	// Blur and scale down recursively.
	real->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
	real->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);

	float size=.5, sizemult=(float)(10000-pp.BloomSpread)*.0001f;

	for(int i=1; i<maps; i++) {

		DDSURFACEDESC2 desc;
		memset(&desc, 0, sizeof(desc));
		desc.dwSize=sizeof(desc);
		blooml[i-1]->GetSurfaceDesc(&desc);

		real->BeginScene();

		real->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, 0);
		real->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
		real->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO);
		color=0xff000000;
		size*=sizemult;

		struct VERTEX {
			D3DVALUE sx, sy, sz, rhw;
			D3DCOLOR color;
			struct { 
				D3DVALUE u, v; 
			} t[4];
		} v[4]={{-1.0, (float)desc.dwHeight, 0, .5, color, {{.5f-size, .5f+size}, {.5f-size, .5f+size}, {.5f-size, .5f+size}, {.5f-size, .5f+size}}}, 
				{-1, -1, 0, .5, color, {{.5f-size, .5f-size}, {.5f-size, .5f-size}, {.5f-size, .5f-size}, {.5f-size, .5f-size}}}, 
				{(float)desc.dwWidth, (float)desc.dwHeight, 0, .5, color, {{.5f+size, .5f+size}, {.5f+size, .5f+size}, {.5f+size, .5f+size}, {.5f+size, .5f+size}}}, 
				{(float)desc.dwWidth, -1, 0, .5, color, {{.5f+size, .5f-size}, {.5f+size, .5f-size}, {.5f+size, .5f-size}, {.5f+size, .5f-size}}}};

		const float offset[4][2]={{1.5, .5}, {-.5, 1.5}, {-1.5, -.5}, {.5, -1.5}};

		// Blend in the bloom map several times with some uv offsets.
		for(int c=0; c<4; c++) {
			blooml[i-1]->QueryInterface(IID_IDirect3DTexture2, (LPVOID *)&tex);
			real->SetTexture(c, tex);
			real->SetTextureStageState(c, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP);
			real->SetTextureStageState(c, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
			real->SetTextureStageState(c, D3DTSS_MINFILTER, D3DTFN_LINEAR);
			real->SetTextureStageState(c, D3DTSS_COLOROP,   D3DTOP_ADD);
			real->SetTextureStageState(c, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			real->SetTextureStageState(c, D3DTSS_COLORARG2, D3DTA_CURRENT);
			real->SetTextureStageState(c, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
			real->SetTextureStageState(c, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			real->SetTextureStageState(c, D3DTSS_TEXCOORDINDEX, c);
			for(int s=0; s<4; s++) {
				v[s].t[c].u+=offset[c][0]/(float)(desc.dwWidth);
				v[s].t[c].v+=offset[c][1]/(float)(desc.dwHeight);
			}
		}

		real->SetRenderState(D3DRENDERSTATE_TEXTUREFACTOR, 0xff000000|pp.BloomPersistence);
		real->SetTextureStageState(4, D3DTSS_COLOROP,   D3DTOP_MODULATE);
		real->SetTextureStageState(4, D3DTSS_COLORARG1, D3DTA_TFACTOR);
		real->SetTextureStageState(4, D3DTSS_COLORARG2, D3DTA_CURRENT);
		real->SetTextureStageState(4, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
		real->SetTextureStageState(4, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

		real->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX4, v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);

		real->EndScene();

		// Copy the result out to the next level bloom map.
		RECT r={0, 0, desc.dwWidth, desc.dwHeight};
		blooml[i]->Blt(0, Real2, &r, DDBLTFAST_WAIT, 0);
			
		for(int c=0; c<8; c++) {
			real->SetTexture(c, NULL);
			real->SetTextureStageState(c, D3DTSS_COLOROP, D3DTOP_DISABLE);
			real->SetTextureStageState(c, D3DTSS_MAGFILTER, state.stage[c].D3DTSS_MAGFILTER);
			real->SetTextureStageState(c, D3DTSS_MINFILTER, state.stage[c].D3DTSS_MINFILTER);
			real->SetTextureStageState(c, D3DTSS_TEXCOORDINDEX, 0);
		}
	}

	// Render bloom.

	real->BeginScene();

	real->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, 0);
	real->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
	real->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO);

	// Blend all 8 bloom levels in one pass.
	for(int c=0; c<8 && c+1<maps; c++) {
		blooml[c+1]->QueryInterface(IID_IDirect3DTexture2, (LPVOID *)&tex);
		real->SetTexture(c, tex);
		real->SetTextureStageState(c, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP);
		real->SetTextureStageState(c, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
		real->SetTextureStageState(c, D3DTSS_COLOROP,   D3DTOP_ADD);
		real->SetTextureStageState(c, D3DTSS_COLORARG1, D3DTA_CURRENT);
		real->SetTextureStageState(c, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		real->SetTextureStageState(c, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
		real->SetTextureStageState(c, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	}

	D3DTLVERTEX v[4];
	mkQuad(v, 0xff000000, 0);
	real->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);

	for(int c=0; c<8; c++) {
		real->SetTexture(c, NULL);
		real->SetTextureStageState(c, D3DTSS_COLOROP, D3DTOP_DISABLE);
		real->SetTextureStageState(c, D3DTSS_MAGFILTER, state.stage[c].D3DTSS_MAGFILTER);
		real->SetTextureStageState(c, D3DTSS_MINFILTER, state.stage[c].D3DTSS_MINFILTER);
	}

	if(pp.BloomTrail) {
		// Trailing.
		real->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
		real->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
		real->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
		LPDIRECT3DTEXTURE2 tex=NULL;
		if(trail) trail->QueryInterface(IID_IDirect3DTexture2, (LPVOID *)&tex);
		real->SetTexture(0, tex);
		real->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_POINT);
		real->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_POINT);
		real->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
		real->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		real->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		real->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
		real->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		real->SetTexture(1, NULL);
		real->SetTextureStageState(1, D3DTSS_MAGFILTER, D3DTFG_POINT);
		real->SetTextureStageState(1, D3DTSS_MINFILTER, D3DTFN_POINT);
		real->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_SUBTRACT);
		real->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
		real->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		real->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
		real->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
		real->SetTextureStageState(2, D3DTSS_COLOROP,   D3DTOP_DISABLE);
		real->SetRenderState(D3DRENDERSTATE_TEXTUREFACTOR, 0x010101);
		D3DTLVERTEX v[4];
		mkQuad(v, pp.BloomTrail);
		real->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
	}

	real->EndScene();

	// Update the trail map by copying out the current backbuffer.
	if(pp.BloomTrail) trail->BltFast(0, 0, Real2, NULL, DDBLTFAST_WAIT);

	// Render base.
	real->BeginScene();
	drawQuad(real, 0xff000000|pp.Base, 0, base, 1, D3DBLEND_SRCALPHA, pp.Bloom ? D3DBLEND_ONE : D3DBLEND_ZERO);

	stateRestore();
}

void PostProcess::saturation() {
	if(!pp.Enable || pp.Saturation==0xff || doneSaturation || !fakePrimary) return;
	LPDIRECT3DDEVICE3 real=realIDirect3DDevice;
	LPDIRECTDRAWSURFACE4 Real2=fakePrimary->QueryReal();
	if(!real || !Real2 || !haveScene) return;
	doneSaturation=true;

	init();
	stateSave();
	
	real->SetRenderState(D3DRENDERSTATE_ZENABLE, 0);
	// Texture addressing is clamped at the edges.
	real->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP);

	// Copy luminance (grey) map out of the current backbuffer.
	grey->BltFast(0, 0, Real2, NULL, DDBLTFAST_WAIT);
	
	// Desaturate base by blending in the luminance map.
	real->BeginScene();
	drawQuad(real, ((0xff-pp.Saturation)<<24)|0xffffff, 0, grey);
	real->EndScene();

	// display postprocessing components
	if(0) {
		LPDIRECTDRAWSURFACE4 s[]={base, grey, trail, blooml[0], blooml[1], blooml[2], blooml[3], blooml[4], blooml[5], blooml[6], blooml[7], blooml[8]};
		LPDIRECT3DDEVICE3 d=real;
		LPDIRECT3DTEXTURE2 tex=NULL;
		d->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
		d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO);
		d->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
		d->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFG_LINEAR);
		for(int i=0; i<sizeof(s)/sizeof(s[0]); i++) {
			if(!s[i]) continue;
			D3DTLVERTEX v[4];
			mkQuad(v, 0xffffffff, 2);
			D3DVECTOR t(v[2].sx, v[2].sy, v[2].sz);
			for(int c=0; c<4; c++) {
				v[c].sx=v[c].sx*.98f+t.x*(i%4);
				v[c].sy=v[c].sy*.98f+t.y*(i/4);
			}
			if(i>2) d->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE4X);
			s[i]->QueryInterface(IID_IDirect3DTexture2, (LPVOID *)&tex);
			d->SetTexture(0, tex);
			d->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
		}
	}
	
	stateRestore();
}
