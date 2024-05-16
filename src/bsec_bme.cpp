#include <Arduino.h> 
#include "bsec.h"
#include <main.h>

// Helper functions declarations
void checkIaqSensorStatus(void);
void errLeds(void);

// const uint8_t bsec_config_iaq[] = {
// #include "config/generic_33v_3s_4d/bsec_iaq.txt"
// };
// #define STATE_SAVE_PERIOD	UINT32_C(360 * 60 * 1000) // 360 minutes - 4 times a day
// uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};

// Create an object of the class Bsec
Bsec iaqSensor;

String output;

void initBSEC()
{
  /* Initializes the Serial communication */
  iaqSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  myLog_d("BME sensor addr: %x", BME68X_I2C_ADDR_LOW);
  output = "BSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  myLog_d("%s",output.c_str());
  checkIaqSensorStatus();

  bsec_virtual_sensor_t sensorList[13] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_GAS_PERCENTAGE
  };

  iaqSensor.updateSubscription(sensorList, 13, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();
}

void readBSEC(uint8_t * t_int_pld, uint8_t * t_dec_pld, uint8_t * hum_int_pld, uint8_t * hum_dec_pld, uint16_t * press_pld,
 uint16_t * iaq, uint8_t * iaqAccuracy, uint16_t * co2Equivalent, uint16_t * breathVocEquivalent, uint8_t * gasPercentage)
{
  myLog_d("BSEC read...");
  delay(DEFWAIT);

  //checkIaqSensorStatus();
  myLog_d("Time: %i", iaqSensor.getLastTime());

  if (iaqSensor.run()) { // If new data is available
    unsigned long time_trigger = millis();
    *t_int_pld = iaqSensor.temperature; //put integer part into container
    *t_dec_pld = (iaqSensor.temperature- (*t_int_pld)) * 100; //put decimal part into container
    *hum_int_pld = iaqSensor.humidity; //put integer part into container
    *hum_dec_pld = (iaqSensor.humidity- (*hum_int_pld)) * 100; //put decimal part into container
    double press = iaqSensor.pressure/100;
    *press_pld = press; //put integer part into container
    *iaq = iaqSensor.iaq;
    *iaqAccuracy = iaqSensor.iaqAccuracy;
    *co2Equivalent = iaqSensor.co2Equivalent;
    *breathVocEquivalent = iaqSensor.breathVocEquivalent;
    *gasPercentage = iaqSensor.gasPercentage;

    //myLog_d("last time: %i", iaqSensor.getLastTime());
    //myLog_d("time tr.: %i", time_trigger);
    myLog_d("IAQ: %f, \tIAQ accuracy: %d, \tStatic IAQ: %f", iaqSensor.iaq, iaqSensor.iaqAccuracy, iaqSensor.staticIaq);
    delay(DEFWAIT);
    myLog_d("CO2 eq: %f, Breath VOC eq: %f, \traw T: %f", iaqSensor.co2Equivalent, iaqSensor.breathVocEquivalent, iaqSensor.rawTemperature);
    delay(DEFWAIT);
    myLog_d("Press: %f, \traw H: %f, \tgas R: %f", iaqSensor.pressure, iaqSensor.rawHumidity, iaqSensor.gasResistance);
    delay(DEFWAIT);
    myLog_d("Status: %f, \trunin status: %f, \tT (°C): %f", iaqSensor.stabStatus, iaqSensor.runInStatus, iaqSensor.temperature);
    delay(DEFWAIT);
    myLog_d("comp H (%): %f, \tgas %: %f", iaqSensor.humidity, iaqSensor.gasPercentage);
    delay(DEFWAIT);
    myLog_d("Reading ok");
  } else {
    myLog_d("iaq Sensor not run");
    checkIaqSensorStatus();
  }
}

// Helper function definitions
void checkIaqSensorStatus(void)
{
  myLog_d("Check IAQ sensor status...");
  if (iaqSensor.bsecStatus != BSEC_OK) {
    if (iaqSensor.bsecStatus < BSEC_OK) {
      output = "BSEC error code : " + String(iaqSensor.bsecStatus);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BSEC warning code : " + String(iaqSensor.bsecStatus);
      Serial.println(output);
    }
  }

  if (iaqSensor.bme68xStatus != BME68X_OK) {
    if (iaqSensor.bme68xStatus < BME68X_OK) {
      output = "BME68X error code : " + String(iaqSensor.bme68xStatus);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BME68X warning code : " + String(iaqSensor.bme68xStatus);
      Serial.println(output);
    }
  }
}

void errLeds(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}

