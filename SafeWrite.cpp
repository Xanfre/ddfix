#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#pragma warning(disable:4996)

void SafeWrite8(DWORD addr, BYTE data) {
	DWORD	oldProtect;

	VirtualProtect((void *)addr, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
	*((BYTE*)addr) = data;
	VirtualProtect((void *)addr, 1, oldProtect, &oldProtect);
}

void SafeWrite16(DWORD addr, WORD data) {
	DWORD	oldProtect;

	VirtualProtect((void *)addr, 2, PAGE_EXECUTE_READWRITE, &oldProtect);
	*((WORD*)addr) = data;
	VirtualProtect((void *)addr, 2, oldProtect, &oldProtect);
}

void SafeWrite32(DWORD addr, DWORD data) {
	DWORD	oldProtect;

	VirtualProtect((void *)addr, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
	*((DWORD*)addr) = data;
	VirtualProtect((void *)addr, 4, oldProtect, &oldProtect);
}

void SafeWriteStr(DWORD addr, char* data) {
	DWORD	oldProtect;

	VirtualProtect((void *)addr, strlen(data)+1, PAGE_EXECUTE_READWRITE, &oldProtect);
	strcpy((char *)addr, data);
	VirtualProtect((void *)addr, strlen(data)+1, oldProtect, &oldProtect);
}