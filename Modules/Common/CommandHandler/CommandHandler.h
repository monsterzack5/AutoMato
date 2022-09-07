#pragma once

#include <AnyType.h>
#include <CanConstants.h>
#include <stdint.h>

// Defines the maximum amount of commands we can store
const uint8_t AMOUNT_OF_COMMANDS = 4;

using CommandFunction = AnyType (*)(const void*);

struct Command {
    uint8_t id;
    CommandFunction func;
};

class CommandHandler {
public:
    CommandHandler() = default;
    ~CommandHandler() = default;

    void register_function(uint8_t command_id, const CommandFunction& function)
    {
        m_command_buffer[m_command_buffer_index++] = { command_id, function };
    }

    AnyType run(uint8_t command_id) const
    {
        uint8_t command_index = CAN::NOT_FOUND;
        for (uint8_t i = 0; i < m_command_buffer_index; i += 1) {
            if (m_command_buffer[i].id == command_id) {
                command_index = i;
                break;
            }
        }

        if (command_index == CAN::NOT_FOUND) {
            // This might happen because:
            // - We're storing Event's that are outdated
            // - Someone, somewhere, got confused about command_id's
            // TODO: We should not only print an error,
            //       but send an error over CAN when this happens,
            //       and double check our saved events.
            DEBUG_PRINTLN("ERROR: CommandHandler.run(): command ID not found.");
            return {};
        }

        // TODO: This does not currently work on functions that take input
        AnyType data = m_command_buffer[command_index].func(nullptr);

        return data;
    }

private:
    Command m_command_buffer[AMOUNT_OF_COMMANDS];
    uint8_t m_command_buffer_index { 0 };
};