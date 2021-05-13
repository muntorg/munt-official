Often when there is behaviour like a crash it can be very situation specific. i.e. something specific to your machine/network/setup/wallet is triggering the crash and it doesn't necessarily happen for others.
In such a case the developers sometimes struggle to figure out why such a crash is happening without further information.

The most useful information in this situation is what is called a "stack trace" which can provide the developer with some direct insight into what may be triggering the crash.

If you are reading this it is likely because a developer has asked you for a stack trace, heres how to get one:

1. Install `gdb` - on some platforms (linux) it may be already installed easy to install via your package manager, on others (windows) some steps are required.
* Download and install https://www.msys2.org/
* Run msys2 console (64 bit version)
* Run `pacman -S mingw-w64-x86_64-gdb` to install `gdb`
2. Determine the system path for debug files, you can do this as follows:
* Type `gdb` to run `gdb`
* Type `show debug-file-directory` to list the system debug file path (default on windows will usually be `c:\mingw64\lib\debug`)
* Note down the path and type exit to close `gdb`
3. Fetch the debug file from github (`xxx.node.dbg` - e.g. at time of writing for windows `libflorin_win_x64.node.dbg`) and place it in the directory that you obtained in step 2, if this directory does not already exist first manually create it
* Note the debug file must match the version of the software you are running
4. Launch the application
5. Determine the pid of the application, on windows this can be found in `task manager` 
6. Run `gdb` again this time passing it the PID e.g `gdb --pid 3956` (substitute your own PID here, it changes for every run of the application)
7. Wait a bit for `gdb` to load, it will pause the application
8. Type `info shared` to get a list of shared libraries, look for the one ending with `.node` (it will have a randomised name e.g. `C:\Users\User\AppData\Local\Temp\410aa229-1397-4309-b200-c30cff6348d0.tmp.node`
9. Copy the address (from the first column) that corresponds to it e.g. `0x0000000071861000`
10. Type `add-symbol-file c:/mingw64/lib/debug/libflorin_win_x64.node.dbg 0x0000000071861000` substitute the address to match the one you took from step 9, and the path to match your path from step 3.
11. type `continue` to allow the application to continue
12. Execute the procedures you normally do to trigger a crash, the application will "freeze" instead of crashing as `gdb` will "catch" the crash and pause the application
13. When this occurs type `bt` in gdb and then after that `thread apply all bt`
14. Copy all the output (this is the stack trace) and provide it to the developer, along with any other pertinent information that might be helpful (debug.log, brief description of what you were doing at time of crash etc.)

NB! It is possible to force the program to crash by typing `forcesigseg` in the debug console of the application.
It is recommended to do this once as a trial run (after following the above procedures) to ensure your debug setup works properly and that you know how to do everything.
And only then once you are sure it works, proceed to try and trigger the real crash.

If your debug setup is correct you should see something like:
```
Thread 1384 received signal SIGSEGV, Segmentation fault.
[Switching to Thread 6664.0x1e8]
forcesigseg (request=...) at ../../src/rpc/misc.cpp:534
534     ../../src/rpc/misc.cpp: No such file or directory
```
While if there is a problem you will see something like:
```
Thread 83957 received signal SIGSEGV, Segmentation fault.
[Switching to Thread 3596.0x2964]
0x0000000071aa8150 in ?? ()```

