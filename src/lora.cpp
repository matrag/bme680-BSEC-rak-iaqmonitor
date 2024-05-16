/**
 * @file lora.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief LoRa init, send and receive functions
 * @version 0.1
 * @date 2021-01-03
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "main.h"

// Define LoRa parameters
#define RF_FREQUENCY 868300000	// Hz
#define TX_OUTPUT_POWER 22		// dBm
#define LORA_BANDWIDTH 0		// [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]
#define LORA_SPREADING_FACTOR 7 // [SF7..SF12]
#define LORA_CODINGRATE 1		// [1: 4/5, 2: 4/6,  3: 4/7,  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8	// Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0	// Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE 3000
#define TX_TIMEOUT_VALUE 3000

#define TSYM ((2^LORA_SPREADING_FACTOR)/125)*1000
#define TPREAMBLE (LORA_PREAMBLE_LENGTH+4.25)*TSYM
#define RXtime (2*TSYM)
#define SLEEPtime (TPREAMBLE-RXtime)+(8*TSYM)


// To get maximum power savings we use Radio.SetRxDutyCycle instead of Radio.Rx(0)
// This function keeps the SX1261/2 chip most of the time in sleep and only wakes up short times
// to catch incoming data packages
// See document SX1261_AN1200.36_SX1261-2_RxDutyCycle_V1.0 ==>> https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R0000001O3w/zsdHpRveb0_jlgJEedwalzsBaBnALfRq_MnJ25M_wtI
uint32_t duty_cycle_rx_time = 17 * 15.625; //4 symbols = 17  [DURATION OF rx Preamble SYMBOLS IN us / 15.625 ^2 ]
uint32_t duty_cycle_sleep_time = 1385 * 15.625; //338 symbols = 1385

// DIO1 pin on RAK4631
#define PIN_LORA_DIO_1 47

// LoRa callbacks
static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
void OnTxTimeout(void);
void OnRxTimeout(void);
void OnRxError(void);
void OnCadDone(bool cadResult);

time_t cadTime;
time_t channelTimeout;
uint8_t channelFreeRetryNum = 0;

bool initLoRa(void)
{
	// Initialize library
	if (lora_rak4630_init() == 1)
	{
		return false;
	}

	// Initialize the Radio
	RadioEvents.TxDone = OnTxDone;
	RadioEvents.RxDone = OnRxDone;
	RadioEvents.TxTimeout = OnTxTimeout;
	RadioEvents.RxTimeout = OnRxTimeout;
	RadioEvents.RxError = OnRxError;
	RadioEvents.CadDone = OnCadDone;

	Radio.Init(&RadioEvents);

	Radio.Sleep(); // Radio.Standby();

	Radio.SetChannel(RF_FREQUENCY);

	Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
					  LORA_SPREADING_FACTOR, LORA_CODINGRATE,
					  LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
					  true, 0, 0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);

	Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
					  LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
					  LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
					  0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

#ifdef TX_ONLY
	Radio.Sleep();
#else
	// To get maximum power savings we use Radio.SetRxDutyCycle instead of Radio.Rx(0)
	// This function keeps the SX1261/2 chip most of the time in sleep and only wakes up short times
	// to catch incoming data packages
	// See document SX1261_AN1200.36_SX1261-2_RxDutyCycle_V1.0 ==>> https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R0000001O3w/zsdHpRveb0_jlgJEedwalzsBaBnALfRq_MnJ25M_wtI
	Radio.SetRxDutyCycle(duty_cycle_rx_time, duty_cycle_sleep_time);
#endif
	return true;
}

/**
 * @brief Prepare packet to be sent and start CAD routine
 * 
 */
void sendLoRa()
{
	myLog_d("Start sendLoRa");
	// Prepare LoRa CAD
	Radio.Sleep(); // Radio.Standby();
	Radio.SetCadParams(LORA_CAD_08_SYMBOL, LORA_SPREADING_FACTOR + 13, 10, LORA_CAD_ONLY, 0);
	cadTime = millis();
	channelTimeout = millis();

	// Switch on Indicator lights
	#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_NONE
		digitalWrite(LED_CONN, HIGH);
	#endif

	myLog_d("Start CAD");
	// Start CAD
	Radio.StartCad();
	myLog_d("out of sendLoRa");
}

/**
 * @brief Function to be executed on Radio Tx Done event
 */
void OnTxDone(void)
{
	myLog_d("OnTxDone\n");
	nodeSentPackets ++;

#ifdef TX_ONLY
	Radio.Sleep();
#else
	// To get maximum power savings we use Radio.SetRxDutyCycle instead of Radio.Rx(0)
	// This function keeps the SX1261/2 chip most of the time in sleep and only wakes up short times
	// to catch incoming data packages
	// See document SX1261_AN1200.36_SX1261-2_RxDutyCycle_V1.0 ==>> https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R0000001O3w/zsdHpRveb0_jlgJEedwalzsBaBnALfRq_MnJ25M_wtI
	Radio.SetRxDutyCycle(duty_cycle_rx_time, duty_cycle_sleep_time);
#endif

	// Switch off the indicator lights
#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_NONE
	digitalWrite(LED_CONN, LOW);
#endif
}

/**@brief Function to be executed on Radio Rx Done event
 */
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
	/*if the node is a chain element, start receive processing
	else, skip*/
	#ifdef IS_CHAIN_ELEMENT
		myLog_d("OnRxDone");
		//delay(DEFWAIT);
		eventType = 0;
		// Notify task about the event
		if (taskEvent != NULL)
		{
			xSemaphoreGive(taskEvent);
		}
		
		int rcvRxPayloadIndex = size / sizeof(txPayload); //dynamic index of received payload
		uint8_t PldBuffer [size]; //create received pld buffer
		memcpy(&PldBuffer[0], payload, size); //copy content into buffer 
		uint8_t receivedPldPrevID = PldBuffer [(rcvRxPayloadIndex-1) * sizeof(txPayload)]; //store previous package id

		/*If the ID of the received package is NODEID-1 copy content
		else write to debug message*/
		if (receivedPldPrevID == (NODEID-1) ) 
		{
			pldWrap.wrSize = size; //update wrapper size with received packet size
			#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO //Print payload to debug
				myLog_d("received package from node ID %i", receivedPldPrevID);
				myLog_d("Copying %i bytes into the PldArray!", size);
				myLog_d("RSSI: -%d dBm", txPayload.rssi_last );
				myLog_d("snr: %d dB", txPayload.snr_last );
				delay(DEFWAIT);

				//Print payload to debug
				char rcvdData[256 * 4] = {0};
				int index = 0;
				for (int idx = 0; idx < (size * 3); idx += 3)
				{
					//sprintf(&rcvdData[idx], "%02X ", payload[index++]); //print in hex with leading zeros
					sprintf(&rcvdData[idx], "%02x ", payload[index++]);
				}
				myLog_d(rcvdData);
				delay(DEFWAIT);	
			#endif
			//copy received payload 
			memcpy(&PldArray[0], payload, size); 
			
			//fill received package metadata
			txPayload.rssi_last = -1 * rssi;
			txPayload.snr_last = snr;
		}
		else 
		{
			#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO 
				myLog_d("received package from node ID %i", receivedPldPrevID);
				myLog_d("RSSI: %d dBm", rssi );
				myLog_d("snr: %d dB", snr );
				myLog_d("Message does not come from previous node. Skipping, wait for next. ");
				delay(DEFWAIT);
			#endif
		}

		//PldArray[rcvRxPayloadIndex-1].rssi_last = -1 * rssi;
		//PldArray[rcvRxPayloadIndex-1].snr_last = snr;
		//PldArray[NODEID-1].pckLength = size;

	#endif

	#ifdef TX_ONLY
		Radio.Sleep(); // Radio.Standby();
	#else
		// To get maximum power savings we use Radio.SetRxDutyCycle instead of Radio.Rx(0)
		// This function keeps the SX1261/2 chip most of the time in sleep and only wakes up short times
		// to catch incoming data packages
		// See document SX1261_AN1200.36_SX1261-2_RxDutyCycle_V1.0 ==>> https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R0000001O3w/zsdHpRveb0_jlgJEedwalzsBaBnALfRq_MnJ25M_wtI
		Radio.SetRxDutyCycle(duty_cycle_rx_time, duty_cycle_sleep_time);
	#endif

		// Switch off the indicator lights
	#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO
		digitalWrite(LED_CONN, LOW);
	#endif
}

/**@brief Function to be executed on Radio Tx Timeout event
 */
void OnTxTimeout(void)
{
	myLog_d("OnTxTimeout");

#ifdef TX_ONLY
	Radio.Sleep(); // Radio.Standby();
#else
	// To get maximum power savings we use Radio.SetRxDutyCycle instead of Radio.Rx(0)
	// This function keeps the SX1261/2 chip most of the time in sleep and only wakes up short times
	// to catch incoming data packages
	// See document SX1261_AN1200.36_SX1261-2_RxDutyCycle_V1.0 ==>> https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R0000001O3w/zsdHpRveb0_jlgJEedwalzsBaBnALfRq_MnJ25M_wtI
	Radio.SetRxDutyCycle(duty_cycle_rx_time, duty_cycle_sleep_time);
#endif

	// Switch off the indicator lights
#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO
	digitalWrite(LED_CONN, LOW);
#endif
}

/**@brief Function to be executed on Radio Rx Timeout event
 */
void OnRxTimeout(void)
{
	myLog_d("OnRxTimeout");

#ifdef TX_ONLY
	Radio.Sleep(); // Radio.Standby();
#else
	// To get maximum power savings we use Radio.SetRxDutyCycle instead of Radio.Rx(0)
	// This function keeps the SX1261/2 chip most of the time in sleep and only wakes up short times
	// to catch incoming data packages
	// See document SX1261_AN1200.36_SX1261-2_RxDutyCycle_V1.0 ==>> https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R0000001O3w/zsdHpRveb0_jlgJEedwalzsBaBnALfRq_MnJ25M_wtI
	Radio.SetRxDutyCycle(duty_cycle_rx_time, duty_cycle_sleep_time);
#endif

	// Switch off the indicator lights
#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO
	digitalWrite(LED_CONN, LOW);
#endif
}

/**@brief Function to be executed on Radio Rx Error event
 */
void OnRxError(void)
{
#ifdef TX_ONLY
	Radio.Sleep(); // Radio.Standby();
#else
	// To get maximum power savings we use Radio.SetRxDutyCycle instead of Radio.Rx(0)
	// This function keeps the SX1261/2 chip most of the time in sleep and only wakes up short times
	// to catch incoming data packages
	// See document SX1261_AN1200.36_SX1261-2_RxDutyCycle_V1.0 ==>> https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R0000001O3w/zsdHpRveb0_jlgJEedwalzsBaBnALfRq_MnJ25M_wtI
	Radio.SetRxDutyCycle(duty_cycle_rx_time, duty_cycle_sleep_time);
#endif

	// Switch off the indicator lights
#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO
	digitalWrite(LED_CONN, LOW);
#endif
}

/**@brief Function to be executed on Radio Rx Error event
 */
void OnCadDone(bool cadResult)
{
	myLog_d("CAD done");
	if (cadResult)
	{
		#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_NONE
			myLog_d("Channel Busy");
			delay(DEFWAIT);
		#endif

		#ifdef TX_ONLY
				Radio.Sleep(); // Radio.Standby();
		#else
				// To get maximum power savings we use Radio.SetRxDutyCycle instead of Radio.Rx(0)
				// This function keeps the SX1261/2 chip most of the time in sleep and only wakes up short times
				// to catch incoming data packages
				// See document SX1261_AN1200.36_SX1261-2_RxDutyCycle_V1.0 ==>> https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R0000001O3w/zsdHpRveb0_jlgJEedwalzsBaBnALfRq_MnJ25M_wtI
				Radio.SetRxDutyCycle(duty_cycle_rx_time, duty_cycle_sleep_time);
		#endif

				// Switch off the indicator lights
		#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_INFO
				digitalWrite(LED_CONN, LOW);
		#endif
	}
	else
	{
		#if MYLOG_LOG_LEVEL > MYLOG_LOG_LEVEL_NONE 
			myLog_d("CAD returned channel free after %ldms", (long)(millis() - cadTime));
			//print packet to send
			myLog_d("Dimensions of payload to send: %i", sizeof(txPayload));
			myLog_d("Payload to send: ");
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
		Radio.Send(&txPayload.id, sizeof(txPayload)); //Send packet on LoRa P2P
		myLog_d("radio send.");
	}
}