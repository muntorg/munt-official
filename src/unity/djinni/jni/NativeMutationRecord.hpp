// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#pragma once

#include "djinni_support.hpp"
#include "mutation_record.hpp"

namespace djinni_generated {

class NativeMutationRecord final {
public:
    using CppType = ::MutationRecord;
    using JniType = jobject;

    using Boxed = NativeMutationRecord;

    ~NativeMutationRecord();

    static CppType toCpp(JNIEnv* jniEnv, JniType j);
    static ::djinni::LocalRef<JniType> fromCpp(JNIEnv* jniEnv, const CppType& c);

private:
    NativeMutationRecord();
    friend ::djinni::JniClass<NativeMutationRecord>;

    const ::djinni::GlobalRef<jclass> clazz { ::djinni::jniFindClass("com/novo/jniunifiedbackend/MutationRecord") };
    const jmethodID jconstructor { ::djinni::jniGetMethodID(clazz.get(), "<init>", "(JJLjava/lang/String;Ljava/lang/String;Lcom/novo/jniunifiedbackend/TransactionStatus;I)V") };
    const jfieldID field_mChange { ::djinni::jniGetFieldID(clazz.get(), "mChange", "J") };
    const jfieldID field_mTimestamp { ::djinni::jniGetFieldID(clazz.get(), "mTimestamp", "J") };
    const jfieldID field_mTxHash { ::djinni::jniGetFieldID(clazz.get(), "mTxHash", "Ljava/lang/String;") };
    const jfieldID field_mRecipientAddresses { ::djinni::jniGetFieldID(clazz.get(), "mRecipientAddresses", "Ljava/lang/String;") };
    const jfieldID field_mStatus { ::djinni::jniGetFieldID(clazz.get(), "mStatus", "Lcom/novo/jniunifiedbackend/TransactionStatus;") };
    const jfieldID field_mDepth { ::djinni::jniGetFieldID(clazz.get(), "mDepth", "I") };
};

}  // namespace djinni_generated
