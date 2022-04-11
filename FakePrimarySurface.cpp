#include "FakePrimarySurface.h"
#include "BitmapSaver.h"
#include "FakeVidSurface.h"
#include "PostProcess.h"

#include "main.h"

LPDIRECT3DDEVICE3 realIDirect3DDevice=NULL;
FakePrimarySurface* fakePrimary=NULL;
static bool oddframe=false, fakebb_locked=false;

static inline LPDIRECT3DTEXTURE2 getTex(IDirectDrawSurface4 *s)
{
	LPDIRECT3DTEXTURE2 tex=NULL;
	if(s) s->QueryInterface(IID_IDirect3DTexture2, (LPVOID *)&tex);
	return tex;
}

static inline void drawQuad(LPDIRECT3DDEVICE3 d, DWORD color)
{
	float w=(float)gWidth-.5f, h=(float)gHeight-.5f;
	D3DTLVERTEX v[4];
	v[0]=D3DTLVERTEX(D3DVECTOR(-.5f, h, 0 ), .5, color, 0, 0, 1 );
	v[1]=D3DTLVERTEX(D3DVECTOR(-.5f, -.5f, 0 ), .5, color, 0, 0, 0 );
	v[2]=D3DTLVERTEX(D3DVECTOR(w, h, 0 ), .5, color, 0, 1, 1 );
	v[3]=D3DTLVERTEX(D3DVECTOR(w, -.5f, 0 ), .5, color, 0, 1, 0 );
	d->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
}

renderinfo_t renderinfo={NULL, D3DVECTOR(), 0, {0x10000, 0x10000, -1, -1}};
static int locks=0;
const int fsize=256;

struct {	LPDIRECTDRAWSURFACE4 surf;	union {		void *p;		BYTE *p8;		DWORD *p32;	};} flare[]={	{NULL, NULL},	{NULL, NULL},	{NULL, NULL},	{NULL, NULL},};struct {
	int i;
	float pos, size;
	DWORD color;
} flarecfg[]={
	{0,  1.003f, 2.88f, 0xb4f9e4da},
	{1,  2.078f, 2.88f, 0x55ecd8da},
	{1,  3.010f, 0.22f, 0x737ce46d},
	{1,  3.023f, 0.11f, 0x90f9e46d},
	{0,  0.402f, 0.11f, 0x5af9e46d},
	{0,  0.101f, 0.59f, 0x52c7e4ae},
	{2, -0.402f, 1.13f, 0x52c7e4ae},
	{0, -0.402f, 0.11f, 0x90f9e4ae},
	{1, -0.402f, 2.88f, 0x5af9e4da},
	{0, -1.003f, 0.54f, 0x90f9e4ae},
	{1, -1.054f, 1.08f, 0x36f9e4da},
	{1, -1.066f, 0.11f, 0x90f9e4da},
	{1, -1.205f, 0.36f, 0x2f3172da},
	{1, -1.263f, 0.36f, 0x2f3172da},
	{2, -2.009f, 2.88f, 0x5af9cdc4},
	{2, -2.109f, 1.80f, 0x52e0e4c4},
};

static void GenMipMaps(IDirectDrawSurface4* from) {
	DDSURFACEDESC2 desc;
	memset(&desc, 0, sizeof(desc));
	desc.dwSize=sizeof(desc);
	from->GetSurfaceDesc(&desc);
	if(desc.dwMipMapCount>1) {
		DDSCAPS2 caps = { DDSCAPS_MIPMAP,0,0,0 };
		from->AddRef();
		IDirectDrawSurface4* to;
		for(DWORD i=1;i<desc.dwMipMapCount;i++) {
			from->GetAttachedSurface(&caps, &to);
			to->Blt(0, from, 0, DDBLT_WAIT, 0);
			from->Release();
			from=to;
		}
		to->Release();
	}
}

void prepareFlareData(int i)
{
	flare[i].p=new char[fsize*fsize*((i==0||i==3) ? 4 : 1)];
	for(int y=0; y<fsize; y++) {
		for(int x=0; x<fsize; x++) {
			float cx=(float)(x*2+1-fsize)/(float)fsize, cy=(float)(y*2+1-fsize)/(float)fsize;
			float d2=cx*cx+cy*cy, d=sqrt(d2);
			switch(i) {
			case 0: 
				{
					float r, g, b;
					r=expf(-(d2*4.0f))-expf(-4.0f);
					r=max(0, min(1, r));
					g=b=r;
					float a=expf(-(powf((1.0f-.95f)*25.0f, 2.0f)));
					r+=max(0, expf(-(powf((d-.95f)*25.0f, 2.0f)))-a)/(1.0f-a)*.3f;
					g+=max(0, expf(-(powf((d-.95f)*22.0f, 2.0f)))-a)/(1.0f-a)*.1f;
					b+=max(0, expf(-(powf((d-.90f)*15.0f, 2.0f)))-a)/(1.0f-a);
					r=min(1, r);
					g=min(1, g);
					b=min(1, b);
					flare[i].p32[y*fsize+x]=D3DRGBA(r, g, b, 1);
				}
				break;
			case 1:
				{
					float f=1;
					if(d<.76) f=(d-.74f)/.02f;
					f=max(0, min(1, f));
					flare[i].p8[y*fsize+x]=(BYTE)((1.0f-f)*238.0f);
				}
				break;
			case 2:
				{
					const float low=50.0f/255.0f, high=222.0f/255.0f;
					float f=0;
					if(d<.74f) 
						f=pow((d/.74f), 2.0f)*(high-low)+low;
					else if(d<.76)
						f=(1.0f-(d-.74f)/.02f)*high;
					f=max(0, min(1, f));
					flare[i].p8[y*fsize+x]=(BYTE)(f*255.0f);
				}
				break;
			case 3: 
				{
					float r, g, b, a;
					a=max(0, (1.0f-d))*5.0f;
					a*=a;
					r=max(0, min(1, 2.0f*a));
					g=max(0, min(1, 1.7f*a));
					b=max(0, min(1, 1.0f*a));
					flare[i].p32[y*fsize+x]=D3DRGBA(r, g, b, 1);
				}
				break;
			}
		}
	}
}

void restoreFlare(int i, bool force=false)
{
	if(!flare[i].surf || (!force && !flare[i].surf->IsLost())) return;
	flare[i].surf->Restore();
	DDSURFACEDESC2 ldesc;
	memset(&ldesc, 0, sizeof(DDSURFACEDESC2));
	ldesc.dwSize=sizeof(DDSURFACEDESC2);
	flare[i].surf->Lock(NULL, &ldesc, DDLOCK_WAIT|DDLOCK_WRITEONLY, 0);
	if(ldesc.lpSurface) {
		for(int y=0; y<fsize; y++) {
			memcpy((BYTE *)ldesc.lpSurface+y*ldesc.lPitch, flare[i].p8+ldesc.ddpfPixelFormat.dwRGBBitCount/8*fsize*y, ldesc.ddpfPixelFormat.dwRGBBitCount/8*fsize);
		}
	}
	flare[i].surf->Unlock(NULL);
	GenMipMaps(flare[i].surf);
}

void createFlares()
{
	DDSURFACEDESC2 desc;
	memset(&desc, 0, sizeof(DDSURFACEDESC2));
	desc.dwSize=sizeof(DDSURFACEDESC2);
	desc.dwFlags=DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT;
	desc.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;
	desc.dwWidth=desc.dwHeight=fsize;
	desc.ddpfPixelFormat.dwSize=sizeof(desc.ddpfPixelFormat);
	desc.ddpfPixelFormat.dwFlags=DDPF_LUMINANCE;
	desc.ddpfPixelFormat.dwLuminanceBitCount=8;
	desc.ddpfPixelFormat.dwLuminanceBitMask=0xff;
	desc.dwFlags|=DDSD_MIPMAPCOUNT;
	desc.ddsCaps.dwCaps|=DDSCAPS_COMPLEX|DDSCAPS_MIPMAP;
	desc.dwMipMapCount=8;
	for(int i=0; i<sizeof(flare)/sizeof(flare[0]); i++) {
		switch(i) {
		case 0: case 3:
			desc.ddpfPixelFormat.dwFlags=DDPF_ALPHAPIXELS|DDPF_RGB;
			desc.ddpfPixelFormat.dwRGBBitCount=32;
			desc.ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000;
			desc.ddpfPixelFormat.dwRBitMask=0xff0000;
			desc.ddpfPixelFormat.dwGBitMask=0xff00;
			desc.ddpfPixelFormat.dwBBitMask=0xff;
			break;
		default:
			desc.ddpfPixelFormat.dwFlags=DDPF_LUMINANCE;
			desc.ddpfPixelFormat.dwLuminanceBitCount=8;
			desc.ddpfPixelFormat.dwLuminanceBitMask=0xff;
		}
		rDDraw->CreateSurface(&desc, &flare[i].surf, 0);
		prepareFlareData(i);
		restoreFlare(i, true);
	}
	flare[3].surf->QueryInterface(IID_IDirect3DTexture2, (LPVOID *)&renderinfo.suntex);
}

static const int flareBufSize=12;
static float flareBuf[flareBufSize]={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static DWORD flareBufPos=0;
float getFlareAmount(IDirectDrawSurface4* Real2)
{
	float val=0;
	if(Real2 && realIDirect3DDevice) {
		DDSURFACEDESC2 desc;
		memset(&desc, 0, sizeof(desc));
		desc.dwSize=sizeof(desc);
		Real2->Lock(0, &desc, DDLOCK_READONLY|DDLOCK_WAIT, 0);

		static int num=0;
		static D3DVECTOR sso[256];
		if(!num) {
			const float step=.2f, jitter=step*.5f;
			int row=0;
			for(float y=-1.0f; y<=1.0f; y+=step) {
				for(float x=-1.0f; x<=1.0f; x+=step) {
					float cx=x+(float)rand()/(float)RAND_MAX*jitter+((row%2) ? 0 : step*.5f),
						cy=y+(float)rand()/(float)RAND_MAX*jitter;
					if(cx*cx+cy*cy<=1.0f) {
						sso[num++]=D3DVECTOR(cx, cy, 0);
					}
				}
				row++;
			}
		}
		DWORD buf[256];
		memset(buf, 0, 256*4);
		int count=0;
		for(int i=0; i<num; i++) {
			float a=(float)i/(float)num*2.0f*3.1416f, d=renderinfo.size*.75f;
			float x=renderinfo.pos.x+cos(a)*d+.5f, y=renderinfo.pos.y+sin(a)*d+.5f;
			x=renderinfo.pos.x+sso[i].x*d+.5f; y=renderinfo.pos.y+sso[i].y*d+.5f;
			if(x>=0 && x<desc.dwWidth && y>=0 && y<desc.dwHeight) {
				DWORD *p=((DWORD *)((BYTE *)desc.lpSurface+(DWORD)y*desc.lPitch))+(DWORD)x;
				buf[i]=*p;
				if((*p&0xe000)==0xe000)
					count+=(*p>>8)&0x1f;
			}
		}
		Real2->Unlock(NULL);
		val=(float)count/(float)(num*31);
	}
	flareBuf[flareBufPos]=val;
	flareBufPos=(flareBufPos+1)%flareBufSize;
	val=0;
	for(int i=0; i<flareBufSize; i++) val+=flareBuf[i];
	val/=flareBufSize;
	return val;
}

void renderFlares(IDirectDrawSurface4* Real2)
{
	if(!flare[0].surf) createFlares();
	float flare_amount=getFlareAmount(Real2);
	if(!realIDirect3DDevice || !rDDraw || renderinfo.size==0) return;
	// Took a while for me to notice that originally this is actually constant.
	renderinfo.size=(float)gWidth*.02778f;
	LPDIRECT3DDEVICE3 d=realIDirect3DDevice;
	LPDIRECT3DTEXTURE2 tex=NULL;

	postProcess.stateSave();
	d->BeginScene();
	d->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS);
	d->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, 0);
	d->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
	d->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
	d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);

	d->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
	d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
	d->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
	d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	d->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);

	DWORD color;
	D3DVECTOR c((float)gWidth*.5f, (float)gHeight*.5f, 0);
	D3DVECTOR sd=renderinfo.pos-c;
	D3DVECTOR nsp=sd;
	nsp.x/=(float)gWidth*.5f;
	nsp.y/=(float)gHeight*.5f;
	float posmul=1.0f-Magnitude(nsp);
	posmul=max(0, posmul);

	for(int i=0; i<4; i++) restoreFlare(i);

	// Lens flare.
	for(int i=0; i<sizeof(flarecfg)/sizeof(flarecfg[0])-1; i++) {
		d->SetTexture(0, getTex(flare[flarecfg[i].i].surf));
		color=flarecfg[i].color;
		color=(((DWORD)((float)(color>>24)*flare_amount*posmul*.5f))<<24)|(color&0xffffff);
		float size=flarecfg[i].size*renderinfo.size;
		D3DVECTOR t=c+sd*flarecfg[i].pos;
		D3DTLVERTEX v[4];
		v[0]=D3DTLVERTEX(t+D3DVECTOR(-size, size, 0 ), .5, color, 0, 0, 1 );
		v[1]=D3DTLVERTEX(t+D3DVECTOR(-size, -size, 0 ), .5, color, 0, 0, 0 );
		v[2]=D3DTLVERTEX(t+D3DVECTOR(size, size, 0 ), .5, color, 0, 1, 1 );
		v[3]=D3DTLVERTEX(t+D3DVECTOR(size, -size, 0 ), .5, color, 0, 1, 0 );
		d->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
	}

	// Display flare maps.
	if(0) {
		d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO);
		for(int i=0; i<sizeof(flare)/sizeof(flare[0]); i++) {
			d->SetTexture(0, getTex(flare[flarecfg[i].i].surf));
			color=0xffffffff;
			float size=128.0f;
			D3DVECTOR t=D3DVECTOR(size+size*2*i, size, 0);
			D3DTLVERTEX v[4];
			v[0]=D3DTLVERTEX(t+D3DVECTOR(-size, size, 0 ), .5, color, 0, 0, 1 );
			v[1]=D3DTLVERTEX(t+D3DVECTOR(-size, -size, 0 ), .5, color, 0, 0, 0 );
			v[2]=D3DTLVERTEX(t+D3DVECTOR(size, size, 0 ), .5, color, 0, 1, 1 );
			v[3]=D3DTLVERTEX(t+D3DVECTOR(size, -size, 0 ), .5, color, 0, 1, 0 );
			d->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
		}
	}

	// Glare.
	d->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
	d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
	d->SetTexture(0, NULL);
	d->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
	d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	d->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
	d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	d->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
	color=0xe2f9e5db;
	color=(((DWORD)((float)(color>>24)*flare_amount*posmul))<<24)|(color&0xffffff);
	drawQuad(d, color);
	d->EndScene();
	postProcess.stateRestore();
	renderinfo.pos=D3DVECTOR(0, 0, 0);
}

FakePrimarySurface::FakePrimarySurface(IDirectDraw4* ddraw) {
	Refs=1;
	MenuMode=true;
	HRESULT hr;

	memset(&rDesc, 0, sizeof(DDSURFACEDESC2));
	rDesc.dwSize=sizeof(DDSURFACEDESC2);
	rDesc.dwFlags=DDSD_CAPS;
#ifdef NONEXCLUSIVE
	rDesc.ddsCaps.dwCaps=DDSCAPS_VIDEOMEMORY|DDSCAPS_PRIMARYSURFACE;
#else
	rDesc.dwFlags|=DDSD_BACKBUFFERCOUNT;
	rDesc.ddsCaps.dwCaps=DDSCAPS_VIDEOMEMORY|DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX|DDSCAPS_3DDEVICE;
	rDesc.dwBackBufferCount=TripleBuffer ? 2 : 1;
#endif

	hr=ddraw->CreateSurface(&rDesc, &Real, 0);
	CHECKFAIL(hr);
	hr=Real->GetSurfaceDesc(&rDesc);
	CHECKFAIL(hr);

#ifdef NONEXCLUSIVE
	DDSURFACEDESC2 desc=rDesc;
	desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
	desc.ddsCaps.dwCaps=DDSCAPS_VIDEOMEMORY|DDSCAPS_3DDEVICE;
	desc.dwWidth=gWidth;
	desc.dwHeight=gHeight;
	desc.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
	desc.ddpfPixelFormat.dwFlags=DDPF_RGB;
	desc.ddpfPixelFormat.dwRBitMask=0xff0000;
	desc.ddpfPixelFormat.dwGBitMask=0xff00;
	desc.ddpfPixelFormat.dwBBitMask=0xff;
	desc.ddpfPixelFormat.dwRGBBitCount=32;

	hr=ddraw->CreateSurface(&desc, &Real2, 0);
#else
	DDSCAPS2 caps;
	memset(&caps, 0, sizeof(DDSCAPS2));
	caps.dwCaps=DDSCAPS_BACKBUFFER;
	hr=Real->GetAttachedSurface(&caps, &Real2);
	CHECKFAIL(hr);

	rDesc.dwFlags|=DDSD_BACKBUFFERCOUNT;
	rDesc.ddsCaps.dwCaps=DDSCAPS_VIDEOMEMORY|DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX|DDSCAPS_3DDEVICE;
	rDesc.dwBackBufferCount=1;
#endif

	rDesc.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
	rDesc.ddpfPixelFormat.dwFlags=DDPF_RGB;
	rDesc.ddpfPixelFormat.dwRBitMask=0xf800;
	rDesc.ddpfPixelFormat.dwGBitMask=0x7e0;
	rDesc.ddpfPixelFormat.dwBBitMask=0x1f;
	rDesc.ddpfPixelFormat.dwRGBBitCount=16;

	memset(&mDesc, 0, sizeof(DDSURFACEDESC2));
	mDesc.dwSize=sizeof(DDSURFACEDESC2);
	mDesc.dwFlags=DDSD_CAPS|DDSD_PIXELFORMAT|DDSD_HEIGHT|DDSD_WIDTH;
	if(UseSysMemOverlay==1) {
		mDesc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;
	} else {
		mDesc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
	}
	mDesc.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
	mDesc.ddpfPixelFormat.dwFlags=DDPF_RGB;
	mDesc.ddpfPixelFormat.dwRBitMask=0xf800;
	mDesc.ddpfPixelFormat.dwGBitMask=0x7e0;
	mDesc.ddpfPixelFormat.dwBBitMask=0x1f;
	mDesc.ddpfPixelFormat.dwRGBBitCount=16;
	mDesc.dwWidth=640;
	mDesc.dwHeight=480;

	oDesc=mDesc;
	oDesc.dwWidth=gWidth;
	oDesc.dwHeight=gHeight;
	if(FUMode) {
		oDesc.dwWidth=1024;
		oDesc.dwHeight=768;
	}
	if(UseSysMemOverlay==2) oDesc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;

	hr=ddraw->CreateSurface(&mDesc, &menu, 0);
	CHECKFAIL(hr);
	hr=menu->GetSurfaceDesc(&mDesc);
	CHECKFAIL(hr);
	mDesc.ddsCaps.dwCaps|=DDSCAPS_VIDEOMEMORY|DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
	mDesc.dwBackBufferCount=1;
	mDesc.dwFlags|=DDSD_BACKBUFFERCOUNT;

	/*
	 * FU3 assumes double-buffering, where the overlay graphics are retained, 
	 * so we need two overlay surfaces.
	 */
	oDesc.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;
	hr=ddraw->CreateSurface(&oDesc, &overlay[0], 0);
	CHECKFAIL(hr);
	hr=ddraw->CreateSurface(&oDesc, &overlay[1], 0);
	CHECKFAIL(hr);
	hr=ddraw->CreateSurface(&oDesc, &fakebb, 0);
	CHECKFAIL(hr);
	oDesc.ddpfPixelFormat.dwFlags=DDPF_ALPHAPIXELS|DDPF_RGB;
	oDesc.ddpfPixelFormat.dwRGBBitCount=32;
	oDesc.ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000;
	oDesc.ddpfPixelFormat.dwRBitMask=0xff0000;
	oDesc.ddpfPixelFormat.dwGBitMask=0xff00;
	oDesc.ddpfPixelFormat.dwBBitMask=0xff;
	hr=ddraw->CreateSurface(&oDesc, &olc, 0);
	CHECKFAIL(hr);
	hr=ddraw->CreateSurface(&oDesc, &ola, 0);
	CHECKFAIL(hr);

	hr=overlay[0]->GetSurfaceDesc(&oDesc);
	CHECKFAIL(hr);

	ClearOverlay();

	if(UseSysMemOverlay!=2) {
		/*
		 * Turns out that BltFast source color key doesn't always work the way it should.
		 * The following is a workaround to that problem.
		 */

		// Fill the back buffer with black.
		DDBLTFX fx;
		memset(&fx, 0, sizeof(DDBLTFX));
		fx.dwSize=sizeof(DDBLTFX);
		HRESULT hr=Real2->Blt(0, 0, 0, DDBLT_WAIT|DDBLT_COLORFILL, &fx);
		CHECKFAIL(hr);

		// Set the overlay color key.
		DDCOLORKEY key;
		key.dwColorSpaceHighValue=OverlayColourKey;
		key.dwColorSpaceLowValue=OverlayColourKey;
		hr=overlay[0]->SetColorKey(DDCKEY_SRCBLT, &key);
		CHECKFAIL(hr);

		// Blit the overlay to the back buffer. Overlay has already been filled with the key color.
		BltOverlay();

		// Lock the back buffer and get the color of the first pixel.
		DDSURFACEDESC2 desc;
		memset(&desc, 0, sizeof(desc));
		desc.dwSize=sizeof(desc);
		hr=Real2->Lock(0, &desc, DDLOCK_READONLY|DDLOCK_WAIT, 0);
		CHECKFAIL(hr);
		DWORD ActualOverlayColourKey=*((DWORD *)desc.lpSurface);
		Real2->Unlock(NULL);

		// If the back buffer was still black, the color key worked and we can use it,
		// otherwise we use the back buffer color as the color key.
		if(!ActualOverlayColourKey) ActualOverlayColourKey=OverlayColourKey;

		// Set the actual overlay color key.
		key.dwColorSpaceHighValue=ActualOverlayColourKey;
		key.dwColorSpaceLowValue=ActualOverlayColourKey;
		hr=overlay[0]->SetColorKey(DDCKEY_SRCBLT, &key);
		hr=overlay[1]->SetColorKey(DDCKEY_SRCBLT, &key);
		CHECKFAIL(hr);
	}
	oDesc.ddsCaps.dwCaps|=DDSCAPS_VIDEOMEMORY|DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
	oDesc.dwBackBufferCount=1;
	oDesc.dwFlags|=DDSD_BACKBUFFERCOUNT;

	fakePrimary=this;
}
void FakePrimarySurface::ClearOverlay(LPRECT dr) {
	DDBLTFX fx;
	memset(&fx, 0, sizeof(DDBLTFX));
	fx.dwSize=sizeof(DDBLTFX);
	fx.dwFillColor=OverlayColourKey;
	HRESULT hr=overlay[oddframe]->Blt(dr, 0, 0, DDBLT_WAIT|DDBLT_COLORFILL, &fx);
	CHECKFAIL(hr);
}
void FakePrimarySurface::SetMenuMode(bool b) { 
	MenuMode=b;
	WidescreenSetMenuMode(b);
	GetAsyncKeyState(VK_SNAPSHOT);
	if(b && pp.Enable && pp.DynamicGamma) {
		/*
		 * If dynamic gamma is in use, we need to revert to non-dynamic gamma 
		 * for the menu screens.
		 */
		LPDIRECTDRAWGAMMACONTROL pgc=NULL;
		HRESULT hr=Real->QueryInterface(IID_IDirectDrawGammaControl, (LPVOID *)&pgc);
		CHECKFAIL(hr);
		if(hr) return;
		DDGAMMARAMP ramp;
		for(int c=0; c<256; c++) {
			ramp.red[c]=ramp.green[c]=ramp.blue[c]=(WORD)(powf((float)c/256.0f, postProcess.currentGamma)*65536.0f);
		}
		hr=pgc->SetGammaRamp(0, &ramp);
		CHECKFAIL(hr);
		pgc->Release();
	}
}
IDirectDrawSurface4* FakePrimarySurface::QueryReal() { return Real2; }
class FakeGammaControl : IDirectDrawGammaControl {
private:
	ULONG Refs;
	IDirectDrawGammaControl* Real;
public:
	FakeGammaControl(IDirectDrawGammaControl* real) { Refs=1; Real=real; }
	IDirectDrawGammaControl* QueryReal() { return Real; }
	// IUnknown methods
	HRESULT _stdcall QueryInterface(REFIID riid, LPVOID * ppvObj) { UNUSEDFUNCTION; }
	ULONG _stdcall AddRef()  { return ++Refs; }
	ULONG _stdcall Release() {
		if(!--Refs) {
			if(Real) Real->Release();
			delete this;
			return 0;
		} else return Refs;
	}
	// IDirectDrawGammaControl methods
	HRESULT _stdcall GetGammaRamp(DWORD a, LPDDGAMMARAMP b) { UNUSEDFUNCTION; }
	HRESULT _stdcall SetGammaRamp(DWORD a, LPDDGAMMARAMP b) {
		// Calculate the gamma being set and update to PostProcess.
		postProcess.currentGamma=logf(b->red[128]/65536.0f)/logf(.5f);
		if(Real) return Real->SetGammaRamp(a, b);
		return DD_OK;
	}
};
// IUnknown methods
HRESULT _stdcall FakePrimarySurface::QueryInterface(REFIID a, LPVOID * b) {
	//69c11c3e = IID_IDirectDrawGammaControl
	//93281502 = IID_IDirect3DTexture2
	if(a==IID_IDirectDrawGammaControl) {
		*b=NULL;
		if(!EnableGamma) return E_NOINTERFACE;
		if(!(pp.Enable && pp.DynamicGamma)) {
			// Normal gamma control, return real interface.
			CHECKFUNC(Real->QueryInterface(a,b));
		}
		// Dynamic gamma control, return fake interface.
		*b=(LPVOID)new FakeGammaControl((IDirectDrawGammaControl *)*b);
		return DD_OK;
	} else UNUSEDFUNCTION;
}
ULONG _stdcall FakePrimarySurface::AddRef()  { return ++Refs; }
ULONG _stdcall FakePrimarySurface::Release() {
	if(!--Refs) {
		menu->Release();
		Real->Release();
		//Real2->Release(); //Complex surfaces release all resources on exit
		delete this;
		return 0;
	} else return Refs;
}
// IDirectDrawSurface methods
HRESULT _stdcall FakePrimarySurface::AddAttachedSurface(LPDIRECTDRAWSURFACE4 a) {
	return Real2->AddAttachedSurface(a);
}
HRESULT _stdcall FakePrimarySurface::AddOverlayDirtyRect(LPRECT) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::Blt(LPRECT,LPDIRECTDRAWSURFACE4, LPRECT,DWORD, LPDDBLTFX) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::BltBatch(LPDDBLTBATCH, DWORD, DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::BltFast(DWORD,DWORD,LPDIRECTDRAWSURFACE4, LPRECT,DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::DeleteAttachedSurface(DWORD a,LPDIRECTDRAWSURFACE4 b) {
	return Real2->DeleteAttachedSurface(a,b);
}
HRESULT _stdcall FakePrimarySurface::EnumAttachedSurfaces(LPVOID,LPDDENUMSURFACESCALLBACK2) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::EnumOverlayZOrders(DWORD,LPVOID,LPDDENUMSURFACESCALLBACK2) { UNUSEDFUNCTION; }

#define BITS16TO32(a) ( (((a)&0xf800)<<8) + (((a)&0x7e0)<<5) + (((a)&0x1f)<<3) )

void FakePrimarySurface::BltOverlay() {
	HRESULT hr;
	if(UseSysMemOverlay==2) {
		DDSURFACEDESC2 realdesc;
		DDSURFACEDESC2 overdesc;
		realdesc.dwSize=overdesc.dwSize=sizeof(DDSURFACEDESC2);
		hr=overlay[oddframe]->Lock(0, &overdesc, DDLOCK_READONLY|DDLOCK_WAIT, 0);
		hr=Real2->Lock(0, &realdesc, DDLOCK_WAIT, 0);
		realdesc.lPitch/=4;
		overdesc.lPitch/=2;
		for(DWORD y=0;y<gHeight;y++) {
			for(DWORD x=0;x<gWidth;x++) {
				if(((WORD*)overdesc.lpSurface)[y*overdesc.lPitch+x]!=OverlayColourKey)
					((DWORD*)realdesc.lpSurface)[y*realdesc.lPitch+x]=BITS16TO32(((WORD*)overdesc.lpSurface)[y*overdesc.lPitch+x]);
			}
		}
		hr=Real2->Unlock(0);
		hr=overlay[oddframe]->Unlock(0);
	} else {
		if(FUMode && realIDirect3DDevice) {

			postProcess.stateSave();

			LPDIRECT3DDEVICE3 d=realIDirect3DDevice;

			static LPDIRECTDRAWSURFACE4 base=NULL;
			if(!base) {
				DDSURFACEDESC2 desc;
				memset(&desc, 0, sizeof(DDSURFACEDESC2));
				desc.dwSize=sizeof(DDSURFACEDESC2);
				Real2->GetSurfaceDesc(&desc);
				desc.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;
				rDDraw->CreateSurface(&desc, &base, 0);
			}
			if(base->IsLost()) base->Restore();
			base->Blt(NULL, Real2, 0, DDBLT_WAIT, 0);

			d->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS);
			d->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, 0);
			d->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
			d->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
			d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
			for(int i=0; i<7; i++) {
				d->SetTextureStageState(i, D3DTSS_TEXCOORDINDEX, 0);
				d->SetTextureStageState(i, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP);
				d->SetTextureStageState(i, D3DTSS_MAGFILTER, D3DTFG_POINT);
				d->SetTextureStageState(i, D3DTSS_MINFILTER, D3DTFN_POINT);
			}

			d->SetTexture(0, getTex(overlay[oddframe]));
			d->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_DOTPRODUCT3);
			d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			d->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
			d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

			d->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
			d->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			d->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_SUBTRACT);
			d->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
			d->SetRenderState(D3DRENDERSTATE_TEXTUREFACTOR, 0xfefefefe);
			d->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

			d->SetTextureStageState(2, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
			d->SetTextureStageState(2, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			d->SetTextureStageState(2, D3DTSS_ALPHAOP,   D3DTOP_MODULATE4X);
			d->SetTextureStageState(2, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
			d->SetTextureStageState(2, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

			d->SetTextureStageState(3, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
			d->SetTextureStageState(3, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			d->SetTextureStageState(3, D3DTSS_ALPHAOP,   D3DTOP_MODULATE4X);
			d->SetTextureStageState(3, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
			d->SetTextureStageState(3, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

			d->SetTextureStageState(4, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
			d->SetTextureStageState(4, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			d->SetTextureStageState(4, D3DTSS_ALPHAOP,   D3DTOP_MODULATE4X);
			d->SetTextureStageState(4, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
			d->SetTextureStageState(4, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

			d->SetTextureStageState(5, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
			d->SetTextureStageState(5, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			d->SetTextureStageState(5, D3DTSS_ALPHAOP,   D3DTOP_MODULATE4X);
			d->SetTextureStageState(5, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
			d->SetTextureStageState(5, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

			d->SetTexture(6, getTex(overlay[oddframe]));
			d->SetTextureStageState(6, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
			d->SetTextureStageState(6, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			d->SetTextureStageState(6, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
			d->SetTextureStageState(6, D3DTSS_ALPHAARG1, D3DTA_CURRENT|D3DTA_COMPLEMENT);

			d->SetTextureStageState(7, D3DTSS_COLOROP,   D3DTOP_DISABLE);

			d->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
			d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO);

			{
				DWORD color=0xffb048b0;
				float w=1024.0f-.5f, h=768.0f-.5f;
				D3DTLVERTEX v[4];
				v[0]=D3DTLVERTEX(D3DVECTOR(-.5f, h, 0 ), .5, color, 0, 0, 1 );
				v[1]=D3DTLVERTEX(D3DVECTOR(-.5f, -.5f, 0 ), .5, color, 0, 0, 0 );
				v[2]=D3DTLVERTEX(D3DVECTOR(w, h, 0 ), .5, color, 0, 1, 1 );
				v[3]=D3DTLVERTEX(D3DVECTOR(w, -.5f, 0 ), .5, color, 0, 1, 0 );
				d->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
			}

			RECT r;
			r.left=r.top=0;
			r.right=1024;
			r.bottom=768;
			if(olc->IsLost()) olc->Restore();
			olc->Blt(NULL, Real2, &r, DDBLT_WAIT, 0);
			if(ola->IsLost()) ola->Restore();
			ola->Blt(NULL, Real2, &r, DDBLT_WAIT, 0);

			d->SetTexture(0, getTex(olc));
			d->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
			d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			d->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
			d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
			d->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);

			D3DVECTOR offsets[]={
				D3DVECTOR(-1, 0, 0), D3DVECTOR(1, 0, 0), 
				D3DVECTOR(0, -1, 0), D3DVECTOR(0, 1, 0),
				D3DVECTOR(0, 0, 0),
			};
			for(int i=0; i<sizeof(offsets)/sizeof(offsets[0]); i++) {
				DWORD color=0xffffffff;
				float w=1024.0f-.5f, h=768.0f-.5f;
				D3DTLVERTEX v[4];
				v[0]=D3DTLVERTEX(D3DVECTOR(-.5f, h, 0 ), .5, color, 0, 0+offsets[i].x/1024.0f, 1+offsets[i].y/768.0f );
				v[1]=D3DTLVERTEX(D3DVECTOR(-.5f, -.5f, 0 ), .5, color, 0, 0+offsets[i].x/1024.0f, 0+offsets[i].y/768.0f );
				v[2]=D3DTLVERTEX(D3DVECTOR(w, h, 0 ), .5, color, 0, 1+offsets[i].x/1024.0f, 1+offsets[i].y/768.0f );
				v[3]=D3DTLVERTEX(D3DVECTOR(w, -.5f, 0 ), .5, color, 0, 1+offsets[i].x/1024.0f, 0+offsets[i].y/768.0f );
				d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
				if(!i) d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO);
				d->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
			}

			olc->Blt(NULL, Real2, &r, DDBLT_WAIT, 0);
			Real2->Blt(NULL, base, 0, DDBLT_WAIT, 0);

			d->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
			d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
			d->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
			d->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
			d->SetTextureStageState(1, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
			d->SetTextureStageState(1, D3DTSS_MINFILTER, D3DTFN_LINEAR);
			
			d->SetTexture(0, getTex(olc));
			d->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
			d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			d->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
			d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
			d->SetTexture(1, getTex(ola));
			d->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
			d->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
			d->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
			d->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
			d->SetTextureStageState(2, D3DTSS_COLOROP,   D3DTOP_DISABLE);

			drawQuad(d, 0xffffffff);

			postProcess.stateRestore();

		} else {
			hr=Real2->Blt(NULL, overlay[oddframe], 0, DDBLT_KEYSRC|DDBLT_WAIT, 0);
			CHECKFAIL(hr);
		}
	}
	if(!FUMode) ClearOverlay();
}
HRESULT _stdcall FakePrimarySurface::Flip(LPDIRECTDRAWSURFACE4 a, DWORD b) {
	if(MenuMode) UNUSEDFUNCTION;
	
	if(FUMode) {
		// Loading screen.
		if(locks==1) postProcess.clearTrail(255.0f);
		if(pp.BloomUI) {
			BltOverlay();
			postProcess.bloom();
			renderFlares(Real2);
			postProcess.saturation();
		} else {
			postProcess.bloom();
			BltOverlay();
			renderFlares(Real2);
			postProcess.saturation();
		}
	} else {
		BltOverlay();
		postProcess.bloom();
		postProcess.saturation();
	}
	
	if(GetAsyncKeyState(VK_SNAPSHOT)) {
		/*DWORD total, free;
		DDSCAPS2 caps;
		memset(&caps, 0, sizeof(caps));
		caps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY|DDSCAPS_LOCALVIDMEM;
		HRESULT hr=rDDraw->GetAvailableVidMem(&caps, &total, &free);
		DWORD used=(total-free)/(1024*1024);
		total/=(1024*1024);
		free/=(1024*1024);*/
		SaveBitmap(Real2);
	}

	// Frame rate limiter.
	static __int64 lastc=0, count=0;
	if(FrameRateLimit) {
		count=getCount()-lastc;
		count=(__int64)(1000.0/FrameRateLimit)-count/(getFreq()/1000);
		if(count>0) Sleep((DWORD)count);
		lastc=getCount();
	}

#ifdef NONEXCLUSIVE
	hr=Real->BltFast(0,0,Real2,0,DDBLTFAST_WAIT);
#else
	HRESULT hr=Real->Flip(NULL,DDFLIP_WAIT|((FUMode&&locks==1)?DDFLIP_NOVSYNC:0)|(FlipVSync?0:DDFLIP_NOVSYNC)|FlipInterval);
	CHECKFAIL(hr);
#endif
	postProcess.flip();
	postProcess.dynamicGamma(Real);
	if(FUMode) {
		scene=0;
		locks=0;
		oddframe=!oddframe;
		// Clear z.
		IDirect3DDevice3 *Real=realIDirect3DDevice;
		postProcess.stateSave();
		Real->BeginScene();
		Real->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS);
		Real->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, 1);
		Real->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
		Real->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO);
		Real->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
		D3DTLVERTEX v[4];
		v[0]=D3DTLVERTEX(D3DVECTOR(-1, (float)(gHeight), 1 ), 1, 0, 0, 0, 1 );
		v[1]=D3DTLVERTEX(D3DVECTOR(-1, -1, 1 ), 1, 0, 0, 0, 0 );
		v[2]=D3DTLVERTEX(D3DVECTOR((float)(gWidth), (float)(gHeight), 1 ), 1, 0, 0, 1, 1 );
		v[3]=D3DTLVERTEX(D3DVECTOR((float)(gWidth), -1, 1 ), 1, 0, 0, 1, 0 );
		Real->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, &v, 4, D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTLIGHT);
		Real->EndScene();
		postProcess.stateRestore();
	}
	return DD_OK;
}
HRESULT _stdcall FakePrimarySurface::GetAttachedSurface(LPDDSCAPS2 a, LPDIRECTDRAWSURFACE4 * b) {
	if(a->dwCaps&DDSCAPS_BACKBUFFER) {
		AddRef();
		*b=this;
		return DD_OK;
	} else UNUSEDFUNCTION;
}
HRESULT _stdcall FakePrimarySurface::GetBltStatus(DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetCaps(LPDDSCAPS2 a) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetClipper(LPDIRECTDRAWCLIPPER *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetColorKey(DWORD, LPDDCOLORKEY) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetDC(HDC *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetFlipStatus(DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetOverlayPosition(LPLONG, LPLONG) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetPalette(LPDIRECTDRAWPALETTE *) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetPixelFormat(LPDDPIXELFORMAT) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetSurfaceDesc(LPDDSURFACEDESC2 a) {
	*a=MenuMode?mDesc:oDesc;
	return DD_OK;
}
HRESULT _stdcall FakePrimarySurface::Initialize(LPDIRECTDRAW, LPDDSURFACEDESC2) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::IsLost() {
	return (Real->IsLost()|menu->IsLost()|overlay[0]->IsLost()|overlay[1]->IsLost()|fakebb->IsLost());
}
HRESULT _stdcall FakePrimarySurface::Lock(LPRECT a,LPDDSURFACEDESC2 b,DWORD c,HANDLE d) {
	static DWORD firstcall=0;
	DWORD offset;
	if(!firstcall) firstcall=(DWORD)&offset;
	offset=(DWORD)&offset-firstcall;
	if(MenuMode) {
		/* Menu screens and videos vsync. Values of b (a pointer into the 
		 * stack) can vary, but the offset from the value in the first call 
		 * to the value in the call we want to catch seems to be constant.
		 */
		if(menu->IsLost()) menu->Restore();
		static __int64 lflip=0;
		__int64 count=getCount();
		if(offset==0xc0 || offset==0xa0/*FU3*/ || (count-lflip)>getFreq()/30) {
			lflip=count;
#ifdef NONEXCLUSIVE
			//hr=Real->BltFast(0, 0, menu, 0, 0);
			hr=Real->Blt(0, menu, 0, 0, 0);
#else
			RECT r={0, 0, gWidth, gHeight};
			if(MenuScreenAspect!=0) {
				if((DWORD)((float)gHeight*MenuScreenAspect)<gWidth)
					r.left=(gWidth-(DWORD)((float)gHeight*MenuScreenAspect))/2;
				else
					r.top=(gHeight-(DWORD)((float)gWidth/MenuScreenAspect))/2;
				r.right=gWidth-r.left;
				r.bottom=gHeight-r.top;
			}
			DDBLTFX ddbltfx;
			ddbltfx.dwSize=sizeof(ddbltfx);
			ddbltfx.dwFillColor=0;
			Real2->Blt(0, 0, 0, DDBLT_COLORFILL, &ddbltfx);
			Real2->Blt(&r, menu, 0, 0, 0);
			Real->Flip(NULL,DDFLIP_WAIT|FlipInterval);
#endif
			if(GetAsyncKeyState(VK_SNAPSHOT)) {
				SaveBitmap(menu);
			}
			if(MenuUpdateDelay) Sleep(MenuUpdateDelay);
		}
		HRESULT hr;
		while((hr=menu->Lock(a,b,c,d))==DDERR_SURFACELOST) menu->Restore();
		CHECKFAIL(hr);
		if(FAILED(hr)) return hr;
		b->ddsCaps.dwCaps|=DDSCAPS_VIDEOMEMORY|DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
		b->dwBackBufferCount=1;
		b->dwFlags|=DDSD_BACKBUFFERCOUNT;
		return DD_OK;
	} else {
		locks++;
		if(!FUMode && !pp.BloomUI) postProcess.bloom();
		if(FUMode && renderinfo.olr.right!=-1) ClearOverlay(&renderinfo.olr);
		renderinfo.olr.left=renderinfo.olr.top=0x10000;
		renderinfo.olr.right=renderinfo.olr.bottom=-1;
		HRESULT hr;
		if(FUMode && offset==0xFFFFFFE8) {
			// Stars.
			while((hr=fakebb->Lock(a,b,c,d))==DDERR_SURFACELOST) fakebb->Restore();
			if(!hr) fakebb_locked=true;
		} else {
			while((hr=overlay[oddframe]->Lock(a,b,c,d))==DDERR_SURFACELOST) overlay[oddframe]->Restore();
		}
		CHECKFAIL(hr);
		return hr;
	}
}
HRESULT _stdcall FakePrimarySurface::ReleaseDC(HDC) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::Restore() {
	HRESULT hr;
	hr=Real->Restore();
	CHECKFAIL(hr);
	if(hr) return hr;
	hr=menu->Restore();
	CHECKFAIL(hr);
	if(hr) return hr;
	hr=overlay[0]->Restore();
	CHECKFAIL(hr);
	if(hr) return hr;
	hr=overlay[1]->Restore();
	CHECKFAIL(hr);
	if(hr) return hr;
	hr=fakebb->Restore();
	CHECKFAIL(hr);
	if(hr) return hr;
	for(int i=0; i<4; i++) {
		restoreFlare(i);
	}
	return DD_OK;
}
HRESULT _stdcall FakePrimarySurface::SetClipper(LPDIRECTDRAWCLIPPER) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::SetColorKey(DWORD, LPDDCOLORKEY) { UNUSEDFUNCTION;	}
HRESULT _stdcall FakePrimarySurface::SetOverlayPosition(LONG, LONG) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::SetPalette(LPDIRECTDRAWPALETTE) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::Unlock(LPRECT a) {
	if(MenuMode) {
		CHECKFUNC(menu->Unlock(a));
	} else {
		if(FUMode && fakebb_locked) {
			fakebb->Unlock(a);
			fakebb_locked=false;
			// Blend stars from fakebb to the actual backbuffer.
			if(Real2 && realIDirect3DDevice)
			{
				LPDIRECT3DDEVICE3 d=realIDirect3DDevice;
				static LPDIRECTDRAWSURFACE4 base=NULL;
				if(!base) {
					DDSURFACEDESC2 desc;
					memset(&desc, 0, sizeof(DDSURFACEDESC2));
					desc.dwSize=sizeof(DDSURFACEDESC2);
					Real2->GetSurfaceDesc(&desc);
					desc.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;
					desc.ddpfPixelFormat.dwLuminanceBitCount=8;
					desc.ddpfPixelFormat.dwLuminanceBitMask=0xff;
					desc.ddpfPixelFormat.dwFlags=DDPF_LUMINANCE;
					rDDraw->CreateSurface(&desc, &base, 0);
				}
				if(base->IsLost()) base->Restore();
				// Copy rgb (base) map out of the current backbuffer.
				base->BltFast(0, 0, Real2, NULL, DDBLTFAST_WAIT);

				postProcess.stateSave();

				d->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS);
				d->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, 0);
				d->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
				d->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
				d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
				d->SetTexture(0, getTex(base));
				d->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_POINT);
				d->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFG_POINT);
				d->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE4X);
				d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				d->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
				d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
				d->SetTexture(1, getTex(fakebb));
				d->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_SUBTRACT);
				d->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				d->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
				d->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
				d->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
				d->SetTextureStageState(2, D3DTSS_COLOROP,   D3DTOP_DISABLE);

				drawQuad(d, 0xffffffff);

				postProcess.stateRestore();
			}
			// Clear fakebb.
			DDBLTFX fx;
			memset(&fx, 0, sizeof(DDBLTFX));
			fx.dwSize=sizeof(DDBLTFX);
			HRESULT hr=fakebb->Blt(0, 0, 0, DDBLT_WAIT|DDBLT_COLORFILL, &fx);
			CHECKFAIL(hr);
		} else {
			overlay[oddframe]->Unlock(a);
		}
		if(LayeredOverlay) BltOverlay();
		return DD_OK;
	}
}
HRESULT _stdcall FakePrimarySurface::UpdateOverlay(LPRECT, LPDIRECTDRAWSURFACE4,LPRECT,DWORD, LPDDOVERLAYFX) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::UpdateOverlayDisplay(DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::UpdateOverlayZOrder(DWORD, LPDIRECTDRAWSURFACE4) { UNUSEDFUNCTION; }
//*** Added in the v2 interface ***
HRESULT _stdcall FakePrimarySurface::GetDDInterface(LPVOID*) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::PageLock(DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::PageUnlock(DWORD) { UNUSEDFUNCTION; }
//*** Added in the v3 interface ***
HRESULT _stdcall FakePrimarySurface::SetSurfaceDesc(LPDDSURFACEDESC2, DWORD) { UNUSEDFUNCTION; }
//*** Added in the v4 interface ***
HRESULT _stdcall FakePrimarySurface::SetPrivateData(REFGUID, LPVOID, DWORD, DWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetPrivateData(REFGUID, LPVOID, LPDWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::FreePrivateData(REFGUID) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::GetUniquenessValue(LPDWORD) { UNUSEDFUNCTION; }
HRESULT _stdcall FakePrimarySurface::ChangeUniquenessValue() { UNUSEDFUNCTION; }