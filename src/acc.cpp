/**
 * @file acc.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief 3-axis accelerometer functions
 * @version 0.1
 * @date 2020-07-24
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "main.h"

/** The LIS3DH sensor */
LIS3DH accSensor(I2C_MODE, 0x18);

/** Required for give semaphore from ISR */
BaseType_t xHigherPriorityTaskWoken = pdFALSE;


/**
 * @brief Initialize LIS3DH 3-axis 
 * acceleration sensor
 * 
 * @return true If sensor was found and is initialized
 * @return false If sensor initialization failed
 */
bool initACC(void)
{

	accSensor.settings.accelSampleRate = 25; //Hz.  Can be: 0,1,10,25,50,100,200,400,1600,5000 Hz
	accSensor.settings.accelRange = 2;		 //Max G force readable.  Can be: 2, 4, 8, 16

	accSensor.settings.adcEnabled = 0;
	accSensor.settings.tempEnabled = 0;
	accSensor.settings.xAccelEnabled = 1;
	accSensor.settings.yAccelEnabled = 1;
	accSensor.settings.zAccelEnabled = 1;

	if (accSensor.begin() != 0)
	{
		return false;
	}

	uint8_t dataToWrite = 0;
	dataToWrite |= 0x20;								   //Z high
	dataToWrite |= 0x08;								   //Y high
	dataToWrite |= 0x02;								   //X high
	accSensor.writeRegister(LIS3DH_INT1_CFG, dataToWrite); // Enable interrupts on high tresholds for x, y and z

	//LIS3DH_INT1_THS   
	dataToWrite = 0;
	dataToWrite |= 0x20; // 0x20 = 512mg @ 2G range
	accSensor.writeRegister(LIS3DH_INT1_THS, dataToWrite);
	delay(100);
	
	//LIS3DH_INT1_DURATION  
	dataToWrite = 0;
	dataToWrite |= 0x08;
	accSensor.writeRegister(LIS3DH_INT1_DURATION, dataToWrite);
	delay(100);

	//LIS3DH_INT2_CFG   
	dataToWrite = 0;
	dataToWrite |= 0x10;//Z low
	accSensor.writeRegister(0x34, dataToWrite);
	delay(100);

	//LIS3DH_INT2_THS   
	dataToWrite = 0;
	dataToWrite |= 0x10; // 0x10 = 1/8 range
	accSensor.writeRegister(0x36, dataToWrite);
	delay(100);
	
	//LIS3DH_INT2_DURATION  
	dataToWrite = 0;
	dataToWrite |= 0x08;
	accSensor.writeRegister(0x37, dataToWrite);
	delay(100);
	
	//LIS3DH_CTRL_REG0
	// Turn off pullup resistor to save power
	dataToWrite = 0;
	dataToWrite |= 0x90;
	accSensor.writeRegister(0x1E, dataToWrite);
	delay(100);
	
	//LIS3DH_CTRL_REG1
	// Low Power Mode
	accSensor.readRegister(&dataToWrite, LIS3DH_CTRL_REG1);
	dataToWrite |= 0x08; // Set Low Power Mode
	accSensor.writeRegister(LIS3DH_CTRL_REG1, dataToWrite);
	delay(100);

	//LIS3DH_CTRL_REG3
	//Choose source for pin 1
	dataToWrite = 0;
	dataToWrite |= 0x40; //AOI1 event (Generator 1 interrupt on pin 1)
	accSensor.writeRegister(LIS3DH_CTRL_REG3, dataToWrite);
	delay(100);

	// No interrupt on pin 2
    accSensor.writeRegister(LIS3DH_CTRL_REG6, 0x00);

    // Enable high pass filter
    accSensor.writeRegister(LIS3DH_CTRL_REG2, 0x01);
	
	//LIS3DH_CTRL_REG5
	accSensor.readRegister(&dataToWrite, LIS3DH_CTRL_REG5);
	dataToWrite = 0;
	dataToWrite &= 0x78; //Clear bits of interest
	accSensor.writeRegister(LIS3DH_CTRL_REG5, dataToWrite);
	delay(100);

	//LIS3DH_CTRL_REG6
	dataToWrite = 0;
	dataToWrite |= 0x20; // I2_IA2 -- works
	accSensor.writeRegister(LIS3DH_CTRL_REG6, dataToWrite);
	delay(100);

	clearAccInt();

	// Mini Base Slot D IO = WB_IO5
	pinMode(WB_IO5, INPUT);
	delay(100);
	attachInterrupt(WB_IO5, accIntHandler, CHANGE);
	delay(100);

	pinMode(WB_IO6, INPUT);
	delay(100);
	attachInterrupt(WB_IO6, accIntHandler, CHANGE);
	delay(100);

	return true;
}

/**
 * @brief ACC interrupt handler
 * @note gives semaphore to wake up main loop
 * 
 */
void accIntHandler(void)
{	
	detachInterrupt(digitalPinToInterrupt(WB_IO5));
	detachInterrupt(digitalPinToInterrupt(WB_IO6));
	myLog_d("Sberla!");
	eventType = 2;
	xSemaphoreGiveFromISR(taskEvent, &xHigherPriorityTaskWoken);
}

/**
 * @brief Clear ACC interrupt register to enable next wakeup
 * 
 */
void clearAccInt(void)
{
	uint8_t dataRead;
	accSensor.readRegister(&dataRead, LIS3DH_INT1_SRC);
	if (dataRead & 0x40)
		myLog_d("Interrupt Active 0x%X\n", dataRead);
	if (dataRead & 0x20)
		myLog_d("Z high");
	if (dataRead & 0x10)
		myLog_d("Z low");
	if (dataRead & 0x08)
		myLog_d("Y high");
	if (dataRead & 0x04)
		myLog_d("Y low");
	if (dataRead & 0x02)
		myLog_d("X high");
	if (dataRead & 0x01)
		myLog_d("X low");
}

//Calculate tilt along axes
void calculateTilt(float xacc, float yacc, float zacc, uint8_t * xinc, uint8_t * yinc, uint8_t * zinc){
	*xinc = (180/PI)*atan2( xacc, sqrt( pow(yacc,2) + pow(zacc,2) ) );
	*yinc = (180/PI)*atan2( yacc, sqrt( pow(xacc,2) + pow(zacc,2) ) );
	*zinc = (180/PI)*atan2( sqrt( pow(xacc,2) + pow(yacc,2) ), zacc );
}
