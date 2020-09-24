/*
 * PLC.h
 *
 *  Created on: May 2, 2020
 *      Author: javad
 */

#ifndef INCLUDE_PLC_H_
#define INCLUDE_PLC_H_

#include <dataItem.h>
#include <stdio.h>
#include <iostream>
#include <writer.h>
#include <document.h>
#include "stringbuffer.h"
#include <istreamwrapper.h>
#include <fstream>
#include <map>
#include <pthread.h>

#define SIEMENS_STATUS -1

using namespace std;

enum ModbusErrorCodes
{
	OK,
	InvalidSize,
	DeviceIsOffLine,
	InvalidS7DataItem,
	InvalidModbusAddr,
	UnSoppotedMopdbusFC
};

class SiemensPLC
{
public:
	static SiemensPLC * GetInstance(void);
	static pthread_mutex_t Inst_lock;
	bool StartSiemens(string ConfigPath);
	bool GetStatus(void);
	void WaitForRunThreadToStop(void);

	ModbusErrorCodes WriteBitsFromModbus(vector<int> ResourceIds, uint8_t * data, uint16_t bitCount);
	ModbusErrorCodes WriteBytesFromModbus(vector<int> ResourceIds, uint8_t * data, uint16_t byteCount);
	ModbusErrorCodes ReadBitsFromModbus(vector<int> ResourceIds, uint16_t bitCount, uint8_t * data);
	ModbusErrorCodes ReadBytesFromModbus(vector<int> ResourceIds, uint16_t RegCount, uint8_t * data);

	vector<DataItem * > ListOfDataItems;

private:
	SiemensPLC(void);
	static SiemensPLC * Instance;
	string IPAddress;
	int RackNum;
	int SlotNum;
	bool Status;

	pthread_mutex_t ReadWriteMutex;

	TS7Client * Client;

	pthread_t RunThread_id;

	map<int, vector<DataItem *>> ListOfDataItemGroups;

	void AddDataItem(uint16_t area, uint16_t dbnum, uint8_t gid, uint16_t start, uint8_t bitIndex, SIEMENSDATATYPES Type, bool access);
	void MakeDataItemGroups(void);
	void TryToReconnect(void);
	int ping(string IP);
	void SetDataItemStatus(bool status);

	int WriteToResource(int id, vector<uint8_t> data);
	vector<int> WriteToMultipleResources(vector<int> ids, vector<vector<uint8_t>> dataList);

	static void * StartRunThread(void * inst);
	void RunThreadFunction(void);

	SIEMENSDATATYPES determineType(string type);
	uint16_t determineArea(string area);

	vector<uint8_t> ConvertForWrite(vector<uint8_t> data);

};



#endif /* INCLUDE_PLC_H_ */
