// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef BITCOIN_NET_H
#define BITCOIN_NET_H

#include "amount.h"
#include "bloom.h"
#include "compat.h"
#include "limitedmap.h"
#include "netbase.h"
#include "protocol.h"
#include "random.h"
#include "streams.h"
#include "sync.h"
#include "uint256.h"

#include <atomic>
#include <deque>
#include <stdint.h>

#ifndef WIN32
#include <arpa/inet.h>
#endif

#include <boost/filesystem/path.hpp>
#include <boost/foreach.hpp>
#include <boost/signals2/signal.hpp>

class CAddrMan;
class CScheduler;
class CNode;

namespace boost {
class thread_group;
} // namespace boost

/** Time between pings automatically sent out for latency probing and keepalive (in seconds). */
static const int PING_INTERVAL = 2 * 60;
/** Time after which to disconnect, after waiting for a ping response (or inactivity). */
static const int TIMEOUT_INTERVAL = 20 * 60;
/** Run the feeler connection loop once every 2 minutes or 120 seconds. **/
static const int FEELER_INTERVAL = 120;
/** The maximum number of entries in an 'inv' protocol message */
static const unsigned int MAX_INV_SZ = 50000;
/** The maximum number of new addresses to accumulate before announcing. */
static const unsigned int MAX_ADDR_TO_SEND = 1000;
/** Maximum length of incoming protocol messages (no message over 4 MB is currently acceptable). */
static const unsigned int MAX_PROTOCOL_MESSAGE_LENGTH = 4 * 1000 * 1000;
/** Maximum length of strSubVer in `version` message */
static const unsigned int MAX_SUBVERSION_LENGTH = 256;
/** -listen default */
static const bool DEFAULT_LISTEN = true;
/** -upnp default */
#ifdef USE_UPNP
static const bool DEFAULT_UPNP = USE_UPNP;
#else
static const bool DEFAULT_UPNP = false;
#endif
/** The maximum number of entries in mapAskFor */
static const size_t MAPASKFOR_MAX_SZ = MAX_INV_SZ;
/** The maximum number of entries in setAskFor (larger due to getdata latency)*/
static const size_t SETASKFOR_MAX_SZ = 2 * MAX_INV_SZ;
/** The maximum number of peer connections to maintain. */
static const unsigned int DEFAULT_MAX_PEER_CONNECTIONS = 125;
/** The default for -maxuploadtarget. 0 = Unlimited */
static const uint64_t DEFAULT_MAX_UPLOAD_TARGET = 0;
/** Default for blocks only*/
static const bool DEFAULT_BLOCKSONLY = false;

static const bool DEFAULT_FORCEDNSSEED = false;
static const size_t DEFAULT_MAXRECEIVEBUFFER = 5 * 1000;
static const size_t DEFAULT_MAXSENDBUFFER = 1 * 1000;

static const ServiceFlags REQUIRED_SERVICES = NODE_NETWORK;

static const unsigned int DEFAULT_MISBEHAVING_BANTIME = 60 * 60 * 24; // Default 24-hour ban

unsigned int ReceiveFloodSize();
unsigned int SendBufferSize();

typedef int NodeId;

void AddOneShot(const std::string& strDest);
void AddressCurrentlyConnected(const CService& addr);
CNode* FindNode(const CNetAddr& ip);
CNode* FindNode(const CSubNet& subNet);
CNode* FindNode(const std::string& addrName);
CNode* FindNode(const CService& ip);
CNode* FindNode(const NodeId id); //TODO: Remove this
bool OpenNetworkConnection(const CAddress& addrConnect, bool fCountFailure, CSemaphoreGrant* grantOutbound = NULL, const char* strDest = NULL, bool fOneShot = false, bool fFeeler = false);
void MapPort(bool fUseUPnP);
unsigned short GetListenPort();
bool BindListenPort(const CService& bindAddr, std::string& strError, bool fWhitelisted = false);
void StartNode(boost::thread_group& threadGroup, CScheduler& scheduler);
bool StopNode();
void SocketSendData(CNode* pnode);

struct CombinerAll {
    typedef bool result_type;

    template <typename I>
    bool operator()(I first, I last) const
    {
        while (first != last) {
            if (!(*first))
                return false;
            ++first;
        }
        return true;
    }
};

struct CNodeSignals {
    boost::signals2::signal<int()> GetHeight;
    boost::signals2::signal<bool(CNode*), CombinerAll> ProcessMessages;
    boost::signals2::signal<bool(CNode*), CombinerAll> SendMessages;
    boost::signals2::signal<void(NodeId, const CNode*)> InitializeNode;
    boost::signals2::signal<void(NodeId)> FinalizeNode;
};

CNodeSignals& GetNodeSignals();

enum {
    LOCAL_NONE, // unknown
    LOCAL_IF, // address a local interface listens on
    LOCAL_BIND, // address explicit bound to
    LOCAL_UPNP, // address reported by UPnP
    LOCAL_MANUAL, // address explicitly specified (-externalip=)

    LOCAL_MAX
};

bool IsPeerAddrLocalGood(CNode* pnode);
void AdvertiseLocal(CNode* pnode);
void SetLimited(enum Network net, bool fLimited = true);
bool IsLimited(enum Network net);
bool IsLimited(const CNetAddr& addr);
bool AddLocal(const CService& addr, int nScore = LOCAL_NONE);
bool AddLocal(const CNetAddr& addr, int nScore = LOCAL_NONE);
bool RemoveLocal(const CService& addr);
bool SeenLocal(const CService& addr);
bool IsLocal(const CService& addr);
bool GetLocal(CService& addr, const CNetAddr* paddrPeer = NULL);
bool IsReachable(enum Network net);
bool IsReachable(const CNetAddr& addr);
CAddress GetLocalAddress(const CNetAddr* paddrPeer = NULL);

extern bool fDiscover;
extern bool fListen;
extern ServiceFlags nLocalServices;
extern ServiceFlags nRelevantServices;
extern bool fRelayTxes;
extern uint64_t nLocalHostNonce;
extern CAddrMan addrman;

/** Maximum number of connections to simultaneously allow (aka connection slots) */
extern int nMaxConnections;

extern std::vector<CNode*> vNodes;
extern CCriticalSection cs_vNodes;
extern limitedmap<uint256, int64_t> mapAlreadyAskedFor;

extern std::vector<std::string> vAddedNodes;
extern CCriticalSection cs_vAddedNodes;

extern NodeId nLastNodeId;
extern CCriticalSection cs_nLastNodeId;

/** Subversion as sent to the P2P network in `version` messages */
extern std::string strSubVersion;

struct LocalServiceInfo {
    int nScore;
    int nPort;
};

extern CCriticalSection cs_mapLocalHost;
extern std::map<CNetAddr, LocalServiceInfo> mapLocalHost;
typedef std::map<std::string, uint64_t> mapMsgCmdSize; //command, total bytes

class CNodeStats {
public:
    NodeId nodeid;
    ServiceFlags nServices;
    bool fRelayTxes;
    int64_t nLastSend;
    int64_t nLastRecv;
    int64_t nTimeConnected;
    int64_t nTimeOffset;
    std::string addrName;
    int nVersion;
    std::string cleanSubVer;
    bool fInbound;
    int nStartingHeight;
    uint64_t nSendBytes;
    mapMsgCmdSize mapSendBytesPerMsgCmd;
    uint64_t nRecvBytes;
    mapMsgCmdSize mapRecvBytesPerMsgCmd;
    bool fWhitelisted;
    double dPingTime;
    double dPingWait;
    double dPingMin;
    std::string addrLocal;
};

class CNetMessage {
public:
    bool in_data; // parsing header (false) or data (true)

    CDataStream hdrbuf; // partially received header
    CMessageHeader hdr; // complete header
    unsigned int nHdrPos;

    CDataStream vRecv; // received message data
    unsigned int nDataPos;

    int64_t nTime; // time (in microseconds) of message receipt.

    CNetMessage(const CMessageHeader::MessageStartChars& pchMessageStartIn, int nTypeIn, int nVersionIn)
        : hdrbuf(nTypeIn, nVersionIn)
        , hdr(pchMessageStartIn)
        , vRecv(nTypeIn, nVersionIn)
    {
        hdrbuf.resize(24);
        in_data = false;
        nHdrPos = 0;
        nDataPos = 0;
        nTime = 0;
    }

    bool complete() const
    {
        if (!in_data)
            return false;
        return (hdr.nMessageSize == nDataPos);
    }

    void SetVersion(int nVersionIn)
    {
        hdrbuf.SetVersion(nVersionIn);
        vRecv.SetVersion(nVersionIn);
    }

    int readHeader(const char* pch, unsigned int nBytes);
    int readData(const char* pch, unsigned int nBytes);
};

typedef enum BanReason {
    BanReasonUnknown = 0,
    BanReasonNodeMisbehaving = 1,
    BanReasonManuallyAdded = 2
} BanReason;

class CBanEntry {
public:
    static const int CURRENT_VERSION = 1;
    int nVersion;
    int64_t nCreateTime;
    int64_t nBanUntil;
    uint8_t banReason;

    CBanEntry()
    {
        SetNull();
    }

    CBanEntry(int64_t nCreateTimeIn)
    {
        SetNull();
        nCreateTime = nCreateTimeIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nCreateTime);
        READWRITE(nBanUntil);
        READWRITE(banReason);
    }

    void SetNull()
    {
        nVersion = CBanEntry::CURRENT_VERSION;
        nCreateTime = 0;
        nBanUntil = 0;
        banReason = BanReasonUnknown;
    }

    std::string banReasonToString()
    {
        switch (banReason) {
        case BanReasonNodeMisbehaving:
            return "node misbehaving";
        case BanReasonManuallyAdded:
            return "manually added";
        default:
            return "unknown";
        }
    }
};

typedef std::map<CSubNet, CBanEntry> banmap_t;

/** Information about a peer */
class CNode {
public:
    ServiceFlags nServices;
    ServiceFlags nServicesExpected;
    SOCKET hSocket;
    CDataStream ssSend;
    size_t nSendSize; // total size of all vSendMsg entries
    size_t nSendOffset; // offset inside the first vSendMsg already sent
    uint64_t nSendBytes;
    std::deque<CSerializeData> vSendMsg;
    CCriticalSection cs_vSend;

    std::deque<CInv> vRecvGetData;
    std::deque<CNetMessage> vRecvMsg;
    CCriticalSection cs_vRecvMsg;
    uint64_t nRecvBytes;
    int nRecvVersion;

    int64_t nLastSend;
    int64_t nLastRecv;
    int64_t nTimeConnected;
    int64_t nTimeOffset;
    const CAddress addr;
    std::string addrName;
    CService addrLocal;
    int nVersion;

    std::string strSubVer, cleanSubVer;
    bool fWhitelisted; // This peer can bypass DoS banning.
    bool fFeeler; // If true this node is being used as a short lived feeler.
    bool fOneShot;
    bool fClient;
    bool fInbound;
    bool fNetworkNode;
    bool fSuccessfullyConnected;
    bool fDisconnect;

    bool fRelayTxes; //protected by cs_filter
    bool fSentAddr;
    CSemaphoreGrant grantOutbound;
    CCriticalSection cs_filter;
    CBloomFilter* pfilter;
    int nRefCount;
    NodeId id;

    const uint64_t nKeyedNetGroup;

protected:
    static banmap_t setBanned;
    static CCriticalSection cs_setBanned;
    static bool setBannedIsDirty;

    static std::vector<CSubNet> vWhitelistedRange;
    static CCriticalSection cs_vWhitelistedRange;

    mapMsgCmdSize mapSendBytesPerMsgCmd;
    mapMsgCmdSize mapRecvBytesPerMsgCmd;

    void Fuzz(int nChance); // modifies ssSend

public:
    uint256 hashContinue;
    int nStartingHeight;

    std::vector<CAddress> vAddrToSend;
    CRollingBloomFilter addrKnown;
    bool fGetAddr;
    std::set<uint256> setKnown;
    uint256 hashCheckpointKnown;
    int64_t nNextAddrSend;
    int64_t nNextLocalAddrSend;

    CRollingBloomFilter filterInventoryKnown;

    std::set<uint256> setInventoryTxToSend;

    std::vector<uint256> vInventoryBlockToSend;
    CCriticalSection cs_inventory;
    std::set<uint256> setAskFor;
    std::multimap<int64_t, CInv> mapAskFor;
    int64_t nNextInvSend;

    std::vector<uint256> vBlockHashesToAnnounce;

    bool fSendMempool;

    std::atomic<int64_t> timeLastMempoolReq;

    std::atomic<int64_t> nLastBlockTime;
    std::atomic<int64_t> nLastTXTime;

    uint64_t nPingNonceSent;

    int64_t nPingUsecStart;

    int64_t nPingUsecTime;

    int64_t nMinPingUsecTime;

    bool fPingQueued;

    CAmount minFeeFilter;
    CCriticalSection cs_feeFilter;
    CAmount lastSentFeeFilter;
    int64_t nextSendTimeFeeFilter;

    CNode(SOCKET hSocketIn, const CAddress& addrIn, const std::string& addrNameIn = "", bool fInboundIn = false);
    ~CNode();

private:
    static CCriticalSection cs_totalBytesRecv;
    static CCriticalSection cs_totalBytesSent;
    static uint64_t nTotalBytesRecv;
    static uint64_t nTotalBytesSent;

    static uint64_t nMaxOutboundTotalBytesSentInCycle;
    static uint64_t nMaxOutboundCycleStartTime;
    static uint64_t nMaxOutboundLimit;
    static uint64_t nMaxOutboundTimeframe;

    CNode(const CNode&);
    void operator=(const CNode&);

    static uint64_t CalculateKeyedNetGroup(const CAddress& ad);

public:
    NodeId GetId() const
    {
        return id;
    }

    int GetRefCount()
    {
        assert(nRefCount >= 0);
        return nRefCount;
    }

    unsigned int GetTotalRecvSize()
    {
        unsigned int total = 0;
        BOOST_FOREACH (const CNetMessage& msg, vRecvMsg)
            total += msg.vRecv.size() + 24;
        return total;
    }

    bool ReceiveMsgBytes(const char* pch, unsigned int nBytes);

    void SetRecvVersion(int nVersionIn)
    {
        nRecvVersion = nVersionIn;
        BOOST_FOREACH (CNetMessage& msg, vRecvMsg)
            msg.SetVersion(nVersionIn);
    }

    CNode* AddRef()
    {
        nRefCount++;
        return this;
    }

    void Release()
    {
        nRefCount--;
    }

    void AddAddressKnown(const CAddress& addr)
    {
        addrKnown.insert(addr.GetKey());
    }

    void PushAddress(const CAddress& addr)
    {

        if (addr.IsValid() && !addrKnown.contains(addr.GetKey())) {
            if (vAddrToSend.size() >= MAX_ADDR_TO_SEND) {
                vAddrToSend[insecure_rand() % vAddrToSend.size()] = addr;
            } else {
                vAddrToSend.push_back(addr);
            }
        }
    }

    void AddInventoryKnown(const CInv& inv)
    {
        {
            LOCK(cs_inventory);
            filterInventoryKnown.insert(inv.hash);
        }
    }

    void PushInventory(const CInv& inv)
    {
        LOCK(cs_inventory);
        if (inv.type == MSG_TX) {
            if (!filterInventoryKnown.contains(inv.hash)) {
                setInventoryTxToSend.insert(inv.hash);
            }
        } else if (inv.type == MSG_BLOCK) {
            vInventoryBlockToSend.push_back(inv.hash);
        }
    }

    void PushBlockHash(const uint256& hash)
    {
        LOCK(cs_inventory);
        vBlockHashesToAnnounce.push_back(hash);
    }

    void AskFor(const CInv& inv);

    void BeginMessage(const char* pszCommand) EXCLUSIVE_LOCK_FUNCTION(cs_vSend);

    void AbortMessage() UNLOCK_FUNCTION(cs_vSend);

    void EndMessage(const char* pszCommand) UNLOCK_FUNCTION(cs_vSend);

    void PushVersion();

    void PushMessage(const char* pszCommand)
    {
        try {
            BeginMessage(pszCommand);
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1>
    void PushMessage(const char* pszCommand, const T1& a1)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1;
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    /** Send a message containing a1, serialized with flag flag. */
    template <typename T1>
    void PushMessageWithFlag(int flag, const char* pszCommand, const T1& a1)
    {
        try {
            BeginMessage(pszCommand);
            WithOrVersion(&ssSend, flag) << a1;
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2;
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3;
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4;
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5;
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6;
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7;
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8;
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8, const T9& a9)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9;
            EndMessage(pszCommand);
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }

    void CloseSocketDisconnect();

    static void ClearBanned(); // needed for unit testing
    static bool IsBanned(CNetAddr ip);
    static bool IsBanned(CSubNet subnet);
    static void Ban(const CNetAddr& ip, const BanReason& banReason, int64_t bantimeoffset = 0, bool sinceUnixEpoch = false);
    static void Ban(const CSubNet& subNet, const BanReason& banReason, int64_t bantimeoffset = 0, bool sinceUnixEpoch = false);
    static bool Unban(const CNetAddr& ip);
    static bool Unban(const CSubNet& ip);
    static void GetBanned(banmap_t& banmap);
    static void SetBanned(const banmap_t& banmap);

    static bool BannedSetIsDirty();

    static void SetBannedSetDirty(bool dirty = true);

    static void SweepBanned();

    void copyStats(CNodeStats& stats);

    static bool IsWhitelistedRange(const CNetAddr& ip);
    static void AddWhitelistedRange(const CSubNet& subnet);

    static void RecordBytesRecv(uint64_t bytes);
    static void RecordBytesSent(uint64_t bytes);

    static uint64_t GetTotalBytesRecv();
    static uint64_t GetTotalBytesSent();

    static void SetMaxOutboundTarget(uint64_t limit);
    static uint64_t GetMaxOutboundTarget();

    static void SetMaxOutboundTimeframe(uint64_t timeframe);
    static uint64_t GetMaxOutboundTimeframe();

    static bool OutboundTargetReached(bool historicalBlockServingLimit);

    static uint64_t GetOutboundTargetBytesLeft();

    static uint64_t GetMaxOutboundTimeLeftInCycle();
};

class CTransaction;
void RelayTransaction(const CTransaction& tx);

/** Access to the (IP) address database (peers.dat) */
class CAddrDB {
private:
    boost::filesystem::path pathAddr;

public:
    CAddrDB();
    bool Write(const CAddrMan& addr);
    bool Read(CAddrMan& addr);
    bool Read(CAddrMan& addr, CDataStream& ssPeers);
};

/** Access to the banlist database (banlist.dat) */
class CBanDB {
private:
    boost::filesystem::path pathBanlist;

public:
    CBanDB();
    bool Write(const banmap_t& banSet);
    bool Read(banmap_t& banSet);
};

/** Return a timestamp in the future (in microseconds) for exponentially distributed events. */
int64_t PoissonNextSend(int64_t nNow, int average_interval_seconds);

struct AddedNodeInfo {
    std::string strAddedNode;
    CService resolvedAddress;
    bool fConnected;
    bool fInbound;
};

std::vector<AddedNodeInfo> GetAddedNodeInfo();

#endif // BITCOIN_NET_H
