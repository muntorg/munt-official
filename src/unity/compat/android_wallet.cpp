// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "android_wallet.h"
#include "util.h"


#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.pb.h>
#include <crypto/scrypt/crypto_scrypt.h>
#include <crypto/aes.h>

#include <codecvt>
#include <locale>

#include <boost/scope_exit.hpp>



using namespace google::protobuf;


// The bitcoinj authors opted to split the utf16 password characters into bytes as opposed to doing a utf8 conversion or similar
// This replicates that behaviour.
std::vector<uint8_t> convertToJavaByteArray(const std::u16string& u16_string)
{
    std::vector<uint8_t> ret;
    ret.resize(u16_string.length()*2);
    for(uint32_t i = 0; i < u16_string.length(); i++)
    {
        int bytePosition = i << 1;
        ret[bytePosition] = uint8_t((u16_string[i]&0xFF00) >> 8);
        ret[bytePosition + 1] = uint8_t(u16_string[i]&0x00FF);
    }
    return ret;
}

std::string codec_type = "EmptyMessage";
android_wallet ParseAndroidProtoWallet(std::string walletPath, std::string walletPassword)
{
    android_wallet walletRet;

    //Parse the file with a blank/empty proto
    DescriptorPool pool;
    FileDescriptorProto file;
    file.set_name("empty_message.proto");
    file.add_message_type()->set_name(codec_type);
    pool.BuildFile(file);
    const Descriptor* type = pool.FindMessageTypeByName(codec_type);

    DynamicMessageFactory dynamic_factory(&pool);
    std::unique_ptr<Message> message(dynamic_factory.GetPrototype(type)->New());
    std::unique_ptr<Message> nestedMessage(dynamic_factory.GetPrototype(type)->New());

    int fd = open(walletPath.c_str(), O_RDONLY);
    if (fd<=0)
    {
        walletRet.resultMessage = "Failed to open input file.";
        walletRet.fileOpenError = true;
        return walletRet;
    }
    // Parse binary input
    io::FileInputStream in(fd);
    BOOST_SCOPE_EXIT(&fd) { close(fd); } BOOST_SCOPE_EXIT_END

    if (!message->ParsePartialFromZeroCopyStream(&in))
    {
        walletRet.resultMessage = "Failed to parse input.";
        return walletRet;
    }

    if (!message->IsInitialized())
    {
        walletRet.resultMessage = "Failed to parse input, missing required fields.";
        return walletRet;
    }

    auto fieldSet = message->GetReflection()->MutableUnknownFields(message.get());

    walletRet.numWalletFields = fieldSet->field_count();

    //Variables for dealing with password hashing
    std::string scryptSalt;
    int64_t scryptN=16383;
    int32_t scryptR=8;
    int32_t scryptP=1;
    std::string aesIV;
    std::string aesCryptedData;

    // Navigate the field tree
    for (int i=0;i<fieldSet->field_count();++i)
    {
        auto field = fieldSet->field(i);
        switch (field.number())
        {
            case 1:
            {
                if (field.type() == 3)
                {
                    // All valid wallet files should have this field.
                    if (field.length_delimited() == "com.gulden.production") { walletRet.validWalletProto = true; }
                }
                break;
            }
            case 6:
            {
                if (!nestedMessage->ParsePartialFromString(field.length_delimited()))
                {
                    walletRet.resultMessage = "Failed to parse input for nested scrypt data.";
                    return walletRet;
                }
                auto nestedFieldSet =  nestedMessage->GetReflection()->MutableUnknownFields(nestedMessage.get());
                for (int j=0;j<nestedFieldSet->field_count();++j)
                {
                    auto nestedField = nestedFieldSet->field(j);
                    switch (nestedField.number())
                    {
                        case 1: scryptSalt = nestedField.length_delimited(); break;
                        case 2: scryptN = nestedField.varint(); break;
                        case 3: scryptR = nestedField.varint(); break;
                        case 4: scryptP = nestedField.varint(); break;
                        default: walletRet.resultMessage = "Unknown encyption paramater."; return walletRet;
                    }
                }
                break;
            }
            case 3:
            {
                if (!nestedMessage->ParsePartialFromString(field.length_delimited()))
                {
                    walletRet.resultMessage = "Failed to parse input for nested key.";
                    return walletRet;
                }
                auto nestedFieldSet =  nestedMessage->GetReflection()->MutableUnknownFields(nestedMessage.get());
                int keyType = -1;
                for (int j=0;j<nestedFieldSet->field_count();++j)
                {
                    auto nestedField = nestedFieldSet->field(j);
                    if (keyType < 0)
                    {
                        if (nestedField.number() == 1 && nestedField.type() == 0)
                        {
                            keyType = nestedField.varint();
                            switch (keyType)
                            {
                                case 3: break;
                                case 5: break;
                                case 4: LogPrintf("ParseAndroidProtoWallet: Skipping HD key\n"); break;
                                default: LogPrintf("ParseAndroidProtoWallet: Unhandled key type [%d]\n", keyType); break;
                            }
                        }
                    }
                    else 
                    {
                        switch (keyType)
                        {
                            case 3:
                            case 5:
                            {
                                if (nestedField.type()==3)
                                {
                                    switch (nestedField.number())
                                    {
                                        case 2:
                                        {
                                            walletRet.walletSeedMnemonic = nestedField.length_delimited();
                                            break;
                                        }
                                        case 6:
                                        {
                                            walletRet.encrypted = true;

                                            std::unique_ptr<Message> aesMessage(dynamic_factory.GetPrototype(type)->New());
                                            if (!aesMessage->ParsePartialFromString(nestedField.length_delimited()))
                                            {
                                                walletRet.resultMessage = "Failed to parse input for encrypted key.";
                                                return walletRet;
                                            }
                                            auto aesFieldSet = aesMessage->GetReflection()->MutableUnknownFields(aesMessage.get());
                                            for (int k=0;k<aesFieldSet->field_count();++k)
                                            {
                                                auto aesField = aesFieldSet->field(k);
                                                switch (aesField.number())
                                                {
                                                    case 1: aesIV = aesField.length_delimited(); break;
                                                    case 2: aesCryptedData = aesField.length_delimited(); break;
                                                    default: walletRet.resultMessage = "Invalid encryption data for encrypted key."; return walletRet;
                                                }
                                            }
                                            break;
                                        }
                                    }
                                }
                                else if (nestedField.number() == 5 && nestedField.type() == 0)
                                {
                                    walletRet.walletBirth = nestedField.varint();
                                }
                                break;
                            }
                            case 4: break;
                            default: LogPrintf("ParseAndroidProtoWallet: Unhandled field         [%d] [%d]\n", nestedField.number(), nestedField.type()); break;
                        }
                    }
                }
                break;
            }
            default: LogPrintf("ParseAndroidProtoWallet: Unhandled field [%d] [%d]\n", field.number(), field.type()); break;
        }
    }

    if (!scryptSalt.empty())
    {
        // Generate aes key data from password using the provided scrypt paramaters.
        std::vector<uint8_t> aesKeyData;
        aesKeyData.resize(32);
        std::vector<uint8_t> passwordData = convertToJavaByteArray(std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(walletPassword));
        crypto_scrypt((uint8_t*)&passwordData[0], passwordData.size(), (uint8_t*)scryptSalt.c_str(), scryptSalt.length(), scryptN, scryptR, scryptP, &aesKeyData[0], 32);

        // Combine the AES key with the IV and the encrypted data to retrieve the original unencrypted data
        AES256CBCDecrypt dec(&aesKeyData[0], (const unsigned char*)&aesIV[0], true);
        int nLen = aesCryptedData.size(); // plaintext will always be equal to or lesser than length of ciphertext
        walletRet.walletSeedMnemonic.resize(nLen);
        nLen = dec.Decrypt((const unsigned char*)&aesCryptedData[0], aesCryptedData.size(), (unsigned char*)&walletRet.walletSeedMnemonic[0]);
        if (nLen == 0)
        {
            walletRet.resultMessage = "Decryption failed.";
            return walletRet;
        }
        walletRet.walletSeedMnemonic.resize(nLen);
    }
    if (!walletRet.walletSeedMnemonic.empty())
    {
        walletRet.validWallet = true;
    }

    return walletRet;
}
