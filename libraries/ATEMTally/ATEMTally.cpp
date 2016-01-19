#include <Arduino.h>
#include <Ethernet.h>
#include <ATEMTally.h>
#include <TextFinder.h>
#include <EEPROM.h>

// can be called to reset from code (directly to RESET PIN)
int RESTART_PIN = 7;

// button for resetting EEPROM
int RESET_PIN = 8;

// define LED pins
int R_PIN = 3;
int G_PIN = 5;
int B_PIN = 6;

//used to identify if valid data in EEPROM the "know" bit
const byte ID = 0x92;

// HTML for the setup page
PROGMEM prog_char html0[] = "";
PROGMEM prog_char html1[] = "<html><title>ATEM Tally Transmitter Setup</title></html>";
PROGMEM prog_char html2[] = "<table bgcolor=\"#999999\" border";
PROGMEM prog_char html3[] = "=\"0\" width=\"100%\" cellpadding=\"1\" style=\"font-family:Verdana;color:#fff";
PROGMEM prog_char html4[] = "fff;font-size:12px;\"><tr><td>&nbsp ATEM Tally Transmitter Setup</td></tr></table><br>";

PROGMEM prog_char html5[] = "<script>function hex2num (s_hex) {eval(\"var n_num=0X\" + s_hex);return n_num;}";
PROGMEM prog_char html6[] = "</script><table><form><input type=\"hidden\" name=\"SBM\" value=\"1\"><tr><td>MAC:</td>";
PROGMEM prog_char html7[] = "<td><input id=\"T1\" type=\"text\" size=\"2\" maxlength=\"2\" name=\"DT1\" value=\"";
PROGMEM prog_char html8[] = "\">.<input id=\"T3\" type=\"text\" size=\"2\" maxlength=\"2\" name=\"DT2\" value=\"";
PROGMEM prog_char html9[] = "\">.<input id=\"T5\" type=\"text\" size=\"2\" maxlength=\"2\" name=\"DT3\" value=\"";
PROGMEM prog_char html10[] = "\">.<input id=\"T7\" type=\"text\" size=\"2\" maxlength=\"2\" name=\"DT4\" value=\"";
PROGMEM prog_char html11[] = "\">.<input id=\"T9\" type=\"text\" size=\"2\" maxlength=\"2\" name=\"DT5\" value=\"";
PROGMEM prog_char html12[] = "\">.<input id=\"T11\" type=\"text\" size=\"2\" maxlength=\"2\" name=\"DT6\" value=\"";

PROGMEM prog_char html13[] = "\"><input id=\"T2\" type=\"hidden\" name=\"DT1\"><input id=\"T4\" type=\"hidden\" name=\"DT2";
PROGMEM prog_char html14[] = "\"><input id=\"T6\" type=\"hidden\" name=\"DT3\"><input id=\"T8\" type=\"hidden\" name=\"DT4";
PROGMEM prog_char html15[] = "\"><input id=\"T10\" type=\"hidden\" name=\"DT5\"><input id=\"T12\" type=\"hidden\" name=\"D";
PROGMEM prog_char html16[] = "T6\"></td></tr><tr><td>IP:</td><td><input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT7\" value=\"";
PROGMEM prog_char html17[] = "\">.<input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT8\" value=\"";
PROGMEM prog_char html18[] = "\">.<input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT9\" value=\"";
PROGMEM prog_char html19[] = "\">.<input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT10\" value=\"";

PROGMEM prog_char html20[] = "\"></td></tr><tr><td>ATEM SWITCHER:</td><td><input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT11\" value=\"";
PROGMEM prog_char html21[] = "\">.<input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT12\" value=\"";
PROGMEM prog_char html22[] = "\">.<input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT13\" value=\"";
PROGMEM prog_char html23[] = "\">.<input type=\"text\" size=\"3\" maxlength=\"3\" name=\"DT14\" value=\"";
PROGMEM prog_char html24[] = "\"> PORT<input type=\"text\" size=\"6\" maxlength=\"6\" name=\"DT15\" value=\"";
PROGMEM prog_char html25[] = "\"></td></tr><tr><td><br></td></tr><tr><td><input id=\"button1\"type=\"submit\" name=\"submit\" value=\"SUBMIT\" ";

PROGMEM prog_char html26[] = "Onclick=\"document.getElementById('T2').value ";
PROGMEM prog_char html27[] = "= hex2num(document.getElementById('T1').value);";
PROGMEM prog_char html28[] = "document.getElementById('T4').value = hex2num(document.getElementById('T3').value);";
PROGMEM prog_char html29[] = "document.getElementById('T6').value = hex2num(document.getElementById('T5').value);";
PROGMEM prog_char html30[] = "document.getElementById('T8').value = hex2num(document.getElementById('T7').value);";
PROGMEM prog_char html31[] = "document.getElementById('T10').value = hex2num(document.getElementById('T9').value);";
PROGMEM prog_char html32[] = "document.getElementById('T12').value = hex2num(document.getElementById('T11').value);\"";
PROGMEM prog_char html33[] = "></td></tr></form></table>";
PROGMEM prog_char html34[] = "<br>Restarting...";

PGM_P html[] PROGMEM = { html0, html1, html2, html3, html4, html5, html6, html7, html8, html9, html10, html11, html12,
	html13, html14, html15, html16, html17, html18, html19, html20, html21, html22, html23, html24, html25, html26, html27, 
	html28, html29, html30, html31, html32, html33, html34 };

ATEMTally::ATEMTally() {}

/*
	Initializes the ATEMTally - sets the pin modes
*/

void ATEMTally::initialize() {
	pinMode(R_PIN, OUTPUT);
	pinMode(G_PIN, OUTPUT);
	pinMode(B_PIN, OUTPUT);
	pinMode(RESET_PIN, INPUT);
}

/*
	Sets up Ethernet
*/

void ATEMTally::setup_ethernet(byte mac[6], byte ip[4], byte switcher_ip[4], int& switcher_port) {
	int idcheck = EEPROM.read(0);
	
	// if EEPROM contains saved data, use that data
	if (idcheck == ID) {
		for (int i = 0; i < 6; i++){
			mac[i] = EEPROM.read(i+1);
		}
		for (int i = 0; i < 4; i++){
			ip[i] = EEPROM.read(i+7);
		}
		for (int i = 0; i < 4; i++){
			switcher_ip[i] = EEPROM.read(i+11); 
		}
		// read switcher port (2 bytes) from address 15 & 16
		switcher_port = ATEMTally::eeprom_read_int(15);
	}
	
	Ethernet.begin(mac, ip);
}

/*
	Prints the HTML for the setup page
*/

void ATEMTally::print_html(EthernetClient& client, byte mac[6], byte ip[4], byte switcher_ip[4], int switcher_port) {
	if (client) {
		TextFinder finder(client);
		while (client.connected()) {      
			if (client.available()) {
				if (finder.find("GET /")) {
					client.println("HTTP/1.1 200 OK");
					client.println("Content-Type: text/html");
					client.println();
					
					bool submitted = false;
					if (finder.findUntil("SBM", "\n\r")) submitted = true;
					
					// if submit was pressed, save the EEPROM
					if (submitted) ATEMTally::save_eeprom(finder, mac, ip, switcher_ip, switcher_port);

					for (int i=1; i < 34; i++) {
						ATEMTally::set_field_value(client, i, mac, ip, switcher_ip, switcher_port);
						ATEMTally::print_buffer(client, &(html[i]), 0, false, false);
					}
					
					// if submit was pressed, restart the device
					if (submitted) {
						ATEMTally::print_buffer(client, &(html[34]), 0, false, false);
						ATEMTally::restart_device();
					}
					
					break;
				}
			}
		}
		client.stop();
	}
}

/*
	Changes the LED state
*/

void ATEMTally::change_LED_state(int state) {
	int r_value = 0;
	int g_value = 0;
	int b_value = 0;
	
	switch(state) {
		case 1: r_value = 0; g_value = 255; b_value = 255; break;
		case 2: r_value = 255; g_value = 0; b_value = 255; break;
		case 3: r_value = 255; g_value = 255; b_value = 0; break;
	}
  
	analogWrite(R_PIN, r_value);
	analogWrite(G_PIN, g_value);
	analogWrite(B_PIN, b_value);
}

/*
	Helps with printing buffer
*/

void ATEMTally::print_buffer(EthernetClient& client, const prog_char** s, int i, bool number, bool hex) {
	int total = 0;
	int start = 0;
	char buffer[100];
	
	memset(buffer,0,100);

	char* html_segment = (char*)pgm_read_word(s);
	int len = strlen_P(html_segment);

	while (total <= len) {
		strncpy_P(buffer, html_segment+start, 100-1);

		if (number) {
			client.print(i, DEC);
		} else if (hex) {
			client.print(i, HEX);
		} else {
			client.print(buffer);
		}

		total = start + 100-1;
		start = start + 100-1;
	}
}

/*
	Helps with assigning values to input fields
*/

void ATEMTally::set_field_value(EthernetClient& client, int i, byte mac[6], byte ip[4], byte switcher_ip[4], int switcher_port) {
	switch(i) {
		case 8: ATEMTally::print_buffer(client, &(html[0]), mac[0], false, true); break;
		case 9: ATEMTally::print_buffer(client, &(html[0]), mac[1], false, true); break;
		case 10: ATEMTally::print_buffer(client, &(html[0]), mac[2], false, true); break;
		case 11: ATEMTally::print_buffer(client, &(html[0]), mac[3], false, true); break;
		case 12: ATEMTally::print_buffer(client, &(html[0]), mac[4], false, true); break;
		case 13: ATEMTally::print_buffer(client, &(html[0]), mac[5], false, true); break;
		case 17: ATEMTally::print_buffer(client, &(html[0]), ip[0], true, false); break;
		case 18: ATEMTally::print_buffer(client, &(html[0]), ip[1], true, false); break;
		case 19: ATEMTally::print_buffer(client, &(html[0]), ip[2], true, false); break;
		case 20: ATEMTally::print_buffer(client, &(html[0]), ip[3], true, false); break;
		case 21: ATEMTally::print_buffer(client, &(html[0]), switcher_ip[0], true, false); break;
		case 22: ATEMTally::print_buffer(client, &(html[0]), switcher_ip[1], true, false); break;
		case 23: ATEMTally::print_buffer(client, &(html[0]), switcher_ip[2], true, false); break;
		case 24: ATEMTally::print_buffer(client, &(html[0]), switcher_ip[3], true, false); break;
		case 25: ATEMTally::print_buffer(client, &(html[0]), switcher_port, true, false); break;
	}
}

/*
	Saves new values to EEPROM
*/

void ATEMTally::save_eeprom(TextFinder& finder, byte mac[6], byte ip[4], byte switcher_ip[4], int& switcher_port) {
	byte SET = finder.getValue();
	
	// Now while you are looking for the letters "DT", you'll have to remember
	// every number behind "DT" and put them in "val" and while doing so, check
	// for the according values and put those in mac, ip, subnet and gateway.
	while(finder.findUntil("DT", "\n\r")){
		int val = finder.getValue();
		// if val from "DT" is between 1 and 6 the according value must be a MAC value.
		if(val >= 1 && val <= 6) {
			mac[val - 1] = finder.getValue();
		}
		// if val from "DT" is between 7 and 10 the according value must be a IP value.
		if(val >= 7 && val <= 10) {
			ip[val - 7] = finder.getValue();
		}
		// if val from "DT" is between 11 and 14 the according value must be a Switcher value.
		if(val >= 11 && val <= 14) {
			switcher_ip[val - 11] = finder.getValue(); 
		}
		// if val from "DT" is 15, set switcher port
		if(val == 15) {
			switcher_port = finder.getValue(); 
		}
	}
	
	// Now that we got all the data, we can save it to EEPROM
	for (int i = 0 ; i < 6; i++){
		EEPROM.write(i + 1, mac[i]);
	}
	for (int i = 0 ; i < 4; i++){
		EEPROM.write(i + 7, ip[i]);
	}
	for (int i = 0 ; i < 4; i++){
		EEPROM.write(i + 11, switcher_ip[i]); 
	}
	ATEMTally::eeprom_write_int(15, switcher_port); // write switcher port (2 bytes) to address 15 & 16

	// set ID to the known bit, so when you reset the Arduino is will use the EEPROM values
	EEPROM.write(0, ID);
}

/*
	Writes to EEPROM
*/

void ATEMTally::eeprom_write_int(int p_address, int p_value) {
	byte lowByte = ((p_value >> 0) & 0xFF);
	byte highByte = ((p_value >> 8) & 0xFF);

	EEPROM.write(p_address, lowByte);
	EEPROM.write(p_address + 1, highByte);
}

/*
	Reads from EEPROM
*/

unsigned int ATEMTally::eeprom_read_int(int p_address) {
	byte lowByte = EEPROM.read(p_address);
	byte highByte = EEPROM.read(p_address + 1);

	return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}

/*
	Resets EEPROM
*/

void ATEMTally::reset_eeprom() {
	for (int i = 0; i < 512; i++)
		EEPROM.write(i, 0); 
}

/*
	Restarts device
*/

void ATEMTally::restart_device() {
	asm volatile("jmp 0x7800");
}

/*
	Monitors for the reset button press - triggers reset and restart on press
*/

void ATEMTally::monitor_reset() {
	if (digitalRead(RESET_PIN)) {		
		ATEMTally::reset_eeprom();
		ATEMTally::restart_device();
	}
}