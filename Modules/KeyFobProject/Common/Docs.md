
TODO: Fully document this project!

Message Format:

rolling code, serial number, command

```
-------------------------------------------------------------------- ----------------
| 1 Byte Marker | 4 Byte IV | 4 Byte Rolling Code | 1 Byte Command | | 16 Byte GMAC | 
-------------------------------------------------------------------- ----------------
                            ^------Encrypted With AES128-GCM-------^
```

Total Array Size: 26 -> (1 + 4 + 4 + 1 + 16)

KeyFob Order Of Operations:
- Generate Random Nonce
- Combine Rolling Code, Command Byte into 1 array,
  sizeof ((4 + 1))
- Combine that with the 4 byte nonce, non-encrypted
- Send that buffer!