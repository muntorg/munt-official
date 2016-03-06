#Gulden#

###About###
Gulden is a simple and very powerful payment system. Gulden uses blockchain technology to improve financial systems, speed up transactions and offer a cost-effective solution to all things finance. Our software is open source, meaning you can implement Gulden in your own software and build tools and services that help accomplish our goal; creating a stable and blockchain-based financial system built on a trustless and permissionless network that connects with all our financial needs.


Find out more on http://Gulden.com or connect with the community through one or more of the following websites:

|Website:|http://Gulden.com|
|:-----------|:-------|
|Official forums:|http://community.gulden.com|
|Community forums:|http://gulden.zone|
|Twitter:|http://twitter.com/gulden|
|Facebook:|http://facebook.com/guldenpay|
|Reddit:|https://reddit.com/r/gulden/|
|IRC:|https://webchat.freenode.net/?channels=Gulden|


You can also join a dedicated group of individuals on slack. Get your invitation here: 
https://Gulden.com/join


|Technical specifications||
|:-----------|:---------|
|Algorithm:|Scrypt proof of work, with DELTA difficulty adjustment|
|Transaction confirmations:|6|
|Total coins:|1680M|
|Premine reserved for development, marketing and funding of community projects:|170M (10%)|
|Starting diff:|0.00244|
|Block reward:|100 NLG|
|Block time:|150 seconds|
|P2P Port|9231|
|P2P Port [Testnet]|9923|
|RPC Port|9232|
|RPC Port [Testnet]|9924|
|P2P Network Header|fcfef7e0|
|P2P Network Header [Testnet]|fcfef700|
|Address version byte|38|
|Address version byte [Testnet]|98|




###Building###
Gulden is autotools based, to build Gulden from this repository please follow these steps:
* ./autogen.sh
* automake
* ./configure
* make

For older distributions with outdated autotools the above process may fail, a source tarball is available for these distributions.
* https://developer.gulden.com/download/155/Gulden155-linux.tar.gz
To build from source tarball follow these steps:
* tar -zxvf Gulden155-linux.tar.gz
* ./configure
* make
