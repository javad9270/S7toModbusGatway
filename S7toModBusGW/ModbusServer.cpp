/*
 * ModbusSlave.cpp
 *
 *  Created on: May 4, 2020
 *      Author: javad
 */

#include <math.h>
#include <ModbusServer.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_DATA_READ_LENGTH 500

using namespace rapidjson;
using namespace std;

ModbusServer * ModbusServer::Instance;
pthread_mutex_t ModbusServer::Inst_lock;

static void close_sigint(int dummy)
{
	ModbusServer * Inst = ModbusServer::GetInstance();
    close(Inst->server_socket);
    modbus_free(Inst->TCPstation);
}

ModbusServer * ModbusServer::GetInstance()
{
	pthread_mutex_lock(&Inst_lock);
	if (!Instance)
	{
		Instance = new ModbusServer();
	}
	pthread_mutex_unlock(&Inst_lock);
	return Instance;
}

ModbusServer::ModbusServer(void)
{
	RunThreadId = 0;
	PLC = SiemensPLC::GetInstance();
	DeviceMemory = NULL;
	server_socket = 0;
	TCPstation = NULL;
	IsConnected = false;
}

void ModbusServer::StartRunThread(string ConfigPath)
{
	ifstream ifs(ConfigPath.c_str());
	IStreamWrapper isw(ifs);
	Document document;

	int ResourceId = 0;

	int CurrentHoldingAddress = 0;
	int CurrentInpuutRegsAddress = 0;

	document.ParseStream(isw);

	InputContacts.push_back(SIEMENS_STATUS);


	for (vector<DataItem *>::iterator itr = PLC->ListOfDataItems.begin() ; itr != PLC->ListOfDataItems.end(); ++itr)
	{
		switch((*itr)->DetermineModbusRegType())
		{
			case S7COIL:
			{
				Coils.push_back(ResourceId);
				printf("Pushing resource %d [%d, %d] to Coils.\n", ResourceId, (*itr)->S7DataItemPtr->Area, (*itr)->S7DataItemPtr->Start);
				break;
			}
			case S7INCONT:
			{
				InputContacts.push_back(ResourceId);
				printf("Pushing resource %d [%d, %d] to Input Contacts.\n", ResourceId, (*itr)->S7DataItemPtr->Area, (*itr)->S7DataItemPtr->Start);
				break;
			}
			case S7HOLDREG:
			{
				HoldingRegs.insert({CurrentHoldingAddress, ResourceId});
				CurrentHoldingAddress += ceil((*itr)->size/2);
				printf("Pushing resource %d [%d, %d] to Holding Registers.\n", ResourceId, (*itr)->S7DataItemPtr->Area, (*itr)->S7DataItemPtr->Start);
				break;
			}
			case S7INPUTREG:
			{
				InputRegs.insert({CurrentInpuutRegsAddress, ResourceId});
				CurrentInpuutRegsAddress += ceil((*itr)->size/2);
				printf("Pushing resource %d [%d, %d] to Input Registers.\n", ResourceId, (*itr)->S7DataItemPtr->Area, (*itr)->S7DataItemPtr->Start);
				break;
			}
			default:
			{
				HoldingRegs.insert({CurrentHoldingAddress, ResourceId});
				CurrentHoldingAddress += ceil((*itr)->size/2);
				break;
			}
		}
		ResourceId++;
	}

	DeviceMemory = modbus_mapping_new(Coils.size(), InputContacts.size(), CurrentHoldingAddress, CurrentInpuutRegsAddress);

	pthread_create(&RunThreadId, NULL, ModbusServer::WaitForReqThread, this);
}

void ModbusServer::WaitForRunThread(void)
{
	pthread_join(RunThreadId, NULL);
}

void * ModbusServer::WaitForReqThread(void * Inst)
{
	ModbusServer * inst = (ModbusServer *)Inst;
	while(true)
	{
		inst->Run();
		printf("Run stopped!\n");
		sleep(1);
	}
	return NULL;
}

ModbusErrorCodes ModbusServer::ReadCoils(uint8_t * tcp_query)
{
	ModbusErrorCodes result = OK;
	uint16_t RequestAddress = tcp_query[8] * 0x100 + tcp_query[9];
	uint16_t itemCount 		= tcp_query[10] * 0x100 + tcp_query[11];
	vector<int> ResourceIds;
	uint8_t Readdata[MAX_DATA_READ_LENGTH];
	memset(Readdata, 0, MAX_DATA_READ_LENGTH);

	if ((uint16_t)(RequestAddress + itemCount - 1) < Coils.size())
	{
		ResourceIds.assign(Coils.begin() + RequestAddress, Coils.begin() + RequestAddress + itemCount);
		result = PLC->ReadBitsFromModbus(ResourceIds, itemCount, Readdata);
		if (result == OK)
		{
			printf("setting tab bits from S7 data item 0x%X!\n", *Readdata);
			modbus_set_bits_from_bytes(DeviceMemory->tab_bits, RequestAddress, itemCount, Readdata);
		}
		else
		{
			printf("Error reading from PLC dataItems!\n");
		}
	}
	else
	{
		result = InvalidModbusAddr;
	}

	return result;
}

ModbusErrorCodes ModbusServer::ReadInputBits(uint8_t * tcp_query)
{
	ModbusErrorCodes result = OK;
	uint16_t RequestAddress = tcp_query[8] * 0x100 + tcp_query[9];
	uint16_t itemCount 		= tcp_query[10] * 0x100 + tcp_query[11];
	vector<int> ResourceIds;
	uint8_t Readdata[MAX_DATA_READ_LENGTH];
	memset(Readdata, 0, MAX_DATA_READ_LENGTH);

	//printf("Reading input bits, item count = %d, request address = %d\n", itemCount, RequestAddress);

	if ((uint16_t)(RequestAddress + itemCount - 1) < InputContacts.size())
	{
		ResourceIds.assign(InputContacts.begin() + RequestAddress, InputContacts.begin() + RequestAddress + itemCount);
		result = PLC->ReadBitsFromModbus(ResourceIds, itemCount, Readdata);
		if (result == OK)
		{
			modbus_set_bits_from_bytes(DeviceMemory->tab_input_bits, RequestAddress, itemCount, Readdata);
			/*for (int i = 0; i < 20; i++)
			{
				printf("%d ", Readdata[i]);
			}
			printf("\n");*/
		}
		else
		{
			printf("Error reading from PLC dataItems!\n");
		}
	}
	else
	{
		result = InvalidModbusAddr;
	}

	return result;
}

ModbusErrorCodes ModbusServer::ReadHoldingRegs(uint8_t * tcp_query)
{
	ModbusErrorCodes result = OK;
	uint16_t RequestAddress = tcp_query[8] * 0x100 + tcp_query[9];
	uint16_t itemCount 		= tcp_query[10] * 0x100 + tcp_query[11];
	map<int,int>::iterator ResourceItr;
	vector<int> ResourceIds;
	uint8_t Readdata[MAX_DATA_READ_LENGTH];
	memset(Readdata, 0, MAX_DATA_READ_LENGTH);

	if (HoldingRegs.find(RequestAddress) != HoldingRegs.end())
	{
		ResourceItr = HoldingRegs.find(RequestAddress);
		for (uint16_t i = 0; i < itemCount; i++)
		{
			ResourceItr = HoldingRegs.find(RequestAddress + i);
			if (ResourceItr != HoldingRegs.end())
			{
				ResourceIds.push_back(ResourceItr->second);
			}
		}
		result = PLC->ReadBytesFromModbus(ResourceIds, itemCount, Readdata);
		if (result == OK)
		{
			/*for (uint16_t i = 0; i < itemCount * 2; i++)
			{
				printf("0x%X ", *((uint8_t *)Readdata + i));
			}
			printf("\n");*/
			memcpy((DeviceMemory->tab_registers + RequestAddress), Readdata, itemCount * 2);
		}
		else
		{
			printf("Error reading from PLC dataItems!\n");
		}
	}
	else
	{
		printf("Invalid holding register address 0x%X!\n", RequestAddress);

	}

	return result;
}

ModbusErrorCodes ModbusServer::ReadInputRegs(uint8_t * tcp_query)
{
	ModbusErrorCodes result = OK;
	uint16_t RequestAddress = tcp_query[8] * 0x100 + tcp_query[9];
	uint16_t itemCount 		= tcp_query[10] * 0x100 + tcp_query[11];
	map<int,int>::iterator ResourceItr;
	vector<int> ResourceIds;
	uint8_t Readdata[MAX_DATA_READ_LENGTH];
	memset(Readdata, 0, MAX_DATA_READ_LENGTH);

	if (InputRegs.find(RequestAddress) != InputRegs.end())
	{
		for (uint16_t i = 0; i < itemCount; i++)
		{
			ResourceItr = InputRegs.find(RequestAddress + i);
			if (ResourceItr != InputRegs.end())
			{
				ResourceIds.push_back(ResourceItr->second);
			}
		}
		result = PLC->ReadBytesFromModbus(ResourceIds, itemCount, Readdata);
		if (result == OK)
		{
			/*for (uint16_t i = 0; i < itemCount * 2; i++)
			{
				printf("0x%X ", *((uint8_t *)Readdata + i));
			}
			printf("\n");*/
			memcpy((DeviceMemory->tab_input_registers + RequestAddress), Readdata, itemCount * 2);
		}
		else
		{
			printf("Error reading from PLC dataItems!\n");
		}
	}
	else
	{
		result = InvalidModbusAddr;
	}

	return result;
}

ModbusErrorCodes ModbusServer::WriteCoils(uint8_t * tcp_query)
{
	ModbusErrorCodes result = OK;

	uint16_t RequestAddress = tcp_query[8] * 0x100 + tcp_query[9];
	uint16_t itemCount 		= tcp_query[10] * 0x100 + tcp_query[11];
	vector<int> ResourceIds;
	int ByteCount = 0;
	uint8_t Writedata[MAX_DATA_READ_LENGTH];

	if ((uint16_t)(RequestAddress + itemCount - 1) < Coils.size())
	{
		ResourceIds.assign(Coils.begin() + RequestAddress, Coils.begin() + RequestAddress + itemCount - 1);
		ByteCount = tcp_query[12];
		memcpy(Writedata, tcp_query + 13, ByteCount);
		result = PLC->WriteBitsFromModbus(ResourceIds, Writedata, itemCount);
	}
	else
	{
		result = InvalidModbusAddr;
	}

	return result;
}

ModbusErrorCodes ModbusServer::WriteRegisters(uint8_t * tcp_query)
{
	ModbusErrorCodes result = OK;
	uint16_t RequestAddress = tcp_query[8] * 0x100 + tcp_query[9];
	uint16_t itemCount 		= tcp_query[10] * 0x100 + tcp_query[11];
	map<int,int>::iterator ResourceItr;
	vector<int> ResourceIds;
	int ByteCount = 0;
	uint8_t Writedata[MAX_DATA_READ_LENGTH];

	if (HoldingRegs.find(RequestAddress) != HoldingRegs.end())
	{
		for (uint16_t i = 0; i < itemCount; i++)
		{
			ResourceItr = HoldingRegs.find(RequestAddress + i);
			if (ResourceItr != HoldingRegs.end())
			{
				ResourceIds.push_back(ResourceItr->second);
			}
		}
		ByteCount = tcp_query[12];
		memcpy(Writedata, tcp_query + 13, ByteCount);
		printf("Write to S7 Siemens!\n");
		result = PLC->WriteBytesFromModbus(ResourceIds, Writedata, itemCount);
		if (result != OK)
		{
			printf("writing to S7 Siemens failed (%d)!\n", result);
		}
	}
	else
	{
		printf("Request address 0x%X not found Holding registers!\n", RequestAddress);
		result = InvalidModbusAddr;
	}

	return result;
}

ModbusErrorCodes ModbusServer::WriteSingleCoil(uint8_t * tcp_query)
{
	ModbusErrorCodes result = OK;
	uint16_t RequestAddress = tcp_query[8] * 0x100 + tcp_query[9];
	uint8_t Writedata[MAX_DATA_READ_LENGTH];
	vector<int> ResourceIds;
	uint16_t Status;

	if (RequestAddress < Coils.size())
	{
		ResourceIds.assign(1, 0);
		ResourceIds[0] = Coils[RequestAddress];
		Status = tcp_query[10] * 0x100 + tcp_query[11];
		if (Status == 0x0)
		{
			Writedata[0] = 0;
		}
		else
		{
			Writedata[0] = 1;
		}
		result = PLC->WriteBitsFromModbus(ResourceIds, Writedata, 1);
	}
	else
	{
		result = InvalidModbusAddr;
	}

	return result;
}

ModbusErrorCodes ModbusServer::WriteSingleRegister(uint8_t * tcp_query)
{
	ModbusErrorCodes result = OK;
	uint16_t RequestAddress = tcp_query[8] * 0x100 + tcp_query[9];
	uint8_t Writedata[MAX_DATA_READ_LENGTH];
	vector<int> ResourceIds;

	if (HoldingRegs.find(RequestAddress) != HoldingRegs.end())
	{
		ResourceIds.assign(1, 0);
		ResourceIds[0] = HoldingRegs.find(RequestAddress)->second;
		memcpy(Writedata, tcp_query + 10, 2);
		result = PLC->WriteBytesFromModbus(ResourceIds, Writedata, 1);
	}
	else
	{
		printf("Request address 0x%X not found Holding registers!\n", RequestAddress);
		result = InvalidModbusAddr;
	}

	return result;
}

void ModbusServer::SendReply(ModbusErrorCodes result, uint8_t * tcp_query, int queryLength)
{
	switch(result)
	{
		case OK:
		{
			if (modbus_reply(TCPstation, tcp_query, queryLength, DeviceMemory) > 0)
			{
				//printf("Reply sent back to modbus server!\n");
			}
			else
			{
				printf("Reply is not generated, error %d!\n", errno);
			}
			break;
		}
		case DeviceIsOffLine:
		{
			modbus_reply_exception(TCPstation, tcp_query, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
			break;
		}
		case InvalidModbusAddr:
		case InvalidS7DataItem:
		{
			modbus_reply_exception(TCPstation, tcp_query, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
			break;
		}
		case InvalidSize:
		{
			modbus_reply_exception(TCPstation, tcp_query, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
			break;
		}
		case UnSoppotedMopdbusFC:
		{
			modbus_reply_exception(TCPstation, tcp_query, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
			break;
		}
		default:
		{
			modbus_reply_exception(TCPstation, tcp_query, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
			break;
		}
	}
}

int ModbusServer::WaitForFunctionRequests(void)
{
	int QueryLength = 0;

	uint8_t TCP_Query[MODBUS_TCP_MAX_ADU_LENGTH];

	unsigned char FunctionCode;

	ModbusErrorCodes result = OK;

	memset(TCP_Query, 0, MODBUS_TCP_MAX_ADU_LENGTH);


    modbus_set_socket(TCPstation, master_socket);
	QueryLength = modbus_receive(TCPstation , TCP_Query);
	if (QueryLength > 0)
	{
		FunctionCode 	= TCP_Query[7];

		switch(FunctionCode)
		{
			case MODBUS_FC_READ_COILS:
			{
				result = ReadCoils(TCP_Query);
				break;
			}
			case MODBUS_FC_READ_DISCRETE_INPUTS:
			{
				result = ReadInputBits(TCP_Query);
				break;
			}
			case MODBUS_FC_READ_HOLDING_REGISTERS:
			{
				result = ReadHoldingRegs(TCP_Query);
				break;
			}
			case MODBUS_FC_READ_INPUT_REGISTERS:
			{
				result = ReadInputRegs(TCP_Query);
				break;
			}
			case MODBUS_FC_WRITE_MULTIPLE_COILS:
			{
				result = WriteCoils(TCP_Query);
				break;
			}
			case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
			{
				printf("write request.............\n");
				result = WriteRegisters(TCP_Query);
				break;
			}
			case MODBUS_FC_WRITE_SINGLE_COIL:
			{
				result = WriteSingleCoil(TCP_Query);
				break;
			}
			case MODBUS_FC_WRITE_SINGLE_REGISTER:
			{
				printf("write request.............\n");
				result = WriteSingleRegister(TCP_Query);
				break;
			}
			default:
			{
				printf("Unsupported function code 0x%X!\n", FunctionCode);
				result = UnSoppotedMopdbusFC;
				break;
			}
		}
		SendReply(result, TCP_Query, QueryLength);
	}
	else
	{
		printf("Error in receive modbus! [%d]\n", QueryLength);
		close(server_socket);
		//modbus_tcp_accept(TCPstation , &server_socket);
	}
	return QueryLength;
}



	

void ModbusServer::Run(void)
{
    fd_set rdset;
    /* Maximum file descriptor number */
    int fdmax;
    int rc;
    fd_set refset;

	TCPstation = modbus_new_tcp(NULL , MODBUS_TCP_DEFAULT_PORT);

	printf("MODBUS:: waiting for client to connect!\n");
	modbus_flush(TCPstation) ;
	server_socket = modbus_tcp_listen( TCPstation , 1) ;

    //signal(SIGINT, close_sigint);

	/* Clear the reference set of socket */
    FD_ZERO(&refset);
    /* Add the server socket */
    FD_SET(server_socket, &refset);
 
    /* Keep track of the max file descriptor */
    fdmax = server_socket;
    printf("the server socket is %d............\n", fdmax);

    while(true)
    {
    	struct timeval tv = {20, 0};
    	rdset = refset;
    	//printf("waiting for requests!\n");
    	rc = select(fdmax + 1, &rdset, NULL, NULL, &tv);
	    if (rc == -1)
	    {
	        perror("Server select() failure.");   
    		close_sigint(1);
		    pthread_mutex_lock(&Inst_lock);
		    IsConnected = false;
		    pthread_mutex_unlock(&Inst_lock);
    		break; 
	    }
	    else if(rc == 0)
	    {
    		printf("timeout in receiving modbus request, terminating.....\n");
    		close_sigint(1);
		    pthread_mutex_lock(&Inst_lock);
		    IsConnected = false;
		    pthread_mutex_unlock(&Inst_lock);
    		break;
	    }
	    else
	    {
		    for (master_socket = 0; master_socket <= fdmax; master_socket++)
		    {

		        if (FD_ISSET(master_socket, &rdset))
		        {
		            if (master_socket == server_socket)
		            {
		                /* A client is asking a new connection */
		                socklen_t addrlen;
		                struct sockaddr_in clientaddr;
		                int newfd;

		                /* Handle new connections */
		                addrlen = sizeof(clientaddr);
		                memset(&clientaddr, 0, sizeof(clientaddr));
		                newfd = accept(server_socket, (struct sockaddr *)&clientaddr, &addrlen);
		                if (newfd == -1)
		                {
		                    perror("Server accept() error");
		                    server_socket = -1;
		                }
		                else
		                {
		                    FD_SET(newfd, &refset);
		                    if (newfd > fdmax)
		                    {
		                        /* Keep track of the maximum */
		                        fdmax = newfd;
		                    }
		                    printf("New connection from %s:%d on socket %d\n", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
		                    pthread_mutex_lock(&Inst_lock);
		                    IsConnected = true;
		                    pthread_mutex_unlock(&Inst_lock);
		                }
		            }
	    	        else
			        {
			        	if (WaitForFunctionRequests() <= 0)
						{
		                    pthread_mutex_lock(&Inst_lock);
		                    IsConnected = false;
		                    pthread_mutex_unlock(&Inst_lock);
						}
		        	}
		        }

	    	}
	    }

    }
}

bool ModbusServer::GetModbusStatus(void)
{
	bool out;
	pthread_mutex_lock(&Inst_lock);
	out = IsConnected;
	pthread_mutex_unlock(&Inst_lock);
	return out;
}
