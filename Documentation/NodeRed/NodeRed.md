# Node-Red Integration
Node-Red is used as our primary front end for creating automation flows.
We currently support 3 different flow systems, all with pros and cons.
For 2 of the systems, they are event based, and work by sending a compiled
version of the event onto the module itself.

### Star Vs Mesh Automation
Star Automation: The command output of one module needs to be sent to the MCM, 
for it to be processed. Then, the MCM will determine what needs to be done, 
and possibly send a command to a different module, repeating this cycle.

Mesh Automation: Modules can directly talk to each other, and a connection
to the MCM is not required.

## Events
Events are compiled versions of Node-Red flows, which are stored on the modules
themselves. This system offers two main advantages; first it allows distributing
work load to the modules themselves, and secondly, it greatly reduces CANBUS noise.
Events work off of an interval system, where they will check the output of some function
every X milliseconds/seconds/minutes, etc, and compare that output against some
expected output. 

We have two main systems of events, Offline and Online events, and both systems
support Main and Secondary event types.

### Difference between Main and Secondary Events
Given that a flow might be extensive, and contain many conditionals, we don't want
to store an entire flow on one module. We will quickly use up all of its available EEPROM.
So we split up flows into sections, and store the section on whatever module it's
applicable to.

### Main Events
Main events store the first part of a flow, which contains the interval of how often
a module should check the output of a user function.

A Main Event Contains:
- A unique ID for this Event
- User Function to run on the module
- Conditional, which defines how to check the output of the function
- Value, which is what the conditional checks against
- An interval, how often to run the specified function
- 2 Section numbers, which will be called depending if the output
  of the conditional check is true or false
<!-- - The ID of an "Other" module, the module we will send something to -->
<!-- - An "Other" module function, the function we call when the condition is met -->

### Secondary Events
A Secondary event is a section of a flow after the initial section, and does
not contain an interval. They are only ran in reaction to input from another module.

A Secondary Event Contains:
- unique ID, For this Event
- Section Number, which says what part in the flow we are in.
- User function to run
Optionally:
- Conditional, which defines how to check the output of the function
- Value, which we will check against
- 2 more Section numbers, which define which part we will call next
  if the condition is true or false. 

## System 1: Offline Events "Mesh Automation"
Offline events are events that are entirely compiled, and stored on a modules
EEPROM. They work entirely independently from Node-Red, but are still created
using the Node-Red UI. We use "Offline Event" nodes in Node-Red for this purpose,
which only pass their saved configuration over to the next node as JSON.
Where the last node will send the JSON configuration of the flow over to
CanRed, which will compile down the flow into an offline Broadcast event.

Pros:
- Able to work when the MCM/Node-Red is offline
- Persistent over module reboots

Cons:
- Unable to do any direct interaction with Node-Red
- Unable to make extensively complex flows

## System 2: Online Events "Star Automation"
Online events are events that only store and compile to a single Main Event.
Online events are used such that, a function will run on an interval inside of
a Module, and when it's condition is met, will send that input back to the MCM,
which can then pass the data around in Node-Red.

Online events are a single block at the start of a flow, that define the single
Main event, and then any Node-Red nodes, including nodes that call a module function.
Due to the MCM needing to be online to use them, they are not stored locally on the
modules in EEPROM. On system startup, the MCM will send all needed broadcast events
out to the modules.

Pros:
- Can integrate into any Node-Red flow
- Can Be used for much more complex flows

Cons:
- Requires the MCM/Node-Red to be online

## System 3: Pure Node-Red "Star Automation"
Pure Node-Red is when you directly call a module command from a Node-Red node,
and use the output in inside of a Node-Red flow. This system is ideally for external
event triggers, such as another automation system sending an MQTT message in, requesting
that a user function be run on a module. In this mode, nothing extra is stored on the
modules.

Pros:
- Can be called from any external trigger, as often as needed
- Fully integrates the Node-Red automation style

Cons:
- Requires the MCM/Node-Red to be online
- Calling user functions extremely often can increase CANBUS load