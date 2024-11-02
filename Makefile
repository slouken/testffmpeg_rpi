
TARGET := testffmpeg_rpi
SOURCES := main.cpp video_display.cpp video_display_rpi.cpp video_display_egl.cpp video_display_drm.cpp \
			external/hello_wayland/init_window.c \
			external/hello_wayland/dmabuf_alloc.c \
			external/hello_wayland/dmabuf_pool.c \
			external/hello_wayland/fb_pool.c \
			external/hello_wayland/generic_pool.c \
			external/hello_wayland/init_window.c \
			external/hello_wayland/pollqueue.c \
			external/hello_wayland/wayout.c \
			external/hello_wayland/build/fullscreen-shell-unstable-v1-protocol.c \
			external/hello_wayland/build/linux-dmabuf-unstable-v1-protocol.c \
			external/hello_wayland/build/single-pixel-buffer-v1-client-protocol.c \
			external/hello_wayland/build/viewporter-protocol.c \
			external/hello_wayland/build/xdg-decoration-unstable-v1-protocol.c \
			external/hello_wayland/build/xdg-shell-protocol.c \
			external/drmu/test/drmprime_out.c \
			external/drmu/drmu/drmu_atomic.c \
			external/drmu/drmu/drmu_av.c \
			external/drmu/drmu/drmu.c \
			external/drmu/drmu/drmu_dmabuf.c \
			external/drmu/drmu/drmu_fmts.c \
			external/drmu/drmu/drmu_math.c \
			external/drmu/drmu/drmu_output.c \
			external/drmu/drmu/drmu_poll.c \
			external/drmu/drmu/drmu_pool.c \
			external/drmu/drmu/drmu_util.c
OBJECTS := ${SOURCES:.c=.o}
OBJECTS := ${OBJECTS:.cpp=.o}
CFLAGS := -g -I. -Iexternal/hello_wayland/build -Iexternal/drmu -Iexternal/drmu/drmu -Iexternal/drmu/pollqueue -I/usr/include/libdrm
CXXFLAGS := $(CFLAGS)
LIBS := -lSDL3 -lavcodec -lavformat -lavutil -lwayland-client -lwayland-egl -lepoxy -lEGL -ldrm -lgbm

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(LIBS)

clean:
	$(RM) $(OBJECTS) $(TARGET)
