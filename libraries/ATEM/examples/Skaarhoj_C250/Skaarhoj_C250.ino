/*****************
 * Touch Control for ATEM panel
 * Requires connection to a touch screen via the SKAARHOJ utils library.
 *
 * This example also uses several custom libraries which you must install first. 
 * Search for "#include" in this file to find the libraries. Then download the libraries from http://skaarhoj.com/wiki/index.php/Libraries_for_Arduino
 *
 * Works with ethernet-enabled arduino devices (Arduino Ethernet or a model with Ethernet shield)
 * Make sure to configure IP and addresses! Look for all instances of "<= SETUP" in the code below!
 * 
 * - kasper
 */



// Including libraries: 
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>


// MAC address and IP address for this *particular* Arduino / Ethernet Shield!
// The MAC address is printed on a label on the shield or on the back of your device
// The IP address should be an available address you choose on your subnet where the switcher is also present
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x5E, 0xFF };      // <= SETUP!  MAC address of the Arduino
IPAddress ip(192, 168, 10, 99);              // <= SETUP!  IP address of the Arduino


// Include ATEM library and make an instance:
// Connect to an ATEM switcher on this address and using this local port:
// The port number is chosen randomly among high numbers.
#include <ATEM.h>
ATEM AtemSwitcher(IPAddress(192, 168, 10, 240), 56417);  // <= SETUP (the IP address of the ATEM switcher)



// No-cost stream operator as described at 
// http://arduiniana.org/libraries/streaming/
template<class T>
inline Print &operator <<(Print &obj, T arg)
{  
  obj.print(arg); 
  return obj; 
}



// Include the SkaarhojUtils library. 
#include "SkaarhojUtils.h"
SkaarhojUtils utils;


// If this flag is set true, a tap on a source brings it directly to program
// If a Bi-color LED is connected to digital output 2 (green) and 3 (red) (both active high) it will light red when this functionality is on and green when preview select is on. It will light orange if it's in the process of connecting to the ATEM Switcher.
bool directlyToProgram = false;  


void setup() { 

    // Touch Screen power-up. Should not affect touch screen solution where it is not wired up.
    // Uses D4 to trigger the "on" signal (through a 15K res), first set it in Tri-state mode.
    // Uses Analog pin A5 to detect from the LED voltage levels whether it is already on or not
  pinMode(4, INPUT);
  delay(3000);
  if (analogRead(A5)>700)  {  // 700 is the threshold value found by trying. If off, it's around 790, if on, it's around 650
    // Now, trying to turn the screen on:
    digitalWrite(4,0);  // First, set the next output of D4 to low
    pinMode(4, OUTPUT);  // Change D4 from an high impedance input to an output, effectively triggering a button press
    delay(500);  // Wait a while.
    pinMode(4, INPUT);  // Change D4 back to high impedance input.
  }
  
    // Initialize touch library
  utils.touch_init();

  // Initializing the slider:
  utils.uniDirectionalSlider_init(10, 35, 35, A4);
  
  // The line below is calibration numbers for a specific monitor. 
  // Substitute this with calibration for YOUR monitor (see example "Touch_Calibrate")
  utils.touch_calibrationPointRawCoordinates(330,711,763,716,763,363,326,360);

  // Start the Ethernet, Serial (debugging) and UDP:
  Ethernet.begin(mac,ip);
  Serial.begin(9600);
  Serial << F("\n- - - - - - - -\nSerial Started\n");  

  // Sets the Bi-color LED to off = "no connection"
  digitalWrite(2,false);
  digitalWrite(3,false);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);


  // Initialize a connection to the switcher:
  //AtemSwitcher.serialOutput(true);  // Remove or comment out this line for production code. Serial output may decrease performance!
  AtemSwitcher.connect();
  
    // Set Bi-color LED orange - indicates "connecting...":
  digitalWrite(3,true);
  digitalWrite(2,true);
}


bool AtemOnline = false;
uint8_t MVFieldMapping[] = {1,2,3,4, 5,6,7,8}; // Maps the multiviewer fields 1-4,5-8 in two rows to input source numbers. This needs to correspond to the switcher connected

void loop() {
  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();

  // If connection is gone, try to reconnect:
  if (AtemSwitcher.isConnectionTimedOut())  {
    if (AtemOnline)  {
      AtemOnline = false;
      
        // Set Bi-color LED off = "no connection"
      digitalWrite(3,false);
      digitalWrite(2,false);
    }

    Serial.println("Connection to ATEM Switcher has timed out - reconnecting!");
    AtemSwitcher.connect();

    // Set Bi-color LED orange - indicates "connecting...":
    digitalWrite(3,true);
    digitalWrite(2,true);
  }

  // If the switcher has been initialized, check for button presses as reflect status of switcher in button lights:
  if (AtemSwitcher.hasInitialized())  {
    if (!AtemOnline)  {
      AtemOnline = true;
      Serial << "ATEM model: " << AtemSwitcher.getATEMmodel() << "\n";
      if (AtemSwitcher.getATEMmodel()==0)  {  // TVS:
        MVFieldMapping[3] = 10;  // Media Player 1
        MVFieldMapping[4] = 4;
        MVFieldMapping[5] = 5;
        MVFieldMapping[6] = 6;
        MVFieldMapping[7] = 12;  // Media Player 2
      }
    }

    AtemSwitcher.delay(15);	// This determines how smooth you will see the graphic move.
    manageTouch();

    // Set Bi-color LED to red or green depending on mode:
    digitalWrite(3,directlyToProgram);
    digitalWrite(2,!directlyToProgram);

      // "T-bar" slider:
    if (utils.uniDirectionalSlider_hasMoved())  {
      AtemSwitcher.changeTransitionPosition(utils.uniDirectionalSlider_position());
      AtemSwitcher.delay(20);
      if (utils.uniDirectionalSlider_isAtEnd())  {
		AtemSwitcher.changeTransitionPositionDone();
		AtemSwitcher.delay(5);  
      }
    }
  }
}


void manageTouch()  {

  uint8_t tState = utils.touch_state();  // See what the state of touch is:
  if (tState !=0)  {  // Different from zero? Then a touch even has happened:
  
      // Calculate which Multiviewer field on an ATEM we pressed:
    uint8_t MVfieldPressed = touchArea(utils.touch_getEndedXCoord(),utils.touch_getEndedYCoord());

    if (tState==9 && MVfieldPressed==16)  {  // held down in program area
      directlyToProgram=!directlyToProgram;
      Serial.print("directlyToProgram=");
      Serial.println(directlyToProgram);
    } else
    if (tState==1)  {  // Single tap somewhere:
      if (MVfieldPressed == AtemSwitcher.getPreviewInput() || MVfieldPressed==17)  {  // If Preview or a source ON preview is touched, bring in on.
        AtemSwitcher.doCut();
      } 
      else if (MVfieldPressed>=1 && MVfieldPressed<=8)  {  // If any other source is touched, bring it to preview or program depending on mode:
        if (directlyToProgram)  {
          AtemSwitcher.changeProgramInput(MVFieldMapping[MVfieldPressed-1]);
        } else {
          AtemSwitcher.changePreviewInput(MVFieldMapping[MVfieldPressed-1]);
        }
      }
    } else
    if (tState==10) {  // Left -> Right swipe
      if (MVfieldPressed==16 && touchArea(utils.touch_getStartedXCoord(),utils.touch_getStartedYCoord())==17)  {
         Serial << F("Swipe from Preview to Program\n"); 
         AtemSwitcher.doAuto();
      }
    }
  }
}

// Returns which field in MV was pressed:
uint8_t touchArea(int x, int y)  {
  if (y<=720/2)  {  // lower than middel
    if (x<=1280/4)  {
      if (y>=720/4)  {
        // 1: 
        return 1;
      } 
      else {
        // 5:
        return 5;
      }
    } 
    else if (x<=1280/2) {
      if (y>=720/4)  {
        // 2: 
        return 2;
      } 
      else {
        // 6:
        return 6;
      }
    } 
    else if (x<=1280/4*3) {
      if (y>=720/4)  {
        // 3: 
        return 3;
      } 
      else {
        // 7:
        return 7;
      }
    } 
    else {
      if (y>=720/4)  {
        // 4: 
        return 4;
      } 
      else {
        // 8:
        return 8;
      }
    }
  } 
  else {  // Preview or program:
    if (x<=1280/2)  {
      // Preview:
      return 17;
    } 
    else {
      // Program:
      return 16;
    }
  }
}

