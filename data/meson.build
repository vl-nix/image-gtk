desktop = meson.project_name() + '.desktop'

r = run_command('sh', '-c', 'echo "[Desktop Entry]\nName=Image-Gtk\nComment=Lightweight picture viewer\nExec=image-gtk %U\nIcon=image\nTerminal=false\nType=Application\nCategories=Graphics;Viewer;\n" > desktop', check: true)
configure_file(input: 'desktop', output: desktop, copy: true, install: true, install_dir: join_paths('share', 'applications'))

meson.add_install_script('sh', '-c', 'update-desktop-database -q /usr/share/applications')
