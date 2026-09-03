//
// Created by A.P.A. Slaa (a.p.a.slaa@projectsource.nl) on 1/14/24.
// Optimized version
//

#ifndef NODE_LXC_PROMISEWORKER_H
#define NODE_LXC_PROMISEWORKER_H

#include <napi.h>
#include <functional>
#include <tuple>
#include <string>
#include <memory>

template<typename... Args>
class AsyncPromise : public Napi::AsyncWorker {
public:
    // Static wrapper functions with const references where possible
    static Napi::Value UndefinedWrapper(const AsyncPromise *worker, const std::tuple<Args...> &) {
        return worker->Env().Undefined();
    }

    static Napi::Value NullWrapper(const AsyncPromise *worker, const std::tuple<Args...> &) {
        return worker->Env().Null();
    }

    static Napi::String StdStringWrapper(const AsyncPromise<std::string> *worker,
                                         const std::tuple<std::string> &tuple) {
        return Napi::String::New(worker->Env(), std::get<0>(tuple));
    }

    static Napi::Array StringArrayWrapper(const AsyncPromise<char **> *worker, const std::tuple<char **> &tuple) {
        auto array = Napi::Array::New(worker->Env());
        auto values = std::get<0>(tuple);
        if (values != nullptr) {
            for (int i = 0; values[i] != nullptr; ++i) {
                array[i] = Napi::String::New(worker->Env(), values[i]);
                free(values[i]);
            }
            free(values);
        }
        return array;
    }

    static Napi::String CharStringWrapper(const AsyncPromise<char *> *worker, const std::tuple<char *> &tuple) {
        return Napi::String::New(worker->Env(), std::get<0>(tuple));
    }

    static Napi::Value SizeCharStringWrapper(const AsyncPromise<char *, size_t> *worker,
                                             const std::tuple<char *, size_t> &tuple) {
        char *data = std::get<0>(tuple);
        size_t len = std::get<1>(tuple);
        Napi::Value result = Napi::String::New(worker->Env(), data, len);
        free(data);
        return result;
    }

    static Napi::Number NumberWrapper(const AsyncPromise *worker, const std::tuple<Args...> &tuple) {
        return Napi::Number::New(worker->Env(), std::get<0>(tuple));
    }

    static Napi::Boolean BooleanWrapper(const AsyncPromise *worker, const std::tuple<bool> &tuple) {
        return Napi::Boolean::New(worker->Env(), std::get<0>(tuple));
    }

    // Constructor using perfect forwarding for std::function arguments
    AsyncPromise(
        const Napi::Env &env,
        std::function<void(AsyncPromise<Args...> *)> asyncFunction,
        std::function<Napi::Value(const AsyncPromise<Args...> *, const std::tuple<Args...> &)> valueWrapper =
                UndefinedWrapper,
        void *data = nullptr
    ) : Napi::AsyncWorker(env),
        deferred_(Napi::Promise::Deferred::New(env)),
        asyncFunction_(std::move(asyncFunction)),
        valueWrapper_(std::move(valueWrapper)),
        data_(data) {
    }

    // Constructor with provided deferred object
    AsyncPromise(
        const Napi::Promise::Deferred &deferred,
        std::function<void(AsyncPromise<Args...> *)> asyncFunction,
        std::function<Napi::Value(const AsyncPromise<Args...> *, const std::tuple<Args...> &)> valueWrapper =
                UndefinedWrapper,
        void *data = nullptr
    ) : Napi::AsyncWorker(deferred.Env()),
        deferred_(deferred),
        asyncFunction_(std::move(asyncFunction)),
        valueWrapper_(std::move(valueWrapper)),
        data_(data) {
    }

    // Execute the async function with proper error handling
    void Execute() override {
        try {
            asyncFunction_(this);
        } catch (const Napi::Error &e) {
            SetError(e.Message());
        } catch (const std::exception &e) {
            SetError(e.what());
        } catch (...) {
            SetError("Unknown error occurred");
        }
    }

    // Set the result using perfect forwarding
    template<typename... ResultArgs>
    void Result(ResultArgs &&... args) {
        val_ = std::make_tuple(std::forward<ResultArgs>(args)...);
    }

    // Set error message
    void Error(const std::string &error) {
        SetError(error);
    }

    // Callback when async work completes successfully
    void OnOK() override {
        Napi::HandleScope scope(Env());
        deferred_.Resolve(valueWrapper_(this, val_));
    }

    // Callback when async work fails
    void OnError(const Napi::Error &e) override {
        Napi::HandleScope scope(Env());
        deferred_.Reject(e.Value());
    }

    // Accessor for data pointer
    void *data() const noexcept {
        return data_;
    }

    // Return promise and optionally queue the work
    Napi::Promise Promise(bool queue = true) {
        if (queue) Queue();
        return deferred_.Promise();
    }

private:
    Napi::Promise::Deferred deferred_;
    std::function<void(AsyncPromise *)> asyncFunction_;
    std::function<Napi::Value(const AsyncPromise *, const std::tuple<Args...> &)> valueWrapper_;
    std::tuple<Args...> val_;
    void *data_;
};

#endif //NODE_LXC_PROMISEWORKER_H
