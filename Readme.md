### [Image-Gtk](https://github.com/vl-nix/image-gtk)

* Picture viewer


#### Dependencies

* gcc
* meson
* libgtk 3.0 ( & dev )


#### Build

1. Clone: git clone https://github.com/vl-nix/image-gtk.git

2. Configure: meson build --prefix /usr --strip

3. Build: ninja -C build

4. Install: sudo ninja -C build install

5. Uninstall: sudo ninja -C build uninstall
