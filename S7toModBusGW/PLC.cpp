/*
 * PLC.cpp
 *
 *  Created on: May 3, 2020
 *      Author: javad
 */


#include <PLC.h>
#include <unistd.h>
#include <error.h>

using namespace rapidjson;

pthread_mutex_t SiemensPLC::Inst_lock;
SiemensPLC * SiemensPLC::Instance;

SiemensPLC * SiemensPLC::GetInstance()
{
	pthread_mutex_lock(&Inst_lock);
	if (!Instance)
	{
		Instance = new SiemensPLC();
	}
	pthread_mutex_unlock(&Inst_lock);
	return Instance;
}

SiemensPLC::SiemensPLC(void)
{
	IPAddress = "";
	RackNum = 0;
	SlotNum = 0;
	Status = false;
	Client = NULL;
	RunThread_id = 0;
	pthread_mutex_init(&ReadWriteMutex, NULL);
}

bool SiemensPLC::StartSiemens(string ConfigPath)
{
	bool result = false;
	ifstream ifs(ConfigPath.c_str());
	IStreamWrapper isw(ifs);
	Document document;
	document.ParseStream(isw);
	Client = new TS7Client();


	if (document.IsObject())
	{
		try
		{
			IPAddress = document["IP"].GetString();
			RackNum = document["RackNum"].GetInt();
			SlotNum = document["SlotNum"].GetInt();
			for (const auto & Resource : document["DataItems"].GetArray())
			{
				AddDataItem(determineArea(Resource["Area"].GetString()), Resource["dbNum"].GetInt(), Resource["groupId"].GetInt(),
						Resource["Start"].GetInt(), Resource["BitIndex"].GetInt(), determineType(Resource["type"].GetString()), Resource["Access"].GetBool());
			}

			MakeDataItemGroups();
			result = true;
			/* starting Run Thread */
			pthread_create(&RunThread_id, NULL, SiemensPLC::StartRunThread, this);

		}
		catch (int e)
		{
			printf("Corrupt JSON file, restart application after fixing the JSON file (%d)!\n", e);
		}

	}
	else
	{
		printf("Corrupt JSON file, restart application after fixing the JSON file!\n");
	}

	return result;
}

bool SiemensPLC::GetStatus(void)
{
	return Status;
}

int SiemensPLC::WriteToResource(int id, vector<uint8_t> data)
{
	int result = 0;
	TS7DataItem * ptr;
	printf("writing to resource %d\n", id);
	if (Status)
	{
		if (ListOfDataItems[id]->IsDataItemWritable())
		{
			pthread_mutex_lock(&ReadWriteMutex);
			ptr = ListOfDataItems[id]->GetS7DataItemPtr();
			printf("writing to area %d, start %d and value %d\n", ptr->Area, ptr->Start,
					*(uint8_t *)(data.data()));
			result = Client->WriteArea(ptr->Area, ptr->DBNumber, ptr->Start, 1, ptr->WordLen, (void *)(data.data()));
			pthread_mutex_unlock(&ReadWriteMutex);
		}
		else
		{
			printf("resource %d is not writable!\n", id);
			result = 2;
		}

	}
	else
	{
		result = 1;
	}
	return result;
}

vector<int> SiemensPLC::WriteToMultipleResources(vector<int> ids, vector<vector<uint8_t>> dataList)
{
	vector<int> results;
	bool result = true;
	int WriteResult = 1;

	TS7DataItem * ptr = (TS7DataItem * )(malloc(ids.size() * sizeof(TS7DataItem)));
	TS7DataItem * InitPtr = ptr;

	if (Status)
	{
		pthread_mutex_lock(&ReadWriteMutex);
		for (uint8_t i = 0; i < ids.size(); i++)
		{
			if (ListOfDataItems[ids[i]]->IsDataItemWritable())
			{
				memcpy(ptr, ListOfDataItems[ids[i]]->GetS7DataItemPtr(), sizeof(TS7DataItem));
				memcpy(ptr->pdata, dataList[i].data(), dataList[i].size());
			}
			else
			{
				result = false;
				break;
			}
			ptr++;
		}
		if (result)
		{
			WriteResult = Client->WriteMultiVars(InitPtr, ids.size());
			if (WriteResult == 0)
			{
				for (uint8_t i = 0; i < ids.size(); i++)
				{
					results.push_back((InitPtr + i)->Result);
				}
			}
			else
			{
				results.assign(ids.size(), WriteResult);
			}
		}
		else
		{
			results.assign(ids.size(), WriteResult);
		}
		pthread_mutex_unlock(&ReadWriteMutex);
	}
	else
	{
		results.assign(ids.size(), WriteResult);
	}

	delete[] InitPtr;

	return results;
}

void SiemensPLC::AddDataItem(uint16_t area, uint16_t dbnum, uint8_t gid, uint16_t start, uint8_t bitIndex, SIEMENSDATATYPES Type, bool access)
{
	DataItem * dataItem = new DataItem(area, dbnum, gid, start, bitIndex, Type, access);
	ListOfDataItems.push_back(dataItem);
}

void SiemensPLC::MakeDataItemGroups(void)
{
	vector<DataItem *> tempList;
	for (vector<DataItem *>::iterator itr = ListOfDataItems.begin(); itr != ListOfDataItems.end(); ++itr)
	{
		if (ListOfDataItemGroups.find((*itr)->GroupId) == ListOfDataItemGroups.end())
		{
			ListOfDataItemGroups.insert({(*itr)->GroupId, tempList});
		}

		printf("add item [%d, %d] to group %d!\n", (*itr)->GetS7DataItemPtr()->Area, (*itr)->GetS7DataItemPtr()->Start, (*itr)->GroupId);

		ListOfDataItemGroups.find((*itr)->GroupId)->second.push_back((*itr));
	}
}

void * SiemensPLC::StartRunThread(void * Instance)
{
	SiemensPLC * inst = (SiemensPLC *)Instance;

	printf("starting Siemens thread .....\n");

	inst->RunThreadFunction();

	return NULL;
}

void SiemensPLC::RunThreadFunction(void)
{
	TS7DataItem * ptr;
	TS7DataItem * InitPtr;
	int GroupSize = 0;
	int result = 1;
	int Error_Count = 0;
	printf("Before connecting to PLC %s......\n", IPAddress.c_str());
	if (Client->ConnectTo(IPAddress.c_str(), RackNum, SlotNum) == 0)
	{
		printf("Connected to PLC!\n");
		pthread_mutex_lock(&ReadWriteMutex);
		Status = true;
		pthread_mutex_unlock(&ReadWriteMutex);
	}
	else
	{
		printf("Failed to connect to PLC %s\n", IPAddress.c_str());
	}

	printf("Start reading data items!\n");
	while(true)
	{
		if (Status)
		{
			pthread_mutex_lock(&ReadWriteMutex);
			for (map<int, vector <DataItem *>>::iterator itr = ListOfDataItemGroups.begin(); itr != ListOfDataItemGroups.end(); ++itr)
			{
				GroupSize = itr->second.size();
				//printf("Initializing pointer for %d dataItems!\n", GroupSize);
				ptr = (TS7DataItem *)(malloc(GroupSize * sizeof(TS7DataItem)));
				InitPtr = ptr;
				for (vector<DataItem *>::iterator itr2 = itr->second.begin(); itr2 != itr->second.end(); ++itr2)
				{
					//printf("Copying resource pointer to ptr %p!\n", ptr);
					memcpy(ptr, (*itr2)->GetS7DataItemPtr(), sizeof(TS7DataItem));
					//printf("pointer to data of this item is %p!\n", ptr->pdata);
					ptr++;
				}
				//printf("Reading multiple vars for group %d\n", itr->first);
				result = Client->ReadMultiVars(InitPtr, GroupSize);
				if (result != 0)
				{
					printf("error in reading multiple vars %d!\n", Error_Count);
					Error_Count++;
				}
				else
				{
					ptr = InitPtr;
					for (vector<DataItem *>::iterator itr2 = itr->second.begin(); itr2 != itr->second.end(); ++itr2)
					{
						if(ptr->Result == 0)
						{
							(*itr2)->SetStatus(true);
							if ((*itr2)->CheckDataChange(ptr->pdata))
							{
								printf("data change from Siemens for [%d, %d]: ", ptr->Area, ptr->Start);
								for (uint8_t i = 0; i < (*itr2)->size; i++)
								{
									printf("0x%X ", *((uint8_t *)(ptr->pdata) + i));
								}
								printf("\n");
							}
						}
						else
						{
							(*itr2)->SetStatus(false);
							printf("Error in reading from Area %d, Start %d, error = 0x%X\n", ptr->Area, ptr->Start, ptr->Result);

						}
						ptr++;
					}
				}
				delete[] InitPtr;
				if (Error_Count >= 3)
				{
					break;
				}
			}
			if (Error_Count >= 3)
			{
				printf("Error in reading for 3 times, changing PLC status to OFF!\n");
				Status = false;
				Error_Count = 0;
				SetDataItemStatus(false);
				Client->Disconnect();
			}
			pthread_mutex_unlock(&ReadWriteMutex);
			usleep(100000);
		}
		else
		{
			pthread_mutex_lock(&ReadWriteMutex);
			TryToReconnect();
			pthread_mutex_unlock(&ReadWriteMutex);
			sleep(1);
		}
	}
}

void SiemensPLC::WaitForRunThreadToStop(void)
{
	pthread_join(RunThread_id, NULL);
}

void SiemensPLC::TryToReconnect(void)
{
	if (ping(IPAddress) == 0)
	{
		printf("PLC %s is Alive!\n", IPAddress.c_str());
		printf("Trying to connect to PLC:\n");
		if (Client->ConnectTo(IPAddress.c_str(), RackNum, SlotNum) == 0)
		{
			printf("Successfully connected to PLC!\n");
			Status = true;
		}
		else
		{
			printf("Could not connect!\n");
		}
	}
	else
	{
		printf("PLC %s is dead!\n", IPAddress.c_str());
	}
}

SIEMENSDATATYPES SiemensPLC::determineType(string type)
{
	SIEMENSDATATYPES stype = INT;

	if (type == "Bool")
	{
		stype = BOOL;
	}
	else if (type == "Unsigned char")
	{
		stype = BYTE;
	}
	else if (type == "Int16")
	{
		stype = INT;
	}
	else if(type == "Int")
	{
		stype = D_INT;
	}
	else if(type == "Float")
	{
		stype = REAL;
	}
	else if(type == "timeofDay")
	{
		stype = D_INT;
	}
	else if(type == "counter")
	{
		stype = COUNTER;
	}
	else if(type == "time")
	{
		stype = TIME;
	}
	else if(type == "char")
	{
		stype = CHAR;
	}
	else
	{
		printf("Invalid type %s, considering integer!\n", type.c_str());
		printf("This may cause problem in data item read, please fix the configuration JSON!\n");
	}

	return stype;
}

uint16_t SiemensPLC::determineArea(string area)
{
	uint16_t SArea = S7AreaDB;

	if (area == "MB")
	{
		SArea = S7AreaMK;
	}
	else if (area == "DB")
	{
		SArea = S7AreaDB;
	}
	else if (area == "QB")
	{
		SArea = S7AreaPA;
	}
	else if (area == "IN")
	{
		SArea = S7AreaPE;
	}
	else if (area == "CT")
	{
		SArea = S7AreaCT;
	}
	else if(area == "TM")
	{
		SArea = S7AreaTM;
	}
	else
	{
		printf("Invalid area in JSON file, considering %s ad DB!\n", area.c_str());
		printf("This may cause some problems, please check your configuration JSON file!\n");
	}
	return SArea;
}

int SiemensPLC::ping(string IP)
{
	int result = 0;

	char buffer[512];

	string command = "ping -c 4 " + IP + " 2>&1";
	FILE * f = popen(command.c_str(), "r");

	if (f != NULL)
	{
		fscanf(f, "%s", buffer);
		if (strstr(buffer, "error"))
		{
			result = -1;
		}
		pclose(f);
	}
	else
	{
		result = -1;
	}



	return result;
}

void SiemensPLC::SetDataItemStatus(bool status)
{

	for (vector<DataItem *>::iterator itr = ListOfDataItems.begin(); itr != ListOfDataItems.end(); ++itr)
	{
		(*itr)->SetStatus(status, true);
	}
}

ModbusErrorCodes SiemensPLC::WriteBitsFromModbus(vector<int> ResourceIds, uint8_t * data, uint16_t itemCount)
{
	ModbusErrorCodes result = OK;
	vector<uint8_t> payload;
	int tempResult = 0;
	vector<vector<uint8_t>> dataList;
	uint8_t CurrentBit = 0;
	uint8_t ByteCounter = 0;
	vector<uint8_t> tempData;
	vector<int> results;


	if (Status)
	{
		payload.assign(itemCount, 0);
		if (ResourceIds.size() == 1)
		{
			*(payload.data()) = *data & 0x01;
			tempResult = WriteToResource(ResourceIds[0], payload);
			if ( tempResult == 0)
			{
				result = OK;
			}
			else
			{
				if (tempResult == 1)
				{
					result = DeviceIsOffLine;
				}
				else
				{
					result = InvalidS7DataItem;
				}
			}
		}
		else
		{
			for (uint16_t i = 0; i < itemCount; i++)
			{
				CurrentBit = CurrentBit << (i%8);
				data[ByteCounter]  = data[ByteCounter] & CurrentBit;
				tempData.push_back(data[ByteCounter]);
				dataList.push_back(tempData);
				tempData.clear();
				if (i%8 == 0)
				{
					if (i != 0)
					{
						ByteCounter++;
					}
				}
			}
			results = WriteToMultipleResources(ResourceIds, dataList);
			for (uint8_t i = 0; i < results.size(); i++)
			{
				if (results[i] != 0)
				{
					if (results[i] == 1)
					{
						result = DeviceIsOffLine;
					}
					else
					{
						result = InvalidS7DataItem;
					}
					break;
				}
			}
		}
	}
	else
	{
		result = DeviceIsOffLine;
	}

	return result;
}

ModbusErrorCodes SiemensPLC::WriteBytesFromModbus(vector<int> ResourceIds, uint8_t * data, uint16_t itemCount)
{
	ModbusErrorCodes result = OK;

	int ActualSize = 0;
	vector<uint8_t> payload;
	vector<vector<uint8_t>> dataList;
	vector<int> results;
	int tempResult = 0;
	int pos = 0;
	uint16_t byteCount = 2 * itemCount;

	vector<uint8_t> tempdata;

	if (Status)
	{
		payload.assign(byteCount, 0);
		memcpy(payload.data(), data, byteCount);

		for (vector<int>::iterator itr = ResourceIds.begin(); itr != ResourceIds.end(); ++itr)
		{
			ActualSize += ListOfDataItems[*itr]->size;
		}
		printf("Actual size %d, bytecount %d\n", ActualSize, byteCount);

		if (ActualSize == byteCount)
		{
			if (ResourceIds.size() == 1)
			{
				tempResult = WriteToResource(ResourceIds[0], ConvertForWrite(payload));
				if ( tempResult == 0)
				{
					result = OK;
				}
				else
				{
					if (tempResult == 1)
					{
						result = DeviceIsOffLine;
					}
					else
					{
						result = InvalidS7DataItem;
					}
				}
			}
			else
			{
				for (vector<int>::iterator itr = ResourceIds.begin(); itr != ResourceIds.end(); ++itr)
				{
					tempdata.assign(payload.begin() + pos, payload.begin() + pos + ListOfDataItems[*itr]->size);
					pos += ListOfDataItems[*itr]->size;
					dataList.push_back(ConvertForWrite(tempdata));
					tempdata.clear();
				}
				results = WriteToMultipleResources(ResourceIds, dataList);

				for (uint8_t i = 0; i < results.size(); i++)
				{
					if (results[i] != 0)
					{
						if (results[i] == 1)
						{
							result = DeviceIsOffLine;
						}
						else
						{
							result = InvalidS7DataItem;
						}
						break;
					}
				}
			}
		}
		else
		{
			result = InvalidSize;
		}
	}
	else
	{
		result = DeviceIsOffLine;
	}

	return result;
}

ModbusErrorCodes SiemensPLC::ReadBitsFromModbus(vector<int> ResourceIds, uint16_t bitCount, uint8_t * data)
{
	ModbusErrorCodes result = OK;

	int bitCounter = 0;

	for (vector<int>::iterator itr = ResourceIds.begin(); itr != ResourceIds.end(); ++itr)
	{
		if ( *itr != SIEMENS_STATUS)
		{
			if (ListOfDataItems[*itr]->GetStatus())
			{
				//printf("Actual data is 0x%X!\n", (*(uint8_t *)ListOfDataItems[*itr]->GetData()));
				data[(bitCounter/8)] += ((*(uint8_t *)ListOfDataItems[*itr]->GetData()) << (bitCounter % 8));
			}
			else
			{
				result = InvalidS7DataItem;
				break;
			}
		}
		else
		{
			data[(bitCounter/8)] += (((uint8_t)Status) << (bitCounter % 8));
		}
		bitCounter++;
	}

	return result;
}

ModbusErrorCodes SiemensPLC::ReadBytesFromModbus(vector<int> ResourceIds, uint16_t RegCount, uint8_t * data)
{
	ModbusErrorCodes result = OK;

	int Resource_byteCount = 0;
	uint8_t * ptr = data;


	uint8_t byteCount = RegCount * 2; /* number of bytes is two times the number of registers*/

	//printf("read %d bytes from PLC data items for Modbus!\n", byteCount);

	for (vector<int>::iterator itr = ResourceIds.begin(); itr != ResourceIds.end(); ++itr)
	{
		//printf("resource Id is %d\n", *itr);
		Resource_byteCount += ListOfDataItems[*itr]->size;
	}

	if (Resource_byteCount == byteCount)
	{
		if (Status)
		{
			for (vector<int>::iterator itr = ResourceIds.begin(); itr != ResourceIds.end(); ++itr)
			{
				if (ListOfDataItems[*itr]->GetStatus())
				{
					memcpy(ptr, ListOfDataItems[*itr]->GetData(), ListOfDataItems[*itr]->size);
					if (ListOfDataItems[*itr]->size == 1)
					{
						ptr += 2;
					}
					else
					{
						ptr += ListOfDataItems[*itr]->size;
					}
				}
				else
				{
					result = InvalidS7DataItem;
					break;
				}
			}
		}
		else
		{
			result = DeviceIsOffLine;
		}
	}
	else
	{
		printf("Invalid register size for data items of PLC!\n");
		result = InvalidSize;
	}
	return result;
}

vector<uint8_t> SiemensPLC::ConvertForWrite(vector<uint8_t> data)
{
	vector<uint8_t> out;
	int16_t twoBytes;
	int32_t fourBytes;

	switch(data.size())
	{
		case 2:
		{
			twoBytes = htole16(*((int16_t *)data.data()));
			out.assign(2, 0);
			memcpy(out.data(), &twoBytes, 2);
			break;
		}
		case 4:
		{
			fourBytes = htole32(*((int32_t *)data.data()));
			out.assign(4, 0);
			memcpy(out.data(), &fourBytes, 4);
			break;
		}
		default:
		{
			out = data;
			break;
		}
	}
	return out;
}
