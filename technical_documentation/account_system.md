Account system
======

The Gulden wallets implement a hierarchical deterministic account system in which the keys of each account are physically partitioned from those of other accounts within the wallet. This allows for the seperation of funds with the guarantee that funds from different accounts will not unintentionally get mixed to pay one anothers fees, via shared change 
or other wallet shortcomings or bugs that are prevalent in other cryptocurrencies that do not use such a system.

In addition to this basic account system the wallets also support various advanced capabilities, amongst which are:
* The ability to import/control multiple `seeds` (trees of accounts created from a phrase) within a single wallet
* The ability to share a single account as a read only account with another wallet; with the other wallet being able to generate future addresses that match ours
* The ability to share an entire seed as a read only seed with another wallet; with the other wallet being able to generate the same future accounts and addresses as ours
* The ability to link an account in a read/write way with our mobile wallets 

These capabilities allow for proper secure implementation of various services for Gulden - e.g. by setting up a merchant or website with a read only seed they can generate and receive payments without any risk of the funds being compromised if they server is compromised (as it contains a read only key)
For more on the account system please view the RPC help.


TODO: add more text here
