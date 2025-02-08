# FujiNet PC BIOS Specification

The FujiNet PC BIOS Specification reserves software INT F5 to provide a standardized way to communicate with the FujiNet, regardless of how it is physically connected. It is designed to send any command that the PC FujiNet can interpret, to any of its virtual devices, with or without payload, to or from the PC or FujiNet.

The current version of this document may be found on wiki: 
https://github.com/FujiNetWIFI/fujinet-firmware/wiki/MS%E2%80%90DOS-BIOS-Specification

## Canonical implementation

Can be found here: https://github.com/FujiNetWIFI/fujinet-rs232/tree/main/sys

## Registers Used

| Register | Description                            |
|---       |---                                     |
| AH       | 00 = No Payload, 40 = READ, 80 = WRITE |
| AL       | Subdevice                              |
| BX       | OFFSET for payload buffer              |
| CL       | Command                                |
| DH       | AUX 2                                  |
| DL       | AUX 1                                  |
| DI       | Length of Payload Buffer               |
| ES       | SEGMENT for payload buffer             |

** Note: ** ES, DI and BX are not used for AH=00

** Note: ** ES:BX Specifies the far location in memory of a payload buffer. DI specifies its length.

## Register AH - Set Payload direction

The BIOS has exactly three functions; set by register AH. These differ in how any payload is handled, and in which direction the payload should be sent.

| AH   | Description                                                                      |
|---   |---                                                                               |
| 0x00 | No payload, Just send the command in the command frame.                          |
| 0x40 | READ. Payload comes from the FujiNet to the PC to address in ES:BX, length in DI |
| 0x80 | WRITE. Payload comes from PC to the FujiNet via address in ES:BX, length in DI   |

## Register AL - Set Device ID

The FujiNet has a number of virtual devices, all referenced by device ID. Register AL specifies the destination device for the command.

| AL   | Description |
|---   |--- |
| 0x31 | Disk Drive (block device) #1  |
| 0x32 | Disk Drive (block device) #2  |
| 0x33 | Disk Drive (block device) #3  |
| 0x34 | Disk Drive (block device) #4  |
| 0x35 | Disk Drive (block device) #5  |
| 0x36 | Disk Drive (block device) #6  |
| 0x37 | Disk Drive (block device) #7  |
| 0x38 | Disk Drive (block device) #8  |
| 0x45 | Real Time Clock (NTP)         |
| 0x70 | FujiNet Control Device        |
| 0x71 | Network (character device) #1 | 
| 0x72 | Network (character device) #2 | 
| 0x73 | Network (character device) #3 | 
| 0x74 | Network (character device) #4 | 
| 0x75 | Network (character device) #5 | 
| 0x76 | Network (character device) #6 | 
| 0x77 | Network (character device) #7 | 
| 0x78 | Network (character device) #8 | 

## Register/Segment ES:BX

ES:BX is used to specify the segment and offset location of a buffer in the PC.

* For NONE (0x00) operations, there is no payload, and this segment and offset pair is not used.

* For READ (0x40) operations, this indicates a destination buffer for data coming from the FujiNet.

* For WRITE (0x80) operations, this indicates a source buffer for data going to the FujiNet.

## Register DI

The DI index register is used to specify the length of the payload buffer. This parameter is required if AH is either 0x40 or 0x80, and is ignored if it is 0x00.

## Register CL

The low register of CX (CL) is used to specify the command byte. CH is currently not used.

For example, the commands from these pages can be used:

* [[SIO Commands for Device IDs $31 to $38]]
* [[SIO Commands for Device ID $45]]
* [[SIO Commands for Device ID $70]]
* [[SIO Commands for Device IDs $71 to $78]]

## Register DX (DL and DH)

Register DX is used to pass the AUX1 and AUX2 values for any command. These are sent as part of the command frame, and can be treated either as two 8-bit values, or a single 16-bit value. The values used for these are specific to each command and device.

For example, The block devices (0x31 - 0x38) use AUX1 and AUX2 to specify a 16-bit sector value for READ and WRITE commands.

## Return Value

For any command sent to INT F5, the AL register will contain the result code as a single 8-bit character. Valid result codes are:

| Char | Description                                           |
|---   |---                                                    |
| 'C'  | Complete. The command finished without error.         |
| 'E'  | Error. The command finished, but there was a problem. |
| 'N'  | NAK. The command was not recognized by the device.    | 

**Note:** A return value of 'A' is an ACK, but this should not be seen by the user programs, and indicates a potential protocol implementation error.

## Assembler example

The following assembler example will send a command to reset the FujiNet by sending command 0xFF, to device 0x70. 

```asm
        MOV AH,00                ; No payload
        MOV CL,FF                ; FF = Reset
        INT F5                   ; Do it. AL contains result (e.g. 'C')
```

## C Example 1: AH Function 0x40: FAC - Show FujiNet Adapter Config

```c
/**
 * @brief   Display FujiNet Adapter Config
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details
 */

#include <dos.h>

typedef struct _adapterConfig
{
	char ssid[33];
	char hostname[64];
	unsigned char localIP[4];
	unsigned char gateway[4];
	unsigned char netmask[4];
	unsigned char dnsIP[4];
	unsigned char macAddress[6];
	unsigned char bssid[6];
	char fn_version[15];
} AdapterConfig;

unsigned char get_adapterconfig(AdapterConfig *ac)
{
	union  REGS r;
	struct SREGS sr;

	r.h.ah = 0x40; /* READ               */
	r.h.al = 0x70; /* FUJI Device        */
	r.h.cl = 0xe8; /* Get Adapter Config */
	r.x.di = sizeof(AdapterConfig);  /* Expecting 140 bytes */
	
	sr.es = FP_SEG(ac);
	r.x.bx = FP_OFF(ac);

	int86x(0xF5,&r,&r,&sr);

	return r.h.al;	
}

void display_adapterconfig(AdapterConfig *ac)
{
	printf("-------------------------------------\r\n");
	printf("Current FujiNet Network Configuration\r\n");
	printf("-------------------------------------\r\n\r\n");
	
	printf("%20s %s\n","SSID:",ac->ssid);
	printf("%20s %s\n","Host Name:",ac->hostname);

	printf("%20s %u.%u.%u.%u\n","IPV4:",ac->localIP[0],
					 ac->localIP[1],
					 ac->localIP[2],
					 ac->localIP[3]);

	printf("%20s %u.%u.%u.%u\n","Gateway:",ac->gateway[0],
					    ac->gateway[1],
					    ac->gateway[2],
					    ac->gateway[3]);

	printf("%20s %u.%u.%u.%u\n","Netmask:",ac->netmask[0],
					 ac->netmask[1],
					 ac->netmask[2],
					 ac->netmask[3]);

	printf("%20s %u.%u.%u.%u\n","DNS:",ac->dnsIP[0],
					 ac->dnsIP[1],
					 ac->dnsIP[2],
					 ac->dnsIP[3]);

	printf("%20s %02X:%02X:%02X:%02X:%02X:%02X\n","MAC:",ac->macAddress[0],
							   ac->macAddress[1],
							   ac->macAddress[2],
							   ac->macAddress[3],
							   ac->macAddress[4],
							   ac->macAddress[5]);

	printf("%20s %02X:%02X:%02X:%02X:%02X:%02X\n","BSSID:",ac->bssid[0],
							   ac->bssid[1],
							   ac->bssid[2],
							   ac->bssid[3],
							   ac->bssid[4],
							   ac->bssid[5]);

	printf("%20s %s\n","Version:",ac->fn_version);
}

int main(void)
{
	AdapterConfig ac;
	unsigned char r; 

	memset(&ac,0,sizeof(AdapterConfig));

	r = get_adapterconfig(&ac);

	if (r != 'C')
	{
		printf("get_adapterConfig returned: %02x",r);
		printf("Could not fetch adapter config from FujiNet. Exiting.");
		return 1;
	}

	display_adapterconfig(&ac);

	return 0;
}
```

## Example 2: AH Function 0x80 - Set SSID

```c
/**
 * @brief   Set SSID/Password
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see COPYING for details.
 */

#include <dos.h>
#include <stdio.h>
#include <fujicom.h>

struct SETSSID
{
	char ssid[33];
	char password[64];
} ss;

union REGS r;
struct SREGS sr;

int main(int argc, char *argv[])
{
	if (argc<3)
		{
			printf("setssid <ssid> <password>\r\n");
			return 1;
		}

	strcpy(ss.ssid,argv[1]);
	strcpy(ss.password,argv[2]);

	r.h.ah = 0x80; /* WRITE */
	r.h.al = 0x70; /* to FUJI device */
	r.h.cl = 0xFB; /* Set SSID command */

	/* set payload */
	sr.es  = FP_SEG(&ss);
	r.x.bx = FP_OFF(&ss);
	r.x.di = sizeof(ss); /* payload size */
	

	int86x(0xF5,&r,&r,&sr);

	printf("Set SSID Returned '%c'\r\n",r.h.al);

	return 0;
}
```
