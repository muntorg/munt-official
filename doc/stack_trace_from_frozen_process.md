Often when there is behaviour like a frozen program it can be very situation specific. i.e. something specific to your machine/network/setup/wallet is triggering the freeze and it doesn't necessarily happen for others, or if it does its only a subset of users and the pattern of which users are/aren't affected is often difficult to determine.
In such a case the developers sometimes struggle to figure out why such a crash is happening without further information and no way to reproduce the crash themselves.

The most useful information in this situation is what is called a "stack trace" which can provide the developer with some direct insight into what may be triggering the crash.

If you are reading this it is likely because a developer has asked you for a stack trace, heres how to get one:

1. DO NOT CLOSE THE FROZEN PROGRAM - KEEP IT OPEN
2. Install `gdb` - on some platforms (linux) it may be already installed easy to install via your package manager, on others (windows) some steps are required.
* Download and install https://www.msys2.org/
* Run msys2 console (64 bit version)
* Run `pacman -S mingw-w64-x86_64-gdb` to install `gdb`
3. Fetch the debug info file from github for your arcitecture (e.g. `Munt-3.0.0-x86_64-linux-gnu-debug.tar.gz` for 64 bit linux debug info) extract and place in a folder on your harddrive
* Note the debug file must match the version of the software you are running; make sure to download a version that matches
4. Launch the application
5. Determine the pid of the application
* Windows - This can be found in `task manager` (right click on columns at the top to enable PID column; then search for Munt in the list of running tasks and take note of the PID)
* Linux/macos - `ps aux | grep Munt` in the console/terminal, then note down the PID
6. Run `gdb` passing it the PID e.g `gdb --pid 3956` (substitute your own PID here from step 5 here, it changes for every run of the application)
7. Wait a bit for `gdb` to load, it will pause the application
8. Type `info files` - at the very top there will be an "entry point" note this down
```
Local exec file:
        `/tmp/.mount_MuntFboNYk/usr/bin/Munt', file type elf64-x86-64.
        Entry point: 0x55ca5940bb30
```
* In this example the entry point is 0x55ca5940bb30
9. Use the path from step 3 and the `entry point` from step 8 and enter it via the `add-symbol-file` command
* `add-symbol-file /home/developer/dbg/Munt.dbg 0x55ca5940bb30`
11. Enter `y` to accept
12. Run the command `thread apply all bt`
13. Allow all output to print (push `c` if prompted to have it display it all)
14. Copy all of the output and provide it to a developer

This will provide the developer with highly useful and actionable information with which they can proceed to look into the issue. It is recommended to also make a copy of your `debug.log` as the developer may want this as well.
Once you have both of these things you can proceed to close the program and restart it.
