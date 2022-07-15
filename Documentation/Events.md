# Events / Broadcast Events
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

Since most microcontrollers have limited amounts of ram, we currently try to compress serialized events as much as possible on the module side of things.A Block Diagram:
```
                                                -----------------   
---------------    ------------  |> True Leg -> | Command Block |
| Event Block | -> | If Block | -|              -----------------
---------------    ------------  |               -----------------
                                 |> False Leg -> | Command Block |
                                                 ----------------- 
```