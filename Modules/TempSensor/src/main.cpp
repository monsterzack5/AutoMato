#include <Automato.h>
#include <DHT.h>
#include <MCP2515Interface.h>
#include <TempSensor_Config.h>

Automato automato(CONFIG);

// D8/GPIO 15/HSPI For an ESP8266
// Or GPIO 5/VSPI on an ESP32
MCP2515Interface can_interface(D8);

// D2/GPIO 4 on an ESP8266 or ESP32, No special meaning
DHT dht_sensor(D2, DHT22);

AutomatoReturnData ReadHumidity(const void*)
{
    float humidity = dht_sensor.readHumidity();
    return automato_return(humidity);
}

AutomatoReturnData ReadTemperatureFahrenheit(const void*)
{
    float temperature = dht_sensor.readTemperature(true);
    return automato_return(temperature);
}

AutomatoReturnData ReadTemperatureCelsius(const void*)
{
    float temperature = dht_sensor.readTemperature(false);
    return automato_return(temperature);
}

void setup()
{
    automato.register_callback(1, ReadHumidity);
    automato.register_callback(2, ReadTemperatureFahrenheit);
    automato.register_callback(2, ReadTemperatureCelsius);

    automato.add_interface(&can_interface);

    automato.setup();

    dht_sensor.begin();
}

void loop()
{
    automato.loop();
}
