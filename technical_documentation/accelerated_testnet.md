Accelerated testnet
======

Gulden implements a unique "accelerated testnet" system, that is used extensively by the developers for testing.
The system allows for the easy creation/management of multiple testnets, with different block target paramaters and different hash types; and even the ability to switch between them at will.

This allows for easy setup of a custom testnet to test specific scenarios.
Testnet paramaters are controlled by a unique 'testnet specifier' for each testnet. The code ensures that clients with different testnet paramaters will not interact with one another.

An example of such a specifier `-testnet=**C**1529510550:*10*` which can be broken down as follows
* *C*             Use city hash as the hash function
* *1529510550*    Use unix timestamp 1529510550 as the start date
*  *10*           Attempt to mine blocks every 10 seconds Block interval target

Current testnet paramaters that are supported (more may be added in future)
Hash Function:
* C               City hash; use this for CPU friendly testing.
* S               Scrypt hash; use this for testing mining pools or other funcitonality that requires the same properties as mainnet.
Block target:
* 1               Use this to de-activate block targetting; the testnet will allow you to simply mine blocks as fast as your CPU allows. This is good for testing scenarios that might take months in real world time in a matter of minutes instead.
* 2 or higher     Testnet will attempt to mine blocks at a frequency that matches your settings
Timestamp:
* There is no specific restriction on the timestamp other than that it should be reasonably recent, just grab the current timestamp with `date +%s` or similar and use this.

Start your own testnet
=======

To create a new testnet simply create a new testnet specifier from the options above picking the ones that suit your need.
You can then pass it to the program:
* `./Gulden -testnet=C1529510552:5`

Once in the program open the debug window and type the following (which will limit mining to 3000 hashes a second and start mining on a single core):
* `sethashlimit 3000`
* `setgenerate true 1`

Congratulations you have a testnet and are good for solo testing.
For multiuser testing just launch a second instance with the same `testnet=xxx` arguments and `-addnode=<youriphere>` and you can start testing.


Official testnet
======

Of course for those who just want to test something on an existing testnet with lots of other users we have that as well, complete with nocks testbed integation.
Just start with `-testnet=C1534687770:60 -addnode=devbak.net` to connect then head to our Slack to interact with other testnet users!
