<table cellspacing="0" cellpadding="0" color="grey" border="1px">
  <tr border=0>
    <td border="0px" width="80%" rowspan="7">
      <a href="https://www.munt.org">
        <img height=260px align="left" src="https://munt.org/img/design/munt-000000.svg" alt="Munt"/>
      </a>
      <p>Munt is a witness-secured decentralized network for payments, digital assets, finance and more<br/>
      Munt takes the basic blockchain concept and improves on the areas where it has shortcomings in order to make a product that is more suitable for every day use.</p>
      <p>The project is driven at the core by a focus on key concepts of <i>usability</i> and <i>quality</i>.</p><p>Join the Munt project today and help build the future!</p>
    </td>
    <td width="20%" border=0>
      <a href="#">
        <img height="20px" src="https://travis-ci.org/muntorg/munt-official.svg?branch=master" alt="ci build status"/>
      </a>
    </td>
  </tr>
  <tr border=0>
    <td>
      <a href="https://github.com/muntorg/munt-official/issues">
        <img  height="20px" src="https://img.shields.io/github/issues/muntorg/munt-official.svg?color=blue" alt="open issues"/>
    </td>
  </tr>
  <tr border=0>
    <td>
      <a href="https://github.com/muntorg/munt-official/issues?q=is%3Aissue+is%3Aclosed">
        <img  height="20px" src="https://img.shields.io/github/issues-closed/muntorg/munt-official.svg?color=blue" alt="closed issues"/>
      </a>
    </td>
  </tr>
  <tr border=0>
    <td border=0>
      <a href="https://github.com/muntorg/munt-official/releases">
        <img height="20px" src="https://img.shields.io/github/downloads/muntorg/munt-official/total.svg?color=blue" alt="total downloads"/>
      </a>
    </td>
  </tr>
  <tr border=0>
    <td>
      <a href="https://github.com/munt/munt-official/commits/master">
        <img height="20px" src="https://img.shields.io/github/commit-activity/y/muntorg/munt-official.svg" alt="commits 1y"/>
      </a>
    </td>
  </tr>
  <tr>
    <td>
      <a href="https://github.com/muntorg/munt-official/compare/master@%7B12month%7D...develop">
        <img height="20px" src="https://img.shields.io/badge/dev%20branch-develop-blue.svg" alt="active_branch"/>
      </a>
    </td>
  </tr>
</table>



### License
All code, binaries and other assets in this repository are subject to [The Libre Chain License](https://github.com/muntorg/munt-official/blob/master/COPYING_munt) except where explicitely stated otherwise.

### Branches
`master` branch tracks the current public release; is generally updated only for each release (with accompanying tag) and very occasionally for minor documentation or other commits. If all you want is to build/track the current version of the sofrware than use the `master` branch.

`develop` branch tracks current major development branch
This is where most development that will go into the next major release is always taking place (feature branches are merged into here) and is linked in the table at the top of this README as the `dev branch`. For development changes you will generally want to work on this branch.


Major features are worked on in temporary feature branches until they can be merged back into one of the development branches. 
The currently most active branch, where most development that will go into the next major release is taking place is linked in the table at the top of this README as the `dev branch` and may be updated as development mandates seperate branches, for development changes you will generally want to work on this branch.

Old versions that are still maintained get their own version branch e.g. there might be `2.4_development` even if we are already on `3.x` development series


### Contributing
If you are thinking about contributing toward the development of Munt in some capacity whether small or large, code or translating; Please read [this guide](./CONTRIBUTING.md) first for information on how to do so.

### Technical documentation
* [PoW² whitepaper](.//technical_documentation/PoW2.pdf); [PoW² activation](./technical_documentation/PoW2_activation.md)
* [Transaction format](./technical_documentation/transaction_format.md)
* [Account system](./technical_documentation/account_system.md)
* [Accelerated testnet](./technical_documentation/accelerated_testnet.md)
* [Official testnet](./technical_documentation/accelerated_testnet.md#official-testnet)


### Community

Connect with the community through one or more of the following:

[Website](https://munt.org) ◾ [Slack](https://munt.org/join) ◾ [Twitter](https://twitter.com/munt_org) ◾ [Facebook](http://facebook.com/gulden) ◾ [Meetup](https://www.meetup.com/gulden) ◾ [Reddit](https://www.reddit.com/r/munt) ◾ [IRC](https://web.libera.chat/?channels=Muntorg)


### Downloading

The latest binaries and installers can be found [here](https://github.com/muntorg/munt-official/releases) for all platforms, including raspbian.

### Building
[Binaries](https://github.com/munt/munt-official/releases) for both the UI as well as the daemon and command line interface for multiple architectures.

For those who really need too or are technically inclined it is of course possible to build the software yourself. Please read the [build instructions](./doc/building.md) before attempting to build the software and/or seeking support.

### Additional technical information


|Technical specifications|Main network|[Testnet](./technical_documentation/accelerated_testnet.md#official-testnet)|
|:-----------|:---------|:---------|
|Consensus algorithm:|PoW² SIGMA/Witness|PoW² SIGMA/Witness|
|Recommended transaction confirmations:|2|2|
|Block reward SIGMA:|75 (10|65) MUNT|1000 MUNT|
|Block reward witness:|15 MUNT|20 MUNT|
|Block interval target:|150 seconds (2.5 minutes)|Configurable|
|Difficulty adjustment period:|Every block|Every block|
|Difficulty adjustment algorithm:|Delta|Delta|
|Total coins to be minted over time:|750M|-|
|P2P Port|9231|9923|
|RPC Port|9232|9924|
|P2P Network Header|fcfef7e0|Look in debug.log|
|Address version byte|38 (G)|65 (T)|
|P2SH version byte|98 (g)|127 (t)|
|BIP44 coin type|87 0x80000057|87 0x80000057|
|**Infrastructure**|**Main network**|**[Testnet](./technical_documentation/accelerated_testnet.md#official-testnet)**|
|Official block explorer|https://explorer.munt.org/|-|
|Community block explorer|https://muntbook.org/|-|
|Community block explorer|https://munt.chainviewer.org/|-|
|DNS Seed 1|seed.gulden.com|-|
|DNS Seed 2|amsterdam.gulden.com|-|
|DNS Seed 3|rotterdam.gulden.network|-|
