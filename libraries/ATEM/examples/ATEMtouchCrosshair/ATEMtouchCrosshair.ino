/*****************
 * Centers the DVE source to the coordinates indicated by the touch of the touchscreen.
 * Requires a touchscreen with the SkaarhojUtil library as a driver
 * There is a 720p cross hair graphic in the same folder as this example.
 * For this example to work you must activate the DVE for one of the keyers on your 
 * ATEM 1M/E and set it "On Air" and also set the DVE source to the cross hair graphic. 
 * It's designed to work in 720p
 * 
 * - kasper
 */


#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>


// MAC address and IP address for this *particular* Ethernet Shield!
// MAC address is printed on the shield
// IP address is an available address you choose on your subnet where the switcher is also present:
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x6B, 0x6A };      // MAC address of the Arduino Ethernet behind the screen!
IPAddress ip(192, 168, 10, 99);              // IP address for the switcher control


// Include ATEM library and make an instance:
#include <ATEM.h>
ATEM AtemSwitcher;



#include "SkaarhojUtils.h"
SkaarhojUtils utils;


void setup() { 
  utils.touch_init();

  // The line below is calibration numbers for a specific monitor. 
  // Substitute this with calibration for YOUR monitor (see example "Touch_Calibrate")
  utils.touch_calibrationPointRawCoordinates(330,711,763,716,763,363,326,360);

  // Start the Ethernet, Serial (debugging) and UDP:
  Ethernet.begin(mac,ip);
  Serial.begin(9600);  

  // Connect to an ATEM switcher on this address and using this local port:
  // The port number is chosen randomly among high numbers.
  AtemSwitcher.begin(IPAddress(192, 168, 10, 240), 56417);    // <= SETUP!
  AtemSwitcher.serialOutput(true);
  AtemSwitcher.connect();
}

bool AtemOnline = false;

void loop() {

  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();

  // If connection is gone, try to reconnect:
  if (AtemSwitcher.isConnectionTimedOut())  {
    if (AtemOnline)  {
      AtemOnline = false;
    }

    Serial.println("Connection to ATEM Switcher has timed out - reconnecting!");
    AtemSwitcher.connect();
  }

  // If the switcher has been initialized, check for button presses as reflect status of switcher in button lights:
  if (AtemSwitcher.hasInitialized())  {
    if (!AtemOnline)  {
      AtemOnline = true;
    }

    AtemSwitcher.delay(15);	// This determines how smooth you will see the graphic move.
    manageTouch();
  }
}

int prevCenterX, prevCenterY;

void manageTouch()  {
  if (utils.touch_isTouched())  {
    Serial.print("Calibrated (x,y): ");
    Serial.print(utils.touch_coordX(utils.touch_getRawXVal()));
    Serial.print(",");
    Serial.println(utils.touch_coordY(utils.touch_getRawYVal()));  

    if (prevCenterX!=utils.touch_coordX(utils.touch_getRawXVal()) || prevCenterY!=utils.touch_coordY(utils.touch_getRawYVal()))  {
      Serial.println("Moving DVE...");  

      // Calculating center:
      int centerX = prevCenterX = utils.touch_coordX(utils.touch_getRawXVal());
      int centerY = prevCenterY = utils.touch_coordY(utils.touch_getRawYVal());

      long coordRangeX = 7950 * 4;
      long coordRangeY = 4450 * 4;

      long centerXscr = ((long)centerX*coordRangeX/1280-coordRangeX/2);
      long centerYscr = ((long)centerY*coordRangeY/720-coordRangeY/2);

      AtemSwitcher.changeDVESettingsTemp(centerXscr, centerYscr, 1000, 1000);
    }
  }
}



