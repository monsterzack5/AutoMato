#include <CanManager.h>
#include <Database.h>
#include <EventManager.h>
#include <FileWatcher.h>
#include <SerialInterface.h>
#include <SocketWatcher.h>
#include <atomic>
#include <condition_variable>
#include <fmt/format.h>
#include <fstream>
#include <json.hpp>
#include <mutex>
#include <queue>
#include <signal.h>
#include <sys/stat.h>
#include <thread>

void handle_sigint(int signal_number)
{
    // Most terminals will output ^C when receiving a ctrl-c
    // So this just completes the message :^)
    fmt::print("anRed Stopped!\n");
    exit(signal_number);
}

// When running on a battery powered embedded OS, a big
// consideration is saving battery, which is much easier when
// code is only being run when it needs to be, and have no spinlocking.
// As such, CanRed is designed to be purely event based, where
// every thread is sleeping as often as possible.
// Now this does incur some performance losses as opposed to
// a standard superloop, but in preliminary testing,
// CanRed dropped from being able to parse a max of 90k frames/s
// to around 60k frames/s. With average CPU usage going from 40%
// to around 3%. Not even counting the common-case of nothing happening
// at all.

// Created Threads:
// 1. A Thread for every interface
// 2. The Main Thread
// 3. ACK Checking Thread
// 4. Checking For Modified Events From Node-Red
// 5. Checking For Injected/Requested CanFrames
// 6. Checking For Incoming Socket Data

// Basic Overview:
// - The main thread waits on a condition variable, 
//   and when given, checks for which process reported work to be done.
// - All other threads wait on an event to happen and when it does
//   they notify the main thread, and go back to sleeping. 

int main(int, const char**)
{
    signal(SIGINT, handle_sigint);

    fmt::print("CanRed Started!\n");

    std::condition_variable cv;
    std::mutex main_lock;

    std::queue<CAN::Frame> frame_queue;
    std::mutex frame_queue_mutex;

    std::vector<EventUpdate> events_needing_update;
    std::mutex events_mutex;
    std::atomic<bool> do_events_need_updating { false };

    std::vector<SocketRequest> socket_requests;
    std::mutex socket_request_lock;
    std::atomic<bool> do_we_have_new_socket_data { false };

    std::atomic<bool> should_check_acks { false };
    std::atomic<int> command_to_inject { 0 };

    // Initialize all interfaces, This should be able to be done
    // via a config file later on.
    // Store them in an array for easy iterating.
    SerialInterface serial_interface("/dev/ttyUSB1");
    std::vector<AutomatoInterface*> interfaces;
    interfaces.push_back(&serial_interface);

    // Create a thread for every interface, where
    // each will block forever until they read a frame.
    std::vector<std::thread> interface_threads;
    for (auto* interface : interfaces) {
        interface_threads.push_back(std::thread([interface, &cv, &frame_queue, &frame_queue_mutex] {
            CAN::Frame holder_frame;
            for (;;) {
                if (interface->read_frame(&holder_frame)) {
                    std::unique_lock<std::mutex> lock(frame_queue_mutex);

                    frame_queue.push(holder_frame);
                    cv.notify_one();
                }
            }
        }));
    }

    auto main_thread = std::thread([&]() {
        std::unique_lock<std::mutex> lock(main_lock);
        // TODO: in CanRed, adding interfaces should
        // be easy, and not require editing source code
        // for a default configuration.

        CanManager manager(interfaces);

        for (;;) {
            cv.wait(lock);

            {
                std::unique_lock<std::mutex> frame_queue_lock(frame_queue_mutex);
                while (!frame_queue.empty()) {
                    manager.handle_incoming_frame(frame_queue.front());
                    frame_queue.pop();
                }
            }

            if (should_check_acks) {
                manager.check_for_old_acks();
                should_check_acks = false;
            }

            if (do_events_need_updating) {
                std::unique_lock<std::mutex> events_lock(events_mutex);
                manager.update_events(events_needing_update);
                do_events_need_updating = false;
            }

            if (command_to_inject > 0) {
                uint8_t buffer[1];
                buffer[0] = (uint8_t)command_to_inject;

                CAN::ID id(CAN::UID::MCM, CAN::UID::CCM);
                CAN::Frame frame(id, buffer, 1);

                manager.inject_frame(frame);

                command_to_inject = 0;
            }

            if (do_we_have_new_socket_data) {
                manager.parse_socket_input(socket_requests, socket_request_lock);
                do_we_have_new_socket_data = false;
            }
        }
    });

    auto check_acks_thread = std::thread([&]() {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            should_check_acks = true;
            cv.notify_one();
        }
    });

    auto events_thread = std::thread([&]() {
        for (;;) {
            if (EventManager::wait_for_changes(events_needing_update, events_mutex)) {
                fmt::print("updating do_events_need_updating\n");
                do_events_need_updating = true;
                cv.notify_one();
            }
        }
    });

    // Very basic, testing only inject mechanism.
    // Maybe making a small little gui for this would be cool
    // like, kinda how you can select files in zsh
    // and have buttons to repeat and stuff
    // would be kinda cool using sockets!
    auto inject_thread = std::thread([&]() {
        const std::string INJECT_FILE_NAME = "/tmp/CanRed/inject";
        mkdir("/tmp/CanRed", 0700);
        std::ofstream test(INJECT_FILE_NAME);

        for (;;) {
            if (wait_for_file_change(INJECT_FILE_NAME)) {
                // inject file was modified
                std::ifstream inject_file(INJECT_FILE_NAME);
                std::string line = "";
                std::getline(inject_file, line);
                truncate(INJECT_FILE_NAME.c_str(), 0);

                command_to_inject = std::stoi(line);
                fmt::print("Injecting byte {} into CAN\n", command_to_inject);
                cv.notify_one();
            }
        }
    });

    auto socket_thread = std::thread([&]() {
        SocketWatcher watcher(socket_requests, socket_request_lock);

        for (;;) {
            if (watcher.watch()) {
                do_we_have_new_socket_data = true;
                cv.notify_one();
            }
        }
    });

    main_thread.join();
    check_acks_thread.join();
    events_thread.join();
    inject_thread.join();
    socket_thread.join();

    for (auto& interface_thread : interface_threads) {
        interface_thread.join();
    }

    return 0;
}
