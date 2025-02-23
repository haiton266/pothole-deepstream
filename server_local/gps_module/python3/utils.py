def parse_gngll(nmea_sentence):
    """
    Extracts latitude and longitude from a $GNGLL NMEA sentence and converts them to decimal degrees.
    If GPS data is unavailable, it returns (0.0, 0.0).

    Parameters:
        nmea_sentence (str): NMEA sentence in the format
                             "$GNGLL,2232.474478,N,11404.704396,E,093837.000,A,D*46"

    Returns:
        tuple: (latitude, longitude) in decimal degrees or (0.0, 0.0) if invalid
    """
    try:
        parts = nmea_sentence.split(",")

        # Check if latitude or longitude fields are empty (no GPS fix)
        if not parts[1] or not parts[3]:
            return 0.0, 0.0  # No GPS data available

        # Extract latitude and longitude information
        raw_lat = parts[1]   # Latitude (ddmm.mmmm)
        lat_dir = parts[2]   # Latitude direction (N/S)
        raw_lon = parts[3]   # Longitude (dddmm.mmmm)
        lon_dir = parts[4]   # Longitude direction (E/W)

        # Convert latitude to decimal degrees
        lat_degrees = int(raw_lat[:2])  # First 2 digits are degrees
        lat_minutes = float(raw_lat[2:]) / 60  # Convert minutes to degrees
        latitude = lat_degrees + lat_minutes
        if lat_dir == "S":
            latitude *= -1  # South (S) means negative latitude

        # Convert longitude to decimal degrees
        lon_degrees = int(raw_lon[:3])  # First 3 digits are degrees
        lon_minutes = float(raw_lon[3:]) / 60  # Convert minutes to degrees
        longitude = lon_degrees + lon_minutes
        if lon_dir == "W":
            longitude *= -1  # West (W) means negative longitude

        return round(latitude, 6), round(longitude, 6)

    except Exception as e:
        print(f"Error processing NMEA sentence: {e}")
        return 0.0, 0.0  # Return (0.0, 0.0) in case of any unexpected error
