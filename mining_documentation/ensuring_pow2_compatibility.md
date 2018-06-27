The basics
=========

PoW² activates in several phases.

* At this moment the network is in what we call `Phase 1`
* `Phase 2` is activated by miner upgrades - and allows people to start creating witnessing addresses; there is a roughly 2 week delay before this is allowed to commence and then an indeterminate but hopefully short period of time before it actually occurs.
* `Phase 3` is activated by creation of enough witness addresses, and is the point at which witnessing first kicks in. Any miner/pool who has not followed the upgrade steps will be isolated from the chain and all earnings orphaned.
* `Phase 4` is set to activate at soonest a month after phase 3, and involved the activation of amongst other things a new transaction format. Any miner/pool who has not follwed the upgrade steps will again be isolated from the chain.

It is therefore in the interests of all mining pool operators to read and follow this guide. And further to either join our slack, or to subscribe to this repo and carefully watch for news over the next month.
Note that this guide is currently only for `phase 3` upgrade; information on `phase 4` upgrade will only become available later. Please *do not* ask for this information now, the developers will release a similar guide to this for `phase 4` when the time is right.

Special thanks to Gregory de Graaf for his patience in helping to test these steps.

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
a076a165-3bb8-4745-adbc-d27cfb48029b
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit2"
acc009a0-604c-453d-81d1-64ab5a2a83ea
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit3"
64cb416a-673f-4d09-b089-baae5768cf8c
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit4"
0306880a-2542-4eee-ae15-5976f9ff4dc1
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit5"
5be64366-db07-41a3-8283-cabd91f2b094
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit6"
a50c39c2-afbe-4072-9d72-ac2a3039b5d1
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit7"
8867e340-841b-4907-af7d-33fd562f3a3f
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit8"
712f8edf-703c-4ebe-b796-7ee7b31d2727
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit9"
cc3df1d5-e35f-4eb4-b6a5-671328f632c0
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit10"
62965b3e-b42d-4d88-b9c1-37c302b4d95c
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit11"
ef2ae4ea-5be3-4260-9025-33573b7152ad
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit12"
367b6434-f576-4d82-b7c1-23e805a3a5a3
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit13"
fb66eef5-9ad8-4060-b6c2-edd345384456
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit14"
9b22cde7-1b70-4a69-8810-da50b61fb382
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit15"
517d5035-b1f7-4367-b3de-526dbb1bbbfb
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit16"
680da8ea-dc03-47e0-85fd-d7b88d5ad8e7
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit17"
f6fbd27f-2334-4b44-a1ce-12cc9df6e236
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit18"
2efeb416-71e0-4f68-9b8a-8500f36fcdfc
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit19"
b830077e-c413-4e03-9869-bb94bafa2d25
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit20"
685bcd24-7f6b-4748-9ac9-d8dece5b26b9
# ./src/Gulden-cli -testnet=C1529916460:3 -rpcuser=a -rpcpassword=a -rpcport=8003 createwitnessaccount "wit21"
ee6f9598-a998-48d9-bb03-235fbd87e91b
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
* Wipe the data directories and start two GuldenD instance again
* Make a testnet copy of your mining pool
* Check the debug.log for the magic header bytes (they are logged near the start) and update your testnet pool with the information as well as other testnet information from above (rpc port etc.)
* Some small changes need to be made to your pool software - see [This sample](https://github.com/UNOMP/node-merged-pool/compare/master...mjmacleod:master) to get an idea of what needs to be done. Note this ignores other Gulden specific customisations that might be required - these are only the PoW² specific changes.
* Start mining with the pool
* Follow the same basic steps again ont he witness node to create witnesses and make sure that the pool continues to mine blocks well into phase 3

If you run into issues, add debug logging (some sample log calls in the patchset above - just uncomment them) and then seek assistance.
The Gulden developers will be available for pool related questions after the 2.0 launch.

Advanced
--------

Final advanced testing should be done if you want to ensure 100% pool performance.
* Setup everything as before
* This time launch three `GuldenD` witnesses instead of 2
* Split your witness accounts evenly between 2 of the 3 nodes
* Advance to `phase 3` as before
* During `phase 3` close on of the 2 instances that has witness accounts
* At this point you should start to see some orphans in your log but the chain should keep moving 
* If the chain keeps moving everything is 100%; if the chain freezes when you do this then you need further configuration adjustments and should seek support.
