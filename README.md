# arduino-eeprom-programmer
Arduino EEPROM programmer written and tested for 28C256 EEPROM. Should work with any EEPROM given proper timings

## Important note
- The client only works when `picocom` is running in a seperate terminal. It probably has something to do with termios' buffers. Will probably fix this, or not

- It's really slow atm but I am going to implement paged writes on arduino side when I feel like it
