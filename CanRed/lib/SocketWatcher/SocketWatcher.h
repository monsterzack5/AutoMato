#pragma once

#include <mutex>
#include <string>
#include <sys/epoll.h>
#include <unordered_map>
#include <vector>

struct SocketRequest {
    std::string module_name;
    std::string module_function;
    uint16_t module_uid { 0 };
    uint8_t command_uid { 0 };
    int32_t file_descriptor { 0 };
};

// TODO: This should be in a namespace
enum class OutputType : uint8_t {
    signed_int = 1,
    unsigned_int = 2,
    signed_bigint = 3,
    unsigned_bigint = 4,
    f_float = 5,
    d_double = 6,
    b_bool = 7,
    v_void = 8,
};

class SocketWatcher {

public:
    SocketWatcher(std::vector<SocketRequest>& socket_requests, std::mutex& socket_requests_mutex);
    SocketWatcher() = delete;
    ~SocketWatcher();

    bool watch();

private:
    const char* SOCKET_FILE = "/tmp/CanRed/red.sock";

    // This defines the maximum amount of simultaneous
    // connections we can have.
    static const size_t EPOLL_MAX_EVENTS = 64;

    bool are_we_okay { true };

    void add_client(int32_t file_descriptor);
    bool handle_client_message(int32_t file_descriptor);
    void parse_client_message(int32_t file_descriptor, const std::string& full_message);

    int32_t m_socket_fd { 0 };
    int32_t m_epoll_fd { 0 };

    std::vector<SocketRequest>& m_socket_requests;
    std::mutex& m_socket_request_mutex;

    epoll_event m_events[EPOLL_MAX_EVENTS];

    std::unordered_map<int, std::string> m_client_buffers;
};
