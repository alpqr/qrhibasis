Simple experiment for using the transcoder from
https://github.com/BinomialLLC/basis_universal together with Qt's upcoming
graphics abstraction layer, see f.ex.
https://www.qt.io/blog/qt-quick-on-vulkan-metal-direct3d

The example loads a .basis file (generated with basisu -mipmap stuff.png) and
transcodes either to ETC2 or BC1, depending on what is supported at run time.
Falls back to transcoding to plain RGBA8 otherwise.
It then draws a textured quad.

Should work on all QRhi backends, switch with command line args: -d (D3D11) -v
(Vulkan) -m (Metal) -g (OpenGL).

Should build against qtbase/5.14. Uses private APIs though so it may break in
arbitrary ways. Only tested with MSVC2019 so far.
