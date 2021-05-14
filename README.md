<table cellspacing="0" cellpadding="0" color="grey" border="1px">
  <tr border=0>
    <td border="0px" width="80%" rowspan="7">
      <a href="https://www.florin.org">
        <img align="left" src="./src/frontend/electron/img/icon_128.png" alt="Florin"/>
      </a>
      <p>Florin; The climate friendly blockchain based currency with an even distribution<br/>
    </td>
    <td width="20%" border=0>
      <a href="#">
        <img height="20px" src="https://travis-ci.org/florinxfl/florin-core.svg?branch=master" alt="ci build status"/>
      </a>
    </td>
  </tr>
  <tr border=0>
    <td>
      <a href="https://github.com/florinxfl/florin-core/issues">
        <img  height="20px" src="https://img.shields.io/github/issues/florinxfl/florin-core.svg?color=blue" alt="open issues"/>
    </td>
  </tr>
  <tr border=0>
    <td>
      <a href="https://github.com/florinxfl/florin-core/issues?q=is%3Aissue+is%3Aclosed">
        <img  height="20px" src="https://img.shields.io/github/issues-closed/florinxfl/florin-core.svg?color=blue" alt="closed issues"/>
      </a>
    </td>
  </tr>
  <tr border=0>
    <td border=0>
      <a href="https://github.com/florinxfl/florin-core/releases">
        <img height="20px" src="https://img.shields.io/github/downloads/florinxfl/florin-core/total.svg?color=blue" alt="total downloads"/>
      </a>
    </td>
  </tr>
  <tr border=0>
    <td>
      <a href="https://github.com/florinxfl/florin-core/commits/master">
        <img height="20px" src="https://img.shields.io/github/commit-activity/y/florinxfl/florin-core.svg" alt="commits 1y"/>
      </a>
    </td>
  </tr>
  <tr>
    <td>
      <a href="https://github.com/florinxfl/florin-core/compare/master@%7B12month%7D...development">
        <img height="20px" src="https://img.shields.io/badge/dev%20branch-development-blue.svg" alt="active_branch"/>
      </a>
    </td>
  </tr>
</table>



### License
All code, binaries and other assets in this repository are subject to [The Florin license](https://github.com/florinxfl/florin-core/blob/master/COPYING_florin) except where explicitely stated otherwise.

### Branches
`master` branch tracks the current public release; is generally updated only for each release (with accompanying tag) and very occasionally for minor documentation or other commits. If all you want is to build/track the current version of the software than use the `master` branch.

`development` branch tracks the current ongoing development that has not yet been released. Individual features may have their own feature branch from time to time.
Major features are worked on in temporary feature branches until they can be merged back into one of the development branches.

The currently most active branch, where most development that will go into the next major release is taking place is linked in the table at the top of this README as the `dev branch` and may be updated as development mandates seperate branches, for development changes you will generally want to work on this branch.

### Contributing
If you are thinking about contributing toward the development of Florin in some capacity whether small or large, code or translating; Please read [this guide](./CONTRIBUTING.md) first for information on how to do so.

### Technical documentation
* [Transaction format](./technical_documentation/transaction_format.md)
* [Account system](./technical_documentation/account_system.md)
* [Accelerated testnet](./technical_documentation/accelerated_testnet.md)
* [Official testnet](./technical_documentation/accelerated_testnet.md#official-testnet)


### Community
Connect with the community through one or more of the following:
[Website](https://florin.org) ◾ [Slack](https://Florinxfl.slack.com) ◾ [Twitter](http://twitter.com/florinxfl)


### Downloading
The latest binaries and installers can be found [here](https://github.com/florinxfl/florin-core/releases) for all platforms, including raspbian.

### Building
[Binaries](https://github.com/florinxfl/florin-core/releases) for both the UI as well as the daemon and command line interface for multiple architectures.

For those who really need too or are technically inclined it is of course possible to build the software yourself. Please read the [build instructions](./doc/building.md) before attempting to build the software and/or seeking support.

### Additional technical information
|Technical specifications|Main network|[Testnet](./technical_documentation/accelerated_testnet.md#official-testnet)|
|:-----------|:---------|:---------|
|Consensus algorithm:|PoW² SIGMA/Holding|PoW² SIGMA/Holding|
|Recommended transaction confirmations:|2|2|
|Block reward SIGMA:|0.10 XFL|0.10 XFL|
|Block reward holder:|0.10 XFL|0.10 XFL|
|Block interval target:|300 seconds (5 minutes)|Configurable|
|Difficulty adjustment period:|Every block|Every block|
|Difficulty adjustment algorithm:|Delta|Delta|
|Total coins to be minted over time:|210'000 XFL||
|P2P Port|9233|9235|
|RPC Port|9234|9236|
|P2P Network Header|fcfdfcfd|Configurable|
|Address version byte|53 (N)|65 (T)|
|P2SH version byte|113 (n)|127 (t)|
|BIP44 coin type|530 0x80000212|530 0x80000212|
|**Infrastructure**|**Main network**|**[Testnet](./technical_documentation/accelerated_testnet.md#official-testnet)**|
|Official block explorer|blockchain.florin.org|-|
|Community block explorer||-|
|DNS Seed 1|seed1.florin.org|-|
|DNS Seed 2|seed2.florin.org|-|
