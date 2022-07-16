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

Although near completely functional, AutoMato and generally everything in this repository are in an alpha state. Error handling and reliability are areas that are definitely lacking, and need to be improved before we can recommend actually using this inside of your car.

# AutoMato
A car specific framework for automation to breathe new life into an old car.

# About
We all have things we wish our car could do, especially if you have an older one. AutoMato seeks to give users the ability to program automations to make that happen. AutoMato is an extensible framework that allows users to create modules that control features of the car based on events or user input. 

AutoMato implements 3 things to help abstract away all the complexities of car automation:
- ## AutoMato Embedded Library
  - Provides an easy yet powerful way for user created modules to talk to the rest of the AutoMato framework, along with options to customize and extend it.

- ## CanRed
  - The centralized control program for all the modules, handles communication between the modules and Node Red along with fundamental infrastructure that is necessary for the system to function.

- ## NodeRed Integration
  - An intuitive interface that allows you to easily build event based automation flows using a flowchart style interface.

## Modules
Modules are pieces of hardware that contain a microcontroller and specific circuitry to control features of the car such as the windows, climate controls, or windshield wipers. Modules can be connected together via CANBUS or serial by default, but other interfaces can be added in through our library. Modules can be powered all the time (S0) or can be powered only when the car is in ACC (S1). It is planned to have a module to make the car go into ACC to access any S1 modules that are in an offline flow.

users are encouraged to control as much as they can with a single module due to power draw. Simply put, the fewer modules the car has the less quiescent current will be drawn when the car is off.

## Automation

Automation flows can be created very easily using NodeRed and the AutoMato plugin for it. The modules that are connected to the system will be added dynamically to the interface with the help of CanRed when you add them to the network.

There are two types of flows that can be created:
- ### Online Event Flows
Online event flows require the SBC (ex: Raspberry Pi) to be powered up in order for them to work. These are generally meant to be used only when the car is running because it has a high power draw. With this configuration, NodeRed is actively handling module events and issuing commands. This allows you to use the powerful existing library of NodeRed nodes with your flows.  
For example: using a PID control node for the cabin temperature of your car.
- ### Offline Event Flows
Offline event flows can be run when the car is on or off, allowing you to have flows that can be run 24/7. These are compiled from NodeRed into "Event Blocks" and spread throughout the modules used in the flow.  
For example: using a rain sensor to roll up the windows in your car.

# Documentation
Information about specific systems and general development info can be found in `/Documentation`.
The docs are only loosely organized and are by no-means extensive. In the future, as more systems get ironed out, documentation will improve drastically.

# How to Use
In the "finished" system, CanRed and NodeRed are both ideally ran on an SBC (ex: Raspberry Pi) inside of your car with the ability to turn itself off to conserve power. As mentioned above, modules can either be connected to a fulltime 12v source, or the cars ACC line. CanRed can be linked to the modules via a CANBUS network or the modules can be daisy chained to each other via Serial. You can even mix and match depending on what is best suited for your application.

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
- [x] Module to central controller communication (Star Networking)
- [x] Supports Zephyr and Arduino frameworks
- [x] JSON based unix socket API (Working, but very limited)
- [x] Module linking via Serial or CANBUS
- [x] "Online" Automation, using NodeRed
- [x] "Offline" Automation, without needing NodeRed
- [x] Independent sub-networks
- [x] Easily extensible API for modifying several parts of the system.

# Planned / In Development
- [ ] Module to module communication (This works for the event system, but not for user functions)
- [ ] Module health checks (Currently half-baked)
- [ ] An easy to use error system, where module runtime errors can be sent to CanRed
- [ ] Many more JSON IPC features
- [ ] Integrating with other Automation frameworks, such as HomeAssistant
- [ ] Integrating a SIM7000A module for LTE/CAT-M1 network contectivity
- [ ] Support for STM32Cube and Expressif frameworks


# Going into a bit more depth
- ## Online and Offline Automation
  - ### Online Automation Flows
    - flows that are created inside of NodeRed using online event nodes. These flows can use any NodeRed nodes including third party ones. We utilize CanRed to read the message from the module, interpret which node it is from, and tell NodeRed that an event happened. For commands, NodeRed sends the command to the correct module based on the information provided in the node to CanRed.

  - ### Offline Automation Flows
    - flows that are also created inside of NodeRed except users must use offline event nodes. These flows are limited to other offline event nodes as they need to be parsed and compiled into "Event Blocks" that are saved to the modules. When an "Event Block" is triggered, the initilization module runs a command and/or sends a command to another module with an event ID. That module will then run its own "Event Block". This will continue until the flow has completed. NodeRed simply acts as an interface to construct these offline flows, it is not actively involved in the flows as the modules communicate in a mesh style.

- ## JSON UNIX Socket API
CanRed creates and listens to any socket connections to `/tmp/CanRed/red.sock`. This API can be used as an IPC system for any other programs running on the Raspberry Pi. Internally it's how NodeRed talks to CanRed, and vise versa.

- ## Independent Sub-networks
Modules can be connected to each other in all sorts of ways, this allows flexibility and the ability to overcome device number limits on busses if neccessary.

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
Currently this system uses runtime-polymorphism since it was the easiest to implement, but avoiding the overhead of vtables is planned in the future. __The API is subject to drastically change in the future__.  

Although the AutoMato library tries to implement specialized wrapper code for interacting with EEPROM and connecting to a network (Like CANBUS/Serial), sometimes you want to be able to use something else, LINBUS or Z-Wave. We allow users to be able to implement this themselves by passing the implementation to either the AutoMato constructor (in the case of a custom EEPROM or NVS implementation) or by using the `.add_interface` API for custom bus interfaces. 
