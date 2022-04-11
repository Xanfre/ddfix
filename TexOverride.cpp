#include "FakeCommon.h"
#include "SafeWrite.h"
#include "TexOverride.h"
#include "BitmapSaver.h"
#include "FakeVidSurface.h"
#include "PostProcess.h"

#include <wincrypt.h>

#include <string>
#include <hash_map>
#include <vector>

#include "gif_lib.h"

using namespace std;
using namespace stdext;

static const int HASH_SIZE=128/8;
static const int DDFIX_SIGNATURE_LENGTH=19;
static const int MIN_OVERRIDE_SIZE=256;

static __int64 start=0;

static void SpawnTexture(DWORD width, DWORD height, DDSURFACEDESC2* desc, IDirectDrawSurface4** surf, IDirect3DTexture2** tex);
static void _cdecl CheckTex(void* tex);

#pragma pack(push)
#pragma pack(1)
struct tgaHeader
{
  BYTE IDLength;        /* 00h  Size of Image ID field */
  BYTE ColorMapType;    /* 01h  Color map type */
  BYTE ImageType;       /* 02h  Image type code */
  WORD CMapStart;       /* 03h  Color map origin */
  WORD CMapLength;      /* 05h  Color map length */
  BYTE CMapDepth;       /* 07h  Depth of color map entries */
  WORD XOffset;         /* 08h  X origin of image */
  WORD YOffset;         /* 0Ah  Y origin of image */
  WORD Width;           /* 0Ch  Width of image */
  WORD Height;          /* 0Eh  Height of image */
  BYTE PixelDepth;      /* 10h  Image pixel size */
  BYTE ImageDescriptor; /* 11h  Image descriptor byte */
};
struct PCXHeader
{
	BYTE manufacturer;
	BYTE version;
	BYTE encoding;

	BYTE bitsPerPixel;
	WORD xmin, ymin;
	WORD xmax, ymax;
	WORD hDpi, vDpi;

	BYTE colormap[48];
	BYTE reserved;
	BYTE nPlanes;
	WORD bytesPerLine;
	WORD paletteInfo;
	WORD hscreenSize, vscreenSize;

	BYTE filler[54];
};
struct texinfo {
	BYTE *data;
	char _padding[4];
	unsigned short int xsize, ysize, pitch;
};
#pragma pack(pop)

typedef void (_cdecl *RealFunc)(void*,void*);
typedef pair<DWORD, OverridenTex*> MyPair;
typedef pair<DWORD, IDirectDrawSurface4*> MyPair2;

static RealFunc rCopyTex=NULL;
static RealFunc rMipMap=NULL;
static RealFunc rCreateVidTexture=NULL;
static RealFunc rTextureExists=NULL;

static bool Overriding=false;
static bool FirstOverride=false;
static OverridenTex* lastTex;

static hash_map<DWORD, OverridenTex*> texMap;
static hash_map<DWORD, IDirectDrawSurface4*> vidMap;
static vector<OverridenTex*> texList;
vector<OverridenTex*> animTexList;

static OverridenTex* cTex=0;
OverridenTex* GetOverride() {
	return cTex;
}


static void _stdcall CreateVidTextureHook2(DWORD* d) {
	hash_map<DWORD, OverridenTex*> :: const_iterator itr;
	itr=texMap.find(*d);
	if(itr!=texMap.end()) {
		cTex=itr->second;
		return;
	} else {
		CheckTex((void*)d);
	}
	itr=texMap.find(*d);
	cTex=itr->second;
}

static void _declspec(naked) CreateVidTextureHook() {
	_asm {
		mov eax, esp;
		pushad;
		mov eax, [eax+4];
		mov eax, [eax];
		push eax;
		call CreateVidTextureHook2;
		popad;
		mov eax, rCreateVidTexture;
		jmp eax;
	}
}

static void ReadString(char* buf, const BYTE* ptr) {
	int index=0;
	while(*ptr!=1&&index<MIN_OVERRIDE_SIZE-DDFIX_SIGNATURE_LENGTH-1) {
		buf[index++]=*ptr;
		ptr++;
	}
	buf[index]=0;
}

static void BltFromFile(char* dest, HANDLE source, DWORD width, DWORD height, DWORD destPitch, DWORD sourceDepth, bool invVert, bool invHorz) {
	DWORD unused;
	if(sourceDepth==24) {
		char* buf=new char[width*height*3 + 4];
		ReadFile(source, buf, width*height*3, &unused, 0);
		destPitch/=4;
		for(DWORD y=0;y<height;y++) {
			DWORD offset=(invVert?height-(y+1):y)*destPitch;
			DWORD offset2=y*width*3;
			if(invHorz) {
				for(DWORD x=0;x<width;x++) ((DWORD*)dest)[offset+(width-(x+1))]=(*(DWORD*)(&buf[offset2+x*3])) | 0xff000000;
			} else {
				for(DWORD x=0;x<width;x++) ((DWORD*)dest)[offset+x]=(*(DWORD*)(&buf[offset2+x*3])) | 0xff000000;
			}
		}
		delete[] buf;
	} else if(sourceDepth==32) {
		width <<= 2;
		if(invVert) {
			for(DWORD y=1;y<=height;y++) ReadFile(source, &dest[(height-y)*destPitch], width, &unused, 0);
		} else {
			if(destPitch==width) {
				ReadFile(source, dest, width*height, &unused, 0);
			} else {
				for(DWORD y=0;y<height;y++) ReadFile(source, &dest[y*destPitch], width, &unused, 0);
			}
		}
		if(invHorz) {
			DWORD tmp;
			for(DWORD y=0;y<height;y++) {
				for(DWORD x=0;x<width/2;x+=4) {
					tmp=*(DWORD*)&dest[y*destPitch+x];
					*(DWORD*)&dest[y*destPitch+x]=dest[y*destPitch+width-(x+4)];
					*(DWORD*)&dest[y*destPitch+width-(x+4)]=tmp;
				}
			}
		}
	}
}
static void GenMipMaps(IDirectDrawSurface4* from, const DDSURFACEDESC2* desc) {
	if(desc->dwMipMapCount) {
		DDSCAPS2 caps = { DDSCAPS_MIPMAP,0,0,0 };
		from->AddRef();
		IDirectDrawSurface4* to;
		for(DWORD i=1;i<desc->dwMipMapCount;i++) {
			from->GetAttachedSurface(&caps, &to);
			to->Blt(0, from, 0, DDBLT_WAIT, 0);
			from->Release();
			from=to;
		}
		to->Release();
	}
}
static bool LoadTga(const char* buf, OverridenTex* ot) {
	char buf2[MAX_PATH];
	strcpy_s(buf2, buf);
	strcat_s(buf2, ".tga");
	HANDLE file=CreateFileA(buf2, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if(file==INVALID_HANDLE_VALUE) return false;
	tgaHeader h;
	DWORD unused;
	ReadFile(file, &h, sizeof(h), &unused, 0);
	if((h.PixelDepth!=24&&h.PixelDepth!=32)||h.CMapLength!=0||h.ImageType!=2||h.ColorMapType!=0) {
		CloseHandle(file);
		return false;
	}

	ReadFile(file, buf2, h.IDLength, &unused, 0);
	DDSURFACEDESC2 desc;
	SpawnTexture(h.Width, h.Height, &desc, &ot->surf, &ot->tex);
	DDSURFACEDESC2 desc2;
	memset(&desc2, 0, sizeof(desc2));
	desc2.dwSize=sizeof(desc2);
	ot->surf->Lock(0, &desc2, DDLOCK_WRITEONLY|DDLOCK_WAIT, 0);
	BltFromFile((char*)desc2.lpSurface, file, h.Width, h.Height, desc2.lPitch, h.PixelDepth, (h.ImageDescriptor&0x20)==0, (h.ImageDescriptor&0x10)==0x10);
	ot->surf->Unlock(0);
	CloseHandle(file);
	GenMipMaps(ot->surf, &desc);
	ot->desc=desc;
	return true;
}
static bool LoadBmp(const char* buf, OverridenTex* ot) {
	char buf2[MAX_PATH];
	strcpy_s(buf2, buf);
	strcat_s(buf2, ".bmp");
	HANDLE file=CreateFileA(buf2, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if(file==INVALID_HANDLE_VALUE) return false;
	DWORD width, height, depth;
	if(!bmpLoad(file, &width, &height, &depth)) {
		CloseHandle(file);
		return false;
	}
	DDSURFACEDESC2 desc;
	SpawnTexture(width, height, &desc, &ot->surf, &ot->tex);
	DDSURFACEDESC2 desc2;
	memset(&desc2, 0, sizeof(desc2));
	desc2.dwSize=sizeof(desc2);
	ot->surf->Lock(0, &desc2, DDLOCK_WRITEONLY|DDLOCK_WAIT, 0);
	BltFromFile((char*)desc2.lpSurface, file, width, height, desc2.lPitch, depth, true, false);
	ot->surf->Unlock(0);
	CloseHandle(file);
	GenMipMaps(ot->surf, &desc);
	ot->desc=desc;
	return true;
}

static bool LoadDds(const char* buf, OverridenTex* ot) {
	char buf2[MAX_PATH];
	strcpy_s(buf2, buf);
	strcat_s(buf2, ".dds");
	HANDLE file=CreateFileA(buf2, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if(file==INVALID_HANDLE_VALUE) return false;
	DWORD magic, unused;
	ReadFile(file, &magic, 4, &unused, 0);
	if(magic!=0x20534444) {
		CloseHandle(file);
		return false;
	}
	DDSURFACEDESC2 desc;
	ReadFile(file, &desc, sizeof(desc), &unused, 0);
	if(desc.dwSize!=124||!(desc.ddpfPixelFormat.dwFlags&DDPF_RGB)||(desc.ddpfPixelFormat.dwRGBBitCount!=24&&desc.ddpfPixelFormat.dwRGBBitCount!=32)) {
		CloseHandle(file);
		return false;
	}
	if(desc.ddsCaps.dwCaps&DDSCAPS_ALPHA) desc.ddsCaps.dwCaps^=DDSCAPS_ALPHA;
	desc.ddsCaps.dwCaps|=DDSCAPS_SYSTEMMEMORY;
	DWORD depth=desc.ddpfPixelFormat.dwRGBBitCount;
	if(depth==24) {
		desc.ddpfPixelFormat.dwRGBBitCount=32;
		desc.ddpfPixelFormat.dwRBitMask=0xff0000;
		desc.ddpfPixelFormat.dwGBitMask=0xff00;
		desc.ddpfPixelFormat.dwBBitMask=0xff;
	}
	HRESULT hr=rDDraw->CreateSurface(&desc, &ot->surf, 0);
	if(hr) {
		CloseHandle(file);
		return false;
	}
	DDSURFACEDESC2 desc2;
	memset(&desc2, 0, sizeof(desc2));
	desc2.dwSize=sizeof(desc2);
	IDirectDrawSurface4* surf=ot->surf;
	surf->AddRef();
	
	DWORD mipmaps;
	if(!(desc.dwFlags&DDSD_MIPMAPCOUNT)) mipmaps=1;
	else mipmaps=desc.dwMipMapCount;
	DDSCAPS2 caps = { DDSCAPS_MIPMAP,0,0,0 };
	for(DWORD i=1;i<=mipmaps;i++) {
		surf->Lock(0, &desc2, DDLOCK_WRITEONLY|DDLOCK_WAIT, 0);
		BltFromFile((char*)desc2.lpSurface, file, desc2.dwWidth, desc2.dwHeight, desc2.lPitch, depth, false, false);
		surf->Unlock(0);
		surf->Release();
		if(i<mipmaps) surf->GetAttachedSurface(&caps, &surf);
	}
	CloseHandle(file);

	ot->surf->QueryInterface(IID_IDirect3DTexture2, (void**)&ot->tex);
	ot->desc=desc;
	ot->desc.ddsCaps.dwCaps^=DDSCAPS_SYSTEMMEMORY;
	ot->desc.ddsCaps.dwCaps|=DDSCAPS_VIDEOMEMORY|DDSCAPS_ALLOCONLOAD;
	return true;
}

static bool LoadAvi(const char* buf, OverridenTex* ot) {
	char buf2[MAX_PATH];
	strcpy_s(buf2, buf);
	strcat_s(buf2, ".avi");

	if(AVIFileOpen(&ot->pavif, buf2, OF_SHARE_DENY_WRITE|OF_READ, NULL)) return false;
	if(AVIFileInfo(ot->pavif, &ot->afinfo, sizeof(ot->afinfo))) return false;
	if(ot->afinfo.dwWidth>4096 || ot->afinfo.dwHeight>4096) return false;
	int c;
	for(c=1; c<(int)ot->afinfo.dwWidth; ) c<<=1;
	if(c!=ot->afinfo.dwWidth) return false;
	for(c=1; c<(int)ot->afinfo.dwHeight; ) c<<=1;
	if(c!=ot->afinfo.dwHeight) return false;
	if(AVIFileGetStream(ot->pavif, &ot->pavis, streamtypeVIDEO, 0)) return false;
	if(AVIStreamInfo(ot->pavis, &ot->asinfo, sizeof(ot->asinfo))) return false;
	if(!(ot->gf=AVIStreamGetFrameOpen(ot->pavis, (LPBITMAPINFOHEADER)AVIGETFRAMEF_BESTDISPLAYFMT))) return false;

	DDSURFACEDESC2 desc;
	ot->surf=0;
	ot->tex=0;
	memset(&desc, 0, sizeof(DDSURFACEDESC2));
	desc.dwSize=sizeof(DDSURFACEDESC2);
	desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
	desc.dwWidth=ot->afinfo.dwWidth;
	desc.dwHeight=ot->afinfo.dwHeight;
	desc.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
	desc.ddpfPixelFormat.dwFlags=DDPF_RGB;//DDPF_ALPHAPIXELS|DDPF_RGB;
	desc.ddpfPixelFormat.dwRGBBitCount=32;
	desc.ddpfPixelFormat.dwBBitMask=0xff;
	desc.ddpfPixelFormat.dwGBitMask=0xff00;
	desc.ddpfPixelFormat.dwRBitMask=0xff0000;
	desc.ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000;
	desc.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_SYSTEMMEMORY;

	DWORD mipmaps=0;
	DWORD test=(ot->afinfo.dwWidth<ot->afinfo.dwHeight)?ot->afinfo.dwWidth:ot->afinfo.dwHeight;
	while(test>1) {
		mipmaps++;
		test >>= 1;
	}
	if(mipmaps) {
		desc.dwFlags|=DDSD_MIPMAPCOUNT;
		desc.ddsCaps.dwCaps|=DDSCAPS_COMPLEX|DDSCAPS_MIPMAP;
		desc.dwMipMapCount=mipmaps;
	}

	if(rDDraw->CreateSurface(&desc, &ot->surf, 0)!=DD_OK) return false;
	if(ot->surf->QueryInterface(IID_IDirect3DTexture2, (void**)&ot->tex)!=S_OK) return false;

	desc.ddsCaps.dwCaps^=DDSCAPS_SYSTEMMEMORY|DDSCAPS_VIDEOMEMORY|DDSCAPS_ALLOCONLOAD;

	GenMipMaps(ot->surf, &desc);
	ot->desc=desc;

	return true;
}

void updateAviTextures() {
	__int64 count=getCount();
	if(!start) start=count;

	for(DWORD i=0; i<animTexList.size(); i++) {
		int new_frame=((LONG)((__int64)(animTexList[i]->afinfo.dwRate/animTexList[i]->afinfo.dwScale)*(count-start)/getFreq()))%animTexList[i]->asinfo.dwLength;

		if(animTexList[i]->frame!=new_frame) {
			IDirectDrawSurface4* surf=IsTextureInVRam(animTexList[i]);
			if(surf) {
				surf=((FakeVidSurface *)surf)->getReal();
				if(surf) {
					animTexList[i]->frame=new_frame;
					LPVOID fp=AVIStreamGetFrame(animTexList[i]->gf, animTexList[i]->frame);
					DDSURFACEDESC2 desc2;
					memset(&desc2, 0, sizeof(desc2));
					desc2.dwSize=sizeof(desc2);
					surf->Lock(0, &desc2, DDLOCK_WRITEONLY|DDLOCK_WAIT, 0);
					for(int y=0; y<(int)animTexList[i]->afinfo.dwHeight; y++)
						memcpy((char *)desc2.lpSurface+y*desc2.lPitch, 
							   (char *)fp+(animTexList[i]->afinfo.dwHeight-y-1)*animTexList[i]->afinfo.dwWidth*4+*((DWORD *)fp), 
							   animTexList[i]->afinfo.dwWidth*4);
					surf->Unlock(0);
					GenMipMaps(surf, &animTexList[i]->desc);
				}
			}
		}
	}
}

static inline int hashData(const BYTE *p, int len, char *hash) {
	CRYPT_HASH_MESSAGE_PARA hp={0};
	hp.cbSize=sizeof(hp);
	hp.dwMsgEncodingType=X509_ASN_ENCODING|PKCS_7_ASN_ENCODING;
	hp.HashAlgorithm.pszObjId=szOID_RSA_MD5;
	const BYTE *data[]={(BYTE *)p};
	DWORD datasize[]={(DWORD)len};
	DWORD hashsize=HASH_SIZE;
	if(!CryptHashMessage(&hp, TRUE, 1, data, datasize, NULL, NULL, (BYTE *)hash, &hashsize)) return 1;
	return 0;
}

static inline int hashTex(void *tex, char *hash) {
	texinfo *ti=(texinfo *)tex;
	return hashData(ti->data, ti->pitch*ti->ysize, hash);
}

static inline bool isOverride(const BYTE *s) {
	// Check for override signature:
	// 01 02 03 04 01 02 03 04 01 01 02 03 04 64 64 66 69 78 01
	DWORD *p=(DWORD *)s;
	if(p[0]==0x04030201 && p[1]==0x04030201 && p[2]==0x03020101 &&
		p[3]==0x66646404 && (p[4]&0xffffff)==0x017869) return true;
	return false;
}

struct MD5 {
	union {
		char byte[HASH_SIZE];
		DWORD dword[HASH_SIZE/4];
	} v;
	MD5() { memset(&v, 0, sizeof(v)); }
	MD5(const char *h) { memcpy(&v, h, sizeof(v)); }
};
bool operator< (const MD5& k0, const MD5& k1) {
	if(k0.v.dword[0]!=k1.v.dword[0]) return k0.v.dword[0]<k1.v.dword[0];
	if(k0.v.dword[1]!=k1.v.dword[1]) return k0.v.dword[1]<k1.v.dword[1];
	if(k0.v.dword[2]!=k1.v.dword[2]) return k0.v.dword[2]<k1.v.dword[2];
	return k0.v.dword[3]<k1.v.dword[3];
}
bool operator== (const MD5& k0, const MD5& k1) {
	return k0.v.dword[0]==k1.v.dword[0] && k0.v.dword[1]==k1.v.dword[1] &&
		k0.v.dword[2]==k1.v.dword[2] && k0.v.dword[3]==k1.v.dword[3];
}
bool operator!= (const MD5& k0, const MD5& k1) {
	return !(k0==k1);
}
struct MD5Hasher {
	static const size_t bucket_size = 4;
	static const size_t min_buckets = 8;
	size_t operator() (const MD5& k) const {
		return *((size_t *)&k.v);
	}
	bool operator() (const MD5& k0, const MD5& k1) const {
		return k0<k1;
	}
};
typedef pair<MD5, string> HPair;
// Mapping from texture MD5 to replacement file path.
static hash_map<MD5, string, MD5Hasher> hMap;

static inline bool getReplacement(void *tex, string& path) {
	MD5 md5;
	if(!hashTex(tex, md5.v.byte)) {
		hash_map<MD5, string>::const_iterator i;
		if((i=hMap.find(md5))!=hMap.end()) {
			path=i->second;
			return true;
		}
	}
	return false;
}

static BYTE *fdata=NULL;
static DWORD fasize=0;

static BYTE *readFile(const char *fname, DWORD *fsize) {
	DWORD nread=0;
	HANDLE h=CreateFileA(fname, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if(h==INVALID_HANDLE_VALUE) return NULL;
	*fsize=GetFileSize(h, NULL);
	if(fasize<*fsize) {
		delete fdata;
		fasize=(*fsize+0x10000)&0xffff0000;
		fdata=new BYTE[fasize];
	}
	if(fdata) ReadFile(h, fdata, *fsize, &nread, NULL);
	CloseHandle(h);
	if(nread!=*fsize) return NULL;
	return fdata;
}

static BYTE *decodePCX(const BYTE *buf, DWORD fsize)
{
	BYTE *data=NULL;
	PCXHeader *hdr=(PCXHeader *)buf;
	if(hdr->manufacturer==10 && hdr->nPlanes==1 && hdr->bitsPerPixel==8 && hdr->encoding) {
		DWORD datao=0, fileo=sizeof(*hdr),
			dsize=(hdr->xmax+1-hdr->xmin)*(hdr->ymax+1-hdr->ymin);
		data=new BYTE[dsize];
		while(datao<dsize && fileo<fsize) {
			int count=1;
			if((buf[fileo]&0xc0)==0xc0) {
				count=buf[fileo]&0x3f;
				fileo++;
			}
			while(count-- && datao<dsize) {
				data[datao]=buf[fileo];
				datao++;
			}
			fileo++;
		}
		if(datao==dsize) return data;
	}
	delete data;
	return NULL;
}

static inline void insertHash(const texinfo *ti, const char *fname) {
	MD5 md5;
	hashData(ti->data, ti->pitch*ti->ysize, md5.v.byte);
	hash_map<MD5, string>::const_iterator i;
	if((i=hMap.find(md5))==hMap.end()) {
		hMap.insert(HPair(md5, string(fname)));
	}
}

static void _cdecl CheckTex(void* tex) {
	texinfo *ti=(texinfo *)tex;
	if((DWORD)ti->data>=0x10000 && ti->xsize*ti->ysize>=MIN_OVERRIDE_SIZE) {
		string path;
		char buf[MAX_PATH]={0};
		if(isOverride(ti->data)) {
			strcpy_s(buf, TexturePath);
			ReadString(&buf[strlen(TexturePath)], ti->data+DDFIX_SIGNATURE_LENGTH);
		} else if(getReplacement(tex, path)) {
			strcpy(buf, path.c_str());
		}
		if(buf[0]) {
			OverridenTex* ot=new OverridenTex();
			if(LoadDds(buf, ot) || LoadTga(buf, ot) || LoadBmp(buf, ot) || LoadAvi(buf, ot)) {
				lastTex=ot;
				texList.push_back(ot);
				if(ot->pavif) animTexList.push_back(ot);
				texMap.insert(MyPair(*(DWORD*)tex, ot));
				return;
			} else {
				delete ot;
			}
		}
	}
	texMap.insert(MyPair(*(DWORD*)tex, (OverridenTex*)0));
}

static void _cdecl CopyTex(void* dest, void* source) {
	Overriding=false;
	texinfo *ti=(texinfo *)source;
	if(ti && ti->xsize*ti->ysize>=MIN_OVERRIDE_SIZE) {
		string path;
		char buf[MAX_PATH]={0};
		if(isOverride(ti->data)) {
			strcpy_s(buf, TexturePath);
			ReadString(&buf[strlen(TexturePath)], ti->data+DDFIX_SIGNATURE_LENGTH);
		} else if(getReplacement(source, path)) {
			strcpy(buf, path.c_str());
		}
		if(buf[0]) {
			OverridenTex* ot=new OverridenTex();
			if(LoadDds(buf, ot) || LoadTga(buf, ot) || LoadBmp(buf, ot) || LoadAvi(buf, ot)) {
				lastTex=ot;
				Overriding=true;
				texList.push_back(ot);
				if(ot->pavif) animTexList.push_back(ot);
				texMap.insert(MyPair((DWORD)dest, ot));
				rCopyTex(dest,source);
				return;
			} else {
				delete ot;
			}
		}
	}
	texMap.insert(MyPair((DWORD)dest, (OverridenTex*)0));
	rCopyTex(dest,source);
}

void LevelStart() {
	for(DWORD i=0;i<texList.size();i++) delete texList[i];
	texList.clear();
	animTexList.clear();
	texMap.clear();
	vidMap.clear();
	postProcess.clearTrail();
	start=0;
}

static void _cdecl MipMap(void* dest, void* source) {
	if(Overriding) {
		texMap.insert(MyPair(*(DWORD*)dest, lastTex));
	} else texMap.insert(MyPair(*(DWORD*)dest, (OverridenTex*)0));
	rMipMap(dest,source);
}

static FakeVidTexture *tex;
static FakeVidSurface *surf;

static void _declspec(naked) TextureExistsHook() {
	_asm {
		pushad;
		mov tex, eax;
	}
	surf=tex->QuerySurf();
	tex->Release();
	surf->Release();
	_asm {
		popad;
		mov eax, rTextureExists;
		jmp eax;
	}
}

/*static char ReadHookBuf[MAX_PATH];
static bool TwoCount=false;

static BOOL _stdcall ReadFileHook(HANDLE a, LPVOID b, DWORD c, LPDWORD d, LPOVERLAPPED e) {
	if(ReadHookBuf[0]) {
		if(!TwoCount) {
			TwoCount=true;
		} else {
			TwoCount=0;
			ReadHookBuf[0]=0;
		}
	}
	return ReadFile(a,b,c,d,e);
}*/

static HANDLE _stdcall CreateFileHook(const char* fname, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sec, DWORD creation, DWORD flags, HANDLE temp) {

	int l=strlen(fname);

	if(!_strcmpi(fname+l-4, ".mis")) {

		// Here we detect the mis files we want to force global fog with.
		fogp.Detected=0;
		if(!strcmp(fname, "miss11.mis") || /* LotP */
			!strcmp(fname, "miss14.mis") || /* Cargo */
			!strcmp(fname, "miss13.mis")) /* Masks */
				fogp.Detected=1;

		// Per-mission ini files.
		if(EnableMissionIni) {
			readIni(".\\ddfix.ini");
			char ininame[MAX_PATH];
			strcpy(ininame, ".\\");
			strcat(ininame, fname);
			strcpy(ininame+strlen(ininame)-3, "ddfix.ini");
			readIni(ininame, false);
		}

		LevelStart();
	}

	if(TextureReplacement && (!_strcmpi(fname+l-4, ".pcx")||!_strcmpi(fname+l-4, ".gif")||!_strcmpi(fname+l-4, ".tga"))) {

		// Check for explicit override.
		char buf[MAX_PATH];
		strcpy_s(buf, fname);
		strcat_s(buf,".override");
		HANDLE h=CreateFileA(buf, access, share, sec, OPEN_EXISTING, flags, temp);
		if(h!=INVALID_HANDLE_VALUE) {
			//replaced=true;
			return h;
		}

		// Check for auto replacement.
		strcpy(buf, fname);
		buf[l-4]=0;
		string path=string(buf)+string(".r");
		h=CreateFileA((path+string(".dds")).c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
		if(h==INVALID_HANDLE_VALUE) h=CreateFileA((path+string(".tga")).c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
		if(h==INVALID_HANDLE_VALUE) h=CreateFileA((path+string(".bmp")).c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
		if(h==INVALID_HANDLE_VALUE) h=CreateFileA((path+string(".avi")).c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
		if(h!=INVALID_HANDLE_VALUE) {
			CloseHandle(h);
			BYTE *fdata;
			DWORD fsize;
			texinfo ti;

			if(!_strcmpi(fname+l-4, ".pcx")) {
				while(fdata=readFile(fname, &fsize)) {
					PCXHeader *hdr=(PCXHeader *)fdata;
					ti.data=fdata+sizeof(PCXHeader);
					ti.pitch=hdr->bytesPerLine;
					ti.xsize=hdr->xmax+1-hdr->xmin;
					ti.ysize=hdr->ymax+1-hdr->ymin;
					if(hdr->encoding && !(ti.data=decodePCX(fdata, fsize))) break;
					insertHash(&ti, path.c_str());
					delete ti.data;
					break;
				}
			} else if(!_strcmpi(fname+l-4, ".gif")) {
				GifFileType *gf=DGifOpenFileName(fname);
				while(gf) {
					if(DGifSlurp(gf)!=GIF_OK || gf->ImageCount<1 || gf->SavedImages[0].ImageDesc.Width<1 || gf->SavedImages[0].ImageDesc.Height<1) {
						DGifCloseFile(gf);
						break;
					}
					ti.data=(BYTE *)gf->SavedImages[0].RasterBits;
					ti.pitch=gf->SavedImages[0].ImageDesc.Width;
					ti.xsize=gf->SavedImages[0].ImageDesc.Width;
					ti.ysize=gf->SavedImages[0].ImageDesc.Height;
					insertHash(&ti, path.c_str());
					DGifCloseFile(gf);
					break;
				}
			} else if(!_strcmpi(fname+l-4, ".tga")) {
				while(fdata=readFile(fname, &fsize)) {

					tgaHeader *hdr=(tgaHeader *)fdata;
					if((hdr->PixelDepth!=24&&hdr->PixelDepth!=32)||hdr->CMapLength!=0||hdr->ImageType!=2||hdr->ColorMapType!=0) break;
					WORD *pd=new WORD[hdr->Width*hdr->Height];
					bool invVert=(hdr->ImageDescriptor&0x20)==0, invHorz=(hdr->ImageDescriptor&0x10)==0x10;
					if(hdr->PixelDepth==32) {
						DWORD *tgap=(DWORD *)(fdata+sizeof(tgaHeader));
						for(int y=0; y<hdr->Height; y++) {
							for(int x=0; x<hdr->Width; x++) {
								DWORD v=tgap[(invVert ? hdr->Height-1-y : y)*hdr->Width+(invHorz ? hdr->Width-1-x : x)];
								pd[y*hdr->Width+x]=(WORD)(((v&0xf0000000)>>16)|((v&0xf00000)>>12)|((v&0xf000)>>8)|((v&0xf0)>>4));
							}
						}
					} else {
						BYTE *tgap=fdata+sizeof(tgaHeader);
						for(int y=0; y<hdr->Height; y++) {
							for(int x=0; x<hdr->Width; x++) {
								DWORD v=*((DWORD *)(tgap+((invVert ? hdr->Height-1-y : y)*hdr->Width+(invHorz ? hdr->Width-1-x : x))*3));
								v|=0xff000000;
								pd[y*hdr->Width+x]=(WORD)(((v&0xf0000000)>>16)|((v&0xf00000)>>12)|((v&0xf000)>>8)|((v&0xf0)>>4));
							}
						}
					}
					ti.data=(BYTE *)pd;
					ti.pitch=hdr->Width*2;
					ti.xsize=hdr->Width;
					ti.ysize=hdr->Height;
					insertHash(&ti, path.c_str());
					delete pd;
					break;
				}
			}
		}
	}
	return CreateFileA(fname, access, share, sec, creation, flags, temp);
}

void OverrideSetup() {
	if(memaddrp.CopyTex) {
		rCopyTex=(RealFunc)(*((DWORD *)memaddrp.CopyTex)+memaddrp.CopyTex+4);
		SafeWrite32(memaddrp.CopyTex, (DWORD)&CopyTex - (memaddrp.CopyTex+4));
	}
	if(memaddrp.MipMap) {
		rMipMap=(RealFunc)(*((DWORD *)memaddrp.MipMap)+memaddrp.MipMap+4);
		SafeWrite32(memaddrp.MipMap, (DWORD)&MipMap - (memaddrp.MipMap+4));
	}
	if(memaddrp.CreateVidTexture) {
		rCreateVidTexture=(RealFunc)*((DWORD *)memaddrp.CreateVidTexture);
		SafeWrite32(memaddrp.CreateVidTexture, (DWORD)&CreateVidTextureHook);
	}
	if(memaddrp.TextureExists) {
		rTextureExists=(RealFunc)(memaddrp.TextureExists+4);
		SafeWrite32(memaddrp.TextureExists, (DWORD)&TextureExistsHook - (memaddrp.TextureExists+4));
	}
	if(memaddrp.CreateFile) SafeWrite32(memaddrp.CreateFile, (DWORD)&CreateFileHook);
	if(memaddrp.SS2CreateFile) {
		SafeWrite16(memaddrp.SS2CreateFile-2, 0xe890);
		SafeWrite32(memaddrp.SS2CreateFile, (DWORD)&CreateFileHook - (memaddrp.SS2CreateFile+4));
	}
	if(memaddrp.BltPrimary) SafeWrite32(memaddrp.BltPrimary, 0x000008c2);

	//SafeWrite16(0x0041D95F, 0x9090);
	//ReadHookBuf[0]=0;
	//SafeWrite32(0x006001A4, (DWORD)&ReadFileHook);
}

static void SpawnTexture(DWORD width, DWORD height, DDSURFACEDESC2* desc, IDirectDrawSurface4** surf, IDirect3DTexture2** tex) {
	*surf=0;
	*tex=0;
	memset(desc, 0, sizeof(DDSURFACEDESC2));
	desc->dwSize=sizeof(DDSURFACEDESC2);
	desc->dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
	desc->dwWidth=width;
	desc->dwHeight=height;
	desc->ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
	desc->ddpfPixelFormat.dwFlags=DDPF_ALPHAPIXELS|DDPF_RGB;
	desc->ddpfPixelFormat.dwRGBBitCount=32;
	desc->ddpfPixelFormat.dwBBitMask=0xff;
	desc->ddpfPixelFormat.dwGBitMask=0xff00;
	desc->ddpfPixelFormat.dwRBitMask=0xff0000;
	desc->ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000;
	desc->ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_SYSTEMMEMORY;

	DWORD mipmaps=0;
	DWORD test=(width>height)?width:height;
	while(test>1) {
		mipmaps++;
		test >>= 1;
	}
	if(mipmaps) {
		desc->dwFlags|=DDSD_MIPMAPCOUNT;
		desc->ddsCaps.dwCaps|=DDSCAPS_COMPLEX|DDSCAPS_MIPMAP;
		desc->dwMipMapCount=mipmaps;
	}

	rDDraw->CreateSurface(desc, surf, 0);
	(*surf)->QueryInterface(IID_IDirect3DTexture2, (void**)tex);

	desc->ddsCaps.dwCaps^=DDSCAPS_SYSTEMMEMORY|DDSCAPS_VIDEOMEMORY|DDSCAPS_ALLOCONLOAD;
}

IDirectDrawSurface4* IsTextureInVRam(OverridenTex* ot) {
	hash_map<DWORD, IDirectDrawSurface4*> :: const_iterator itr;
	itr=vidMap.find((DWORD)ot);
	if(itr==vidMap.end()) return 0;
	else return itr->second;
}
void AddTextureToVRam(OverridenTex* ot, IDirectDrawSurface4* surf) {
	vidMap.insert(MyPair2((DWORD)ot, surf));
}
void RemoveTextureFromVRam(OverridenTex* ot) {
	vidMap.erase((DWORD)ot);
}