from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import requests
from datetime import datetime

app = FastAPI()

# Define a Pydantic model for the request body
class PotholeDetectionRequest(BaseModel):
    image_path: str

# Define a Pydantic model for the GPS data
class GPSData(BaseModel):
    latitude: float
    longitude: float

# Define a Pydantic model for the payload to the cloud server
class CloudPayload(BaseModel):
    timestamp: str
    image: str
    location: GPSData

@app.post("/pothole-detected")
async def handle_pothole_detection(request: PotholeDetectionRequest):
    # Extract image path from the request
    image_path = request.image_path

    # Call GPS API
    gps_data = get_gps_data()

    # Combine data
    payload = CloudPayload(
        image=image_path,
        location=gps_data
    )

    # Post to cloud server
    try:
        response = requests.post('https://cloud-server.com/api/data', json=payload.dict())
        response.raise_for_status()  # Raise an exception for HTTP errors
    except requests.exceptions.RequestException as e:
        raise HTTPException(status_code=500, detail=f"Failed to send data to cloud server: {str(e)}")

    return {"status": "success"}

def get_gps_data() -> GPSData:
    # Call GPS API and return latitude/longitude
    # Replace this with actual GPS API call logic
    return GPSData(latitude=37.7749, longitude=-122.4194)