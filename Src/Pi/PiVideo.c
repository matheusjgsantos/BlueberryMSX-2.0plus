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
** GLES code is based on
** https://sourceforge.net/projects/atari800/ and
** https://code.google.com/p/pisnes/
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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "Properties.h"
#include "VideoRender.h"

//#include <interface/vchiq_arm/vchiq_if.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>


typedef	struct ShaderInfo {
	GLuint program;
	GLint a_position;
	GLint a_texcoord;
	GLint u_vp_matrix;
	GLint u_texture;
	GLboolean scanline;
} ShaderInfo;

#define	TEX_WIDTH  600
#define	TEX_HEIGHT 480

#define BIT_DEPTH       16
#define BYTES_PER_PIXEL (BIT_DEPTH >> 3)
#define ZOOM            1
#define	WIDTH           544
#define	HEIGHT          480

#define	minU 0.0f
#define	maxU ((float)WIDTH / TEX_WIDTH)	
#define	minV 0.0f
#define	maxV ((float)HEIGHT / TEX_HEIGHT)

// Get the EGL error back as a string. Useful for debugging.
static const char *eglGetErrorStr()
{
    switch (eglGetError())
    {
    case EGL_SUCCESS:
        return "The last function succeeded without error.";
    case EGL_NOT_INITIALIZED:
        return "EGL is not initialized, or could not be initialized, for the "
               "specified EGL display connection.";
    case EGL_BAD_ACCESS:
        return "EGL cannot access a requested resource (for example a context "
               "is bound in another thread).";
    case EGL_BAD_ALLOC:
        return "EGL failed to allocate resources for the requested operation.";
    case EGL_BAD_ATTRIBUTE:
        return "An unrecognized attribute or attribute value was passed in the "
               "attribute list.";
    case EGL_BAD_CONTEXT:
        return "An EGLContext argument does not name a valid EGL rendering "
               "context.";
    case EGL_BAD_CONFIG:
        return "An EGLConfig argument does not name a valid EGL frame buffer "
               "configuration.";
    case EGL_BAD_CURRENT_SURFACE:
        return "The current surface of the calling thread is a window, pixel "
               "buffer or pixmap that is no longer valid.";
    case EGL_BAD_DISPLAY:
        return "An EGLDisplay argument does not name a valid EGL display "
               "connection.";
    case EGL_BAD_SURFACE:
        return "An EGLSurface argument does not name a valid surface (window, "
               "pixel buffer or pixmap) configured for GL rendering.";
    case EGL_BAD_MATCH:
        return "Arguments are inconsistent (for example, a valid context "
               "requires buffers not supplied by a valid surface).";
    case EGL_BAD_PARAMETER:
        return "One or more argument values are invalid.";
    case EGL_BAD_NATIVE_PIXMAP:
        return "A NativePixmapType argument does not refer to a valid native "
               "pixmap.";
    case EGL_BAD_NATIVE_WINDOW:
        return "A NativeWindowType argument does not refer to a valid native "
               "window.";
    case EGL_CONTEXT_LOST:
        return "A power management event has occurred. The application must "
               "destroy all contexts and reinitialise OpenGL ES state and "
               "objects to continue rendering.";
    default:
        break;
    }
    return "Unknown error!";
}

extern Video *video;
extern Properties *properties;

static void drawQuad(const ShaderInfo *sh);
static GLuint createShader(GLenum type, const char *shaderSrc);
static GLuint createProgram(const char *vertexShaderSrc, const char *fragmentShaderSrc);
static void setOrtho(float m[4][4],
	float left, float right, float bottom, float top,
	float near, float far, float scaleX, float scaleY);

//static EGL_DISPMANX_WINDOW_T nativeWindow;
//static EGLDisplay display = NULL;
EGLDisplay display;
//static EGLSurface surface = NULL;
EGLSurface surface;
//static EGLContext context = NULL;
EGLContext context;

//uint32_t screenWidth = 0;
//uint32_t screenHeight = 0;
uint32_t screenWidth = 800;
uint32_t screenHeight = 600;

static ShaderInfo shader;
static GLuint buffers[3];
static GLuint textures[2];

static SDL_Surface *sdlScreen;

char *msxScreen = NULL;
int msxScreenPitch;
int height;

static const char* vertexShaderSrc =
        "#version 100\n"
        "uniform mat4 u_vp_matrix;\n"
        "uniform bool scanline;\n"
        "attribute vec4 a_position;\n"
        "attribute vec2 a_texcoord;\n"
        "attribute vec4 in_Colour;\n"
        "varying vec4 v_vColour;\n"
        "varying vec4 TEX0;\n"
        "varying mediump vec2 v_texcoord;\n"
        "void main() {\n"
        "       v_texcoord = a_texcoord;\n"
        "       v_vColour = in_Colour;\n"
        "   if (scanline)\n"
        "       {\n"
        "       gl_Position = a_position.x*u_vp_matrix[0] + a_position.y*u_vp_matrix[1] + a_position.z*u_vp_matrix[2] + a_position.w*u_vp_matrix[3];\n"
        "       TEX0.xy = a_position.xy;\n"
        "       } else {"
        "               gl_Position = u_vp_matrix * a_position;\n"
        "       }\n"
        "}\n";

static const char* fragmentShaderSrc =
        "#version 100\n"
        "varying mediump vec2 v_texcoord;\n"
        "uniform bool scanline;\n"
        "uniform sampler2D u_texture;\n"
        "varying mediump vec4 TEX0;\n"
        "uniform mediump vec2 TextureSize;\n"
        "void main() {\n"
        "   if (scanline)\n"
        "       {\n"
        "               mediump vec3 col;\n"
        "               mediump float x = TEX0.x * TextureSize.x;\n"
        "               mediump float y = floor(gl_FragCoord.y / 3.0) + 0.5;\n"
        "               mediump float ymod = mod(gl_FragCoord.y, 3.0);\n"
        "               mediump vec2 f0 = vec2(x, y);\n"
        "               mediump vec2 uv0 = f0 / TextureSize.xy;\n"
        "               mediump vec3 t0 = texture2D(u_texture, v_texcoord).xyz;\n"
        "               if (ymod > 2.0) {\n"
        "               mediump vec2 f1 = vec2(x, y + 1.0);\n"
        "               mediump vec2 uv1 = f1 / TextureSize.xy;\n"
        "               mediump vec3 t1 = texture2D(u_texture, uv1).xyz * 0.1;\n"
        "               col = (t0 + t1) / 1.6;\n"
        "               } else {\n"
        "               col = t0;\n"
        "               } \n"
        "               gl_FragColor = vec4(col, 1.0);\n"
        "       } else {"
        "               gl_FragColor = texture2D(u_texture, v_texcoord);\n"
        "       }\n"
        "}\n";


static const GLfloat uvs[] = {
	minU, minV,
	maxU, minV,
	maxU, maxV,
	minU, maxV,
};

static const GLushort indices[] = {
	0, 1, 2,
	0, 2, 3,
};

static const int kVertexCount = 4;
static const int kIndexCount = 6;

static float projection[4][4];

static const GLfloat vertices[] = {
	-0.5, -0.5f, 0.0f,
	+0.5f, -0.5f, 0.0f,
	+0.5f, +0.5f, 0.0f,
	-0.5, +0.5f, 0.0f,
};

// Extracted from triangle_rpi4.c
int device;
drmModeModeInfo mode;
struct gbm_device *gbmDevice;
struct gbm_surface *gbmSurface;
drmModeCrtc *crtc;
uint32_t connectorId;

static drmModeConnector *getConnector(drmModeRes *resources)
{
    for (int i = 0; i < resources->count_connectors; i++)
    {
        drmModeConnector *connector = drmModeGetConnector(device, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED)
        {
            return connector;
        }
        drmModeFreeConnector(connector);
    }

    return NULL;
}

static drmModeEncoder *findEncoder(drmModeConnector *connector)
{
    if (connector->encoder_id)
    {
        return drmModeGetEncoder(device, connector->encoder_id);
    }
    return NULL;
}

static int getDisplay(EGLDisplay *display)
{
    drmModeRes *resources = drmModeGetResources(device);
    if (resources == NULL)
    {
        fprintf(stderr, "Unable to get DRM resources\n");
        return -1;
    }

    drmModeConnector *connector = getConnector(resources);
    if (connector == NULL)
    {
        fprintf(stderr, "Unable to get connector\n");
        drmModeFreeResources(resources);
        return -1;
    }

    connectorId = connector->connector_id;
    mode = connector->modes[0];
    printf("resolution: %ix%i\n", mode.hdisplay, mode.vdisplay);

    drmModeEncoder *encoder = findEncoder(connector);
    if (encoder == NULL)
    {
        fprintf(stderr, "Unable to get encoder\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        return -1;
    }

    crtc = drmModeGetCrtc(device, encoder->crtc_id);
    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    gbmDevice = gbm_create_device(device);
    gbmSurface = gbm_surface_create(gbmDevice, mode.hdisplay, mode.vdisplay, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    *display = eglGetDisplay(gbmDevice);
    return 0;
}

static int matchConfigToVisual(EGLDisplay display, EGLint visualId, EGLConfig *configs, int count)
{
    EGLint id;
    for (int i = 0; i < count; ++i)
    {
        if (!eglGetConfigAttrib(display, configs[i], EGL_NATIVE_VISUAL_ID, &id))
            continue;
        if (id == visualId)
            return i;
    }
    return -1;
}

static struct gbm_bo *previousBo = NULL;
static uint32_t previousFb;

static void gbmSwapBuffers(EGLDisplay *display, EGLSurface *surface)
{
    //fprintf(stderr,"display: %p, surface: %p\n",display, surface);
    eglSwapBuffers(*display, *surface);
    //fprintf(stderr,"eglSwapBufffer returned %s\n", eglGetErrorStr());
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(gbmSurface);
    //fprintf(stderr,"gbm_surface_lock_front returned %lu\n",bo);
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    //fprintf(stderr,"gbm_bo_get_handle returned %lu\n",handle);
    uint32_t pitch = gbm_bo_get_stride(bo);
    //fprintf(stderr,"gbm_bo_get_pitch returned %lu\n",pitch);
    uint32_t fb;
    drmModeAddFB(device, mode.hdisplay, mode.vdisplay, 24, 32, pitch, handle, &fb);
    // for tests purpose - drmModeAddFB(device, mode.hdisplay, mode.vdisplay, 24, 32, 3328, 1, &fb);
    drmModeSetCrtc(device, crtc->crtc_id, fb, 0, 0, &connectorId, 1, &mode);

    if (previousBo)
    {
        drmModeRmFB(device, previousFb);
        gbm_surface_release_buffer(gbmSurface, previousBo);
    }
    previousBo = bo;
    previousFb = fb;
}

static void gbmClean()
{
    // set the previous crtc
    drmModeSetCrtc(device, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &connectorId, 1, &crtc->mode);
    drmModeFreeCrtc(crtc);

    if (previousBo)
    {
        drmModeRmFB(device, previousFb);
        gbm_surface_release_buffer(gbmSurface, previousBo);
    }

    gbm_surface_destroy(gbmSurface);
    gbm_device_destroy(gbmDevice);
}

// The following code was adopted from
// https://github.com/matusnovak/rpi-opengl-without-x/blob/master/triangle.c
// and is licensed under the Unlicense.
static const EGLint configAttribs[] = {
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_DEPTH_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE};

static const EGLint contextAttribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE};

int piInitVideo()
{
    //bcm_host_init();

    //EGLDisplay display;
    //You can try chaning this to "card0" if "card1" does not work.
    device = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (getDisplay(&display) != 0)
    {
        close(device);
        fprintf(stderr, "Unable to get EGL display using /dev/dri/card0, trying next\n");
    	device = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);
    	if (getDisplay(&display) != 0)
    	{
        	fprintf(stderr, "Unable to get EGL display using /dev/dri/card0, trying next\n");
		close(device);
		return EXIT_FAILURE;
	}
    }

    //We will use the screen resolution as the desired width and height for the viewport.
    //int desiredWidth = mode.hdisplay;
    //int desiredHeight = mode.vdisplay;
    int desiredWidth = 800;
    int desiredHeight = 600;

    // Other variables we will need further down the code.
    int major, minor;
    GLuint program, vert, frag, vbo;
    GLint posLoc, colorLoc, result;

   if (eglInitialize(display, &major, &minor) == EGL_FALSE)
    {
        fprintf(stderr, "Failed to get EGL version! Error: %s\n",
                eglGetErrorStr());
        eglTerminate(display);
        gbmClean();
        return EXIT_FAILURE;
    }

    // Make sure that we can use OpenGL in this EGL app.
    eglBindAPI(EGL_OPENGL_API);

    printf("Initialized EGL version: %d.%d\n", major, minor);

    EGLint count;
    EGLint numConfigs;
    eglGetConfigs(display, NULL, 0, &count);
    EGLConfig *configs = malloc(count * sizeof(configs));

    if (!eglChooseConfig(display, configAttribs, configs, count, &numConfigs))
    {
        fprintf(stderr, "Failed to get EGL configs! Error: %s\n",
                eglGetErrorStr());
        eglTerminate(display);
        gbmClean();
        return EXIT_FAILURE;
    }

    // I am not exactly sure why the EGL config must match the GBM format.
    // But it works!
    int configIndex = matchConfigToVisual(display, GBM_FORMAT_XRGB8888, configs, numConfigs);
    if (configIndex < 0)
    {
        fprintf(stderr, "Failed to find matching EGL config! Error: %s\n",
                eglGetErrorStr());
        eglTerminate(display);
        gbm_surface_destroy(gbmSurface);
        gbm_device_destroy(gbmDevice);
        return EXIT_FAILURE;
    }

    EGLContext context =
        eglCreateContext(display, configs[configIndex], EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT)
    {
        fprintf(stderr, "Failed to create EGL context! Error: %s\n",
                eglGetErrorStr());
        eglTerminate(display);
        gbmClean();
        return EXIT_FAILURE;
    }

    //EGLSurface surface =
    EGLint attribList[] = 
    {
	    EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
	    EGL_NONE
    };
    //surface = eglCreateWindowSurface(display, configs[configIndex], gbmSurface, NULL);
    surface = eglCreateWindowSurface(display, configs[configIndex], gbmSurface, attribList);
    if (surface == EGL_NO_SURFACE)
    {
        fprintf(stderr, "Failed to create EGL surface! Error: %s\n",
                eglGetErrorStr());
        eglDestroyContext(display, context);
        eglTerminate(display);
        gbmClean();
        return EXIT_FAILURE;
    } else {
	    fprintf(stderr,"eglCreateWindowSurface returned %s\n",eglGetErrorStr());
    }

    free(configs);
    result = eglMakeCurrent(display, surface, surface, context);
    if (result == EGL_FALSE) {
	    fprintf(stderr, "eglMakeCurrent() failed: EGL_FALSE\n"); 
	    return 0;
    } else {
	    fprintf(stderr,"eglMakeCurrent returned &s\n",eglGetErrorStr());
    }
    
    // Set GL Viewport size, always needed!
    glViewport(0, 0, desiredWidth, desiredHeight);

    // Get GL Viewport size and test if it is correct.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // viewport[2] and viewport[3] are viewport width and height respectively
    printf("GL Viewport size: %dx%d\n", viewport[2], viewport[3]);

    if (viewport[2] != desiredWidth || viewport[3] != desiredHeight)
    {
        fprintf(stderr, "Error! The glViewport returned incorrect values! Something is wrong!\n");
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        gbmClean();
        return EXIT_FAILURE;
    }

    // Clear whole screen (front buffer)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/*Replaced by triangle_rpi4 display initialization
	// get an EGL display connection
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY) {
		fprintf(stderr, "eglGetDisplay() failed: EGL_NO_DISPLAY\n");
		return 0;
	} else {
		fprintf(stderr, "eglDisplay() returned %lu\n",display);
	}

	// initialize the EGL display connection
	EGLBoolean result = eglInitialize(display, NULL, NULL);
	if (result == EGL_FALSE) {
		fprintf(stderr, "eglInitialize() failed: EGL_FALSE %lu\n",eglGetError());
		//return 0;
	}

	// get an appropriate EGL frame buffer configuration
	EGLint numConfig;
	EGLConfig config;
	static const EGLint attributeList[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};
	result = eglChooseConfig(display, attributeList, &config, 1, &numConfig);
	if (result == EGL_FALSE) {
		fprintf(stderr, "eglChooseConfig() failed: EGL_FALSE\n");
		return 0;
	}

	result = eglBindAPI(EGL_OPENGL_ES_API);
	if (result == EGL_FALSE) {
		fprintf(stderr, "eglBindAPI() failed: EGL_FALSE\n");
		return 0;
	}

	// create an EGL rendering context
	static const EGLint contextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
	if (context == EGL_NO_CONTEXT) {
		fprintf(stderr, "eglCreateContext() failed: EGL_NO_CONTEXT\n");
		return 0;
	}

	// create an EGL window surface
	int32_t success = graphics_get_display_size(0, &screenWidth, &screenHeight);
	if (result < 0) {
		fprintf(stderr, "graphics_get_display_size() failed: < 0\n");
		return 0;
	}

	printf( "Width/height: %d/%d\n", screenWidth, screenHeight);
	if (screenHeight < 600 && video)
		video->scanLinesEnable = 0;

	//VC_RECT_T dstRect;
	dstRect.x = 0;
	dstRect.y = 0;
	dstRect.width = screenWidth;
	dstRect.height = screenHeight;

	//VC_RECT_T srcRect;
	srcRect.x = 0;
	srcRect.y = 0;
	srcRect.width = screenWidth << 16;
	srcRect.height = screenHeight << 16;

	DISPMANX_DISPLAY_HANDLE_T dispManDisplay = vc_dispmanx_display_open(0);
	DISPMANX_UPDATE_HANDLE_T dispmanUpdate = vc_dispmanx_update_start(0);
	/DISPMANX_ELEMENT_HANDLE_T dispmanElement = vc_dispmanx_element_add(dispmanUpdate,
		dispManDisplay, 0, &dstRect, 0, &srcRect,
		DISPMANX_PROTECTION_NONE, NULL, NULL, DISPMANX_NO_ROTATE);

	nativeWindow.element = dispmanElement;
	nativeWindow.width = screenWidth;
	nativeWindow.height = screenHeight;
	vc_dispmanx_update_submit_sync(dispmanUpdate);

	fprintf(stderr, "Initializing window surface...\n");

	surface = eglCreateWindowSurface(display, config, &nativeWindow, NULL);
	if (surface == EGL_NO_SURFACE) {
		fprintf(stderr, "eglCreateWindowSurface() failed: EGL_NO_SURFACE\n");
		return 0;
	}

	fprintf(stderr, "Connecting context to surface...\n");

	// connect the context to the surface
	result = eglMakeCurrent(display, surface, surface, context);
	if (result == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent() failed: EGL_FALSE\n");
		return 0;
	}
	*/

	fprintf(stderr, "Initializing shaders...\n");

	// Init shader resources
	memset(&shader, 0, sizeof(ShaderInfo));
	shader.program = createProgram(vertexShaderSrc, fragmentShaderSrc);
	if (!shader.program) {
		fprintf(stderr, "createProgram() failed\n");
		return 0;
	}

	fprintf(stderr, "Initializing textures/buffers...\n");

	shader.a_position	= glGetAttribLocation(shader.program,	"a_position");
	shader.a_texcoord	= glGetAttribLocation(shader.program,	"a_texcoord");
	shader.u_vp_matrix	= glGetUniformLocation(shader.program,	"u_vp_matrix");
	shader.u_texture	= glGetUniformLocation(shader.program,	"u_texture");
	shader.scanline		= glGetUniformLocation(shader.program,  "scanline");
	
	glGenTextures(1, textures);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);

	glGenBuffers(3, buffers);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 3, vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 2, uvs, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, kIndexCount * sizeof(GL_UNSIGNED_SHORT), indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);
	
	float sx = 1.0f;
	float sy = 1.0f;
	float zoom = (float)ZOOM;

	// Screen aspect ratio adjustment
	float a = (float)screenWidth / screenHeight;
	float a0 = (float)WIDTH / (float)HEIGHT;

	if (a > a0) {
		sx = a0/a;
	} else {
		sy = a/a0;
	}

	setOrtho(projection, -0.5f, +0.5f, +0.5f, -0.5f, -1.0f, 1.0f,
		sx * zoom, sy * zoom);

	fprintf(stderr, "Setting up screen...\n");

	msxScreenPitch = WIDTH * BIT_DEPTH / 8;
	msxScreen = (char*)calloc(1, BIT_DEPTH / 8 * TEX_WIDTH * TEX_HEIGHT);
	if (!msxScreen) {
		fprintf(stderr, "Error allocating screen texture\n");
		return 0;
	}

	fprintf(stderr, "Initializing SDL video...\n");

	// We're doing our own video rendering - this is just so SDL-based keyboard
	// can work
	SDL_Init(SDL_INIT_EVERYTHING);
        //SDL_VideoInit("fbdev", 0);
	//sdlScreen = SDL_SetVideoMode(0, 0, 0, 0);//SDL_ASYNCBLIT);

	//SDL_VideoInit("KMSDRM");

        //sdlScreen = SDL_CreateWindow("BlueberryMSX", 0, 0, 0, 0, NULL);
        /*sdlScreen = SDL_CreateWindow("BlueberryMSX", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_OPENGL);

	fprintf(stderr,"After SDL_CreateWindow\n");
    	gbmSwapBuffers(&display,&surface);
    	getchar();
	*/

        SDL_ShowCursor(SDL_DISABLE);

	fprintf(stderr,"PyInitVideo returning 1\n");
	return 1;
}

void piDestroyVideo()
{
	if (sdlScreen) {
		SDL_FreeSurface(sdlScreen);
	}
	if (msxScreen) {
		free(msxScreen);
	}

	// Destroy shader resources
	if (shader.program) {
		glDeleteProgram(shader.program);
		glDeleteBuffers(3, buffers);
		glDeleteTextures(1, textures);
	}
	
	// Release OpenGL resources
	if (display) {
		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroySurface(display, surface);
		eglDestroyContext(display, context);
		eglTerminate(display);
		//Added gbm clean up
		gbmClean();
	}
 
	//bcm_host_deinit();
}

int width = -1;
int lines = -1;
int interlace = -1;

void piUpdateEmuDisplay()
{
	int w = 0;
	if (!shader.program) {
		fprintf(stderr, "Shader not initialized\n");
		return;
	}

	glClear(GL_COLOR_BUFFER_BIT);
	if (properties->video.force4x3ratio)
		w = (screenWidth - (screenHeight*4/3.0f));
	if (w < 0) w = 0;
	glViewport(w/2, 0, screenWidth-w, screenHeight);

	ShaderInfo *sh = &shader;

	glDisable(GL_BLEND);
	glUseProgram(sh->program);

	FrameBuffer* frameBuffer;
	frameBuffer = frameBufferFlipViewFrame(properties->emulation.syncMethod == P_EMU_SYNCTOVBLANKASYNC);
	if (frameBuffer == NULL) {
		frameBuffer = frameBufferGetWhiteNoiseFrame();
	}
	videoRender(video, frameBuffer, BIT_DEPTH, 1, msxScreen, 0, msxScreenPitch*2, -1);
//	int borderWidth = ((int)((WIDTH - frameBuffer->maxWidth)  * ZOOM)) >> 1;
//	if (borderWidth < 0)
//		borderWidth = 0;

	
//	videoRender(video, frameBuffer, BIT_DEPTH, 1,
//				msxScreen + borderWidth * BYTES_PER_PIXEL, 0, msxScreenPitch, -1);

	glUniform1i(shader.scanline, video->scanLinesEnable);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[0]);

	// if (borderWidth > 0) {
	// 	int h = height;
	// 	while (h--) {
	// 		memset(dpyData, 0, borderWidth * BYTES_PER_PIXEL);
	// 		memset(dpyData + (width - borderWidth) * BYTES_PER_PIXEL, 0, borderWidth * BYTES_PER_PIXEL);
	// 		dpyData += msxScreenPitch;
	// 	}
	// }

//	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT,
//					GL_RGB, GL_UNSIGNED_SHORT_5_6_5, msxScreen);

	glTexSubImage2D(GL_TEXTURE_2D, 0, (WIDTH-msxScreenPitch)/2, 0, msxScreenPitch, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, msxScreen);
//	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, msxScreenPitch, lines, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, msxScreen);
	if (frameBufferGetDoubleWidth(frameBuffer, 0) != width || height != frameBuffer->lines)
	{
		width = frameBufferGetDoubleWidth(frameBuffer, 0);
		height = frameBuffer->lines;
		msxScreenPitch = frameBuffer->maxWidth * (width+1);//(256+16)*(width+1);
		interlace = frameBuffer->interlace;
		float sx = 1.0f * msxScreenPitch/WIDTH;
		float sy = 1.0f * height / HEIGHT;
//		printf("screen = %x, width = %d, height = %d, double = %d, interlaced = %d\n", msxScreen, msxScreenPitch, height, width, interlace);
//		printf("sx=%f,sy=%f\n", sx, sy);
		fflush(stdin);
		if (sy == 1.0f)
			setOrtho(projection, -sx/2, sx/2,  sy/2, -sy/2, -0.5f, +0.5f,1,1);		
		else
			setOrtho(projection, -sx/2, sx/2,    0,   -sy, -0.5f, +0.5f,1,1);		
		//setOrtho(projection, -1, 1,    1,   -1, -0.5f, +0.5f,1,1);		
		glUniformMatrix4fv(sh->u_vp_matrix, 1, GL_FALSE, projection);
	}					
	drawQuad(sh);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	//Replaced by gbmSwapBuffers - eglSwapBuffers(display, surface);
	//fprintf(stderr,"Calling gbmSwapBuffers using %p as display and %p as surface\n",&display,&surface);
	gbmSwapBuffers(&display, &surface);
}

static GLuint createShader(GLenum type, const char *shaderSrc)
{
	GLuint shader = glCreateShader(type);
	if (!shader) {
		fprintf(stderr, "glCreateShader() failed: %d\n", glGetError());
		return 0;
	}

	// Load and compile the shader source
	glShaderSource(shader, 1, &shaderSrc, NULL);
	glCompileShader(shader);

	// Check the compile status
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char* infoLog = (char *)malloc(sizeof(char) * infoLen);
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			fprintf(stderr, "Error compiling shader:\n%s\n", infoLog);
			free(infoLog);
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static GLuint createProgram(const char *vertexShaderSrc, const char *fragmentShaderSrc)
{
	GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexShaderSrc);
	if (!vertexShader) {
		fprintf(stderr, "createShader(GL_VERTEX_SHADER) failed\n");
		return 0;
	}

	GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
	if (!fragmentShader) {
		fprintf(stderr, "createShader(GL_FRAGMENT_SHADER) failed\n");
		glDeleteShader(vertexShader);
		return 0;
	}

	GLuint programObject = glCreateProgram();
	if (!programObject) {
		fprintf(stderr, "glCreateProgram() failed: %d\n", glGetError());
		return 0;
	}

	glAttachShader(programObject, vertexShader);
	glAttachShader(programObject, fragmentShader);

	// Link the program
	glLinkProgram(programObject);

	// Check the link status
	GLint linked = 0;
	glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLint infoLen = 0;
		glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char* infoLog = (char *)malloc(infoLen);
			glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
			fprintf(stderr, "Error linking program: %s\n", infoLog);
			free(infoLog);
		}

		glDeleteProgram(programObject);
		return 0;
	}

	// Delete these here because they are attached to the program object.
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return programObject;
}

static void setOrtho(float m[4][4],
	float left, float right, float bottom, float top,
	float near, float far, float scaleX, float scaleY)
{
	memset(m, 0, 4 * 4 * sizeof(float));
	m[0][0] = 2.0f / (right - left) * scaleX;
	m[1][1] = 2.0f / (top - bottom) * scaleY;
	m[2][2] = 0;//-2.0f / (far - near);
	m[3][0] = -(right + left) / (right - left);
	m[3][1] = -(top + bottom) / (top - bottom);
	m[3][2] = 0;//-(far + near) / (far - near);
	m[3][3] = 1;
}

static void drawQuad(const ShaderInfo *sh)
{
	glUniform1i(sh->u_texture, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glVertexAttribPointer(sh->a_position, 3, GL_FLOAT,
		GL_FALSE, 3 * sizeof(GLfloat), NULL);
	glEnableVertexAttribArray(sh->a_position);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glVertexAttribPointer(sh->a_texcoord, 2, GL_FLOAT,
		GL_FALSE, 2 * sizeof(GLfloat), NULL);
	glEnableVertexAttribArray(sh->a_texcoord);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);

	glDrawElements(GL_TRIANGLES, kIndexCount, GL_UNSIGNED_SHORT, 0);
}
