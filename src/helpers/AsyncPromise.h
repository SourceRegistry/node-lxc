//
// Created by A.P.A. Slaa (a.p.a.slaa@projectsource.nl) on 1/14/24.
//

#ifndef NODE_LXC_PROMISEWORKER_H
#define NODE_LXC_PROMISEWORKER_H

#include <napi.h>
#include <stdexcept>

template<typename... Args>
class AsyncPromise : public Napi::AsyncWorker {

public:

    static Napi::Value UndefinedWrapper(AsyncPromise *worker, const std::tuple<Args...> &) {
        return worker->Env().Undefined();
    }

    static Napi::Value NullWrapper(AsyncPromise *worker, const std::tuple<Args...> &) {
        return worker->Env().Null();
    }

    static Napi::String StdStringWrapper(AsyncPromise<std::string> *worker, const std::tuple<std::string> &tuple) {
        return Napi::String::New(worker->Env(), std::get<0>(tuple));
    }

    static Napi::Array StringArrayWrapper(AsyncPromise<char **> *worker, const std::tuple<char **> &tuple) {
        auto array = Napi::Array::New(worker->Env());
        auto values = std::get<0>(tuple);
        if (values != nullptr) {
            // Iterate over the array of strings obtained.
            for (int i = 0; values[i] != nullptr; ++i) {
                // Convert each string to a Napi::Object Type and add it to the Napi::Array.
                array[i] = Napi::String::New(worker->Env(), values[i]);
            }
            // Free the memory allocated for the array of strings.
            free(values);
        }
        return array;
    }

    static Napi::String CharStringWrapper(AsyncPromise<char *> *worker, const std::tuple<char *> &tuple) {
        return Napi::String::New(worker->Env(), std::get<0>(tuple));
    }

    static Napi::String
    SizeCharStringWrapper(AsyncPromise<char *, size_t> *worker, const std::tuple<char *, size_t> &tuple) {
        return Napi::String::New(worker->Env(), std::get<0>(tuple), std::get<1>(tuple));
    }

    static Napi::Number NumberWrapper(AsyncPromise *worker, const std::tuple<Args...> &tuple) {
        return Napi::Number::New(worker->Env(), std::get<0>(tuple));
    }

    static Napi::Boolean BooleanWrapper(AsyncPromise *worker, const std::tuple<bool> &tuple) {
        return Napi::Boolean::New(worker->Env(), std::get<0>(tuple));
    }

public:
    AsyncPromise(
            Napi::Env &env,
            std::function<void(AsyncPromise<Args...> *)> &&asyncFunction,
            std::function<Napi::Value(AsyncPromise<Args...> *, std::tuple<Args...>)> &&valueWrapper,
            void *data = nullptr
    ) :
            Napi::AsyncWorker(env),
            deferred_(Napi::Promise::Deferred::New(env)),
            asyncFunction_(std::move(asyncFunction)),
            valueWrapper_(std::move(valueWrapper)),
            data_(data) {}

    AsyncPromise(
            const Napi::Promise::Deferred &deferred,
            std::function<void(AsyncPromise<Args...> *)> &&asyncFunction,
            std::function<Napi::Value(AsyncPromise<Args...> *, std::tuple<Args...>)> &&valueWrapper,
            void *data = nullptr
    ) :
            Napi::AsyncWorker(deferred.Env()),
            deferred_(deferred),
            asyncFunction_(std::move(asyncFunction)),
            valueWrapper_(std::move(valueWrapper)),
            data_(data) {}

    AsyncPromise(
            const Napi::Promise::Deferred &deferred,
            std::function<void(AsyncPromise *)> &&asyncFunction,
            void *data = nullptr
    ) :
            Napi::AsyncWorker(deferred.Env()), deferred_(deferred), asyncFunction_(std::move(asyncFunction)),
            data_(data) {}

    void Execute() override {
        try {
            asyncFunction_(this);
        } catch (const Napi::Error &e) {
            SetError(e.Message());
        } catch (const std::exception &e) {
            SetError(e.what());
        }
    }

    void Result(Args... args) {
        val_ = std::make_tuple(std::forward<Args>(args)...);
    }

    void Error(const std::string &error) {
        SetError(error);
    }

    void OnOK() override {
        Napi::HandleScope scope(Env());
        deferred_.Resolve(this->valueWrapper_(this, val_));
    }

    void OnError(const Napi::Error &e) override {
        Napi::HandleScope scope(Env());
        deferred_.Reject(e.Value());
    }

    void *data() {
        return this->data_;
    }

    Napi::Promise Promise(bool queue = true) {
        if (queue) this->Queue();
        return deferred_.Promise();
    }

private:
    Napi::Promise::Deferred deferred_;
    std::function<void(AsyncPromise *)> asyncFunction_;
    std::function<Napi::Value(AsyncPromise *,
                              std::tuple<Args...>)> valueWrapper_ = AsyncPromise::UndefinedWrapper;
    std::tuple<Args...> val_;
    void *data_;
};


#endif //NODE_LXC_PROMISEWORKER_H
