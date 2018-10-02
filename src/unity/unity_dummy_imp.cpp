#include "unity_impl.h"
// Djinni generated files
#include "gulden_unified_backend.hpp"
#include "gulden_unified_frontend.hpp"
#include "qrcode_record.hpp"
#include "balance_record.hpp"
#include "uri_record.hpp"
#include "uri_recipient.hpp"
#include "transaction_record.hpp"
#include "transaction_type.hpp"
//#include "djinni_support.hpp"

#include <boost/chrono.hpp>

#include <algorithm>
#include <iostream>
#include <vector>

int32_t GuldenUnifiedBackend::InitUnityLib(const std::string & data_dir, const std::shared_ptr<GuldenUnifiedFrontend> & signals) {
    
    std::vector<int> ns{1, 2, 3, 4, 5};
    for (auto n: ns) std::cout << n << ", ";
    std::cout << '\n';
    std::for_each_n(ns.begin(), 3, [](auto& n){ n *= 2; });
    for (auto n: ns) std::cout << n << ", ";
    std::cout << '\n';
    
    auto p = std::make_pair("hop", "hap");
    int i = 2;
    int a = i * i;
    return a;
}
void GuldenUnifiedBackend::TerminateUnityLib() {}
QrcodeRecord GuldenUnifiedBackend::QRImageFromString(const std::string & qr_string, int32_t width_hint) {
    return QrcodeRecord(0, std::vector<uint8_t>());
}
std::string GuldenUnifiedBackend::GetReceiveAddress() { return ""; }
std::string GuldenUnifiedBackend::GetRecoveryPhrase() { return ""; }
UriRecipient GuldenUnifiedBackend::IsValidRecipient(const UriRecord & request) { return     UriRecipient(false, "", "", ""); }
    bool performPaymentToRecipient(const UriRecipient & request) { return false; }
    std::vector<TransactionRecord> GuldenUnifiedBackend::getTransactionHistory() { return std::vector<TransactionRecord>(); }
bool GuldenUnifiedBackend::performPaymentToRecipient(const UriRecipient & request) { return false; }

