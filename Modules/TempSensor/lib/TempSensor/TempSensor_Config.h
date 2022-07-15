#include <pgmspace.h>
#include <stdint.h>

const char CONFIG[] PROGMEM = R"=====(
{
    "Name": "TempSensor",
    "Type": "reader",
    "Description": "Reads the temperature and humidity",
    "Commands": [
        {
            "CommandName": "Read Humidity",
            "CommandID": 1,
            "ReturnFormat": "float"
        },
        {
            "CommandName": "Read Temperature in Fahrenheit",
            "CommandID": 2,
            "ReturnFormat": "float"
        },
        {
            "CommandName": "Read Temperature in Celsius",
            "CommandID": 3,
            "ReturnFormat": "float"
        }
    ]
}
)=====";
