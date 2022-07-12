# CanRed Socket Interface
CanRed can read, parse and handle messages from UNIX Domain sockets.
The main way for external communication to CanRed, is through our JSON UNIX
Socket API.

CanRed listens for socket connections at `/tmp/CanRed/red.sock`. Clients are expected
to connect and send a request first, that is valid JSON and conforms to our spec.
Theres a few different kinds of requests, such as requesting a user command to be ran,
getting the configuration or updating the configuration of a module, and injecting
certain frames directly to a module, the last request is mainly used for testing.

## Output Format Identifier
For ease of using JSON, user function output's are returned as a String, with another
JSON key being used to define the output type. Types Are:

```
Signed   32-bit: 1
Unsigned 32-bit: 2
Signed   64-bit: 3
Unsigned 64-bit: 4
Float    32-bit: 5
Double   64-bit: 6
Boolean   8-bit: 7
No return value: 8
```


## Socket Request Format:
### Running a User Command
Format For Requesting a User Function to be run:
```jsonc
{
    "request_type": "user_function",
    "module_function": "Get Temperature",
    "module_name": "DHT22",
    "return_output": "True" // True means CanRed will reply back with the output of the function.
                            // False Means CanRed will reply when the module replies with ACK for the function.
}
```
Reply Format:
```jsonc
{
    "module_function": "Get Temperature",
    "module_name": "DHT22",
    "output": "<output as string>",
    "output_type": "<number defining output type>"
}
```

<!-- TODO: -->
<!-- Inject Frames -->
<!-- Config Reading -->