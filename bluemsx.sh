#export LD_LIBRARY_PATH=/usr/local/lib/:/usr/lib/arm-linux-gnueabihf:/usr/lib
#export SDL_VIDEO_EGL_DRIVER=libEGL.so 
#export SDL_VIDEO_GL_DRIVER=libGLESv2.so
#export SDL_VIDEODRIVER=KMSDRM 
#export SDL_RENDER_DRIVER=software 
#export SDL_RENDER_DRIVER=opengles2 
#export SDL_RENDER_DRIVER=opengles2
./bluemsx-pi /romtype1 msxbus /romtype2 msxbus $@
