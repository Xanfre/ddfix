#include "main.h"

//#include <process.h>
//typedef void (_cdecl* ThreadProc)(void*);

#pragma pack(push)
#pragma pack(1)
struct BMPFileHeader {
	WORD val;
	DWORD size;
	DWORD reserved;
	DWORD offset;
};
struct BMPDataHeader {
	DWORD size;
	DWORD width;
	DWORD height;
	WORD planes;
	WORD bitdepth;
	DWORD compression;
	DWORD isize;
	DWORD xppm;
	DWORD yppm;
	DWORD clr;
	DWORD clri;
};
struct BMPMaskHeader {
	DWORD red;
	DWORD green;
	DWORD blue;
};
#pragma pack(pop)

bool bmpLoad(HANDLE hfile, DWORD* width, DWORD* height, DWORD* depth) {
	BMPFileHeader header;
	BMPDataHeader data;

	DWORD unused;
	ReadFile(hfile, &header, sizeof(header), &unused, 0);
	ReadFile(hfile, &data, sizeof(data), &unused, 0);

	if(header.val!=19778) return false;
	if(data.bitdepth!=24&&data.bitdepth!=32) return false;

	*width=data.width;
	*height=data.height;
	*depth=data.bitdepth;

	return true;
}

/*struct ThreadedArgs {
	void* ptr;
	DWORD pitch;
};

static void _cdecl SaveBitmapThreaded(ThreadedArgs* args) {
	void* ptr=args->ptr;
	DWORD pitch=args->pitch;

	BMPFileHeader header;
	BMPDataHeader data;
	memset(&header, 0, sizeof(header));
	memset(&data, 0, sizeof(data));

	header.val = 19778;
	header.size=sizeof(header)+sizeof(data)+gWidth*gHeight*3;
	header.offset = sizeof(header) + sizeof(data);

	data.size=sizeof(data);
	data.planes=1;
	data.width=gWidth;
	data.height=gHeight;
	data.bitdepth=24;

	char FileName[MAX_PATH];
	char Number[6];
	WIN32_FIND_DATA fdata;
	HANDLE h;
	for(long i=1;i<32000;i++) {
		strcpy_s(FileName, ".\\screenshot");
		_ltoa_s(i,Number,10);
		strcat_s(FileName,Number);
		strcat_s(FileName,".bmp");
		h=FindFirstFileA(FileName, &fdata);
		if(h==INVALID_HANDLE_VALUE) break;
		else FindClose(h);
	}

	DWORD unused;
	HANDLE file=CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if(file==INVALID_HANDLE_VALUE) return;

	WriteFile(file,&header, sizeof(header), &unused, 0);
	WriteFile(file,&data, sizeof(data), &unused, 0);
	ptr=(void*)((DWORD)ptr+pitch*(gHeight-1));
	unused=0;
	for(DWORD i=0;i<gHeight;i++) {
		void* lastptr=ptr;
		for(DWORD j=0;j<gWidth;j++) {
			*((DWORD*)(&buffer[unused]))=*(DWORD*)ptr;
			ptr=(void*)((DWORD)ptr + 4);
			unused+=3;
		}

		ptr=(void*)((DWORD)lastptr-pitch);
	}

	WriteFile(file, buffer, gWidth*gHeight*3, &unused, 0);

	CloseHandle(file);

	delete[] args->ptr;
	delete args;
}*/

void SaveBitmap(LPDIRECTDRAWSURFACE4 surf, bool clearAlpha) {
	/*char* data=new char[pitch*gHeight*4];
	memcpy(data, ptr, pitch*gHeight*4);
	ThreadedArgs* args=new ThreadedArgs;
	args->ptr=data;
	args->pitch=pitch;
	_beginthread((ThreadProc)&SaveBitmapThreaded, 0, args);*/

	char FileName[MAX_PATH];
	char Number[6];
	WIN32_FIND_DATA fdata;
	HANDLE h;
	for(long i=1;i<32000;i++) {
		strcpy_s(FileName, ".\\screenshot");
		_ltoa_s(i,Number,10);
		strcat_s(FileName,Number);
		strcat_s(FileName,".bmp");
		h=FindFirstFileA(FileName, &fdata);
		if(h==INVALID_HANDLE_VALUE) break;
		else FindClose(h);
	}

	DWORD unused;
	HANDLE file=CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if(file==INVALID_HANDLE_VALUE) return;

	DDSURFACEDESC2 desc;
	memset(&desc, 0, sizeof(desc));
	desc.dwSize=sizeof(desc);
	surf->Lock(0, &desc, DDLOCK_READONLY|DDLOCK_WAIT, 0);
	BMPFileHeader header;
	BMPDataHeader data;
	BMPMaskHeader mask;
	memset(&header, 0, sizeof(header));
	memset(&data, 0, sizeof(data));
	memset(&mask, 0, sizeof(mask));

	data.size=sizeof(data);
	data.planes=1;
	data.width=desc.dwWidth;
	data.height=desc.dwHeight;
	WORD bitdepth=data.bitdepth=(WORD)desc.ddpfPixelFormat.dwRGBBitCount;
	if(clearAlpha && data.bitdepth==32) data.bitdepth=24;
	switch(bitdepth) {
		case 16: case 24: case 32: break;
		default:
			surf->Unlock(NULL);
			CloseHandle(file);
			return;
	}
	header.val = 19778;
	header.size=sizeof(header)+sizeof(data)+desc.dwWidth*desc.dwHeight*(bitdepth/8);
	header.offset = sizeof(header) + sizeof(data);

	if(bitdepth==16) {
		header.offset += sizeof(mask);
		data.compression=3; // bi_bitfields
	}

	WriteFile(file,&header, sizeof(header), &unused, 0);
	WriteFile(file,&data, sizeof(data), &unused, 0);

	if(bitdepth==16) {
		mask.red=desc.ddpfPixelFormat.dwRBitMask;
		mask.green=desc.ddpfPixelFormat.dwGBitMask;
		mask.blue=desc.ddpfPixelFormat.dwBBitMask;
		WriteFile(file,&mask, sizeof(mask), &unused, 0);
	}

	if(clearAlpha && bitdepth==32) {
		char *sl=new char[desc.dwWidth*3+1];
		for(DWORD i=0;i<desc.dwHeight;i++) {
			for(DWORD x=0; x<desc.dwWidth; x++) {
				*((DWORD *)(sl+x*3))=((DWORD *)((char*)desc.lpSurface+(desc.dwHeight-1-i)*desc.lPitch))[x];
			}
			WriteFile(file, sl, desc.dwWidth*3, &unused, 0);
		}
		delete sl;
	} else {
		for(DWORD i=0;i<desc.dwHeight;i++) {
			WriteFile(file, (char*)desc.lpSurface+(desc.dwHeight-1-i)*desc.lPitch, desc.dwWidth*(bitdepth/8), &unused, 0);
		}
	}

	surf->Unlock(NULL);
	CloseHandle(file);
}
