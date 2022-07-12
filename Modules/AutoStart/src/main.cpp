#include <AutoStart_Config.h>
#include <Automato.h>
#include <Log.hpp>
#include <MCP2515Interface.h>

AutomatoReturnData LockTheDoors(const void*)
{
    DEBUG_PRINTLN("Called LockTheDoors");
    return automato_return();
}

AutomatoReturnData UnlockTheDoors(const void*)
{
    DEBUG_PRINTLN("Called UnlockTheDoors");
    return automato_return();
}

AutomatoReturnData StartTheCar(const void*)
{
    DEBUG_PRINTLN("Called StartTheCar");
    return automato_return();
}

AutomatoReturnData BlinkTheLight(const void*)
{
    DEBUG_PRINTLN("Called BlinkTheLight");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(300);
    digitalWrite(LED_BUILTIN, LOW);
    return automato_return();
}

Automato automato(CONFIG, 0);

MCP2515Interface can_interface(D8);

void setup()
{
    ENABLE_DEBUG_LOGGING(115200);

    delay(2000);

    automato.dont_use_builtin_led();

    automato.register_callback(1, LockTheDoors);
    automato.register_callback(2, UnlockTheDoors);
    automato.register_callback(3, StartTheCar);
    automato.register_callback(4, BlinkTheLight);

    automato.add_interface(&can_interface);

    automato.setup();
}

void loop()
{
    automato.loop();
}
