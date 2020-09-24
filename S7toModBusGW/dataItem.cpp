/*
 * dataItem.cpp
 *
 *  Created on: May 3, 2020
 *      Author: javad
 */

#include <dataItem.h>
#include <string.h>


DataItem::DataItem(uint16_t area, uint16_t dbnum, uint8_t gid, uint16_t start, uint8_t bitIndex, SIEMENSDATATYPES Type, bool access)
{
	S7DataItemPtr = new TS7DataItem();
	S7DataItemPtr->Area = area;
	S7DataItemPtr->DBNumber = dbnum;
	S7DataItemPtr->Amount = 1;
	S7DataItemPtr->Start = start;

	datatype = Type;
	ReadWriteAccess = access;
	GroupId = gid;

	switch(Type)
	{
		case BOOL:
		{
			S7DataItemPtr->Start = 8 * start + bitIndex;
			S7DataItemPtr->WordLen = S7WLBit;
			size = 1;
			break;
		}
		case BYTE:
		case CHAR:
		{
			S7DataItemPtr->WordLen = S7WLByte;
			size = 1;
			break;
		}
		case INT:
		{
			S7DataItemPtr->WordLen  = S7WLWord;
			size = 2;
			break;
		}
		case D_INT:
		{
			S7DataItemPtr->WordLen  = S7WLDWord;
			size = 4;
			break;
		}
		case REAL:
		{
			S7DataItemPtr->WordLen = S7WLReal;
			size = 4;
			break;
		}
		case COUNTER:
		{
			S7DataItemPtr->WordLen = S7WLCounter;
			size = 2;
			break;
		}
		case TIME:
		{
			S7DataItemPtr->WordLen = S7WLTimer;
			size = 2;
			break;
		}
		default:
		{
			printf("invalid type, check your JSON file!\n");
			printf("Considering the type Int!\n");
			S7DataItemPtr->WordLen  = S7WLWord;
			size = 2;
			break;
		}
	}

	S7DataItemPtr->pdata = malloc(size);
	localCopy = malloc(size);
	memset(localCopy, 0, size);

	TransformedCopy = malloc(size);
	memset(TransformedCopy, 0, size);

	Status = false;
	localStatusCopy = false;
}

bool DataItem::GetStatus(void)
{
	return Status;
}

void DataItem::SetStatus(bool status, bool setlocalcopy)
{
	Status = status;
	if (setlocalcopy)
	{
		localStatusCopy = status;
	}
}

void * DataItem::GetData(void)
{
	uint16_t IntValue;
	uint32_t RealValue;
	switch(size)
	{
	case 2:
	{
		IntValue = htobe16(*(uint16_t *)(localCopy));
		memcpy(TransformedCopy, &IntValue, 2);
		break;
	}
	case 4:
	{
		RealValue = htobe32(*(uint32_t *)(localCopy));
		memcpy(TransformedCopy, &RealValue, 4);
		break;
	}
	default:
	{
		memcpy(TransformedCopy, localCopy, size);
		break;
	}
	}
	return TransformedCopy;
}

TS7DataItem * DataItem::GetS7DataItemPtr(void)
{
	return S7DataItemPtr;
}
bool DataItem::IsDataItemWritable(void)
{
	return ReadWriteAccess;
}

bool DataItem::CheckDataChange(void * data)
{
	bool result = false;
	if (Status)
	{
		if (localStatusCopy)
		{
			if (memcmp(localCopy, data, size) != 0)
			{
				result = true;
			}
		}
		else
		{
			result = true;
		}
		memcpy(localCopy, data, size);
	}
	localStatusCopy = Status;
	return result;
}

ModBusRegisterType DataItem::DetermineModbusRegType(void)
{
	ModBusRegisterType out = S7COIL;
	if (datatype == BOOL)
	{
		if (ReadWriteAccess)
		{
			out = S7COIL;
		}
		else
		{
			out = S7INCONT;
		}
	}
	else
	{
		if (ReadWriteAccess)
		{
			out = S7HOLDREG;
		}
		else
		{
			out = S7INPUTREG;
		}
	}
	return out;
}
