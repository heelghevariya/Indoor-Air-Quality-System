import serial
import requests
import json
import time
from datetime import datetime

# Constants
SERIAL_PORT = 'COM6'  # Replace with your serial port
BAUD_RATE = 115200  # Adjust as needed
FIREBASE_URL = 'https://air-quality-monitoring-89c66-default-rtdb.firebaseio.com/'
FIREBASE_PATH = '/data.json'  # Path in your Firebase database
INTERVAL = 1  # Interval in seconds for reading data

# Initialize serial connection


def init_serial(port, baud_rate):
    try:
        ser = serial.Serial(port, baud_rate)
        print(f"Serial connection established on {port} at {baud_rate} baud.")
        return ser
    except serial.SerialException as e:
        print(f"Failed to connect to serial port {port}: {e}")
        return None

# Read data from UART (waits until valid JSON is received)


def read_uart(ser):
    buffer = ""
    while True:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8').strip()
                buffer += line  # Accumulate the buffer

                try:
                    data = json.loads(buffer)
                    get_eco2 = data['eco2']
                    print(get_eco2)
                    get_tvoc = data['tvoc']

                    get_pm1f = float(f"{data['pm1'][0]}.{data['pm1'][1]}")
                    get_pm1 = round(get_pm1f, 2)

                    get_pm2f = float(f"{data['pm2.5'][0]}.{data['pm2.5'][1]}")
                    get_pm2 = round(get_pm2f, 2)

                    get_pm4f = float(f"{data['pm4'][0]}.{data['pm4'][1]}")
                    get_pm4 = round(get_pm4f, 2)

                    get_pm10f = float(f"{data['pm10'][0]}.{data['pm10'][1]}")
                    get_pm10 = round(get_pm10f, 2)

                    get_humidity = float(f"{data['humidity'][0]}")
                    get_temp = float(f"{data['temperature'][0]}")
                    get_co2 = data['co2']

                    buffer = ""  # Reset buffer after successful JSON parsing

                    # Return the data as a dictionary
                    return {
                        'eco2': get_eco2,
                        'tvoc': get_tvoc,
                        'pm1': get_pm1,
                        'pm2': get_pm2,
                        'pm4': get_pm4,
                        'pm10': get_pm10,
                        'co2': get_co2,
                        'humidity': get_humidity,
                        'temperature': get_temp
                    }

                except json.JSONDecodeError:
                    pass
                except KeyError as e:
                    print(f"KeyError: {e}")
                    buffer = ""  # Reset buffer if there is a KeyError

        except serial.SerialException as e:
            print(f"Error reading from serial port: {e}")

        time.sleep(0.1)  # Adjust delay as needed


def normalize(value, min_val, max_val):
    if value < min_val:
        return 0
    elif value > max_val:
        return 100
    else:
        return ((value - min_val) / (max_val - min_val)) * 100


def indoor_air_quality_index(temp, humidity, co2, pm1, pm25, pm4, pm10, tvoc):
    # Comfortable temperature range in °C
    if 18 <= temp <= 22:
        temp_norm = normalize(temp, 18, 22)
    elif 22 < temp <= 26:
        temp_norm = normalize(temp, 22, 26)
    else:
        # Using extreme values for out of comfort zone
        temp_norm = max(normalize(temp, -10, 18), normalize(temp, 26, 50))

    # Comfortable humidity range in %
    if 30 <= humidity <= 50:
        humidity_norm = normalize(humidity, 30, 50)
    elif 50 < humidity <= 60:
        humidity_norm = normalize(humidity, 50, 60)
    else:
        # Using extreme values for out of comfort zone
        humidity_norm = max(normalize(humidity, 0, 30),
                            normalize(humidity, 60, 100))

    # Acceptable CO2 levels in ppm
    co2_norm = normalize(co2, 0, 1000) if co2 <= 1000 else normalize(
        co2, 1000, 2000) if co2 <= 2000 else 100

    # PM1 guideline (WHO recommendation)
    pm1_norm = 0  # Placeholder as no clear threshold provided

    # PM2.5 guideline (WHO recommendation)
    pm25_norm = normalize(pm25, 0, 12) if pm25 <= 12 else normalize(
        pm25, 12, 35.4) if pm25 <= 35.4 else normalize(pm25, 35.4, 55.4) if pm25 <= 55.4 else 100

    # PM4 guideline (WHO recommendation)
    pm4_norm = 0  # Placeholder as no clear threshold provided

    # PM10 guideline (WHO recommendation)
    pm10_norm = normalize(pm10, 0, 54) if pm10 <= 54 else normalize(
        pm10, 54, 154) if pm10 <= 154 else normalize(pm10, 154, 254) if pm10 <= 254 else 100

    # TVOC levels in ppb (WHO recommendation)
    tvoc_norm = normalize(tvoc, 0, 220) if tvoc <= 220 else normalize(
        tvoc, 220, 660) if tvoc <= 660 else normalize(tvoc, 660, 2200) if tvoc <= 2200 else 100

    # Create a dictionary with variable names and their corresponding normalized values
    variables = {
        'temp_norm': temp_norm,
        'humidity_norm': humidity_norm,
        'co2_norm': co2_norm,
        'pm1_norm': pm1_norm,
        'pm25_norm': pm25_norm,
        'pm4_norm': pm4_norm,
        'pm10_norm': pm10_norm,
        'tvoc_norm': tvoc_norm
    }

    # max_level = max(variables, key=variables.get)
    # max_value = variables[max_level]
    # iaqi = max_value
    iaqi = (temp_norm + humidity_norm + co2_norm + pm1_norm +
            pm25_norm + pm4_norm + pm10_norm + tvoc_norm) / 8
    print(f"temp_norm: {temp_norm:.2f}\n"
          f"hum_norm: {humidity_norm:.2f}\n"
          f"co2_norm: {co2_norm:.2f}\n"
          f"pm1_norm: {pm1_norm:.2f}\n"
          f"pm25_norm: {pm25_norm:.2f}\n"
          f"pm4_norm: {pm4_norm:.2f}\n"
          f"pm10_norm: {pm10_norm:.2f}\n"
          f"tvoc_norm: {tvoc_norm:.2f}\n"
          f"AIR QUALITY INDEX: {iaqi}")

    # Print the variable name and its value
    # print(f"Variable with the highest value: {max_level}, Value: {iaqi}")
    return iaqi
    # return iaqi, max_value


# Extract eco2, tvoc, and other fields, and calculate IAQI
def extract_and_calculate_iaq(data):
    if data and all(key in data for key in ['eco2', 'tvoc', 'pm1', 'pm2', 'pm4', 'pm10', 'temperature', 'humidity']):
        # iaqi,higest_level
        iaqi = indoor_air_quality_index(
            temp=data['temperature'],
            humidity=data['humidity'],
            co2=data['co2'],
            pm1=data['pm1'],
            pm25=data['pm2'],
            pm4=data['pm4'],
            pm10=data['pm10'],
            tvoc=data['tvoc']
        )
        return {
            'eco2': data['eco2'],
            'tvoc': data['tvoc'],
            'pm1': data['pm1'],
            'pm2': data['pm2'],
            'pm4': data['pm4'],
            'pm10': data['pm10'],
            'co2': data['co2'],
            'humidity': data['humidity'],
            'temperature': data['temperature'],
            'iaqi': iaqi
            # 'higest_level': higest_level
        }
    return None


def send_to_firebase(data, path):
    timestamp = datetime.now().isoformat()
    data_with_timestamp = {
        'timestamp': timestamp,
        'sensor_data': data
    }
    url = f"{FIREBASE_URL}{path}"
    try:
        response = requests.post(url, json=data_with_timestamp)
        if response.status_code == 200:
            print("Data sent successfully")
        else:
            print(f"Failed to send data: {
                  response.status_code}, {response.text}")
    except requests.RequestException as e:
        print(f"Error sending data to Firebase: {e}")

# Main loop


def main():
    ser = init_serial(SERIAL_PORT, BAUD_RATE)
    if ser is None:
        return

    while True:
        data = read_uart(ser)
        if data:
            extracted_data = extract_and_calculate_iaq(data)
            if extracted_data:
                send_to_firebase(extracted_data, FIREBASE_PATH)
        time.sleep(INTERVAL)


if __name__ == "__main__":
    main()
