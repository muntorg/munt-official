// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#pragma once

#include "djinni_support.hpp"
#include "i_accounts_listener.hpp"

namespace djinni_generated {

class NativeIAccountsListener final : ::djinni::JniInterface<::IAccountsListener, NativeIAccountsListener> {
public:
    using CppType = std::shared_ptr<::IAccountsListener>;
    using CppOptType = std::shared_ptr<::IAccountsListener>;
    using JniType = jobject;

    using Boxed = NativeIAccountsListener;

    ~NativeIAccountsListener();

    static CppType toCpp(JNIEnv* jniEnv, JniType j) { return ::djinni::JniClass<NativeIAccountsListener>::get()._fromJava(jniEnv, j); }
    static ::djinni::LocalRef<JniType> fromCppOpt(JNIEnv* jniEnv, const CppOptType& c) { return {jniEnv, ::djinni::JniClass<NativeIAccountsListener>::get()._toJava(jniEnv, c)}; }
    static ::djinni::LocalRef<JniType> fromCpp(JNIEnv* jniEnv, const CppType& c) { return fromCppOpt(jniEnv, c); }

private:
    NativeIAccountsListener();
    friend ::djinni::JniClass<NativeIAccountsListener>;
    friend ::djinni::JniInterface<::IAccountsListener, NativeIAccountsListener>;

    class JavaProxy final : ::djinni::JavaProxyHandle<JavaProxy>, public ::IAccountsListener
    {
    public:
        JavaProxy(JniType j);
        ~JavaProxy();

        void onActiveAccountChanged(const std::string & accountUUID) override;
        void onActiveAccountNameChanged(const std::string & newAccountName) override;
        void onAccountNameChanged(const std::string & accountUUID, const std::string & newAccountName) override;
        void onAccountAdded(const std::string & accountUUID, const std::string & accountName) override;
        void onAccountDeleted(const std::string & accountUUID) override;
        void onAccountModified(const std::string & accountUUID, const ::AccountRecord & accountData) override;

    private:
        friend ::djinni::JniInterface<::IAccountsListener, ::djinni_generated::NativeIAccountsListener>;
    };

    const ::djinni::GlobalRef<jclass> clazz { ::djinni::jniFindClass("unity_wallet/jniunifiedbackend/IAccountsListener") };
    const jmethodID method_onActiveAccountChanged { ::djinni::jniGetMethodID(clazz.get(), "onActiveAccountChanged", "(Ljava/lang/String;)V") };
    const jmethodID method_onActiveAccountNameChanged { ::djinni::jniGetMethodID(clazz.get(), "onActiveAccountNameChanged", "(Ljava/lang/String;)V") };
    const jmethodID method_onAccountNameChanged { ::djinni::jniGetMethodID(clazz.get(), "onAccountNameChanged", "(Ljava/lang/String;Ljava/lang/String;)V") };
    const jmethodID method_onAccountAdded { ::djinni::jniGetMethodID(clazz.get(), "onAccountAdded", "(Ljava/lang/String;Ljava/lang/String;)V") };
    const jmethodID method_onAccountDeleted { ::djinni::jniGetMethodID(clazz.get(), "onAccountDeleted", "(Ljava/lang/String;)V") };
    const jmethodID method_onAccountModified { ::djinni::jniGetMethodID(clazz.get(), "onAccountModified", "(Ljava/lang/String;Lunity_wallet/jniunifiedbackend/AccountRecord;)V") };
};

}  // namespace djinni_generated
