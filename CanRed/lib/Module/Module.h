#pragma once

#include <fmt/format.h>
#include <stdint.h>
#include <string>

enum class ModuleType : uint8_t {
    Reader,
    Writer
};

enum class StoredModuleStatus : uint8_t {
    NOT_STORED,
    NOT_MODIFIED,
    MODIFIED,
    ERROR
};

struct Module {
    uint16_t uid;
    std::string name;
    std::string description;
    ModuleType type;
    bool active;
    bool did_come_online;

    bool operator==(const Module& other) const
    {
        // FIXME:
        // We don't really care about active or
        // did_come_online.
        // But not comparing members of an
        // aggregate is definitely a code smell.
        return (uid == other.uid
            && name == other.name
            && description == other.description
            && type == other.type);
    }

    void print() const
    {
        // TODO: Print reader/writer instead of 0/1
        fmt::print("Module uid: {}\nModule Name: {}\nModule Description: {}\nModuleType: {}\n", uid, name, description, static_cast<uint8_t>(type));
    }
};

struct ModuleCommand {
    uint16_t module_uid;
    uint8_t command_uid;
    std::string name;

    bool operator==(const ModuleCommand& other) const
    {
        return (module_uid == other.module_uid
            && command_uid == other.command_uid
            && name == other.name);
    }

    void print() const
    {
        fmt::print("Module Command\nModule uid: {}\nCommand uid: {}\nCommand name: {}\n", module_uid, command_uid, name);
    }
};