#include "EventManager.h"

#include <Event.h>
#include <FileWatcher.h>
#include <algorithm>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fstream>
#include <get_env_var.h>
#include <iostream>
#include <json.hpp>

// TODO:
// Currently, the system cannot detect when a user has
// only changed a single block in a flow, and only update
// that single block, the system currently will just mark
// that old flow as entirely inactive, and rewrite it.
// We can improve this a few ways:
// - Keep a cache of all events stored on the modules,
//   and use that for a DIFF, so we know exactly what
//   we need to update
// - Make the json file parser more intelligent, and have it
//   understand better when a user has only changed some
//   insignificant part of a flow, that is easy to update

namespace {

const static std::string PARSED_FLOWS_FILE = get_env_var(ENV::NODE_RED_DIR) + "/automato.parsed.flows.json";

void store_new_flow(uint8_t flow_id, const std::string& flow_name, const std::vector<Event>& flow)
{
    Database::the().prepare("INSERT INTO broadcast_flows (flow_id, flow_name) VALUES (?, ?)").run(flow_id, flow_name);

    // Flows we have read from disk, do not have a proper flow_id set
    // So set the events flow_id using the one we're given.
    auto insert_new_event = Database::the().prepare("INSERT INTO broadcast_events (flow_id, flow_name, module_uid, event_blob, section_number) VALUES (?, ?, ?, ?, ?)");
    for (auto event : flow) {
        event.flow_id = flow_id;
        auto serialized_event = event.serialize();
        insert_new_event.run(flow_id, flow_name, event.module_uid, serialized_event, event.section_number);
    }
}

bool has_flow_changed(uint8_t flow_id, const std::vector<Event>& flows)
{
    for (const auto& block : flows) {

        auto serialized_event = block.serialize();

        auto query = Database::the().prepare("SELECT EXISTS (SELECT 1 FROM broadcast_events WHERE flow_id = ? AND module_uid = ? AND event_blob = ? LIMIT 1)");
        bool is_event_stored = query.get(flow_id, block.module_uid, serialized_event).column(0);

        if (is_event_stored) {
            return true;
        }
    }

    return false;
}

nlohmann::json read_events_file()
{
    std::ifstream json_file(PARSED_FLOWS_FILE);
    std::stringstream json_file_stream;
    nlohmann::json json;

    // https://github.com/nlohmann/json/issues/2029
    json_file_stream << json_file.rdbuf();
    const std::string json_string = json_file_stream.str();

    // TODO: This fails quite often for no reason.
    //       I think we should just hack around it.
    //       And call this in a loop, with a max tries of 5
    //       and a small delay.

    try {
        json = nlohmann::json::parse(json_string);
    } catch (const std::exception& e) {
        fmt::print("Failed to read events file, with exception:\n{}\n", e.what());
        fmt::print("JSON File Contents at time of reading:\n{}\n", json_string);
        return {};
    }

    if (!json.is_array()) {
        // JSON is not in the correct format.
        fmt::print("Failed to read the parsed flows file!\n");
        return {};
    }

    return json;
}

std::vector<std::vector<Event>> get_events_from_json(const nlohmann::json& json)
{
    std::vector<std::vector<Event>> all_loaded_events;

    for (const auto& automato_flow : json) {
        std::vector<Event> flow_events;

        for (const auto& event_block : automato_flow) {
            const auto created_event = Event::from_json(event_block);
            flow_events.push_back(created_event);
        }
        all_loaded_events.push_back(std::move(flow_events));
    }

    return all_loaded_events;
}

uint8_t get_flow_id_from_event(const Event& event)
{
    auto serialized_event = event.serialize();

    auto query = Database::the().prepare("SELECT flow_id FROM broadcast_events WHERE event_blob = ?");
    auto statement = query.get(serialized_event);
    uint8_t flow_id = statement.column(0);

    return flow_id;
}

} // namespace

namespace EventManager {

uint8_t get_new_flow_id()
{
    return Database::the().prepare("SELECT MAX(flow_id) FROM broadcast_events").get().column(0);
}

void print_event_update(const EventUpdate& update)
{
    const std::string update_string = (update.update_type == EventUpdateType::add) ? "add" : "remove";

    fmt::print(fmt::fg(fmt::color::green), "Module Uid: {}\nUpdate Type: {}\nUpdate Size: {}\nUpdate Bytes: {:#04x}\n",
        update.module_uid, update_string, update.event_blob.size(), fmt::join(update.event_blob, " "));
}

void append_event_updates(EventUpdateType type, uint8_t flow_id, std::vector<EventUpdate>& updates_needed, const std::vector<Event>& events)
{
    // TODO: flow_id is not being set properly above, in the chain
    for (auto block : events) {

        block.flow_id = flow_id;
        auto serialized_event = block.serialize();

        updates_needed.emplace_back(EventUpdate {
            block.module_uid,
            type,
            std::move(serialized_event),
        });
    }
}

bool wait_for_changes(std::vector<EventUpdate>& updates_needed, std::mutex& lock)
{
    // Wait indefinitely until the flows file is modified.
    wait_for_file_change(PARSED_FLOWS_FILE);

    std::unique_lock<std::mutex> updates_needed_lock(lock);

    // TODO: This function needs proper error handling
    const auto json = read_events_file();
    const auto all_flows_from_disk = get_events_from_json(json);

    bool do_modules_need_updating = false;

    // Steps:
    // 1. Blanket all flows as is_active = FALSE
    // 2. Get the main event from the flow
    // 3. Check to see if we're storing the main event
    // 4. if so, check if all of its child nodes are the same
    // 5. if they are, do nothing else.
    // 6. if they are not, get which events have been modified
    //    and update only them
    // 7. if we're not storing this main event,
    //    store this flow as new.
    // 8. any Events that are still marked as old, delete them
    //    if they're storing on a module

    Database::the().prepare("UPDATE broadcast_flows SET is_active = \"FALSE\"").run();

    for (const auto& disk_flow : all_flows_from_disk) {

        const auto main_event_iter = std::find_if(disk_flow.begin(), disk_flow.end(), [](const Event& event) { return event.event_type == EventType::Main; });

        if (main_event_iter == disk_flow.end()) {
            // We did not find a main event for this flow
            // The Main event might be disabled, or non-existent.
            // Either way, we cannot parse this flow.
            fmt::print("Couldn't find a main event when parsing a flow!\n");
            continue;
        }

        const auto& main_event = *main_event_iter;

        // Events from disk don't have event.flow_id set, since
        // that is not stored in NodeRed, and therefore not stored in
        // the resulting parsed JSON.

        uint8_t flow_id = get_flow_id_from_event(main_event);

        // Either the main event changed, or we have seen
        // this event before. Either way, store this flow
        // as if it was brand new.
        if (flow_id == 0) {
            fmt::print("Found a flow we're not storing!\n");

            flow_id = get_new_flow_id();
            store_new_flow(flow_id, main_event.flow_name, disk_flow);
            append_event_updates(EventUpdateType::add, flow_id, updates_needed, disk_flow);

            do_modules_need_updating = true;
            continue;
        }

        // We've seen this main event before, and need
        // to check if any of it's children have changed.
        if (has_flow_changed(flow_id, disk_flow)) {
            fmt::print("Found a flow that diverges from what we're storing\n");
            // TODO: can't we just reuse flow_id here?
            uint8_t new_flow_id = get_new_flow_id();

            // Store this flow as if it were brand new.
            store_new_flow(new_flow_id, main_event.flow_name, disk_flow);
            append_event_updates(EventUpdateType::add, flow_id, updates_needed, disk_flow);

            do_modules_need_updating = true;
            continue;
        }

        fmt::print("Found a flow we're storing!\n");
        Database::the().prepare("UPDATE broadcast_flows SET is_active = \"TRUE\" WHERE flow_id = ?").run(flow_id);
    }

    // TODO: Check if the event has been stored on a module or not.
    // At this point, any event flow that has is_active = false
    // is outdated, and should be removed from the database

    // Get all events that are marked as is_active = false
    // and append them to updates_need with EventUpdateType = remove

    const auto inactive_blobs = Database::the().prepare("SELECT event_blob FROM broadcast_events WHERE flow_id IN (SELECT flow_id FROM broadcast_flows WHERE is_active = \"FALSE\")");
    std::vector<std::vector<uint8_t>> inactive_event_blobs;
    for (const auto& statement : inactive_blobs) {
        std::vector<uint8_t> event_blob = statement.column(0);
        inactive_event_blobs.push_back(event_blob);
    }

    // At this point, there might be flow_id's that are still is_active = false
    // get them, and then delete them from modules (if they are not stored
    // on the modules, this is a harmless no-op)

    for (const auto& blob : inactive_event_blobs) {
        do_modules_need_updating = true;
        const uint8_t module_uid = Database::the().prepare("SELECT module_uid FROM broadcast_events WHERE event_blob = ?").get(blob).column(0);

        updates_needed.emplace_back(
            EventUpdate {
                module_uid,
                EventUpdateType::remove,
                std::move(blob),
            });
    }

    Database::the().prepare("DELETE FROM broadcast_events WHERE flow_id IN (SELECT flow_id FROM broadcast_flows WHERE is_active = \"FALSE\")").run();
    Database::the().prepare("DELETE FROM broadcast_flows WHERE is_active = \"FALSE\"").run();

    if (do_modules_need_updating) {
        fmt::print(fmt::fg(fmt::color::green), "Event Updates to be sent to modules:\n");
        std::for_each(updates_needed.begin(), updates_needed.end(), [](const EventUpdate& update) {
            print_event_update(update);
        });
        fmt::print("\n");
    }

    // Sort so EventUpdateType::remove always comes before EventUpdateType::add
    std::sort(updates_needed.begin(), updates_needed.end(), [](const EventUpdate& lhs, const EventUpdate& rhs) {
        return static_cast<uint8_t>(lhs.update_type) < static_cast<uint8_t>(rhs.update_type);
    });

    return do_modules_need_updating;
}

} // namespace EventManager
