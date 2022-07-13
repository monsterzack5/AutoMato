# KeyFob Project
Adding remote start to a 2000 Subaru Outback, what could possibly go wrong?
This project was created to test the capability of the AutoMato subsystem, and to find out where any pain points might arise.

# An Early Prototype of the Keyfob, 
This version of the KeyFob used an ESP8266 to send AT Commands to the STM32WLE5JC, as it made testing and development easier. In the current version we program the STM32 directly.
![Keyfob Front](Modules/KeyFobProject/Common/Images/keyfob_prototype_v1_front.jpeg)
![Keyfob back](Modules/KeyFobProject/Common/Images/keyfob_prototype_v1_back.jpeg)

# About
Using two STM32WLE5JC microcontrollers, this project uses P2P LoRa as the transmission protocol, and AES-GCM to secure the wireless communication. It contains 2 projects, the KeyFob itself, and the AutoMato module that stays inside of the car. The keyfob spends most of it's time in the lowest power state it can, whereas the module is intended to be connected to 12v power 24/7.

# Communication
A representation of what we send over LoRa:
```
---------------------------------------------------------------------------------------
| 1 Byte Marker | 4 Byte IV | 4 Byte Rolling Code | 1 Byte Command | 16 Byte GMAC Tag | 
---------------------------------------------------------------------------------------
                            ^-------Encrypted via AES-128-GCM------^
```

- ### Marker Byte
We scan for this whenever we get in a LoRa message that's the correct length we're expecting. It's just used as a quick way to short circuit and ignore random LoRa messages that we end up reading.
- ### IV / Nonce
Although it's recommended to use a 16 Byte IV for AES-GCM, we ended up going with only using 4 byte for a couple reasons, the biggest being that extra bytes seem to increase the LoRa latency by a decent amount (around +500ms). Further, 4 bytes gives us enough room for 4 billion lock/unlock/start-car requests, before we run into IV reuse issues. For these reasons, shrinking the IV to 4 bytes seems acceptable. 
- ### Rolling Code
We employee the industrial standard of using a rolling-code to prevent the ability to save and reuse old LoRa messages.
- ### Command Byte
This byte is what actually tells the other module what action to do.
- ### GMAC Tag
Although shrinking the IV to 4 bytes is acceptable, shrinking a GMAC tag does implore much more risk, and as such, is kept at 16 bytes despite the added latency.

# Design considerations
## LoRa Settings
- 125KHz - We use 125KHz vs 500KHz, as the loss in speed is negligible for our small message frames, and the greater range it gives us is desirable for a keyfob.
- SF10 - A spreading factor of 10 gives a decent balance between range and battery consumption. We originally tried for a spreading factor of 12, for the most range possible, but it proved too costly on battery life. Nearly cutting estimated battery life in half.

## Encryption
Ensuring data integrity and authenticity is vital to protecting against attacks. As such, using an encrypt+tag algorithm was the only way to go. AES-GCM offers all of those features, and by using an algorithm that combines both the encryption and tag generation in one step, it half's the cycles needed versus independently encrypting and then generating a tag for the message. Further, AES-GCM is widely implemented in the embedded world, making compatibility between crypto implementations much easier to facilitate. 

# References
https://en.wikipedia.org/wiki/Galois/Counter_Mode \
http://ww1.microchip.com/downloads/en/AppNotes/Atmel-2600-AVR411-Secure-Rolling-Code-Algorithm-for-Wireless-Link_Application-Note.pdf   