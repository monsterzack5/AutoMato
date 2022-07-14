# Modules
Modules are designed as a single dedicated microcontroller that handles a specific subgroup of tasks. In development, the microcontroller usually ends up being an ESP8266 for simple modules and an ESP32 for more advanced modules. Any microcontroller capable of running the Arduino or Zephyr framework will be able to support AutoMato, but cheaper MCU's such as ATtinys which don't have enough program space for the Arduino framework are currently not supported. (but support is planned!)

# A Rough Example
Here's a rough example of the steps needed to implement an "AutoStart" module. This is only intended to be a quick overview for demonstration purposes. This example is purposefully limited as there will be a full write-up for this module after it has been implemented in a car and all safety features are tested and verified 100% working. 

Even once a write-up is provided, we strongly recommend against messing with any safety features or internals of your car. We assume no liability for anything done to your vehicle.

Hardware Diagram
```
 ----------------              -----------------------
 | MCM / CanRed | <- CANBUS -> | Remote Start Module | 
 ----------------              -----------------------
                                 v-----GPIO Pins-----v
       |-------------------------|         |         |
       |> Relay for switching ACC          | v-------|  
                                           | |> Relay for Switching
                   |-----------------------| |> Starter signal wire
                   v
                   |> Relay for switching
                   |> ignition wire
```

Example main file using the Arduino framework. Again this is just an example.
```cpp
Automato automato(CONFIG);
MCP2515Interface CANBUS_interface(D8);

AutomatoReturnData StartTheCar(const void*) {
    // Enable ignition and ACC
    digitalWrite(ignition_pin, HIGH);
    digitalWrite(acc_pin, HIGH);

    // Turn on the starter
    digitalWrite(starter_pin, HIGH)

    while (true) {
        // There's multiple ways to check for this, via checking
        // if the alternator excitor pin is active, or
        // checking for 14v at the alternator, or
        // or using a CAN Sniffer on the cars OBDII port.
        if (has_car_started()) {
            digitalWrite(starter_pin, LOW);            
            // Implement some sort of timeout, to protect against
            // running the starter for long periods of time. 
        }
    }
}

void setup() {
    automato.register_callback(1, StartTheCar);
    automato.add_interface(&CANBUS_interface);

    // User setup code...
}

void lopp() {
    automato.loop();
    // User runtime code...
}
```

# More examples
Inside of this folder, there's an AutoStart folder which contains a working example
of how you would program the microcontrollers. Though with user functions that currently don't do much besides print debug information. 

More examples will be provided soon, as the system gets ironed out and API's are finalized.

# Hardware testing platforms
[Daniel](https://github.com/sugarbooty) designed and had prototyping PCBs made, and handles most of the hardware side of this project. Theres schematics and PCB layout files here [here](https://github.com/monsterzack5/AutoMato/tree/development/Modules/Common/Schematics/generic_module). Currently these boards support soldering on an ESP8266 (with some limitations), ESP32, or ESP32-S3 module. Along with a Raspberry Pi header, a MCP2515 CAN Controller, and a TJA1050 CAN Transceiver. 4 Logic level converter channels, and a "breakout" portion of the board for discrete circuitry and prototyping.

Here's an image of the boards:
![Generic Module PCBs](https://raw.githubusercontent.com/monsterzack5/AutoMato/development/Modules/Common/Images/generic_module_pcbs.jpeg)