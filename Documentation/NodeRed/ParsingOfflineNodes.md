# Parsing Offline Flows

## Input
The input flows.json is an array of objects in no sorted order

## Correct Format
- Flows always start with an "Event" Node
- "Event" Nodes must always have one child node
- "If" nodes can come after any block, but cannot be
  the last block.
- "If" nodes can only have one output


Example 1:
Event -> If 
            True  -> Command -> Command 
            False -> If 
                        True  -> Command
                        False ->

Example 2:
Event -> Command -> Command -> If
                                 True  ->
                                 False -> Command
Example 3:
Event -> If
            True  -> If
                        True  ->
                        False -> Command

            False -> If
                        True  -> Command
                        False ->



Now, how are we gonna break these down into broadcast events?

Example 1 AST:
<MainEvent>
<If>
<True>
<SecondaryEvent>
<SecondaryEvent>
<False>
<If>
<True>
<SecondaryEvent>
<False>

Example 2 AST:
<MainEvent>
<SecondaryEvent>
<SecondaryEvent>
<If>
<True>
<False>
<SecondaryEvent>


Now, we need to break them up, and store
the different sections on the modules
in EEPROM, which is limited in space
But we should not worry about that
right now. We can Optimize later.

Drawbacks:
- Minimum 2 (3?) Byte Overhead
- Seems hard to create the correct amount
  of buffer space.

Thoughts:
- We can combine events to save space
- How are we gonna even parse this?
- We can probably incorporate the event
  type into the section number,
  that would lose us 2 bits, so
  max size for a flow is 63
  which seems valid

MainEvent:
- unique_id
- section_number
- function_id_to_check
- condition
- value_to_check
- interval
- interval_unit
- other_module_uid
- other_module_function

SecondaryEvent:
- section_number
- unique_id
- module_function

IfEvent:
- unique_id
- section_number
- module_function
- conditional
- value_to_check
- next section number if true
- next section number if false

## Parsing Flows.json
Alright! Lets get down to it
We need to parse the flows.json
and create some data structure that we can easily parse down into
little byte sized pieces


Some things to think about:
We need to serialize this into Events, so how are we gonna do that?


Alright, For storage, how are we gonna parse these things?

Lets recreate flows.json, in our own format

So:
[AutomatoMainEvent | AutomatoSecondaryEvent | AutomatoIfEvent ]

Where:
- Each Element has an ID, and a pointer to the next ID
- Each IF node has two pointers, in an array
  where the first is given


## What is the normalized layout of the JSON right before serializing?

For These Two Examples:

Example 2:
Event -> Command -> Command -> If
                                 True  ->
                                 False -> Command
Example 3:
Event ->                                      (0)
         If                                   (1)
            True  -> If                       (2)
                        True  ->              (x)
                        False -> Command      (3)
                                              (X)
            False -> If                       (4)
                        True  -> Command      (5)
                        False ->              (x)
                        