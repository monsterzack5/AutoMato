#pragma once

#include <fmt/format.h>
#include <stdio.h>
#include <string>
#include <sys/inotify.h>
#include <unistd.h>

// Sleeps the current thread until the given filename is modified.
// Returns: True: The file was modified
//          False: The file was not modified, but inotify timed out.
//                 This should not happen often, but should be checked for.
inline bool wait_for_file_change(const std::string& filename)
{
    int inotify_file_descriptor = inotify_init();

    if (inotify_file_descriptor == -1) {
        fmt::print("Inotify Error, Inotify returned an invalid file descriptor\n");
        return false;
    }

    inotify_add_watch(inotify_file_descriptor, filename.c_str(), IN_MODIFY);

    const uint32_t inotify_event_size = sizeof(inotify_event);
    const uint32_t read_buffer_size = 1024 * (inotify_event_size + 16);

    char buffer[read_buffer_size];

    uint32_t length = read(inotify_file_descriptor, buffer, read_buffer_size);
    return (length > 0);
}