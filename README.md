<table cellspacing="0" cellpadding="0" color="grey" border="1px">
<trborder=0>
<td border="0px" width="80%" rowspan="7"><img src="https://dev.gulden.com/images/branding/gblue256x256.png" alt="Gulden"/></td>
<td border=0><img height="20px" src="https://travis-ci.org/Gulden/gulden-official.svg?branch=pow2_testing" alt="ci build status"/>
</tr>
<tr border=0><td border=0><img height="20px" src="https://img.shields.io/github/downloads/gulden/gulden-official/total.svg" alt="total downloads"/></td></tr>
<tr><td><img  height="20px" src="https://img.shields.io/github/commit-activity/4w/gulden/gulden-official.svg" alt="commits 4w"/></td></tr>
  <tr><td>&nbsp;</td></tr>
  <tr><td>&nbsp;</td></tr>
  <tr><td>&nbsp;</td></tr>
</table>



### License
All code is subject to [The Gulden license](https://github.com/Gulden/gulden-official/blob/master/COPYING_gulden)

### Contributing
If you are thinking about contributing toward the development of Gulden in some capacity whether small or large, code or translating; Please read [this guide](./CONTRIBUTING.md) first for information on how to do so.

### Technical documentation
* [PoW² whitepaper](https://github.com/Gulden/gulden-official/raw/master/technical_documentation/Gulden_PoW2.pdf)

### Community

Connect with the community through one or more of the following:

* [Website](https://gulden.com)
* [Slack](https://gulden.com/join)
* [Twitter](http://twitter.com/gulden)
* [Facebook](http://facebook.com/gulden)
* [Meetup](https://www.meetup.com/gulden)
* [Reddit](https://www.reddit.com/r/GuldenCommunity)
* [IRC](https://webchat.freenode.net/?channels=Gulden)


### Downloading

The latest binaries and installers can be found [here](https://github.com/Gulden/gulden-official/releases) for all platforms, including raspbian.

### Building
First, reconsider whether it is actually necessary for you to build.
[Linux binaries for the daemon are provided by us at every release for multiple architectures and are best in most cases. A lot of headaches can be saved by simply using these, especially if you are not an experienced developer.](https://github.com/Gulden/gulden-official/releases)

Please read all the below instructions before attempting to build the software or asking for assistance with building the software.

Distro specific instructions:

|Distro|Version|
|:-----------|:-------|
|Ubuntu|[16.04.1](https://gist.github.com/mjmacleod/a3562af661661ce6206e5950e406ff9d) |
|Ubuntu|[14.04.4](https://gist.github.com/mjmacleod/31ad31386fcb421a7ba04948e83ace76) |


Generic instructions:

Gulden is autotools based, to build Gulden from this repository please follow these steps:
* ./autogen.sh
* automake
* ./configure
* make

Prerequisites:
> &gt;=g++-78 pkg-config autoconf libtool automake gperf bison flex

Required dependencies:
> &gt;=bdb-4.8.30 &gt;=boost-1_66_0 expat-2 openssl miniupnpc protobuf zeromq

Optional dependencies (depending on configure - e.g. qt only for GUI builds):
> dbus fontconfig freetype icu libevent libX11 libXau libxcb libXext libXrender  qrencode &gt;=qt-5.6.1 renderproto xcb_proto xextproto xproto xtrans


Troubleshooting:

If your distro  does not have boost 1.66 you can do the following to build it
cd depends
make boost

And then add it to your configure flags
> ./configure --with-boost=<path> LDFLAGS="-L<path>/lib/" CPPFLAGS="-I<path>/include" <otherconfigureflagshere>

To run after doing the above
> LD_LIBRARY_PATH=<path>/lib src/GuldenD 

If your distro is missing Berkley DB 4.8 (error: Found Berkeley DB other than 4.8, required for portable wallets)
Either configure with an incompatible bdb (Your wallet may not be portable to machines using older versions in this case):
> ./configure --with-incompatible-bdb <otherconfigureflagshere>

Or compile your own:
> sudo mkdir /db-4.8 && sudo chmod -R a+rwx /db-4.8 && wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz' && tar -xzvf db-4.8.30.NC.tar.gz && cd db-4.8.30.NC/build_unix/ && ../dist/configure --enable-cxx --disable-shared --with-pic --prefix=/db-4.8/ && make install

> ./configure LDFLAGS="-L/db-4.8/lib/" CPPFLAGS="-I/db-4.8/include"

Binaries are output as follows by the build process:

|Binary|Location|
|:-----------|:---------|
|Qt wallet|src/qt/Gulden|
|Gulden Daemon/RPC server|src/GuldenD|
|Gulden RPC client|src/Gulden-cli|
|Gulden tx utility|src/Gulden-tx|




### Additional technical information


|Technical specifications|Main network|Testnet|
|:-----------|:---------|:---------|
|Algorithm:|Scrypt proof of work, with DELTA difficulty adjustment||
|Transaction confirmations:|6||
|Total coins:|1680M||
|Premine reserved for development, marketing and funding of community projects:|170M (10%)||
|Block reward:|100 NLG||
|Block time:|150 seconds||
|P2P Port|9231|9923|
|RPC Port|9232|9924|
|P2P Network Header|fcfef7e0|fcfef702|
|Address version byte|38 (G)|65 (T)|
|P2SH version byte|98 (g)|127 (t)|
|BIP44 coin type|87 0x80000057||

|Infrastructure|Main network|Testnet|
|:-----------|:---------|:---------|
|Official block explorer|blockchain.gulden.com||
|Community block explorer|guldenchain.com|testnet.guldenchain.com|
|Electrum server 1|electrum1.gulden.com:5038||
|Electrum server 2|electrum2.gulden.com:5038||
|DNS Seed 1|seed.gulden.com||
|DNS Seed 2|amsterdam.gulden.com||
|DNS Seed 3|seed.gulden.network||
|DNS Seed 4|rotterdam.gulden.network||
|DNS Seed 5|seed.gulden.blue||

### Official testnet settings
-testnet=C1511943855:60 -addnode=64.137.191.5 -addnode=64.137.228.95 -addnode=64.137.228.88 -addnode=64.137.228.46
