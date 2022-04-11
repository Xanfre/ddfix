#include "main.h"

class PostProcess
{
private:
	static const int maxmaps=9, savestages=8;
	int maps, stages;
	bool haveScene, doneBloom, doneSaturation, doneLevel;
	float averageLevel;
	LPDIRECTDRAWSURFACE4 blooml[maxmaps], base, grey, trail;
	struct {
		DWORD D3DRENDERSTATE_ZENABLE,
			D3DRENDERSTATE_ZWRITEENABLE,
			D3DRENDERSTATE_ALPHABLENDENABLE,
			D3DRENDERSTATE_SRCBLEND,
			D3DRENDERSTATE_DESTBLEND,
			D3DRENDERSTATE_TEXTUREFACTOR;
		struct {
			LPDIRECT3DTEXTURE2 tex;
			DWORD D3DTSS_MAGFILTER,	D3DTSS_MINFILTER,
				D3DTSS_COLOROP,	D3DTSS_COLORARG1, D3DTSS_COLORARG2,
				D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2, 
				D3DTSS_TEXCOORDINDEX, D3DTSS_ADDRESS;
		} stage[savestages];
	} state;
	void init();
public:
	static float currentGamma;
	PostProcess();
	void stateSave();
	void stateRestore();
	void stateLog();
	void clearTrail(float newAverage=64.0f);
	void beginScene() { haveScene=true; }
	void flip() { haveScene=doneBloom=doneSaturation=doneLevel=false; }
	void updateLevel();
	void dynamicGamma(LPDIRECTDRAWSURFACE4 s);
	void bloom();
	void saturation();
};

extern PostProcess postProcess;
