/*
 * ModbusSlave.h
 *
 *  Created on: May 4, 2020
 *      Author: javad
 */

#ifndef INCLUDE_MODBUSSERVER_H_
#define INCLUDE_MODBUSSERVER_H_

#include <modbus-tcp.h>
#include <PLC.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

class ModbusServer
{
public:
	static ModbusServer * GetInstance();
	static pthread_mutex_t Inst_lock;

	void StartRunThread(string JSON);

	void WaitForRunThread(void);

	bool GetModbusStatus(void);

	modbus_mapping_t * DeviceMemory;

	int server_socket;
	int master_socket;
	modbus_t * TCPstation;

private:
	static ModbusServer * Instance;
	SiemensPLC * PLC;
	pthread_t RunThreadId;

	vector<int> Coils;
	vector<int> InputContacts;
	map<int, int> HoldingRegs;
	map<int, int> InputRegs;

	ModbusServer(void);
	static void * WaitForReqThread(void * inst);
	void Run(void);

	//int WaitForConnectRequest(void);
	int WaitForFunctionRequests(void);

	ModbusErrorCodes ReadCoils(uint8_t * tcp_query);
	ModbusErrorCodes ReadInputBits(uint8_t * tcp_query);
	ModbusErrorCodes ReadHoldingRegs(uint8_t * tcp_query);
	ModbusErrorCodes ReadInputRegs(uint8_t * tcp_query);
	ModbusErrorCodes WriteCoils(uint8_t * tcp_query);
	ModbusErrorCodes WriteRegisters(uint8_t * tcp_query);
	ModbusErrorCodes WriteSingleCoil(uint8_t * tcp_query);
	ModbusErrorCodes WriteSingleRegister(uint8_t * tcp_query);

	void SendReply(ModbusErrorCodes result, uint8_t * tcp_query, int queryLength);

	bool IsConnected;

};



#endif /* INCLUDE_MODBUSSERVER_H_ */
