/*****************
 * Setting Ethernet Addresses for a SKAARHOJ device
 * 
 * - kasper
 */





// Including libraries: 
#include <EEPROM.h>      // For storing IP numbers


// Configure the IP addresses and MAC address with the sketch "ConfigEthernetAddresses":
uint8_t ip[4];        // Will hold the Arduino IP address
uint8_t atem_ip[4];  // Will hold the ATEM IP address
uint8_t mac[6];    // Will hold the Arduino Ethernet shield/board MAC address (loaded from EEPROM memory, set with ConfigEthernetAddresses example sketch)
uint8_t atem2_ip[4];  // Will hold the ATEM IP address
uint8_t videohub_ip[4];  // Will hold the ATEM IP address




// No-cost stream operator as described at 
// http://arduiniana.org/libraries/streaming/
template<class T>
inline Print &operator <<(Print &obj, T arg)
{  
  obj.print(arg); 
  return obj; 
}



char buffer[18];

void setup() { 

  // Start the Ethernet, Serial (debugging) and UDP:
  Serial.begin(9600);
  Serial << F("\n- - - - - - - -\nSerial Started\n");  



  // Setting the Arduino IP address:
  ip[0] = EEPROM.read(0+2);
  ip[1] = EEPROM.read(1+2);
  ip[2] = EEPROM.read(2+2);
  ip[3] = EEPROM.read(3+2);
  Serial << F("SKAARHOJ Device IP Address: ") << ip[0] << "." << ip[1] << "." << ip[2] << "." << ip[3] << "\n";

  // Setting the ATEM IP address:
  atem_ip[0] = EEPROM.read(0+2+4);
  atem_ip[1] = EEPROM.read(1+2+4);
  atem_ip[2] = EEPROM.read(2+2+4);
  atem_ip[3] = EEPROM.read(3+2+4);
  Serial << F("ATEM Switcher IP Address: ") << atem_ip[0] << "." << atem_ip[1] << "." << atem_ip[2] << "." << atem_ip[3] << "\n";

  // Setting the 2. ATEM IP address:
  atem2_ip[0] = EEPROM.read(40);
  atem2_ip[1] = EEPROM.read(41);
  atem2_ip[2] = EEPROM.read(42);
  atem2_ip[3] = EEPROM.read(43);
  Serial << F("Second ATEM Switcher IP Address (if any): ") << atem2_ip[0] << "." << atem2_ip[1] << "." << atem2_ip[2] << "." << atem2_ip[3] 
          << " - Checksum: " << ((atem2_ip[0]+atem2_ip[1]+atem2_ip[2]+atem2_ip[3]) & 0xFF) << " = " << EEPROM.read(44) << " ? \n";
  
  // Setting the videohub IP address:
  videohub_ip[0] = EEPROM.read(45);
  videohub_ip[1] = EEPROM.read(46);
  videohub_ip[2] = EEPROM.read(47);
  videohub_ip[3] = EEPROM.read(48);
  Serial << F("Videohub IP Address (if any): ") << videohub_ip[0] << "." << videohub_ip[1] << "." << videohub_ip[2] << "." << videohub_ip[3] 
          << " - Checksum: " << ((videohub_ip[0]+videohub_ip[1]+videohub_ip[2]+videohub_ip[3]) & 0xFF) << " = " << EEPROM.read(49) << " ? \n";
  
  // Setting MAC address:
  mac[0] = EEPROM.read(10);
  mac[1] = EEPROM.read(11);
  mac[2] = EEPROM.read(12);
  mac[3] = EEPROM.read(13);
  mac[4] = EEPROM.read(14);
  mac[5] = EEPROM.read(15);
  char buffer[18];
  sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial << F("SKAARHOJ Device MAC address: ") << buffer << F(" - Checksum: ")
        << ((mac[0]+mac[1]+mac[2]+mac[3]+mac[4]+mac[5]) & 0xFF) << "\n";
  if ((uint8_t)EEPROM.read(16)!=((mac[0]+mac[1]+mac[2]+mac[3]+mac[4]+mac[5]) & 0xFF))  {
    Serial << F("MAC address not found in EEPROM memory!\n") <<
      F("Please load example sketch ConfigEthernetAddresses to set it.\n") <<
      F("The MAC address is found on the backside of your Ethernet Shield/Board\n (STOP)");
    while(true);
  }

}

void loop() {
}

