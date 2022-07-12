#include <pgmspace.h>
#include <stdint.h>

const char CONFIG[] PROGMEM = R"=====(
{
    "Name": "AutoStart",
    "Type": "writer",
    "Description": "Controls all of the functions of a car remote!",
    "UUID": "IMPLEMENT-ME",
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
            "CommandName": "Blink the Light",
            "CommandID": 4,
            "ReturnFormat": "-"
        }
    ]
}
)=====";
