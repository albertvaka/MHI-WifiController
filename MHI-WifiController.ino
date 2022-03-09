
const int PIN_HVAC = 2;
const int PIN_CON = 3;

const int LONG_SILENCE_MILIS = 50;
const int BIT_DURATION_MICROS = 833; //830
const int MID_BIT_DURATION_MICROS = 110; // time to wait between a bit start and the moment we sample it

unsigned long timeHighCon = 0;
unsigned long timeHighHvac = 0;

bool controllerOnOff = false;
int controllerTemperature = -1;
int controllerFanSpeed = -1;
int controllerMode = -1;

bool myOnOff = false;
int myTemperature = -1;
int myFanSpeed = -1;
int myMode = -1;

void printByte(byte var) {
  for (unsigned int test = 0x80; test; test >>= 1) {
    Serial.write(var  & test ? '1' : '0');
  }
}

void printRawTemperatureInDegrees(byte raw) {
  // Celsius = (0.25 * raw) - 15
  // We can avoid using floats, though
  Serial.print((raw >> 2)-15);
  switch(raw & 0b11) {
    case 0b00:
      Serial.print(".0");
      break;
    case 0b01:
      Serial.print(".25");
      break;
    case 0b10:
      Serial.print(".5");
      break;
    case 0b11:
      Serial.print(".75");
      break;
  }
}

int parseTemperature(const byte data[]) {
  // Range is from 1 - 16 from 00001 to 10000 with bit indices 54321
  return 14 + (data[4] >> 1) & 0b11111;
}

bool parseOnOff(const byte data[]) {
  return bitRead(data[2], 5);
}

int parseFanSpeed(const byte data[]) {
  return bitRead(data[5], 1);
}

int parseMode(const byte data[]) {
  return data[2] & 0b111;
}

bool bitForTemperature(int temp, int bitNum) {
  // Range is from 1 - 16 from 00001 to 10000 with bit indices 54321
  return bitRead(((temp - 14) << 1), bitNum);
}

void printState(bool on_off, int temp, int fan, int mode) {
  Serial.print(on_off? "ON " : "OFF");
  Serial.print(" SetPoint: ");
  Serial.print(temp);
  Serial.print(" Fan: ");
  Serial.print(fan? "FAST" : "SLOW");
  Serial.print(" Mode: ");
  switch (mode) {
    case 0: Serial.print("AUTO"); break;
    case 1: Serial.print("DEHU"); break;
    case 2: Serial.print("COLD"); break;
    case 3: Serial.print("VENT"); break;
    case 4: Serial.print("HEAT"); break;
    default: Serial.print("????"); break;
  }
  Serial.println("");
}

byte countBits(byte val) {
  byte bcount = 0;
  byte numBits = 8;
  while(numBits > 0) {
    bcount += (val & 1 ? 1 : 0);
    val >>= 1;
    numBits--;
  }
  return bcount;
}

byte isEvenParity(byte val) {
  return (countBits(val) & 1 ? 1 : 0);
}

int incomingPacket(int origin, int dest) {
  //This gets called at the beginning of the start bit (first 0 after a long 1)
    
  byte original_data[16];
  byte modified_data[16];
  byte calculated_checksum;
  byte byte_idx = 0;
  bool packet_modified = false;
  while (true) {
    bool byte_modified = false;
    
    delayMicroseconds(MID_BIT_DURATION_MICROS); // wait so the sample point is in the middle of the bit
    
    // read start bit
    if (digitalRead(origin) == HIGH) {
      Serial.println("Start bit is high!");
      return;
    }
    
    //start bit
    digitalWrite(dest, LOW);
    
    // data byte
    byte original_byte;
    byte modified_byte;
    bool original_bit;
    bool modified_bit;
    for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
      delayMicroseconds(BIT_DURATION_MICROS);
      original_bit = digitalRead(origin);
      modified_bit = original_bit;
      if (origin == PIN_CON) {
        // Modify data sent by controller
        if (byte_idx == 15) {
          // Checksum
          modified_bit = bitRead(calculated_checksum, bit_idx);
        } else if (myOnOff != controllerOnOff && byte_idx == 2 && bit_idx == 5) {
          // On/off
          modified_bit = myOnOff;
        } else if (myTemperature != controllerTemperature && byte_idx == 4 && (bit_idx > 0 && bit_idx < 6)) {
          // Temperature
          modified_bit = bitForTemperature(myTemperature, bit_idx);
        } else if (myFanSpeed != controllerFanSpeed && byte_idx == 5 && bit_idx == 1) {
          // Fan speed
          modified_bit = myFanSpeed;
        } else if (myMode != controllerMode && byte_idx == 2 && bit_idx < 3) {
          // Mode
          modified_bit = bitRead(myMode, bit_idx);
        }
      }
      bitWrite(original_byte, bit_idx, original_bit);
      bitWrite(modified_byte, bit_idx, modified_bit);
      digitalWrite(dest, modified_bit);
    }
    
    if (original_byte != modified_byte) {
      packet_modified = true;
    }
    
    original_data[byte_idx] = original_byte;
    modified_data[byte_idx] = modified_byte;

    // Checksum
    if (byte_idx == 14) {
      int16_t accum = 0;
      for (int i =0; i< 15; i++) {
        accum += modified_data[i];
      }
      calculated_checksum = (byte)accum;
    } else if (byte_idx == 15 && !packet_modified) {
      if (calculated_checksum != original_byte) {
        Serial.println("Checksum missmatch!");
        return;
      }
    }
    
    // parity bit
    delayMicroseconds(BIT_DURATION_MICROS);
    byte parity = digitalRead(origin);
    if (original_byte != modified_byte) {
      parity = isEvenParity(modified_byte);
    } else if (parity != isEvenParity(original_byte)) {
      Serial.print("Parity bit missmatch on byte ");
      Serial.print(byte_idx);
      Serial.print(": ");
      printByte(original_byte);
      Serial.write(parity? " 1" : " 0");
      Serial.println();
      return;
    }
    digitalWrite(dest, parity);
    
    delayMicroseconds(BIT_DURATION_MICROS); // stop bit
    if (digitalRead(origin) == LOW) {
      Serial.println("Stop bit is low!");
      return;
    }
    digitalWrite(dest, HIGH);
    
    byte_idx++;

    //Wait until the next start bit begins
    //We can use this to synchronize since the transition stop->start is always 0->1
    unsigned long waitBeginTime = millis();
    while(digitalRead(origin) == HIGH) {
      unsigned long waitedTime = millis()-waitBeginTime;
      if (waitedTime > LONG_SILENCE_MILIS) {
        // End of packet
        if (byte_idx != 16) { 
          Serial.print("Packet has number of bytes (");
          Serial.print(byte_idx);
          Serial.print(") received from ");
          Serial.println(origin == PIN_CON? "controller" : "hvac");
          return;
        }
        if (origin == PIN_CON) {
          // Parse states
          bool original_onOff = parseOnOff(original_data);
          int original_temperature = parseTemperature(original_data);
          int original_fanSpeed = parseFanSpeed(original_data);
          int original_mode = parseMode(original_data);

          bool modified_onOff = parseOnOff(modified_data);
          int modified_temperature = parseTemperature(modified_data);
          int modified_fanSpeed = parseFanSpeed(modified_data);
          int modified_mode = parseMode(modified_data);
          
          // Print states
          
          Serial.println("Original:");
          for (int i = 0; i < 16; i++){
            printByte(original_data[i]);
            Serial.print(" ");
          }
          Serial.println();
          printState(original_onOff, original_temperature, original_fanSpeed, original_mode);

          if (packet_modified) {
            Serial.println("Modified:");
            for (int i = 0; i < 16; i++){
              printByte(original_data[i]);
              Serial.print(" ");
            }
            Serial.println();
            printState(modified_onOff, modified_temperature, modified_fanSpeed, modified_mode);
          } else {
            Serial.println("No changes made");
          }
          Serial.println("");

          // Detect changes made by humans on the controller
          if (original_onOff != controllerOnOff
              || original_temperature != controllerTemperature
              || original_fanSpeed != controllerFanSpeed
              || original_mode != controllerMode
          ) {
            controllerOnOff = original_onOff;
            controllerTemperature = original_temperature;
            controllerFanSpeed = original_fanSpeed;
            controllerMode = original_mode;
            Serial.println("Controller change detected, resetting my state.");
            Serial.println("");
            myOnOff = original_onOff;
            myTemperature = original_temperature;
            myFanSpeed = original_fanSpeed;
            myMode = original_mode;
          }

          // Not needed: when controller sensor is set to on, air reports back this same value
          //Serial.print("Controller temp: ");
          //Serial.println(original_data[6]);
          //Serial.println("");
            
        } else { // origin == HVAC
            Serial.print("Room temp: ");
            Serial.print(original_data[6]);
            Serial.print(" (");
            printRawTemperatureInDegrees(original_data[6]);
            Serial.println("C)");
        }

        // Update timeHigh
        if (origin == PIN_CON) {
          timeHighCon = millis()-waitBeginTime;
        } else {
          timeHighHvac = millis()-waitBeginTime;
        }
        return;
      }
    }
  }
}

void checkStartBit(unsigned long dt) {
  int con = digitalRead(PIN_CON);
  if (con == HIGH) {
    timeHighCon += dt;
  } else {
    if (timeHighCon > LONG_SILENCE_MILIS) {
      pinMode(PIN_HVAC, OUTPUT);
      incomingPacket(PIN_CON,PIN_HVAC);
      pinMode(PIN_HVAC, INPUT);
    }
    timeHighCon = 0;
  }

  int hvac = digitalRead(PIN_HVAC);
  if (hvac == HIGH) {
    timeHighHvac += dt;
  } else {
    if (timeHighHvac > LONG_SILENCE_MILIS) {
      pinMode(PIN_CON, OUTPUT);
      incomingPacket(PIN_HVAC, PIN_CON);
      pinMode(PIN_CON, INPUT);
    }
    timeHighHvac = 0;
  }
}

void loop() {
  static unsigned long lastTime = millis();
  unsigned long time = millis();
  checkStartBit(time-lastTime);
  lastTime = time;

  // debug button
  static bool last_button = false;
  pinMode(7, INPUT);
  bool button = digitalRead(7);
  if (button != last_button && button) {
    Serial.println("INCREASE TEMP");
    myTemperature++;
  }
  last_button  = button;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_CON, INPUT);
  //digitalWrite(PIN_CON, HIGH); // turn on pullup resistors
  pinMode(PIN_HVAC, INPUT);
  //digitalWrite(PIN_HVAC, HIGH); // turn on pullup resistors
}
