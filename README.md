<table cellspacing="0" cellpadding="0" color="grey" border="1px">
  <tr border=0>
    <td border="0px" width="80%" rowspan="7">
      <a href="https://www.Gulden.com">
        <img align="left" src="https://gulden.com/img/design/g-111444.svg" alt="Gulden"/>
      </a>
      <p>Gulden is a <i>revolutionary</i> next generation blockchain based currency and payment system.<br/>
      Gulden takes the blockchain concept to a whole new level; improving on multiple downsides and deficiencies in the earlier implementations of blockchain.</p>
      <p>The project is driven at the core by a focus on key concepts like <i>usability</i> and <i>quality</i> which shows through in our software.</p><p>Join the Gulden project today and help build the future!</p>
    </td>
    <td width="20%" border=0>
      <a href="#">
        <img height="20px" src="https://travis-ci.org/Gulden/gulden-official.svg?branch=master" alt="ci build status"/>
      </a>
    </td>
  </tr>
  <tr border=0>
    <td>
      <a href="https://github.com/Gulden/gulden-official/issues">
        <img  height="20px" src="https://img.shields.io/github/issues/gulden/gulden-official.svg" alt="open issues"/>
    </td>
  </tr>
  <tr border=0>
    <td>
      <a href="https://github.com/Gulden/gulden-official/issues?q=is%3Aissue+is%3Aclosed">
        <img  height="20px" src="https://img.shields.io/github/issues-closed/gulden/gulden-official.svg" alt="closed issues"/>
      </a>
    </td>
  </tr>
  <tr border=0>
    <td border=0>
      <a href="https://github.com/Gulden/gulden-official/releases">
        <img height="20px" src="https://img.shields.io/github/downloads/gulden/gulden-official/total.svg" alt="total downloads"/>
      </a>
    </td>
  </tr>
  <tr border=0>
    <td>
      <a href="https://github.com/Gulden/gulden-official/commits/master">
        <img height="20px" src="https://img.shields.io/github/commit-activity/y/gulden/gulden-official.svg" alt="commits 1y"/>
      </a>
    </td>
  </tr>
  <tr>
    <td>&nbsp;</td>
  </tr>
</table>



### License
All code, binaries and other assets in this repository are subject to [The Gulden license](https://github.com/Gulden/gulden-official/blob/master/COPYING_gulden) except where explicitely stated otherwise.

### Contributing
If you are thinking about contributing toward the development of Gulden in some capacity whether small or large, code or translating; Please read [this guide](./CONTRIBUTING.md) first for information on how to do so.

### Technical documentation
* [PoW² whitepaper](.//technical_documentation/Gulden_PoW2.pdf); [PoW² activation](./technical_documentation/PoW2_activation.md)
* [Transaction format](./technical_documentation/transaction_format.md)
* [Account system](./technical_documentation/account_system.md)
* [Accelerated testnet](./technical_documentation/accelerated_testnet.md)
* [Information for mining pools](./mining_documentation/mining_Gulden.md); [PoW² upgrade for mining pools](./mining_documentation/ensuring_pow2_compatibility.md); [List of identified mining pools](./mining_documentation/list_of_mining_pools.md)


### Community

Connect with the community through one or more of the following:

[Website](https://gulden.com) ◾ [Slack](https://gulden.com/join) ◾ [Twitter](http://twitter.com/gulden) ◾ [Facebook](http://facebook.com/gulden) ◾ [Meetup](https://www.meetup.com/gulden) ◾ [Reddit](https://www.reddit.com/r/GuldenCommunity) ◾ [IRC](https://webchat.freenode.net/?channels=Gulden)


### Downloading

The latest binaries and installers can be found [here](https://github.com/Gulden/gulden-official/releases) for all platforms, including raspbian.

### Building
Reconsider whether it is actually necessary for you to build.
[Binaries for both the UI software as well as the command line/server are provided by us at every release for multiple architectures.](https://github.com/Gulden/gulden-official/releases) For users who are not developers a lot of headaches can be avoided by simply using these.

If you are sure you need to build the software yourself, please read the [build instructions](./doc/building.md) before attempting to build the software.

### Additional technical information


|Technical specifications|Main network|Testnet|
|:-----------|:---------|:---------|
|Consensus algorithm:|PoW² Scrypt/Witness|PoW² Scrypt/Witness|
|Recommended transaction confirmations:|2|2|
|Block reward scrypt:|80 NLG|1000 NLG|
|Block reward witness:|20 NLG|20 NLG|
|Block interval target:|150 seconds (2.5 minutes)|Configurable|
|Difficulty adjustment period:|Every block|Every block|
|Difficulty adjustment algorithm:|Delta|Delta|
|Total coins to be minted over time:|1680M|-|
|P2P Port|9231|9923|
|RPC Port|9232|9924|
|P2P Network Header|fcfef7e0|Look in debug.log|
|Address version byte|38 (G)|65 (T)|
|P2SH version byte|98 (g)|127 (t)|
|BIP44 coin type|87 0x80000057|87 0x80000057|
|**Infrastructure**|**Main network**|**Testnet**|
|Official block explorer|blockchain.gulden.com|-|
|Community block explorer|www.dactual.com|-|
|Community block explorer|www.guldenchain.com|testnet.guldenchain.com|
|DNS Seed 1|seed.gulden.com|-|
|DNS Seed 2|amsterdam.gulden.com|-|
|DNS Seed 3|rotterdam.gulden.network|-|
