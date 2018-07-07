PoW² activation
======
PoW² will activate in five phases, we are currently in Phase 1 of the activation.


Phase 1
-----
During `phase 1` everything is as it was in previous releases. It is possible to `create` witness accounts but not to do anything with them.
During this phase the focus is on getting miners to upgrade, as PoW² cannot activate properly until this happens.

On `July 12th 2018 - 16::00 UTC` upgraded miners will be able to begin signalling their upgrade status in the blocks they mine.
Every 2016 blocks (roughly 3.5 days) the quantity of upgraded blocks is measured and once a sufficient percentage signal upgrade `Phase 2` will become "locked in"; after this another 2016 blocks window period is allowed before `Phase 2` becomes fully activated.

Note that this is not an opportunity for miners to 'vote'; but only a mechanism to ensure miners have time to upgrade for a smooth upgrade. That the community wants PoW² to activate is abundantly clear and the developers will under no circumstances allow a minority of people to attempt to prevent this. In the unlikely and unfortunate event that after a reasonable period of time miners still refuse to upgrade or otherwise attempt to abuse the situation to override the will of the community alternative activation steps can and will be taken to ensure that `phase 2` activates.

Interested users can monitor this process via `getblockchainfo` RPC command.


Phase 2
-----
Once `phase 2` activates, non-upgraded miners will no longer be able to mine on the network, the network otherwise remains backwards compatible for users of older wallet software etc.
During `phase 2` users can start to `fund` witness accounts, but other than locking the funds these accounts will not yet be able to do anything. This is to prevent witnessing from activating before their are a sufficient number of witnesses are present on the network.
It is important that once you have funded your witness accounts that you keep your wallet open from that point onwards (or export your witness key to a suitable always on witness device - e.g. g-dash on a pi)
When at least `300` accounts have been funded and a combined witnessing weight of over `20000000` is present on the network the network will immediately transition into `Phase 3`

Interested users users can monitor this process via `getwitnessinfo` RPC command.


Phase 3
-----
Once `phase 3` activates, initial PoW² witnessing is officially 'live' on the network, at this point in the process it operates in a special backwards compatible manner which allows older mobile wallets, 1.6.x desktop wallets and exchanges the capability of continuing to function without the need for any upgrades.
While basic witnessing functionality is in place certain features e.g. `compounding` are not available during this phase.

A month after the initial `phase 3` activation, signalling for `phase 4` will begin. Witnesses will automatically signal for `phase 4` in the blocks they witness based on whether the majority of their peers are identified as being upgraded to 2.0 or not.
This is to ensure that we don't activate `phase 4` until most users are on compatible software, for the smoothest possible experience.
As we near `phase 4` activation it will be time for services like exchanges to upgrade and the Gulden developers will monitor and assist to ensure that this takes place.

Interested users can monitor this process via `getblockchaininfo` RPC command.


Phase 4
-----
The activation of `phase 4` allows for our new transaction format `SegSig` to go live. This transaction format is not backwards compatible so at this point all non-upgraded wallets will be disconnected from the network and require an upgrade to connect again.
Witnessing will immediately take advantage of the new transaction format which will allow for various additional witnessing functionality to become active:
* Compounding of earnings
* Extension of witness period and balance in a locked account
* Splitting of a single locked account into multiple locked accounts (This is only useful/necessary for users with large witnessing balances who wish to optimise their earnings)
* Merging previously split locked accounts back into one
* Rotation of witness key for a new one if the old one has been compromised; or as a periodic precaution

As witnesses witness, their accounts will automatically be upgraded from the old backwards compatible format to the new transaction format.
Once all witnesses are upgraded `Phase 5` will activate automatically.

Interested users users can monitor this process via `getwitnessinfo` RPC command.


Phase 5
-----
Phase 5 signals that the network is now in its final, completely upgraded state.
At this point the developers will be able to put out a new release that removes a lot of the special code that was necessary for `phases 1-4` which will help improve wallet performance as well as being good for the quality of the codebase.
The developers will also be able to turn off automated-checkpointing after this has happened.
