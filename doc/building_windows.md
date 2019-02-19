Note
-----

The easiest least technical way to build a copy of the software on a windows machine is to make use of WSL, the instructions below detail how to do this.


This is however not the only way, it is possible also to cross compile for windows from a linux machine; this is how the official builds are done, see [regular build instructions](building.md)

Finally there are various other ways to compile via mingw/cygwin on windows, there are currently no detailed instructions for this, if you attempt this please consider contributing instructions.

Binaries
-----
There are binaries for every release, please reconsider your need to build and unless you have a very good reason to do so rather just download these.
Latest binaries can always be found here: https://github.com/Gulden/gulden-official/releases
Download the latest linux*.tar.gz extract it and simply copy GuldenD out of it instead of going through the unnecessary hassle of building.


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

`git clone https://github.com/gulden/gulden-official.git`

`cd gulden-official`

(Optional) If you want to contribute towards development you may want to check out a development branch

`git checkout -b 2.1_development remotes/origin/2.1_development`

Build Gulden dependencies

`cd depends`

`sudo make`

`cd ..`

Build Gulden

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

`LD_LIBRARY_PATH=depends/x86_64-pc-linux-gnu/lib/ ./src/qt/Gulden`

Modify the `LD_LIBRARY_PATH` as appropriate for your system.


Troubleshooting
-----

I receive an error "QXcbConnection: Could not connect to display" when attempting to run the compiled software
> Follow the steps "Running the compiled software" 

