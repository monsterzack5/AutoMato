<!-- Some things to add:
- Talk about online flows,
- Talk about the actual hardware setup
- Future Plans
-->
# A High Level Overview For The Automato System
AutoMato, is specifically the library that runs on microcontrollers, but it is only a single part of the system. Other parts are CanRed and our NodeRed integration. CanRed and NodeRed both run on a module that's generally known as the MCM, aka the Main Control Module. This module is intended to be an SBC running a linux environment.

## AutoMato
The AutoMato library does a few things, specially dealing with interacting with the entire outside system. A few ways it does it is by dealing with interfaces and frames.

- ### Frames
Originally designed around CANBUS, our frame format follows the standard CANBUS design of having a 32bit ID and 8 data bytes. Encoded inside of the ID are a few different things, mainly the 11bit ID of the module itself, another 11bit ID for the module the frame is for, and a few flag bits. Mainly marking if this frame is part of a LongFrame group, and the priority of the message.

- ### Long Frames
Since designed around CANBUS, our frames have a max size of 8 bytes. Which is not nearly enough to support all the different possible kinds of messages. Internally the system splits up messages larger than 8 bytes into 7 byte chunks, keeping the 8th byte as a uid of the frame group. On the receiving side of things, AutoMato internally combines the group back into one contiguous message, and parses it as it would a single frame. 

- ### Interfaces
Interfaces are the generic way of implementing how to connect to a bus. Currently we have 3 included interfaces; one for a MCP2515, another for Serial on modules, and a 3rd for Serial that specially works on the Linux kernel. Internally the system splits up LongFrames automatically and handles dropped frames. This is so interfaces just need to have a system for sending frames. Although currently this does cause slowdowns on interfaces that don't require only sending 8 bytes at a time. 

# CanRed
CanRed is a cpp program that runs on the MCM, which acts as the main interface for all the modules. CanRed internally keeps a database of all modules, their events and any errors they may have generated.

# NodeRed
NodeRed is the main automation program we interface with, where we include a custom wrapper for it inside of this repository.

# Protocol
Here's an example of our protocol and frame format, showing serialization and how we handle LongFrame groups. Imagine this conversation between 2 modules, the MCM and the AutoStart module.

The AutoStart module wants to send 3, 4 byte integers. Integers A,B,C and D.

```
First AutoStart Message:
Byte[0]: Unique ID for this Frame Group
Byte[1]: Protocol::REPLY_U4_BYTES
Byte[2]: A1
Byte[3]: A2
Byte[4]: A3
Byte[5]: A4
Byte[6]: Protocol::REPLY_U4_BYTES
Byte[7]: B1

Second AutoStart Message:
Byte[0]: Unique ID for this Frame Group
Byte[1]: B2
Byte[2]: B3
Byte[3]: B4
Byte[4]: Protocol::REPLY_U4_BYTES
Byte[5]: C1
Byte[6]: C2
Byte[7]: C3

Third AutoStart Message:
Byte[0]: Unique ID for this Frame Group
Byte[1]: C4
```
LongFrame groups are marked as finished when the last frame in the group has less than 8 data bytes. This does mean that if the message can be perfectly divided into 7 byte chunks, one final message must be sent containing just the uid for the frame group.

# MCM (Main Control Module)
The MCM was created to serve as an overseer, handling and processing any information from the outside world. Running both CanRed and NodeRed.

# Modules
Modules are separate microcontrollers which are meant to accomplish one task, or one small subset of tasks effectively. They can either "read" data from their environment, such as air temperature modules, and/or "write" to their environment, such as unlocking the doors, rolling up a window, turning on the wipers, etc.

Currently, modules are setup using a JSON configuration string that is passed to the AutoMato libraries constructor. This current implementation is a janky solution that will be replaced at some point in the future. 
<!-- if only these are a simple way to implement reflection in cpp, sigh -->

Here's an example of a configuration string:
```json
{
    "Name": "AutoStart",
    "Type": "reader+writer",
    "Description": "Controls all of the functions of a car remote!",
    "Commands": [
        {
            "CommandName": "Lock The Doors",
            "CommandID": 1,
            "ReturnFormat": "-"
        },
        {
            "CommandName": "Unlock The Doors",
            "CommandID": 2,
            "ReturnFormat": "-"
        },
        {
            "CommandName": "Start The Car",
            "CommandID": 3,
            "ReturnFormat": "-"
        },
        {
            "CommandName": "Is the car running?",
            "CommandID": 4,
            "ReturnFormat": "bool"
        }
    ]
}

```

# Broadcast Events / Offline Events
Broadcast Events are a system-wide internal automation mechanism, where you can program in "Flows" that can check every so often for some input, and respond accordingly. They are implemented using 3 blocks, Events, Commands, and If blocks.

An Example flow: While the car is running, every 5 minutes, check if the ambient light level is below a certain threshold. If it is, turn on the headlights, if not, turn off the headlights.

A Block Diagram:
```
                                                -----------------   
---------------    ------------  |> True Leg -> | Command Block |
| Event Block | -> | If Block | -|              -----------------
---------------    ------------  |               -----------------
                                 |> False Leg -> | Command Block |
                                                 ----------------- 
```

- ## Event Blocks
Each flow starts with an Event block, which is given a ModuleFunction to run, an interval for how often to run it, some expected output and a way to compare that output.  

- ## Command Blocks
Commands are simple, they just contain a ModuleFunction to run. They are called whenever they are next in a flow.

- ## If Blocks
If blocks take in an input from earlier in the flow and compare it to an expected output. If the comparison returns true, they run whatever is next in their true leg flow, if false, they run whatever is next in their false leg flow.

## Implementation
Currently, we use NodeRed as a UI for creating these flows using "Offline" nodes. Once a flow in NodeRed is deployed, they are parsed by our NodeRed wrapper and "compiled". These compiled blocks get sent to the modules via CanRed where they are then stored in EEPROM. Each module only contains blocks that are specific to the module itself. 

# Power States
Since most modules don't require being ON all the time, we have a system of power states controlled by the MCM. Internally they are designed to be able to be switched on and off by the MCM itself, but an ancillary approach of having the modules go into a low power mode on their own is also planned.

We have 4 main power states which are:
- S0: Fully Powered On
- S1: Low Power Sleep
- S4: Deep Sleep
- S5: Fully Powered Off  

### S0
When the car is running, all modules can be turned on and fully powered.

### S1
Modules go into S1 when they are potentially needed when the car is turned off.

### S4
Modules in this state should be internally set to wake up via interrupt only. Ideally a bitmask is set on the MCP2515 chip, which can wake up the module if it receives the correct frame. 

### S5
Modules that are definitely not needed when the car is turned off. Such as ambient
air temperature modules, wiper control modules, etc.
