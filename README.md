# arduino-eeprom-programmer
Arduino EEPROM programmer written and tested for 28C256 EEPROM. Should work with any EEPROM given proper timings.

## Features
- Manual rom write: Write single bytes with a interactive interface.
- Manual rom read: Read single bytes with a interactive interface.    

**Following modes are only meant to be activated by the client**    
- ROM file write: Send raw data in 64 byte blocks. Do not send more than 64 bytes. "O" means OK, "D" means done. Will exit after a certain timeout. Starts from address 0.
- ROM file read: Meant to be activated by the client. Send the number of bytes you wanna read. Arduino will reply with the data. Starts from address 0.

## Important notes
- The client only works when `picocom` is running in a seperate terminal. Otherwise it creates random corrupt bytes everywhere. It probably has something to do with termios' buffers. Will probably fix this, or not...

- Do not send anything to arduino before running the client. Just plug in the arduino and run the client.

- It's really slow atm but I am going to implement paged write on arduino side when I feel like it
