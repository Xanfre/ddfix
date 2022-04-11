#include "main.h"
#include "SafeWrite.h"

static float GameModeAspectCorrection=1.0;
static float OrbAspectCorrection=1.0;
float AspectCorrection=1.0f/65536.0f;

void WidescreenSetMenuMode(bool MenuMode)
{
	if(MenuMode) AspectCorrection=1.0f/65536.0f;
	else AspectCorrection=GameModeAspectCorrection/65536.0f;
}

void WidescreenSetup()
{
	GameModeAspectCorrection=4.0f/3.0f*(float)gHeight/(float)gWidth;
	OrbAspectCorrection=GameModeAspectCorrection/65536.0f;

	if(memaddrp.Resolution) {
		SafeWrite16(memaddrp.Resolution, (WORD)gWidth);
		SafeWrite16(memaddrp.Resolution+2, (WORD)gHeight);
	}

	if(memaddrp.VertResLimit) {
		SafeWrite16(memaddrp.VertResLimit, (WORD)gHeight);
	}

	if(memaddrp.FovModification) {
		float f=GameModeAspectCorrection*100.0f/FovModification;
		SafeWrite32(*((DWORD *)memaddrp.FovModification), *((DWORD *)&f));
	}

	if(memaddrp.AspectCorrection) {
		SafeWrite32(memaddrp.AspectCorrection, (DWORD)&AspectCorrection);
	}

	if(memaddrp.OrbAspectCorrection) {
		SafeWrite32(memaddrp.OrbAspectCorrection, (DWORD)&OrbAspectCorrection);
	}

	if(memaddrp.ItemSize) {
		SafeWrite8(memaddrp.ItemSize, 0xa);
		SafeWrite32(*((DWORD *)(memaddrp.ItemSize+4)), ItemSize*4/3);
	}

}
