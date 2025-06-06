import requests

def send_pothole_detection(latitude, longitude, image_path, potholes, user_id='6639ff3aa242c91b70a1f851', type_detection='Ổ gà'):
    """
    Gửi dữ liệu phát hiện ổ gà lên API.

    Args:
        user_id (str): ID người dùng
        type_detection (str): Loại phát hiện (ví dụ: "Ổ gà")
        latitude (float): Vĩ độ GPS
        longitude (float): Kinh độ GPS
        image_path (str): Đường dẫn file ảnh

    Returns:
        dict: Phản hồi từ server
    """
    if latitude == 0.0 and longitude == 0.0:
        latitude = 16.074868
        longitude = 108.154072

    url = "http://51.144.112.213:3003/api/detection/create-for-jetson"

    # Dữ liệu gửi lên (form-data)
    data = {
        "userId": user_id,
        "typeDetection": type_detection,
        "location": f"LatLng(latitude:{latitude}, longitude:{longitude})",
        "description": "Large",
    }

    image_path = f"../../.{image_path}"
    with open(image_path, "rb") as img_file:
        files = {"image": img_file}
        response = requests.post(url, data=data, files=files)

    # Trả về phản hồi từ server
    if response.status_code == 200:
        print("✅ Thành công! Dữ liệu phản hồi từ server:")
        return response.json()
    else:
        print(f"❌ Lỗi {response.status_code}: {response.text}")
        return None

# Nhập dữ liệu từ người dùng
if __name__ == "__main__":
    latitude = 15.074548
    longitude = 101.15208
    image_path = '/home/jetson/hai/app_copy/_my-app/images_saved/img125.jpg'

    # Gọi hàm gửi dữ liệu
    response = send_pothole_detection(latitude, longitude, image_path, '')

    # Hiển thị kết quả
    if response:
        print(response)
