
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/openthread.h>
#include <openthread/thread.h>
#include <openthread/udp.h>
#include <zephyr/drivers/sensor/ccs811.h>
#include <stdio.h> // printf

#include "sensors/sps30/sps30.h"
#include "cJSON.h"
// #include "scd4x_i2c.h"
#include "sensors/scd41/scd4x_i2c.h"
#include "sensors/scd41/sensirion_common.h"
#include "sensors/scd41/sensirion_i2c_hal.h"

int16_t get_serial(int16_t error, int16_t serial_0, int16_t serial_1, int16_t serial_2);
bool read_scd41();

#define SLEEP_TIME_MS 5000
#define I2C_DEV DT_LABEL(DT_ALIAS(i2c0))
#define I2C_NODE DT_NODELABEL(i2c0)
#define I2C_DEV_LABEL DT_LABEL(DT_NODELABEL(i2c0))
#define interval 30

struct sensor_value _eco2, _tvoc;

uint16_t _co2;
int32_t _temperature;
int32_t _humidity;

// Function to create a JSON string dynamically
char *create_json_string(int eco2, int tvoc, struct sensor_value pm1, struct sensor_value pm2, struct sensor_value pm4, struct sensor_value pm10, struct sensor_value temperature, struct sensor_value humidity, int co2)
{
        // Create a JSON object
        cJSON *json = cJSON_CreateObject();

        if (json == NULL)
        {
                fprintf(stderr, "Failed to create JSON object.\n");
                return NULL;
        }

        // Add a float to the JSON object
        cJSON_AddNumberToObject(json, "eco2", eco2);

        cJSON_AddNumberToObject(json, "tvoc", tvoc);

        // Create a JSON array
        cJSON *array = cJSON_CreateArray();

        // Add an integer and a float to the array
        cJSON_AddItemToArray(array, cJSON_CreateNumber(pm1.val1));
        cJSON_AddItemToArray(array, cJSON_CreateNumber(pm1.val2));

        // Add the array to the JSON object with the key "values"
        cJSON_AddItemToObject(json, "pm1", array);

        array = cJSON_CreateArray();
        // Add an integer and a float to the array
        cJSON_AddItemToArray(array, cJSON_CreateNumber(pm2.val1));
        cJSON_AddItemToArray(array, cJSON_CreateNumber(pm2.val2));

        // Add the array to the JSON object with the key "values"
        cJSON_AddItemToObject(json, "pm2.5", array);

        array = cJSON_CreateArray();
        // Add an integer and a float to the array
        cJSON_AddItemToArray(array, cJSON_CreateNumber(pm4.val1));
        cJSON_AddItemToArray(array, cJSON_CreateNumber(pm4.val2));

        // Add the array to the JSON object with the key "values"
        cJSON_AddItemToObject(json, "pm4", array);

        array = cJSON_CreateArray();
        // Add an integer and a float to the array
        cJSON_AddItemToArray(array, cJSON_CreateNumber(pm10.val1));
        cJSON_AddItemToArray(array, cJSON_CreateNumber(pm10.val2));

        // Add the array to the JSON object with the key "values"
        cJSON_AddItemToObject(json, "pm10", array);

        cJSON_AddNumberToObject(json, "co2", co2);

        // cJSON_AddNumberToObject(json, "humidity", _humidity);

        // cJSON_AddNumberToObject(json, "temperature", _temperature);

        array = cJSON_CreateArray();
        // Add an integer and a float to the array
        cJSON_AddItemToArray(array, cJSON_CreateNumber(humidity.val1));
        cJSON_AddItemToArray(array, cJSON_CreateNumber(humidity.val2));

        // Add the array to the JSON object with the key "values"
        cJSON_AddItemToObject(json, "humidity", array);

        array = cJSON_CreateArray();
        // Add an integer and a float to the array
        cJSON_AddItemToArray(array, cJSON_CreateNumber(temperature.val1));
        cJSON_AddItemToArray(array, cJSON_CreateNumber(temperature.val2));

        // Add the array to the JSON object with the key "values"
        cJSON_AddItemToObject(json, "temperature", array);

        // Print the JSON object
        char *json_string = cJSON_Print(json);
        if (json_string == NULL)
        {
                cJSON_Delete(json);
                fprintf(stderr, "Failed to print JSON object.\n");
                return NULL;
        }

        // Free the JSON object
        cJSON_Delete(json);

        // Return the JSON string
        return json_string;
}

static void udp_send(char *string)
{
        otError error = OT_ERROR_NONE;
        const char *buf = string;
        printk("Sending Data :- %s\n", buf);
        otInstance *myInstance;
        myInstance = openthread_get_default_instance();
        otUdpSocket mySocket;
        otMessageInfo messageInfo;
        memset(&messageInfo, 0, sizeof(messageInfo));
        otIp6AddressFromString("ff03::1", &messageInfo.mPeerAddr);
        messageInfo.mPeerPort = 1235;
        do
        {
                error = otUdpOpen(myInstance, &mySocket, NULL, NULL);
                if (error != OT_ERROR_NONE)
                {
                        break;
                }
                otMessage *test_Message = otUdpNewMessage(myInstance, NULL);
                error = otMessageAppend(test_Message, buf, (uint16_t)strlen(buf));
                if (error != OT_ERROR_NONE)
                {
                        break;
                }
                error = otUdpSend(myInstance, &mySocket, test_Message, &messageInfo);
                if (error != OT_ERROR_NONE)
                {
                        break;
                }
                error = otUdpClose(myInstance, &mySocket);
        } while (false);
        if (error == OT_ERROR_NONE)
        {
                printk("Send.\n");
        }
        else
        {
                printk("udpSend error: %d\n", error);
        }
}

int16_t get_serial(int16_t error, int16_t serial_0, int16_t serial_1, int16_t serial_2)
{
        error = scd4x_get_serial_number(&serial_0, &serial_1, &serial_2);
        return error;
}

bool read_scd41()
{
        int16_t error;
        bool data_ready_flag = false;
        error = scd4x_get_data_ready_flag(&data_ready_flag);
        if (error)
        {
                printf("Error executing scd4x_get_data_ready_flag(): %i\n", error);
                _humidity = -1;
                _temperature = -1;
                return false;
        }
        if (!data_ready_flag)
        {
                _humidity = -1;
                _temperature = -1;
                return false;
        }

        error = scd4x_read_measurement(&_co2, &_temperature, &_humidity);
        if (error)
        {
                _humidity = -1;
                _temperature = -1;
                printf("Error executing scd4x_read_measurement(): %i\n", error);
                return false;
        }
        else if (_co2 == 0)
        {
                printf("Invalid sample detected, skipping.\n");
                _humidity = -1;
                _temperature = -1;
        }
        else
        {
                printk("SCD41: \n");
                _temperature = _temperature / 1000;
                _humidity = _humidity / 1000;
                printk("CO2: %u ppm\n", _co2);
                printk("Temperature: %u *C\n", _temperature);
                printk("Humidity: %u %%\n", _humidity);
        }
        return true;
}

void read_ccs811(struct device ccs811)
{
        //     /* Initialize CCS811 */
        // const struct device *ccs811 = DEVICE_DT_GET_ONE(ams_ccs811);
        int rc = 0;
        rc = sensor_sample_fetch(&ccs811);

        if (rc == 0)
        {
                sensor_channel_get(&ccs811, SENSOR_CHAN_CO2, &_eco2);
                sensor_channel_get(&ccs811, SENSOR_CHAN_VOC, &_tvoc);
        }
        else
        {
                _eco2.val1 = -1;
                _tvoc.val1 = -1;
        }

        printk("\nCCS811: %u ppm eCO2: %u ppb eTVOC\n", _eco2.val1, _tvoc.val1);
}

struct sps30_measurement read_sps30(int16_t ret, struct sps30_measurement m)
{
        sensirion_sleep_usec(SPS30_MEASUREMENT_DURATION_USEC);
        ret = sps30_read_measurement(&m);
        if (ret < 0)
        {
                printk("error reading measurement\n");
                m.mc_1p0 = -1;
                m.mc_2p5 = -1;
                m.mc_4p0 = -1;
                m.mc_10p0 = -1;
        }
        else
        {
                printk("SPS30:\n"
                       "%0.2f pm1.0\n"
                       "%0.2f pm2.5\n"
                       "%0.2f pm4.0\n"
                       "%0.2f pm10.0\n"
                       "%0.2f nc0.5\n"
                       "%0.2f nc1.0\n"
                       "%0.2f nc2.5\n"
                       "%0.2f nc4.5\n"
                       "%0.2f nc10.0\n"
                       "%0.2f typical particle size\n\n",
                       (double)m.mc_1p0, (double)m.mc_2p5, (double)m.mc_4p0, (double)m.mc_10p0, (double)m.nc_0p5, (double)m.nc_1p0,
                       (double)m.nc_2p5, (double)m.nc_4p0, (double)m.nc_10p0, (double)m.typical_particle_size);
        }
        return m;
}

int main(void)
{
        // --------------------------Initializing SCD41--------------------------
        int16_t error = 0;
        sensirion_i2c_hal_init();
        scd4x_wake_up();
        scd4x_stop_periodic_measurement();
        scd4x_reinit();
        struct sensor_value temp, humi;

        uint16_t serial_0 = 0;
        uint16_t serial_1 = 0;
        uint16_t serial_2 = 0;

        error = get_serial(error, serial_0, serial_1, serial_2);
        if (error)
        {
                printk("Error executing scd4x_get_serial_number(): %i\n", error);
                _humidity = -1;
                _temperature = -1;
        }
        else
        {
                printk("serial: 0x%04x%04x%04x\n", serial_0, serial_1, serial_2);
                _humidity = -1;
                _temperature = -1;
        }

        error = scd4x_start_periodic_measurement();
        if (error)
        {
                printk("Error executing scd4x_start_periodic_measurement(): %i\n",
                       error);
                _humidity = -1;
                _temperature = -1;
        }

        // --------------------------Initializing ccs811--------------------------
        const struct device *ccs811 = DEVICE_DT_GET_ONE(ams_ccs811);

        sensirion_i2c_init();

        if (ccs811 == NULL) // ccs811_init(&ccs811, i2c_dev, CCS811_I2C_ADDRESS) != 0
        {
                printk("Failed to initialize CCS811 sensor\n");
                _eco2.val1 = -1;
                _tvoc.val1 = -1;
        }
        printk("CCS811 initialized\n");

        // --------------------------Initializing sps30--------------------------

        uint8_t fw_major;
        uint8_t fw_minor;
        int16_t ret;
        struct sps30_measurement m;
        struct sensor_value pm1, pm2, pm4, pm10;

        while (sps30_probe() != 0)
        {
                printk("SPS30 sensor probing failed\n");
                k_sleep(K_SECONDS(1));
        }
        printk("SPS sensor probing successful\n");

        ret = sps30_read_firmware_version(&fw_major, &fw_minor);
        if (ret)
        {
                printk("error reading firmware version\n");
        }
        else
        {
                printk("FW: %u.%u\n", fw_major, fw_minor);
        }

        char serial_number[SPS30_MAX_SERIAL_LEN];
        ret = sps30_get_serial(serial_number);
        if (ret)
        {
                printk("error reading serial number\n");
        }
        else
        {
                printk("Serial Number: %s\n\n", serial_number);
        }

        ret = sps30_start_measurement();
        if (ret < 0)
        {
                printk("error starting measurement\n");
                m.mc_1p0 = -1;
                m.mc_2p5 = -1;
                m.mc_4p0 = -1;
                m.mc_10p0 = -1;
        }

        printk("Attempting to read data from all three sensors.\n");

        while (1)
        {
                bool success = read_scd41();
                if (!success)
                {
                        continue;
                        k_sleep(K_SECONDS(5));
                }
                read_ccs811(*ccs811);
                m = read_sps30(ret, m);
                //  This below code for convert the float value to two int numbers
                // For example 12.52 float value so first int value is 12 and second is 52
                sensor_value_from_float(&pm1, m.mc_1p0);
                sensor_value_from_float(&pm2, m.mc_2p5);
                sensor_value_from_float(&pm4, m.mc_4p0);
                sensor_value_from_float(&pm10, m.mc_10p0);

                sensor_value_from_float(&humi, _humidity);
                sensor_value_from_float(&temp, _temperature);

                // This function is helps to convert the normal string to JSON string
                char *json_string = create_json_string((int)_eco2.val1, (int)_tvoc.val1, pm1, pm2, pm4, pm10, temp, humi, _co2);

                // printk("Sending Data :- %s\n", json_string);

                // This funstion send json string to other board using COAP protocol
                // coap_send_data_request(json_string);
                udp_send(json_string);

                k_sleep(K_SECONDS(interval));
        }
        return 0;
}