/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 16-12-2023
 */

#include <napi.h>
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

Napi::Value ListAllContainer(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    // You can specify the LXC path or set it to NULL for the default path
    char **names;
    // Get the list of all containers
    int num_containers = list_all_containers(info[0].IsString() ? info[0].ToString().Utf8Value().c_str()
                                                                : lxc_get_global_config_item("lxc.lxcpath"), &names,
                                             nullptr);
    if (num_containers == -1) {
        Napi::Error::New(env, strerror(errno)).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    // Create an array to store container names
    Napi::Array result = Napi::Array::New(env);
    // Iterate through the containers and add their names to the array
    for (int i = 0; i < num_containers; ++i) {
        const char *name = names[i];
        result.Set(i, Napi::String::New(env, name));
    }
    return result;
}

Napi::Value ListAllDefinedContainer(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    // You can specify the LXC path or set it to NULL for the default path
    char **names;
    // Get the list of all defined containers
    int num_containers = list_defined_containers(info[0].IsString() ? info[0].ToString().Utf8Value().c_str()
                                                                : lxc_get_global_config_item("lxc.lxcpath"), &names,
                                             nullptr);
    if (num_containers == -1) {
        Napi::Error::New(env, strerror(errno)).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    // Create an array to store container names
    Napi::Array result = Napi::Array::New(env);
    // Iterate through the containers and add their names to the array
    for (int i = 0; i < num_containers; ++i) {
        const char *name = names[i];
        result.Set(i, Napi::String::New(env, name));
    }
    return result;
}

Napi::Value ListAllActiveContainers(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    // You can specify the LXC path or set it to NULL for the default path
    char **names;
    // Get the list of all defined containers
    int num_containers = list_active_containers(info[0].IsString() ? info[0].ToString().Utf8Value().c_str()
                                                                : lxc_get_global_config_item("lxc.lxcpath"), &names,
                                             nullptr);
    if (num_containers == -1) {
        Napi::Error::New(env, strerror(errno)).ThrowAsJavaScriptException();
        return env.Null();
    }
    // Create an array to store container names
    Napi::Array result = Napi::Array::New(env);
    // Iterate through the containers and add their names to the array
    for (int i = 0; i < num_containers; ++i) {
        const char *name = names[i];
        result.Set(i, Napi::String::New(env, name));
    }
    return result;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Container::Init(env, exports);
    exports.Set("GetVersion", Napi::Function::New(env,GetVersion));
    exports.Set("GetGlobalConfigItem", Napi::Function::New(env, GetGlobalConfigItem));
    exports.Set("ListAllContainers", Napi::Function::New(env, ListAllContainer));
    exports.Set("ListAllDefinedContainers", Napi::Function::New(env, ListAllDefinedContainer));
    exports.Set("ListAllActiveContainers", Napi::Function::New(env, ListAllActiveContainers));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
