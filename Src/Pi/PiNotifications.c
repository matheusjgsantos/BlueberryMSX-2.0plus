/*****************************************************************************
**
** blueberryMSX
** https://github.com/pokebyte/blueberryMSX
**
** An MSX Emulator for Raspberry Pi based on blueMSX
**
** Copyright (C) 2003-2006 Daniel Vik
** Copyright (C) 2014 Akop Karapetyan
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
******************************************************************************
*/

#include "ArchNotifications.h"
#include <stdlib.h>
#include <stdio.h>

extern char *msxScreen;
extern int msxScreenPitch;
extern int height;
void* ScreenShotPng(void* src, int srcPitch, int width, int height, int* bitmapSize);

extern int cfileexists(const char * filename);

static int scrshotno = 0;

void* archScreenCapture(ScreenCaptureType type, int* bitmapSize, int onlyBmp) { 
	void *data = NULL;
	int size = 0;
	FILE *f;
	char str[255];
	if (msxScreen != NULL)
	{
		data = ScreenShotPng(msxScreen, msxScreenPitch, msxScreenPitch, height, &size);
	}
	if (data != NULL)
	{
		do {
			sprintf(str, "screenshot%03d.png", scrshotno++);
		} while(cfileexists(str));
		f = fopen(str, "wb");
		fwrite(data, size, 1, f);
		fclose(f);
	}
	return NULL; 
}

void archUpdateEmuDisplayConfig() {}

void archDiskQuickChangeNotify() {
    system("aplay sound/Toggle.wav");
}

void archThemeSetNext() {}
void archThemeUpdate(struct Theme* theme) {}

void archVideoOutputChange() {}
void archMinimizeMainWindow() {}

int archGetFramesPerSecond() { return 60; }

void* archWindowCreate(struct Theme* theme, int childWindow) { return NULL; }
void archWindowStartMove() {}
void archWindowMove() {}
void archWindowEndMove() {}

void archVideoCaptureSave() {}

typedef unsigned char BYTE;
typedef long DWORD;
typedef unsigned int UInt32;

// =======================================================================================================================
// PNG Stuff
// =======================================================================================================================
#ifdef __BIG_ENDIAN__
#define ntohul(ul)  (ul)
#else
#define ntohul(ul)  (((ul << 24) & 0xff000000) | ((ul << 8) & 0x00ff0000) | ((ul >> 8) & 0x0000ff00) | ((ul >> 24) & 0x000000ff))
#endif

typedef struct PNGHeader {
    DWORD dwWidth;
    DWORD dwHeight;
    BYTE  bBitDepth;
    BYTE  bColourType;
    BYTE  bCompressionType;
    BYTE  bFilterType;
    BYTE  bInterlaceType;
} PNGHeader;

#define PNG_HEADER_SIZE  13

const BYTE PngSignature[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
const char PngName[] = "Software\0blueMSX";

#define PNG_IHDR 0x49484452
#define PNG_IDAT 0x49444154
#define PNG_IEND 0x49454E44
#define PNG_TEXT 0x74455874
#define PNG_SRGB 0x73524742
#define PNG_GAMA 0x67414D41
#define PNG_CHRM 0x6348524D
#define PNG_PHYS 0x70485973

static BYTE srgb[] = { 
    0x00
};

static BYTE gama[] = { 
    0x00, 0x00, 0xb1, 0x8f 
};

static BYTE chrm[] = { 
    0x00, 0x00, 0x7a, 0x26, 0x00, 0x00, 0x80, 0x84,
    0x00, 0x00, 0xfa, 0x00, 0x00, 0x00, 0x80, 0xe8,
    0x00, 0x00, 0x75, 0x30, 0x00, 0x00, 0xea, 0x60,
    0x00, 0x00, 0x3a, 0x98, 0x00, 0x00, 0x17, 0x70
};

static BYTE phys[] = { 
    0x00, 0x00, 0x0e, 0xc4, 0x00, 0x00, 0x0e, 0xc4, 0x01
};


int pngAddChunk(BYTE* dest, int type, const void* data, int length)
{
    UInt32 crc;

    *(DWORD*)(dest + 0) = ntohul(length);
    *(DWORD*)(dest + 4) = ntohul(type);
    memcpy(dest + 8, data, length);

    crc = calcCrc32(dest + 4, length + 4);
    *(DWORD*)(dest + 8 + length) = ntohul(crc);


    return length + 12;
}

void* ScreenShotPng(void* src, int srcPitch, int width, int height, int* bitmapSize)
{
    int   compressedSize;
    BYTE* compressedData;
    int   pngSize;
    BYTE* pngData;
    PNGHeader hdr;
    unsigned short * srcPtr = (unsigned short*)src;
    int w;
    int h;
    int rawSize = (3 * width + 1) * height;
	if (width > 272)
		rawSize *= 2;
    BYTE* rawData = (BYTE*)malloc(rawSize);
    BYTE* dstPtr = rawData;
    printf("width=%d, height=%d\n", width, height);

	for (h = 0; h < height; h++) {
		*dstPtr++ = 0; // Default PNG filter
		for (w = 0; w < width; w++) {
			*dstPtr++ = (BYTE)((srcPtr[w] & 0xf800) >> 8);
			*dstPtr++ = (BYTE)((srcPtr[w] & 0x07e0) >> 3);
			*dstPtr++ = (BYTE)((srcPtr[w] & 0x001f) << 3);
		}
		srcPtr += srcPitch;
	}
    compressedSize = 0;
    compressedData = zipCompress(rawData, rawSize, &compressedSize);
    free(rawData);
    if (compressedData == NULL) {
        return NULL;
    }

    pngData = malloc(compressedSize + 200);
    
    hdr.dwWidth          = ntohul(width);
    hdr.dwHeight         = ntohul(height);
    hdr.bBitDepth        = 8;
    hdr.bColourType      = 2; // RGB
    hdr.bCompressionType = 0;
    hdr.bFilterType      = 0;
    hdr.bInterlaceType   = 0;

    pngSize = 0;

    memcpy(pngData, PngSignature, sizeof(PngSignature));
    pngSize += sizeof(PngSignature);
    pngSize += pngAddChunk(pngData + pngSize, PNG_IHDR, &hdr, PNG_HEADER_SIZE);
#if 0
    pngSize += pngAddChunk(pngData + pngSize, PNG_SRGB, srgb, sizeof(srgb));
    pngSize += pngAddChunk(pngData + pngSize, PNG_GAMA, gama, sizeof(gama));
    pngSize += pngAddChunk(pngData + pngSize, PNG_CHRM, chrm, sizeof(chrm));
    pngSize += pngAddChunk(pngData + pngSize, PNG_PHYS, phys, sizeof(phys));
#endif
    pngSize += pngAddChunk(pngData + pngSize, PNG_IDAT, compressedData, compressedSize);
    pngSize += pngAddChunk(pngData + pngSize, PNG_TEXT, PngName, sizeof(PngName)-1);
    pngSize += pngAddChunk(pngData + pngSize, PNG_IEND, NULL, 0);

    free(compressedData);

    *bitmapSize = pngSize;
    return pngData;
}