/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 16-12-2023
 */

#include "Container.h"

#include <sys/wait.h>
#include <sstream>
#include "./helpers/Array.h"
#include "./helpers/helpers.h"
#include "./helpers/AsyncPromise.h"

Napi::Object Container::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func =
            DefineClass(env, "Container", {
                            /* Instance Accessors */
                            InstanceAccessor("error", &Container::GetError, nullptr),
                            InstanceAccessor("name", &Container::GetName, &Container::SetName),
                            InstanceAccessor("defined", &Container::GetDefined, nullptr),
                            InstanceAccessor("state", &Container::GetState, nullptr),
                            InstanceAccessor("running", &Container::GetRunning, nullptr),
                            InstanceAccessor("initPID", &Container::GetInitPID, nullptr),
                            InstanceAccessor("configFileName", &Container::GetConfigFileName, nullptr),
                            InstanceAccessor("daemonize", &Container::GetDaemonize, &Container::SetDaemonize),
                            InstanceAccessor("configPath", &Container::GetConfigPath, &Container::SetConfigPath),

                            /* Instance Methods */
                            InstanceMethod("freeze", &Container::Freeze),
                            InstanceMethod("unfreeze", &Container::Unfreeze),
                            InstanceMethod("loadConfig", &Container::LoadConfig),

                            InstanceMethod("start", &Container::Start),
                            InstanceMethod("stop", &Container::Stop),

                            InstanceMethod("wantCloseAllFds", &Container::WantCloseAllFds),
                            InstanceMethod("wait", &Container::Wait),
                            InstanceMethod("setConfigItem", &Container::SetConfigItem),
                            InstanceMethod("destroy", &Container::Destroy),
                            InstanceMethod("save", &Container::Save),

                            InstanceMethod("create", &Container::Create),
                            InstanceMethod("reboot", &Container::Reboot),
                            InstanceMethod("shutdown", &Container::Shutdown),
                            InstanceMethod("clearConfig", &Container::ClearConfig),
                            InstanceMethod("clearConfigItem", &Container::ClearConfigItem),
                            InstanceMethod("getConfigItem", &Container::GetConfigItem),
                            InstanceMethod("getRunningConfigItem", &Container::GetRunningConfigItem),
                            InstanceMethod("getKeys", &Container::GetKeys),
                            InstanceMethod("getInterfaces", &Container::GetInterfaces),
                            InstanceMethod("getIPs", &Container::GetIPs),
                            InstanceMethod("getCGroupItem", &Container::GetCGroupItem),
                            InstanceMethod("setCGroupItem", &Container::SetCGroupItem),
                            InstanceMethod("clone", &Container::Clone),
                            InstanceMethod("consoleGetFds", &Container::ConsoleGetFds),
                            InstanceMethod("console", &Container::Console),

                            InstanceMethod("attach", &Container::Attach), // WITH run_wait
                            InstanceMethod("snapshot", &Container::Snapshot),
                            InstanceMethod("snapshotList", &Container::SnapshotList),
                            InstanceMethod("snapshotRestore", &Container::SnapshotRestore),
                            InstanceMethod("snapshotDestroy", &Container::SnapshotDestroy),
                            // TODO: MAY CONTROL (may_control)???
                            InstanceMethod("addDeviceNode", &Container::AddDeviceNode),
                            InstanceMethod("removeDeviceNode", &Container::RemoveDeviceNode),
                            InstanceMethod("attachInterface", &Container::AttachInterface),
                            InstanceMethod("detachInterface", &Container::DetachInterface),
                            InstanceMethod("checkpoint", &Container::Checkpoint),
                            InstanceMethod("restore", &Container::Restore),
                            InstanceMethod("migrate", &Container::Migrate),
                            InstanceMethod("consoleLog", &Container::ConsoleLog),
                            //                    InstanceMethod("reboot2", &Container::Reboot2),
                            InstanceMethod("mount", &Container::Mount),
                            InstanceMethod("umount", &Container::Umount),

                            // TODO: THIS MAY NEED TO BE A GETTER
                            InstanceMethod("seccompNotifyFd", &Container::SeccompNotifyFd),
                            InstanceMethod("seccompNotifyFdActive", &Container::SeccompNotifyFdActive),
                            InstanceMethod("initPIDFd", &Container::InitPIDFd),
                            InstanceMethod("devptsFd", &Container::DevptsFd),

                            InstanceMethod("exec", &Container::Exec),
                        });

    auto *constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);
    exports.Set("Container", func);
    return exports;
}

Napi::Value Container::GetError(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    auto obj = Napi::Object::New(info.Env());
    obj.Set("num", Napi::Number::New(info.Env(), _container->error_num));
    obj.Set("string", Napi::String::New(info.Env(), _container->error_string));
    return obj;
}

Napi::Value Container::GetName(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    return Napi::String::New(info.Env(), _container->name);
}

void Container::SetName(const Napi::CallbackInfo &info, const Napi::Value &value) {
    assert_void(_container, "Invalid container pointer")
    assert_void(value.IsString(), "Invalid arguments")
    check_void(_container->rename(_container, value.ToString().Utf8Value().c_str()),
               "Unable to rename container to " + value.ToString().Utf8Value())
}

Napi::Value Container::GetState(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    return Napi::String::New(info.Env(), _container->state(_container));
}

Napi::Value Container::GetRunning(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    return Napi::Boolean::New(info.Env(), _container->is_running(_container));
}

Napi::Value Container::GetInitPID(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    return Napi::Number::New(info.Env(), _container->init_pid(_container));
}

Napi::Value Container::GetConfigFileName(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    return Napi::String::New(info.Env(), _container->config_file_name(_container));
}

Napi::Value Container::GetDefined(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    return Napi::Boolean::New(info.Env(), _container->is_defined(_container));
}

Container::Container(const Napi::CallbackInfo &info) : Napi::ObjectWrap<Container>(info) {
    check_void(info.Length() <= 0 || !(info[0].IsString() || info[0].IsExternal()), "Invalid argument")

    if (info[0].IsString()) {
        _container = lxc_container_new(info[0].ToString().Utf8Value().c_str(),
                                       info[1].IsString()
                                           ? info[1].ToString().Utf8Value().c_str()
                                           : lxc_get_global_config_item("lxc.lxcpath"));
        if (info[2].IsString()) {
            if (info[2].ToString().Utf8Value() != "none") {
                _container->load_config(_container, info[2].ToString().Utf8Value().c_str());
            } else {
                _container->load_config(_container, lxc_get_global_config_item("lxc.default_config"));
            }
        }
    } else if (info[0].IsExternal()) {
        _container = info[0].As<Napi::External<lxc_container> >().Data();
    } else {
        /* Never reached */
        Napi::Error::New(info.Env(), "Invalid constructor argument").ThrowAsJavaScriptException();
        return;
    }
}

Container::~Container() {
    if (_container) {
        // Clean up the container if needed
        lxc_container_put(_container);
    }
}

Napi::Value Container::Freeze(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    auto worker = new AsyncPromise<>(
        deferred,
        [this](AsyncPromise<> *worker) {
            if (!_container->freeze(_container)) {
                worker->Error("Freezing " + std::string(_container->name) + " failed");
                return;
            }
        });
    return worker->Promise();
}

Napi::Value Container::Unfreeze(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    auto worker = new AsyncPromise<>(deferred, [this](AsyncPromise<> *worker) {
        if (!_container->unfreeze(_container)) {
            worker->Error("Unfreezing " + std::string(_container->name) + " failed");
            return;
        }
    });
    return worker->Promise();
}

Napi::Value Container::LoadConfig(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    auto alt_file = info[0].ToString().Utf8Value();
    auto worker = new AsyncPromise<>(deferred, [this, &alt_file](AsyncPromise<> *worker) {
        if (!_container->load_config(_container, alt_file.c_str())) {
            worker->Error(std::string(_container->name) + " is unable to load config " + alt_file);
            return;
        }
    });
    return worker->Promise();
}

Napi::Value Container::Start(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")

    // Get the array of strings from the JavaScript side
    auto useinit = info[0].IsNumber() ? info[0].ToNumber().Int32Value() : 0;

    uint32_t argvLength = 0;
    char **argv = nullptr;
    if (info[1].IsArray()) {
        argv = Array::NapiToCharStarArray(info[1].As<Napi::Array>(), argvLength);
    }

    auto worker = new AsyncPromise<int>(
        deferred,
        [this, useinit, argv, argvLength](AsyncPromise<int> *worker) {
            if (!this->_container->may_control(this->_container)) {
                worker->Error("Insufficient privileges to control container");
                return;
            }
            if (this->_container->is_running(_container)) {
                return;
            }
            if (!_container->start(_container, useinit, argv)) {
                worker->Error(strerror(_container->error_num));
            }
            Array::free(argv, argvLength);
        },
        AsyncPromise<int>::NumberWrapper);
    return worker->Promise();
}

Napi::Value Container::Stop(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    auto worker = new AsyncPromise<>(deferred, [this](AsyncPromise<> *worker) {
        if (!_container->may_control(_container)) {
            worker->Error("Insufficient privileges to control container");
            return;
        }
        if (!_container->is_running(_container)) {
            return;
        }
        if (!_container->stop(_container)) {
            worker->Error(std::string(_container->name) + " failed to stop");
            return;
        }
    });
    return worker->Promise();
}

void Container::SetDaemonize(const Napi::CallbackInfo &info, const Napi::Value &value) {
    assert_void(_container, "Invalid container pointer")
    assert_void(value.IsBoolean(), "Invalid argument")
    assert_void(_container->want_daemonize(_container, info[0].ToBoolean()),
                "Unable to set container wants to daemonize")
}

Napi::Value Container::GetDaemonize(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    return Napi::Boolean::New(info.Env(), _container->daemonize);
}

Napi::Value Container::GetConfigPath(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    return Napi::String::New(info.Env(), _container->get_config_path(_container));
}

void Container::SetConfigPath(const Napi::CallbackInfo &info, const Napi::Value &value) {
    assert_void(_container, "Invalid container pointer")
    assert_void(value.IsString(), "Invalid argument")
    assert_void(_container->set_config_path(_container, info[0].ToString().Utf8Value().c_str()),
                "Unable to set container config path")
}

Napi::Value Container::WantCloseAllFds(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    check(info.Length() <= 0 || !info[0].IsBoolean(), "Invalid arguments")
    return Napi::Boolean::New(info.Env(), _container->want_close_all_fds(_container, info[0].ToBoolean()));
}

Napi::Value Container::Wait(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")

    auto state = info[0].ToString().Utf8Value();
    auto timeout = info[1].IsNumber() ? info[1].ToNumber().Int32Value() : -1;

    auto worker = new AsyncPromise<>(
        deferred,
        [this, state, timeout](AsyncPromise<> *worker) {
            if (!this->_container->may_control(this->_container)) {
                worker->Error("Insufficient privileges to control container");
                return;
            }
            if (!this->_container->wait(_container, state.c_str(), timeout)) {
                worker->Error("Container timed out");
            }
        });
    return worker->Promise();
}

Napi::Value Container::Create(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    check_deferred(info.Length() <= 0 || !info[0].IsObject(), "Invalid arguments")

    auto options = info[0].ToObject();

    // Parse container creation parameters
    auto template_ = opt_obj_val_str("template", ToString().Utf8Value(), "none");
    auto bdevtype = opt_obj_val_str("bdevtype", ToString().Utf8Value(), "dir");

    // create bdev_spec struct properties
    std::string bdev_spec_fstype;
    uint64_t bdev_spec_fssize = 0;
    std::string bdev_spec_zfs_zfsroot;
    std::string bdev_spec_lvm_vg;
    std::string bdev_spec_lvm_lv;
    std::string bdev_spec_lvm_thinpool;
    std::string bdev_spec_dir;
    std::string bdev_spec_rbd_rbdname;
    std::string bdev_spec_rbd_rbdpool;

    if (opt_has_val_checked("bdev_specs", IsObject())) {
        auto bdevSpecsObj = options.Get("bdev_specs").ToObject();
        obj_has_val_checked_assign(bdevSpecsObj, "fstype", IsString(), ToString(), bdev_spec_fstype)
        /* fssize can be a number or a bigint if number larger than int64 aka uint64*/
        obj_has_val_checked_assign(bdevSpecsObj, "fssize", IsNumber(), ToNumber().Int64Value(), bdev_spec_fssize)
        obj_has_val_checked_assign(bdevSpecsObj, "fssize", IsBigInt(), As<Napi::BigInt>().Uint64Value(nullptr),
                                   bdev_spec_fssize)
        obj_has_val_checked_assign(bdevSpecsObj, "fstype", IsString(), ToString(), bdev_spec_fstype)
        /* zfs specs */
        auto bdev_spec_zfs = bdevSpecsObj.Get("zfs").ToObject();
        obj_has_val_checked_assign(bdev_spec_zfs, "zfsroot", IsString(), ToString(), bdev_spec_dir)
        /* lvm specs */
        auto bdev_spec_lvm = bdevSpecsObj.Get("lvm").ToObject();
        obj_has_val_checked_assign(bdev_spec_lvm, "vg", IsString(), ToString(), bdev_spec_lvm_vg)
        obj_has_val_checked_assign(bdev_spec_lvm, "lv", IsString(), ToString(), bdev_spec_lvm_lv)
        obj_has_val_checked_assign(bdev_spec_lvm, "thinpool", IsString(), ToString(), bdev_spec_lvm_vg)
        /* dir property */
        obj_has_val_checked_assign(bdevSpecsObj, "dir", IsString(), ToString(), bdev_spec_dir)
        /* rbd specs */
        auto bdev_spec_rbd = bdevSpecsObj.Get("zfs").ToObject();
        obj_has_val_checked_assign(bdev_spec_rbd, "rbdname", IsString(), ToString(), bdev_spec_rbd_rbdname)
        obj_has_val_checked_assign(bdev_spec_rbd, "rbdpool", IsString(), ToString(), bdev_spec_rbd_rbdname)
    }
    //  Get the creation flags for the container
    int flags = opt_obj_val("flags", ToNumber().Int32Value(), LXC_CREATE_QUIET);

    // Get the array of strings from the JavaScript side
    uint32_t argvLength = 0;
    char **argv = nullptr;
    if (opt_has_val_checked("argv", IsArray())) {
        argv = Array::NapiToCharStarArray(info[0].ToObject().Get("argv").As<Napi::Array>(), argvLength);
    }

    auto worker = new AsyncPromise<>(
        deferred,
        [this, template_, bdevtype, flags, argv, argvLength, bdev_spec_fstype, bdev_spec_fssize, bdev_spec_zfs_zfsroot,
            bdev_spec_lvm_lv, bdev_spec_lvm_vg, bdev_spec_lvm_thinpool, bdev_spec_dir, bdev_spec_rbd_rbdname,
            bdev_spec_rbd_rbdpool](
    AsyncPromise<> *worker) {
            if (_container->is_defined(_container)) {
                worker->Error("Container already exits");
                return;
            }
            bdev_specs specs{
                .fstype = const_cast<char *>(bdev_spec_fstype.empty()
                                                 ? nullptr
                                                 : bdev_spec_fstype.c_str()),
                .fssize = bdev_spec_fssize,
                .zfs{
                    .zfsroot = const_cast<char *>(bdev_spec_zfs_zfsroot.empty()
                                                      ? nullptr
                                                      : bdev_spec_zfs_zfsroot.c_str()),
                },
                .lvm{
                    .vg = const_cast<char *>(bdev_spec_lvm_vg.empty() ? nullptr : bdev_spec_lvm_vg.c_str()),
                    .lv = const_cast<char *>(bdev_spec_lvm_lv.empty() ? nullptr : bdev_spec_lvm_lv.c_str()),
                    .thinpool = const_cast<char *>(bdev_spec_lvm_thinpool.empty()
                                                       ? nullptr
                                                       : bdev_spec_lvm_thinpool.c_str()),
                },
                .dir = const_cast<char *>(bdev_spec_dir.empty()
                                              ? nullptr
                                              : bdev_spec_dir.c_str()),
                .rbd{
                    .rbdname = const_cast<char *>(bdev_spec_rbd_rbdname.empty()
                                                      ? nullptr
                                                      : bdev_spec_lvm_vg.c_str()),
                    .rbdpool = const_cast<char *>(bdev_spec_rbd_rbdpool.empty()
                                                      ? nullptr
                                                      : bdev_spec_rbd_rbdpool.c_str()),
                }
            };
            if (!_container->create(_container, template_.c_str(), bdevtype.c_str(), &specs, flags, argv)) {
                worker->Error(strerror(errno));
            }
            Array::free(argv, argvLength);
        });
    return worker->Promise();
}

Napi::Value Container::Reboot(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")

    auto timeout = info[0].IsNumber() ? info[0].ToNumber().Int32Value() : -1; // Default -1 wait forever

    auto worker = new AsyncPromise<>(deferred, [this, timeout](AsyncPromise<> *worker) {
        if (!this->_container->is_running(_container)) {
            worker->Error(std::string(this->_container->name) + " not running");
            return;
        }
        if (!this->_container->reboot2(_container, timeout)) {
            worker->Error("Container reboot timed out");
        }
    });

    return worker->Promise();
}

Napi::Value Container::Shutdown(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")

    auto timeout = info[0].IsNumber() ? info[0].ToNumber().Int32Value() : -1; // Default -1 wait forever

    auto worker = new AsyncPromise<>(
        deferred,
        [this, timeout](AsyncPromise<> *worker) {
            if (!this->_container->is_running(_container)) {
                worker->Error(std::string(this->_container->name) + " not running");
                return;
            }
            if (!this->_container->shutdown(_container, timeout)) {
                worker->Error("Container shutdown timed out");
            }
        });

    return worker->Promise();
}

Napi::Value Container::Destroy(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")

    auto force = false;
    auto include_snapshots = false;
    if (info[0].IsObject()) {
        auto options = info[0].ToObject();
        force = opt_obj_val("force", ToBoolean(), false);
        include_snapshots = opt_obj_val("include_snapshots", ToBoolean(), false);
    }
    auto worker = new AsyncPromise<>(
        deferred,
        [this, force, include_snapshots](AsyncPromise<> *worker) {
            // TODO: Check if container has snapshots

            if (_container->is_running(_container)) {
                if (!force) {
                    worker->Error(std::string(_container->name) + " is running");
                    return;
                }
                /* If the container was ephemeral, it will be removed on shutdown. */
                if (!_container->stop(_container)) {
                    worker->Error("Failed to stop " + std::string(_container->name));
                    return;
                }
            }
            /* If the container was ephemeral we have already removed it when we stopped it. */
            if (_container->is_defined(_container)) {
                char buf[256];
                auto ret = _container->get_config_item(_container, "lxc.ephemeral", buf, 256);
                if (ret > 0 && strcmp(buf, "0") == 0) {
                    if (include_snapshots) {
                        if (!_container->destroy_with_snapshots(_container))
                        // TODO: THIS DOESN'T WORK YET MAYBE BECAUSE CONTAINER HAS NO SNAPSHOT???
                        {
                            worker->Error("Destroying " + std::string(_container->name) + " failed");
                            return;
                        }
                        _container = nullptr;
                    } else {
                        if (!_container->destroy(_container)) {
                            worker->Error("Destroying " + std::string(_container->name) + " failed");
                            return;
                        }
                        _container = nullptr;
                    }
                }
            } else {
                _container = nullptr;
            }
        });

    return worker->Promise();
}

Napi::Value Container::Save(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")

    auto alt_file = info[0].ToString().Utf8Value();

    auto worker = new AsyncPromise<>(
        deferred,
        [this, alt_file](AsyncPromise<> *worker) {
            if (!_container->save_config(_container, alt_file.c_str())) {
                worker->Error("Container config could not be saved");
                return;
            }
        });

    return worker->Promise();
}

Napi::Value Container::GetConfigItem(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    check(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")

    char *value;

    int len = _container->get_config_item(_container, info[0].ToString().Utf8Value().c_str(), nullptr, 0);
    if (len <= 0) {
        return info.Env().Null();
    }

again:
    value = (char *) malloc(sizeof(char) * len + 1);
    if (value == nullptr)
        goto again;

    if (_container->get_config_item(_container, info[0].ToString().Utf8Value().c_str(), value, len + 1) != len) {
        free(value);
        return info.Env().Null();
    }
    return Napi::String::New(info.Env(), value);
}

Napi::Value Container::GetRunningConfigItem(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    check(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    return Napi::String::New(info.Env(),
                             _container->get_running_config_item(_container, info[0].ToString().Utf8Value().c_str()));
}

Napi::Value Container::GetKeys(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")

    auto keys = Napi::Array::New(info.Env());

    char *value;
    int len = _container->get_keys(_container, info[0].IsString() ? info[0].ToString().Utf8Value().c_str() : nullptr,
                                   nullptr, 0);
    if (len <= 0) {
        return keys;
    }

again:
    value = (char *) malloc(sizeof(char) * len + 1);
    if (value == nullptr)
        goto again;

    if (_container->get_keys(_container, info[0].IsString() ? info[0].ToString().Utf8Value().c_str() : nullptr, value,
                             len + 1) !=
        len) {
        Napi::Error::New(info.Env(), "Key amount mismatch on retrieval").ThrowAsJavaScriptException();
        free(value);
    }
    // region split string /n
    std::string s(value);
    std::stringstream ss(s);
    std::string item;
    int index = 0;
    while (getline(ss, item, '\n')) {
        keys.Set(index++, Napi::String::New(info.Env(), item));
    }
    // endregion
    return keys;
}

Napi::Value Container::GetInterfaces(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")

    auto worker = new AsyncPromise<char **>(
        deferred,
        [this](AsyncPromise<char **> *worker) {
            if (!_container->is_running(_container)) {
                worker->Error(std::string(this->_container->name) + " not running");
                return;
            }
            auto interfaces = _container->get_interfaces(_container);
            if (!interfaces) {
                worker->Error("Interfaces could not be retrieved");
                return;
            }
            worker->Result(interfaces);
        },
        AsyncPromise<char **>::StringArrayWrapper);

    return worker->Promise();
}

Napi::Value Container::GetIPs(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString() || !info[1].IsString(), "Invalid arguments")

    auto iface = info[0].ToString().Utf8Value();
    auto family = info[1].ToString().Utf8Value();
    auto scope = (family == "inet6" && info[2].IsNumber()) ? info[2].ToNumber().Int32Value() : -1;

    auto worker = new AsyncPromise<char **>(
        deferred,
        [this, iface, family, scope](AsyncPromise<char **> *worker) {
            if (!_container->is_running(_container)) {
                worker->Error(std::string(this->_container->name) + " not running");
                return;
            }
            auto interfaces = _container->get_ips(_container, iface.c_str(), family.c_str(), scope);
            if (!interfaces) {
                worker->Error("IPs for interface " + iface + " could not be retrieved");
                return;
            }
            worker->Result(interfaces);
        },
        AsyncPromise<char **>::StringArrayWrapper);

    return worker->Promise();
}

Napi::Value Container::GetCGroupItem(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    check(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")

    char *value;
    int len = _container->get_cgroup_item(_container, info[0].ToString().Utf8Value().c_str(), nullptr, 0);
    if (len <= 0) {
        return info.Env().Undefined();
    }

again:
    value = (char *) malloc(sizeof(char) * len + 1);
    if (value == nullptr)
        goto again;

    if (_container->get_cgroup_item(_container, info[0].ToString().Utf8Value().c_str(), value, len + 1) != len) {
        free(value);
        return info.Env().Undefined();
    }
    return Napi::String::New(info.Env(), value);
}

void Container::SetCGroupItem(const Napi::CallbackInfo &info) {
    assert_void(_container, "Invalid container pointer")
    check_void(info.Length() <= 0 || !info[0].IsString() || !info[1].IsString(), "Invalid arguments")
    assert_void(_container->set_cgroup_item(_container, info[0].ToString().Utf8Value().c_str(),
                    info[1].ToString().Utf8Value().c_str()),
                "Unable to set cgroup value")
}

Napi::Value Container::Clone(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsObject(), "Invalid arguments")

    auto options = info[0].ToObject();

    auto newname = opt_obj_val_str("newname", ToString().Utf8Value(), "");
    auto lxcpath = opt_obj_val_str("lxcpath", ToString().Utf8Value(), "");
    auto flags = opt_obj_val("flags", ToNumber().Int32Value(), 0);
    auto bdevtype = opt_obj_val_str("bdevtype", ToString().Utf8Value(), "");
    auto bdevdata = opt_obj_val_str("bdevdata", ToString().Utf8Value(), "");
    auto newsize = opt_obj_val("newsize", ToNumber().Int64Value(), 0);

    char **hookargs = nullptr;
    uint32_t hookargsLength = 0;
    if (options.Has("hookargs")) {
        hookargs = Array::NapiToCharStarArray(options.Get("hookargs").As<Napi::Array>(), hookargsLength);
    }

    auto worker = new AsyncPromise<lxc_container *>(
        deferred,
        [this, newname, lxcpath, flags, bdevtype, bdevdata, newsize, hookargsLength, hookargs](
    AsyncPromise<lxc_container *> *worker) {
            if (_container->is_running(_container)) {
                worker->Error("Container needs to be stopped to clone");
                Array::free(hookargs, hookargsLength);
                return;
            }
            auto clone = _container->clone(_container,
                                           (!newname.empty() ? newname.c_str() : nullptr),
                                           (!lxcpath.empty() ? lxcpath.c_str() : nullptr),
                                           flags,
                                           (!bdevtype.empty() ? bdevtype.c_str() : nullptr),
                                           (!bdevdata.empty() ? bdevdata.c_str() : nullptr),
                                           newsize,
                                           hookargs);
            if (!clone) {
                worker->Error("Unable to clone container");
            } else {
                worker->Result(clone);
            }
            Array::free(hookargs, hookargsLength);
        },
        std::function<Napi::Value(const AsyncPromise<lxc_container *> *, const std::tuple<lxc_container *> &)>{
            [](const AsyncPromise<lxc_container *> *worker, const std::tuple<lxc_container *> &c) {
                return New(worker->Env(), {
                               Napi::External<lxc_container>::New(worker->Env(), std::get<0>(c))
                           });
            }
        });


    return worker->Promise();
}

Napi::Value Container::ConsoleGetFds(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container point")
    int _ttynum = info[0].IsNumber() ? info[0].ToNumber().Int32Value() : -1;

    auto worker = new AsyncPromise<int, int>(
        deferred,
        [this, _ttynum](AsyncPromise<int, int> *worker) {
            if (!_container->is_running(_container)) {
                worker->Error("Container is not running");
                return;
            }
            int ttynum = _ttynum;
            int ptxfd;
            ttynum = _container->console_getfd(_container, &ttynum, &ptxfd);
            if (ttynum < 0) {
                worker->Error("Unable to allocate console tty");
                return;
            }
            worker->Result(ttynum, ptxfd);
        },
        std::function<Napi::Value(const AsyncPromise<int, int> *, const std::tuple<int, int> &)>{
            [](const AsyncPromise<int, int> *worker, const std::tuple<int, int> &tuple) {
                auto array = Napi::Array::New(worker->Env());
                array.Set((uint32_t) 0, Napi::Number::New(worker->Env(), std::get<0>(tuple)));
                array.Set((uint32_t) 1, Napi::Number::New(worker->Env(), std::get<1>(tuple)));
                return array;
            }
        }
    );


    return worker->Promise();
}

void Container::SetConfigItem(const Napi::CallbackInfo &info) {
    assert_void(_container, "Invalid container pointer")
    check_void(info.Length() <= 0 || !info[0].IsString() || !info[1].IsString(), "Invalid arguments")
    assert_void(_container->set_config_item(_container, info[0].ToString().Utf8Value().c_str(),
                    info[1].ToString().Utf8Value().c_str()),
                "Unable to set config item")
}

void Container::ClearConfigItem(const Napi::CallbackInfo &info) {
    assert_void(_container, "Invalid container pointer")
    check_void(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    assert_void(_container->clear_config_item(_container, info[0].ToString().Utf8Value().c_str()),
                "Unable to clean config item")
}

void Container::ClearConfig(const Napi::CallbackInfo &info) {
    assert_void(_container, "Invalid container pointer")
    _container->clear_config(_container);
}

int wait_for_pid_status(pid_t pid) {
    int status, ret;

again:
    ret = waitpid(pid, &status, 0);
    if (ret == -1) {
        if (errno == EINTR)
            goto again;
        return -1;
    }
    if (ret != pid)
        goto again;
    return status;
}

Napi::Value Container::Attach(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")

    auto *attach_options = (lxc_attach_options_t *) malloc(sizeof(struct lxc_attach_options_t));

    uint32_t extra_env_varsLength = 0;
    uint32_t extra_keep_envLength = 0;

    if (info[0].IsObject()) {
        auto options = info[0].ToObject();
        attach_options->attach_flags = opt_obj_val("attach_flags", ToNumber().Int32Value(), LXC_ATTACH_DEFAULT);
        attach_options->namespaces = opt_obj_val("namespaces", ToNumber().Int32Value(), -1);
        attach_options->personality = opt_obj_val("personality", As<Napi::BigInt>().Int64Value(nullptr),
                                                  LXC_ATTACH_DETECT_PERSONALITY);
        attach_options->initial_cwd = (char *) (opt_strdup_val_checked("initial_cwd",
                                                                       nullptr)); // Clear at the end of promise
        attach_options->uid = opt_obj_val("uid", ToNumber().Uint32Value(), (uid_t) -1);
        attach_options->gid = opt_obj_val("gid", ToNumber().Uint32Value(), (gid_t) -1);

        attach_options->env_policy = (lxc_attach_env_policy_t) opt_obj_val("env_policy", ToNumber().Int32Value(),
                                                                           LXC_ATTACH_CLEAR_ENV);
        if (opt_has_val_checked("extra_env_vars", IsArray())) {
            auto **extra_env_vars = Array::NapiToCharStarArray(options.Get("extra_env_vars").As<Napi::Array>(),
                                                               extra_env_varsLength); // Clear at the end of promise
            attach_options->extra_env_vars = extra_env_vars;
        } else {
            attach_options->extra_env_vars = nullptr;
        }
        if (opt_has_val_checked("extra_keep_env", IsArray()) && attach_options->env_policy == LXC_ATTACH_CLEAR_ENV) {
            auto **extra_keep_env = Array::NapiToCharStarArray(options.Get("extra_keep_env").As<Napi::Array>(),
                                                               extra_keep_envLength); // Clear at the end of promise
            attach_options->extra_keep_env = extra_keep_env;
        } else {
            attach_options->extra_keep_env = nullptr;
        }
        if (opt_has_val_checked("stdio", IsArray())) {
            auto stdio = options.Get("stdio").As<Napi::Array>();
            attach_options->stdin_fd = stdio.Get((uint32_t) 0).ToNumber().Int32Value();
            attach_options->stdout_fd = stdio.Get((uint32_t) 1).ToNumber().Int32Value();
            attach_options->stderr_fd = stdio.Get((uint32_t) 2).ToNumber().Int32Value();
        } else {
            attach_options->stdin_fd = 0;
            attach_options->stdout_fd = 1;
            attach_options->stderr_fd = 2;
        }

        attach_options->log_fd = opt_obj_val("log_fd", ToNumber().Int32Value(), -EBADF);
        attach_options->lsm_label = opt_strdup_val_checked("lsm_label", nullptr); // Clear at the end of promise
#if VERSION_AT_LEAST(4, 0, 9)
        if (opt_has_val_checked("groups", IsArray())) {
            auto jsGroups = options.Get("groups").As<Napi::Array>();
            lxc_groups_t groups = {
                .size = jsGroups.Length(),
                .list = jsGroups.Length() > 0 ? new gid_t[jsGroups.Length()] : nullptr
            };
            if (groups.list != nullptr) {
                for (size_t i = 0; i < jsGroups.Length(); ++i) {
                    groups.list[i] = jsGroups.Get(i).ToNumber().Uint32Value();
                }
                attach_options->groups = groups;
                attach_options->attach_flags &= LXC_ATTACH_SETGROUPS;
            }
        } else {
            attach_options->groups = {};
        }
#endif
    } else {
        attach_options->attach_flags = LXC_ATTACH_DEFAULT;
        attach_options->namespaces = -1;
        attach_options->personality = LXC_ATTACH_DETECT_PERSONALITY;
        attach_options->initial_cwd = nullptr;
        attach_options->uid = (uid_t) -1;
        attach_options->gid = (gid_t) -1;
        attach_options->env_policy = LXC_ATTACH_KEEP_ENV;
        attach_options->extra_env_vars = nullptr;
        attach_options->extra_keep_env = nullptr;
        attach_options->stdin_fd = 0;
        attach_options->stdout_fd = 1;
        attach_options->stderr_fd = 2;
        attach_options->log_fd = -EBADF;
        attach_options->lsm_label = nullptr;
        attach_options->groups = {};
    }
    auto worker = new AsyncPromise<int>(
        deferred,
        [this, attach_options, extra_env_varsLength, extra_keep_envLength](
    AsyncPromise<int> *worker) {
            pid_t pid;
            if (_container->attach(_container, lxc_attach_run_shell, nullptr, attach_options, &pid) < 0) {
                worker->Error(strerror(errno));
            } else {
                worker->Result(pid);
            }
            auto ret = wait_for_pid_status(pid);
            if (ret < 0) {
                goto end;
            }
            if (WIFEXITED(ret)) {
                ret = WEXITSTATUS(ret);
                goto end;
            }
        end:
            // Cleanup: Free-up allocated memory
            free(attach_options->initial_cwd);
            Array::free(attach_options->extra_env_vars, extra_env_varsLength);
            Array::free(attach_options->extra_keep_env, extra_keep_envLength);
            free(attach_options->lsm_label);
            free(attach_options);
        },
        AsyncPromise<int>::NumberWrapper);

    return worker->Promise();
}

Napi::Value Container::Exec(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsObject(), "Invalid arguments")

    auto *attach_options = (lxc_attach_options_t *) malloc(sizeof(struct lxc_attach_options_t));

    uint32_t extra_env_varsLength = 0;
    uint32_t extra_keep_envLength = 0;
    uint32_t argvLength = 0;

    auto options = info[0].ToObject();
    attach_options->attach_flags = opt_obj_val("attach_flags", ToNumber().Int32Value(), LXC_ATTACH_DEFAULT);
    attach_options->namespaces = opt_obj_val("namespaces", ToNumber().Int32Value(), -1);
    attach_options->personality = opt_obj_val("personality", As<Napi::BigInt>().Int64Value(nullptr),
                                              LXC_ATTACH_DETECT_PERSONALITY);
    attach_options->initial_cwd = (char *) (opt_strdup_val_checked("initial_cwd",
                                                                   nullptr)); // Clear at the end of promise
    attach_options->uid = opt_obj_val("uid", ToNumber().Uint32Value(), (uid_t) -1);
    attach_options->gid = opt_obj_val("gid", ToNumber().Uint32Value(), (gid_t) -1);

    attach_options->env_policy = (lxc_attach_env_policy_t) opt_obj_val("env_policy", ToNumber().Int32Value(),
                                                                       LXC_ATTACH_CLEAR_ENV);
    if (opt_has_val_checked("extra_env_vars", IsArray())) {
        auto **extra_env_vars = Array::NapiToCharStarArray(options.Get("extra_env_vars").As<Napi::Array>(),
                                                           extra_env_varsLength); // Clear at the end of promise
        attach_options->extra_env_vars = extra_env_vars;
    } else {
        attach_options->extra_env_vars = nullptr;
    }
    if (opt_has_val_checked("extra_keep_env", IsArray()) && attach_options->env_policy == LXC_ATTACH_CLEAR_ENV) {
        auto **extra_keep_env = Array::NapiToCharStarArray(options.Get("extra_keep_env").As<Napi::Array>(),
                                                           extra_keep_envLength); // Clear at the end of promise
        attach_options->extra_keep_env = extra_keep_env;
    } else {
        attach_options->extra_keep_env = nullptr;
    }
    if (opt_has_val_checked("stdio", IsArray())) {
        auto stdio = options.Get("stdio").As<Napi::Array>();
        attach_options->stdin_fd = stdio.Get((uint32_t) 0).ToNumber().Int32Value();
        attach_options->stdout_fd = stdio.Get((uint32_t) 1).ToNumber().Int32Value();
        attach_options->stderr_fd = stdio.Get((uint32_t) 2).ToNumber().Int32Value();
    } else {
        attach_options->stdin_fd = 0;
        attach_options->stdout_fd = 1;
        attach_options->stderr_fd = 2;
    }

    attach_options->log_fd = opt_obj_val("log_fd", ToNumber().Int32Value(), -EBADF);
    attach_options->lsm_label = opt_strdup_val_checked("lsm_label", nullptr); // Clear at the end of promise
#if VERSION_AT_LEAST(4, 0, 9)
    if (opt_has_val_checked("groups", IsArray())) {
        auto jsGroups = options.Get("groups").As<Napi::Array>();
        lxc_groups_t groups = {
            .size = jsGroups.Length(),
            .list = jsGroups.Length() > 0 ? new gid_t[jsGroups.Length()] : nullptr
        };
        if (groups.list != nullptr) {
            for (size_t i = 0; i < jsGroups.Length(); ++i) {
                groups.list[i] = jsGroups.Get(i).ToNumber().Uint32Value();
            }
            attach_options->groups = groups;
            attach_options->attach_flags &= LXC_ATTACH_SETGROUPS;
        }
    } else {
        attach_options->groups = {};
    }
#endif

    auto *command = (lxc_attach_command_t *) malloc(sizeof(struct lxc_attach_command_t));
    auto argv = Array::NapiToCharStarArray(options.Get("argv").As<Napi::Array>(), argvLength);
    command->program = argv[0];
    command->argv = argv;

    auto worker = new AsyncPromise<int>(
        deferred,
        [this, attach_options, command, extra_env_varsLength, extra_keep_envLength](
    AsyncPromise<int> *worker) {
            pid_t pid;
            int ret = _container->attach(_container, lxc_attach_run_command, command, attach_options, &pid);
            if (ret < 0) {
                worker->Error(strerror(errno));
            } else {
                worker->Result(pid);
            }
            free(attach_options->initial_cwd);
            Array::free(attach_options->extra_env_vars, extra_env_varsLength);
            Array::free(attach_options->extra_keep_env, extra_keep_envLength);
            free(attach_options->lsm_label);
            free(attach_options);
            free(command);
        },
        AsyncPromise<int>::NumberWrapper);

    return worker->Promise();
}

Napi::Value Container::Console(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsNumber() || !info[1].IsArray() || !info[2].IsNumber(),
                   "Invalid arguments")

    auto ttynum = info[0].ToNumber().Int32Value();
    auto _stdin = info[1].As<Napi::Array>().Get((uint32_t) 0).ToNumber().Int32Value();
    auto _stdout = info[1].As<Napi::Array>().Get((uint32_t) 1).ToNumber().Int32Value();
    auto _stderr = info[1].As<Napi::Array>().Get((uint32_t) 2).ToNumber().Int32Value();
    auto escape = info[2].ToNumber().Int32Value();

    auto worker = new AsyncPromise<int>(
        deferred,
        [this, ttynum, _stdin, _stdout, _stderr, escape](AsyncPromise<int> *worker) {
            if (!this->_container->may_control(this->_container)) {
                worker->Error("Insufficient privileges to control container");
                return;
            }
            if (!this->_container->is_running(_container)) {
                worker->Error(std::string(this->_container->name) + " not running");
                return;
            }
            int ret = this->_container->console(_container, ttynum, _stdin, _stdout, _stderr, escape);
            if (ret < 0) {
                worker->Error(strerror(_container->error_num));
            }
        },
        AsyncPromise<int>::NumberWrapper);

    return worker->Promise();
}

Napi::Object Container::New(Napi::Env env, const std::initializer_list<napi_value> &args) {
    Napi::EscapableHandleScope scope(env);
    Napi::Object obj = env.GetInstanceData<Napi::FunctionReference>()->New(args);
    return scope.Escape(napi_value(obj)).ToObject();
}

Napi::Value Container::Snapshot(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    auto commentfile = info[0].ToString().Utf8Value();

    auto worker = new AsyncPromise<int>(
        deferred,
        [this, commentfile](AsyncPromise<int> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            auto num = _container->snapshot(_container, commentfile.c_str());
            if (num < 0) {
                worker->Error("Unable to create snapshot");
                return;
            }
            worker->Result(num);
        },
        AsyncPromise<int>::NumberWrapper);

    return worker->Promise();
}

Napi::Value Container::SnapshotList(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    auto worker = new AsyncPromise<lxc_snapshot *, int>(
        deferred,
        [this](AsyncPromise<lxc_snapshot *, int> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }

            struct lxc_snapshot *s;
            auto num = _container->snapshot_list(_container, &s);
            if (num < 0) {
                worker->Error("Unable to create snapshot list");
                return;
            }

            worker->Result(s, num);
        },
        std::function<Napi::Value(const AsyncPromise<lxc_snapshot *, int> *, const std::tuple<lxc_snapshot *, int> &)>{
            [](const AsyncPromise<lxc_snapshot *, int> *worker, const std::tuple<lxc_snapshot *, int> &data) {
                auto snapshotArray = Napi::Array::New(worker->Env());
                lxc_snapshot *snapshots = std::get<0>(data);
                int count = std::get<1>(data);

                for (int i = 0; i < count; i++) {
                    const auto &snapshot = snapshots[i];

                    auto snapshotObj = Napi::Object::New(worker->Env());
                    snapshotObj.Set("name", Napi::String::New(worker->Env(), snapshot.name));
                    snapshotObj.Set("comment_pathname", Napi::String::New(worker->Env(), snapshot.comment_pathname));
                    snapshotObj.Set("timestamp", Napi::String::New(worker->Env(), snapshot.timestamp));
                    snapshotObj.Set("lxcpath", Napi::String::New(worker->Env(), snapshot.lxcpath));

                    snapshotArray.Set(i, snapshotObj);
                }

                // Free snapshots array
                if (count > 0) {
                    snapshots->free(snapshots);
                }

                return snapshotArray;
            }
        }
    );
    return worker->Promise();
}

Napi::Value Container::SnapshotRestore(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    auto snapname = info[0].ToString().Utf8Value();
    auto newname = info[1].IsString() ? info[1].ToString().Utf8Value() : std::string(_container->name);

    auto worker = new AsyncPromise<>(
        deferred,
        [this, snapname, newname](AsyncPromise<> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            if (!_container->snapshot_restore(_container, snapname.c_str(), newname.c_str())) {
                worker->Error(
                    "Unable to restore " + std::string(_container->name) + " to snapshot " + snapname);
                return;
            }
        });

    return worker->Promise();
}

Napi::Value Container::SnapshotDestroy(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !(info[0].IsString() || info[1].IsBoolean()), "Invalid arguments")
    bool all = false;
    std::string snapname;
    if (info[0].IsString()) {
        snapname = info[0].ToString().Utf8Value();
    } else if (info[0].IsBoolean()) {
        all = info[0].ToBoolean();
    }

    auto worker = new AsyncPromise<>(
        deferred,
        [this, snapname, all](AsyncPromise<> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            if (all) {
                if (!_container->snapshot_destroy_all(_container)) {
                    worker->Error(
                        "Unable to destroy " + std::string(_container->name) + " all snapshots ");
                    return;
                }
            } else if (!snapname.empty()) {
                if (!_container->snapshot_destroy(_container, snapname.c_str())) {
                    worker->Error(
                        "Unable to destroy " + std::string(_container->name) + " to snapshot " + snapname);
                    return;
                }
            }
        });
    return worker->Promise();
}

Napi::Value Container::AddDeviceNode(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    auto src_path = info[0].ToString().Utf8Value();
    auto dest_path = info[1].IsString() ? info[1].ToString().Utf8Value() : "";

    auto worker = new AsyncPromise<lxc_snapshot *, int>(
        deferred,
        [this, src_path, dest_path](AsyncPromise<lxc_snapshot *, int> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            if (!_container->add_device_node(_container, src_path.c_str(),
                                             dest_path.empty() ? nullptr : dest_path.c_str())) {
                worker->Error("Unable to add device node " + src_path + ":" +
                              (dest_path.empty() ? src_path : dest_path) + " to " +
                              std::string(_container->name));
                return;
            }
        });
    return worker->Promise();
}

Napi::Value Container::RemoveDeviceNode(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    auto src_path = info[0].ToString().Utf8Value();
    auto dest_path = info[1].IsString() ? info[1].ToString().Utf8Value() : "";

    auto worker = new AsyncPromise<lxc_snapshot *, int>(
        deferred,
        [this, src_path, dest_path](AsyncPromise<lxc_snapshot *, int> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            if (!_container->remove_device_node(_container, src_path.c_str(),
                                                dest_path.empty() ? nullptr : dest_path.c_str())) {
                worker->Error("Unable to remove device node " + src_path + ":" +
                              (dest_path.empty() ? src_path : dest_path) + " from " +
                              std::string(_container->name));
                return;
            }
        });
    return worker->Promise();
}

Napi::Value Container::AttachInterface(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    auto dev = info[0].ToString().Utf8Value();
    auto dst_dev = info[1].IsString() ? info[1].ToString().Utf8Value() : dev;

    auto worker = new AsyncPromise<lxc_snapshot *, int>(
        deferred,
        [this, dev, dst_dev](AsyncPromise<lxc_snapshot *, int> *worker) {
            if (!_container->attach_interface(_container, dev.c_str(), dst_dev.c_str())) {
                worker->Error("Unable to attach interface " + dev + ":" + dst_dev + " to " +
                              std::string(_container->name));
                return;
            }
        });
    return worker->Promise();
}

Napi::Value Container::DetachInterface(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    auto dev = info[0].ToString().Utf8Value();
    auto dst_dev = info[1].IsString() ? info[1].ToString().Utf8Value() : dev;

    auto worker = new AsyncPromise<lxc_snapshot *, int>(
        deferred,
        [this, dev, dst_dev](AsyncPromise<lxc_snapshot *, int> *worker) {
            if (!_container->attach_interface(_container, dev.c_str(), dst_dev.c_str())) {
                worker->Error("Unable to detach interface " + dev + ":" + dst_dev + " to " +
                              std::string(_container->name));
                return;
            }
        });
    return worker->Promise();
}

Napi::Value Container::Checkpoint(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    auto directory = info[0].ToString().Utf8Value();
    auto stop = info[1].ToBoolean();
    auto verbose = info[2].ToBoolean();

    auto worker = new AsyncPromise<>(
        deferred,
        [this, directory, stop, verbose](AsyncPromise<> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            if (!_container->is_defined(_container)) {
                worker->Error(std::string(_container->name) + " not defined");
                return;
            }
            if (!_container->checkpoint(_container, const_cast<char *>(directory.c_str()), stop, verbose)) {
                worker->Error("Unable to create checkpoint for " + std::string(_container->name));
                return;
            }
        });
    return worker->Promise();
}

Napi::Value Container::Restore(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    auto directory = info[0].ToString().Utf8Value();
    auto verbose = info[1].ToBoolean();

    auto worker = new AsyncPromise<>(
        deferred,
        [this, directory, verbose](AsyncPromise<> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            if (!_container->restore(_container, const_cast<char *>(directory.c_str()), verbose)) {
                worker->Error("Unable to restore container");
                return;
            }
        });
    return worker->Promise();
}

Napi::Value Container::Migrate(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsNumber() || !info[0].IsObject(), "Invalid arguments")

    auto cmd = info[0].ToNumber().Int32Value();
    auto options = info[1].ToObject();

    auto opts = (struct migrate_opts *) calloc(1, sizeof(migrate_opts));

    opts->directory = opt_obj_val("directory", ToString().Utf8Value().data(), nullptr);
    opts->verbose = opt_obj_val("verbose", ToBoolean(), false);
    opts->stop = opt_obj_val("stop", ToBoolean(), false);
    opts->predump_dir = opt_obj_val("predump_dir", ToString().Utf8Value().data(), nullptr);
    opts->predump_dir = opt_obj_val("predump_dir", ToString().Utf8Value().data(), nullptr);
    opts->pageserver_address = opt_obj_val("pageserver_address", ToString().Utf8Value().data(), nullptr);
    opts->pageserver_port = opt_obj_val("pageserver_port", ToString().Utf8Value().data(), nullptr);
    opts->preserves_inodes = opt_obj_val("preserves_inodes", ToBoolean(), false);
    opts->action_script = opt_obj_val("action_script", ToString().Utf8Value().data(), nullptr);
    opts->disable_skip_in_flight = opt_obj_val("disable_skip_in_flight", ToBoolean(), false);
    opts->ghost_limit = opt_obj_val("ghost_limit", As<Napi::BigInt>().Uint64Value(nullptr), 0);
    opts->features_to_check = opt_obj_val("features_to_check", As<Napi::BigInt>().Uint64Value(nullptr), 0);

    auto size = sizeof(*opts);

    auto worker = new AsyncPromise<>(
        deferred,
        [this, cmd, opts, size](AsyncPromise<> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                goto cleanup;
            }
            if (_container->migrate(_container, cmd, opts, size) != 0) {
                worker->Error("Unable to migrate " + std::string(_container->name));
            }
        cleanup:
            free(opts);
        });
    return worker->Promise();
}

Napi::Value Container::ConsoleLog(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsObject(), "Invalid arguments")

    auto options = info[0].ToObject();

    uint64_t _read_max = (uint64_t) opt_obj_val("read_max", ToNumber().Int64Value(), 0);

    struct lxc_console_log log
    {
        .clear = opt_obj_val("clear", ToBoolean(), false),
        .read = opt_obj_val("read", ToBoolean(), false),
        .read_max = &_read_max,
        .data = nullptr
    };

    auto worker = new AsyncPromise<char *, size_t>(
        deferred,
        [this, &log](AsyncPromise<char *, size_t> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            auto ret = _container->console_log(_container, &log);
            if (ret < 0) {
                worker->Error(std::string(strerror(-ret)) + "- Failed to retrieve console log");
                return;
            } else {
                worker->Result(log.data, *log.read_max);
            }
        },
        AsyncPromise<char *, size_t>::SizeCharStringWrapper);
    return worker->Promise();
}

Napi::Value Container::Mount(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 ||
                   !info[0].IsString() || // source
                   !info[1].IsString() || // target
                   //                   !info[2].IsString() || // filesystemtype
                   !(info[3].IsBigInt() | info[3].IsNumber()) || // mountflags
                   !info[4].IsObject(), // lxc_mount
                   "Invalid arguments")

    auto source = info[0].ToString().Utf8Value();
    auto target = info[1].ToString().Utf8Value();
    auto filesystemtype = info[2].ToString().Utf8Value();
    auto mountflags = info[3].IsBigInt()
                          ? info[3].As<Napi::BigInt>().Uint64Value(nullptr)
                          : (uint64_t) info[3].ToNumber().Int64Value();

    lxc_mount mnt
    {
        .version = info[4].ToObject().Get("version").ToNumber().Int32Value()
    };

    auto worker = new AsyncPromise<>(
        deferred,
        [this, source, target, filesystemtype, mountflags, &mnt](AsyncPromise<> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            auto ret = _container->mount(_container, source.c_str(), target.c_str(),
                                         filesystemtype.empty() ? filesystemtype.c_str() : nullptr, mountflags,
                                         nullptr, &mnt); //TODO: Throws Segmentation fault
            if (ret < 0) {
                worker->Error(std::string("Failed to mount Error: ") + strerror(ret));
                //TODO: Throws Segmentation fault
            }
        });
    return worker->Promise();
}

Napi::Value Container::Umount(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 ||
                   !info[0].IsString() || // source
                   !(info[1].IsBigInt() | info[1].IsNumber()) || // mountflags
                   !info[2].IsObject(), // lxc_mount
                   "Invalid arguments")

    auto target = info[1].ToString().Utf8Value();
    auto mountflags = info[3].IsBigInt()
                          ? info[3].As<Napi::BigInt>().Uint64Value(nullptr)
                          : (uint64_t) info[3].ToNumber().Int64Value();

    struct lxc_mount mnt
    {
        .version = info[4].ToObject().Get("version").ToNumber().Int32Value()
    };

    auto worker = new AsyncPromise<>(
        deferred,
        [this, target, mountflags, &mnt](AsyncPromise<> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            auto ret = _container->umount(_container, target.c_str(), mountflags, &mnt);
            if (ret < 0) {
                worker->Error("Failed to unmount " + target);
                return;
            }
        });
    return worker->Promise();
}

Napi::Value Container::SeccompNotifyFd(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    auto worker = new AsyncPromise<int>(
        deferred,
        [this](AsyncPromise<int> *worker) {
            auto ret = _container->seccomp_notify_fd(_container);
            if (ret < 0) {
                worker->Error("Failed to retrieve a file descriptor for the container's seccomp filter");
                return;
            }
            worker->Result(ret);
        },
        AsyncPromise<int>::NumberWrapper);
    return worker->Promise();
}

Napi::Value Container::SeccompNotifyFdActive(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    auto worker = new AsyncPromise<int>(
        deferred,
        [this](AsyncPromise<int> *worker) {
            auto ret = _container->seccomp_notify_fd_active(_container);
            if (ret < 0) {
                worker->Error(
                    "Failed to retrieve a file descriptor for the running container's seccomp filter");
                return;
            }
            worker->Result(ret);
        },
        AsyncPromise<int>::NumberWrapper);
    return worker->Promise();
}

Napi::Value Container::InitPIDFd(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    auto worker = new AsyncPromise<int>(
        deferred,
        [this](AsyncPromise<int> *worker) {
            auto ret = _container->init_pidfd(_container);
            if (ret < 0) {
                worker->Error(
                    "Failed to retrieve a file descriptor for the running container's seccomp filter");
                return;
            }
            worker->Result(ret);
        },
        AsyncPromise<int>::NumberWrapper);
    return worker->Promise();
}

Napi::Value Container::DevptsFd(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    auto worker = new AsyncPromise<int>(
        deferred,
        [this](AsyncPromise<int> *worker) {
            auto ret = _container->init_pidfd(_container);
            if (ret < 0) {
                worker->Error("Failed to retrieve a mount fd for the container's devpts instance");
                return;
            }
            worker->Result(ret);
        },
        AsyncPromise<int>::NumberWrapper);
    return worker->Promise();
}
