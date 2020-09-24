/*
 * Resource.h
 *
 *  Created on: May 2, 2020
 *      Author: javad
 */

#ifndef INCLUDE_DATAITEM_H_
#define INCLUDE_DATAITEM_H_


#include <snap7.h>
#include <vector>
#include <string>

using namespace std;

enum SIEMENSDATATYPES
{
	BOOL,
	BYTE,
	WORD,
	DWORD,
	INT,
	D_INT,
	REAL,
	COUNTER,
	TIME,
	CHAR
};

enum ModBusRegisterType
{
	S7COIL,
	S7INCONT,
	S7HOLDREG,
	S7INPUTREG
};


class DataItem
{
public:
	DataItem(uint16_t area, uint16_t dbnum, uint8_t gid, uint16_t start, uint8_t bit_index, SIEMENSDATATYPES Type, bool access);

	uint8_t GroupId;
	int size;

	bool GetStatus(void);
	void SetStatus(bool status, bool setlocalcopy = false);
	void * GetData(void);

	TS7DataItem * GetS7DataItemPtr(void);
	bool IsDataItemWritable(void);
	bool CheckDataChange(void * data);

	ModBusRegisterType DetermineModbusRegType(void);
	TS7DataItem * S7DataItemPtr;

private:
	bool Status;
	bool localStatusCopy;
	void * localCopy;
	void * TransformedCopy;
	
	SIEMENSDATATYPES datatype;
	bool ReadWriteAccess; /* false for read-only, true for write access*/

};


#endif /* INCLUDE_DATAITEM_H_ */
