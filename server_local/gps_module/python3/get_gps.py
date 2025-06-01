import L76X
import time

def initialize_gps():
    """Perform the initial GPS setup once."""
    x = L76X.L76X()  # Initialize GPS device
    x.L76X_Set_Baudrate(115200)  # Set baud rate to 115200
    x.L76X_Send_Command(x.SET_NMEA_BAUDRATE_115200)  # Send NMEA baud rate command
    time.sleep(2)  # Wait for 2 seconds for the initial setup
    x.L76X_Set_Baudrate(115200)  # Set baud rate again (in case of reset)
    x.L76X_Send_Command(x.SET_POS_FIX_400MS)  # Set position fix at 400ms interval
    x.L76X_Send_Command(x.SET_NMEA_OUTPUT)  # Enable NMEA output
    x.L76X_Exit_BackupMode()  # Exit backup mode if any
    return x

def get_gps_data(x):
    """Retrieve GPS data when needed."""
    try:
        latitude, longitude = x.L76X_Gat_GNRMC()  # Get latitude and longitude
        return latitude, longitude

    except Exception as e:
        print(f"Error: {e}")  # Print any error that occurs

if __name__ == "__main__":
    # Call initialize_gps() once to set up the GPS
    gps_device = initialize_gps()

    # Then you can call get_gps_data() whenever you need to retrieve GPS data without waiting
    print(get_gps_data(gps_device))
