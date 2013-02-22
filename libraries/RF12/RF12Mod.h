// RFM12B driver definitions
// 2009-02-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#ifndef RF12Mod_h
#define RF12Mod_h

#include <stdint.h>

/// RF12Mod Protocol version.
/// Version 1 did not include the group code in the crc.
/// Version 2 does include the group code in the crc.
#define RF12Mod_VERSION    2

/// Shorthand for RF12Mod group byte in RF12Mod_buf.
#define RF12Mod_grp        RF12Mod_buf[0]
/// Shorthand for RF12Mod header byte in RF12Mod_buf.
#define RF12Mod_hdr        RF12Mod_buf[1]
/// Shorthand for RF12Mod length byte in RF12Mod_buf.
#define RF12Mod_len        RF12Mod_buf[2]
/// Shorthand for first RF12Mod data byte in RF12Mod_buf.
#define RF12Mod_data       (RF12Mod_buf + 3)

/// RF12Mod CTL bit mask.
#define RF12Mod_HDR_CTL    0x80
/// RF12Mod DST bit mask.
#define RF12Mod_HDR_DST    0x40
/// RF12Mod ACK bit mask.
#define RF12Mod_HDR_ACK    0x20
/// RF12Mod HDR bit mask.
#define RF12Mod_HDR_MASK   0x1F

/// RF12Mod Maximum message size in bytes.
#define RF12Mod_MAXDATA    66

#define RF12Mod_433MHZ     1
#define RF12Mod_868MHZ     2
#define RF12Mod_915MHZ     3

// EEPROM address range used by the RF12Mod_config() code
#define RF12Mod_EEPROM_ADDR ((uint8_t*) 0x20)
#define RF12Mod_EEPROM_SIZE 32
#define RF12Mod_EEPROM_EKEY (RF12Mod_EEPROM_ADDR + RF12Mod_EEPROM_SIZE)
#define RF12Mod_EEPROM_ELEN 16

// shorthands to simplify sending out the proper ACK when requested
#define RF12Mod_WANTS_ACK ((RF12Mod_hdr & RF12Mod_HDR_ACK) && !(RF12Mod_hdr & RF12Mod_HDR_CTL))
#define RF12Mod_ACK_REPLY (RF12Mod_hdr & RF12Mod_HDR_DST ? RF12Mod_HDR_CTL : \
            RF12Mod_HDR_CTL | RF12Mod_HDR_DST | (RF12Mod_hdr & RF12Mod_HDR_MASK))
            
// options for RF12Mod_sleep()
#define RF12Mod_SLEEP 0
#define RF12Mod_WAKEUP -1

/// Running crc value, should be zero at end.
extern volatile uint16_t RF12Mod_crc;
/// Recv/xmit buf including hdr & crc bytes.
extern volatile uint8_t RF12Mod_buf[];
/// Seq number of encrypted packet (or -1).
extern long RF12Mod_seq;

/// Only needed if you want to init the SPI bus before RF12Mod_initialize does it.
void RF12Mod_spiInit(void);

/// Call this once with the node ID, frequency band, and optional group.
uint8_t RF12Mod_initialize(uint8_t id, uint8_t band, uint8_t group=0xD4);

/// Initialize the RF12Mod module from settings stored in EEPROM by "RF12Moddemo"
/// don't call RF12Mod_initialize() if you init the hardware with RF12Mod_config().
/// @return the node ID as 1..31 value (1..26 correspond to nodes 'A'..'Z').
uint8_t RF12Mod_config(uint8_t show =1);

/// Call this frequently, returns true if a packet has been received.
uint8_t RF12Mod_recvDone(void);

/// Call this to check whether a new transmission can be started.
/// @return true when a new transmission may be started with RF12Mod_sendStart().
uint8_t RF12Mod_canSend(void);

/// Call this only when RF12Mod_recvDone() or RF12Mod_canSend() return true.
void RF12Mod_sendStart(uint8_t hdr);
/// Call this only when RF12Mod_recvDone() or RF12Mod_canSend() return true.
void RF12Mod_sendStart(uint8_t hdr, const void* ptr, uint8_t len);
/// Deprecated: use RF12Mod_sendStart(hdr,ptr,len) followed by RF12Mod_sendWait(sync).
void RF12Mod_sendStart(uint8_t hdr, const void* ptr, uint8_t len, uint8_t sync);

/// Wait for send to finish. @param mode sleep mode 0=none, 1=idle, 2=standby, 3=powerdown.
void RF12Mod_sendWait(uint8_t mode);

/// This simulates OOK by turning the transmitter on and off via SPI commands.
/// Use this only when the radio was initialized with a fake zero node ID.
void RF12Mod_onOff(uint8_t value);

/// Power off the RF12Mod, ms > 0 sets watchdog to wake up again after N * 32 ms.
/// @note once off, calling this with -1 can be used to bring the RF12Mod back up.
void RF12Mod_sleep(char n);

/// @return true if the supply voltage is below 3.1V.
char RF12Mod_lowbat(void);

/// Set up the easy tranmission mode, arg is number of seconds between packets.
void RF12Mod_easyInit(uint8_t secs);

/// Call this often to keep the easy transmission mode going.
char RF12Mod_easyPoll(void);

/// Send new data using the easy transmission mode, buffer gets copied to driver.
char RF12Mod_easySend(const void* data, uint8_t size);

/// Enable encryption (null arg disables it again).
void RF12Mod_encrypt(const uint8_t*);

/// Low-level control of the RFM12B via direct register access.
/// http://tools.jeelabs.org/rfm12b is useful for calculating these.
uint16_t RF12Mod_control(uint16_t cmd);

/// See http://blog.strobotics.com.au/2009/07/27/rfm12-tutorial-part-3a/
/// Transmissions are packetized, don't assume you can sustain these speeds! 
///
/// @note Data rates are approximate. For higher data rates you may need to
/// alter receiver radio bandwidth and transmitter modulator bandwidth.
/// Note that bit 7 is a prescaler - don't just interpolate rates between
/// RF12Mod_DATA_RATE_3 and RF12Mod_DATA_RATE_2.
enum RF12ModDataRates {
    RF12Mod_DATA_RATE_CMD = 0xC600,
    RF12Mod_DATA_RATE_9 = RF12Mod_DATA_RATE_CMD | 0x02,  // Approx 115200 bps
    RF12Mod_DATA_RATE_8 = RF12Mod_DATA_RATE_CMD | 0x05,  // Approx  57600 bps
    RF12Mod_DATA_RATE_7 = RF12Mod_DATA_RATE_CMD | 0x06,  // Approx  49200 bps
    RF12Mod_DATA_RATE_6 = RF12Mod_DATA_RATE_CMD | 0x08,  // Approx  38400 bps
    RF12Mod_DATA_RATE_5 = RF12Mod_DATA_RATE_CMD | 0x11,  // Approx  19200 bps
    RF12Mod_DATA_RATE_4 = RF12Mod_DATA_RATE_CMD | 0x23,  // Approx   9600 bps
    RF12Mod_DATA_RATE_3 = RF12Mod_DATA_RATE_CMD | 0x47,  // Approx   4800 bps
    RF12Mod_DATA_RATE_2 = RF12Mod_DATA_RATE_CMD | 0x91,  // Approx   2400 bps
    RF12Mod_DATA_RATE_1 = RF12Mod_DATA_RATE_CMD | 0x9E,  // Approx   1200 bps
    RF12Mod_DATA_RATE_DEFAULT = RF12Mod_DATA_RATE_7,
};

#endif
