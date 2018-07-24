The basics
=========

PoW² activates in several phases.

* At this moment the network is in what we call `Phase 3`
* `Phase 3` is activated by creation of enough witness addresses, and is the point at which witnessing first kicks in. Any miner/pool who has not followed the upgrade steps will be isolated from the chain and all earnings orphaned.
* `Phase 4` is set to activate at soonest a month, likely later, after phase 3, and involves the activation of amongst other things a new transaction format. Any miner/pool who has not follwed the upgrade steps will again be isolated from the chain.

It is therefore in the interests of all mining pool operators to read and follow this guide. And further to either join our slack, or to subscribe to this repo and carefully watch for news over the next month.
Note that this guide is currently only for `phase 3` upgrade; information on `phase 4` upgrade will only become available later. Please *do not* ask for this information now, the developers will release a similar guide to this for `phase 4` when the time is right.

Special thanks to Gregory de Graaf for his patience in helping to test these steps.


Sample changes for various pool software, note that these changes are applied and tested by a few pools, but the developers cannot guarantee they will work for you the onus is on you the pool operator to test them. Should you run into issues while testing we will happily assist but we cannot test on your behalf.

* [unomp](https://github.com/UNOMP/node-merged-pool/compare/master...mjmacleod:master)
* [yiimp](https://github.com/tpruvot/yiimp/compare/next...mjmacleod:next)
* [slush0/stratum_mining](https://gist.github.com/mjmacleod/4437340f8e75c44fedcd5db540c5a51c)
* [p2pool](https://github.com/0x0aNL/p2pool-0x0a/blob/PoW2/p2pool/bitcoin/data.p
> y#L175)

Phase 3 upgrade guide
============

Basic sanity test
--------

The first step to upgrading is to do a basic `GuldenD` only sanity test to familiarise yourself with the commands and ensure everything works as expected.
* Download and install a PoW² capable `GuldenD` (details in #mining on our slack)
* Set up two basic GuldenD instances communicating with one another, one for mining the other for witnessing
```
# mkdir miner && mkdir witness
# ./GuldenD -testnet=C1529916460:3 -port=8000 -listen=1 -rpcuser=a -rpcpassword=a -rpcport=8002 -daemon -datadir=miner                         
Gulden server starting
# ./GuldenD -testnet=C1529916460:3 -port=8001 -listen=1 -addnode=127.0.0.1:8000 -rpcuser=a -rpcpassword=a -rpcport=8003 -daemon -datadir=witness
Gulden server starting
```
* Start mining
```
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8002 setgenerate true 
Mining enabled into account [My account], thread limit: [1].
```
* Wait a while and then verify that phase 2 has activated
```
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 getblockcount
30
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 getwitnessinfo
[
  {
    "pow2_phase": 1,
    "number_of_witnesses_raw": 0,
    "number_of_witnesses_total": 0,
    "number_of_witnesses_eligible": 0,
    "total_witness_weight_raw": 0,
    "total_witness_weight_eligible_raw": 0,
    "total_witness_weight_eligible_adjusted": 0,
    "selected_witness_address": ""
  }
]
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 getblockcount
72
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 getwitnessinfo
[
  {
    "pow2_phase": 2,
    "number_of_witnesses_raw": 0,
    "number_of_witnesses_total": 0,
    "number_of_witnesses_eligible": 0,
    "total_witness_weight_raw": 0,
    "total_witness_weight_eligible_raw": 0,
    "total_witness_weight_eligible_adjusted": 0,
    "selected_witness_address": ""
  }
]
```
* Get an address from the witness
```
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 getnewaddress
TBpkoSkUPpVtdREUBcuDXMVRaYrSh9FTYU
```
* And send it some funds from the miner
```
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8002 sendtoaddress "TBpkoSkUPpVtdREUBcuDXMVRaYrSh9FTYU" 30000000
c2f3e0a8988bcae3a8004ea66e346e247f1388b3c72ce06f5b35f182f1db0d57
```
* Now create and fund the witness accounts
```
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit1"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit2"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit3"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit4"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit5"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit6"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit7"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit8"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit9"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit10"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit11"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit12"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit13"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit14"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit15"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit16"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit17"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit18"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit19"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit20"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit21"
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit1" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit2" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit3" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit4" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit5" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit6" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit7" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit8" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit9" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit10" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit11" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit12" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit13" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit14" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit15" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit16" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit17" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit18" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit19" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit20" 50000 3y
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 fundwitnessaccount "My account" "wit21" 50000 3y
```
* Wait a while and check that `phase 3` has activated and the chain is still moving.
```
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8002 getwitnessinfo
[
  {
    "pow2_phase": 3,
    "number_of_witnesses_raw": 21,
    "number_of_witnesses_total": 21,
    "number_of_witnesses_eligible": 9,
    "total_witness_weight_raw": 4313991,
    "total_witness_weight_eligible_raw": 4313991,
    "total_witness_weight_eligible_adjusted": 223698,
    "selected_witness_address": "2akSa7oc6HqsVnmo57xooP7HPxZmboL2z7JqRtCtVvL6yZjXejUTeEB2AmskES"
  }
]
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 getwitnessinfo
[
  {
    "pow2_phase": 3,
    "number_of_witnesses_raw": 21,
    "number_of_witnesses_total": 21,
    "number_of_witnesses_eligible": 13,
    "total_witness_weight_raw": 4313991,
    "total_witness_weight_eligible_raw": 4313991,
    "total_witness_weight_eligible_adjusted": 423873,
    "selected_witness_address": "2aTQMniwyiiyHBRiriQ63GJ6UPQ8UH9RfdHTzyrMQssBQ64cKZj7bSg2w4rFJw"
  }
]
```

The process is now complete.


Actual pool upgrade
--------

Now that we have done the upgrade without a pool the process with a pool is very similar.
Note that cpu miner software is sufficient for this, while you can use an asic for testing it is not necessary...

Note that for this part of the instructions you will want to use `Scrypt` for your testnet and not `City hash` so `-testnet=S1529916460:3` and not `-testnet=C1529916460:3`
* Wipe the data directories and start two GuldenD instance again
* Make a testnet copy of your mining pool
* Check the debug.log for the magic header bytes (they are logged near the start) and update your testnet pool with the information as well as other testnet information from above (rpc port etc.)
* Some small changes need to be made to your pool software
See some sample changes [unomp](https://github.com/UNOMP/node-merged-pool/compare/master...mjmacleod:master) [yiimp](https://github.com/tpruvot/yiimp/compare/next...mjmacleod:next) [slush0/stratum_mining](https://gist.github.com/mjmacleod/4437340f8e75c44fedcd5db540c5a51c) [p2pool](https://github.com/0x0aNL/p2pool-0x0a/blob/PoW2/p2pool/bitcoin/data.py#L175)

Note the onus is on you the pool operator to properly test these changes, we cannot guarantee they work for you without further customisation.
Note the above samples ignore other Gulden specific customisations that might be required and already done on your pool - these are only the PoW² specific changes.

* Start mining with the pool
* Follow the same basic steps again on the witness node to create witnesses and make sure that the pool continues to mine blocks well into phase 3

If you run into issues, add debug logging (some sample log calls in the patchset above - just uncomment them) and then seek assistance.
The Gulden developers are available on our slack for all pool related questions.

Advanced
--------

Final advanced testing should be done if you want to ensure 100% pool performance.
* Setup everything as before
* This time launch three `GuldenD` witnesses instead of 2
* Split your witness accounts evenly between 2 of the 3 nodes
* Advance to `phase 3` as before
* During `phase 3` close one of the 2 instances that has witness accounts
* At this point you should start to see some orphans in your log but the chain should keep moving 
* If the chain keeps moving everything is 100%; if the chain freezes when you do this then you need further configuration adjustments and should seek support.
