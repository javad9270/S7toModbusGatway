/*
 * main.cpp
 *
 *  Created on: May 1, 2020
 *      Author: javad
 */


#include <ModbusServer.h>
#include <PLC.h>
#include <cstdlib>
#include <time.h>
#include <LED.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include "bcrypt/BCrypt.hpp"
#include <iostream>

#define MACHASH 

int mac_eth0(void)
{
  struct ifreq ifr;
  int s;
  char str[19];
  string a = string("$2y$12$0SOQuMlgyzprWKUGlkRrBOM4ehJGnsr87Hww4Hgo.KCmD93Z3W81q");
  if ((s = socket(AF_INET, SOCK_STREAM,0)) < 0) {
    perror("socket");
    return -1;
  }
  
  strcpy(ifr.ifr_name, "eth0");
  if (ioctl(s, SIOCGIFHWADDR, &ifr) < 0) {
    perror("ioctl");
    return -1;
  }
  
  unsigned char *hwaddr = (unsigned char *)ifr.ifr_hwaddr.sa_data;
  printf("%02X:%02X:%02X:%02X:%02X:%02X\n", hwaddr[0], hwaddr[1], hwaddr[2],
                                          hwaddr[3], hwaddr[4], hwaddr[5]);
  sprintf(str,"%02X:%02X:%02X:%02X:%02X:%02X", hwaddr[0], hwaddr[1], hwaddr[2],
                                          hwaddr[3], hwaddr[4], hwaddr[5]);

  str[18] = '\0';

  close(s);

  if (BCrypt::validatePassword(str,a) == 1)
  {
	return 0;
  }
  else
  {
  	return -1;
  }
  
}

int main(int argc, char * argv[])
{
	string JSONPath;
	vector<int> WriteIds;
	vector<vector<uint8_t>> WritedataList;
	vector<uint8_t> data;
	vector<int> results;

	if (argc > 1)
	{
		JSONPath = string(argv[1]);
	}
	else
	{
		printf("No path for JSON file is inserted, terminating program!\n");
		exit(1);
	}

	pthread_mutex_init(&(ModbusServer::Inst_lock), NULL);
	pthread_mutex_init(&(SiemensPLC::Inst_lock), NULL);
	pthread_mutex_init(&(LED::Inst_lock), NULL);

	SiemensPLC * PLC = SiemensPLC::GetInstance();
	ModbusServer * MBSlave = ModbusServer::GetInstance();
	LED * led = LED::GetInstance();
	if (mac_eth0() != 0)
    {
    	led->TurnOnFaultLED();
    	exit(2);
    }
	led->TurnOnRunLED();

	if(PLC->StartSiemens(JSONPath))
	{
		sleep(2);
		MBSlave->StartRunThread(JSONPath);

		MBSlave->WaitForRunThread();
		PLC->WaitForRunThreadToStop();
		led->WaitForThreads();
	}



	printf("Siemens run thread exited abnormally!\n");
	exit(1);

}

