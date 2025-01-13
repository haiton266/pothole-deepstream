import cv2
import os

# Replace with your RTSP stream URL
rtsp_url = 'rtsp://localhost:8080/stream'

# Output directory to save images
output_dir = 'debug/saved_images'
os.makedirs(output_dir, exist_ok=True)

# Open the RTSP stream
cap = cv2.VideoCapture(rtsp_url)

if not cap.isOpened():
    print("Failed to open RTSP stream.")
    exit()

frame_count = 0
save_interval = 20  # Save every 30 frames (adjust as needed)

try:
    while True:
        ret, frame = cap.read()
        if not ret:
            print("Failed to read frame from stream.")
            break

        # Save frame at specified intervals
        if frame_count % save_interval == 0:
            image_path = os.path.join(output_dir, f"frame_{frame_count}.jpg")
            cv2.imwrite(image_path, frame)
            print(f"Saved {image_path}")

        frame_count += 1

except KeyboardInterrupt:
    print("Stream interrupted.")

finally:
    # Release resources
    cap.release()
    print("Stream closed.")
