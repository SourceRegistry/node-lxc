/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 16-12-2023
 */

#include "Container.h"

#include <fcntl.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <poll.h>

#include <sys/wait.h>
#include <sstream>
#include <sys/ioctl.h>
#include <syslimits.h>

#include "./helpers/Array.h"
#include "./helpers/helpers.h"
#include "./helpers/AsyncPromise.h"

// Forward declaration — defined later in this file
int wait_for_pid_status(pid_t pid);

// ============================================================================
// ExecSession — backing struct for execAsync()
// ============================================================================

struct ExecSession {
    int stdout_fd = -1;
    int stderr_fd = -1;
    int done_rd   = -1;
    int done_wr   = -1;
    pid_t pid     = -1;

    std::atomic<bool> cleaned{false};
    std::atomic<bool> exitEmitted{false};
    std::atomic<int>  activePipes{2};
    std::atomic<bool> waitpidDone{false};
    std::atomic<int>  storedExitCode{-1};

    std::mutex mutex;

    uv_poll_t *stdoutPoll = nullptr;
    uv_poll_t *stderrPoll = nullptr;
    uv_poll_t *donePoll   = nullptr;

    std::atomic<bool> stdoutTsfnInit{false};
    std::atomic<bool> stderrTsfnInit{false};
    std::atomic<bool> exitTsfnInit{false};

    Napi::ThreadSafeFunction stdoutTsfn;
    Napi::ThreadSafeFunction stderrTsfn;
    Napi::ThreadSafeFunction exitTsfn;

    Napi::ObjectReference *jsRef = nullptr;

    ~ExecSession() { cleanup(); }

    void emitDataChunk(std::vector<uint8_t> *vec,
                       Napi::ThreadSafeFunction &tsfn,
                       std::atomic<bool> &init) {
        if (!init.load()) { delete vec; return; }
        napi_status st = tsfn.NonBlockingCall(vec,
            [](Napi::Env env, Napi::Function cb, std::vector<uint8_t> *v) {
                if (!v) return;
                if (cb.IsFunction())
                    cb.Call({Napi::Buffer<uint8_t>::New(env, v->data(), v->size())});
                delete v;
            });
        if (st != napi_ok) delete vec;
    }

    void flushPipe(int fd, Napi::ThreadSafeFunction &tsfn, std::atomic<bool> &init) {
        if (fd < 0) return;
        uint8_t buf[4096];
        ssize_t n;
        while ((n = ::read(fd, buf, sizeof(buf))) > 0)
            emitDataChunk(new std::vector<uint8_t>(buf, buf + n), tsfn, init);
    }

    void onPipeEof(int which) {
        auto closePipe = [](uv_poll_t *&poll, int &fd) {
            if (poll) {
                uv_poll_stop(poll);
                auto *h = poll; poll = nullptr;
                uv_close(reinterpret_cast<uv_handle_t *>(h),
                         [](uv_handle_t *hh) { delete reinterpret_cast<uv_poll_t *>(hh); });
            }
            if (fd >= 0) { ::close(fd); fd = -1; }
        };
        if (which == 0) closePipe(stdoutPoll, stdout_fd);
        else            closePipe(stderrPoll, stderr_fd);

        if (activePipes.fetch_sub(1) == 1 && waitpidDone.load())
            tryDoExit();
    }

    void onWaitpidDone(int code) {
        storedExitCode.store(code);
        waitpidDone.store(true);

        if (donePoll) {
            uv_poll_stop(donePoll);
            auto *h = donePoll; donePoll = nullptr;
            uv_close(reinterpret_cast<uv_handle_t *>(h),
                     [](uv_handle_t *hh) { delete reinterpret_cast<uv_poll_t *>(hh); });
        }
        if (done_rd >= 0) { ::close(done_rd); done_rd = -1; }
        if (done_wr >= 0) { ::close(done_wr); done_wr = -1; }

        if (activePipes.load() == 0)
            tryDoExit();
    }

    void tryDoExit() {
        if (exitEmitted.exchange(true)) return;

        // Flush any data still sitting in pipe buffers
        flushPipe(stdout_fd, stdoutTsfn, stdoutTsfnInit);
        flushPipe(stderr_fd, stderrTsfn, stderrTsfnInit);

        if (exitTsfnInit.load()) {
            int code = storedExitCode.load();
            auto *cp = new int(code);
            exitTsfn.NonBlockingCall(cp,
                [](Napi::Env env, Napi::Function cb, int *c) {
                    if (cb.IsFunction()) cb.Call({Napi::Number::New(env, *c)});
                    delete c;
                });
        }

        cleanup();
    }

    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex);
        if (cleaned.exchange(true)) return;

        auto closePoll = [](uv_poll_t *&h) {
            if (!h) return;
            uv_poll_stop(h);
            auto *p = h; h = nullptr;
            uv_close(reinterpret_cast<uv_handle_t *>(p),
                     [](uv_handle_t *hp) { delete reinterpret_cast<uv_poll_t *>(hp); });
        };
        closePoll(stdoutPoll);
        closePoll(stderrPoll);
        closePoll(donePoll);

        auto closeFd = [](int &fd) { if (fd >= 0) { ::close(fd); fd = -1; } };
        closeFd(stdout_fd);
        closeFd(stderr_fd);
        closeFd(done_rd);
        closeFd(done_wr);

        if (stdoutTsfnInit.exchange(false)) stdoutTsfn.Release();
        if (stderrTsfnInit.exchange(false)) stderrTsfn.Release();
        if (exitTsfnInit.exchange(false))   exitTsfn.Release();

        if (jsRef) { jsRef->Reset(); delete jsRef; jsRef = nullptr; }
    }

    bool isClosed() const { return cleaned.load(); }
};

// ============================================================================
// ExecAsyncWorker — runs lxc_attach off the main thread for execAsync()
// ============================================================================

class ExecAsyncWorker : public Napi::AsyncWorker {
public:
    ExecAsyncWorker(const Napi::Promise::Deferred &deferred,
                    lxc_container *container,
                    lxc_attach_options_t *opts,
                    lxc_attach_command_t *command,
                    char **argv, uint32_t argvLen,
                    uint32_t envVarsLen, uint32_t keepEnvLen)
        : Napi::AsyncWorker(deferred.Env()),
          deferred_(deferred), _container(container),
          opts_(opts), command_(command),
          argv_(argv), argvLen_(argvLen),
          envVarsLen_(envVarsLen), keepEnvLen_(keepEnvLen) {}

    void Execute() override {
        int devnull = open("/dev/null", O_RDONLY);

        if (pipe(out_pipe_) < 0) {
            SetError("stdout pipe: " + std::string(strerror(errno)));
            if (devnull >= 0) close(devnull);
            freeOpts();
            return;
        }
        if (pipe(err_pipe_) < 0) {
            SetError("stderr pipe: " + std::string(strerror(errno)));
            close(out_pipe_[0]); close(out_pipe_[1]);
            if (devnull >= 0) close(devnull);
            freeOpts();
            return;
        }

        opts_->stdin_fd  = devnull >= 0 ? devnull : 0;
        opts_->stdout_fd = out_pipe_[1];
        opts_->stderr_fd = err_pipe_[1];

        int ret = _container->attach(_container, lxc_attach_run_command, command_, opts_, &pid_);

        close(out_pipe_[1]);
        close(err_pipe_[1]);
        if (devnull >= 0) close(devnull);
        freeOpts();

        if (ret < 0) {
            close(out_pipe_[0]);
            close(err_pipe_[0]);
            SetError("attach: " + std::string(strerror(errno)));
            return;
        }

        fcntl(out_pipe_[0], F_SETFL, O_NONBLOCK);
        fcntl(err_pipe_[0], F_SETFL, O_NONBLOCK);
    }

    void OnOK() override {
        Napi::Env env = Env();
        Napi::HandleScope scope(env);

        int done_pipe[2] = {-1, -1};
        if (pipe(done_pipe) < 0) {
            ::close(out_pipe_[0]);
            ::close(err_pipe_[0]);
            deferred_.Reject(Napi::String::New(env, "done pipe: " + std::string(strerror(errno))));
            return;
        }
        fcntl(done_pipe[0], F_SETFL, O_NONBLOCK);

        auto *session    = new ExecSession();
        session->pid      = pid_;
        session->stdout_fd = out_pipe_[0];
        session->stderr_fd = err_pipe_[0];
        session->done_rd   = done_pipe[0];
        session->done_wr   = done_pipe[1];

        uv_loop_t *loop = uv_default_loop();

        auto startPoll = [&](uv_poll_t *&ref, int fd, uv_poll_cb cb) -> bool {
            auto *p = new uv_poll_t;
            p->data = session;
            if (uv_poll_init(loop, p, fd) < 0 || uv_poll_start(p, UV_READABLE, cb) < 0) {
                delete p;
                return false;
            }
            ref = p;
            return true;
        };

        bool ok = startPoll(session->stdoutPoll, out_pipe_[0],
            [](uv_poll_t *h, int status, int events) {
                auto *sess = static_cast<ExecSession *>(h->data);
                if (!sess || status < 0 || sess->cleaned.load()) return;
                if (!(events & UV_READABLE)) return;
                uint8_t buf[4096];
                while (true) {
                    ssize_t n = ::read(sess->stdout_fd, buf, sizeof(buf));
                    if (n > 0)
                        sess->emitDataChunk(new std::vector<uint8_t>(buf, buf + n),
                                            sess->stdoutTsfn, sess->stdoutTsfnInit);
                    else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
                        { sess->onPipeEof(0); break; }
                    else break;
                }
            });

        ok = ok && startPoll(session->stderrPoll, err_pipe_[0],
            [](uv_poll_t *h, int status, int events) {
                auto *sess = static_cast<ExecSession *>(h->data);
                if (!sess || status < 0 || sess->cleaned.load()) return;
                if (!(events & UV_READABLE)) return;
                uint8_t buf[4096];
                while (true) {
                    ssize_t n = ::read(sess->stderr_fd, buf, sizeof(buf));
                    if (n > 0)
                        sess->emitDataChunk(new std::vector<uint8_t>(buf, buf + n),
                                            sess->stderrTsfn, sess->stderrTsfnInit);
                    else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
                        { sess->onPipeEof(1); break; }
                    else break;
                }
            });

        ok = ok && startPoll(session->donePoll, done_pipe[0],
            [](uv_poll_t *h, int status, int events) {
                auto *sess = static_cast<ExecSession *>(h->data);
                if (!sess || status < 0) return;
                if (!(events & UV_READABLE)) return;
                int code = -1;
                ::read(sess->done_rd, &code, sizeof(code));
                sess->onWaitpidDone(code);
            });

        if (!ok) {
            delete session;
            deferred_.Reject(Napi::String::New(env, "Failed to start polling"));
            return;
        }

        // Waitpid thread — writes exit code to done_pipe[1] then closes it
        pid_t pid = pid_;
        int done_wr = done_pipe[1];
        std::thread([pid, done_wr]() {
            int st = 0;
            waitpid(pid, &st, 0);
            int code = (WIFEXITED(st)) ? WEXITSTATUS(st) : -1;
            ::write(done_wr, &code, sizeof(code));
            ::close(done_wr);
        }).detach();

        // Build JS session object
        Napi::Object obj = Napi::Object::New(env);
        session->jsRef = new Napi::ObjectReference(Napi::Persistent(obj));

        obj.DefineProperty(Napi::PropertyDescriptor::Value(
            "_session", Napi::External<ExecSession>::New(env, session)));

        obj.DefineProperty(Napi::PropertyDescriptor::Accessor(
            "closed",
            [](const Napi::CallbackInfo &ci) -> Napi::Value {
                auto ext = ci.This().As<Napi::Object>().Get("_session");
                if (!ext.IsExternal()) return Napi::Boolean::New(ci.Env(), true);
                return Napi::Boolean::New(ci.Env(),
                    ext.As<Napi::External<ExecSession>>().Data()->isClosed());
            }));

        obj.Set("on", Napi::Function::New(env, [session](const Napi::CallbackInfo &ci) -> Napi::Value {
            Napi::Env e = ci.Env();
            if (ci.Length() != 2 || !ci[0].IsString() || !ci[1].IsFunction()) {
                Napi::TypeError::New(e, "on(event, callback)").ThrowAsJavaScriptException();
                return e.Undefined();
            }
            if (session->isClosed()) {
                Napi::Error::New(e, "Session is closed").ThrowAsJavaScriptException();
                return e.Undefined();
            }
            std::string event = ci[0].ToString().Utf8Value();
            Napi::Function cb  = ci[1].As<Napi::Function>();

            auto makeTsfn = [&](Napi::ThreadSafeFunction &tsfn,
                                 std::atomic<bool> &init,
                                 const char *name) -> bool {
                if (init.load()) {
                    Napi::Error::New(e, std::string(name) + " listener already registered")
                        .ThrowAsJavaScriptException();
                    return false;
                }
                tsfn = Napi::ThreadSafeFunction::New(e, cb, name, 0, 1, [](Napi::Env) {});
                init = true;
                return true;
            };

            if      (event == "stdout") makeTsfn(session->stdoutTsfn, session->stdoutTsfnInit, "ExecStdout");
            else if (event == "stderr") makeTsfn(session->stderrTsfn, session->stderrTsfnInit, "ExecStderr");
            else if (event == "exit")   makeTsfn(session->exitTsfn,   session->exitTsfnInit,   "ExecExit");
            else {
                Napi::TypeError::New(e, "Supported events: 'stdout', 'stderr', 'exit'")
                    .ThrowAsJavaScriptException();
            }
            return ci.This();
        }));

        obj.Set("kill", Napi::Function::New(env, [session](const Napi::CallbackInfo &ci) -> Napi::Value {
            if (!session->isClosed() && session->pid > 0) {
                int sig = (ci.Length() > 0 && ci[0].IsNumber())
                          ? ci[0].ToNumber().Int32Value() : SIGTERM;
                ::kill(session->pid, sig);
            }
            return ci.Env().Undefined();
        }));

        obj.AddFinalizer([](Napi::Env, ExecSession *s) {
            if (s) { s->cleanup(); delete s; }
        }, session);

        deferred_.Resolve(obj);
    }

    void OnError(const Napi::Error &e) override {
        Napi::HandleScope scope(Env());
        deferred_.Reject(e.Value());
    }

private:
    Napi::Promise::Deferred deferred_;
    lxc_container           *_container;
    lxc_attach_options_t    *opts_;
    lxc_attach_command_t    *command_;
    char                   **argv_;
    uint32_t                 argvLen_, envVarsLen_, keepEnvLen_;
    pid_t                    pid_ = -1;
    int                      out_pipe_[2] = {-1, -1};
    int                      err_pipe_[2] = {-1, -1};

    void freeOpts() {
        free(opts_->initial_cwd);
        Array::free(opts_->extra_env_vars, envVarsLen_);
        Array::free(opts_->extra_keep_env, keepEnvLen_);
        free(opts_->lsm_label);
#if VERSION_AT_LEAST(4, 0, 9)
        delete[] opts_->groups.list;
#endif
        free(opts_);
        Array::free(argv_, argvLen_);
        free(command_);
    }
};

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
                            InstanceMethod("consoleGetFds", &Container::ConsoleGetFd),
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
                            InstanceMethod("execOutput", &Container::ExecOutput),
                            InstanceMethod("execAsync", &Container::ExecAsync),

                            InstanceMethod("setTimeout", &Container::SetTimeout),
                            InstanceMethod("mayControl", &Container::MayControl),
                            InstanceMethod("getConfigItems", &Container::GetConfigItems),
                            InstanceMethod("stats", &Container::Stats),

                            //Custom Enhancements
                            InstanceMethod("consoleAsync", &Container::ConsoleAsync),

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
    obj.Set("string", _container->error_string
        ? Napi::String::New(info.Env(), _container->error_string).As<Napi::Value>()
        : info.Env().Null());
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
    auto worker = new AsyncPromise<>(deferred, [this, alt_file](AsyncPromise<> *worker) {
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
                                                      : bdev_spec_rbd_rbdname.c_str()),

                    .rbdpool = const_cast<char *>(bdev_spec_rbd_rbdpool.empty()
                                                      ? nullptr
                                                      : bdev_spec_rbd_rbdpool.c_str()),
                }
            };
            if (!_container->create(_container, template_.c_str(), bdevtype.c_str(), &specs, flags, argv)) {
                std::string msg = _container->error_string
                    ? std::string(_container->error_string)
                    : strerror(_container->error_num ? _container->error_num : errno);
                worker->Error(msg);
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
            char buf[256];
            auto ret = _container->get_config_item(_container, "lxc.ephemeral", buf, sizeof(buf));
            bool is_ephemeral = (ret > 0 && strcmp(buf, "1") == 0);

            if (is_ephemeral && _container->is_running(_container)) {
                if (!force) {
                    worker->Error("Cannot destroy running ephemeral container without force");
                    return;
                }
                if (!_container->stop(_container)) {
                    worker->Error("Failed to stop ephemeral container");
                    return;
                }
                // Ephemeral containers auto-remove on stop
                _container = nullptr;
                return;
            }

            // Only destroy if not ephemeral
            if (_container->is_defined(_container)) {
                if (include_snapshots) {
                    if (!_container->destroy_with_snapshots(_container)) {
                        worker->Error("Destroy failed with snapshots");
                        return;
                    }
                } else {
                    if (!_container->destroy(_container)) {
                        worker->Error("Destroy failed");
                        return;
                    }
                }
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
    int len = _container->get_config_item(_container, info[0].ToString().Utf8Value().c_str(), nullptr, 0);
    if (len <= 0) return info.Env().Null();

    std::unique_ptr<char[]> value(new char[len + 1]);
    if (_container->get_config_item(_container, info[0].ToString().Utf8Value().c_str(), value.get(), len + 1) != len) {
        return info.Env().Null();
    }
    return Napi::String::New(info.Env(), value.get());
}

Napi::Value Container::GetRunningConfigItem(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    check(info.Length() <= 0 || !info[0].IsString(), "Invalid arguments")
    char *result = _container->get_running_config_item(_container, info[0].ToString().Utf8Value().c_str());
    if (!result) return info.Env().Null();
    Napi::String val = Napi::String::New(info.Env(), result);
    free(result);
    return val;
}

Napi::Value Container::GetKeys(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")

    auto keys = Napi::Array::New(info.Env());
    std::string prefixStr;
    if (info[0].IsString()) prefixStr = info[0].ToString().Utf8Value();
    const char *prefix = info[0].IsString() ? prefixStr.c_str() : nullptr;

    int len = _container->get_keys(_container, prefix, nullptr, 0);
    if (len <= 0) {
        return keys;
    }

    std::unique_ptr<char[]> value(new char[len + 1]);
    if (_container->get_keys(_container, prefix, value.get(), len + 1) != len) {
        Napi::Error::New(info.Env(), "Key count mismatch on retrieval").ThrowAsJavaScriptException();
        return keys;
    }

    std::stringstream ss(value.get());
    std::string item;
    int index = 0;
    while (getline(ss, item, '\n')) {
        if (!item.empty()) {
            keys.Set(index++, Napi::String::New(info.Env(), item));
        }
    }
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

    auto subsys = info[0].ToString().Utf8Value();
    int len = _container->get_cgroup_item(_container, subsys.c_str(), nullptr, 0);
    if (len <= 0) {
        return info.Env().Undefined();
    }

    std::unique_ptr<char[]> value(new char[len + 1]);
    if (_container->get_cgroup_item(_container, subsys.c_str(), value.get(), len + 1) != len) {
        return info.Env().Undefined();
    }
    return Napi::String::New(info.Env(), value.get());
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

Napi::Value Container::ConsoleGetFd(const Napi::CallbackInfo &info) {
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
            auto ttyfd = _container->console_getfd(_container, &ttynum, &ptxfd);
            if (ttyfd < 0) {
                worker->Error("Unable to allocate console tty");
                return;
            }
            worker->Result(ttyfd, ptxfd);
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

struct ConsoleSession {
    int ptxfd = -1;
    int ttynum = -1;
    int ttyfd = -1;
    std::atomic<bool> running{false};
    std::atomic<bool> cleaned{false};
    std::atomic<bool> closeEmitted{false};

    std::mutex closeMutex;

    // N-API
    Napi::ThreadSafeFunction tsfn;
    Napi::ThreadSafeFunction closeTsfn;
    std::atomic<bool> tsfnInitialized{false};
    std::atomic<bool> closeTsfnInitialized{false};

    Napi::ObjectReference *containerJsRef = nullptr;
    Napi::ObjectReference *consoleJsRef = nullptr; // Strong ref to keep JS object alive

    // libuv
    uv_poll_t *pollHandle = nullptr;
    pid_t initPid;

    ConsoleSession(Napi::ObjectReference &containerRef, pid_t pid, int tty, int fd)
        : ttynum(tty), ttyfd(fd), initPid(pid) {
        if (!containerRef.IsEmpty()) {
            containerJsRef = new Napi::ObjectReference(Napi::ObjectReference::New(containerRef.Value(), 1));
        }
    }

    ~ConsoleSession() {
        cleanup();  // RAII: always ensure cleanup runs
    }

    void cleanup() {
        std::lock_guard<std::mutex> lock(closeMutex);
        if (cleaned.exchange(true)) return;

        running = false;

        // Emit close event
        if (closeTsfnInitialized.load() && !closeEmitted.exchange(true)) {
            emitClose();
        }

        // Stop polling
        if (pollHandle) {
            uv_poll_stop(pollHandle);
            uv_close(reinterpret_cast<uv_handle_t *>(pollHandle), [](uv_handle_t *h) {
                delete reinterpret_cast<uv_poll_t *>(h);
            });
            pollHandle = nullptr;
        }

        // Release TSFNs
        if (tsfnInitialized.exchange(false)) {
            tsfn.Release();
        }
        if (closeTsfnInitialized.exchange(false)) {
            closeTsfn.Release();
        }

        // Close file descriptors
        if (ptxfd >= 0) {
            close(ptxfd);
            ptxfd = -1;
        }
        if (ttyfd >= 0 && ttyfd != ptxfd) {
            close(ttyfd);
        }
        ttyfd = -1;

        // Release JS references
        if (containerJsRef) {
            containerJsRef->Reset();
            delete containerJsRef;
            containerJsRef = nullptr;
        }
        if (consoleJsRef) {
            consoleJsRef->Reset();
            delete consoleJsRef;
            consoleJsRef = nullptr;
        }
    }

    // Getter for .closed
    bool isClosed() const {
        return cleaned.load();
    }

private:
    void emitClose() {
        if (!closeTsfnInitialized.load()) return;

        closeTsfn.BlockingCall([](Napi::Env, Napi::Function cb) {
            try {
                if (cb.IsFunction()) {
                    cb.Call({});
                }
            } catch (...) {
                // Silent fail
            }
        });
    }
};

Napi::Value Container::ConsoleAsync(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    if (!_container) {
        Napi::TypeError::New(env, "Invalid container").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (info.Length() < 1 || !info[0].IsNumber()) {
        Napi::TypeError::New(env, "Expected TTY number").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    int ttynum = info[0].ToNumber().Int32Value();

    if (!_container->is_running(_container)) {
        Napi::Error::New(env, "Container is not running").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    // Allocate console TTY
    int ptxfd_out = -1;
    int ttyfd = _container->console_getfd(_container, &ttynum, &ptxfd_out);
    if (ttyfd < 0 || ptxfd_out < 0) {
        if (ptxfd_out >= 0) ::close(ptxfd_out);
        if (ttyfd >= 0) ::close(ttyfd);
        Napi::Error::New(env, "Failed to allocate console TTY").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    // Make ptx non-blocking
    if (fcntl(ptxfd_out, F_SETFL, O_NONBLOCK) < 0) {
        ::close(ptxfd_out);
        ::close(ttyfd);
        Napi::Error::New(env, "Failed to set non-blocking mode").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    // Get init PID
    pid_t initPid = _container->init_pid(_container);
    if (initPid <= 0) {
        ::close(ptxfd_out);
        ::close(ttyfd);
        Napi::Error::New(env, "Failed to get container init PID").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    // Create session
    auto thisObj = info.This().As<Napi::Object>();
    Napi::ObjectReference containerRef = Napi::Persistent(thisObj);

    ConsoleSession *session = nullptr;
    try {
        session = new ConsoleSession(containerRef, initPid, ttynum, ttyfd);
        session->ptxfd = ptxfd_out;
    } catch (...) {
        ::close(ptxfd_out);
        ::close(ttyfd);
        delete session;
        Napi::Error::New(env, "Failed to create console session").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    // Create JS console object
    Napi::Object consoleObj = Napi::Object::New(env);

    // 🔥 Hold strong reference to self to prevent GC
    session->consoleJsRef = new Napi::ObjectReference(Napi::Persistent(consoleObj));

    // Store session in external
    consoleObj.DefineProperty(Napi::PropertyDescriptor::Value(
        "_session",
        Napi::External<ConsoleSession>::New(env, session)
    ));

    consoleObj.DefineProperty(Napi::PropertyDescriptor::Accessor(
        "closed",
        [](const Napi::CallbackInfo &info) -> Napi::Value {
            Napi::Value ext = info.This().As<Napi::Object>().Get("_session");
            if (ext.IsUndefined() || !ext.IsExternal()) {
                return Napi::Boolean::New(info.Env(), true);
            }
            ConsoleSession *session = ext.As<Napi::External<ConsoleSession>>().Data();
            return Napi::Boolean::New(info.Env(), session->isClosed());
        }
    ));

    // === .on(event, callback) ===
    consoleObj.Set("on", Napi::Function::New(env, [session](const Napi::CallbackInfo &cbInfo) -> Napi::Value {
        Napi::Env e = cbInfo.Env();

        if (cbInfo.Length() != 2 || !cbInfo[0].IsString() || !cbInfo[1].IsFunction()) {
            Napi::TypeError::New(e, "Usage: on(event, callback)").ThrowAsJavaScriptException();
            return e.Undefined();
        }

        std::string event = cbInfo[0].ToString().Utf8Value();
        Napi::Function callback = cbInfo[1].As<Napi::Function>();

        std::lock_guard lock(session->closeMutex);

        if (session->cleaned.load()) {
            Napi::Error::New(e, "Console session is closed").ThrowAsJavaScriptException();
            return e.Undefined();
        }

        if (event == "data") {
            if (session->running.load() || session->tsfnInitialized.load()) {
                Napi::Error::New(e, "Data listener already registered").ThrowAsJavaScriptException();
                return e.Undefined();
            }

            try {
                session->tsfn = Napi::ThreadSafeFunction::New(
                    e, callback, "ConsoleData", 0, 1,
                    [](Napi::Env) {}
                );
                session->tsfnInitialized = true;
            } catch (const Napi::Error &err) {
                Napi::Error::New(e, "Failed to create ThreadSafeFunction for 'data'").ThrowAsJavaScriptException();
                return e.Undefined();
            }

            // Setup uv_poll
            session->pollHandle = new uv_poll_t;
            session->pollHandle->data = session;

            uv_loop_t *loop = uv_default_loop();
            int result = uv_poll_init(loop, session->pollHandle, session->ptxfd);
            if (result < 0) {
                session->tsfnInitialized = false;
                session->tsfn.Release();
                delete session->pollHandle;
                session->pollHandle = nullptr;
                Napi::Error::New(e, "Failed to initialize UV poll handle").ThrowAsJavaScriptException();
                return e.Undefined();
            }

            result = uv_poll_start(session->pollHandle, UV_READABLE,
                [](uv_poll_t *handle, int status, int events) {
                    ConsoleSession *sess = static_cast<ConsoleSession *>(handle->data);
                    if (!sess || status < 0 || sess->cleaned.load()) {
                        return;
                    }

                    if (events & UV_READABLE) {
                        uint8_t buffer[4096];
                        ssize_t n = read(sess->ptxfd, buffer, sizeof(buffer));

                        if (n > 0) {
                            auto vec = new std::vector<uint8_t>(buffer, buffer + n);

                            // Avoid calling TSFN if not initialized
                            if (!sess->tsfnInitialized.load()) {
                                delete vec;
                                return;
                            }

                            napi_status stat = sess->tsfn.NonBlockingCall(vec,
                                [](Napi::Env env, Napi::Function jsCallback, std::vector<uint8_t> *vec) {
                                    if (!vec || !jsCallback.IsFunction()) {
                                        delete vec;
                                        return;
                                    }
                                    Napi::Buffer<uint8_t> nodeBuf = Napi::Buffer<uint8_t>::New(env, vec->data(), vec->size());
                                    jsCallback.Call({nodeBuf});
                                    delete vec;
                                }
                            );

                            if (stat != napi_ok) {
                                sess->cleanup();
                            }
                        } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                            sess->cleanup();
                        }
                    }
                }
            );

            if (result < 0) {
                uv_close(reinterpret_cast<uv_handle_t *>(session->pollHandle), [](uv_handle_t *h) {
                    delete reinterpret_cast<uv_poll_t *>(h);
                });
                session->pollHandle = nullptr;
                session->tsfnInitialized = false;
                session->tsfn.Release();
                Napi::Error::New(e, "Failed to start polling").ThrowAsJavaScriptException();
                return e.Undefined();
            }

            session->running = true;
            return cbInfo.This();
        }

        if (event == "close") {
            if (session->closeTsfnInitialized.load()) {
                Napi::Error::New(e, "Close listener already registered").ThrowAsJavaScriptException();
                return e.Undefined();
            }

            try {
                session->closeTsfn = Napi::ThreadSafeFunction::New(e, callback, "ConsoleClose", 0, 1);
                session->closeTsfnInitialized = true;
            } catch (const Napi::Error &err) {
                Napi::Error::New(e, "Failed to create ThreadSafeFunction for 'close'").ThrowAsJavaScriptException();
                return e.Undefined();
            }

            return cbInfo.This();
        }

        Napi::TypeError::New(e, "Only 'data' and 'close' events are supported").ThrowAsJavaScriptException();
        return e.Undefined();
    }));

    // === .write(data) ===
    consoleObj.Set("write", Napi::Function::New(env, [session](const Napi::CallbackInfo &cbInfo) -> Napi::Value {
        Napi::Env e = cbInfo.Env();

        if (cbInfo.Length() < 1) {
            Napi::TypeError::New(e, "Expected data to write").ThrowAsJavaScriptException();
            return e.Undefined();
        }

        std::lock_guard<std::mutex> lock(session->closeMutex);
        if (session->cleaned.load() || session->ptxfd < 0) {
            Napi::Error::New(e, "Console is closed").ThrowAsJavaScriptException();
            return e.Undefined();
        }

        std::vector<uint8_t> data;
        Napi::Value arg = cbInfo[0];

        if (arg.IsString()) {
            std::string str = arg.ToString().Utf8Value();
            data.assign(str.begin(), str.end());
        } else if (arg.IsBuffer()) {
            auto buf = arg.As<Napi::Buffer<uint8_t>>();
            data.assign(buf.Data(), buf.Data() + buf.Length());
        } else {
            Napi::TypeError::New(e, "write() expects string or buffer").ThrowAsJavaScriptException();
            return e.Undefined();
        }

        if (data.empty()) return e.Undefined();

        const uint8_t *ptr = data.data();
        size_t remaining = data.size();
        ssize_t written = 0;

        while (remaining > 0) {
            ssize_t n = write(session->ptxfd, ptr + written, remaining);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                Napi::Error::New(e, "Write failed: " + std::string(strerror(errno))).ThrowAsJavaScriptException();
                return e.Undefined();
            }
            written += n;
            remaining -= n;
        }

        return e.Undefined();
    }));

    // === .close() ===
    consoleObj.Set("close", Napi::Function::New(env, [session](const Napi::CallbackInfo &cbInfo) -> Napi::Value {
        session->cleanup();
        return cbInfo.Env().Undefined();
    }));

    // === .resize(cols, rows) ===
    consoleObj.Set("resize", Napi::Function::New(env, [session, this](const Napi::CallbackInfo &info) -> Napi::Value {
        Napi::Env e = info.Env();

        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            Napi::TypeError::New(e, "Usage: resize(cols, rows)").ThrowAsJavaScriptException();
            return e.Undefined();
        }

        int cols = info[0].ToNumber().Int32Value();
        int rows = info[1].ToNumber().Int32Value();

        if (cols <= 0 || rows <= 0) {
            Napi::TypeError::New(e, "Columns and rows must be positive").ThrowAsJavaScriptException();
            return e.Undefined();
        }

        std::lock_guard<std::mutex> lock(session->closeMutex);
        if (session->cleaned.load() || session->ptxfd < 0) {
            Napi::Error::New(e, "Console is closed").ThrowAsJavaScriptException();
            return e.Undefined();
        }

        struct winsize ws = {};
        ws.ws_row = static_cast<unsigned short>(rows);
        ws.ws_col = static_cast<unsigned short>(cols);

        if (ioctl(session->ptxfd, TIOCSWINSZ, &ws) == -1) {
            Napi::Error::New(e, "TIOCSWINSZ failed: " + std::string(strerror(errno))).ThrowAsJavaScriptException();
            return e.Undefined();
        }

        pid_t initPid = _container->init_pid(_container);
        if (initPid > 0) {
            kill(initPid, SIGWINCH);
        }

        return e.Undefined();
    }));

    // 🔥 Finalizer: Called when JS object is GC'd — ONLY PLACE THAT DELETES
    consoleObj.AddFinalizer([](Napi::Env, ConsoleSession *s) {
        if (s) {
            s->cleanup();   // Idempotent: safe if already closed
            delete s;       // Only deletion point
        }
    }, session);

    return consoleObj;
}

// ============================================================================
// ExecOutput — capture stdout/stderr and return { exitCode, stdout, stderr }
// ============================================================================

// Shared option parsing helper (fills opts, leaves stdin/stdout/stderr for caller)
static void parseExecOpts(const Napi::Object &options,
                           lxc_attach_options_t *opts,
                           uint32_t &envVarsLen,
                           uint32_t &keepEnvLen) {
    opts->attach_flags = opt_obj_val("attach_flags", ToNumber().Int32Value(), LXC_ATTACH_DEFAULT);
    opts->namespaces   = opt_obj_val("namespaces",   ToNumber().Int32Value(), -1);
    opts->personality  = opt_obj_val("personality",  As<Napi::BigInt>().Int64Value(nullptr),
                                     LXC_ATTACH_DETECT_PERSONALITY);
    opts->initial_cwd  = (char *)opt_strdup_val_checked("initial_cwd", nullptr);
    opts->uid          = opt_obj_val("uid", ToNumber().Uint32Value(), (uid_t)-1);
    opts->gid          = opt_obj_val("gid", ToNumber().Uint32Value(), (gid_t)-1);
    opts->env_policy   = (lxc_attach_env_policy_t)opt_obj_val("env_policy",
                          ToNumber().Int32Value(), LXC_ATTACH_CLEAR_ENV);
    opts->log_fd       = opt_obj_val("log_fd", ToNumber().Int32Value(), -EBADF);
    opts->lsm_label    = opt_strdup_val_checked("lsm_label", nullptr);
    opts->stdin_fd     = 0;
    opts->stdout_fd    = 1;
    opts->stderr_fd    = 2;

    if (opt_has_val_checked("extra_env_vars", IsArray()))
        opts->extra_env_vars = Array::NapiToCharStarArray(
            options.Get("extra_env_vars").As<Napi::Array>(), envVarsLen);
    else
        opts->extra_env_vars = nullptr;

    if (opt_has_val_checked("extra_keep_env", IsArray()) && opts->env_policy == LXC_ATTACH_CLEAR_ENV)
        opts->extra_keep_env = Array::NapiToCharStarArray(
            options.Get("extra_keep_env").As<Napi::Array>(), keepEnvLen);
    else
        opts->extra_keep_env = nullptr;

#if VERSION_AT_LEAST(4, 0, 9)
    if (opt_has_val_checked("groups", IsArray())) {
        auto jsG = options.Get("groups").As<Napi::Array>();
        lxc_groups_t g = { .size = jsG.Length(),
                            .list = jsG.Length() > 0 ? new gid_t[jsG.Length()] : nullptr };
        if (g.list)
            for (size_t i = 0; i < jsG.Length(); ++i)
                g.list[i] = jsG.Get(i).ToNumber().Uint32Value();
        opts->groups = g;
    } else {
        opts->groups = {};
    }
#endif
}

static void freeExecOpts(lxc_attach_options_t *opts,
                          uint32_t envVarsLen, uint32_t keepEnvLen) {
    free(opts->initial_cwd);
    Array::free(opts->extra_env_vars, envVarsLen);
    Array::free(opts->extra_keep_env, keepEnvLen);
    free(opts->lsm_label);
#if VERSION_AT_LEAST(4, 0, 9)
    delete[] opts->groups.list;
#endif
    free(opts);
}

Napi::Value Container::ExecOutput(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsObject(), "Invalid arguments")

    auto *opts = (lxc_attach_options_t *)malloc(sizeof(lxc_attach_options_t));
    uint32_t envVarsLen = 0, keepEnvLen = 0, argvLen = 0;

    parseExecOpts(info[0].ToObject(), opts, envVarsLen, keepEnvLen);

    auto options = info[0].ToObject();
    if (!options.Has("argv") || !options.Get("argv").IsArray()) {
        deferred.Reject(Napi::String::New(info.Env(), "execOutput requires argv array"));
        freeExecOpts(opts, envVarsLen, keepEnvLen);
        return deferred.Promise();
    }

    auto *argv = Array::NapiToCharStarArray(options.Get("argv").As<Napi::Array>(), argvLen);
    if (argvLen == 0 || !argv || !argv[0]) {
        Array::free(argv, argvLen);
        deferred.Reject(Napi::String::New(info.Env(), "execOutput requires at least one argv entry"));
        freeExecOpts(opts, envVarsLen, keepEnvLen);
        return deferred.Promise();
    }

    auto *command    = (lxc_attach_command_t *)malloc(sizeof(lxc_attach_command_t));
    command->program = argv[0];
    command->argv    = argv;

    using Result = AsyncPromise<int, std::string, std::string>;

    auto *worker = new Result(
        deferred,
        [this, opts, command, argv, argvLen, envVarsLen, keepEnvLen](Result *w) {
            int out_pipe[2] = {-1, -1}, err_pipe[2] = {-1, -1};
            int devnull = open("/dev/null", O_RDONLY);

            auto cleanup = [&]() {
                freeExecOpts(opts, envVarsLen, keepEnvLen);
                Array::free(argv, argvLen);
                free(command);
            };

            if (pipe(out_pipe) < 0) {
                w->Error("stdout pipe: " + std::string(strerror(errno)));
                if (devnull >= 0) close(devnull);
                cleanup();
                return;
            }
            if (pipe(err_pipe) < 0) {
                w->Error("stderr pipe: " + std::string(strerror(errno)));
                close(out_pipe[0]); close(out_pipe[1]);
                if (devnull >= 0) close(devnull);
                cleanup();
                return;
            }

            opts->stdin_fd  = devnull >= 0 ? devnull : 0;
            opts->stdout_fd = out_pipe[1];
            opts->stderr_fd = err_pipe[1];

            pid_t pid;
            int ret = _container->attach(_container, lxc_attach_run_command, command, opts, &pid);

            close(out_pipe[1]);
            close(err_pipe[1]);
            if (devnull >= 0) close(devnull);
            cleanup();

            if (ret < 0) {
                close(out_pipe[0]); close(err_pipe[0]);
                w->Error("attach: " + std::string(strerror(errno)));
                return;
            }

            std::string out_str, err_str;

            auto read_all = [](int fd, std::string &dst) {
                char buf[4096]; ssize_t n;
                while ((n = ::read(fd, buf, sizeof(buf))) > 0) dst.append(buf, n);
                ::close(fd);
            };

            std::thread t1(read_all, out_pipe[0], std::ref(out_str));
            std::thread t2(read_all, err_pipe[0], std::ref(err_str));
            t1.join(); t2.join();

            int st = wait_for_pid_status(pid);
            w->Result((st >= 0 && WIFEXITED(st)) ? WEXITSTATUS(st) : -1, out_str, err_str);
        },
        [](const Result *w, const std::tuple<int, std::string, std::string> &t) -> Napi::Value {
            auto obj = Napi::Object::New(w->Env());
            obj.Set("exitCode", Napi::Number::New(w->Env(), std::get<0>(t)));
            obj.Set("stdout",   Napi::String::New(w->Env(), std::get<1>(t)));
            obj.Set("stderr",   Napi::String::New(w->Env(), std::get<2>(t)));
            return obj;
        });

    return worker->Promise();
}

// ============================================================================
// ExecAsync — stream stdout/stderr as events, emit exit on process exit
// ============================================================================

Napi::Value Container::ExecAsync(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 || !info[0].IsObject(), "Invalid arguments")

    auto *opts = (lxc_attach_options_t *)malloc(sizeof(lxc_attach_options_t));
    uint32_t envVarsLen = 0, keepEnvLen = 0, argvLen = 0;

    parseExecOpts(info[0].ToObject(), opts, envVarsLen, keepEnvLen);

    auto options = info[0].ToObject();
    if (!options.Has("argv") || !options.Get("argv").IsArray()) {
        deferred.Reject(Napi::String::New(info.Env(), "execAsync requires argv array"));
        freeExecOpts(opts, envVarsLen, keepEnvLen);
        return deferred.Promise();
    }

    auto *argv = Array::NapiToCharStarArray(options.Get("argv").As<Napi::Array>(), argvLen);
    if (argvLen == 0 || !argv || !argv[0]) {
        Array::free(argv, argvLen);
        deferred.Reject(Napi::String::New(info.Env(), "execAsync requires at least one argv entry"));
        freeExecOpts(opts, envVarsLen, keepEnvLen);
        return deferred.Promise();
    }

    auto *command    = (lxc_attach_command_t *)malloc(sizeof(lxc_attach_command_t));
    command->program = argv[0];
    command->argv    = argv;

    auto *worker = new ExecAsyncWorker(deferred, _container,
                                        opts, command,
                                        argv, argvLen,
                                        envVarsLen, keepEnvLen);
    worker->Queue();
    return deferred.Promise();
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
#if VERSION_AT_LEAST(4, 0, 9)
            delete[] attach_options->groups.list;
#endif
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

    if (!options.Has("argv") || !options.Get("argv").IsArray()) {
        deferred.Reject(Napi::String::New(info.Env(), "exec requires options.argv array"));
        free(attach_options->initial_cwd);
        Array::free(attach_options->extra_env_vars, extra_env_varsLength);
        Array::free(attach_options->extra_keep_env, extra_keep_envLength);
        free(attach_options->lsm_label);
#if VERSION_AT_LEAST(4, 0, 9)
        delete[] attach_options->groups.list;
#endif
        free(attach_options);
        return deferred.Promise();
    }

    auto argv = Array::NapiToCharStarArray(options.Get("argv").As<Napi::Array>(), argvLength);
    if (argvLength == 0 || !argv || !argv[0]) {
        Array::free(argv, argvLength);
        deferred.Reject(Napi::String::New(info.Env(), "exec requires at least one argv entry"));
        free(attach_options->initial_cwd);
        Array::free(attach_options->extra_env_vars, extra_env_varsLength);
        Array::free(attach_options->extra_keep_env, extra_keep_envLength);
        free(attach_options->lsm_label);
#if VERSION_AT_LEAST(4, 0, 9)
        delete[] attach_options->groups.list;
#endif
        free(attach_options);
        return deferred.Promise();
    }

    auto *command = (lxc_attach_command_t *) malloc(sizeof(struct lxc_attach_command_t));
    command->program = argv[0];
    command->argv = argv;

    auto worker = new AsyncPromise<int>(
        deferred,
        [this, attach_options, command, argv, argvLength, extra_env_varsLength, extra_keep_envLength](
    AsyncPromise<int> *worker) {
            pid_t pid;
            int ret = _container->attach(_container, lxc_attach_run_command, command, attach_options, &pid);
            if (ret < 0) {
                worker->Error(strerror(errno));
            } else {
                auto status = wait_for_pid_status(pid);
                int exit_code = 0;
                if (status >= 0 && WIFEXITED(status)) {
                    exit_code = WEXITSTATUS(status);
                }
                worker->Result(exit_code);
            }
            free(attach_options->initial_cwd);
            Array::free(attach_options->extra_env_vars, extra_env_varsLength);
            Array::free(attach_options->extra_keep_env, extra_keep_envLength);
            free(attach_options->lsm_label);
#if VERSION_AT_LEAST(4, 0, 9)
            delete[] attach_options->groups.list;
#endif
            free(attach_options);
            Array::free(argv, argvLength);
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

    auto worker = new AsyncPromise<>(
        deferred,
        [this, src_path, dest_path](AsyncPromise<> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges...");
                return;
            }
            if (!_container->add_device_node(_container, src_path.c_str(),
                                             dest_path.empty() ? nullptr : dest_path.c_str())) {
                worker->Error("Unable to add device node...");
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

    auto worker = new AsyncPromise<>(
        deferred,
        [this, src_path, dest_path](AsyncPromise<> *worker) {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            if (!_container->remove_device_node(_container, src_path.c_str(),
                                                dest_path.empty() ? nullptr : dest_path.c_str())) {
                worker->Error("Unable to remove device node " + src_path + ":" +
                              (dest_path.empty() ? src_path : dest_path) + " from " +
                              std::string(_container->name));
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

    auto worker = new AsyncPromise<>(
        deferred,
        [this, dev, dst_dev](AsyncPromise<> *worker) {
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

    auto worker = new AsyncPromise<>(
        deferred,
        [this, dev, dst_dev](AsyncPromise<> *worker) {
            if (!_container->detach_interface(_container, dev.c_str(), dst_dev.c_str())) {
                worker->Error("Unable to detach interface " + dev + " from " + std::string(_container->name));
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
    check_deferred(info.Length() < 2 || !info[0].IsNumber() || !info[1].IsObject(), "Invalid arguments")

    auto cmd = info[0].ToNumber().Int32Value();
    auto options = info[1].ToObject();

    auto opts = (struct migrate_opts *) calloc(1, sizeof(migrate_opts));

    opts->directory = opt_obj_val("directory", ToString().Utf8Value().data(), nullptr);
    opts->verbose = opt_obj_val("verbose", ToBoolean(), false);
    opts->stop = opt_obj_val("stop", ToBoolean(), false);
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

    auto *read_max = new uint64_t((uint64_t) opt_obj_val("read_max", ToNumber().Int64Value(), 0));

    lxc_console_log log
    {
        .clear = opt_obj_val("clear", ToBoolean(), false),
        .read = opt_obj_val("read", ToBoolean(), false),
        .read_max = read_max,
        .data = nullptr
    };

    auto worker = new AsyncPromise<char *, size_t>(
        deferred,
        [this, log, read_max](AsyncPromise<char *, size_t> *worker) mutable {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                delete read_max;
                return;
            }
            auto ret = _container->console_log(_container, &log);
            if (ret < 0) {
                worker->Error(std::string(strerror(-ret)) + " - Failed to retrieve console log");
            } else {
                worker->Result(log.data, *log.read_max);
            }
            delete read_max;
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
                   !(info[2].IsString() || info[2].IsNull() || info[2].IsUndefined()) || // filesystemtype
                   !(info[3].IsBigInt() | info[3].IsNumber()) || // mountflags
                   !info[4].IsObject(), // lxc_mount
                   "Invalid arguments")

    auto source = info[0].ToString().Utf8Value();
    auto target = info[1].ToString().Utf8Value();
    auto filesystemtype = info[2].IsString() ? info[2].ToString().Utf8Value() : "";
    auto mountflags = info[3].IsBigInt()
                          ? info[3].As<Napi::BigInt>().Uint64Value(nullptr)
                          : (uint64_t) info[3].ToNumber().Int64Value();

    lxc_mount mnt
    {
        .version = info[4].ToObject().Get("version").ToNumber().Int32Value()
    };

    auto worker = new AsyncPromise<>(
        deferred,
        [this, source, target, filesystemtype, mountflags, mnt](AsyncPromise<> *worker) mutable {
            if (!_container->may_control(_container)) {
                worker->Error("Insufficient privileges to control " + std::string(_container->name));
                return;
            }
            const char *fs_type = filesystemtype.empty() ? nullptr : filesystemtype.c_str();
            auto ret = _container->mount(_container, source.c_str(), target.c_str(),
                                         fs_type, mountflags, nullptr, &mnt);
            if (ret < 0) {
                worker->Error(std::string("Failed to mount Error: ") + strerror(ret));
            }
        });
    return worker->Promise();
}

Napi::Value Container::Umount(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")
    check_deferred(info.Length() <= 0 ||
                   !info[0].IsString() || // target
                   !(info[1].IsBigInt() || info[1].IsNumber()) || // mountflags
                   !info[2].IsObject(), // lxc_mount
                   "Invalid arguments")

    auto target = info[0].ToString().Utf8Value();
    auto mountflags = info[1].IsBigInt()
                          ? info[1].As<Napi::BigInt>().Uint64Value(nullptr)
                          : (uint64_t) info[1].ToNumber().Int64Value();
    lxc_mount mnt{
        .version = info[2].ToObject().Get("version").ToNumber().Int32Value()
    };

    auto worker = new AsyncPromise<>(
        deferred,
        [this, target, mountflags, mnt](AsyncPromise<> *worker) mutable {
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
            auto ret = _container->devpts_fd(_container);
            if (ret < 0) {
                worker->Error("Failed to retrieve devpts fd");
                return;
            }
            worker->Result(ret);
        },
        AsyncPromise<int>::NumberWrapper);
    return worker->Promise();
}

Napi::Value Container::SetTimeout(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    check(info.Length() <= 0 || !info[0].IsNumber(), "Invalid arguments")
#ifdef LXC_HAS_SET_TIMEOUT
    int timeout = info[0].ToNumber().Int32Value();
    return Napi::Boolean::New(info.Env(), _container->set_timeout(_container, timeout));
#else
    Napi::TypeError::New(info.Env(), "set_timeout is not available in this LXC build").ThrowAsJavaScriptException();
    return info.Env().Undefined();
#endif
}

Napi::Value Container::MayControl(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")
    return Napi::Boolean::New(info.Env(), _container->may_control(_container));
}

Napi::Value Container::GetConfigItems(const Napi::CallbackInfo &info) {
    assert(_container, "Invalid container pointer")

    std::string prefixStr;
    if (info[0].IsString()) prefixStr = info[0].ToString().Utf8Value();
    const char *prefix = info[0].IsString() ? prefixStr.c_str() : nullptr;
    auto result = Napi::Object::New(info.Env());

    int len = _container->get_keys(_container, prefix, nullptr, 0);
    if (len <= 0) return result;

    std::unique_ptr<char[]> keysBuf(new char[len + 1]);
    if (_container->get_keys(_container, prefix, keysBuf.get(), len + 1) != len) return result;

    std::stringstream ss(keysBuf.get());
    std::string key;
    while (getline(ss, key, '\n')) {
        if (key.empty()) continue;
        int vlen = _container->get_config_item(_container, key.c_str(), nullptr, 0);
        if (vlen <= 0) {
            result.Set(key, info.Env().Null());
            continue;
        }
        std::unique_ptr<char[]> val(new char[vlen + 1]);
        if (_container->get_config_item(_container, key.c_str(), val.get(), vlen + 1) == vlen) {
            result.Set(key, Napi::String::New(info.Env(), val.get()));
        } else {
            result.Set(key, info.Env().Null());
        }
    }
    return result;
}

static std::string readCgroupItem(lxc_container *c, const char *key) {
    int len = c->get_cgroup_item(c, key, nullptr, 0);
    if (len <= 0) return {};
    std::unique_ptr<char[]> buf(new char[len + 1]);
    if (c->get_cgroup_item(c, key, buf.get(), len + 1) != len) return {};
    return std::string(buf.get());
}

using StatsMap = std::vector<std::pair<std::string, std::string>>;

Napi::Value Container::Stats(const Napi::CallbackInfo &info) {
    auto deferred = Napi::Promise::Deferred::New(info.Env());
    assert_deferred(_container, "Invalid container pointer")

    auto worker = new AsyncPromise<StatsMap *>(
        deferred,
        [this](AsyncPromise<StatsMap *> *worker) {
            if (!_container->is_running(_container)) {
                worker->Error(std::string(_container->name) + " not running");
                return;
            }
            static const char *keys[] = {
                "memory.usage_in_bytes",
                "memory.limit_in_bytes",
                "memory.memsw.usage_in_bytes",
                "cpuacct.usage",
                "cpu.stat",
                "blkio.throttle.io_service_bytes",
                nullptr
            };
            auto *stats = new StatsMap();
            for (int i = 0; keys[i]; ++i) {
                stats->emplace_back(keys[i], readCgroupItem(_container, keys[i]));
            }
            worker->Result(stats);
        },
        std::function<Napi::Value(const AsyncPromise<StatsMap *> *, const std::tuple<StatsMap *> &)>{
            [](const AsyncPromise<StatsMap *> *worker, const std::tuple<StatsMap *> &data) -> Napi::Value {
                auto *stats = std::get<0>(data);
                auto result = Napi::Object::New(worker->Env());
                for (const auto &[key, value] : *stats) {
                    if (value.empty()) {
                        result.Set(key, worker->Env().Null());
                    } else {
                        result.Set(key, Napi::String::New(worker->Env(), value));
                    }
                }
                delete stats;
                return result;
            }
        });
    return worker->Promise();
}
