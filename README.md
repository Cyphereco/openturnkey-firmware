# Introduction
The repository is the prove-of-concept of OTK which implements required features for OTK on NRF52832 for the main chip and XTB0811 for the fingerprint recognition module.

# Current features
* Generate random seed
* Generate HD master node which includes private key, public key and chain code.
* Generate HD child node from parent node and given index.
* Sign a message with private key.
* Verify a signature with public key.
* Write device info and HD nodes to storage.
* Read device info and HD nodes from storage.
* Set NFC records for reading.
* Writable NFC records(currently it doesn't process the written data).
* Power management. Go to sleep mode when there is no NFC communication for 30 seconds.
* Fingerprint capture, enroll, erase and matching
* Sign transaction - OTK device signs the transaction(a message in the record#2 for sign transaction comment), and once it's completed the device goes to reading result state(LED1 4 fast blinking). Phone APP can read the result(i.e. signature) and the device goes to idle state. If the result is not read by timeout(10 seconds for now), the device goes to idle.
* Verfiy signature - OTK device appends a string ~~(for now it's "otk")~~ (following the Bitcoin sign message protocol) to the message sent from APP and sign it back. APP can verify the signature to see if the device really holds the private key. ~~Device behaviour is the same as sign transaction command.~~ (now it only return the signature as hex (64) bytes string, not yet implement full signature construction method which will calculate the value of 1st additional byte (0x1b ~ 0x22) to distinguish the corresponding address type.
* Get key info - It gets master serialized public key and extended serialized public key.
* Set new key - Given child index and derive the new HD child node from master node. *Note: Currently only support one level child node.*

# TODOs next
* Implement get Key info command.
* Implement set new key command.

# Build OTK-POC
## Clone the repository to where you can build with NRF SDK.
Currently only build with GCC is supported. More info for building with GCC please see https://devzone.nordicsemi.com/tutorials/b/getting-started/posts/development-with-gcc-and-eclipse
## Configure SDK root path in armgcc/Makefile
```Bash
SDK_ROOT := ../../..
```
## Make it
```Bash
make
```
## Burn the image to the device
Copy armgcc/_build/nrf52832_xxaa.hex to the JLINK flash disk of the development board. More details please check http://www.nordicsemi.com/eng/Products/Getting-started-with-the-nRF52-Development-Kit

# Play with current OTK-POC
Once the image is loaded to the deivce, what you can try includes:
* Approach a mobile phone with NFC enabled to the anttena of the device, NFC tools(an APP) will be bring up. If the APP is not installed, it goes to the APP page of Play Store.
* In NFC tools you can:
    * Read records. Currently it contains public key, manufactorer, manufactured date and lock state.
    * Write records. You can write record in NFC tools' WRITE tab(Notice: you have to move the device away from handset after write record so that the command starts to execute).
        * Write text record#0 with data '1': Lock device(fingerprint capture and enroll).
        * Write text record#0 with data '2': Unlock device(fingerprint match and erase).
        * Write text record#0 with data '3' and record#1 for the message to sign: Sign transaction. After command wrote, it requires finger print match(led: 2 fast blinking), once matched, it sign the message and then wait for APP to read the result(led: 4 fast blinking). After read result, it goes to idle state.
        * Write text record#0 with data '4' and record#1 for the message to sign: Verify signature.
        * Write text record#0 with data '5': Get key info. After command wrote, it requires finger print match(led: 2 fast blinking), once matched, it sets the records(#0 for master serialized public key and #1 for extended seialized public key) of key info and then wait for APP to read the result(led: 4 fast blinking). After read result, it goes to idle state.
        * Write text record#0 with data '6' and record#1 for child index.
        * Write text record#0 with data '9': Erase all finger prints(development feature only, no matching is required).
    * Check detailed log from RTT console. You see what happens in details.
* When there is no NFC communication for 30 seconds, the device goes to sleep mode(all LEDs turns off and NFC also be turned down). Touch fingerprint sensor or button 1 wakes up the device.

## LEDs and buttons definition
* LED 0 - State indication.
    * Long blinking(1s on - 1s off). Idle and locked.
    * Short blinking(0.5s on - 0.5s off). Idle and unlocked.
    * 3 fast blinking - Finger print capturing.
    * 2 fast blinking - Finger print matching.
    * 4 fast blinking - Waiting for reading result(used for sign transaction and verify signature).
* LED 1 - NFC field on.
* LED 2 - Waiting on UART read.
* LED 3 - Not used.

* Button 0 - Wake up device when in sleep mode.
* Button 1 - Not used.
* Button 2 - Not used.
* Button 3 - Not used.

## Pin definition
* GPIO 25 - UART RX(XTB0811 pin 3->NRF52832 pin 25)
* GPIO 24 - UART TX(NRF52832 pin 24->XTB0811 pin 4)
* GPIO 23 - Wake up pin(XTB0811 Pin 5->NRF52832 Pin 23)

# Unit tests
Current unit tests include only four tests:
* Generate master HD node from a seed
* Derive child key from master node
* Sign a hash with private key
* Verify the signature with public key

## Build for unit tests
```Bash
make clean;make unittest
```
## Run unit tests
Load the image to device and it runs the tests. LED0 indicate the success of unit tests. See UART console log for detailed unit test result.
## LEDs and buttons definition for unit test
* LED 0 - All tests passed.
* LED 1 - Not used.
* LED 2 - Not used.
* LED 3 - Not used.

* Button 0 - Not used.
* Button 1 - Not used.
* Button 2 - Not used.
* Button 3 - Not used.

# Issues
* No issue now.
