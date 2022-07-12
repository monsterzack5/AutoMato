# Events
Events are ways of storing automation flows onto the modules

<!-- todo: yada yada yada -->

## Sending them to modules
How are we gonna send them over CAN?

CanID: <LOW Priority> <YES Long Frame Flag> <MCM uid> <Module uid>
Byte[0]: CAN::Protocol::EVENT_ADD_NEW
Byte[1..N]: Serialized Event



## DIFFing
Alright, so we right now know when we have changed flows
But we need someway to understand what exactly we need to change
to stay up to date

So, for example

Module 1 has 2 events, Module 2 has 3 events

Module 1:
- (A) Main Event (Section number 0)
- (B) Command (Section number 3)

Module 2:
- (C) IF (Section number 2) (true: 3, false: 4)
- (D) Command (Section number 1)
- (E) Command (Section number 4)

The user changes the flows, so now we have:

Module 1:
- (A) Main Event (Section number 0)
- (B) Command (Section number 3 -> 3) (MODIFIED)

Module 2:
- (C) IF (Section number 2) (true: 3, false: 0)
- (D) Command (Section number 1) (MODIFIED)
- (E) Command (Section number 4) (DELETED)


What exactly do we need to change, and in what order?

So, for our current system, we should take these steps:

Tell Module 1 to mark B as deleted
Tell Module 1 to store B as if it were new
Tell Module 2 to Mark D as deleted
Tell Module 2 to Mark E as deleted
Tell Module 2 to store D as if it were new


## Module Side Of Things
Alright, time to work on the module side of things!

What we need to do:
- Handling the event related CAN::Protocol's
- Store the events in EEPROM
- Read the Events from EEPROM
- Keep a buffer of flow_ids and section numbers?


<!-- 
Main Event Contains:
interval
interval_unit
conditional
value_to_check_against
this_fuction_id

IF Event Contains:
conditional
value_to_check_against
this_fuction_id
if_true
if_false
section_number

Command Event Contains:
this_function_id
section_number
next_section_number
-->