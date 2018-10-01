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

int32_t GuldenUnifiedBackend::InitUnityLib(const std::string & data_dir, const std::shared_ptr<GuldenUnifiedFrontend> & signals) {
    
    int i = 2;
    int a = i * i;
    return a;
}
void GuldenUnifiedBackend::TerminateUnityLib() {}
QrcodeRecord GuldenUnifiedBackend::QRImageFromString(const std::string & qr_string, int32_t width_hint) {}
std::string GuldenUnifiedBackend::GetReceiveAddress() {}
std::string GuldenUnifiedBackend::GetRecoveryPhrase() {}
UriRecipient GuldenUnifiedBackend::IsValidRecipient(const UriRecord & request) {}
bool performPaymentToRecipient(const UriRecipient & request) {}
std::vector<TransactionRecord> GuldenUnifiedBackend::getTransactionHistory() {}
bool GuldenUnifiedBackend::performPaymentToRecipient(const UriRecipient & request) {}

