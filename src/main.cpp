#include "main.h"


/** Semaphore used by events to wake up loop task */
SemaphoreHandle_t taskEvent = NULL;
/** Timer to wakeup task frequently and send message */
SoftwareTimer taskWakeupTimer;

TxdPayload txPayload;
uint16_t nodeSentPackets = 0;
uint32_t wakeCounter = 0;
//A0 Short, A1 Short : 0x18
//A0 Open,  A1 Short : 0x19
//A0 Short, A1 Open  : 0x1A
//A0 Open,  A1 Open  : 0x1B


/**
 * @brief Flag for the event type
 * -1 => no event
 * 0 => LoRaWan data received
 * 1 => Timer wakeup
 * 2 => tbd
 * ...
 */
uint8_t eventType = -1;

/**
 * @brief Timer event that wakes up the loop task frequently
 * 
 * @param unused 
 */
void periodicWakeup(TimerHandle_t unused)
{
	// Switch on blue LED to show we are awake
	#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO
		digitalWrite(LED_CONN, HIGH);
	#endif
	eventType = 1;
	// Give the semaphore, so the loop task will wake up
	xSemaphoreGiveFromISR(taskEvent, pdFALSE);
}

void setup()
{	
	delay(5000);	
	
	txPayload.id = NODEID; //set the node id

	// Setup the build in LED
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(LED_CONN, OUTPUT);
	digitalWrite(LED_CONN, LOW);

	#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_NONE
		digitalWrite(LED_BUILTIN, HIGH);
		// Start serial
		Serial.begin(115200);

		// Wait seconds for a terminal to connect
		time_t timeout = millis();
		while (!Serial)
		{
			//Blink led while serial connects
			if ((millis() - timeout) < 2000) 
			{
				delay(200);
				digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
			}
			else
			{
				break;
			}
		}
		myLog_d("====================================");
		myLog_d("LoRa P2P deep sleep implementation");
		myLog_d("====================================");
	#endif

	// Switch off LED
	digitalWrite(LED_BUILTIN, LOW);

	// Create the semaphore for the loop task
	myLog_d("Create task semaphore");
	delay(100); // Give Serial time to send
	taskEvent = xSemaphoreCreateBinary();

	// Give the semaphore, seems to be required to initialize it
	myLog_d("Initialize task Semaphore");
	delay(100); // Give Serial time to send
	xSemaphoreGive(taskEvent);

	// Take the semaphore, so loop will be stopped waiting to get it
	myLog_d("Take task Semaphore - loop task stopped");
	delay(100); // Give Serial time to send
	xSemaphoreTake(taskEvent, 10);

	// Start LoRa
	if (!initLoRa())
	{
		myLog_e("Init LoRa failed");
		while (1)
		{
			digitalWrite(LED_CONN,1);
			delay(50);
			digitalWrite(LED_CONN,0);
			delay(300);
		}
	}
	myLog_d("Init LoRa success");

	/* bme680 init */
  	//init_bme680();
	initBSEC();

	/* vbat adc init */
  	initReadVBAT();

	/* acc init */
  	if (initACC())
		myLog_d("Init acc success");
	txPayload.accAlarm = 0;

	// Now we are connected, start the timer that will wakeup the loop frequently
	myLog_d("Start Wakeup Timer");

	taskWakeupTimer.begin(SLEEP_TIME, periodicWakeup);

	taskWakeupTimer.start();

	#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO
		// Give Serial some time to send everything
		delay(1000);
	#endif
}

void loop()
{
	// Sleep until we are woken up by an event
	if (xSemaphoreTake(taskEvent, portMAX_DELAY) == pdTRUE)
	{
		// Switch on green LED to show we are awake
		#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO
				digitalWrite(LED_BUILTIN, HIGH);
				delay(500); // Only so we can see the green LED
		#endif

		// Check the wake up reason
		switch (eventType)
		{
		case 0: // Wakeup reason is package downlink arrived
			myLog_d("Received package over LoRa");
			break;
		case 1: // Wakeup reason is timer
		{	
			myLog_d("Timer wakeup");
			txPayload.accAlarm = 0;
			handleLoopActions();
			
			myLog_d("send interval in millis: %i", (SEND_INTERVAL*1000));
			myLog_d("time millis: %i", millis());
			uint32_t remainder = (millis() - ( wakeCounter*(SEND_INTERVAL*1000) )  );
			myLog_d("time elapsed since last send: %i", remainder);

			if( millis() > (RESTART_INTERVAL) )
			{
				myLog_d("SYSTEM RESET TIMER TRIGGERED!");
				NVIC_SystemReset();
			}

			if( remainder > (SEND_INTERVAL*1000) )
			{
				wakeCounter++;
				#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_NONE
					myLog_d("Payload filled in loop: ");
					char rcvdData[sizeof(txPayload) * 4] = {0};
					uint8_t PldPrintBuffer [sizeof(txPayload)] = {0};
					memcpy(PldPrintBuffer, &txPayload, sizeof(txPayload));
					int index = 0;
					for (int idx = 0; idx < sizeof(txPayload) * 3; idx += 3)
					{
						sprintf(&rcvdData[idx], "%02x ", PldPrintBuffer[index++]);
					}
					myLog_d(rcvdData);
					delay(DEFWAIT);	
				#endif
				myLog_d("Initiate sending");
				sendLoRa();
			}

			break;
		}
		case 2: // Wakeup reason is accelerometer
		{
			myLog_d("ACC wakeup");
			txPayload.accAlarm = 1;
			
			handleLoopActions();

			#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_NONE
				myLog_d("Payload filled in loop: ");
				char rcvdData[sizeof(txPayload) * 4] = {0};
				uint8_t PldPrintBuffer [sizeof(txPayload)] = {0};
				memcpy(PldPrintBuffer, &txPayload, sizeof(txPayload));
				int index = 0;
				for (int idx = 0; idx < sizeof(txPayload) * 3; idx += 3)
				{
					sprintf(&rcvdData[idx], "%02x ", PldPrintBuffer[index++]);
				}
				myLog_d(rcvdData);
				delay(DEFWAIT);	
			#endif

			// Send the data package
			myLog_d("Initiate sending");

			sendLoRa();
			break;
		}
		default:
			myLog_d("This should never happen ;-)");
			NVIC_SystemReset();
			break;
		}

		myLog_d("Loop goes back to sleep.\n");
		attachInterrupt(WB_IO5, accIntHandler, CHANGE);
		attachInterrupt(WB_IO6, accIntHandler, CHANGE);

		// Go back to sleep - take the loop semaphore
		xSemaphoreTake(taskEvent, 10);

		#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO
				digitalWrite(LED_BUILTIN, LOW); //turn off indicator led
		#endif
	}

	// handleLoopActions();
	// delay(3000);
}

/* update txPayload with sensor data*/
void handleLoopActions(){
	txPayload.id = NODEID;
	delay(DEFWAIT);
	//bme680_get(&txPayload.temp_int, &txPayload.temp_dec, &txPayload.humdity_int, &txPayload.humdity_dec, &txPayload.bar_press);
	readBSEC(&txPayload.temp_int, &txPayload.temp_dec, &txPayload.humdity_int, &txPayload.humdity_dec, &txPayload.bar_press,
	 &txPayload.iaq, &txPayload.iaqAccuracy, &txPayload.co2equivalent, &txPayload.breathVocEquivalent, &txPayload.gasPercentage);
	myLog_d("T_INT payload: %i", txPayload.temp_int);
	myLog_d("H_INT payload: %i", txPayload.humdity_int);
	
	txPayload.bat_perc = readBatt();

	float accx = accSensor.readFloatAccelX();
	float accy = accSensor.readFloatAccelY();
	float accz = accSensor.readFloatAccelZ();

		#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_NONE
			myLog_d("Acc X: %f", accx ); 
			myLog_d("Acc y: %f", accy );
			myLog_d("Acc z: %f",accz );
			delay(DEFWAIT);
		#endif
		calculateTilt(accx, accy, accz, &txPayload.inc_x, &txPayload.inc_y, &txPayload.inc_z);
		#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_NONE
			myLog_d("Inc X: %i", txPayload.inc_x ); 
			myLog_d("Inc y: %i", txPayload.inc_y );
			myLog_d("Inc z: %i", txPayload.inc_z );
			delay(DEFWAIT);
		#endif

	txPayload.sentPackets = nodeSentPackets;

}