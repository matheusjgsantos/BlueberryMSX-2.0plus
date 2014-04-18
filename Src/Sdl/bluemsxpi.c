#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>

#include "CommandLine.h"
#include "Properties.h"
#include "ArchFile.h"
#include "VideoRender.h"
#include "AudioMixer.h"
#include "Casette.h"
#include "PrinterIO.h"
#include "UartIO.h"
#include "MidiIO.h"
#include "Machine.h"
#include "Board.h"
#include "Emulator.h"
#include "FileHistory.h"
#include "Actions.h"
#include "Language.h"
#include "LaunchFile.h"
#include "ArchEvent.h"
#include "ArchSound.h"
#include "ArchNotifications.h"
#include "JoystickPort.h"
#include "SdlShortcuts.h"

#include <bcm_host.h>
#include <interface/vchiq_arm/vchiq_if.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#define assert(x)

typedef	struct ShaderInfo {
		GLuint program;
		GLint a_position;
		GLint a_texcoord;
		GLint u_vp_matrix;
		GLint u_texture;
		GLint u_palette;
} ShaderInfo;

#define BIT_DEPTH   16
#define	TEX_WIDTH	512
#define	TEX_HEIGHT	256

#define	WIDTH		320
#define	HEIGHT		240

#define	minU		0.0f
#define	maxU		((float)WIDTH / TEX_WIDTH - minU)
#define	minV		0.0f
#define	maxV		((float)HEIGHT/TEX_HEIGHT)

#define EVENT_UPDATE_DISPLAY 2

static void setDefaultPaths(const char* rootDir);
static void handleEvent(SDL_Event* event);
static int  initEgl();
static void destroyEgl();
static void drawQuad(const ShaderInfo *sh, GLuint textures[2]);
static void updateScreen(int width, int height);

static GLuint createShader(GLenum type, const char *shaderSrc);
static GLuint createProgram(const char *vertexShaderSrc, const char *fragmentShaderSrc);
static void setOrtho(float m[4][4],
	float left, float right, float bottom, float top,
	float near, float far, float scaleX, float scaleY);

static Properties *properties;
static Video *video;
static Mixer *mixer;

static Shortcuts* shortcuts;
static int doQuit = 0;

static int pendingDisplayEvents = 0;
static void *dpyUpdateAckEvent = NULL;

static EGL_DISPMANX_WINDOW_T nativeWindow;
static EGLDisplay display = NULL;
static EGLSurface surface = NULL;
static EGLContext context = NULL;

static uint32_t screenWidth = 0;
static uint32_t screenHeight = 0;

static ShaderInfo shader;
static ShaderInfo shaderFiltering;
static GLuint buffers[3];
static GLuint textures[2];

static int paletteChanged;
static char *msxScreen = NULL;
static int msxScreenPitch;

static const char* vertexShaderSrc =
	"uniform mat4 u_vp_matrix;								\n"
	"attribute vec4 a_position;								\n"
	"attribute vec2 a_texcoord;								\n"
	"varying mediump vec2 v_texcoord;						\n"
	"void main()											\n"
	"{														\n"
	"	v_texcoord = a_texcoord;							\n"
	"	gl_Position = u_vp_matrix * a_position;				\n"
	"}														\n";

static const char* fragmentShaderSrc =
	"varying mediump vec2 v_texcoord;												\n"
	"uniform sampler2D u_texture;													\n"
	"uniform sampler2D u_palette;													\n"
	"void main()																	\n"
	"{																				\n"
	"	vec4 p0 = texture2D(u_texture, v_texcoord);									\n" // use paletted texture
	"	vec4 c0 = texture2D(u_palette, vec2(p0.r + 1.0/256.0*0.5, 0.5)); 		\n"
	"	gl_FragColor = c0;											 				\n"
	"}																				\n";

static const char* fragmentShaderFilteringSrc =
	"varying mediump vec2 v_texcoord;												\n"
	"uniform sampler2D u_texture;													\n"
	"uniform sampler2D u_palette;													\n"
	"void main()																	\n"
	"{																				\n"
	"	vec4 p0 = texture2D(u_texture, v_texcoord);									\n" // manually linear filtering of paletted texture is awful
	"	vec4 p1 = texture2D(u_texture, v_texcoord + vec2(1.0/512.0, 0)); 			\n"
	"	vec4 p2 = texture2D(u_texture, v_texcoord + vec2(0, 1.0/256.0)); 			\n"
	"	vec4 p3 = texture2D(u_texture, v_texcoord + vec2(1.0/512.0, 1.0/256.0)); 	\n"
	"	vec4 c0 = texture2D(u_palette, vec2(p0.r + 1.0/256.0*0.5, 0.5)); 			\n"
	"	vec4 c1 = texture2D(u_palette, vec2(p1.r + 1.0/256.0*0.5, 0.5)); 			\n"
	"	vec4 c2 = texture2D(u_palette, vec2(p2.r + 1.0/256.0*0.5, 0.5)); 			\n"
	"	vec4 c3 = texture2D(u_palette, vec2(p3.r + 1.0/256.0*0.5, 0.5)); 			\n"
	"	vec2 l = vec2(fract(512.0*v_texcoord.x), fract(256.0*v_texcoord.y)); 		\n"
	"	gl_FragColor = mix(mix(c0, c1, l.x), mix(c2, c3, l.x), l.y); 				\n"
	"}																				\n";

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

static const GLfloat vertices[] = {
	-0.5f, -0.5f, 0.0f,
	+0.5f, -0.5f, 0.0f,
	+0.5f, +0.5f, 0.0f,
	-0.5f, +0.5f, 0.0f,
};

int archUpdateEmuDisplay(int syncMode) 
{
	SDL_Event event;
	if (pendingDisplayEvents > 1) {
		return 1;
	}

	pendingDisplayEvents++;

	event.type = SDL_USEREVENT;
	event.user.code = EVENT_UPDATE_DISPLAY;
	event.user.data1 = NULL;
	event.user.data2 = NULL;

	SDL_PushEvent(&event);

	if (properties->emulation.syncMethod == P_EMU_SYNCFRAMES) {
		archEventWait(dpyUpdateAckEvent, 500);
	}

	return 1;
}

void archUpdateWindow()
{
}

void archEmulationStartNotification()
{
}

void archEmulationStopNotification()
{
}

void archEmulationStartFailure()
{
}

void archTrap(UInt8 value)
{
}

static int initEgl()
{
	bcm_host_init();

	// get an EGL display connection
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY) {
		fprintf(stderr, "eglGetDisplay() failed: EGL_NO_DISPLAY\n");
		return 0;
	}

	// initialize the EGL display connection
	EGLBoolean result = eglInitialize(display, NULL, NULL);
	if (result == EGL_FALSE) {
		fprintf(stderr, "eglInitialize() failed: EGL_FALSE\n");
		return 0;
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

	VC_RECT_T dstRect;
	dstRect.x = 0;
	dstRect.y = 0;
	dstRect.width = screenWidth;
	dstRect.height = screenHeight;

	VC_RECT_T srcRect;
	srcRect.x = 0;
	srcRect.y = 0;
	srcRect.width = screenWidth << 16;
	srcRect.height = screenHeight << 16;

	DISPMANX_DISPLAY_HANDLE_T dispManDisplay = vc_dispmanx_display_open(0);
	DISPMANX_UPDATE_HANDLE_T dispmanUpdate = vc_dispmanx_update_start(0);
	DISPMANX_ELEMENT_HANDLE_T dispmanElement = vc_dispmanx_element_add(dispmanUpdate,
		dispManDisplay, 0, &dstRect, 0, &srcRect,
		DISPMANX_PROTECTION_NONE, NULL, NULL, DISPMANX_NO_ROTATE);

	nativeWindow.element = dispmanElement;
	nativeWindow.width = screenWidth;
	nativeWindow.height = screenHeight;
	vc_dispmanx_update_submit_sync(dispmanUpdate);

	surface = eglCreateWindowSurface(display, config, &nativeWindow, NULL);
	if (surface == EGL_NO_SURFACE) {
		fprintf(stderr, "eglCreateWindowSurface() failed: EGL_NO_SURFACE\n");
		return 0;
	}

	// connect the context to the surface
	result = eglMakeCurrent(display, surface, surface, context);
	if (result == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent() failed: EGL_FALSE\n");
		return 0;
	}

	// Init shader resources
	memset(&shader, 0, sizeof(ShaderInfo));
	shader.program = createProgram(vertexShaderSrc, fragmentShaderSrc);
	if (shader.program) {
		shader.a_position	= glGetAttribLocation(shader.program,	"a_position");
		shader.a_texcoord	= glGetAttribLocation(shader.program,	"a_texcoord");
		shader.u_vp_matrix	= glGetUniformLocation(shader.program,	"u_vp_matrix");
		shader.u_texture	= glGetUniformLocation(shader.program,	"u_texture");
		shader.u_palette	= glGetUniformLocation(shader.program,	"u_palette");
	}

	memset(&shaderFiltering, 0, sizeof(ShaderInfo));
	shaderFiltering.program = createProgram(vertexShaderSrc, fragmentShaderFilteringSrc);
	if (shaderFiltering.program) {
		shaderFiltering.a_position	= glGetAttribLocation(shaderFiltering.program,	"a_position");
		shaderFiltering.a_texcoord	= glGetAttribLocation(shaderFiltering.program,	"a_texcoord");
		shaderFiltering.u_vp_matrix	= glGetUniformLocation(shaderFiltering.program,	"u_vp_matrix");
		shaderFiltering.u_texture	= glGetUniformLocation(shaderFiltering.program,	"u_texture");
		shaderFiltering.u_palette	= glGetUniformLocation(shaderFiltering.program,	"u_palette");
	}

	paletteChanged = 1;
	if (shader.program && shaderFiltering.program) {
		glGenTextures(2, textures);
		glBindTexture(GL_TEXTURE_2D, textures[0]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, TEX_WIDTH, TEX_HEIGHT, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, textures[1]);	// color palette
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);

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
	}

	return 1;
}

static void destroyEgl() 
{
	// Destroy shader resources
	if (shader.program) {
		glDeleteProgram(shader.program);
	}

	if (shaderFiltering.program) {
		glDeleteProgram(shaderFiltering.program);
		if (shader.program) {
			glDeleteBuffers(3, buffers);
			glDeleteTextures(2, textures);
		}
	}

	// Release OpenGL resources
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(display, surface);
	eglDestroyContext(display, context);
	eglTerminate(display);

	bcm_host_deinit();
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
		if(infoLen > 1) {
			char* infoLog = (char *)malloc(infoLen);
			glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
			fprintf(stderr, "Error linking program:\n%s\n", infoLog);
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
	m[2][2] = -2.0f / (far - near);
	m[3][0] = -(right + left) / (right - left);
	m[3][1] = -(top + bottom) / (top - bottom);
	m[3][2] = -(far + near) / (far - near);
	m[3][3] = 1;
}

static void drawQuad(const ShaderInfo *sh, GLuint textures[2])
{
	glUniform1i(sh->u_texture, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glUniform1i(sh->u_palette, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

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

// FIXME
#define Colours_GetR(x) (x & 0xff)
#define Colours_GetG(x) (x & 0xff)
#define Colours_GetB(x) (x & 0xff)

static inline unsigned short BGR565(unsigned char r, unsigned char g, unsigned char b)
{
	return ((r&~7) << 8)|((g&~3) << 3)|(b >> 3);
}

static void updateScreen(int width, int height)
{
	if (!shader.program || !shaderFiltering.program) {
		fprintf(stderr, "Shader not initialized\n");
		return;
	}

	float sx = 1.0f;
	float sy = 1.0f;
	float zoom = 1.0f;

	// Screen aspect ratio adjustment
	float a = (float)width / height;
	float a0 = (float)WIDTH / (float)HEIGHT;

	if (a > a0) {
		sx = a0/a;
	} else {
		sy = a/a0;
	}

	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(0, 0, width, height);

	ShaderInfo *sh = &shaderFiltering;

	float proj[4][4];
	glDisable(GL_BLEND);
	glUseProgram(sh->program);
	setOrtho(proj, -0.5f, +0.5f, +0.5f, -0.5f, -1.0f, 1.0f,
		sx * zoom, sy * zoom);
	glUniformMatrix4fv(sh->u_vp_matrix, 1, GL_FALSE, &proj[0][0]);

	// FIXME
	if (paletteChanged) {
		int i;
		unsigned short palette[256];

		paletteChanged = 0;
		for (i = 0; i < 256; ++i) {
			palette[i] = BGR565(Colours_GetR(i), Colours_GetG(i), Colours_GetB(i));
		}

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, textures[1]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 
			1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, palette);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	// glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_LUMINANCE, GL_UNSIGNED_BYTE, Screen_atari);

	// FIXME: shit happens here
////

	FrameBuffer* frameBuffer;
	frameBuffer = frameBufferFlipViewFrame(properties->emulation.syncMethod == P_EMU_SYNCTOVBLANKASYNC);
	if (frameBuffer == NULL) {
		frameBuffer = frameBufferGetWhiteNoiseFrame();
	}
	int borderWidth = (320 - frameBuffer->maxWidth) * zoom / 2;
	int bytesPerPixel = BIT_DEPTH >> 3;
	int y;
	
		videoRender(video, frameBuffer, BIT_DEPTH, 1,
					msxScreen + borderWidth * bytesPerPixel, 0, msxScreenPitch, -1);

		// if (borderWidth > 0) {
		// 	int h = height;
		// 	while (h--) {
		// 		memset(dpyData, 0, borderWidth * bytesPerPixel);
		// 		memset(dpyData + (width - borderWidth) * bytesPerPixel, 0, borderWidth * bytesPerPixel);
		// 		dpyData += msxScreenPitch;
		// 	}
		// }

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT,
						GL_RGB, GL_UNSIGNED_SHORT_5_6_5, msxScreen);

		// updateAll |= properties->video.driver == P_VIDEO_DRVDIRECTX;

		// for (y = 0; y < height; y += linesPerBlock) {
		// 	if (updateAll || isLineDirty(y, linesPerBlock)) {
		// 		if (!isDirty) {
		// 			glEnable(GL_TEXTURE_2D);
		// 			glEnable(GL_ASYNC_TEX_IMAGE_SGIX);
		// 			glBindTexture(GL_TEXTURE_2D, textureId);

		// 			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		// 			isDirty = 1;
		// 		}

		// 		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, linesPerBlock,
		// 						GL_RGB, GL_UNSIGNED_SHORT_5_6_5, msxScreen + y * displayPitch);
		// 	}
		// }


////

	// glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT,
	// 	GL_LUMINANCE, GL_UNSIGNED_BYTE, screen);
	drawQuad(sh, textures);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	eglSwapBuffers(display, surface);
}

static void handleEvent(SDL_Event* event) 
{
	switch (event->type) {
	// case 0xa: // FIXME
	case SDL_USEREVENT:
		switch (event->user.code) {
		// default:
		case EVENT_UPDATE_DISPLAY:
			updateScreen(screenWidth, screenHeight);
			archEventSet(dpyUpdateAckEvent);
			pendingDisplayEvents--;
			break;
		}
		break;
	case SDL_ACTIVEEVENT:
		if (event->active.state & SDL_APPINPUTFOCUS) {
			keyboardSetFocus(1, event->active.gain);
		}
		break;
	case SDL_KEYDOWN:
		shortcutCheckDown(shortcuts, HOTKEY_TYPE_KEYBOARD, keyboardGetModifiers(), event->key.keysym.sym);
		break;
	case SDL_KEYUP:
		shortcutCheckUp(shortcuts, HOTKEY_TYPE_KEYBOARD, keyboardGetModifiers(), event->key.keysym.sym);
		break;
	default:
	fprintf(stderr, "guh? %x\n", event->type);
	}
}

static void setDefaultPaths(const char* rootDir)
{
	char buffer[512]; // FIXME

	propertiesSetDirectory(rootDir, rootDir);

	sprintf(buffer, "%s/Audio Capture", rootDir);
	archCreateDirectory(buffer);
	actionSetAudioCaptureSetDirectory(buffer, "");

	sprintf(buffer, "%s/Video Capture", rootDir);
	archCreateDirectory(buffer);
	actionSetAudioCaptureSetDirectory(buffer, "");

	sprintf(buffer, "%s/QuickSave", rootDir);
	archCreateDirectory(buffer);
	actionSetQuickSaveSetDirectory(buffer, "");

	sprintf(buffer, "%s/SRAM", rootDir);
	archCreateDirectory(buffer);
	boardSetDirectory(buffer);

	sprintf(buffer, "%s/Casinfo", rootDir);
	archCreateDirectory(buffer);
	tapeSetDirectory(buffer, "");

	sprintf(buffer, "%s/Databases", rootDir);
	archCreateDirectory(buffer);
	mediaDbLoad(buffer);

	sprintf(buffer, "%s/Keyboard Config", rootDir);
	archCreateDirectory(buffer);
	keyboardSetDirectory(buffer);

	sprintf(buffer, "%s/Shortcut Profiles", rootDir);
	archCreateDirectory(buffer);
	shortcutsSetDirectory(buffer);

	sprintf(buffer, "%s/Machines", rootDir);
	archCreateDirectory(buffer);
	machineSetDirectory(buffer);
}

int main(int argc, char **argv)
{
	if (!initEgl()) {
		fprintf(stderr, "initEgl() failed");
		return 1;
	}

	msxScreen = (char*)calloc(1, BIT_DEPTH / 8 * TEX_WIDTH * TEX_HEIGHT);
	msxScreenPitch = WIDTH * BIT_DEPTH / 8;

	SDL_Init(SDL_INIT_EVERYTHING);

	SDL_ShowCursor(SDL_DISABLE);
	SDL_Surface *screen = SDL_SetVideoMode(0, 0, 32, SDL_SWSURFACE);

	SDL_Event event;
	char szLine[8192] = "";
	int resetProperties;
	char path[512] = "";
	int i;

	for (i = 1; i < argc; i++) {
		if (strchr(argv[i], ' ') != NULL && argv[i][0] != '\"') {
			strcat(szLine, "\"");
			strcat(szLine, argv[i]);
			strcat(szLine, "\"");
		} else {
			strcat(szLine, argv[i]);
		}
		strcat(szLine, " ");
	}

	setDefaultPaths(archGetCurrentDirectory());

	resetProperties = emuCheckResetArgument(szLine);
	strcat(path, archGetCurrentDirectory());
	strcat(path, DIR_SEPARATOR "bluemsx.ini");

	properties = propCreate(resetProperties, 0, P_KBD_EUROPEAN, 0, "");
	properties->video.windowSize = P_VIDEO_SIZEX1;
	properties->emulation.syncMethod = P_EMU_SYNCFRAMES;

	if (resetProperties == 2) {
		propDestroy(properties);
		return 0;
	}

	video = videoCreate();
	videoSetColors(video, properties->video.saturation,
		properties->video.brightness,
		properties->video.contrast, properties->video.gamma);
	videoSetScanLines(video, properties->video.scanlinesEnable,
		properties->video.scanlinesPct);
	videoSetColorSaturation(video, properties->video.colorSaturationEnable,
		properties->video.colorSaturationWidth);

	properties->video.driver = P_VIDEO_DRVDIRECTX_VIDEO;

	dpyUpdateAckEvent = archEventCreate(0);

	keyboardInit();

	mixer = mixerCreate();
	for (i = 0; i < MIXER_CHANNEL_TYPE_COUNT; i++) {
		mixerSetChannelTypeVolume(mixer, i,
			properties->sound.mixerChannel[i].volume);
		mixerSetChannelTypePan(mixer, i,
			properties->sound.mixerChannel[i].pan);
		mixerEnableChannelType(mixer, i,
			properties->sound.mixerChannel[i].enable);
	}
	mixerSetMasterVolume(mixer, properties->sound.masterVolume);
	mixerEnableMaster(mixer, properties->sound.masterEnable);

	emulatorInit(properties, mixer);
	actionInit(video, properties, mixer);
	tapeSetReadOnly(properties->cassette.readOnly);

	langInit();
	langSetLanguage(properties->language);

	joystickPortSetType(0, properties->joy1.typeId);
	joystickPortSetType(1, properties->joy2.typeId);

	printerIoSetType(properties->ports.Lpt.type,
		properties->ports.Lpt.fileName);
	printerIoSetType(properties->ports.Lpt.type,
		properties->ports.Lpt.fileName);
	uartIoSetType(properties->ports.Com.type,
		properties->ports.Com.fileName);
	midiIoSetMidiOutType(properties->sound.MidiOut.type,
		properties->sound.MidiOut.fileName);
	midiIoSetMidiInType(properties->sound.MidiIn.type,
		properties->sound.MidiIn.fileName);
	ykIoSetMidiInType(properties->sound.YkIn.type,
		properties->sound.YkIn.fileName);

	emulatorRestartSound();

	videoUpdateAll(video, properties);

	shortcuts = shortcutsCreate();

	mediaDbSetDefaultRomType(properties->cartridge.defaultType);

	for (i = 0; i < PROP_MAX_CARTS; i++) {
		if (properties->media.carts[i].fileName[0]) {
			insertCartridge(properties, i,
				properties->media.carts[i].fileName,
				properties->media.carts[i].fileNameInZip,
				properties->media.carts[i].type, -1);
		}
		updateExtendedRomName(i,
			properties->media.carts[i].fileName,
			properties->media.carts[i].fileNameInZip);
	}

	for (i = 0; i < PROP_MAX_DISKS; i++) {
		if (properties->media.disks[i].fileName[0]) {
			insertDiskette(properties, i,
				properties->media.disks[i].fileName,
				properties->media.disks[i].fileNameInZip, -1);
		}
		updateExtendedDiskName(i,
			properties->media.disks[i].fileName,
			properties->media.disks[i].fileNameInZip);
	}

	for (i = 0; i < PROP_MAX_TAPES; i++) {
		if (properties->media.tapes[i].fileName[0]) {
			insertCassette(properties, i,
				properties->media.tapes[i].fileName,
				properties->media.tapes[i].fileNameInZip, 0);
		}
		updateExtendedCasName(i,
			properties->media.tapes[i].fileName,
			properties->media.tapes[i].fileNameInZip);
	}

	{
		Machine* machine = machineCreate(properties->emulation.machineName);
		if (machine != NULL) {
			boardSetMachine(machine);
			machineDestroy(machine);
		}
	}

	boardSetFdcTimingEnable(properties->emulation.enableFdcTiming);
	boardSetY8950Enable(properties->sound.chip.enableY8950);
	boardSetYm2413Enable(properties->sound.chip.enableYM2413);
	boardSetMoonsoundEnable(properties->sound.chip.enableMoonsound);
	boardSetVideoAutodetect(properties->video.detectActiveMonitor);

	i = emuTryStartWithArguments(properties, szLine, NULL);
	if (i < 0) {
		printf("Failed to parse command line\n");
		return 0;
	}

	if (i == 0) {
		emulatorStart(NULL);
	}

	fprintf(stderr, "Entering main loop\n");

	while (!doQuit) {
		SDL_WaitEvent(&event);
		do {
			if (event.type == SDL_QUIT ) {
				doQuit = 1;
			} else {
				handleEvent(&event);
			}
		} while(SDL_PollEvent(&event));
	}

	if (screen) {
		SDL_FreeSurface(screen);
		screen = NULL;
	}

	// For stop threads before destroy.
	// Clean up.
	SDL_Quit();

	videoDestroy(video);
	propDestroy(properties);
	archSoundDestroy();
	mixerDestroy(mixer);

	destroyEgl();

	if (display) {
		free(display);
	}

	return 0;
}
