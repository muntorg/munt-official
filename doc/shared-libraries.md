Shared Libraries
================

## guldenconsensus

The purpose of this library is to make the verification functionality that is critical to Gulden's consensus available to other applications, e.g. to language bindings.

### API

The interface is defined in the C header `guldenconsensus.h` located in  `src/script/guldenconsensus.h`.

#### Version

`guldenconsensus_version` returns an `unsigned int` with the API version *(currently at an experimental `0`)*.

#### Script Validation

`guldenconsensus_verify_script` returns an `int` with the status of the verification. It will be `1` if the input script correctly spends the previous output `scriptPubKey`.

##### Parameters
- `const unsigned char *scriptPubKey` - The previous output script that encumbers spending.
- `unsigned int scriptPubKeyLen` - The number of bytes for the `scriptPubKey`.
- `const unsigned char *txTo` - The transaction with the input that is spending the previous output.
- `unsigned int txToLen` - The number of bytes for the `txTo`.
- `unsigned int nIn` - The index of the input in `txTo` that spends the `scriptPubKey`.
- `unsigned int flags` - The script validation flags *(see below)*.
- `guldenconsensus_error* err` - Will have the error/success code for the operation *(see below)*.

##### Script Flags
- `guldenconsensus_SCRIPT_FLAGS_VERIFY_NONE`
- `guldenconsensus_SCRIPT_FLAGS_VERIFY_P2SH` - Evaluate P2SH ([BIP16](https://github.com/gulden/bips/blob/master/bip-0016.mediawiki)) subscripts
- `guldenconsensus_SCRIPT_FLAGS_VERIFY_DERSIG` - Enforce strict DER ([BIP66](https://github.com/gulden/bips/blob/master/bip-0066.mediawiki)) compliance

##### Errors
- `guldenconsensus_ERR_OK` - No errors with input parameters *(see the return value of `guldenconsensus_verify_script` for the verification status)*
- `guldenconsensus_ERR_TX_INDEX` - An invalid index for `txTo`
- `guldenconsensus_ERR_TX_SIZE_MISMATCH` - `txToLen` did not match with the size of `txTo`
- `guldenconsensus_ERR_DESERIALIZE` - An error deserializing `txTo`

### Example Implementations
- [NGulden](https://github.com/NicolasDorier/NGulden/blob/master/NGulden/Script.cs#L814) (.NET Bindings)
- [node-libguldenconsensus](https://github.com/bitpay/node-libguldenconsensus) (Node.js Bindings)
- [java-libguldenconsensus](https://github.com/dexX7/java-libguldenconsensus) (Java Bindings)
- [guldenconsensus-php](https://github.com/Bit-Wasp/guldenconsensus-php) (PHP Bindings)
