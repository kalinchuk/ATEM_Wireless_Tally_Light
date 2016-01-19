#include <JeeLib.h>

// PROGRAM LED pin
int PROGRAM_PIN = A0;

// PREVIEW LED pin
int PREVIEW_PIN = A1;

// POWER LED pin (blinks the node # on power up)
int POWER_PIN = A2;

// Node # DIP switch 1 pin
int DIP1_PIN = 4;

// Node # DIP switch 2 pin
int DIP2_PIN = 5;

// Node # DIP switch 3 pin
int DIP3_PIN = 6;

// Node # DIP switch 4 pin
int DIP4_PIN = 7;

// last received radio signal time (to power off LEDs when no signal)
unsigned long last_radio_recv = 0;

// default Node # 200 (alias to 0) will blink constantly if signal exists
int this_node = 200;

// The current program 1 camera number
int program_1;

// The current program 2 camera number
int program_2;

// The current preview number.
int preview;

void setup() {
	// initialize all the defined pins
	pinMode(PROGRAM_PIN, OUTPUT);
	pinMode(PREVIEW_PIN, OUTPUT);
	pinMode(POWER_PIN, OUTPUT);
	pinMode(DIP1_PIN, INPUT);
	pinMode(DIP2_PIN, INPUT);
	pinMode(DIP3_PIN, INPUT);
	pinMode(DIP4_PIN, INPUT);

	// turn all LEDs off (1023 is off)
  analogWrite(PROGRAM_PIN, 1023);
  analogWrite(PREVIEW_PIN, 1023);
  analogWrite(POWER_PIN, 1023);  
  
	// create an array of DIP pins
  int dipPins[] = {DIP1_PIN, DIP2_PIN, DIP3_PIN, DIP4_PIN};
  
	// set the Node # according to the DIP pins
  setNodeID(dipPins, 4);
  
	if (this_node == 200) {
		// if the Node # is 0 (alias to 200), blink quickly (30 times) on power on
		for (int i=0; i < 30; i++) {
			analogWrite(POWER_PIN, 0);
			delay(20);
			analogWrite(POWER_PIN, 1023);
			delay(20);
		}
	} else {
		// if the Node # is a set number, blink the Node # on power on
		for (int i=0; i < this_node; i++) {
			analogWrite(POWER_PIN, 0);
			delay(300);
			analogWrite(POWER_PIN, 1023);
			delay(300);
		}
	}
  
	// set this variable to current time
  last_radio_recv = millis();
}

void loop() {
	if (rf12_recvDone() && rf12_crc == 0 && rf12_len >= 1) {
		// a radio signal was received
		
		// get the program and preview numbers
		program_1 = rf12_data[0];
		program_2 = rf12_data[2];
    preview = rf12_data[4];

		// turn off all LEDs
		digitalWrite(PREVIEW_PIN, 1023);
		digitalWrite(PROGRAM_PIN, 1023);
		digitalWrite(POWER_PIN, 1023);

		if (this_node == 200) {
			// if the Node # is 200 (which is also 0), blink POWER LED every 1 second if signal exists
			digitalWrite(POWER_PIN, 0);
			delay(1000);
			digitalWrite(POWER_PIN, 1023);
			delay(1000);
		} else {
			// if the Node # is a set number, trigger an LED accordingly
			if (program_1 == this_node || program_2 == this_node) {
				digitalWrite(PREVIEW_PIN, 1023);
				analogWrite(PROGRAM_PIN, 0);
			} else if (preview == this_node) {
				analogWrite(PROGRAM_PIN, 1023);
				digitalWrite(PREVIEW_PIN, 0);
			}
		}
		
		// keep track of last radio signal time
		last_radio_recv = millis();
	}
  
	// turn off LEDs when no radio signal exists (the past 1 second)
	if (millis() - last_radio_recv > 1000) {
		digitalWrite(PREVIEW_PIN, 1023);
		digitalWrite(PROGRAM_PIN, 1023);
		digitalWrite(POWER_PIN, 1023);
	}
}

// reads the DIP switches and determines the Node #
byte setNodeID(int* dipPins, int numPins) {
	float j=0;
  
	for(int i=0; i < numPins; i++) {
		if (digitalRead(dipPins[i])) {
			j += pow(2, i);
		}
	}

  this_node = (int)(j + 0.5);

	// if the DIP switches are all OFF, assign 200 (alias to 0) to the Node #
  this_node = this_node == 0 ? 200 : this_node;

	// initialize the radio
  rf12_initialize(this_node, RF12_433MHZ, 4);
}