
const int PIN_HVAC = 2;
const int PIN_CON = 3;

const int LONG_SILENCE_MILIS = 50;
const int BIT_DURATION_MICROS = 833; //830
const int MID_BIT_DURATION_MICROS = 150; // time to wait between a bit start and the moment we sample it

unsigned long timeHighCon = 0;
unsigned long timeHighHvac = 0;

void printByte(byte var) {
  for (unsigned int test = 0x80; test; test >>= 1) {
    Serial.write(var  & test ? '1' : '0');
  }
}

void printState(const byte data[]) {
  bool on_off = bitRead(data[2], 5);
  Serial.print(on_off? "ON " : "OFF");
  int temp = 14 + (data[4] >> 1) & 0b11111;
  Serial.print(" SetPoint: ");
  Serial.print(temp);
  bool fan_fast = bitRead(data[5], 1);
  Serial.print(" Fan: ");
  Serial.print(fan_fast? "FAST" : "SLOW");
  int mode = data[2] & 0b111;
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
    
  byte data[16];
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
    byte b;
    byte r;
    for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
      delayMicroseconds(BIT_DURATION_MICROS);
      r = digitalRead(origin);
      if (byte_idx == 15) {
        byte_modified = true;
        r = bitRead(calculated_checksum, bit_idx);
      } else if (origin == PIN_CON && byte_idx == 2 && bit_idx == 5) {
        // on/off
        //byte_modified = true;
        //r = 1;
      } else if (origin == PIN_CON && byte_idx == 4 && (bit_idx > 1 && bit_idx < 5)) {
        // Temperature change
        // Range is from 1 - 16 from 00001 to 10000 with bit indices 54321 (this code only changes 432)
        //byte_modified = true;
        //r = 1;
      } else if (origin == PIN_CON && byte_idx == 5 && bit_idx == 1) {
        // Fan speed
        //byte_modified = true;
        //r = 1;
      }
      bitWrite(b, bit_idx, r);
      digitalWrite(dest, r);
    }
    
    if (byte_modified) {
      packet_modified = true;
    }
    
    data[byte_idx] = b;
    if (byte_idx == 14) {
      int16_t accum = 0;
      for (int i =0; i< 15; i++) {
        accum += data[i];
      }
      calculated_checksum = (byte)accum;
    } else if (byte_idx == 15 && !packet_modified) {
      if (calculated_checksum != b) {
        Serial.println("Checksum missmatch!");
        return;
      }
    }
    
    // parity bit
    delayMicroseconds(BIT_DURATION_MICROS);
    byte parity = digitalRead(origin);
    if (byte_modified) {
      parity = isEvenParity(b);
    } else if (parity != isEvenParity(b)) {
      Serial.print("Parity bit missmatch on byte ");
      Serial.print(byte_idx);
      Serial.print(": ");
      printByte(b);
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
        // Print packet
        if (origin == PIN_CON) {
          for (int i = 0; i < 16; i++){
            printByte(data[i]);
            Serial.print(" ");
          }
          Serial.println();
          printState(data);
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
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_CON, INPUT);
  //digitalWrite(PIN_CON, HIGH); // turn on pullup resistors
  pinMode(PIN_HVAC, INPUT);
  //digitalWrite(PIN_HVAC, HIGH); // turn on pullup resistors
}