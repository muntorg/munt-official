Best practices for pools who want to mine Gulden
========

Gulden is a unique digital currency that differs from other bitcoin derived currencies in many quite substantial ways. If you want to mine Gulden it is best to become familiar with these in order to maximise your earnings and reduce problems for both yourself and us.
Do not expect Gulden to be another 'clone coin' that you can just drop in place and forget about, if you do you will be disappointed when things don't work for you properly.

PoW²
--------

We are currently in PoW² phase 3 - as long as this phase remains active it is necessary to have special pool changes in place in order to mine us.
Failiure to implement these changes will result in all blocks being orphaned, so make sure to follow them properly.
If you struggle to follow them ask for assistance.

[PoW² upgrade for mining pools](./ensuring_pow2_compatibility.md)

Latest version
--------

Gulden is cutting edge software that is rapidly developing, as such it is important to keep up with events as much as possible.
* Try to keep up with the latest version
* Keep in touch with the dev team for assistance, ideally on our slack, alternatively give us your contact details
* Try to follow our news to be aware of changes in advance

We also currently have a small team, we use the latest tools to improve our efficiency and quality but we do not have an endless amount of developer time and resources so we have to prioritise.
Most of our users do not even know what a compiler is, they have no interest in compiling and pleasing these users is where the bulk of our priorities lie.
This does not mean that we don't want others to be able to compile our software, we do, but it means that if you want to compile our software you need to be realistic, there will be breakages from time to time especially around major releases, we will deal with these but it takes time. If you cannot handle this then use the [binaries](https://github.com/Gulden/gulden-official/releases) we provide instead it is far simpler. 

Do:
* If possible use the [binaries](https://github.com/Gulden/gulden-official/releases]) this is the path of least resistance
* Leave github issues explaining the problem you face compiling with as much detail (config.log) as possible
* Be patient

Don't:
* Start making demands or insulting the dev team because you have issues compiling, this is not helpful for anyone.

limitdeltadiffdrop
--------

For optimal mining, it is important to set `limitdeltadiffdrop=n` in your Gulden.conf - if you do not do this a default setting will be chosen for you that might be non-optimal for your pool size.

If you are a large pool - likely to at times have 20% of network weight or more:
* Set `limitdeltadiffdrop=0`

If you are a medium sized pool - unlikely to ever exceed 20% of network weight: 
* Set one of `limitdeltadiffdrop=1` `limitdeltadiffdrop=2` ... `limitdeltadiffdrop=6` depending on size

If you are a smaller pool:
* Set one of `limitdeltadiffdrop=7` ... `limitdeltadiffdrop=30`

If you are a very small pool or small solo miner:
* Set `limitdeltadiffdrop=30` or larger

Time
-------

The gulden network is a lot less lenient than other networks when it comes to timestamps.

Do:
* Make sure you have `ntpd` running in your system regularly to keep your time accurate
* Make sure that your pool calls `getblocktemplate` frequently to update the timestamp
* Make sure that your pool uses the timestamp returned by getblocktemplate and doesn't try to fiddle with timestamps, doing so will reduce your earnings via increased orphan rates and worse network block times.

Don't:
* Try to fiddle with timestamps to try trick difficulty - you will only lose money this way

Address or coinbase
-------

It is best for everyone involved if it is possible to identify which blocks are mined by you.

Why it is good for you:
* Free advertising to miners via our block explorers
* Get notified when something is wrong with the blocks you are mining instead of losing money

Why it is good for us:
* Easier to ensure our network is healthy
* Can contact you when something is wrong and sort problems out

What to do:
* Add a [coinbase tag](https://github.com/zone117x/node-stratum-pool/blob/4649a557dad29d4446941741b0d77da4f6ec5fd5/lib/transactions.js#L253) to your mining pool
* If for some reason you can't add a coinbase tag use a static address
* Let us know that you are mining us
* [Submit a pull request to the list](https://github.com/Gulden/gulden-official/blob/master/mining_documentation/list_of_mining_pools.md)


Other things you should know
--------

Gulden uses a unique system PoW² which involves all blocks you mine being signed by a witness, each block has one and only one witness who can sign it.
What this means for you:
* When your pool mines a block e.g. block 800000 it cannot proceed to mine block 800001 not until 800000 is witnessed, so instead it will attempt to mine another block 800000 sometimes it will find two. This is not a bug but by design and in your interests to do as there is no guarantee your first block 800000 will get accepted.
* This also means that you can't expect to jump in with 10 times the network hashrate and find 3 blocks in super quick succession - you'll get lots of orphans this way. To minimise orphans it is better to have more stable hash and/or to use hash that is in line with the network hash and not vastly in excess of it.

Gulden is 100% resistant to selfish mining.
What this means for you:
* When your pool mines a block you should broadcast it immediately, delaying the broadcast will not assist you in finding more blocks it will just increase your orphan rate.
* You should attempt to have your GuldenD connect to multiple peers, or even host multiple GuldenD instances to which your pools GuldenD then connects; in order to broadcast your blocks to the network as fast as possible, the more peers it reaches and faster it does so the less orphans you will have.
