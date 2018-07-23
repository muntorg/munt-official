Best practices for pools who want to mine Gulden
========

Gulden is a unique digital currency that differs from other bitcoin derived currencies in several ways. If you want to mine Gulden it is best to become familiar with these in order to maximise your earnings and reduce 

PoW²
--------

We are currently in PoW² phase 3 - as long as this phase remains active it is necessary to have special pool changes in place in order to mine us.
Failiure to implement these changes will result in all blocks being orphaned, so make sure to follow them properly.
If you struggle to follow them ask for assistance.

[PoW² upgrade for mining pools](./mining_documentation/ensuring_pow2_compatibility.md)

Latest version
--------

Gulden is cutting edge software that is rapidly developing, as such it is important to keep up with events as much as possible.
* Try to keep up with the latest version
* Keep in touch with the dev team for assistance, ideally on our slack, alternatively give us your contact details
* Try to follow our news to be aware of changes in advance

limitdeltadiffdrop
--------

For optimal mining, it is important to set `limitdeltadiffdrop=n` in your Gulden.conf - if you do not do this you risk having more orphans and in some special cases having your pool crash due to your miners dos'ing you with low difficulty blocks.

If you are a large pool - likely to at times have 20% of network weight or more:
* Set `limitdeltadiffdrop=0`

If you are a medium sized pool - unlikely to ever exceed 20% of network weight: 
* Set one of `limitdeltadiffdrop=1` `limitdeltadiffdrop=2` ... `limitdeltadiffdrop=6` depending on size

If you are a smaller pool:
* Set one of `limitdeltadiffdrop=7` ... `limitdeltadiffdrop=30`

If you are a very small pool or small solo miner:
* Do not set this option at all just leave it out

Time
-------

The gulden network is a lot less lenient than other networks when it comes to timestamps.

Do:
* Make sure you have `ntpd` running in your system regularly to keep your time accurate
* Make sure that your pool calls `getblocktemplate` frequently to update the timestamp
* Make sure that your pool uses the timestamp returned by getblocktemplate and doesn't try 

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
* Add a coinbase tag to your mining pool
* If for some reason you can't add a coinbase tag use a static address
* Let us know that you are mining us
* (Submit a pull request to the list)[https://github.com/Gulden/gulden-official/blob/master/mining_documentation/list_of_mining_pools.md]


Other things you shold know
--------

Gulden uses a unique system PoW² which involves all blocks you mine being signed by a witness, each block has one and only one witness who can sign it.
What this means for you:
* When your pool mines a block e.g. block 800000 it cannot proceed to mine block 800001 not until 800000 is witnessed, so instead it will attempt to mine another block 800000 sometimes it will find two. This is not a bug but by design and in your interests to do as there is no guarantee your first block 800000 will get accepted.
* This also means that you can't expect to jump in with 10 times the network hashrate and find 3 blocks in super quick succession - you'll get lots of orphans this way. To minimise orphans it is better to have more stable hash and/or to use hash that is in line with the network hash and not vastly in excess of it.
