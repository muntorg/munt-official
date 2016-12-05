// Copyright (c) 2012-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "addrman.h"
#include "test/test_bitcoin.h"
#include <string>
#include <boost/test/unit_test.hpp>

#include "hash.h"
#include "random.h"

using namespace std;

class CAddrManTest : public CAddrMan {
    uint64_t state;

public:
    CAddrManTest()
    {
        state = 1;
    }

    //! Ensure that bucket placement is always the same for testing purposes.
    void MakeDeterministic()
    {
        nKey.SetNull();
        seed_insecure_rand(true);
    }

    int RandomInt(int nMax)
    {
        state = (CHashWriter(SER_GETHASH, 0) << state).GetHash().GetCheapHash();
        return (unsigned int)(state % nMax);
    }

    CAddrInfo* Find(const CNetAddr& addr, int* pnId = NULL)
    {
        return CAddrMan::Find(addr, pnId);
    }

    CAddrInfo* Create(const CAddress& addr, const CNetAddr& addrSource, int* pnId = NULL)
    {
        return CAddrMan::Create(addr, addrSource, pnId);
    }

    void Delete(int nId)
    {
        CAddrMan::Delete(nId);
    }
};

BOOST_FIXTURE_TEST_SUITE(addrman_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(addrman_simple)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    CNetAddr source = CNetAddr("252.2.2.2");

    BOOST_CHECK(addrman.size() == 0);
    CAddrInfo addr_null = addrman.Select();
    BOOST_CHECK(addr_null.ToString() == "[::]:0");

    CService addr1 = CService("250.1.1.1", 9231);
    addrman.Add(CAddress(addr1, NODE_NONE), source);
    BOOST_CHECK(addrman.size() == 1);
    CAddrInfo addr_ret1 = addrman.Select();
    BOOST_CHECK(addr_ret1.ToString() == "250.1.1.1:9231");

    CService addr1_dup = CService("250.1.1.1", 9231);
    addrman.Add(CAddress(addr1_dup, NODE_NONE), source);
    BOOST_CHECK(addrman.size() == 1);

    CService addr2 = CService("250.1.1.2", 9231);
    addrman.Add(CAddress(addr2, NODE_NONE), source);
    BOOST_CHECK(addrman.size() == 2);

    addrman.Clear();
    BOOST_CHECK(addrman.size() == 0);
    CAddrInfo addr_null2 = addrman.Select();
    BOOST_CHECK(addr_null2.ToString() == "[::]:0");
}

BOOST_AUTO_TEST_CASE(addrman_ports)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    CNetAddr source = CNetAddr("252.2.2.2");

    BOOST_CHECK(addrman.size() == 0);

    CService addr1 = CService("250.1.1.1", 9231);
    addrman.Add(CAddress(addr1, NODE_NONE), source);
    BOOST_CHECK(addrman.size() == 1);

    CService addr1_port = CService("250.1.1.1", 8334);
    addrman.Add(CAddress(addr1_port, NODE_NONE), source);
    BOOST_CHECK(addrman.size() == 1);
    CAddrInfo addr_ret2 = addrman.Select();
    BOOST_CHECK(addr_ret2.ToString() == "250.1.1.1:9231");

    addrman.Good(CAddress(addr1_port, NODE_NONE));
    BOOST_CHECK(addrman.size() == 1);
    bool newOnly = true;
    CAddrInfo addr_ret3 = addrman.Select(newOnly);
    BOOST_CHECK(addr_ret3.ToString() == "250.1.1.1:9231");
}

BOOST_AUTO_TEST_CASE(addrman_select)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    CNetAddr source = CNetAddr("252.2.2.2");

    CService addr1 = CService("250.1.1.1", 9231);
    addrman.Add(CAddress(addr1, NODE_NONE), source);
    BOOST_CHECK(addrman.size() == 1);

    bool newOnly = true;
    CAddrInfo addr_ret1 = addrman.Select(newOnly);
    BOOST_CHECK(addr_ret1.ToString() == "250.1.1.1:9231");

    addrman.Good(CAddress(addr1, NODE_NONE));
    BOOST_CHECK(addrman.size() == 1);
    CAddrInfo addr_ret2 = addrman.Select(newOnly);
    BOOST_CHECK(addr_ret2.ToString() == "[::]:0");

    CAddrInfo addr_ret3 = addrman.Select();
    BOOST_CHECK(addr_ret3.ToString() == "250.1.1.1:9231");

    BOOST_CHECK(addrman.size() == 1);

    CService addr2 = CService("250.3.1.1", 9231);
    CService addr3 = CService("250.3.2.2", 9999);
    CService addr4 = CService("250.3.3.3", 9999);

    addrman.Add(CAddress(addr2, NODE_NONE), CService("250.3.1.1", 9231));
    addrman.Add(CAddress(addr3, NODE_NONE), CService("250.3.1.1", 9231));
    addrman.Add(CAddress(addr4, NODE_NONE), CService("250.4.1.1", 9231));

    CService addr5 = CService("250.4.4.4", 9231);
    CService addr6 = CService("250.4.5.5", 7777);
    CService addr7 = CService("250.4.6.6", 9231);

    addrman.Add(CAddress(addr5, NODE_NONE), CService("250.3.1.1", 9231));
    addrman.Good(CAddress(addr5, NODE_NONE));
    addrman.Add(CAddress(addr6, NODE_NONE), CService("250.3.1.1", 9231));
    addrman.Good(CAddress(addr6, NODE_NONE));
    addrman.Add(CAddress(addr7, NODE_NONE), CService("250.1.1.3", 9231));
    addrman.Good(CAddress(addr7, NODE_NONE));

    BOOST_CHECK(addrman.size() == 7);

    BOOST_CHECK(addrman.Select().ToString() == "250.4.6.6:9231");
    BOOST_CHECK(addrman.Select().ToString() == "250.3.2.2:9999");
    BOOST_CHECK(addrman.Select().ToString() == "250.3.3.3:9999");
    BOOST_CHECK(addrman.Select().ToString() == "250.4.4.4:9231");
}

BOOST_AUTO_TEST_CASE(addrman_new_collisions)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    CNetAddr source = CNetAddr("252.2.2.2");

    BOOST_CHECK(addrman.size() == 0);

    for (unsigned int i = 1; i < 18; i++) {
        CService addr = CService("250.1.1." + boost::to_string(i));
        addrman.Add(CAddress(addr, NODE_NONE), source);

        BOOST_CHECK(addrman.size() == i);
    }

    CService addr1 = CService("250.1.1.18");
    addrman.Add(CAddress(addr1, NODE_NONE), source);
    BOOST_CHECK(addrman.size() == 17);

    CService addr2 = CService("250.1.1.19");
    addrman.Add(CAddress(addr2, NODE_NONE), source);
    BOOST_CHECK(addrman.size() == 18);
}

BOOST_AUTO_TEST_CASE(addrman_tried_collisions)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    CNetAddr source = CNetAddr("252.2.2.2");

    BOOST_CHECK(addrman.size() == 0);

    for (unsigned int i = 1; i < 80; i++) {
        CService addr = CService("250.1.1." + boost::to_string(i));
        addrman.Add(CAddress(addr, NODE_NONE), source);
        addrman.Good(CAddress(addr, NODE_NONE));

        BOOST_TEST_MESSAGE(addrman.size());
        BOOST_CHECK(addrman.size() == i);
    }

    CService addr1 = CService("250.1.1.80");
    addrman.Add(CAddress(addr1, NODE_NONE), source);
    BOOST_CHECK(addrman.size() == 79);

    CService addr2 = CService("250.1.1.81");
    addrman.Add(CAddress(addr2, NODE_NONE), source);
    BOOST_CHECK(addrman.size() == 80);
}

BOOST_AUTO_TEST_CASE(addrman_find)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    BOOST_CHECK(addrman.size() == 0);

    CAddress addr1 = CAddress(CService("250.1.2.1", 9231), NODE_NONE);
    CAddress addr2 = CAddress(CService("250.1.2.1", 9999), NODE_NONE);
    CAddress addr3 = CAddress(CService("251.255.2.1", 9231), NODE_NONE);

    CNetAddr source1 = CNetAddr("250.1.2.1");
    CNetAddr source2 = CNetAddr("250.1.2.2");

    addrman.Add(addr1, source1);
    addrman.Add(addr2, source2);
    addrman.Add(addr3, source1);

    CAddrInfo* info1 = addrman.Find(addr1);
    BOOST_CHECK(info1);
    if (info1)
        BOOST_CHECK(info1->ToString() == "250.1.2.1:9231");

    CAddrInfo* info2 = addrman.Find(addr2);
    BOOST_CHECK(info2);
    if (info2)
        BOOST_CHECK(info2->ToString() == info1->ToString());

    CAddrInfo* info3 = addrman.Find(addr3);
    BOOST_CHECK(info3);
    if (info3)
        BOOST_CHECK(info3->ToString() == "251.255.2.1:9231");
}

BOOST_AUTO_TEST_CASE(addrman_create)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    BOOST_CHECK(addrman.size() == 0);

    CAddress addr1 = CAddress(CService("250.1.2.1", 9231), NODE_NONE);
    CNetAddr source1 = CNetAddr("250.1.2.1");

    int nId;
    CAddrInfo* pinfo = addrman.Create(addr1, source1, &nId);

    BOOST_CHECK(pinfo->ToString() == "250.1.2.1:9231");

    CAddrInfo* info2 = addrman.Find(addr1);
    BOOST_CHECK(info2->ToString() == "250.1.2.1:9231");
}

BOOST_AUTO_TEST_CASE(addrman_delete)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    BOOST_CHECK(addrman.size() == 0);

    CAddress addr1 = CAddress(CService("250.1.2.1", 9231), NODE_NONE);
    CNetAddr source1 = CNetAddr("250.1.2.1");

    int nId;
    addrman.Create(addr1, source1, &nId);

    BOOST_CHECK(addrman.size() == 1);
    addrman.Delete(nId);
    BOOST_CHECK(addrman.size() == 0);
    CAddrInfo* info2 = addrman.Find(addr1);
    BOOST_CHECK(info2 == NULL);
}

BOOST_AUTO_TEST_CASE(addrman_getaddr)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    BOOST_CHECK(addrman.size() == 0);
    vector<CAddress> vAddr1 = addrman.GetAddr();
    BOOST_CHECK(vAddr1.size() == 0);

    CAddress addr1 = CAddress(CService("250.250.2.1", 9231), NODE_NONE);
    addr1.nTime = GetAdjustedTime(); // Set time so isTerrible = false
    CAddress addr2 = CAddress(CService("250.251.2.2", 9999), NODE_NONE);
    addr2.nTime = GetAdjustedTime();
    CAddress addr3 = CAddress(CService("251.252.2.3", 9231), NODE_NONE);
    addr3.nTime = GetAdjustedTime();
    CAddress addr4 = CAddress(CService("252.253.3.4", 9231), NODE_NONE);
    addr4.nTime = GetAdjustedTime();
    CAddress addr5 = CAddress(CService("252.254.4.5", 9231), NODE_NONE);
    addr5.nTime = GetAdjustedTime();
    CNetAddr source1 = CNetAddr("250.1.2.1");
    CNetAddr source2 = CNetAddr("250.2.3.3");

    addrman.Add(addr1, source1);
    addrman.Add(addr2, source2);
    addrman.Add(addr3, source1);
    addrman.Add(addr4, source2);
    addrman.Add(addr5, source1);

    BOOST_CHECK(addrman.GetAddr().size() == 1);

    addrman.Good(CAddress(addr1, NODE_NONE));
    addrman.Good(CAddress(addr2, NODE_NONE));
    BOOST_CHECK(addrman.GetAddr().size() == 1);

    for (unsigned int i = 1; i < (8 * 256); i++) {
        int octet1 = i % 256;
        int octet2 = (i / 256) % 256;
        int octet3 = (i / (256 * 2)) % 256;
        string strAddr = boost::to_string(octet1) + "." + boost::to_string(octet2) + "." + boost::to_string(octet3) + ".23";
        CAddress addr = CAddress(CService(strAddr), NODE_NONE);

        addr.nTime = GetAdjustedTime();
        addrman.Add(addr, CNetAddr(strAddr));
        if (i % 8 == 0)
            addrman.Good(addr);
    }
    vector<CAddress> vAddr = addrman.GetAddr();

    size_t percent23 = (addrman.size() * 23) / 100;
    BOOST_CHECK(vAddr.size() == percent23);
    BOOST_CHECK(vAddr.size() == 461);

    BOOST_CHECK(addrman.size() == 2007);
}

BOOST_AUTO_TEST_CASE(caddrinfo_get_tried_bucket)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    CAddress addr1 = CAddress(CService("250.1.1.1", 9231), NODE_NONE);
    CAddress addr2 = CAddress(CService("250.1.1.1", 9999), NODE_NONE);

    CNetAddr source1 = CNetAddr("250.1.1.1");

    CAddrInfo info1 = CAddrInfo(addr1, source1);

    uint256 nKey1 = (uint256)(CHashWriter(SER_GETHASH, 0) << 1).GetHash();
    uint256 nKey2 = (uint256)(CHashWriter(SER_GETHASH, 0) << 2).GetHash();

    BOOST_CHECK(info1.GetTriedBucket(nKey1) == 40);

    BOOST_CHECK(info1.GetTriedBucket(nKey1) != info1.GetTriedBucket(nKey2));

    CAddrInfo info2 = CAddrInfo(addr2, source1);

    BOOST_CHECK(info1.GetKey() != info2.GetKey());
    BOOST_CHECK(info1.GetTriedBucket(nKey1) != info2.GetTriedBucket(nKey1));

    set<int> buckets;
    for (int i = 0; i < 255; i++) {
        CAddrInfo infoi = CAddrInfo(
            CAddress(CService("250.1.1." + boost::to_string(i)), NODE_NONE),
            CNetAddr("250.1.1." + boost::to_string(i)));
        int bucket = infoi.GetTriedBucket(nKey1);
        buckets.insert(bucket);
    }

    BOOST_CHECK(buckets.size() == 8);

    buckets.clear();
    for (int j = 0; j < 255; j++) {
        CAddrInfo infoj = CAddrInfo(
            CAddress(CService("250." + boost::to_string(j) + ".1.1"), NODE_NONE),
            CNetAddr("250." + boost::to_string(j) + ".1.1"));
        int bucket = infoj.GetTriedBucket(nKey1);
        buckets.insert(bucket);
    }

    BOOST_CHECK(buckets.size() == 160);
}

BOOST_AUTO_TEST_CASE(caddrinfo_get_new_bucket)
{
    CAddrManTest addrman;

    addrman.MakeDeterministic();

    CAddress addr1 = CAddress(CService("250.1.2.1", 9231), NODE_NONE);
    CAddress addr2 = CAddress(CService("250.1.2.1", 9999), NODE_NONE);

    CNetAddr source1 = CNetAddr("250.1.2.1");

    CAddrInfo info1 = CAddrInfo(addr1, source1);

    uint256 nKey1 = (uint256)(CHashWriter(SER_GETHASH, 0) << 1).GetHash();
    uint256 nKey2 = (uint256)(CHashWriter(SER_GETHASH, 0) << 2).GetHash();

    BOOST_CHECK(info1.GetNewBucket(nKey1) == 786);

    BOOST_CHECK(info1.GetNewBucket(nKey1) != info1.GetNewBucket(nKey2));

    CAddrInfo info2 = CAddrInfo(addr2, source1);
    BOOST_CHECK(info1.GetKey() != info2.GetKey());
    BOOST_CHECK(info1.GetNewBucket(nKey1) == info2.GetNewBucket(nKey1));

    set<int> buckets;
    for (int i = 0; i < 255; i++) {
        CAddrInfo infoi = CAddrInfo(
            CAddress(CService("250.1.1." + boost::to_string(i)), NODE_NONE),
            CNetAddr("250.1.1." + boost::to_string(i)));
        int bucket = infoi.GetNewBucket(nKey1);
        buckets.insert(bucket);
    }

    BOOST_CHECK(buckets.size() == 1);

    buckets.clear();
    for (int j = 0; j < 4 * 255; j++) {
        CAddrInfo infoj = CAddrInfo(CAddress(
                                        CService(
                                            boost::to_string(250 + (j / 255)) + "." + boost::to_string(j % 256) + ".1.1"),
                                        NODE_NONE),
                                    CNetAddr("251.4.1.1"));
        int bucket = infoj.GetNewBucket(nKey1);
        buckets.insert(bucket);
    }

    BOOST_CHECK(buckets.size() <= 64);

    buckets.clear();
    for (int p = 0; p < 255; p++) {
        CAddrInfo infoj = CAddrInfo(
            CAddress(CService("250.1.1.1"), NODE_NONE),
            CNetAddr("250." + boost::to_string(p) + ".1.1"));
        int bucket = infoj.GetNewBucket(nKey1);
        buckets.insert(bucket);
    }

    BOOST_CHECK(buckets.size() > 64);
}
BOOST_AUTO_TEST_SUITE_END()