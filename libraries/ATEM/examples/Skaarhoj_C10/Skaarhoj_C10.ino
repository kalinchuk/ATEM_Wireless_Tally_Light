/*****************
 * C10 Example with Deck Control of HyperDeck Studio
 *
 * This example uses several custom libraries which you must install first. 
 * Search for "#include" in this file to find the libraries. Then download the libraries from http://skaarhoj.com/wiki/index.php/Libraries_for_Arduino
 *
 * Works with Arduino Mega
 * Make sure to configure IP and addresses! Look for all instances of "<= SETUP" in the code below!
 * 
 * - kasper
 */



// Including libraries: 
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>

// HyperDeck Studio:
#include <HyperDeckStudio.h>
HyperDeckStudio theHyperDeck(Serial1,1);  // Uses around 100 bytes...


// MAC address and IP address for this *particular* Arduino / Ethernet Shield!
// The MAC address is printed on a label on the shield or on the back of your device
// The IP address should be an available address you choose on your subnet where the switcher is also present
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x5F, 0x61 };      // <= SETUP!  MAC address of the Arduino
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



// All related to library "SkaarhojBI8":
#include "Wire.h"
#include "MCP23017.h"
#include "PCA9685.h"
#include "SkaarhojBI8.h"

SkaarhojBI8 buttons;


void setup() { 

  // Start the Ethernet, Serial (debugging) and UDP:
  Ethernet.begin(mac,ip);
  Serial.begin(9600);  
  Serial << F("\n- - - - - - - -\nSerial Started\n");

  // Always initialize Wire before setting up the SkaarhojBI8 class!
  Wire.begin(); 

  // Set up the SkaarhojBI8 boards:
  buttons.begin(0);
  buttons.setDefaultColor(0);  // Off by default
  buttons.testSequence();

  // Initialize HyperDeck object:
  theHyperDeck.begin();
  theHyperDeck.serialOutput(true);  // Normally, this should be commented out!

  // Initialize a connection to the switcher:
  AtemSwitcher.serialOutput(true);  // Remove or comment out this line for production code. Serial output may decrease performance!
  AtemSwitcher.connect();
}

bool AtemOnline = false;

void loop() {
  uint8_t buttonDownPress = buttons.buttonDownAll();  

  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();

  // If connection is gone, try to reconnect:
  if (AtemSwitcher.isConnectionTimedOut())  {
    if (AtemOnline)  {
      AtemOnline = false;

      Serial.println("Turning off buttons light");
      buttons.setDefaultColor(0);  // Off by default
      buttons.setButtonColorsToDefault();
    }

    Serial.println("Connection to ATEM Switcher has timed out - reconnecting!");
    AtemSwitcher.connect();
  }

  // If the switcher has been initialized, check for button presses as reflect status of switcher in button lights:
  if (AtemSwitcher.hasInitialized())  {
    if (!AtemOnline)  {
      AtemOnline = true;
      Serial.println("Turning on buttons");

      buttons.setDefaultColor(0);  // Off by default
      buttons.setButtonColorsToDefault();
    }

    // Implement the functions of the buttons:
    buttonFunctions(buttonDownPress);
  }



  theHyperDeck.runLoop();
  if (theHyperDeck.isOnline())  {
    // B8:
    if (theHyperDeck.isStandby())  {
      if (theHyperDeck.isInLocalModeOnly() && ((unsigned long)millis() & 1023)<255)  {
        buttons.setButtonColor(8, 0);
      } 
      else {
        buttons.setButtonColor(8, theHyperDeck.isRecording() ? 2 : 5);
      }
    } 
    else {
      buttons.setButtonColor(8, 0);
    }

    if (!theHyperDeck.isInLocalModeOnly())  {
      if (buttons.isButtonIn(8, buttonDownPress))  {   // Executes button command if pressed:
        if (theHyperDeck.isRecording())  {
          theHyperDeck.doStop();
        } 
        else {
          theHyperDeck.doRecord();
        }
      }
    }
  } 
  else {
    buttons.setButtonColor(8,0); 
  }
}






/*************************
 * Set button functions
 *************************/
uint8_t PIP = 0;
void buttonFunctions(uint8_t buttonDownPress)  {

  // B5:
  buttons.setButtonColor(5, AtemSwitcher.getProgramInput()==1 ? 2 : 5);
  if (buttons.isButtonIn(5, buttonDownPress))  {   // Executes button command if pressed:
    AtemSwitcher.changeUpstreamKeyOn(1, false); 
    AtemSwitcher.delay(5);
    AtemSwitcher.changeProgramInput(1); 
  }

  // B1:
  buttons.setButtonColor(1, AtemSwitcher.getProgramInput()==2 ? 2 : 5); 
  if (buttons.isButtonIn(1, buttonDownPress))  {   // Executes button command if pressed:
    AtemSwitcher.changeUpstreamKeyOn(1, false); 
    AtemSwitcher.delay(5);
    AtemSwitcher.changeProgramInput(2); 
  }


  // B6:
  buttons.setButtonColor(6, PIP==1 && AtemSwitcher.getUpstreamKeyerStatus(1) ? 2 : 5);
  if (buttons.isButtonIn(6, buttonDownPress))  {   // Executes button command if pressed:
    if (AtemSwitcher.getProgramInput()==1)  {
      AtemSwitcher.changeProgramInput(2); 
      AtemSwitcher.delay(5);
    }

    if (PIP!=1)  {
      AtemSwitcher.changeDVESettingsTemp(-8500,-4500,300,300);
      AtemSwitcher.delay(5);
      if (!AtemSwitcher.getUpstreamKeyerStatus(1))  {
        AtemSwitcher.changeUpstreamKeyOn(1, true); 
      }
      PIP = 1;
      Serial << "PIP 1 set\n";
    } 
    else if (!AtemSwitcher.getUpstreamKeyerStatus(1))  {
      AtemSwitcher.changeUpstreamKeyOn(1, true); 
      Serial << "PIP 1 keyon\n";
    } 
    else {
      AtemSwitcher.changeUpstreamKeyOn(1, false); 
      Serial << "PIP 2 keyoff\n";
    }
  }

  // B2:
  buttons.setButtonColor(2, PIP==2 && AtemSwitcher.getUpstreamKeyerStatus(1) ? 2 : 5);
  if (buttons.isButtonIn(2, buttonDownPress))  {   // Executes button command if pressed:
    if (AtemSwitcher.getProgramInput()==1)  {
      AtemSwitcher.changeProgramInput(2); 
      AtemSwitcher.delay(5);
    }

    if (PIP!=2)  {
      AtemSwitcher.changeDVESettingsTemp(8500,-4500,300,300);
      AtemSwitcher.delay(5);
      if (!AtemSwitcher.getUpstreamKeyerStatus(1))  {
        AtemSwitcher.changeUpstreamKeyOn(1, true); 
      }
      PIP = 2;
      Serial << "PIP 2 set\n";
    } 
    else if (!AtemSwitcher.getUpstreamKeyerStatus(1))  {
      AtemSwitcher.changeUpstreamKeyOn(1, true); 
      Serial << "PIP 2 keyon\n";
    } 
    else {
      AtemSwitcher.changeUpstreamKeyOn(1, false); 
      Serial << "PIP 2 keyoff\n";
    }
  }
}




