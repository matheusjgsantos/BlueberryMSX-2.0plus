/*********************/
/* compile with : gcc -o kmsdrm kmsdrm.c $(pkg-config --libs --cflags libdrm glesv2 gbm egl gl) -nostartfiles -v */
/*********************/

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <errno.h>
#include <gbm.h>
#include <GL/gl.h>
#include <SDL2/SDL_egl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fd;

struct conn {
	/* drm related */
	uint32_t cid;
	char *mode_str;
	drmModeRes *res;
	drmModeConnector *conn;
	drmModeModeInfo *mode;
	drmModeEncoder *encoder;
	int crtc;

	/* gbm stuff */
	struct gbm_device *gbm;
	struct gbm_bo *bo;

	/* egl stuff */
	EGLDisplay dpy;
	EGLContext ctx;
	GLuint fb, color_rb, depth_rb;
	EGLImageKHR image;
};

static int init_gbm(struct conn *c)
{
	c->gbm = gbm_create_device(fd);
	if (!c->gbm)
		return -EINVAL;

	c->bo = gbm_bo_create(c->gbm, c->mode->hdisplay, c->mode->vdisplay,
				GBM_BO_FORMAT_XRGB8888,
				GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!c->bo)
		return -EINVAL;

	return 0;
}

static int init_egl(struct conn *c)
{
	EGLint major, minor;

	c->dpy = eglGetDisplay((void*)c->gbm);
	if (!c->dpy)
		return -EINVAL;

	eglInitialize(c->dpy, &major, &minor);
	eglBindAPI(EGL_OPENGL_API);

	c->ctx = eglCreateContext(c->dpy, NULL, EGL_NO_CONTEXT, NULL);
	if (!c->ctx)
		return -EINVAL;

	eglMakeCurrent(c->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, c->ctx);

	glGenFramebuffers(1, &c->fb);
	glBindFramebuffer(GL_FRAMEBUFFER_EXT, c->fb);

	c->image = eglCreateImageKHR(c->dpy, NULL, EGL_NATIVE_PIXMAP_KHR,
								c->bo, NULL);

	glGenRenderbuffers(1, &c->color_rb);
	glBindRenderbuffer(GL_RENDERBUFFER_EXT, c->color_rb);
	glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER_EXT, c->image);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
		GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, c->color_rb);

	glGenRenderbuffers(1, &c->depth_rb);
	glBindRenderbuffer(GL_RENDERBUFFER_EXT, c->depth_rb);
	glRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT,
					c->mode->hdisplay, c->mode->vdisplay);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
		GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, c->depth_rb);

	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) !=
						GL_FRAMEBUFFER_COMPLETE)
		return -EINVAL;

	return 0;
}

static int find_mode(struct conn *c)
{
	drmModeConnector *connector;
	drmModeModeInfo *info;
	drmModeEncoder *enc;
	int i, j;

	for (i = 0; i < c->res->count_connectors; i++) {
		connector = drmModeGetConnector(fd, c->res->connectors[i]);

		if (!connector)
			continue;

		if (connector->connector_id != c->cid) {
			drmModeFreeConnector(connector);
			continue;
		}

		for (j = 0; j < connector->count_modes; j++) {
			info = &connector->modes[j];
			if (!strcmp(info->name, c->mode_str)) {
				c->conn = connector;
				c->mode = info;
				break;
			}
		}

		if (c->mode)
			break;

		drmModeFreeConnector(connector);
	}

	if (!c->conn)
		return -EINVAL;

	for (i = 0; i < c->res->count_encoders; i++) {
		enc = drmModeGetEncoder(fd, c->res->encoders[i]);

		if (!enc)
			continue;

		if (enc->encoder_id  == c->conn->encoder_id) {
			c->encoder = enc;
			break;
		}

		drmModeFreeEncoder(enc);
	}

	if (!c->encoder)
		return -EINVAL;

	if (c->crtc == -1)
		c->crtc = c->encoder->crtc_id;

	return 0;
}

static int run(struct conn *c)
{
	uint32_t fb_id;
	uint32_t handle, stride;
	int ret;

	handle = gbm_bo_get_handle(c->bo).u32;
	stride = gbm_bo_get_width(c->bo);

	ret = drmModeAddFB(fd, c->mode->hdisplay, c->mode->vdisplay,
					24, 32, stride, handle, &fb_id);
	if (ret)
		return -EINVAL;

	glViewport(0, 0, c->mode->hdisplay, c->mode->vdisplay);

	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBegin(GL_TRIANGLES);
	glColor4f(0.5, 0.5, 0.5, 1.0);
	glVertex3f(1.0, 1.0, 0.0f);
	glVertex3f(0, 0, 0.0f);
	glVertex3f(1.0, 0, 0.0f);
	glEnd();
	glFinish();

	ret = drmModeSetCrtc(fd, c->crtc, fb_id, 0, 0,
						&c->cid, 1, c->mode);
	if (ret)
		return -EINVAL;

	sleep(5);

	return 0;
}

int main()
{
	struct conn c;
	int ret;

	/*
	 * adjust these settings
	 * cid: connector ID
	 * mode_str: mode-string
	 * crtc: crtc ID or -1
	 */
	memset(&c, 0, sizeof(c));
	c.cid = 5;
	c.mode_str = "1024x600";
	c.crtc = -1;

	fd = open("/dev/dri/card0", O_RDWR);
	if (fd < 0)
		return -EINVAL;

	c.res = drmModeGetResources(fd);
	if (!c.res)
		return -EINVAL;

	ret = find_mode(&c);
	if (ret)
		return ret;

	ret = init_gbm(&c);
	if (ret)
		return ret;

	ret = init_egl(&c);
	if (ret)
		return ret;

	ret = run(&c);
	if (ret)
		return ret;

	drmModeFreeResources(c.res);

	return 0;
}