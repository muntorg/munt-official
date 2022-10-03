Note
-----

For development the easiest way to get a working native windows build on a windows host is to use msys2, see instructions below.

If you are not planning to develop but only run the software you may find using WSL slightly easier or faster, the instructions below detail how to do this. However note the result is not a properly native binary but one that runs through the WSL 'emulation' layer.

It is also possible to cross compile for windows from a linux machine; this is how the official builds are done, see [regular build instructions](building.md) which can easily be altered to use a cross compiler.

It should also be possible to compile via msys or cygwin on windows, there are currently no detailed instructions for this, if you attempt this please consider contributing instructions so that others may benefit from your experience.

Binaries
-----
There are binaries for every release, please reconsider your need to build and unless you have a very good reason to do so rather just download these.
Latest binaries can always be found here: https://github.com/Munt/munt-official/releases
Download the latest linux\*.tar.gz extract it and simply copy Munt-daemon out of it instead of going through the unnecessary hassle of building.


Installation of msys2
-----
* Download and run the 64 bit msys installer - www.msys2.org
* Open the *MSYS2 MinGW 64-bit* console (also called *Mingw-w64 64 bit* in the Mintty launcher)
* `pacman -Syu`
* Close and restart the console
* `pacman -Su`
* `pacman -S --noconfirm mingw-w64-i686-toolchain mingw-w64-i686-python2 git make patch tar autoconf automake libtool pkg-config mingw-w64-x86_64-gcc`

Building under msys2 (nodejs addon)
-----
* `git clone https://github.com/muntorg/munt-official`
* `cd munt-official`
* `./developer-tools/build_node.sh`

Building under msys2
-----
* `git clone https://github.com/muntorg/munt-official`
* `cd munt-official/depends`
* `make`
* `cd ..`
* `./autogen.sh`
* `mkdir buildwin`
* `cd buildwin`
* `CONFIG_SITE="$PWD/../depends/i686-pc-mingw32/share/config.site" CXXFLAGS="-I$PWD/../depends/i686-pc-mingw32/include -DZMQ_STATIC" LDFLAGS="-L$PWD/../depends/i686-pc-mingw32/lib" ../configure --prefix=$PWD/../depends/i686-pc-mingw32 --with-protoc-bindir=$PWD/../depends/i686-pc-mingw32/native/bin`
* `make`


Installation of WSL
-----

First install the Windows Subsystem for Linux by opening PowerShell as Administrator and run:

`Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux`

Restart computer when prompted and then install Ubuntu from the following link

`https://www.microsoft.com/store/p/ubuntu/9nblggh4msv6`

Update and upgrade ubuntu within WSL

`sudo apt update && sudo apt upgrade`

Install build prerequisites, git and some build dependencies

`sudo apt-get install curl build-essential libtool autotools-dev autoconf pkg-config libssl-dev libpcre++-dev git`

Clone a copy of this repository

`git clone https://github.com/muntorg/munt-official.git`

`cd munt-official`

(Optional) If you want to contribute towards development you may want to check out a development branch

`git checkout -b 2.1_development remotes/origin/2.1_development`

Build Munt dependencies

`cd depends`

`sudo make`

`cd ..`

Build Munt

`./autogen.sh`

`mkdir build && cd build`

`../configure --prefix=$PWD/../depends/x86_64-pc-linux-gnu/`

`make -j$(nproc)`


Running the compiled software
-----

If you want to run the GUI software after compiling it in WSL then some additional steps are necessary, otherwise you will receive the error "QXcbConnection: Could not connect to display".
This is not necessary for the command line versions of the software.
See this guide for more information: https://virtualizationreview.com/articles/2017/02/08/graphical-programs-on-windows-subsystem-on-linux.aspx?m=1

Remove minimal openssh package and replace with the full package

`sudo apt-get remove openssh-server`

`sudo apt-get install openssh-server`

Change WSL SSH port so th at it doesn't conflict with existing windows SSH server; alternatively you can disable the windows one.

`sudo nano /etc/ssh/sshd_config`

Perform the following lines to the file and then save your changes.

Add:

  ` AllowUsers yourusername`

  ` UsePrivilegeSeparation no`

Change:

  ` PermitRootLogin no`

  ` PasswordAuthentication yes`

  ` ListenAddress 0.0.0.0`

  ` Port 2200`

 Restart ssh service

`sudo service ssh --full-restart`

Install vcXsrv on windows host; Other alternatives like Xming may also be possible

`https://sourceforge.net/projects/vcxsrv/`

After installing vcXsrv. Start XLaunch (with default settings) and then on WSL execute:

`export DISPLAY=:0`

`LD_LIBRARY_PATH=depends/x86_64-pc-linux-gnu/lib/ ./src/qt/Munt`

Modify the `LD_LIBRARY_PATH` as appropriate for your system.


Troubleshooting
-----

I receive an error "QXcbConnection: Could not connect to display" when attempting to run the compiled software
> Follow the steps "Running the compiled software" 

