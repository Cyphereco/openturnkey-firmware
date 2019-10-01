This is the repository of the firmware for OpenTurnKey device, based on NRF52832(MCU+NFC)/XTF1604(FP_sensor).

# Features
* Generate random seed on initial image flash.
* Generate HD master node which includes private key, public key and chain code.
* Generate HD derivative child node from parent node with given index.
* Generate ECDSA (secp256k1) signature with private key.
* Verify ECDSA (secp256k1) signature.
* Save/update device info and HD nodes to storage.
* Set dynamic NFC records for reading.
* Execute command through writable NFC records.
* Power management: sleep mode (15 seconds in no action), power measurement and indication.
* Fingerprint management: enrollment, validation, and removal.
* Fingerprint authorization for command execution.
* PIN code management: default(99999999), set/reset
* PIN code authorization for command execution.

# Build OTK-POC
## Clone the repository (NRF SDK included)
Currently only build with GCC is supported. More info for building with GCC please see https://devzone.nordicsemi.com/tutorials/b/getting-started/posts/development-with-gcc-and-eclipse
## Configure SDK root path in armgcc/Makefile
```Bash
SDK_ROOT := ../../..
```
## Build source code for release version
```Bash
make
```
## Build source code for debug version (Debug version uses fix seed, which is specified in key.c source code, and will not validate the NFC request session ID.)
```Bash
make
```
## Flash build image (.hex) to the connected OTK device via JLink
```Bash
make flash
```
## Reset the connected OTK device via JLink
```Bash
make reset
```
## Clear built result
```Bash
make clean
```
## Burn the image to the device
Copy armgcc/_build/nrf52832_xxaa.hex to the JLINK flash disk of the development board. More details please check http://www.nordicsemi.com/eng/Products/Getting-started-with-the-nRF52-Development-Kit

# NFC Record Format Example / Explanation:
```
Record 1 (Binary/Software URI): com.cyphereco.openturnkey
Record 2 (Text/Device Information):
    Key Mint: Cyphereco OU
    Mint Date: 2019/06/09
    H/W Version: 1.2
    F/W Version: 1.1.6
    Serial No.: b7022ddc25
    Battery Level: 10% / 2860mV
    Note: <user_note>
Record 3 (Text/OTK State): 00000000 
    * 00 00 00 00 = lock state, command execution state, request command, failure reason
    * Refer to the macro definition in nfc.h
Record 4 (Text/Public Key): <public_key>
Record 5 (Text/Session Data):
    <Session_ID>
    14816020211
    <BTC_Addr>
    1LB231Bp3Lx4Vus...
Record 6 (Text/Session Data Signature):
    <Signature>
```

# NFC Request Command Requirements:
1. Session ID must be correct for each session.
2. Request ID can be a random number or a number of any rules implemented by the client software by its design, for example, to check correct request ID when read the NFC updated record after comment execution.
3. Only valid command code will be executed.
4. Request data field is mandatory for some commands.
5. Request option param is optional, but if it is used and there is no request data needed, the client software still need to put some data in request data such as 0. (NFC record is parse in sequential order).

# NFC Request Command Example:
```
Record 1 (Text/Session ID): 12345 
Record 2 (Text/Request ID): 54321
Record 3 (Text/Request Command): 163
Record 4 (Text/Request Data): <hash_value>
Record 5 (Text/Request Options): key=1,pin=99999999
```

# List of NFC Commands:
```
typedef enum {
    NFC_REQUEST_CMD_INVALID = 0,        /* 0 / 0x0, Invalid,  For check purpose. */ 
    NFC_REQUEST_CMD_LOCK = 0xA0,        /* 160 / 0xA0, Enroll fingerpint on OTK. */ 
    NFC_REQUEST_CMD_UNLOCK,             /* 161 / 0xA1, Erase enrolled fingerprint and reset secure PIN to default, OTK (pre)authorization is required. */  
    NFC_REQUEST_CMD_SHOW_KEY,           /* 162 / 0xA2, Present master/derivative extend keys and derivative path and secure PIN code, OTK (pre)authorization is required. */  
    NFC_REQUEST_CMD_SIGN,               /* 163 / 0xA3, Sign external data (32 bytes hash data), taking request option: 1/Using master key, 0/Using derivated key(default), OTK (pre)authorization is required. */  
    NFC_REQUEST_CMD_SET_KEY,            /* 164 / 0xA4, Set/chagne derivative KEY (path), OTK (pre)authorization is required. */ 
    NFC_REQUEST_CMD_SET_PIN,            /* 165 / 0xA5, Set/change secure PIN setting, OTK (pre)authorization is required. */ 
    NFC_REQUEST_CMD_SET_NOTE,           /* 166 / 0xA6, Set customized user note. */ 
    NFC_REQUEST_CMD_CANCEL,             /* 167 / 0xA7, Cancel previous command request. */ 
    NFC_REQUEST_CMD_LAST                /*  -- /, Not used, only for completion. */ 
} NFC_REQUEST_COMMAND;
```

# OpenTurnKey Usage:
1. Fingerprint Pre-authenticated Mode.
    * If OpenTurnKey is locked with a user's fingerprint, the user may pre-authorize OpenTurnKey with the user's enrolled fingerprint by holding on OpenTurnKey fingerprint until seeing a blinking GREEN light. 
    * When OpenTurnKey is pre-authorized, it can accept any valid NFC Request Command and execute it.
2. PIN code authentication Mode.
    * A client software must read OpenTurnKey firstly to read the Session ID and OTK_State before sending any request.
    * If the OTK_state is not (2) <Authrozied>, the client software should ask user to input a PIN code and attach the PIN code in the request option as pin=<pin_code>.
    * If there are other request option parameters, separated each one of them with comma (,).
    * If the PIN code matches with the setting in OpenTurnKey, the request command will be executed, otherwise, it will be rejected and output a failure result. 



## LED indication
* On boot-up:
    * GREEN         - OTK is unlocked (no fingerprint enrolled)
    * RED           - OTK is locked (fingerprint enrolled, currently only allow one fingerprint)
    * Blink times   - Battery Power level, No blink = 65-100%, Blink Twice = 30-65%, Blink Three times = 0-30% (OTK need to be charged)
* On run-time:
    * Blinking BLUE      - stand by mode (double blink before sleep).
    * Blinking GREEN/RED - fingerprint capturing
    * Blinking GREEN     - command executed successfuly / data ready
    * Blinking RED       - command executed failed / error

# Unit tests
Current unit tests include only four tests:
* Generate master HD node from a seed
* Derive child key from master node
* Sign a hash with private key
* Verify the signature with public key
