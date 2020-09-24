/*
 * LED.h
 *
 *  Created on: May 13, 2020
 *      Author: LENOVO
 */

#ifndef INCLUDE_LED_H_
#define INCLUDE_LED_H_

#include <pthread.h>


/* GPIO indexes for LEDs*/
#define MODBUSCONNECTED 5
#define MODBUSNOTCONNECTED 1
#define SIEMENSCONNECTED 0
#define SIEMENSNOTCONNECTED 15
#define RUNLED 8
#define FAULTLED 9

class LED
{
private:
	LED(void);
	static LED * instance;

	static void * BlinkModbusLED(void * arg);
	static void *  BlinkSiemensLED(void * arg);

	pthread_t ModbusBlinkThreadId;
	pthread_t SiemensBlinkThreadId;
	

public:
	static pthread_mutex_t Inst_lock;
	static LED * GetInstance(void);
	void TurnOnRunLED(void);
	void TurnOnFaultLED(void);

	void WaitForThreads(void);



};


#endif /* INCLUDE_LED_H_ */
