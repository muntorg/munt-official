Version 1 transaction format
======
The original Gulden transaction format is not overly interesting, it is based on [the Bitcoin transaction format](https://en.bitcoin.it/wiki/Transaction) (before the introduction of "SegWit") which is well documented elsewhere so there is no need to go into detail on it here.
For our version 2.0 release we have implemented a completely new transaction format which replaces the original transaction format as well as fixing transaction malleability in a different way to "SegWit".


Version 2 transaction format
======
The heart of the V1 transaction format is a powerful script system that allows for all sorts of interesting uses of a blockchain other than just plain money transfers.
This script system makes transactions very versatile and almost anything can be done with them even regular plain `address1` -> `address2` payments are implemented using a script.

On very many levels this is nice however with scaling being one of the key obstacles we face the following has become obvious:
* The majority of payments are just plain `address1` -> `address2` payments with no need for fancy scripting capability
* The scripting capability is adding overhead to these transactions
* The transaction format is otherwise not as optimal as it could be

An important concept is that of "Users should only pay for the features they actually use". And so with this in mind we set out to upgrade the transaction format with the following objectives:
* Users should only pay for the features they need
* Should be as compact as possible
* Should be as fast to process as possible
* Should maintain the advanced scripting capabilities that the V1 format has.
* Should be easy to extend in future for even more advanced functionality


Check back for more technical information, this documentation will be updated over the coming weeks.
