# CAN - In Depth Protocol Documentation

## A Basic Can Frame

- CanID: 32 Bit
- Data: 0-8 Bytes

### CanID
Each frame sent over can requires a unique ID, if two messages contain
the same ID as each other, this causes the bus to create an Error Frame.
The ID is also used as a priority system, where the closer the ID is to 
all zeros, the higher the message priority. CanIDs can change on the fly,
and aside from priority, they do not have much intrinsic meaning.

In AutoMato, we use 11 bit IDs for each node (referred as the "Nodes uid").
We encode the senders uid, and the receiver's uid into each CanID.
We also encode two priority bits at the MSB of each CanID. Where Low Priority
can be encoded as "11..." and Max priority can be encoded as "00...".
Similarly we also encode the 3rd bit to signify if the frame is part of a
group of frames, where we need to transmit more than 8 bytes.

An example of a CAN Frame ID
                      Frame Format Flag
              Remote Transmission Flag|
                   Error Message Flag||
    Long Frame Flag                 |||
     v                              vvv
  00 1 0110 11110010111 11110111111 111
  <>   <--> <-From ID-> <--To ID-->
  ||   |--|
  ||   Reserved Bits
  Priority Bits

### Priority Bits
LOW  = 11
MED  = 10
HIGH = 01
MAX  = 00

### Long Frame Flag:
Used when >8 bytes are needed, the sending node
will split up the data into multiple frames, in our system we use the
first byte of every Frame Group as a unique ID that differentiates this group
of frames. This system has the advantage of allowing a Node to send regular
frames and in between long frame groups without causing any confusion, but
has the draw back of there being a possible collision with another node 
generating the same unique ID

<!-- Talk about the FF, RT and EM Flags, reserve bits, etc -->

## Looking For Lost Messages
We operate under the assumption that the CANBUS might be lossy, even though
it should very rarely drop any frames.

We use a MESSAGE then ACK system, where every sent message expects an ACK in
return. Our current implementation is not perfect, and has the following shortfalls:

- We don't store enough information to recreate lost frames, so we just report them as errors
- Frames and ACKs are asynchronous, where we don't wait after sending a frame for an ACK
  this has a couple issues, if we lost a frame while reading from a LongFrame group, we
  wouldn't know until 3 seconds after the frame was lost.
- ACKs could get lost like any other message, but that is a rabbit hole.

## Our Internal Protocol
We use an internal protocol for CAN messages, where you put a Protocol Specifier
before any usable data. In "Long Frames" We use the first byte as a unique
identifier for the frame group. In "Regular Frames", we use the first byte
as a Protocol Specifier.

Currently, all supported Protocols are in: `{root}/SharedLibraries/CanConstants.h`

An Example:

Module1 Asks Module2 to run Command1, which returns 1 4 byte float.

Module1:
CanID: <LOW Priority> <No Long Frame Flag> <Module1 uid> <Module2 uid>
Byte[0]: Protocol:COMMAND
Byte[1]: Command1 ID

Module2: 
CanID: <LOW Priority> <No Long Frame Flag> <Module2 uid> <Module1 uid>
Byte[0]: Protocol:ACKNOWLEDGEMENT
Byte[1]: Command1 ID

<Time for the module to run the command>

Module2: 
CanID: <LOW Priority> <No Long Frame Flag> <Module2 uid> <Module1 uid>
Byte[0]: Command1 ID
Byte[1]: Protocol:REPLY_F4_BYTES
Byte[2]: Float byte 1
Byte[3]: Float byte 2
Byte[4]: Float byte 3
Byte[5]: Float byte 4

Module1:
CanID: <LOW Priority> <No Long Frame Flag> <Module1 uid> <Module2 uid>
Byte[0]: Protocol:ACKNOWLEDGEMENT
Byte[1]: Command1 ID


## Some Specific Examples

### Module asking for a new ID From MCM
Module1:
CanID: <HIGH Priority> <No Long Frame Flag> <2050 > X > 2000> <MCM uid>
Byte[0]: Protocol:NEW_ID

MCM:
CanID: <HIGH Priority> <No Long Frame Flag> <MCM uid> <X uid>
Byte[0]: Protocol:ACKNOWLEDGEMENT
Byte[1]: Protocol:NEW_ID

MCM:
CanID: <HIGH Priority> <No Long Frame Flag> <MCM uid> <X uid>
Byte[0]: Protocol:REPLY_NEW_UID
Byte[1]: Upper 3 bits of uid
Byte[2]: Lower 8 bits of uid

Module1:
CanID: <HIGH Priority> <No Long Frame Flag> <2050 > X > 2000> <MCM uid>
Byte[0]: Protocol:ACKNOWLEDGEMENT
Byte[1]: Protocol:NEW_ID


### MCM Asking Module1 to update info
MCM: 
CanID: <MED Priority> <No Long Frame Flag> <MCM uid> <Module1 uid>
Byte[0]: Protocol:UPDATE_INFO

Module1:
CanID: <MED Priority> <Yes Long Frame Flag> <Module1 uid> <MCM uid>
Byte[0]: Protocol:ACKNOWLEDGEMENT
Byte[1]: Protocol:UPDATE_INFO

Module1:
CanID: <MED Priority> <Yes Long Frame Flag> <Module1 uid> <MCM uid>
Byte[0]: Unique ID (From the Long Frame Standard)
Byte[1]: Protocol:REPLY_INFO >>>>
Byte[2]: First Char
Byte[3]: Second Char
Byte[4]: Third Char
Byte[5]: Fourth Char
Byte[6]: Fifth Char
Byte[7]: Sixth Char

MCM: 
CanID: <MED Priority> <No Long Frame Flag> <MCM uid> <Module1 uid>
Byte[0]: Protocol:ACKNOWLEDGEMENT
Byte[1]: Protocol:REPLY_UPDATE_INFO

Module1:
CanID: <MED Priority> <Yes Long Frame Flag> <Module1 uid> <MCM uid>
Byte[0]: Unique ID (From the Long Frame Standard)
Byte[1]: Protocol:REPLY_INFO
Byte[2]: First Char
Byte[3]: Second Char
Byte[4]: Third Char
Byte[5]: Fourth Char
Byte[6]: Fifth Char
Byte[7]: Sixth Char

Etc ...

The Module will serialize it's entire configuration and send it
over CAN. This takes a decent amount of time and bandwidth. We can
improve the system by compressing the data, but that is currently
not implemented.

## Module I/O

### Module Replying With Data From Command
Now, lets say we wanna have the CCM ask a DHT22 Sensor for the temperature,
Where:
    - "Command ID 5" is the get_current_temp() function
    - get_current_temp() returns a float

CCM: 
CanID: <LOW Priority> <No Long Frame Flag> <CCM uid> <DHT22 uid>
Byte[0]: CAN::Protocol::COMMAND
Byte[1]: Command 5 ID

DHT22:
CanID: <LOW Priority> <No Long Frame Flag> <DHT22 uid> <CCM uid>
Byte[0]: Protocol:ACKNOWLEDGEMENT
Byte[1]: Command 5 ID

DHT22:
CanID: <LOW Priority> <No Long Frame Flag> <DHT22 uid> <CCM uid>
Byte[0]: CAN::Protocol::COMMAND_REPLY
Byte[1]: Command 5 ID
Byte[2]: CAN::Protocol::FLOAT_4_BYTES
Byte[3]: First Byte Of Float
Byte[4]: Second Byte Of Float
Byte[5]: Third Byte Of Float
Byte[6]: Fourth Byte Of Float

CCM:
CanID: <LOW Priority> <No Long Frame Flag> <CCM uid> <DHT22 uid>
Byte[0]: Protocol:ACKNOWLEDGEMENT
Byte[1]: Command 5 ID

### Passing Data To Module For A Command
We have two modules, the CCM, and an AirConController
We wanna have the CCM send a command to the AirConController
to set the temperature to 70 Degrees

Where:
    - "Command ID 3" is the command to set the temperature
    - "Command ID 3" Takes in a 2 byte signed integer

CCM: 
CanID: <LOW Priority> <No Long Frame Flag> <CCM uid> <AirConController uid>
Byte[0]: CAN::Protocol::COMMAND
Byte[1]: Command 3 ID
Byte[2]: CAN::Protocol::SIGNED_2_BYTES
Byte[3]: First Byte Of Temp
Byte[4]: Second Byte Of Temp

DHT22:
CanID: <LOW Priority> <No Long Frame Flag> <AirConController uid> <CCM uid>
Byte[0]: CAN::Protocol::ACKNOWLEDGEMENT
Byte[1]: Command 3 ID

DHT22:
CanID: <LOW Priority> <No Long Frame Flag> <AirConController uid> <CCM uid>
Byte[0]: CAN::Protocol::COMMAND_REPLY
Byte[1]: Command 3 ID

CCM:
CanID: <LOW Priority> <No Long Frame Flag> <CCM uid> <AirConController uid>
Byte[0]: CAN::Protocol::ACKNOWLEDGEMENT
Byte[1]: Command 3 ID

### Passing Data To A Module, And Having It Reply
When we want to send some data to a module, and want it
to return something!

Where:
    - "Command ID 8" is a command that opens a given window and
       returns if it was successful,

CCM: 
CanID: <LOW Priority> <No Long Frame Flag> <CCM uid> <WindowModule uid>
Byte[0]: CAN::Protocol::COMMAND_INPUT
Byte[1]: Command 8 ID
Byte[2]: CAN::Protocol::UNSIGNED_1_BYTES
Byte[3]: Window Number

WindowModule:
CanID: <LOW Priority> <No Long Frame Flag> <WindowModule uid> <CCM uid>
Byte[0]: Protocol:ACKNOWLEDGEMENT
Byte[1]: Command 3 ID

WindowModule:
CanID: <LOW Priority> <No Long Frame Flag> <WindowModule uid> <CCM uid>
Byte[0]: CAN::Protocol::COMMAND_REPLY
Byte[1]: Command 5 ID
Byte[2]: CAN::Protocol::BOOL_1_BYTE
Byte[3]: 1 Or 0

CCM:
CanID: <LOW Priority> <No Long Frame Flag> <CCM uid> <WindowModule uid>
Byte[0]: Protocol:ACKNOWLEDGEMENT
Byte[1]: Command 5 ID

<!-- We gotta figure out a more efficient way of doing this some day -->

For more information about parsing, see Parsing.md
