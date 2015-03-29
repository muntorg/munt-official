
Debian
====================
This directory contains files used to package guldencoind/guldencoin-qt
for Debian-based Linux systems. If you compile guldencoind/guldencoin-qt yourself, there are some useful files here.

## guldencoin: URI support ##


guldencoin-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install guldencoin-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your guldencoin-qt binary to `/usr/bin`
and the `../../share/pixmaps/guldencoin128.png` to `/usr/share/pixmaps`

guldencoin-qt.protocol (KDE)

