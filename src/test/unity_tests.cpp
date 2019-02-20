#include "unity/compat/android_wallet.h"

#include "clientversion.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "utilstrencodings.h"
#include "utilmoneystr.h"
#include "test/test_gulden.h"

#include <stdint.h>
#include <vector>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(util_tests, BasicTestingSetup)

bool retrieveWallet(std::string sFile, std::string sPassword, std::string phraseCheck)
{
    android_wallet wallet = ParseAndroidProtoWallet(sFile, sPassword);
    if (!wallet.validWalletProto || ! wallet.validWallet)
        return false;
    if (sPassword.length() > 0 && !wallet.encrypted)
        return false;

    return wallet.walletSeedMnemonic == phraseCheck;
}

bool retrieveEncryptedWalletWithoutPassword(std::string sFile, std::string sPassword, std::string phraseCheck)
{
    android_wallet wallet = ParseAndroidProtoWallet(sFile, sPassword);
    if (!wallet.validWalletProto)
        return false;
    if (wallet.encrypted && !wallet.validWallet)
        return true;

    return false;
}

bool retrieveEncryptedWalletWithWrongPassword(std::string sFile, std::string sPassword, std::string phraseCheck)
{
    android_wallet wallet = ParseAndroidProtoWallet(sFile, sPassword);
    if (!wallet.validWalletProto)
        return false;
    if (wallet.encrypted && !wallet.validWallet)
        return true;

    return false;
}

BOOST_AUTO_TEST_CASE(unity_android_wallet_import)
{
    BOOST_CHECK(retrieveWallet(TESTDATADIR"wallet-seed-password-1234-protobuf","1234","umbrella dune genuine busy whip core famous pattern impulse solid nice film"));
    BOOST_CHECK(retrieveWallet(TESTDATADIR"wallet-seed-no-password-protobuf","","umbrella dune genuine busy whip core famous pattern impulse solid nice film"));

    BOOST_CHECK(retrieveWallet(TESTDATADIR"wallet-linked-password-5281-protobuf","5281","EZv3Mzbf2XnGNZ1a8RUXhpuA6KKEmQh57Goqb3o7VBgy-F8zKT5BzCjcMyoQApTvu6jEtViUuGiCQnVBhFjXzeYbj:3mM4jYg7L4FhLC"));
    BOOST_CHECK(retrieveWallet(TESTDATADIR"wallet-linked-no-password-protobuf","","EZv3Mzbf2XnGNZ1a8RUXhpuA6KKEmQh57Goqb3o7VBgy-F8zKT5BzCjcMyoQApTvu6jEtViUuGiCQnVBhFjXzeYbj:3mM4jYg7L4FhLC"));

    BOOST_CHECK(retrieveEncryptedWalletWithoutPassword(TESTDATADIR"wallet-seed-password-1234-protobuf","","umbrella dune genuine busy whip core famous pattern impulse solid nice film"));
    BOOST_CHECK(retrieveEncryptedWalletWithWrongPassword(TESTDATADIR"wallet-seed-password-1234-protobuf","4321","umbrella dune genuine busy whip core famous pattern impulse solid nice film"));
}



BOOST_AUTO_TEST_SUITE_END()
