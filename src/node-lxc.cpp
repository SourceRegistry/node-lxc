/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 16-12-2023
 */

#include <napi.h>
#include <string>
#include "Container.h"

Napi::String GetVersion(const Napi::CallbackInfo &info) {
    return Napi::String::New(info.Env(), lxc_get_version());
}

Napi::Value GetGlobalConfigItem(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() <= 0 || !info[0].IsString()) {
        Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    return Napi::String::New(info.Env(), lxc_get_global_config_item(info[0].ToString().Utf8Value().c_str()));
}

static Napi::Array NamesArrayToJS(Napi::Env env, char **names, int count) {
    Napi::Array result = Napi::Array::New(env);
    for (int i = 0; i < count; ++i) {
        result.Set(i, Napi::String::New(env, names[i]));
        free(names[i]);
    }
    free(names);
    return result;
}

Napi::Value ListAllContainers(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    char **names = nullptr;
    std::string pathStr = info[0].IsString() ? info[0].ToString().Utf8Value() : std::string();
    const char *path = info[0].IsString() ? pathStr.c_str() : lxc_get_global_config_item("lxc.lxcpath");
    int count = list_all_containers(path, &names, nullptr);
    if (count < 0) {
        Napi::Error::New(env, strerror(errno)).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    return NamesArrayToJS(env, names, count);
}

Napi::Value ListAllDefinedContainers(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    char **names = nullptr;
    std::string pathStr = info[0].IsString() ? info[0].ToString().Utf8Value() : std::string();
    const char *path = info[0].IsString() ? pathStr.c_str() : lxc_get_global_config_item("lxc.lxcpath");
    int count = list_defined_containers(path, &names, nullptr);
    if (count < 0) {
        Napi::Error::New(env, strerror(errno)).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    return NamesArrayToJS(env, names, count);
}

Napi::Value ListAllActiveContainers(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    char **names = nullptr;
    std::string pathStr = info[0].IsString() ? info[0].ToString().Utf8Value() : std::string();
    const char *path = info[0].IsString() ? pathStr.c_str() : lxc_get_global_config_item("lxc.lxcpath");
    int count = list_active_containers(path, &names, nullptr);
    if (count < 0) {
        Napi::Error::New(env, strerror(errno)).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    return NamesArrayToJS(env, names, count);
}

Napi::Value ConfigItemIsSupported(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() <= 0 || !info[0].IsString()) {
        Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    return Napi::Boolean::New(env, lxc_config_item_is_supported(info[0].ToString().Utf8Value().c_str()));
}

Napi::Value HasApiExtension(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() <= 0 || !info[0].IsString()) {
        Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    return Napi::Boolean::New(env, lxc_has_api_extension(info[0].ToString().Utf8Value().c_str()));
}

Napi::Value GetWaitStates(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    int count = lxc_get_wait_states(nullptr);
    if (count <= 0) return Napi::Array::New(env);

    std::vector<const char *> states(count);
    lxc_get_wait_states(states.data());

    auto result = Napi::Array::New(env);
    for (int i = 0; i < count; ++i) {
        result.Set(i, Napi::String::New(env, states[i]));
    }
    return result;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Container::Init(env, exports);
    exports.Set("GetVersion", Napi::Function::New(env, GetVersion));
    exports.Set("GetGlobalConfigItem", Napi::Function::New(env, GetGlobalConfigItem));
    exports.Set("ListAllContainers", Napi::Function::New(env, ListAllContainers));
    exports.Set("ListAllDefinedContainers", Napi::Function::New(env, ListAllDefinedContainers));
    exports.Set("ListAllActiveContainers", Napi::Function::New(env, ListAllActiveContainers));
    exports.Set("ConfigItemIsSupported", Napi::Function::New(env, ConfigItemIsSupported));
    exports.Set("HasApiExtension", Napi::Function::New(env, HasApiExtension));
    exports.Set("GetWaitStates", Napi::Function::New(env, GetWaitStates));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
