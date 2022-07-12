#pragma once

#include <Event.h>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>

enum class EventUpdateType : uint8_t {
    remove,
    add,
};

struct EventUpdate {
    uint16_t module_uid;
    EventUpdateType update_type;
    std::vector<uint8_t> event_blob;
};

namespace EventManager {

void print_event_update(const EventUpdate& update);

bool wait_for_changes(std::vector<EventUpdate>& updates_needed, std::mutex& lock);

} // namespace EventManager
