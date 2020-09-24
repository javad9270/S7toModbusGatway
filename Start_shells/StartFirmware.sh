#!/bin/sh
./ConfigNetwork.sh
sleep 2
./S7toModbusGateway Configuration.json
