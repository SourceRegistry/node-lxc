//
// Created by A.P.A. Slaa (a.p.a.slaa@projectsource.nl) on 2/21/24.
//

#ifndef NODE_LXC_HELPERS_H
#define NODE_LXC_HELPERS_H

//region opt and obj
#define opt_obj_val(key, getter, default) \
    (options.Has(key) ? options.Get(key).getter : default)


#define opt_has_val_checked(key, checker) \
    (options.Has(key) && options.Get(key).checker)


#define opt_strdup_val_checked(key, default) \
    (options.Has(key) && options.Get(key).IsString())? strdup(options.Get(key).ToString().Utf8Value().c_str()):default

#define obj_has_val_checked_assign(obj, key, checker, getter, var) \
    if(obj && obj.Has(key) && obj.Get(key).checker){ \
        var = obj.Get(key).getter;                          \
    }

//endregion

//region check

#define check(condition, message) \
    if((condition)){ \
         Napi::Error::New(info.Env(), message).ThrowAsJavaScriptException(); \
         return info.Env().Undefined(); \
    }

#define check_void(condition, message) \
    if((condition)){ \
         Napi::Error::New(info.Env(), message).ThrowAsJavaScriptException(); \
         return; \
    }


#define check_deferred(condition, message) \
    if((condition)){ \
        deferred.Reject(Napi::String::New(info.Env(), message)); \
        return deferred.Promise(); \
    }

//endregion
//region assert

#define assert(handle, message) \
    if(!(handle)){ \
        Napi::Error::New(info.Env(), message).ThrowAsJavaScriptException(); \
        return info.Env().Undefined(); \
    }

#define assert_void(handle, message) \
    if(!(handle)){ \
        Napi::Error::New(info.Env(), message).ThrowAsJavaScriptException(); \
        return; \
    }

#define assert_deferred(handle, message) \
    if(!(handle)){ \
        deferred.Reject(Napi::String::New(info.Env(), message)); \
        return deferred.Promise(); \
    }
//endregion

#endif //NODE_LXC_HELPERS_H
