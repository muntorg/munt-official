Guldencoin Core 0.10.99
=====================

Setup
---------------------
[Guldencoin Core](https://guldencoin.com/download) is the original Guldencoin client and it builds the backbone of the network. However, it downloads and stores the entire history of Bitcoin transactions (which is currently several GBs); depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more.

Running
---------------------
The following are some helpful notes on how to run Guldencoin on your native platform. 

### Unix

You need the Qt4 run-time libraries to run Guldencoin-Qt. On Debian or Ubuntu:

	sudo apt-get install libqtgui4

Unpack the files into a directory and run:

- bin/32/guldencoin-qt (GUI, 32-bit) or bin/32/guldencoind (headless, 32-bit)
- bin/64/guldencoin-qt (GUI, 64-bit) or bin/64/guldencoind (headless, 64-bit)



### Windows

Unpack the files into a directory, and then run guldencoin-qt.exe.

### OSX

Drag Guldencoin-Qt to your applications folder, and then run Guldencoin-Qt.

### Need Help?

* See the documentation at the [Bitcoin Wiki](https://en.bitcoin.it/wiki/Main_Page)
for help and more information.
* Ask for help on [#guldencoin] on irc.tweakers.net.
* Ask for help on the [Guldencoin Community Forum](https://community.guldencoin.com/) forums.
* Ask for help on the [BitcoinTalk](https://bitcointalk.org/) forums, in the [Guldencoin thread](https://bitcointalk.org/index.php?topic=554412).

Building
---------------------
The following are developer notes on how to build Guldencoin on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [OSX Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)

Development
---------------------
The Guldencoin repo's [root README](https://github.com/nlgcoin/guldencoin2/blob/master/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Multiwallet Qt Development](multiwallet-qt.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://dev.visucore.com/bitcoin/doxygen/)
- [Translation Process](translation_process.md)
- [Unit Tests](unit-tests.md)

### Resources
* Discuss on [#guldencoin] on irc.tweakers.net.
* Discuss on the [Guldencoin Community Forum](https://community.guldencoin.com/) forums in the development category (https://community.guldencoin.com/c/development).

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)

License
---------------------
Distributed under the [MIT software license](http://www.opensource.org/licenses/mit-license.php).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
