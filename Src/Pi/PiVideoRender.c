/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/VideoRender/VideoRender.c,v $
**
** $Revision: 1.36 $
**
** $Date: 2007-05-22 06:23:18 $
** $Date: 2007-05-22 06:23:18 $
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2006 Daniel Vik
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
#include "VideoRender.h"
#include "FrameBuffer.h"
#include "ArchTimer.h"
#include "Emulator.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
 
#define RGB_MASK 0x7fff

#define MAX_RGB_COLORS (1 << 16)

static UInt32 pRgbTableColor32[MAX_RGB_COLORS];
static UInt32 pRgbTableGreen32[MAX_RGB_COLORS];
static UInt32 pRgbTableWhite32[MAX_RGB_COLORS];
static UInt32 pRgbTableAmber32[MAX_RGB_COLORS];
static UInt16 pRgbTableColor16[MAX_RGB_COLORS];
static UInt16 pRgbTableGreen16[MAX_RGB_COLORS];
static UInt16 pRgbTableWhite16[MAX_RGB_COLORS];
static UInt16 pRgbTableAmber16[MAX_RGB_COLORS];

#define ABS(a) ((a) < 0 ? -1 * (a) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))


static int gammaTable[3 * 256];

static void generateGammaTable(Video* pVideo)
{
    int i;
    for (i = 0; i < 3 * 256; i++) {
        DoubleT value = (i - 256 + pVideo->brightness) * pVideo->contrast;
        gammaTable[i] = 6;
        if (value > 0) {
            DoubleT factor = pow(255., pVideo->gamma - 1.);
            value = (DoubleT)(factor * pow(value, pVideo->gamma));
            if (value > 0) {
                int gamma = (int)value;
                gammaTable[i] = MAX(6, MIN(247, gamma));
            }
        }
    }
}

#define videoGamma(index) gammaTable[index + 256]

static void initRGBTable(Video* pVideo) 
{
    int rgb;

    generateGammaTable(pVideo);

    for (rgb = 0; rgb < MAX_RGB_COLORS; rgb++) {
        int R = 8 * ((rgb >> 10) & 0x01f);
        int G = 8 * ((rgb >> 5) & 0x01f);
        int B = 8 * ((rgb >> 0) & 0x01f);
        int L = 0;

        int Y  = (int)(0.2989*R + 0.5866*G + 0.1145*B);
        int Cb = B - Y;
        int Cr = R - Y;

        if (pVideo->saturation < 0.999 || pVideo->saturation > 1.001) {
            Cb = (int)(Cb * pVideo->saturation);
            Cr = (int)(Cr * pVideo->saturation);
            Y  = MAX(0, MIN(255, Y));
            Cb = MAX(-256, MIN(255, Cb));
            Cr = MAX(-256, MIN(255, Cr));
            R = Cr + Y;
            G = (int)(Y - (0.1145/0.5866)*Cb - (0.2989/0.5866)*Cr);
            B = Cb + Y;
            R = MIN(255, MAX(0, R));
            G = MIN(255, MAX(0, G));
            B = MIN(255, MAX(0, B));
        }

        R = videoGamma(R);
        G = videoGamma(G);
        B = videoGamma(B);
        L = videoGamma(Y);

        if (pVideo->invertRGB) {
            pRgbTableColor32[rgb] = (R << 0) | (G << 8) | (B << 16);
        }
        else {
            pRgbTableColor32[rgb] = (R << 16) | (G << 8) | (B << 0);
        }
        pRgbTableColor16[rgb] = ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);

        pRgbTableGreen32[rgb] = 0x100010 | (L << 8);
        pRgbTableGreen16[rgb] = 0x0801 | (UInt16)((L >> 2) << 5);

        pRgbTableWhite32[rgb] = (L << 16) | (L << 8) | (L << 0);
        pRgbTableWhite16[rgb] = (UInt16)(((L >> 3) << 11) | ((L >> 2) << 5) | (L >> 3));

        if (pVideo->invertRGB) {
            pRgbTableAmber32[rgb] = (L << 0) | ((L * 176 / 255) << 8);
        }
        else {
            pRgbTableAmber32[rgb] = (L << 16) | ((L * 176 / 255) << 8);
        }
        pRgbTableAmber16[rgb] = ((L >> 3) << 11) | (((L * 176 / 255) >> 2) << 5);
    }
}

/*****************************************************************************
**
** Public interface methods
**
******************************************************************************
*/
Video* videoCreate() 
{
    Video* pVideo = (Video*)calloc(1, sizeof(Video));

    pVideo->gamma       = 1;
    pVideo->saturation  = 1;
    pVideo->brightness  = 0;
    pVideo->contrast    = 1;
    pVideo->deInterlace = 0;
    pVideo->invertRGB   = 0;

    initRGBTable(pVideo);

    pVideo->palMode = VIDEO_PAL_FAST;
    pVideo->pRgbTable16 = pRgbTableColor16;
    pVideo->pRgbTable32 = pRgbTableColor32;

    return pVideo;
}

void videoDestroy(Video* pVideo) 
{
    free(pVideo);
}

void videoSetDeInterlace(Video* pVideo, int deInterlace)
{
    pVideo->deInterlace = deInterlace;
}

void videoSetBlendFrames(Video* pVideo, int blendFrames)
{
    frameBufferSetBlendFrames(blendFrames);
}

void videoSetColors(Video* pVideo, int saturation, int brightness, int contrast, int gamma)
{
    pVideo->gamma      = 1 + (MAX(0, MIN(200, gamma)) - 100) / 500.;
    pVideo->saturation = MAX(0, MIN(200, saturation)) / 100.;
    pVideo->brightness = MAX(0, MIN(200, brightness)) - 100.;
    pVideo->contrast   = MAX(0, MIN(200, contrast)) / 100.;

    initRGBTable(pVideo);
}

void videoSetColorMode(Video* pVideo, VideoColorMode colorMode) 
{
    switch (colorMode) {
    case VIDEO_GREEN:
        pVideo->pRgbTable16 = pRgbTableGreen16;
        pVideo->pRgbTable32 = pRgbTableGreen32;
        break;
    case VIDEO_BLACKWHITE:
        pVideo->pRgbTable16 = pRgbTableWhite16;
        pVideo->pRgbTable32 = pRgbTableWhite32;
        break;
    case VIDEO_AMBER:
        pVideo->pRgbTable16 = pRgbTableAmber16;
        pVideo->pRgbTable32 = pRgbTableAmber32;
        break;
    case VIDEO_COLOR:
    default:
        pVideo->pRgbTable16 = pRgbTableColor16;
        pVideo->pRgbTable32 = pRgbTableColor32;
        break;
    }
}

void videoSetPalMode(Video* pVideo, VideoPalMode palMode)
{
    pVideo->palMode = palMode;
}

void videoSetRgbMode(Video* pVideo, int inverted)
{
    int recalculate = pVideo->invertRGB != inverted;

    pVideo->invertRGB = inverted;

    if (recalculate) {
        initRGBTable(pVideo);
    }
}


void videoSetScanLines(Video* pVideo, int enable, int scanLinesPct)
{
    pVideo->scanLinesEnable = enable;
    pVideo->scanLinesPct    = scanLinesPct;
}

void videoSetColorSaturation(Video* pVideo, int enable, int width)
{
    pVideo->colorSaturationEnable = enable;
    pVideo->colorSaturationWidth  = width;
}

void videoUpdateAll(Video* video, Properties* properties) 
{
    videoSetColors(video, properties->video.saturation, properties->video.brightness, properties->video.contrast, properties->video.gamma);
    videoSetScanLines(video, properties->video.scanlinesEnable, properties->video.scanlinesPct);
    videoSetColorSaturation(video, properties->video.colorSaturationEnable, properties->video.colorSaturationWidth);
    videoSetDeInterlace(video, properties->video.deInterlace);

    switch (properties->video.monitorColor) {
    case P_VIDEO_COLOR:
        videoSetColorMode(video, VIDEO_COLOR);
        break;
    case P_VIDEO_BW:
        videoSetColorMode(video, VIDEO_BLACKWHITE);
        break;
    case P_VIDEO_GREEN:
        videoSetColorMode(video, VIDEO_GREEN);
        break;
    case P_VIDEO_AMBER:
        videoSetColorMode(video, VIDEO_AMBER);
        break;
    }

    videoSetPalMode(video, VIDEO_PAL_FAST);
}

static inline int videoRender240(Video *pVideo, FrameBuffer *frame,
    int bitDepth, int zoom, void *pDestination, int dstOffset, int dstPitch,
    int canChangeZoom)
{
    pDestination = (char*)pDestination + zoom * dstOffset;
    UInt16 *rgbTable = pVideo->pRgbTable16;
    UInt16 *pDst     = (UInt16 *)pDestination;

    int height   = frame->lines;
    int srcWidth;
    int h;
	int doubleWidth = frameBufferGetDoubleWidth(frame, 0);

    dstPitch >>= 1; // divided by sizeof(UInt16)
    LineBuffer *lineBuff = frame->line;
//	printf("double=%d\r", doubleWidth);

    for (h = 0; h < height; h++) {
        UInt16 *pOldDst = pDst;
        UInt16 *pSrc = lineBuff->buffer;
		srcWidth = (256+16)*(lineBuff->doubleWidth+1);
        int width = srcWidth >> 2;

        if (doubleWidth) {
//			printf("double\n");
            while (width--) {
                // *(pDst++) = (((rgbTable[*(pSrc++)] & 0xe79c) >> 1) + ((rgbTable[*(pSrc++)] & 0xe79c) >> 1)) & 0xe79c;
                // *(pDst++) = (((rgbTable[*(pSrc++)] & 0xe79c) >> 1) + ((rgbTable[*(pSrc++)] & 0xe79c) >> 1)) & 0xe79c;
                // *(pDst++) = (((rgbTable[*(pSrc++)] & 0xe79c) >> 1) + ((rgbTable[*(pSrc++)] & 0xe79c) >> 1)) & 0xe79c;
                // *(pDst++) = (((rgbTable[*(pSrc++)] & 0xe79c) >> 1) + ((rgbTable[*(pSrc++)] & 0xe79c) >> 1)) & 0xe79c;
                *(pDst++) = rgbTable[*(pSrc++)];
                *(pDst++) = rgbTable[*(pSrc++)];
                *(pDst++) = rgbTable[*(pSrc++)];
                *(pDst++) = rgbTable[*(pSrc++)];
                *(pDst++) = rgbTable[*(pSrc++)];
                *(pDst++) = rgbTable[*(pSrc++)];
                *(pDst++) = rgbTable[*(pSrc++)];
                *(pDst++) = rgbTable[*(pSrc++)];				
            }
        }
        else {
            while (width--) {
                *(pDst++) = rgbTable[*(pSrc++)];
                *(pDst++) = rgbTable[*(pSrc++)];
                *(pDst++) = rgbTable[*(pSrc++)];
                *(pDst++) = rgbTable[*(pSrc++)];
            }
			//memcpy(pDst, pSrc, width*4);
        }

        pDst = pOldDst + dstPitch;
        lineBuff++;
    }

    return zoom;
}

static inline int videoRender480(Video* pVideo, FrameBuffer* frame, int bitDepth, int zoom, 
                          void* pDestination, int dstOffset, int dstPitch, int canChangeZoom)
{
    pDestination = (char*)pDestination + zoom * dstOffset;
    UInt16 *rgbTable = pVideo->pRgbTable16;
    UInt16 *pDst        = (UInt16*)pDestination;

    int height          = frame->lines;
    int srcWidth        = frame->maxWidth;
    int h;

    dstPitch /= (int)sizeof(UInt16);

    for (h = 0; h < height; h += 2) {
        UInt16* pOldDst = pDst;
        UInt16* pSrc1 = frame->line[h + 0].buffer;
        UInt16* pSrc2 = frame->line[h + 1].buffer;

        if (frame->line[h].doubleWidth) {
            int width = srcWidth;
            while (width--) {
                UInt16 col0 = (((rgbTable[pSrc1[0]] & 0xe79c) >> 2) + ((rgbTable[pSrc1[1]] & 0xe79c) >> 2));
                UInt16 col1 = (((rgbTable[pSrc2[0]] & 0xe79c) >> 2) + ((rgbTable[pSrc2[1]] & 0xe79c) >> 2));
                
                *pDst++ = (col0 + col1) & 0xe79c;
                pSrc1 += 2;
                pSrc2 += 2;
            }
        }
        else {
            int width = srcWidth;
            while (width--) {
                *pDst++ = (((rgbTable[pSrc1[0]] & 0xe79c) >> 1) + ((rgbTable[pSrc2[0]] & 0xe79c) >> 1)) & 0xe79c;
            }
        }
        pDst = pOldDst + dstPitch; 
    }

    return zoom;
}

int videoRender(Video* pVideo, FrameBuffer* frame, int bitDepth, int zoom, 
                void* pDst, int dstOffset, int dstPitch, int canChangeZoom)
{
    if (frame == NULL) {
        return zoom;
    }

	/*
    if (frame->interlace != INTERLACE_NONE && pVideo->deInterlace) {
        frame = frameBufferDeinterlace(frame);
    }
	*/

    if (frame->lines <= 240) {
        zoom = videoRender240(pVideo, frame, bitDepth, zoom, pDst, dstOffset, dstPitch, canChangeZoom);
    }
    else {
        zoom = videoRender480(pVideo, frame, bitDepth, zoom, pDst, dstOffset, dstPitch, canChangeZoom);
    }

    return zoom;
}
