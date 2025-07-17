#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5 // red
#define EEPROM_D7 12 // white
#define WRITE_EN 13
// OE is manually controlled on the board due to the lack of pins on the arduino
// Adding an additional shift register (or a D flip flop) seems like a waste of resources
// It is only changed in between modes anyways
// I can't connect the EEPROM data lines to a shift register as I don't have a bidirectional shift register

#define MODE_NONE 0
#define MODE_ROMWR 1
#define MODE_ROMRE 2
#define MODE_ROMFWR 3
#define MODE_ROMFRE 4

int mode = 0;

void setup() {
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(WRITE_EN, HIGH);
  pinMode(WRITE_EN, OUTPUT);

  Serial.begin(57600);
}

// sets pinmode for all data lines
void pinModeData(int mode) {
  for (int i = EEPROM_D0; i <= EEPROM_D7; i++) {
    pinMode(i, mode);
  }
}

void printBinary(byte inByte) {
  for (int b = 7; b >= 0; b--) {
    Serial.print(bitRead(inByte, b));
  }
}

void setAddress(uint16_t address, bool outputEnable) {
  if (outputEnable) {
    address &= 0x7FFF;  // Clear MSB (bit 15 = 0)
  } else {
    address |= 0x8000;  // Set MSB (bit 15 = 1)
  }

  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);

  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

void romWrite(uint16_t address, uint8_t value) {
  setAddress(address, false);

  // Write data to the ROM's data lanes
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    digitalWrite(pin, value & 1);
    value = value >> 1;
  }

  // Pulse the EEPROM write pin. Wait for it to do it's job
  digitalWrite(WRITE_EN, LOW);
  delayMicroseconds(10);
  digitalWrite(WRITE_EN, HIGH);
  delay(10);  // let it write
}

uint8_t romRead(uint16_t address) {
  setAddress(address, true);

  delay(1); // let it settle

  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin -= 1) {
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}

void writeROMInteractive() {
  while (true) {
    if (Serial.available()) {
      if (Serial.peek() == 'q') {
        Serial.read();  // consume 'q'
        mode = MODE_NONE;
        break;
      }

      uint16_t addr = Serial.parseInt();
      uint8_t value = Serial.parseInt();

      Serial.print("ADR: ");
      printBinary(addr >> 8);  // upper bits
      Serial.print(" ");
      printBinary(addr);  // lower bits
      Serial.print(" VAL: ");
      printBinary(value);
      Serial.println();

      romWrite(addr, value);

      while (Serial.available()) Serial.read();  // Clear buffer
    }
  }
}

void readROMInteractive() {
  while (true) {
    if (Serial.available()) {
      if (Serial.peek() == 'q') {
        Serial.read();  // consume 'q'
        mode = MODE_NONE;
        break;
      }

      uint16_t addr = Serial.parseInt();
      uint8_t value = romRead(addr);

      Serial.print("ADR: ");
      printBinary(addr >> 8);  // upper bits
      Serial.print(" ");
      printBinary(addr);  // lower bits
      Serial.print("0x");
      Serial.print(addr, HEX);
      Serial.print("    VAL: ");
      printBinary(value);
      Serial.print("0x");
      Serial.print(value, HEX);
      Serial.println();

      while (Serial.available()) Serial.read();  // Clear buffer
    }
  }
}

void writeROMFile() {
  Serial.println("Waiting for file...");
  uint16_t startaddr = 0;

  unsigned long lastByteTime = millis();
  const unsigned long timeout = 10000;  // 10 seconds timeout

  while (millis() - lastByteTime < timeout) {
    if (Serial.available()) {
      for (int i = 0; i < 64; i++) {
        while (!Serial.available()) {
          if (millis() - lastByteTime >= timeout) {
            Serial.write('D');  // Timed out
            return;
          }
        }

        uint8_t value = Serial.read();
        romWrite(startaddr, value);
        startaddr++;
        lastByteTime = millis();
      }

      Serial.write('O');  // Confirm 64-byte chunk written
    }
  }

  Serial.write('D');  // Timed out before receiving full chunk
}

void readROMFile(uint16_t size) {
  for (int addr = 0; addr < size; addr++) {
    uint8_t val = romRead(addr);
    Serial.write(val);
  }

  Serial.println("Dump done.");
}

void loop() {
  switch (mode) {
    case MODE_ROMWR:
      Serial.println("Interactive ROM write mode. Send \"q\" to quit. Syntax: <addr> <val>");
      pinModeData(OUTPUT);
      while (Serial.available()) Serial.read();  // flush
      writeROMInteractive();
      break;

    case MODE_ROMRE:
      Serial.println("Interactive ROM read mode. Send \"q\" to quit. Syntax: <addr>");
      pinModeData(INPUT);
      while (Serial.available()) Serial.read();  // flush
      readROMInteractive();
      break;

    case MODE_ROMFWR:
      Serial.println("File write mode. Run the desktop client with options \"write <serial_device> <file_to_send>\". Will quit if there is no input for 10 seconds");
      pinModeData(OUTPUT);
      while (Serial.available()) Serial.read();  // flush
      writeROMFile();
      mode = MODE_NONE;
      break;

    case MODE_ROMFRE:
      {
        Serial.println("File read mode. Run the desktop client with options \"dump <serial_device> <size>\"");
        pinModeData(INPUT);
        while (Serial.available()) Serial.read();  // flush

        while (Serial.available() < 2) {}  // Wait until 2 bytes arrive

        // uint8_t low = Serial.read();   // LSB
        // uint8_t high = Serial.read();  // MSB
        // uint16_t size = (high << 8) | low;

        uint16_t size = Serial.parseInt();

        readROMFile(size);

        mode = MODE_NONE;
        break;
      }

    default:
      Serial.println("Available modes:");
      Serial.println("1. ROM manual write");
      Serial.println("2. ROM manual read");
      Serial.println("3. ROM file write");
      Serial.println("4. ROM file read");
      while (Serial.available()) Serial.read();  // flush
      while (true) {
        if (Serial.available()) {
          mode = Serial.parseInt();
          break;
        }
      }
      break;
  }
}