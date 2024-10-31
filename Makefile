
TARGET := testffmpeg_rpi
SOURCES := main.cpp video_display.cpp video_display_egl.cpp \
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
			external/hello_wayland/build/xdg-shell-protocol.c
OBJECTS := ${SOURCES:.c=.o}
OBJECTS := ${OBJECTS:.cpp=.o}
CFLAGS := -g -I. -Iexternal/hello_wayland/build
CXXFLAGS := $(CFLAGS)
LIBS := -lSDL3 -lavcodec -lavformat -lavutil -lwayland-client -lwayland-egl -lepoxy -lEGL

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(LIBS)

clean:
	$(RM) $(OBJECTS) $(TARGET)
