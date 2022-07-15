# Parsing CAN and Serial, or Serializing Our Semantics
CAN and Serial don't have any predefined methods of how data should
transferred on one end, and parsed on the other. So we have implemented
our own system, it's meant to be easily parsable, and extendable. It also
needs to be able to handle LongFrame groups, where we can have anywhere from
8 to 2048 bytes, depending on the specific module. An Overview of how to parse CAN is defined below.

Definitions:
- Lexer: Creates a collection of bytes, to be passed to the parser
- Parser: Understands and acts on those bytes
- Consume: parse the current byte, then move the cursor to the next byte
- CAN Protocol Specifier: A marker byte that indicates how to parse the next byte, defined in `CanConstants.h`, under `CAN::Protocol::`
- Data Byte: An arbitrary uint8_t number, Example: a CommandID after an ACK
- Primitive Specifier: A `CAN::Primitive::` Specifier that indicates the next n bytes are to be serialized into a primitive value
- a `CAN::Frame`/Can Frame: The packet that gets sent over the CANBUS, that contains:
  - a 32 Bit CAN ID
  - 0-8 Data Bytes
  - a CAN DLC, the number of data bytes a frame contains
- Standard Frame: A CAN::Frame that contains 0-8 bytes
- Long Frame: A CAN::Frame that is marked as Being a Long Frame, and is part of a LongFrame Group
- LongFrame Group: A Collection of LongFrames 


## Single Frames VS Long Frame Groups
LongFrames are designated using a bit inside of the `CAN::ID`, and are handled slightly differently. a LongFrame always uses a unique ID as the first data byte in a group. This Unique ID is used so we can correlate Long Frames Groups, in the case that a Module is receiving multiple LongFrame groups at a time.

We accumulate LongFrames into a buffer, until we get a LongFrame with a CAN DLC that is less than 8 bytes. Meaning we don't need to do any intermittent parsing of a LongFrame group.

All Standard Frames start with a `CAN::Protocol::` Specifier as the first byte, while all LongFrames start with a LongFrame group uid, as the first byte, and a protocol specifier as the second byte.

# Lexing

### Standard Frames
We don't need to Lex a Standard Frame, we simply pass the Frames buffer to the parser

### Long Frames
1. Consume the first byte of the Frame, the LongFrame uid
2. Check any/all LongFrame buffers if the first position of the buffer is the LongFrame uid
   - if we found a buffer, Go to step 3, if not, go to step 5
3. Push Frame Bytes 1-7 into the buffer
4. If the frame CAN DLC is less than 8, send the buffer to the parser, 
   without the first element
5. Create/Get an empty buffer
6. Push the LongFrame group uid into the first position of the buffer
7. Go to Step 3

# Parsing

1. Consume Protocol Specifier
   - If the next byte is a data byte, Go to step 2
   - If the next byte is a Primitive specifier, Go to step 3
2. Consume Data byte
   - if the buffer is not empty, go to step 1
3. Consume Primitive Specifier
4. Consume N bytes
   - if the buffer is not empty, go to step 1

## Protocol Specifiers

### Action Specifiers
"Size" meaning how many bytes come after the `CAN::Protocol::` specifier

- ACKNOWLEDGEMENT
  - Size: 0 Bytes
  - Usage: Sent from a module to acknowledge a recently CAN Message.

- CHECK_IN
   - Size: 0 Bytes
   - Usage: Sent to the MCM from modules, reporting that they are now online and ready to receive commands.

- NEW_UID 
  - Size: 0 Bytes
  - Usage: Sent by modules requesting a new Module uid from the MCM

- REPLY_NEW_UID
   - Size: 2 Bytes
   - Usage: Reply to NEW_UID, containing a new UID
   - Format: \
    Byte[x + 1]: Upper 3 Bits of a Module uid \
    Byte[x + 1]: Lower 8 Bits of a Module uid

- UPDATE_INFO
   - Size: 0 Bytes
   - Usage: Sent from the MCM, asking for a module to serialize and send its JSON configuration back to the MCM.

- REPLY_UPDATE_INFO
   - Size: sizeof(module_config_string)
   - Usage: Reply to UPDATE_INFO, containing every character from a modules JSON configuration
   - Special: This specifier will always be used in a LongFrame group, and will never appear with any other specifier in a LongFrame group. Using these guarantees, the frame format will always be the same.
   - Example: There's an example of this in CAN.md

- ERROR_GENERIC
   - Size: 1 Byte
   - Usage: Indicates an Error returned by a module, that is generic
   - Format: \
     Byte[x + 1]: One Of: CAN::GENERIC_ERRORS::8

- ERROR_MODULE
   - Size: 1 Byte
   - Usage: Indicates an Error returned by a module, that is specific to the module
   - Format: \
     Byte[x + 1]: A Module Specific Error

- COMMAND
   - Size: 1 Byte
   - Usage: Tells a module to run a user CommandFunction
   - Format: \
     Byte[x + 1]: User CommandFunction ID

- COMMAND_INPUT
   - Size: 2 Bytes + 1-8 Byte(s), Command ID, Primitive Specifier + Primitive
   - Usage: Tells a module to run a user CommandFunction, that takes a primitive as an input
   - Format: \
    Byte[x + 1]: User CommandFunction ID \
    Byte[x + 2]: Primitive Specifier \
    Byte[x + 3-n]: a Primitive

- REPLY_COMMAND
   - Size: Either 1 Byte, or 2 Bytes + 1-8 Byte(s), Primitive Specifier + Primitive
   - Usage: A reply sent after running a command, that can possibly contain a return value
   - Special: We send this after running the CommandFunction, to indicate we fully ran it
   - Format: \
    Size 1 Byte: \
    Byte[x + 1]: User CommandFunction ID \
    or \
    Size 2 Bytes + 1-8 Byte(s): \
    Byte[x + 1]: User CommandFunction ID \
    Byte[x + 2]: Primitive Specifier \
    Byte[x + 3-11]: a Primitive 

### Event Specifiers
- EVENT_ADD
   - Size: 5 - 14 Bytes
   - Usage: Sent via the MCM to add an Event to a module
   - Format: \
     byte[x + 1 - n]: A Serialized Event
- EVENT_REMOVE
   - Size: 5 - 14 Bytes
   - Usage: Sent via the MCM, to remove an Event from a module
   - Format: \
     byte[x + 1 - n]: A Serialized Event
- EVENT_REMOVE_ALL
   - Size: 0 Bytes
   - Usage: Sent via the MCM, telling a module to remove all stored events

- EVENT_SEND_STORED
   - Size: 0 Bytes
   - Usage: Sent via the MCM, asking a module to send back all stored events.
- REPLY_EVENT_SEND_STORED
   - Size: 5 - n Bytes
   - Usage: Send from a module, replying back with every stored event
   - Format: \
     byte[x + 1]: Serialized event \
     byte[x + 1 + size of last event]: REPLY_EVENT_SEND_STORED \
     byte[n]: cycle repeats 
- EVENT_RUN_NEXT_PART
   - Size: 2 Bytes
   - Usage: Send via any module, asking for the next event in a flow to be ran
   - Format:
     byte[x + 1]: flow_id of event to run \
     byte[x + 2]: section number of the flow
- FORMAT_EEPROM
   - Size: 0 Bytes
   - Usage: Sent via the MCM, asking for a module to complete format it's storage

### Primitive Specifiers
a Primitive Specifier indicates that the following bytes
can be deserialized into a primitive value, and should be used in some manor

The middle number indicates the number of bytes to read, to form the primitive, here's a list of all supported primitives.

- UNSIGNED_1_BYTES
- UNSIGNED_2_BYTES
- UNSIGNED_4_BYTES
- UNSIGNED_8_BYTES
- SIGNED_1_BYTES
- SIGNED_2_BYTES
- SIGNED_4_BYTES
- SIGNED_8_BYTES
- FLOAT_4_BYTES
- DOUBLE_8_BYTES
- BOOL_1_BYTES

## CanRed vs Automato CAN Parsers
We currently use two different parser implementations for CanRed, and Automato.cpp. This design choice was made since the MCM and Modules are intended to use/handle CAN Messages very differently; such that maintaining compatibility between them, and only using one library would create a very messy and over-engineered parser.

### CanRed (MCM)
CanRed uses the CanManager.h class to handle all of the CAN related functions and utilities. Since CanRed is intended to run on an SBC, with a full operating system, we can take advantage of using heap allocations and more intensive data structures that make programming and development easier. Such as using a `vector<vector<u8>>` as a buffer for Long Frames, where we can handle any number (limited by RAM) of simultaneous Long Frame groups.

### Automato (Modules)
All CAN handling code is contained inside of the Automato.h library, and is intended to be used on Microcontrollers. And as such, steps are taken to avoid any/all heap allocations, and general reduced memory usage. This mostly shows itself in the way we handle Long Frames. we need to hard-code the size of the LongFrame buffer at compile time. Which means we over-provision for what we expect, and waste all that memory in the mean time. Also, we use much simpler data structures, like the ACK buffer. Which, instead of being a hashmap, is just a standard array.