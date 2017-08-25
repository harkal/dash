
Debian
====================
This directory contains files used to package ebakusd/ebakus-qt
for Debian-based Linux systems. If you compile ebakusd/ebakus-qt yourself, there are some useful files here.

## ebakus: URI support ##


ebakus-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install ebakus-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your ebakus-qt binary to `/usr/bin`
and the `../../share/pixmaps/ebakus128.png` to `/usr/share/pixmaps`

ebakus-qt.protocol (KDE)

