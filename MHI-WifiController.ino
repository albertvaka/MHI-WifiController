
const int P_HVAC = 2;
const int P_CON = 3;

int lastTime = 0;

int timeHighCon = 0;
int timeHighHvac = 0;

const int LONG_SILENCE_MILIS = 50;
const int BIT_DURATION_MICROS = 833; //830
const int MID_BIT_DURATION_MICROS = 150; // time to wait between a bit start and the moment we sample it

void printByte(int var) { // aka newFunc
  for (unsigned int test = 0x80; test; test >>= 1) {
    Serial.write(var  & test ? '1' : '0');
  }
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

bool enable_debug = true;
void debug(const char* txt, const char* txt2=NULL) {
  if (!enable_debug) return;
  Serial.print(millis());
  Serial.print(": ");
  Serial.print(txt);
  if (txt2) {
    Serial.print(" ");
    Serial.println(txt2);
  } else {
    Serial.println("");
  }
}

void debug(const char* txt, int num) {
  if (!enable_debug) return;
  Serial.print(millis());
  Serial.print(": ");
  Serial.print(txt);
  Serial.print(" ");
  Serial.println(num);
}

void debugByteAndParity(int var, byte parity) {
  if (!enable_debug) return;
  Serial.print(millis());
  Serial.print(": ");
  for (unsigned int test = 0x80; test; test >>= 1) {
    Serial.write(var  & test ? '1' : '0');
  }
  Serial.write(parity? " 1" : " 0");
  Serial.println();
}


int incomingPacket(int origin, int dest) {
   //This gets called at the beginning of the start bit (first 0 after a long 1)
    
   byte data[16];
   byte by = 0;
   while (true) {
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
    delayMicroseconds(BIT_DURATION_MICROS);
    r = digitalRead(origin);
    bitWrite(b, 0, r == HIGH);
    digitalWrite(dest, r);
    delayMicroseconds(BIT_DURATION_MICROS);
    r = digitalRead(origin);
    bitWrite(b, 1, r == HIGH);
    digitalWrite(dest, r);
    delayMicroseconds(BIT_DURATION_MICROS);
    r = digitalRead(origin);
    bitWrite(b, 2, r == HIGH);
    digitalWrite(dest, r);
    delayMicroseconds(BIT_DURATION_MICROS);
    r = digitalRead(origin);
    bitWrite(b, 3, r == HIGH);
    digitalWrite(dest, r);
    delayMicroseconds(BIT_DURATION_MICROS);
    r = digitalRead(origin);
    bitWrite(b, 4, r == HIGH);
    digitalWrite(dest, r);
    delayMicroseconds(BIT_DURATION_MICROS);
    r = digitalRead(origin);
    bitWrite(b, 5, r == HIGH);
    digitalWrite(dest, r);
    delayMicroseconds(BIT_DURATION_MICROS);
    r = digitalRead(origin);
    bitWrite(b, 6, r == HIGH);
    digitalWrite(dest, r);
    delayMicroseconds(BIT_DURATION_MICROS);
    r = digitalRead(origin);
    bitWrite(b, 7, r == HIGH);
    digitalWrite(dest, r);

    data[by] = b;
   
    // parity bit
    delayMicroseconds(BIT_DURATION_MICROS);
    byte parity = digitalRead(origin);
    if (parity != isEvenParity(b)) {
      Serial.println("Parity bit missmatch!");
      debugByteAndParity(b, parity);
      return;
    }
    digitalWrite(dest, parity);

    delayMicroseconds(BIT_DURATION_MICROS); // stop bit
    if (digitalRead(origin) == LOW) {
      Serial.println("Stop bit is low!");
      return;
    }
    digitalWrite(dest, HIGH);
    
    by++;

    //debug("End of byte");
    //Wait until the next start bit begins
    //We can use this to synchronize since the transition stop->start is always 0->1
    int waitBeginTime = millis();
    while(digitalRead(origin) == HIGH) {
      timeHighCon = millis()-waitBeginTime;
      if (timeHighCon > LONG_SILENCE_MILIS) {
        Serial.println(waitBeginTime);
        Serial.print("Received ");
        Serial.print(by);
        Serial.print(" bytes from ");
        Serial.println(origin == P_CON? "controller" : "hvac");
        for (int i = 0; i < 16; i++){
          printByte(data[i]);
          Serial.print(" ");
        }
        Serial.println();
        return;
      }
      
    }
   }
}

void update(long dt) {
  int con = digitalRead(P_CON);
  if (con == HIGH) {
    timeHighCon += dt;
  } else {
    if (timeHighCon > LONG_SILENCE_MILIS) {
      pinMode(P_HVAC, OUTPUT);
      incomingPacket(P_CON,P_HVAC);
      pinMode(P_HVAC, INPUT);
    }
    timeHighCon = 0;
  }

  int hvac = digitalRead(P_HVAC);
  if (hvac == HIGH) {
    timeHighHvac += dt;
  } else {
    if (timeHighHvac > LONG_SILENCE_MILIS) {
      pinMode(P_CON, OUTPUT);
      incomingPacket(P_HVAC, P_CON);
      pinMode(P_CON, INPUT);
    }
    timeHighHvac = 0;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(P_CON, INPUT);
  //digitalWrite(P_CON, HIGH); // turn on pullup resistors
  pinMode(P_HVAC, INPUT);
  //digitalWrite(P_HVAC, HIGH); // turn on pullup resistors
  lastTime = millis();
}

void loop() {
  int time = millis();
  update(time-lastTime);
  lastTime = time;
}