
Debian
====================
This directory contains files used to package GuldenD/Gulden
for Debian-based Linux systems. If you compile GuldenD/Gulden yourself, there are some useful files here.

## gulden: URI support ##


Gulden.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install Gulden.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your Gulden binary to `/usr/bin`
and the `../../share/pixmaps/gulden128.png` to `/usr/share/pixmaps`

Gulden.protocol (KDE)

