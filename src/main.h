#include <algorithm>
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

//BME functions
	#include <Adafruit_Sensor.h>
	#include <Adafruit_BME680.h>
	#define BMEADDR 0x76
	#define PRESS_DIV 1000
	//BME stuff
	extern Adafruit_BME680 bme;
	void init_bme680();
	void bme680_get(uint8_t * t_int_pld, uint8_t * t_dec_pld, uint8_t * hum_int_pld, uint8_t * hum_dec_pld, uint16_t * press_pld);

//BSEC functions
	void initBSEC();
	void readBSEC(uint8_t * t_int_pld, uint8_t * t_dec_pld, uint8_t * hum_int_pld, uint8_t * hum_dec_pld, uint16_t * press_pld,
 		uint16_t * iaq, uint8_t * iaqAccuracy, uint16_t * co2Equivalent, uint16_t * breathVocEquivalent, uint8_t * gasPercentage);

// ACC functions
	#include <SparkFunLIS3DH.h>
	#define INT1_PIN WB_IO5
	extern LIS3DH accSensor;
	bool initACC(void);
	void clearAccInt(void);
	void accIntHandler(void);
	void calculateTilt(float xacc, float yacc, float zacc, uint8_t * xinc, uint8_t * yinc, uint8_t * zinc);
	extern SemaphoreHandle_t loopEnable;

// Battery functions
	/** Definition of the Analog input that is connected to the battery voltage divider */
	#define PIN_VBAT A0
	/** Definition of milliVolt per LSB => 3.0V ADC range and 12-bit ADC resolution = 3000mV/4096 */
	#define VBAT_MV_PER_LSB (0.73242188F)
	/** Voltage divider value => 1.5M + 1M voltage divider on VBAT = (1.5M / (1M + 1.5M)) */
	#define VBAT_DIVIDER (0.4F)
	/** Compensation factor for the VBAT divider */
	#define VBAT_DIVIDER_COMP (1.73)
	/** Fixed calculation of milliVolt from compensation value */
	#define REAL_VBAT_MV_PER_LSB (VBAT_DIVIDER_COMP * VBAT_MV_PER_LSB)
	float readVBAT(void);
	void initReadVBAT(void);
	uint8_t readBatt(void);
	uint8_t lorawanBattLevel(void);
	extern uint8_t battLevel;

// Debug
#include <myLog.h>
#define MYLOG_LOG_LEVEL MYLOG_LOG_LEVEL_ERROR

#include <SX126x-RAK4630.h>
	#define TX_ONLY
//default wait time for prints and stuff
	#define DEFWAIT 30

//chain elements definitions
	//node IDentifier
	#define NODEID 102
	/* Time the device is sleeping in milliseconds for the node element to perform bsec */
	#define SLEEP_TIME 3 * 1000
	/* Time the device for tx */
	#define SEND_INTERVAL 900
	/*System restart interval*/
	#define RESTART_INTERVAL 86400000

struct __attribute__((packed)) TxdPayload{
		uint8_t id = NODEID;	             // Device ID
		uint8_t bat_perc;		 // Battery percentage
		uint8_t temp_int;        // Temperature integer
		uint8_t temp_dec;		 // Temperature tenths/hundredths
		uint8_t humdity_int;	 // Humidity integer
		uint8_t humdity_dec;	 // Humidity ones/tens/hundreds
		uint16_t bar_press;		 // Barometric pressure in hPa
		uint8_t inc_x;
		uint8_t inc_y;
		uint8_t inc_z;
		uint16_t iaq; //iaq value
		uint8_t iaqAccuracy; //iaq status (0-1-2)
		uint16_t co2equivalent; //co2 estimation ppm
		uint16_t breathVocEquivalent; //breath voc
		uint8_t gasPercentage;
		uint16_t sentPackets; //number of sent packets since last startup
		uint8_t accAlarm;   //accelerometer alarm flag
		//String code = "wmn24";
	};

//Payload Array
extern TxdPayload txPayload;

struct __attribute__((packed)) PldWrapper {
	int wrSize = 0;
	TxdPayload* wrBuffer;
};
extern PldWrapper pldWrap;

extern uint16_t nodeSentPackets;

// LoRa stuff
bool initLoRa(void);
void sendLoRa(void);

// Main loop stuff
void periodicWakeup(TimerHandle_t unused);
extern SemaphoreHandle_t taskEvent;
extern uint8_t eventType;
extern SoftwareTimer taskWakeupTimer;
extern void handleLoopActions();




