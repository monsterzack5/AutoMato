<!-- 
TODO: 
Talk more in depth about:
- Specially what CanRed does and can do
- Specially about the NodeRed wrapper
- Specially about how to actually make your own modules
- How we use the NodeRed API, and the NodeRed nodes we have implemented
- How to properly develop with this repo
- Talk more about subnetworks and where you'd use them.
 -->

Although near completely functional, AutoMato and generally everything in this repository are in an alpha state. Error handling and reliability are areas that are definitely lacking, and need to be improved before I can recommend actually using this inside of your car.

# AutoMato
A car specific framework for automation, that handle's the more annoying things for you.

# About
Automating things can be annoying, especially when you want to have several different microcontrollers each doing a specific task somewhere in the car, and yet can still talk to all the other microcontrollers, and even further accept commands from a central controller. Some considerations to make are:
- Modules talking to each other
- Modules sending back information to the central controller
- Being able to use this information easily

AutoMato implements 3 things to help abstract away all the painful parts. CanRed, which acts as the central controller, responsible for managing the modules. The AutoMato library which has some small setup in your main() function on your microcontrollers, and handles all the internals of the system for you. Finally, a custom NodeRed wrapper which let's you leverage the full power of NodeRed for all of your modules.

# Documentation
Information about specific systems and general development info can be found in `/Documentation`. But, as most of the documentation was written in markdown files without previewing them, they don't appear entirely correct when viewed on a browser. Oops!
The docs are only loosely organized and are by no-means extensive. In the future, as more systems get ironed out, documentation will improve drastically.

# How to Use
In the "finished" system, CanRed and NodeRed are both ideally ran on an SBC (a Raspberry Pi)inside of your car, that has the ability to turn itself off to conserve the cars battery. Modules can either be connected to a fulltime 12v source, or a switched source. CanRed can be linked to the modules via a CANBUS network or the modules can be daisy chained to each other via Serial, or even using both Serial and CANBUS at the same time.

Modules are intended to be user programmed, where the user can create specific functions to do specific tasks, and then register these with the AutoMato library so that they can be called from anywhere. A rough example of that:
```cpp
AutomatoReturnData LockAllCarDoors(const void*)
{
    bool did_doors_lock = lock_all_the_doors();

    if (did_doors_lock) {
        return automato_return(true);
    } else {
        return automato_return(false);
    }
}

int main() {
    Automato automato(configuration_string);

    // The "1" here corresponds to the ID of the function,
    // ideally you'll be able to leave this out in the future.
    automato.register_callback(1, LockAllCarDoors);

    while (true) {
        automato.loop();
        // Other user specific code here
    }
}
```


# Working Features
- Module to central controller communication (Star Networking)
- Supports Zephyr, Arduino, STM32, ESP8266/32/32-C3
- JSON based unix socket API (Working, but very limited)
- Module linking via Serial or CANBUS
- "Online" Automation, using NodeRed
- "Offline" Automation, without needing NodeRed
- Independent sub-networks
- Easily extensible API for modifying several parts of the system.

# Planned / In Development
- Module to module communication (This works for the event system, but not for user functions)
- Module health checks (Currently half-baked)
- An easy to use error system, where module runtime errors can be sent to CanRed
- Many more JSON IPC features
- Integrating with other Automation frameworks, such as HomeAssistant
- Integrating a SIM7000A module for LTE/CAT-M1 network contectivity


# Going into a bit more depth
- ## Online and Offline Automation
Online Automation flows are flows that are created inside of NodeRed, that need to link back into NodeRed at some point. For example, having a NodeRed flow that, at 10:30pm sends a request to a module asking for the current temperature and then sends the temperature to some API over the internet.

Offline Automation flows are created the same way inside of NodeRed, but are simpler flows that can operate without NodeRed. Currently we're just using NodeRed as a sort of UI for these flows. After you create one of these flows, our custom NodeRed wrapper will parse the flow and compile it down into "Event" blocks. These blocks are sent to the modules and stored in EEPROM/NVS. This system allows modules to communicate to each other in a Mesh style without the need for CanRed, which is ideal if the car is powered off and you prefer not having a Raspberry Pi slowly draining your car battery.

- ## JSON UNIX Socket API
CanRed creates and listens to any socket connections to `/tmp/CanRed/red.sock`. This API can be used as an IPC system for any other programs running on the Raspberry Pi. Internally it's how NodeRed talks to CanRed, and vise versa.

- ## Independent Sub-networks
Modules can be connected to each other in all sorts of ways, and the AutoMato library has a few different ways of handling this. 

For example, this is a perfectly valid setup:
```
                                                ------------            ----------------
                                             >  | Module 2 | <-CANBUS-> | Non-Automato |
                                             |  ------------            |    Module    |
----------            ------------           |  ------------            ----------------
| CanRed | <-Serial-> | Module 1 | <- CANBUS +> | Module 3 | 
----------            ------------           |  ------------
                            ^                |  ------------
                            |                >  | Module 4 |
                            |                   ------------
                            | Serial
                            |   ------------    
                            - > | Module 5 |
                                ------------
```
Using the `translate_between_layers` API, you can have multiple networks and sub-networks happily interact with each other.

- ## Easily extensible API
Although the AutoMato library tries to implement specialized wrapper code for interacting with EEPROM and connecting to a network (Like CANBUS/Serial), sometimes you want to be able to use something else, like implementing LINBUS. You can implement these yourself and simply pass the implementations to the AutoMato constructor (for the EEPROM API) or the `.add_interface` api (for BUS interfaces). Currently this system uses runtime-polymorphism since it was the easiest to implement, but avoiding the overhead of vtables is definitely planned in the future. So although this feature does work, and is definitely planned to be kept in, it is subject to being greatly modified in the future.
