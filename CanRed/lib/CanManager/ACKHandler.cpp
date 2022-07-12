#include "ACKHandler.h"

#include <get_current_time_ms.h>
#include <seconds_to_ms.h>

// TODO: We currently don't retain enough information
//       to recreate dropped frames, and resend them.

// For Development, where modules may go offline randomly
#ifndef DISABLE_ACK_HANDLING

void ACKHandler::add_waiting_on_ack(uint16_t module_uid, uint8_t command_id)
{
    auto ack = ACK(module_uid, command_id);

    m_buffer.insert({ ack, get_current_time_ms() });
}

void ACKHandler::remove_waiting_on_ack(uint16_t module_uid, uint8_t command_id)
{
    // TODO: Think of a way to improve this.

    // Since the handling of CAN::Frames should be FIFO, when we get
    // an ack back, where the WaitingOnAck struct is the same,
    // we should delete the oldest occurrence we have

    auto ack = ACK { module_uid, command_id };
    auto iter = m_buffer.equal_range(ack);

    std::unordered_map<ACK, uint64_t>::iterator last_added_iter;

    uint64_t last_added = 0;

    for (auto i = iter.first; i != iter.second; ++i) {
        if (i->second < last_added) {
            last_added = i->second;
            last_added_iter = i;
        }
    }
    m_buffer.erase(last_added_iter);
}

std::vector<ACK> ACKHandler::get_outdated_acks()
{
    uint64_t current_time = get_current_time_ms();

    if (m_buffer.size() == 0) {
        // Nothing to check.
        return {};
    }

    std::vector<ACK> outdated_acks {};
    for (const auto& ack : m_buffer) {
        if (ack.second > (current_time + seconds_to_ms(3))) {
            // ACK is outdated.
            outdated_acks.push_back(ack.first);
            remove_waiting_on_ack(ack.first.module_uid, ack.first.command_id);
        }
    }
    return outdated_acks;
}

#else
void ACKHandler::add_waiting_on_ack(uint16_t module_uid, uint8_t command_id)
{
    (void)module_uid;
    (void)command_id;
}

void ACKHandler::remove_waiting_on_ack(uint16_t module_uid, uint8_t command_id)
{
    (void)module_uid;
    (void)command_id;
}

std::vector<ACK> ACKHandler::get_outdated_acks()
{
    return {};
}
#endif
