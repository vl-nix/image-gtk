### [Image-Gtk](https://github.com/vl-nix/image-gtk)

* Picture viewer
* Drag and Drop: file, folder
* Supported formats: PNG, JPEG, TIFF, TGA, GIF, SVG


#### Dependencies

* gcc
* meson
* libgtk 3.0 ( & dev )


#### Build

1. Clone: git clone https://github.com/vl-nix/image-gtk.git

2. Configure: meson build --prefix /usr --strip

3. Build: ninja -C build

4. Install: ninja install -C build

5. Uninstall: ninja uninstall -C build
