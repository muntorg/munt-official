Developer tools
---------------------

This directory, and sub-directories, contain various scripts that are not a core part of the repository itself, but aid developers with working with maintaining the repository, or building the repository for specific configurations and so forth.

* build_docker.sh        - Build Munt/Munt-daemon binaries inside a docker container
* build_node.sh          - Build a copy of the Munt core "unity" library that can be used from node/electron applications
* build_android_core.sh  - Build a copy of the Munt core "unity" library that can be used from android applications
* build_ios_core.sh      - Build a copy of the Munt core "unity" library that can be used from ios applications
* gencheckpoints.sh      - Generate updated/recent checkpoints to be used for new builds (result must be manually pasted into chainparams.cpp as appropriate)
* genbootstrap.sh        - Build a bootstrap file up until most recent blocks; If an existing checkpoint file exists then it will update the existing file instead of starting from genesis each time.
* update_translations.sh - Sync the translation strings with the latest from translate.munt.org (bi-directional)



Settings
---------------------
### private.conf.example ###

All developer tools scripts make use of a single configuration file 'private.conf' in which developer specific settings/preferences can be set. 'private.conf' is included in .gitignore to prevent accidental commit to the repository.
'private.conf'example' serves as an easy reference of all possible options that can be set, new developers can copy this to create 'private.conf' and then alter options as needed. When adding new options to scripts please also add them to 'private.conf.example' to keep it complete.



Adding/Developing new scripts
---------------------

New scripts should be a bash shell script (.sh) that sources `private.conf`.
It is okay to use other language such as python to do the bulk of the work, many of the existing scripts already do this, in such a case just place the `.py` part of the script in an appropriate place (e.g. python sub-folder) and have a wrapper `.sh` that sets things up.

