#include <SPI.h>
#include <Ethernet.h>
#include <TextFinder.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <ATEM.h>
#include <ATEMTally.h>
#include <JeeLibMod.h>

// set the default MAC address
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// set the default IP address
byte ip[] = {192,168,1,234};

// set the default IP address of the ATEM switcher
byte switcher_ip[] = {192,168,1,240};

// set the default PORT of the ATEM switcher
int switcher_port = 49910;

// initialize the ethernet server (for settings page)
EthernetServer server(80);

// set to false initially; set to true when ATEM switcher initializes
boolean ranOnce = false;

// create a new AtemSwitcher and ATEMTally object
ATEM AtemSwitcher;
ATEMTally ATEMTally;

// define a structure for sending over the radio
struct {
  	int program;
  	int preview;
} payload;

void setup()
{
	// initialize the RF12 radio; set the Node # to 20
	RF12Mod_initialize(20, RF12Mod_915MHZ, 4);

	// initialize the ATEMTally object
	ATEMTally.initialize();
	
	// set the LED to RED
	ATEMTally.change_LED_state(2);
	
	// setup the Ethernet
	ATEMTally.setup_ethernet(mac, ip, switcher_ip, switcher_port);
	
	// start the server
	server.begin();

	delay(1000);
}

void loop()
{
	// run this code only once
	if (!ranOnce) {
		// initialize the AtemSwitcher
		AtemSwitcher.begin(IPAddress(switcher_ip[0], switcher_ip[1], switcher_ip[2], switcher_ip[3]), switcher_port);    
		
		// attempt to connect to the switcher
		AtemSwitcher.connect();

		// change LED to RED
		ATEMTally.change_LED_state(2);
		
		ranOnce = true;
	}
    
	// display the setup page if requested
  	EthernetClient client = server.available();
  	ATEMTally.print_html(client, mac, ip, switcher_ip, switcher_port);
  
	// AtemSwitcher function for retrieving the program and preview camera numbers
  	AtemSwitcher.runLoop();
  
  	// if connection is gone anyway, try to reconnect
  	if (AtemSwitcher.isConnectionTimedOut())  {
    	AtemSwitcher.connect();
  	} else {
		// assign the program and preview numbers to the payload structure
	    payload.program = AtemSwitcher.getProgramInput();
	    payload.preview = AtemSwitcher.getPreviewInput();
        
		// when radio is available, transmit the structure with program and preview numbers
	    RF12Mod_recvDone();
	    if (RF12Mod_canSend()) {
	    	RF12Mod_sendStart(0, &payload, sizeof payload);
	      	ATEMTally.change_LED_state(3);      
	    }    
  	}
    
	// a delay is needed due to some weird issue
  	delay(10);

  	ATEMTally.change_LED_state(1);

	// monitors for the reset button press
  	ATEMTally.monitor_reset();
}