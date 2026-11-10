//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace rebar_ml {

enum class ErrorCode {
    unknown,
    invalid_argument,
    unsupported_format,
    already_connected,
    incompatible_connection,
    graph_cycle,
    graph_in_use,
    invalid_graph,
    node_not_in_pipeline,
    port_not_available,
    input_pending,
    input_closed,
    output_closed,
    cancelled,
    backend_failure,
    not_implemented,
    queue_full,
};

struct Error {
    ErrorCode code = ErrorCode::unknown;
    std::string message;
};

struct Unexpected {
    Error error;
};

inline Unexpected unexpected(Error error) {
    return Unexpected{std::move(error)};
}

template <typename T>
class Result {
public:
    Result(T value) : storage_(std::move(value)) {}
    Result(Unexpected failure) : storage_(std::move(failure.error)) {}

    bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const noexcept { return has_value(); }

    T& value() { return std::get<T>(storage_); }
    const T& value() const { return std::get<T>(storage_); }

    Error& error() { return std::get<Error>(storage_); }
    const Error& error() const { return std::get<Error>(storage_); }

    T& operator*() { return value(); }
    const T& operator*() const { return value(); }
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }

private:
    std::variant<T, Error> storage_;
};

template <>
class Result<void> {
public:
    Result() = default;
    Result(Unexpected failure) : error_(std::move(failure.error)) {}

    bool has_value() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }

    const Error& error() const { return *error_; }
    Error& error() { return *error_; }

private:
    std::optional<Error> error_;
};

enum class TypeId : std::uint8_t {
    unknown = 0,
    text,
    audio,
    image,
    tensor_dict,
    count,
};

using PortId = std::size_t;

constexpr std::size_t kTypeIdCount = static_cast<std::size_t>(TypeId::count);

constexpr std::size_t type_id_index(TypeId id) noexcept {
    return static_cast<std::size_t>(id);
}

constexpr PortId kInvalidPortId = static_cast<PortId>(-1);

template <typename T>
constexpr TypeId type_id_of() {
    static_assert(sizeof(T) == 0, "type_id_of<T>() is not specialized for this type");
    return {};
}

using PayloadDeleter = void (*)(void*);

class AnyPayload {
public:
    AnyPayload() = default;
    AnyPayload(AnyPayload&&) noexcept = default;
    AnyPayload& operator=(AnyPayload&&) noexcept = default;
    AnyPayload(const AnyPayload&) = delete;
    AnyPayload& operator=(const AnyPayload&) = delete;

    TypeId type_id() const noexcept { return type_; }

    explicit operator bool() const noexcept { return static_cast<bool>(storage_); }

private:
    template <typename T>
    friend AnyPayload make_payload(T value);

    template <typename T>
    friend T take_payload(AnyPayload payload);

    template <typename T>
    friend const T& payload_ref(const AnyPayload& payload);

    AnyPayload(TypeId type_value, std::unique_ptr<void, PayloadDeleter> payload_storage)
        : type_(type_value), storage_(std::move(payload_storage)) {}

    TypeId type_{};
    std::unique_ptr<void, PayloadDeleter> storage_{nullptr, +[](void*) {}};
};

template <typename T>
AnyPayload make_payload(T value) {
    using Value = std::remove_cvref_t<T>;
    auto* raw = new Value(std::move(value));
    return AnyPayload{
        type_id_of<Value>(),
        std::unique_ptr<void, PayloadDeleter>(
            raw,
            +[](void* ptr) { delete static_cast<Value*>(ptr); }),
    };
}

template <typename T>
T take_payload(AnyPayload payload) {
    using Value = std::remove_cvref_t<T>;
    const auto expected = type_id_of<Value>();
    assert(payload.type_ == expected && "AnyPayload type mismatch");
    auto* raw = static_cast<Value*>(payload.storage_.get());
    Value out = std::move(*raw);
    payload.storage_.reset();
    return out;
}

template <typename T>
const T& payload_ref(const AnyPayload& payload) {
    using Value = std::remove_cvref_t<T>;
    const auto expected = type_id_of<Value>();
    assert(payload.type_ == expected && "AnyPayload type mismatch");
    return *static_cast<const Value*>(payload.storage_.get());
}

template <typename T>
struct OutputItem {
    T data;
    bool final = true;
};

enum class RetrievalStatus {
    success,
    error,
    cancelled,
};

template <typename T>
struct OutputRetrieval {
    RetrievalStatus status = RetrievalStatus::success;
    std::optional<OutputItem<T>> item;
    std::optional<Error> error;

    static OutputRetrieval success(std::optional<OutputItem<T>> value) {
        OutputRetrieval result;
        result.item = std::move(value);
        return result;
    }

    static OutputRetrieval failure(Error value) {
        OutputRetrieval result;
        result.status = RetrievalStatus::error;
        result.error = std::move(value);
        return result;
    }

    static OutputRetrieval cancelled_result() {
        OutputRetrieval result;
        result.status = RetrievalStatus::cancelled;
        return result;
    }
};

enum class OverflowPolicy {
    block,
    fail,
    drop_oldest,
    drop_newest,
};

struct ConnectionOptions {
    std::size_t queue_capacity = 16;
    OverflowPolicy overflow = OverflowPolicy::block;
};

struct PipelineSessionOptions {
    // Reserved for per-user pipeline session properties.
};

class INode;
class INodeSession;
class IPipelineSession;

class IInputPortAny {
public:
    virtual ~IInputPortAny() = default;
    virtual std::shared_ptr<INode> get_owner() const = 0;
    virtual std::size_t get_index() const noexcept = 0;
    virtual TypeId get_type_id() const noexcept = 0;
    virtual bool is_connected() const noexcept = 0;
};

class IOutputPortAny {
public:
    virtual ~IOutputPortAny() = default;
    virtual std::shared_ptr<INode> get_owner() const = 0;
    virtual std::size_t get_index() const noexcept = 0;
    virtual TypeId get_type_id() const noexcept = 0;
    virtual bool is_connected() const noexcept = 0;
    virtual Result<void> connect(
        const std::shared_ptr<IInputPortAny>& destination,
        const ConnectionOptions& options = {}) = 0;
    virtual Result<void> disconnect() = 0;
};

template <typename T>
class IInputPort : public IInputPortAny {
public:
    TypeId get_type_id() const noexcept override { return type_id_of<T>(); }
};

template <typename T>
class IOutputPort : public IOutputPortAny {
public:
    virtual Result<void> connect(
        const std::shared_ptr<IInputPort<T>>& destination,
        const ConnectionOptions& options = {}) = 0;

    Result<void> connect(
        const std::shared_ptr<IInputPortAny>& destination,
        const ConnectionOptions& options = {}) override {
        auto typed_destination = std::dynamic_pointer_cast<IInputPort<T>>(destination);
        if (!typed_destination) {
            return unexpected(Error{
                ErrorCode::incompatible_connection,
                "destination port is not compatible with this output port",
            });
        }
        return connect(std::move(typed_destination), options);
    }

    TypeId get_type_id() const noexcept override { return type_id_of<T>(); }
};

template <typename T>
class IInputStream {
public:
    virtual ~IInputStream() = default;
    virtual Result<void> add(T value) = 0;
    virtual Result<void> send(T value) = 0;
    virtual bool has_pending_data() const noexcept = 0;
};

class IInputStreamAny {
public:
    virtual ~IInputStreamAny() = default;
    virtual TypeId get_type_id() const noexcept = 0;
    virtual Result<void> add_payload(AnyPayload payload) = 0;
    virtual Result<void> send_payload(AnyPayload payload) = 0;
    virtual bool has_pending_data() const noexcept = 0;
};

template <typename T>
class IOutputStream {
public:
    virtual ~IOutputStream() = default;
    virtual OutputRetrieval<T> receive() = 0;
    virtual void cancel() noexcept = 0;
};

class IOutputStreamAny {
public:
    virtual ~IOutputStreamAny() = default;
    virtual TypeId get_type_id() const noexcept = 0;
    virtual OutputRetrieval<AnyPayload> receive_payload() = 0;
    virtual void cancel() noexcept = 0;
};

namespace detail {

template <typename T>
class TypedInputStream final : public IInputStream<T> {
public:
    explicit TypedInputStream(std::shared_ptr<IInputStreamAny> stream)
        : stream_(std::move(stream)) {}

    Result<void> add(T value) override {
        return stream_->add_payload(make_payload(std::move(value)));
    }

    Result<void> send(T value) override {
        return stream_->send_payload(make_payload(std::move(value)));
    }

    bool has_pending_data() const noexcept override {
        return stream_->has_pending_data();
    }

private:
    std::shared_ptr<IInputStreamAny> stream_;
};

template <typename T>
class TypedOutputStream final : public IOutputStream<T> {
public:
    explicit TypedOutputStream(std::shared_ptr<IOutputStreamAny> stream)
        : stream_(std::move(stream)) {}

    OutputRetrieval<T> receive() override {
        auto received = stream_->receive_payload();
        if (received.status == RetrievalStatus::cancelled) {
            return OutputRetrieval<T>::cancelled_result();
        }
        if (received.status == RetrievalStatus::error) {
            if (received.error) return OutputRetrieval<T>::failure(*received.error);
            return OutputRetrieval<T>::failure(Error{
                ErrorCode::backend_failure,
                "output receive failed",
            });
        }
        if (!received.item) return OutputRetrieval<T>::success(std::nullopt);

        auto& item = *received.item;
        if (item.data.type_id() != type_id_of<T>()) {
            return OutputRetrieval<T>::failure(Error{
                ErrorCode::invalid_graph,
                "output item type does not match output stream type",
            });
        }

        OutputItem<T> typed;
        typed.data = take_payload<T>(std::move(item.data));
        typed.final = item.final;
        return OutputRetrieval<T>::success(std::move(typed));
    }

    void cancel() noexcept override { stream_->cancel(); }

private:
    std::shared_ptr<IOutputStreamAny> stream_;
};

}  // namespace detail

class INode {
public:
    virtual ~INode() = default;
    virtual std::string_view get_name() const noexcept = 0;

    virtual std::size_t input_port_count() const noexcept = 0;
    virtual std::size_t output_port_count() const noexcept = 0;

    virtual std::shared_ptr<IInputPortAny> get_input_port(TypeId type) = 0;
    virtual std::shared_ptr<IOutputPortAny> get_output_port(TypeId type) = 0;

    virtual std::shared_ptr<IInputPortAny> get_input_port(TypeId type, std::size_t type_index) = 0;
    virtual std::shared_ptr<IOutputPortAny> get_output_port(TypeId type, std::size_t type_index) = 0;

    virtual std::shared_ptr<IInputPortAny> get_input_port_by_id(PortId port_id) = 0;
    virtual std::shared_ptr<IOutputPortAny> get_output_port_by_id(PortId port_id) = 0;

    template <typename T>
    std::shared_ptr<IInputPort<T>> get_input_port() {
        return std::dynamic_pointer_cast<IInputPort<T>>(get_input_port(type_id_of<T>()));
    }

    template <typename T>
    std::shared_ptr<IOutputPort<T>> get_output_port() {
        return std::dynamic_pointer_cast<IOutputPort<T>>(get_output_port(type_id_of<T>()));
    }

    template <typename T>
    std::shared_ptr<IInputPort<T>> get_input_port(std::size_t type_index) {
        return std::dynamic_pointer_cast<IInputPort<T>>(
            get_input_port(type_id_of<T>(), type_index));
    }

    template <typename T>
    std::shared_ptr<IOutputPort<T>> get_output_port(std::size_t type_index) {
        return std::dynamic_pointer_cast<IOutputPort<T>>(
            get_output_port(type_id_of<T>(), type_index));
    }

    virtual Result<std::shared_ptr<IPipelineSession>> create_pipeline_session(
        const PipelineSessionOptions& options = {}) = 0;
};

class INodeSession {
public:
    virtual ~INodeSession() = default;
    virtual std::shared_ptr<INode> get_node() const = 0;

    virtual std::shared_ptr<IInputStreamAny> get_input(TypeId type) = 0;
    virtual std::shared_ptr<IOutputStreamAny> get_output(TypeId type) = 0;

    virtual std::shared_ptr<IInputStreamAny> get_input_by_id(PortId port_id) = 0;
    virtual std::shared_ptr<IOutputStreamAny> get_output_by_id(PortId port_id) = 0;

    template <typename T>
    std::shared_ptr<IInputStream<T>> get_input() {
        auto stream = get_input(type_id_of<T>());
        if (!stream) return nullptr;
        return std::make_shared<detail::TypedInputStream<T>>(std::move(stream));
    }

    template <typename T>
    std::shared_ptr<IOutputStream<T>> get_output() {
        auto stream = get_output(type_id_of<T>());
        if (!stream) return nullptr;
        return std::make_shared<detail::TypedOutputStream<T>>(std::move(stream));
    }

    template <typename T>
    std::shared_ptr<IInputStream<T>> get_input(std::size_t type_index) {
        auto node = get_node();
        if (!node) return nullptr;
        auto port = node->get_input_port(type_id_of<T>(), type_index);
        if (!port) return nullptr;
        return this->template get_input_by_id<T>(port->get_index());
    }

    template <typename T>
    std::shared_ptr<IInputStream<T>> get_input_by_id(PortId port_id) {
        auto stream = get_input_by_id(port_id);
        if (!stream) return nullptr;
        return std::make_shared<detail::TypedInputStream<T>>(std::move(stream));
    }

    template <typename T>
    std::shared_ptr<IOutputStream<T>> get_output(std::size_t type_index) {
        auto node = get_node();
        if (!node) return nullptr;
        auto port = node->get_output_port(type_id_of<T>(), type_index);
        if (!port) return nullptr;
        return this->template get_output_by_id<T>(port->get_index());
    }

    template <typename T>
    std::shared_ptr<IOutputStream<T>> get_output_by_id(PortId port_id) {
        auto stream = get_output_by_id(port_id);
        if (!stream) return nullptr;
        return std::make_shared<detail::TypedOutputStream<T>>(std::move(stream));
    }
};

class IPipelineSession {
public:
    virtual ~IPipelineSession() = default;
    virtual Result<std::shared_ptr<INodeSession>> get_node_session(
        const std::shared_ptr<INode>& node) = 0;
    virtual Result<void> submit() = 0;
    virtual Result<void> finish_inputs() = 0;
    virtual void teardown() noexcept = 0;
    virtual void cancel() noexcept = 0;
    virtual bool is_cancelled() const noexcept = 0;
};

}  // namespace rebar_ml
