# ATEM Wireless Tally Light

Wireless tally light system for ATEM switchers.

## Setup and Configuration

Note: We're assuming that you have a transmitter board with the Arduino Ethernet and also a receiver board with a JeeNode.

1.  Copy the dependent libraries from the project's libraries directory to your systems Arduino libraries directory. You may need to overwrite some of the libraries.
1.  Upload `ATEM_Tally_Receiver.ino` to your receiver node.
1.  Upload `ATEM_Tally_Transmitter.ino` to your transmitter.
1.  Connect the transmitter to the same network as the ATEM switcher and power it on (it may take a minute to connect to ATEM).
1.  Set the node # on your receiver using the DIP pins. It will blink the node # on power-on.

Once connected, the receiver will light up RED if on PROGRAM and GREEN if on PREVIEW.

## Transmitter

A transmitter transmits ATEM PROGRAM and PREVIEW camera numbers using an RF12 radio.

### Default Settings

	IP address		: 192.168.1.234
	Mac address 	: DE:AD:BE:EF:FE:ED
	Switcher IP		: 192.168.1.240
	Switcher PORT	: 49910

The settings can be modified using the web interface [http://192.168.1.234/](http://192.168.1.234/) or reset using the RESET button on the transmitter.

### LED States

The following are transmitter LED states:

	RED 		: Not connected to network (or ATEM switcher)
	BLUE 		: Connected to ATEM switcher
	RED BLINK	: Radio signal sent

## Receiver

A receiver receives radio signals from the transmitter and triggers either the PROGRAM LED or the PREVIEW LED according to the transmitter and ATEM.

### LED States

The following are receiver LED states:

	POWER-ON BLUE BLINK	: Blinks node # or rapidly for test mode
	CONSTANT BLUE BLINK	: In test mode (receiving radio signal)
	RED					: Node # is on PROGRAM
	GREEN				: Node # is on PREVIEW

### Test Mode

Setting the receiver's node # to 0 (all DIP pins off) will switch it to _test mode_. When in _test mode_ the receiver will blink BLUE every 1 second if it's receiving a radio signal. On power-on in _test mode_ the LED will blink BLUE rapidly for a second or so.

### Node DIP Pins

The node DIP pins are binary-based. For example, when the DIP pins are `0110`, the node # is `5`.

Note: The receiver needs to be restarted whenever the node # is changed.

## Library Modifications

The RF12 library was slightly modified to allow the Arduino Ethernet to support both the Ethernet and the RF12 radio. Since Ethernet and the radio both use the same `SS_BIT`, they will conflict with each other. Leave Ethernet as it is and modify the RF12 library only. 

Note: There is a new class called `RF12Mod` which is a copy of `RF12` with slight modifications.

## Credits

Based on the [Arduino ATEM Library](https://github.com/kasperskaarhoj/Arduino-Library-for-ATEM-Switchers)

## Contributing

We welcome any contributions and bug fixes. Feel free to send push requests.