#ifndef ATEMTally_h
#define ATEMTally_h

#include <Arduino.h>
#include <Ethernet.h>
#include <TextFinder.h>
#include <EEPROM.h>

class ATEMTally
{
  public:
	ATEMTally();
	void initialize();
	void setup_ethernet(byte mac[6], byte ip[4], byte switcher_ip[4], int& switcher_port);
    void print_html(EthernetClient& client, byte mac[6], byte ip[4], byte switcher_ip[4], int switcher_port);
	void change_LED_state(int state);
	void monitor_reset();
  private:
	void print_buffer(EthernetClient& client, const prog_char** s, int i, bool number, bool hex);
    void set_field_value(EthernetClient& client, int i, byte mac[6], byte ip[4], byte switcher_ip[4], int switcher_port);
	void save_eeprom(TextFinder& finder, byte mac[6], byte ip[4], byte switcher_ip[4], int& switcher_port);
	void eeprom_write_int(int p_address, int p_value);
	unsigned int eeprom_read_int(int p_address);
	void reset_eeprom();
	void restart_device();
};

#endif
