This is a simple example of playing video on the Raspberry Pi 5.

Building:
```
pushd external/hello_world
meson setup build
cd build
meson compile
popd

pushd external/drmu
meson setup build
cd build
meson compile
popd

make
```

Running:
```
./testffmpeg_rpi video_file
```
