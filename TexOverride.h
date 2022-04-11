#pragma once

#include <vector>
#include <Vfw.h>

struct OverridenTex {
 	IDirectDrawSurface4* surf;
 	IDirect3DTexture2* tex;
 	DDSURFACEDESC2 desc;
	PAVIFILE pavif;
	PAVISTREAM pavis;
	AVISTREAMINFO asinfo;
	AVIFILEINFO afinfo;
	IGetFrame *gf;
	int frame;
	OverridenTex() {
		memset(this, 0, sizeof(OverridenTex));
		frame=-1;
	}
	~OverridenTex() {
		if(tex) tex->Release();
		if(surf) surf->Release();
		if(gf) AVIStreamGetFrameClose(gf);
		if(pavis) AVIStreamRelease(pavis);
		if(pavif) AVIFileRelease(pavif);
	}
};

void OverrideSetup();
char* GetOverridePath();

OverridenTex* GetOverride();

IDirectDrawSurface4* IsTextureInVRam(OverridenTex* ot);
void AddTextureToVRam(OverridenTex* ot, IDirectDrawSurface4* surf);
void RemoveTextureFromVRam(OverridenTex* ot);
void updateAviTextures();
