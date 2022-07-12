#include "SocketWatcher.h"

#include <fmt/format.h>
#include <json.hpp>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

SocketWatcher::SocketWatcher(std::vector<SocketRequest>& socket_requests, std::mutex& socket_requests_mutex)
    : m_socket_requests(socket_requests)
    , m_socket_request_mutex(socket_requests_mutex)
{
    memset(m_events, 0, (sizeof(epoll_event) * EPOLL_MAX_EVENTS));

    // This is most likely a race condition
    // This is also a very rare example of needing
    // to explicitly put struct before the type
    // in c++, as there's also a stat function.
    struct stat buffer;
    if (stat(SOCKET_FILE, &buffer) == 0) {
        // Delete the old sock file, if it exists.
        unlink(SOCKET_FILE);
    }

    // Setup the socket
    m_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (m_socket_fd == -1) {
        fmt::print("Failed to open socket!\n");
        are_we_okay = false;
    }

    sockaddr_un server_address;
    memset(&server_address, 0, sizeof(server_address));

    server_address.sun_family = AF_UNIX;
    strcpy(server_address.sun_path, SOCKET_FILE);

    if (bind(m_socket_fd, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) < 0) {
        fmt::print("Failed to bind to socket!\n");
        are_we_okay = false;
    }

    if (listen(m_socket_fd, 10) < 0) {
        fmt::print("Failed to listen to the socket!\n");
        are_we_okay = false;
    }

    // Setup epoll
    m_epoll_fd = epoll_create1(0);

    if (m_epoll_fd == -1) {
        fmt::print("Failed to create an epoll fd!\n");
        are_we_okay = false;
    }

    epoll_event epoll_instance;
    memset(&epoll_instance, 0, sizeof(epoll_instance));

    epoll_instance.events = (EPOLLIN | EPOLLPRI);
    epoll_instance.data.fd = m_socket_fd;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_socket_fd, &epoll_instance) == -1) {
        fmt::print("Failed to add an epoll ctl for our socket!\n");
        are_we_okay = false;
    }
}

SocketWatcher::~SocketWatcher()
{
    // Calling close() on the socket, also closes
    // the m_epoll_fd
    close(m_socket_fd);

    // Close all of our client file descriptors.
    for (const auto& client_fd : m_client_buffers) {
        close(client_fd.first);
    }

    // Delete the socket file.
    unlink(SOCKET_FILE);
}

bool SocketWatcher::watch()
{
    if (are_we_okay) {
        auto triggered_events = epoll_wait(m_epoll_fd, m_events, EPOLL_MAX_EVENTS, -1);

        if (triggered_events == -1) {
            // We got an interupt syscall
            if (errno == EINTR) {
                // Cleanup!
                are_we_okay = false;
                return false;
            }

            fmt::print("in {}, epoll_wait returned -1. errno={}\n", __FUNCTION__, errno);
            return false;
        }

        for (int32_t i = 0; i < triggered_events; i += 1) {
            auto new_fd = m_events[i].data.fd;

            if (new_fd == m_socket_fd) {
                // We have a new client.
                add_client(new_fd);
                return false;
            }

            return handle_client_message(new_fd);
        }
    }
    return false;
}

void SocketWatcher::add_client(int32_t file_descriptor)
{
    sockaddr_un new_client;
    memset(&new_client, 0, sizeof(new_client));

    socklen_t new_client_size = sizeof(new_client);

    auto client_fd = accept(file_descriptor, reinterpret_cast<sockaddr*>(&new_client), &new_client_size);

    if (client_fd == -1) {
        // TODO: Cleanup
        fmt::print("Failed to accept() a clients connection!\n");
        return;
    }

    m_client_buffers.insert({ client_fd, {} });

    epoll_event event;
    memset(&event, 0, sizeof(event));

    event.events = (EPOLLIN | EPOLLPRI);
    event.data.fd = client_fd;

    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0) {
        // TODO: Cleanup
        fmt::print("Failed to add a client to epoll!\n");
        return;
    }

    fmt::print("Added a new client!\n");
}

bool SocketWatcher::handle_client_message(int32_t file_descriptor)
{

    // Alright, we might have a new message, or a continuation
    // of a message!

    const auto client = m_client_buffers.find(file_descriptor);

    if (client == m_client_buffers.end()) {
        fmt::print("{} was passed fd {} but we are not storing that fd!\n", __FUNCTION__, file_descriptor);
        return false;
    }

    char buffer[256];

    auto bytes_read = recv(file_descriptor, buffer, sizeof(buffer), 0);

    if (bytes_read == -1) {
        fmt::print("recv reported an error with fd {}\n", file_descriptor);
        return false;
    }

    if (bytes_read == 0) {
        fmt::print("fd {} reported zero new bytes, removing them from epoll!\n", file_descriptor);

        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, file_descriptor, nullptr) < 0) {
            fmt::print("epoll_ctl failed to remove an event in {}\n", __FUNCTION__);
        }

        // Remove them from the map
        m_client_buffers.erase(client->first);
        return false;
    }

    fmt::print("We just read {} bytes from fd {}\n", bytes_read, file_descriptor);

    client->second.append(buffer);

    // Check if the input is ready for parsing
    // TODO: Our naive method of just checking for an end brace
    //       works fine for our current system, but greatly limits
    //       us in the future if we were to expand our ipc system
    //       to support more complicated messages.
    if (client->second.back() == '}') {
        parse_client_message(file_descriptor, client->second);
        return true;
    }

    return false;
}

void SocketWatcher::parse_client_message(int32_t file_descriptor, const std::string& full_message)
{
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(full_message);
    } catch (const std::exception& e) {
        fmt::print("Failed to Parse Socket JSON in {}\n", __FUNCTION__);
        return;
    }

    const std::string module_function = json.at("module_function");
    const std::string module_name = json.at("module_name");
    const bool return_output = json.at("return_output");

    SocketRequest new_request;
    new_request.module_name = std::move(module_name);
    new_request.module_function = std::move(module_function);
    new_request.file_descriptor = file_descriptor;

    {
        std::unique_lock<std::mutex> lock(m_socket_request_mutex);
        m_socket_requests.push_back(new_request);
    }

    fmt::print("Got a request to run a command function!\nmodule_function: {}\nmodule_name: {}\nreturn_output: {}\n", module_function, module_name, return_output);

    nlohmann::json json_reply;

    json_reply["module_function"] = module_function;
    json_reply["module_name"] = module_name;

    // TODO: Actually replying to a socket request to run a function has not been implemented.
    //       That's on the docket for tomorrow!
    if (return_output) {
        json_reply["output"] = "100";
        json_reply["output_type"] = 1;
    }

    const auto json_reply_as_str = json_reply.dump();
    send(file_descriptor, json_reply_as_str.c_str(), json_reply_as_str.size(), 0);
}
