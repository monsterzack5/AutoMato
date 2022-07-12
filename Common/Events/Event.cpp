#include "Event.h"

#ifdef CANRED
void Event::set_condition(const std::string& value)
{
    conditional = to_conditional(value);
}

void Event::set_interval_unit(const std::string& value)
{
    interval_unit = to_interval_unit(value);
}

Event Event::from_json(const nlohmann::json& json)
{
    // TODO: This function needs a lot more error handling!
    Event event;

    // Common
    auto get_module_uid = Database::the().prepare("SELECT uid FROM can_modules WHERE name = ?");
    const uint16_t module_uid = get_module_uid.get(json.at("module_name").get<std::string>()).column(0);

    auto get_module_function_id = Database::the().prepare("SELECT command_uid FROM can_module_commands WHERE module_uid = ? AND name = ?");
    const uint8_t module_function_id = get_module_function_id.get(module_uid, json.at("module_function").get<std::string>()).column(0);

    event.module_uid = module_uid;
    event.this_function_id = module_function_id;
    event.section_number = json.at("section_number");

    if (json.contains("conditional")) {
        const std::string conditional = json.at("conditional");
        const std::string value_to_check = json.at("value_to_check");
        event.set_condition(conditional);
        event.value_to_check = AnyType::from_string(value_to_check);
    }

    // Main Event
    if (event.section_number == 0) {
        const uint32_t interval = std::stoi(std::string(json.at("interval")));
        const std::string interval_unit = json.at("interval_unit");

        event.event_type = EventType::Main;

        event.set_interval_unit(interval_unit);
        event.interval = interval;

        event.flow_name = json.at("flow_name");

        return event;
    }

    // If Event
    if (json.contains("if_true")) {
        event.if_true = json.at("if_true");
        event.if_false = json.at("if_false");

        event.event_type = EventType::If;

        return event;
    }

    // Command Event
    if (json.at("next_section").is_number()) {
        event.next_section = json.at("next_section");

        event.event_type = EventType::Command;

        return event;
    }

    // Error!
    printf("Value to check type not properly set in create_event_from_json\n");
    return Event {};
}

void Event::print() const
{

    const auto print_any_type = [](const AnyType& value) {
        if (value.is_int()) {
            fmt::print("{}", static_cast<int64_t>(value));
        } else if (value.is_floating()) {
            fmt::print("{}", static_cast<double>(value));
        } else if (value.is_bool()) {
            fmt::print("{}", static_cast<bool>(value));
        } else {
            fmt::print("NOT_SET");
        }
    };

    if (event_type == EventType::Main) {
        fmt::print("<Main Event>\n");
        fmt::print("Flow ID: {}\n", flow_id);
        fmt::print("Every {} ", interval);
        print_interval_unit(interval_unit);
        fmt::print(",\nCheck if Command: <{}>\nis ", this_function_id);
        print_conditional(conditional);

        print_any_type(value_to_check);
        fmt::print(":");

        print_type_used(value_to_check.get_type());
        fmt::print("\n");
        return;
    }

    if (event_type == EventType::If) {
        fmt::print("<IF Event>\n");
        fmt::print("Flow ID: {}\n", flow_id);
        fmt::print("Section number: {}\n", section_number);
        fmt::print("If Command <{}> is ", this_function_id);
        print_conditional(conditional);

        print_any_type(value_to_check);
        fmt::print(":");

        print_type_used(value_to_check.get_type());
        fmt::print("\nRun Section {}\n", if_true);
        fmt::print("Else Run Section {}\n", if_false);
        return;
    }

    if (event_type == EventType::Command) {
        fmt::print("<Command Event>\n");
        fmt::print("Flow ID: {}\n", flow_id);
        fmt::print("Section number: {}\n", section_number);
        fmt::print("Run Command: <{}>\n", this_function_id);
        fmt::print("Then, Run Section: {}\n", next_section);
        return;
    }

    fmt::print("event_type is unknown, cannot print block!\n");
}

#endif

uint8_t Event::max_size() const
{
    switch (event_type) {
    case EventType::Command:
        return Event::COMMAND_MAX_SIZE;
    case EventType::If:
        return Event::IF_MAX_SIZE;
    case EventType::Main:
        return Event::MAIN_MAX_SIZE;
    case EventType::NOT_SET:
        return 0;
    }
    __builtin_unreachable();
}

size_t Event::serialize(uint8_t buffer[]) const
{
    memset(buffer, 0, max_size());

    buffer[0] |= static_cast<uint8_t>(event_type) << 6;

    if (event_type != EventType::Command) {
        buffer[0] |= static_cast<uint8_t>(conditional) << 3;
        buffer[0] |= static_cast<uint8_t>(value_to_check.get_type());
    }

    buffer[1] = flow_id;
    buffer[2] = this_function_id;

    if (event_type == EventType::Command || event_type == EventType::If) {
        buffer[3] = section_number;
    }

    if (event_type == EventType::Command) {
        buffer[4] = next_section;
        return Event::COMMAND_MAX_SIZE;
    }

    if (event_type == EventType::Main) {
        buffer[3] = static_cast<uint8_t>(interval_unit) << 6;
        buffer[4] = interval;
    } else {
        buffer[4] = if_true;
        buffer[5] = if_false;
    }

    const uint8_t value_to_check_offset = (event_type == EventType::If) ? 1 : 0;

    value_to_check.serialize(&buffer[5 + value_to_check_offset]);

    return 5 + value_to_check_offset + value_to_check.size();
}

#ifdef CANRED
std::vector<uint8_t> Event::serialize() const
{
    // TODO:
    // Having this function is a bit of a code smell, since
    // ideally you'd stick to vector on the desktop,
    // and stick to c arrays on embedded platforms.
    // But, until we factor CanRed to be much more
    // desktop c++ like, we are stuck with functions
    // like these.
    uint8_t buffer[MAX_SIZE];

    auto event_size = serialize(buffer);

    return std::vector<uint8_t>(buffer, &buffer[event_size]);
}
#endif

Event Event::from_buffer(uint8_t event_blob[])
{
    Event event;

    event.event_type = static_cast<EventType>((event_blob[0] & EVENT_TYPE_BITMAP) >> 6);

    if (event.event_type != EventType::Command) {
        event.conditional = static_cast<Conditional>((event_blob[0] & CONDITIONAL_BITMAP) >> 3);
    }

    event.flow_id = event_blob[1];
    event.this_function_id = event_blob[2];

    if (event.event_type == EventType::Command) {
        event.section_number = event_blob[3];
        event.next_section = event_blob[4];
        return event;
    }

    if (event.event_type == EventType::If) {
        event.section_number = event_blob[3];
        event.if_true = event_blob[4];
        event.if_false = event_blob[5];
    }

    if (event.event_type == EventType::Main) {
        event.interval_unit = static_cast<IntervalUnit>((event_blob[3] & INTERVAL_UNIT_BITMAP) >> 6);
        event.interval = event_blob[4];
    }

    const uint8_t value_to_check_offset = (event.event_type == EventType::If) ? 1 : 0;

    TypeUsed type = static_cast<TypeUsed>(event_blob[0] & VALUE_TYPE_BITMAP);
    event.value_to_check = AnyType::from_buffer(type, &event_blob[5 + value_to_check_offset]);

    return event;
}

size_t Event::serialized_size(const uint8_t event_blob[])
{
    // using the bits, return the event size

    // we need to know its type
    auto event_type = static_cast<EventType>((event_blob[0] & EVENT_TYPE_BITMAP) >> 6);

    if (event_type == EventType::NOT_SET) {
        // Error!
        return 0;
    }

    if (event_type == EventType::Command) {
        return Event::COMMAND_MAX_SIZE;
    }

    // we need to check its value to check max size
    auto value_type = static_cast<TypeUsed>(event_blob[0] & VALUE_TYPE_BITMAP);

    uint8_t value_size = type_used_size(value_type);

    switch (event_type) {
    case EventType::Main:
        return EVENT_MIN_SIZE_MAIN + value_size;
    case EventType::If:
        return EVENT_MIN_SIZE_IF + value_size;
    default:
        __builtin_unreachable();
        return 0;
    }
}