/*
 * LED.cpp
 *
 *  Created on: May 13, 2020
 *      Author: LENOVO
 */



#include <LED.h>
#include <PLC.h>
#include <ModbusServer.h>

#include <wiringPi.h>

pthread_mutex_t LED::Inst_lock;
LED * LED::instance;

LED::LED(void)
{
	ModbusBlinkThreadId = 0;
	SiemensBlinkThreadId = 0;

	wiringPiSetup();
	pinMode(MODBUSCONNECTED, OUTPUT);
	pinMode(MODBUSNOTCONNECTED, OUTPUT);
	pinMode(SIEMENSCONNECTED, OUTPUT);
	pinMode(SIEMENSNOTCONNECTED, OUTPUT);
	pinMode(RUNLED, OUTPUT);
	pinMode(FAULTLED, OUTPUT);

	sleep(1);

	pthread_create(&ModbusBlinkThreadId, NULL, LED::BlinkModbusLED, NULL);
	pthread_create(&SiemensBlinkThreadId, NULL, LED::BlinkSiemensLED, NULL);
}

LED * LED::GetInstance(void)
{
	pthread_mutex_lock(&Inst_lock);
	if (!instance)
	{
		instance = new LED();
	}
	pthread_mutex_unlock(&Inst_lock);
	return instance;
}

void * LED::BlinkModbusLED(void * arg)
{
	ModbusServer * MB = ModbusServer::GetInstance();
	while(true)
	{
		if (MB->GetModbusStatus())
		{
			/* blinking green */
			digitalWrite(MODBUSCONNECTED, HIGH);
			usleep(300000);
			digitalWrite(MODBUSCONNECTED, LOW);
		}
		else
		{
			/* blinking red */
			digitalWrite(MODBUSNOTCONNECTED, HIGH);
			usleep(300000);
			digitalWrite(MODBUSNOTCONNECTED, LOW);
		}
		usleep(300000);
	}
	return NULL;
}

void * LED::BlinkSiemensLED(void * arg)
{
	SiemensPLC * plc = SiemensPLC::GetInstance();

	while(true)
	{
		if (plc->GetStatus())
		{
			/* blinking green */
			digitalWrite(SIEMENSCONNECTED, HIGH);
			usleep(300000);
			digitalWrite(SIEMENSCONNECTED, LOW);
		}
		else
		{
			/* blinking red */
			digitalWrite(SIEMENSNOTCONNECTED, HIGH);
			usleep(300000);
			digitalWrite(SIEMENSNOTCONNECTED, LOW);
		}
		usleep(300000);
	}
	return NULL;
}

void LED::TurnOnRunLED(void)
{
	digitalWrite(FAULTLED, LOW);
	digitalWrite(RUNLED, HIGH);
}

void LED::TurnOnFaultLED(void)
{
	digitalWrite(RUNLED, LOW);
	digitalWrite(FAULTLED, HIGH);
}

void LED::WaitForThreads(void)
{
	pthread_join(ModbusBlinkThreadId, NULL);
	pthread_join(SiemensBlinkThreadId, NULL);
}

