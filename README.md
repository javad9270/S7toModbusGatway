# S7toModbus
This project is firmware for Snap 7 siemens to MODBUS TCP gateway. It uses JSON files to map the data fields in siemens PLCs to MODBUS data fields. A sample JSON file is as bellow:
```JSON
{
	"IP":"192.168.1.5",
	"RackNum":0,
	"SlotNum":2,
	"DataItems":[
			{
				"Area":"MB",
				"dbNum":0,
				"groupId":0,
				"Start":213,
				"BitIndex":4,
				"type":"Bool",
				"Access":false,
				"Name":"MSP1 POWER SUPPLY FAULT"
			},
			{
				"Area":"DB",
				"dbNum":10,
				"groupId":18,
				"Start":128,
				"BitIndex":0,
				"type":"Int16",
				"Access":true,
				"Name":"SCODE 1 Tempreatrue SetpoInt16 1"
			},
		]
}
```
This firmware runs on Raspberry pi. Some of output pins are configured to trun on/off LEDs for RUN, SIEMENS and MODBUS run indicators. Also, there are two LAN interfaces configured, one for MODBUS connection and the other for siemense connection. The Modbus client can connect to the device and read PLC data based on the configuration in JSON file.


## License
The code could only be run on a device with valid MAC address. The bcrypt hash of mac address is in the code is built into the executable. Upon run, the MAC address is read and compared with the hash value in the code. If comparison fails, the process halts and fault LED will turn on. For every device, the code should be built.  So every device should be licensed. 

## Build
The cmake file is present, which is used to build the code.

## Network Configuration
The IP of device is set in the configure network shell. There are some shell scripts for starting the firmware. They are run by the system file, which should be added to services in linux operating system. 

## Restart Button
Also there is a shell script, monitoring one of the input pins. If the input pin is held for more than some duration, the whole firmware process will be restarted. 

