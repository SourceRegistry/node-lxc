/**
 * @author A.P.A. Slaa
 * @date 16-12-2023
 * @email a.p.a.slaa@projectsource.nl
 * @company ProjectSource V.O.F.
 */

#ifndef NODE_LXC_CONTAINER_H
#define NODE_LXC_CONTAINER_H

#include <napi.h>
#include <lxc/lxccontainer.h>
#include <lxc/version.h>

// Version check macro
#define VERSION_AT_LEAST(major, minor, micro)                      \
    ((LXC_VERSION_MAJOR > (major)) ||                              \
     (LXC_VERSION_MAJOR == (major) && LXC_VERSION_MINOR > (minor)) || \
     (LXC_VERSION_MAJOR == (major) && LXC_VERSION_MINOR == (minor) && LXC_VERSION_MICRO >= (micro)))

// Compatibility for older LXC versions
#if !VERSION_AT_LEAST(4, 0, 9) && !defined(LXC_ATTACH_SETGROUPS)
typedef struct lxc_groups_t {
    size_t size;
    gid_t* list;
} lxc_groups_t;
#endif

// Node.js binding for an LXC container
class Container : public Napi::ObjectWrap<Container> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::Object New(Napi::Env env, const std::initializer_list<napi_value>& args);

    explicit Container(const Napi::CallbackInfo& info);
    ~Container() override;

private:
    // Metadata and state
    Napi::Value GetError(const Napi::CallbackInfo& info);
    Napi::Value GetName(const Napi::CallbackInfo& info);
    void SetName(const Napi::CallbackInfo& info, const Napi::Value& value);
    Napi::Value GetState(const Napi::CallbackInfo& info);
    Napi::Value GetRunning(const Napi::CallbackInfo& info);
    Napi::Value GetInitPID(const Napi::CallbackInfo& info);

    // Config management
    Napi::Value GetConfigFileName(const Napi::CallbackInfo& info);
    Napi::Value GetDaemonize(const Napi::CallbackInfo& info);
    void SetDaemonize(const Napi::CallbackInfo& info, const Napi::Value& value);
    Napi::Value GetConfigPath(const Napi::CallbackInfo& info);
    void SetConfigPath(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetConfigItem(const Napi::CallbackInfo& info);
    void ClearConfig(const Napi::CallbackInfo& info);
    void ClearConfigItem(const Napi::CallbackInfo& info);
    Napi::Value GetConfigItem(const Napi::CallbackInfo& info);
    Napi::Value GetRunningConfigItem(const Napi::CallbackInfo& info);
    Napi::Value GetKeys(const Napi::CallbackInfo& info);
    Napi::Value LoadConfig(const Napi::CallbackInfo& info);
    Napi::Value Save(const Napi::CallbackInfo& info);

    // Lifecycle operations
    Napi::Value Start(const Napi::CallbackInfo& info);
    Napi::Value Stop(const Napi::CallbackInfo& info);
    Napi::Value Reboot(const Napi::CallbackInfo& info);
    Napi::Value Shutdown(const Napi::CallbackInfo& info);
    Napi::Value Destroy(const Napi::CallbackInfo& info);
    Napi::Value Wait(const Napi::CallbackInfo& info);
    Napi::Value WantCloseAllFds(const Napi::CallbackInfo& info);

    // Cloning and snapshotting
    Napi::Value Clone(const Napi::CallbackInfo& info);
    Napi::Value Snapshot(const Napi::CallbackInfo& info);
    Napi::Value SnapshotList(const Napi::CallbackInfo& info);
    Napi::Value SnapshotRestore(const Napi::CallbackInfo& info);
    Napi::Value SnapshotDestroy(const Napi::CallbackInfo& info);

    // Network and devices
    Napi::Value AttachInterface(const Napi::CallbackInfo& info);
    Napi::Value DetachInterface(const Napi::CallbackInfo& info);
    Napi::Value AddDeviceNode(const Napi::CallbackInfo& info);
    Napi::Value RemoveDeviceNode(const Napi::CallbackInfo& info);
    Napi::Value GetInterfaces(const Napi::CallbackInfo& info);
    Napi::Value GetIPs(const Napi::CallbackInfo& info);

    // CGroup
    Napi::Value GetCGroupItem(const Napi::CallbackInfo& info);
    void SetCGroupItem(const Napi::CallbackInfo& info);

    // Filesystems
    Napi::Value Mount(const Napi::CallbackInfo& info);
    Napi::Value Umount(const Napi::CallbackInfo& info);

    // Freezing
    Napi::Value GetDefined(const Napi::CallbackInfo& info);
    Napi::Value Freeze(const Napi::CallbackInfo& info);
    Napi::Value Unfreeze(const Napi::CallbackInfo& info);

    // Attach and Exec
    Napi::Value Attach(const Napi::CallbackInfo& info);
    Napi::Value Exec(const Napi::CallbackInfo& info);

    // Console and debug
    Napi::Value Console(const Napi::CallbackInfo& info);
    Napi::Value ConsoleLog(const Napi::CallbackInfo& info);
    Napi::Value ConsoleGetFds(const Napi::CallbackInfo& info);

    // Migration
    Napi::Value Checkpoint(const Napi::CallbackInfo& info);
    Napi::Value Restore(const Napi::CallbackInfo& info);
    Napi::Value Migrate(const Napi::CallbackInfo& info);

    // Misc
    Napi::Value SeccompNotifyFd(const Napi::CallbackInfo& info);
    Napi::Value SeccompNotifyFdActive(const Napi::CallbackInfo& info);
    Napi::Value InitPIDFd(const Napi::CallbackInfo& info);
    Napi::Value DevptsFd(const Napi::CallbackInfo& info);

    // Underlying LXC container
    struct lxc_container* _container;
};

#endif // NODE_LXC_CONTAINER_H
