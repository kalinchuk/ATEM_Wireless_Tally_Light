// RFM12B driver implementation
// 2009-02-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include "RF12Mod.h"
#include <avr/io.h>
#include <util/crc16.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#if ARDUINO >= 100
#include <Arduino.h> // Arduino 1.0
#else
#include <WProgram.h> // Arduino 0022
#endif

// #define OPTIMIZE_SPI 1  // uncomment this to write to the RFM12B @ 8 Mhz

// pin change interrupts are currently only supported on ATmega328's
// #define PINCHG_IRQ 1    // uncomment this to use pin-change interrupts

// maximum transmit / receive buffer: 3 header + data + 2 crc bytes
#define RF_MAX   (RF12Mod_MAXDATA + 5)

// pins used for the RFM12B interface - yes, there *is* logic in this madness:
//
//  - leave RFM_IRQ set to the pin which corresponds with INT0, because the
//    current driver code will use attachInterrupt() to hook into that
//  - (new) you can now change RFM_IRQ, if you also enable PINCHG_IRQ - this
//    will switch to pin change interrupts instead of attach/detachInterrupt()
//  - use SS_DDR, SS_PORT, and SS_BIT to define the pin you will be using as
//    select pin for the RFM12B (you're free to set them to anything you like)
//  - please leave SPI_SS, SPI_MOSI, SPI_MISO, and SPI_SCK as is, i.e. pointing
//    to the hardware-supported SPI pins on the ATmega, *including* SPI_SS !

#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)

#define RFM_IRQ     2
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      0

#define SPI_SS      53    // PB0, pin 19
#define SPI_MOSI    51    // PB2, pin 21
#define SPI_MISO    50    // PB3, pin 22
#define SPI_SCK     52    // PB1, pin 20

#elif defined(__AVR_ATmega644P__)

#define RFM_IRQ     10
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      4

#define SPI_SS      4
#define SPI_MOSI    5
#define SPI_MISO    6
#define SPI_SCK     7

#elif defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny44__)

#define RFM_IRQ     2
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      1

#define SPI_SS      1     // PB1, pin 3
#define SPI_MISO    4     // PA6, pin 7
#define SPI_MOSI    5     // PA5, pin 8
#define SPI_SCK     6     // PA4, pin 9

#elif defined(__AVR_ATmega32U4__) //Arduino Leonardo 

#define RFM_IRQ     0	    // PD0, INT0, Digital3 
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      6	    // Dig10, PB6

#define SPI_SS      17    // PB0, pin 8, Digital17
#define SPI_MISO    14    // PB3, pin 11, Digital14
#define SPI_MOSI    16    // PB2, pin 10, Digital16
#define SPI_SCK     15    // PB1, pin 9, Digital15

#else

// ATmega168, ATmega328, etc.
#define RFM_IRQ     2
#define SS_DDR      DDRD 	// originally DDRB
#define SS_PORT     PORTD	// originally PORTB
							// using PIN 4 (SDCS in Arduino Ethernet)
#define SS_BIT      4     	// originally for PORTB: 2 = d.10, 1 = d.9, 0 = d.8

#define SPI_SS      10    // PB2, pin 16
#define SPI_MOSI    11    // PB3, pin 17
#define SPI_MISO    12    // PB4, pin 18
#define SPI_SCK     13    // PB5, pin 19

#endif 

// RF12Mod command codes
#define RF_RECEIVER_ON  0x82DD
#define RF_XMITTER_ON   0x823D
#define RF_IDLE_MODE    0x820D
#define RF_SLEEP_MODE   0x8205
#define RF_WAKEUP_MODE  0x8207
#define RF_TXREG_WRITE  0xB800
#define RF_RX_FIFO_READ 0xB000
#define RF_WAKEUP_TIMER 0xE000

// RF12Mod status bits
#define RF_LBD_BIT      0x0400
#define RF_RSSI_BIT     0x0100

// bits in the node id configuration byte
#define NODE_BAND       0xC0        // frequency band
#define NODE_ACKANY     0x20        // ack on broadcast packets if set
#define NODE_ID         0x1F        // id of this node, as A..Z or 1..31

// transceiver states, these determine what to do with each interrupt
enum {
    TXCRC1, TXCRC2, TXTAIL, TXDONE, TXIDLE,
    TXRECV,
    TXPRE1, TXPRE2, TXPRE3, TXSYN1, TXSYN2,
};

static uint8_t nodeid;              // address of this node
static uint8_t group;               // network group
static volatile uint8_t rxfill;     // number of data bytes in RF12Mod_buf
static volatile int8_t rxstate;     // current transceiver state

#define RETRIES     8               // stop retrying after 8 times
#define RETRY_MS    1000            // resend packet every second until ack'ed

static uint8_t ezInterval;          // number of seconds between transmits
static uint8_t ezSendBuf[RF12Mod_MAXDATA]; // data to send
static char ezSendLen;              // number of bytes to send
static uint8_t ezPending;           // remaining number of retries
static long ezNextSend[2];          // when was last retry [0] or data [1] sent

volatile uint16_t RF12Mod_crc;         // running crc value
volatile uint8_t RF12Mod_buf[RF_MAX];  // recv/xmit buf, including hdr & crc bytes
long RF12Mod_seq;                      // seq number of encrypted packet (or -1)

static uint32_t seqNum;             // encrypted send sequence number
static uint32_t cryptKey[4];        // encryption key to use
void (*crypterMod)(uint8_t);           // does en-/decryption (null if disabled)

void RF12Mod_spiInit () {
    bitSet(SS_PORT, SS_BIT);
    bitSet(SS_DDR, SS_BIT);
    digitalWrite(SPI_SS, 1);
    pinMode(SPI_SS, OUTPUT);
    pinMode(SPI_MOSI, OUTPUT);
    pinMode(SPI_MISO, INPUT);
    pinMode(SPI_SCK, OUTPUT);
#ifdef SPCR    
    SPCR = _BV(SPE) | _BV(MSTR);
#if F_CPU > 10000000
    // use clk/2 (2x 1/4th) for sending (and clk/8 for recv, see RF12Mod_xferSlow)
    SPSR |= _BV(SPI2X);
#endif
#else
    // ATtiny
    USICR = bit(USIWM0);
#endif    
    pinMode(RFM_IRQ, INPUT);
    digitalWrite(RFM_IRQ, 1); // pull-up
}

static uint8_t RF12Mod_byte (uint8_t out) {
#ifdef SPDR
    SPDR = out;
    // this loop spins 4 usec with a 2 MHz SPI clock
    while (!(SPSR & _BV(SPIF)))
        ;
    return SPDR;
#else
    // ATtiny
    USIDR = out;
    byte v1 = bit(USIWM0) | bit(USITC);
    byte v2 = bit(USIWM0) | bit(USITC) | bit(USICLK);
#if F_CPU <= 5000000
    // only unroll if resulting clock stays under 2.5 MHz
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
#else
    for (uint8_t i = 0; i < 8; ++i) {
        USICR = v1;
        USICR = v2;
    }
#endif
    return USIDR;
#endif
}

static uint16_t RF12Mod_xferSlow (uint16_t cmd) {
    // slow down to under 2.5 MHz
#if F_CPU > 10000000
    bitSet(SPCR, SPR0);
#endif
    bitClear(SS_PORT, SS_BIT);
    uint16_t reply = RF12Mod_byte(cmd >> 8) << 8;
    reply |= RF12Mod_byte(cmd);
    bitSet(SS_PORT, SS_BIT);
#if F_CPU > 10000000
    bitClear(SPCR, SPR0);
#endif
    return reply;
}

#if OPTIMIZE_SPI
static void RF12Mod_xfer (uint16_t cmd) {
    // writing can take place at full speed, even 8 MHz works
    bitClear(SS_PORT, SS_BIT);
    RF12Mod_byte(cmd >> 8) << 8;
    RF12Mod_byte(cmd);
    bitSet(SS_PORT, SS_BIT);
}
#else
#define RF12Mod_xfer RF12Mod_xferSlow
#endif

// access to the RFM12B internal registers with interrupts disabled
uint16_t RF12Mod_control(uint16_t cmd) {
#ifdef EIMSK
    bitClear(EIMSK, INT0);
    uint16_t r = RF12Mod_xferSlow(cmd);
    bitSet(EIMSK, INT0);
#else
    // ATtiny
    bitClear(GIMSK, INT0);
    uint16_t r = RF12Mod_xferSlow(cmd);
    bitSet(GIMSK, INT0);
#endif
    return r;
}

static void RF12Mod_interrupt() {
    // a transfer of 2x 16 bits @ 2 MHz over SPI takes 2x 8 us inside this ISR
    // correction: now takes 2 + 8 µs, since sending can be done at 8 MHz
    RF12Mod_xfer(0x0000);
    
    if (rxstate == TXRECV) {
        uint8_t in = RF12Mod_xferSlow(RF_RX_FIFO_READ);

        if (rxfill == 0 && group != 0)
            RF12Mod_buf[rxfill++] = group;
            
        RF12Mod_buf[rxfill++] = in;
        RF12Mod_crc = _crc16_update(RF12Mod_crc, in);

        if (rxfill >= RF12Mod_len + 5 || rxfill >= RF_MAX)
            RF12Mod_xfer(RF_IDLE_MODE);
    } else {
        uint8_t out;

        if (rxstate < 0) {
            uint8_t pos = 3 + RF12Mod_len + rxstate++;
            out = RF12Mod_buf[pos];
            RF12Mod_crc = _crc16_update(RF12Mod_crc, out);
        } else
            switch (rxstate++) {
                case TXSYN1: out = 0x2D; break;
                case TXSYN2: out = group; rxstate = - (2 + RF12Mod_len); break;
                case TXCRC1: out = RF12Mod_crc; break;
                case TXCRC2: out = RF12Mod_crc >> 8; break;
                case TXDONE: RF12Mod_xfer(RF_IDLE_MODE); // fall through
                default:     out = 0xAA;
            }
            
        RF12Mod_xfer(RF_TXREG_WRITE + out);
    }
}

#if PINCHG_IRQ
    #if RFM_IRQ < 8
        ISR(PCINT2_vect) {
            while (!bitRead(PIND, RFM_IRQ))
                RF12Mod_interrupt();
        }
    #elif RFM_IRQ < 14
        ISR(PCINT0_vect) { 
            while (!bitRead(PINB, RFM_IRQ - 8))
                RF12Mod_interrupt();
        }
    #else
        ISR(PCINT1_vect) {
            while (!bitRead(PINC, RFM_IRQ - 14))
                RF12Mod_interrupt();
        }
    #endif
#endif

static void RF12Mod_recvStart () {
    rxfill = RF12Mod_len = 0;
    RF12Mod_crc = ~0;
#if RF12Mod_VERSION >= 2
    if (group != 0)
        RF12Mod_crc = _crc16_update(~0, group);
#endif
    rxstate = TXRECV;    
    RF12Mod_xfer(RF_RECEIVER_ON);
}

uint8_t RF12Mod_recvDone () {
    if (rxstate == TXRECV && (rxfill >= RF12Mod_len + 5 || rxfill >= RF_MAX)) {
        rxstate = TXIDLE;
        if (RF12Mod_len > RF12Mod_MAXDATA)
            RF12Mod_crc = 1; // force bad crc if packet length is invalid
        if (!(RF12Mod_hdr & RF12Mod_HDR_DST) || (nodeid & NODE_ID) == 31 ||
                (RF12Mod_hdr & RF12Mod_HDR_MASK) == (nodeid & NODE_ID)) {
            if (RF12Mod_crc == 0 && crypterMod != 0)
                crypterMod(0);
            else
                RF12Mod_seq = -1;
            return 1; // it's a broadcast packet or it's addressed to this node
        }
    }
    if (rxstate == TXIDLE)
        RF12Mod_recvStart();
    return 0;
}

uint8_t RF12Mod_canSend () {
    // no need to test with interrupts disabled: state TXRECV is only reached
    // outside of ISR and we don't care if rxfill jumps from 0 to 1 here
    if (rxstate == TXRECV && rxfill == 0 &&
            (RF12Mod_byte(0x00) & (RF_RSSI_BIT >> 8)) == 0) {
        RF12Mod_xfer(RF_IDLE_MODE); // stop receiver
        //XXX just in case, don't know whether these RF12Mod reads are needed!
        // RF12Mod_xfer(0x0000); // status register
        // RF12Mod_xfer(RF_RX_FIFO_READ); // fifo read
        rxstate = TXIDLE;
        return 1;
    }
    return 0;
}

void RF12Mod_sendStart (uint8_t hdr) {
    RF12Mod_hdr = hdr & RF12Mod_HDR_DST ? hdr :
                (hdr & ~RF12Mod_HDR_MASK) + (nodeid & NODE_ID);
    if (crypterMod != 0)
        crypterMod(1);
    
    RF12Mod_crc = ~0;
#if RF12Mod_VERSION >= 2
    RF12Mod_crc = _crc16_update(RF12Mod_crc, group);
#endif
    rxstate = TXPRE1;
    RF12Mod_xfer(RF_XMITTER_ON); // bytes will be fed via interrupts
}

void RF12Mod_sendStart (uint8_t hdr, const void* ptr, uint8_t len) {
    RF12Mod_len = len;
    memcpy((void*) RF12Mod_data, ptr, len);
    RF12Mod_sendStart(hdr);
}

// deprecated
void RF12Mod_sendStart (uint8_t hdr, const void* ptr, uint8_t len, uint8_t sync) {
    RF12Mod_sendStart(hdr, ptr, len);
    RF12Mod_sendWait(sync);
}

void RF12Mod_sendWait (uint8_t mode) {
    // wait for packet to actually finish sending
    // go into low power mode, as interrupts are going to come in very soon
    while (rxstate != TXIDLE)
        if (mode) {
            // power down mode is only possible if the fuses are set to start
            // up in 258 clock cycles, i.e. approx 4 us - else must use standby!
            // modes 2 and higher may lose a few clock timer ticks
            set_sleep_mode(mode == 3 ? SLEEP_MODE_PWR_DOWN :
#ifdef SLEEP_MODE_STANDBY
                           mode == 2 ? SLEEP_MODE_STANDBY :
#endif
                                       SLEEP_MODE_IDLE);
            sleep_mode();
        }
}

/*!
  Call this once with the node ID (0-31), frequency band (0-3), and
  optional group (0-255 for RF12ModB, only 212 allowed for RF12Mod).
*/
uint8_t RF12Mod_initialize (uint8_t id, uint8_t band, uint8_t g) {
    nodeid = id;
    group = g;
    
    RF12Mod_spiInit();

    RF12Mod_xfer(0x0000); // intitial SPI transfer added to avoid power-up problem

    RF12Mod_xfer(RF_SLEEP_MODE); // DC (disable clk pin), enable lbd
    
    // wait until RFM12B is out of power-up reset, this takes several *seconds*
    RF12Mod_xfer(RF_TXREG_WRITE); // in case we're still in OOK mode
    while (digitalRead(RFM_IRQ) == 0)
        RF12Mod_xfer(0x0000);
        
    RF12Mod_xfer(0x80C7 | (band << 4)); // EL (ena TX), EF (ena RX FIFO), 12.0pF 
    RF12Mod_xfer(0xA640); // 868MHz 
    RF12Mod_xfer(0xC606); // approx 49.2 Kbps, i.e. 10000/29/(1+6) Kbps
    RF12Mod_xfer(0x94A2); // VDI,FAST,134kHz,0dBm,-91dBm 
    RF12Mod_xfer(0xC2AC); // AL,!ml,DIG,DQD4 
    if (group != 0) {
        RF12Mod_xfer(0xCA83); // FIFO8,2-SYNC,!ff,DR 
        RF12Mod_xfer(0xCE00 | group); // SYNC=2DXX； 
    } else {
        RF12Mod_xfer(0xCA8B); // FIFO8,1-SYNC,!ff,DR 
        RF12Mod_xfer(0xCE2D); // SYNC=2D； 
    }
    RF12Mod_xfer(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN 
    RF12Mod_xfer(0x9850); // !mp,90kHz,MAX OUT 
    RF12Mod_xfer(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0 
    RF12Mod_xfer(0xE000); // NOT USE 
    RF12Mod_xfer(0xC800); // NOT USE 
    RF12Mod_xfer(0xC049); // 1.66MHz,3.1V 

    rxstate = TXIDLE;
#if PINCHG_IRQ
    #if RFM_IRQ < 8
        if ((nodeid & NODE_ID) != 0) {
            bitClear(DDRD, RFM_IRQ);      // input
            bitSet(PORTD, RFM_IRQ);       // pull-up
            bitSet(PCMSK2, RFM_IRQ);      // pin-change
            bitSet(PCICR, PCIE2);         // enable
        } else
            bitClear(PCMSK2, RFM_IRQ);
    #elif RFM_IRQ < 14
        if ((nodeid & NODE_ID) != 0) {
            bitClear(DDRB, RFM_IRQ - 8);  // input
            bitSet(PORTB, RFM_IRQ - 8);   // pull-up
            bitSet(PCMSK0, RFM_IRQ - 8);  // pin-change
            bitSet(PCICR, PCIE0);         // enable
        } else
            bitClear(PCMSK0, RFM_IRQ - 8);
    #else
        if ((nodeid & NODE_ID) != 0) {
            bitClear(DDRC, RFM_IRQ - 14); // input
            bitSet(PORTC, RFM_IRQ - 14);  // pull-up
            bitSet(PCMSK1, RFM_IRQ - 14); // pin-change
            bitSet(PCICR, PCIE1);         // enable
        } else
            bitClear(PCMSK1, RFM_IRQ - 14);
    #endif
#else
    if ((nodeid & NODE_ID) != 0)
        attachInterrupt(0, RF12Mod_interrupt, LOW);
    else
        detachInterrupt(0);
#endif
    
    return nodeid;
}

void RF12Mod_onOff (uint8_t value) {
    RF12Mod_xfer(value ? RF_XMITTER_ON : RF_IDLE_MODE);
}

uint8_t RF12Mod_config (uint8_t show) {
    uint16_t crc = ~0;
    for (uint8_t i = 0; i < RF12Mod_EEPROM_SIZE; ++i)
        crc = _crc16_update(crc, eeprom_read_byte(RF12Mod_EEPROM_ADDR + i));
    if (crc != 0)
        return 0;
        
    uint8_t nodeId = 0, group = 0;
    for (uint8_t i = 0; i < RF12Mod_EEPROM_SIZE - 2; ++i) {
        uint8_t b = eeprom_read_byte(RF12Mod_EEPROM_ADDR + i);
        if (i == 0)
            nodeId = b;
        else if (i == 1)
            group = b;
        else if (b == 0)
            break;
        else if (show)
            Serial.print((char) b);
    }
    if (show)
        Serial.println();
    
    RF12Mod_initialize(nodeId, nodeId >> 6, group);
    return nodeId & RF12Mod_HDR_MASK;
}

void RF12Mod_sleep (char n) {
    if (n < 0)
        RF12Mod_control(RF_IDLE_MODE);
    else {
        RF12Mod_control(RF_WAKEUP_TIMER | 0x0500 | n);
        RF12Mod_control(RF_SLEEP_MODE);
        if (n > 0)
            RF12Mod_control(RF_WAKEUP_MODE);
    }
    rxstate = TXIDLE;
}

char RF12Mod_lowbat () {
    return (RF12Mod_control(0x0000) & RF_LBD_BIT) != 0;
}

void RF12Mod_easyInit (uint8_t secs) {
    ezInterval = secs;
}

char RF12Mod_easyPoll () {
    if (RF12Mod_recvDone() && RF12Mod_crc == 0) {
        byte myAddr = nodeid & RF12Mod_HDR_MASK;
        if (RF12Mod_hdr == (RF12Mod_HDR_CTL | RF12Mod_HDR_DST | myAddr)) {
            ezPending = 0;
            ezNextSend[0] = 0; // flags succesful packet send
            if (RF12Mod_len > 0)
                return 1;
        }
    }
    if (ezPending > 0) {
        // new data sends should not happen less than ezInterval seconds apart
        // ... whereas retries should not happen less than RETRY_MS apart
        byte newData = ezPending == RETRIES;
        long now = millis();
        if (now >= ezNextSend[newData] && RF12Mod_canSend()) {
            ezNextSend[0] = now + RETRY_MS;
            // must send new data packets at least ezInterval seconds apart
            // ezInterval == 0 is a special case:
            //      for the 868 MHz band: enforce 1% max bandwidth constraint
            //      for other bands: use 100 msec, i.e. max 10 packets/second
            if (newData)
                ezNextSend[1] = now +
                    (ezInterval > 0 ? 1000L * ezInterval
                                    : (nodeid >> 6) == RF12Mod_868MHZ ?
                                            13 * (ezSendLen + 10) : 100);
            RF12Mod_sendStart(RF12Mod_HDR_ACK, ezSendBuf, ezSendLen);
            --ezPending;
        }
    }
    return ezPending ? -1 : 0;
}

char RF12Mod_easySend (const void* data, uint8_t size) {
    if (data != 0 && size != 0) {
        if (ezNextSend[0] == 0 && size == ezSendLen &&
                                    memcmp(ezSendBuf, data, size) == 0)
            return 0;
        memcpy(ezSendBuf, data, size);
        ezSendLen = size;
    }
    ezPending = RETRIES;
    return 1;
}

// XXTEA by David Wheeler, adapted from http://en.wikipedia.org/wiki/XXTEA

#define DELTA 0x9E3779B9
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + \
                                            (cryptKey[(uint8_t)((p&3)^e)] ^ z)))

static void cryptFun (uint8_t send) {
    uint32_t y, z, sum, *v = (uint32_t*) RF12Mod_data;
    uint8_t p, e, rounds = 6;
    
    if (send) {
        // pad with 1..4-byte sequence number
        *(uint32_t*)(RF12Mod_data + RF12Mod_len) = ++seqNum;
        uint8_t pad = 3 - (RF12Mod_len & 3);
        RF12Mod_len += pad;
        RF12Mod_data[RF12Mod_len] &= 0x3F;
        RF12Mod_data[RF12Mod_len] |= pad << 6;
        ++RF12Mod_len;
        // actual encoding
        char n = RF12Mod_len / 4;
        if (n > 1) {
            sum = 0;
            z = v[n-1];
            do {
                sum += DELTA;
                e = (sum >> 2) & 3;
                for (p=0; p<n-1; p++)
                    y = v[p+1], z = v[p] += MX;
                y = v[0];
                z = v[n-1] += MX;
            } while (--rounds);
        }
    } else if (RF12Mod_crc == 0) {
        // actual decoding
        char n = RF12Mod_len / 4;
        if (n > 1) {
            sum = rounds*DELTA;
            y = v[0];
            do {
                e = (sum >> 2) & 3;
                for (p=n-1; p>0; p--)
                    z = v[p-1], y = v[p] -= MX;
                z = v[n-1];
                y = v[0] -= MX;
            } while ((sum -= DELTA) != 0);
        }
        // strip sequence number from the end again
        if (n > 0) {
            uint8_t pad = RF12Mod_data[--RF12Mod_len] >> 6;
            RF12Mod_seq = RF12Mod_data[RF12Mod_len] & 0x3F;
            while (pad-- > 0)
                RF12Mod_seq = (RF12Mod_seq << 8) | RF12Mod_data[--RF12Mod_len];
        }
    }
}

void RF12Mod_encrypt (const uint8_t* key) {
    // by using a pointer to cryptFun, we only link it in when actually used
    if (key != 0) {
        for (uint8_t i = 0; i < sizeof cryptKey; ++i)
            ((uint8_t*) cryptKey)[i] = eeprom_read_byte(key + i);
        crypterMod = cryptFun;
    } else
        crypterMod = 0;
}
