// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#include "NJSIAccountsListener.hpp"
using namespace std;

void NJSIAccountsListener::onActiveAccountChanged_aimpl__(const std::string & accountUUID)
{
    const auto& env = Env();
    Napi::HandleScope scope(env);
    //Wrap parameters
    std::vector<napi_value> args;
    auto arg_0 = Napi::String::New(env, accountUUID);
    args.push_back(arg_0);
    Napi::Value calling_function_as_value = Value().Get("onActiveAccountChanged");
    if(!calling_function_as_value.IsUndefined() && !calling_function_as_value.IsNull())
    {
        Napi::Function calling_function = calling_function_as_value.As<Napi::Function>();
        auto result_onActiveAccountChanged = calling_function.Call(args);
        if(result_onActiveAccountChanged.IsEmpty())
        {
            Napi::Error::New(env, "NJSIAccountsListener::onActiveAccountChanged call failed").ThrowAsJavaScriptException();
            return;
        }
    }
}

void NJSIAccountsListener::onActiveAccountChanged(const std::string & accountUUID)
{
    uv_work_t* request = new uv_work_t;
    request->data = new std::tuple<NJSIAccountsListener*, std::string>(this, accountUUID);

    uv_queue_work(uv_default_loop(), request, [](uv_work_t*) -> void{}, [](uv_work_t* req, int status) -> void
    {
        NJSIAccountsListener* pthis = std::get<0>(*((std::tuple<NJSIAccountsListener*, std::string>*)req->data));
        pthis->onActiveAccountChanged_aimpl__(std::get<1>(*((std::tuple<NJSIAccountsListener*, std::string>*)req->data)));
        delete (std::tuple<NJSIAccountsListener*, std::string>*)req->data;
        req->data = nullptr;
    }
    );
}

void NJSIAccountsListener::onActiveAccountNameChanged_aimpl__(const std::string & newAccountName)
{
    const auto& env = Env();
    Napi::HandleScope scope(env);
    //Wrap parameters
    std::vector<napi_value> args;
    auto arg_0 = Napi::String::New(env, newAccountName);
    args.push_back(arg_0);
    Napi::Value calling_function_as_value = Value().Get("onActiveAccountNameChanged");
    if(!calling_function_as_value.IsUndefined() && !calling_function_as_value.IsNull())
    {
        Napi::Function calling_function = calling_function_as_value.As<Napi::Function>();
        auto result_onActiveAccountNameChanged = calling_function.Call(args);
        if(result_onActiveAccountNameChanged.IsEmpty())
        {
            Napi::Error::New(env, "NJSIAccountsListener::onActiveAccountNameChanged call failed").ThrowAsJavaScriptException();
            return;
        }
    }
}

void NJSIAccountsListener::onActiveAccountNameChanged(const std::string & newAccountName)
{
    uv_work_t* request = new uv_work_t;
    request->data = new std::tuple<NJSIAccountsListener*, std::string>(this, newAccountName);

    uv_queue_work(uv_default_loop(), request, [](uv_work_t*) -> void{}, [](uv_work_t* req, int status) -> void
    {
        NJSIAccountsListener* pthis = std::get<0>(*((std::tuple<NJSIAccountsListener*, std::string>*)req->data));
        pthis->onActiveAccountNameChanged_aimpl__(std::get<1>(*((std::tuple<NJSIAccountsListener*, std::string>*)req->data)));
        delete (std::tuple<NJSIAccountsListener*, std::string>*)req->data;
        req->data = nullptr;
    }
    );
}

void NJSIAccountsListener::onAccountNameChanged_aimpl__(const std::string & accountUUID, const std::string & newAccountName)
{
    const auto& env = Env();
    Napi::HandleScope scope(env);
    //Wrap parameters
    std::vector<napi_value> args;
    auto arg_0 = Napi::String::New(env, accountUUID);
    args.push_back(arg_0);
    auto arg_1 = Napi::String::New(env, newAccountName);
    args.push_back(arg_1);
    Napi::Value calling_function_as_value = Value().Get("onAccountNameChanged");
    if(!calling_function_as_value.IsUndefined() && !calling_function_as_value.IsNull())
    {
        Napi::Function calling_function = calling_function_as_value.As<Napi::Function>();
        auto result_onAccountNameChanged = calling_function.Call(args);
        if(result_onAccountNameChanged.IsEmpty())
        {
            Napi::Error::New(env, "NJSIAccountsListener::onAccountNameChanged call failed").ThrowAsJavaScriptException();
            return;
        }
    }
}

void NJSIAccountsListener::onAccountNameChanged(const std::string & accountUUID, const std::string & newAccountName)
{
    uv_work_t* request = new uv_work_t;
    request->data = new std::tuple<NJSIAccountsListener*, std::string, std::string>(this, accountUUID, newAccountName);

    uv_queue_work(uv_default_loop(), request, [](uv_work_t*) -> void{}, [](uv_work_t* req, int status) -> void
    {
        NJSIAccountsListener* pthis = std::get<0>(*((std::tuple<NJSIAccountsListener*, std::string, std::string>*)req->data));
        pthis->onAccountNameChanged_aimpl__(std::get<1>(*((std::tuple<NJSIAccountsListener*, std::string, std::string>*)req->data)), std::get<2>(*((std::tuple<NJSIAccountsListener*, std::string, std::string>*)req->data)));
        delete (std::tuple<NJSIAccountsListener*, std::string, std::string>*)req->data;
        req->data = nullptr;
    }
    );
}

void NJSIAccountsListener::onAccountAdded_aimpl__(const std::string & accountUUID, const std::string & accountName)
{
    const auto& env = Env();
    Napi::HandleScope scope(env);
    //Wrap parameters
    std::vector<napi_value> args;
    auto arg_0 = Napi::String::New(env, accountUUID);
    args.push_back(arg_0);
    auto arg_1 = Napi::String::New(env, accountName);
    args.push_back(arg_1);
    Napi::Value calling_function_as_value = Value().Get("onAccountAdded");
    if(!calling_function_as_value.IsUndefined() && !calling_function_as_value.IsNull())
    {
        Napi::Function calling_function = calling_function_as_value.As<Napi::Function>();
        auto result_onAccountAdded = calling_function.Call(args);
        if(result_onAccountAdded.IsEmpty())
        {
            Napi::Error::New(env, "NJSIAccountsListener::onAccountAdded call failed").ThrowAsJavaScriptException();
            return;
        }
    }
}

void NJSIAccountsListener::onAccountAdded(const std::string & accountUUID, const std::string & accountName)
{
    uv_work_t* request = new uv_work_t;
    request->data = new std::tuple<NJSIAccountsListener*, std::string, std::string>(this, accountUUID, accountName);

    uv_queue_work(uv_default_loop(), request, [](uv_work_t*) -> void{}, [](uv_work_t* req, int status) -> void
    {
        NJSIAccountsListener* pthis = std::get<0>(*((std::tuple<NJSIAccountsListener*, std::string, std::string>*)req->data));
        pthis->onAccountAdded_aimpl__(std::get<1>(*((std::tuple<NJSIAccountsListener*, std::string, std::string>*)req->data)), std::get<2>(*((std::tuple<NJSIAccountsListener*, std::string, std::string>*)req->data)));
        delete (std::tuple<NJSIAccountsListener*, std::string, std::string>*)req->data;
        req->data = nullptr;
    }
    );
}

void NJSIAccountsListener::onAccountDeleted_aimpl__(const std::string & accountUUID)
{
    const auto& env = Env();
    Napi::HandleScope scope(env);
    //Wrap parameters
    std::vector<napi_value> args;
    auto arg_0 = Napi::String::New(env, accountUUID);
    args.push_back(arg_0);
    Napi::Value calling_function_as_value = Value().Get("onAccountDeleted");
    if(!calling_function_as_value.IsUndefined() && !calling_function_as_value.IsNull())
    {
        Napi::Function calling_function = calling_function_as_value.As<Napi::Function>();
        auto result_onAccountDeleted = calling_function.Call(args);
        if(result_onAccountDeleted.IsEmpty())
        {
            Napi::Error::New(env, "NJSIAccountsListener::onAccountDeleted call failed").ThrowAsJavaScriptException();
            return;
        }
    }
}

void NJSIAccountsListener::onAccountDeleted(const std::string & accountUUID)
{
    uv_work_t* request = new uv_work_t;
    request->data = new std::tuple<NJSIAccountsListener*, std::string>(this, accountUUID);

    uv_queue_work(uv_default_loop(), request, [](uv_work_t*) -> void{}, [](uv_work_t* req, int status) -> void
    {
        NJSIAccountsListener* pthis = std::get<0>(*((std::tuple<NJSIAccountsListener*, std::string>*)req->data));
        pthis->onAccountDeleted_aimpl__(std::get<1>(*((std::tuple<NJSIAccountsListener*, std::string>*)req->data)));
        delete (std::tuple<NJSIAccountsListener*, std::string>*)req->data;
        req->data = nullptr;
    }
    );
}

void NJSIAccountsListener::onAccountModified_aimpl__(const std::string & accountUUID, const AccountRecord & accountData)
{
    const auto& env = Env();
    Napi::HandleScope scope(env);
    //Wrap parameters
    std::vector<napi_value> args;
    auto arg_0 = Napi::String::New(env, accountUUID);
    args.push_back(arg_0);
    auto arg_1 = Napi::Object::New(env);
    auto arg_1_1 = Napi::String::New(env, accountData.UUID);
    arg_1.Set("UUID", arg_1_1);
    auto arg_1_2 = Napi::String::New(env, accountData.label);
    arg_1.Set("label", arg_1_2);
    auto arg_1_3 = Napi::String::New(env, accountData.state);
    arg_1.Set("state", arg_1_3);
    auto arg_1_4 = Napi::String::New(env, accountData.type);
    arg_1.Set("type", arg_1_4);
    auto arg_1_5 = Napi::Value::From(env, accountData.isHD);
    arg_1.Set("isHD", arg_1_5);
    auto arg_1_6 = Napi::Array::New(env);
    for(size_t arg_1_6_id = 0; arg_1_6_id < accountData.accountLinks.size(); arg_1_6_id++)
    {
        auto arg_1_6_elem = Napi::String::New(env, accountData.accountLinks[arg_1_6_id]);
        arg_1_6.Set((int)arg_1_6_id,arg_1_6_elem);
    }

    arg_1.Set("accountLinks", arg_1_6);

    args.push_back(arg_1);
    Napi::Value calling_function_as_value = Value().Get("onAccountModified");
    if(!calling_function_as_value.IsUndefined() && !calling_function_as_value.IsNull())
    {
        Napi::Function calling_function = calling_function_as_value.As<Napi::Function>();
        auto result_onAccountModified = calling_function.Call(args);
        if(result_onAccountModified.IsEmpty())
        {
            Napi::Error::New(env, "NJSIAccountsListener::onAccountModified call failed").ThrowAsJavaScriptException();
            return;
        }
    }
}

void NJSIAccountsListener::onAccountModified(const std::string & accountUUID, const AccountRecord & accountData)
{
    uv_work_t* request = new uv_work_t;
    request->data = new std::tuple<NJSIAccountsListener*, std::string, AccountRecord>(this, accountUUID, accountData);

    uv_queue_work(uv_default_loop(), request, [](uv_work_t*) -> void{}, [](uv_work_t* req, int status) -> void
    {
        NJSIAccountsListener* pthis = std::get<0>(*((std::tuple<NJSIAccountsListener*, std::string, AccountRecord>*)req->data));
        pthis->onAccountModified_aimpl__(std::get<1>(*((std::tuple<NJSIAccountsListener*, std::string, AccountRecord>*)req->data)), std::get<2>(*((std::tuple<NJSIAccountsListener*, std::string, AccountRecord>*)req->data)));
        delete (std::tuple<NJSIAccountsListener*, std::string, AccountRecord>*)req->data;
        req->data = nullptr;
    }
    );
}

Napi::FunctionReference NJSIAccountsListener::constructor;

Napi::Object NJSIAccountsListener::Init(Napi::Env env, Napi::Object exports) {

    Napi::Function func = DefineClass(env, "NJSIAccountsListener",{});
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("NJSIAccountsListener", func);
    return exports;
}
