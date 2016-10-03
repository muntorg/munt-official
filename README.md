#Gulden#

###About###
Gulden is a simple and very powerful payment system. Gulden uses blockchain technology to improve financial systems, speed up transactions and offer a cost-effective solution to all things finance. Our software is open source, meaning you can implement Gulden in your own software and build tools and services that help accomplish our goal; creating a stable and blockchain-based financial system built on a trustless and permissionless network that connects with all our financial needs.

###Community###

Find out more on https://gulden.com
You can also join a dedicated group of individuals on slack for realtime discussion. Get your invitation at https://Gulden.com/join

Or connect with the community through one or more of the following websites:

|Website:|https://gulden.com|
|:-----------|:-------|
|Slack:|https://guldenpay.slack.com/|
|Official forums:|https://community.gulden.com|
|Twitter:|http://twitter.com/gulden|
|Facebook:|http://facebook.com/guldenpay|
|IRC:|https://webchat.freenode.net/?channels=Gulden|
|Community forums:|https://www.guldenforum.nl/|


###Building###
Gulden is autotools based, to build Gulden from this repository please follow these steps:
* ./autogen.sh
* automake
* ./configure
* make

On some distributions (Ubuntu 16.04) there may be issues when compiling with boost >= 1.58.0 to work around this configure with the following (in adition to your usual configure flags)
CXXFLAGS="$CXXFLAGS -DBOOST_VARIANT_USE_RELAXED_GET_BY_DEFAULT=1" ./configure

For older distributions with outdated autotools the above process may fail, a source tarball is available for these distributions.
* https://developer.gulden.com/download/155/Gulden155-linux.tar.gz
To build from source tarball follow these steps:
* tar -zxvf Gulden155-linux.tar.gz
* ./configure
* make

Binaries are output as follows by the build process:

|Binary|Location|
|:-----------|:---------|
|Qt wallet|src/qt/Gulden|
|Gulden Daemon/RPC server|src/GuldenD|
|Gulden RPC client|src/Gulden-cli|
|Gulden tx utility|src/Gulden-tx|

Alternatively binaries are also available https://developer.gulden.com/apps/


###Additional technical information###


|Technical specifications - Main network||
|:-----------|:---------|
|Algorithm:|Scrypt proof of work, with DELTA difficulty adjustment|
|Transaction confirmations:|6|
|Total coins:|1680M|
|Premine reserved for development, marketing and funding of community projects:|170M (10%)|
|Starting diff:|0.00244|
|Block reward:|100 NLG|
|Block time:|150 seconds|
|P2P Port|9231|
|RPC Port|9232|
|P2P Network Header|fcfef7e0|
|Address version byte|38|
|BIP44 'coin type'|87 0x80000057|

|Technical specifications - Testnet||
|:-----------|:---------|
|P2P Port|9923|
|RPC Port|9924|
|P2P Network Header|fcfef700|
|Address version byte|98|

|Infrastructure||
|:-----------|:---------|
|Official block explorer|blockchain.gulden.com|
|Community block explorer|inzicht.deguldenmijn.eu|
|Electrum server 1|electrum1.gulden.com:5038|
|Electrum server 2|electrum2.gulden.com:5038|
|Seed1|seed.gulden.com|
|Seed2|amsterdam.gulden.com|
|Seed3|seed.gulden.network|
|Seed4|rotterdam.gulden.network|
