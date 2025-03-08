import requests
import cv2
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
        latitude = 15.074548
        longitude = 101.15208

    url = "http://103.188.243.119:3005/api/detection/create-for-jetson"

    # Dữ liệu gửi lên (form-data)
    data = {
        "userId": user_id,
        "typeDetection": type_detection,
        "location": f"LatLng(latitude:{latitude}, longitude:{longitude})",
        "description": "Small",
    }

    image = cv2.imread(image_path)
    # Way 1
    for x1, y1, x2, y2 in potholes:
        x1, y1, x2, y2 = map(lambda v: round(float(v)), [x1, y1, x2, y2])
        cv2.rectangle(image, (x1, y1), (x2, y2), (0, 255, 0), 2)

    cv2.imwrite(image_path, image)

    with open(image_path, "rb") as img_file:
        files = {"image": img_file}
        response = requests.post(url, data=data, files=files)

    # Trả về phản hồi từ server
    if response.status_code == 200:
        print("✅ Thành công! Dữ liệu phản hồi từ server:")
        return response.json()
    else:
        print(f"❌ Lỗi {response.status_code}: {response.text}")
        return Nones

    # Way 2
    # Mã hóa ảnh sang JPEG
    # success, img_encoded = cv2.imencode(".jpg", image, [cv2.IMWRITE_JPEG_QUALITY, 90])
    # if not success:
    #     print("❌ Lỗi: Không thể mã hóa ảnh sang JPEG")
    #     return None

    # files = {"image": ("bbox_image.jpg", img_encoded.tobytes(), "image/jpeg")}

    # # Gửi request với timeout và kiểm soát lỗi
    # try:
    #     with requests.Session() as session:
    #         response = session.post(url, data=data, files=files, timeout=10)

    #     if response.status_code == 200:
    #         print("✅ Thành công! Dữ liệu phản hồi từ server:")
    #         return response.json()
    #     else:
    #         print(f"❌ Lỗi {response.status_code}: {response.text}")
    #         return None

    # except requests.exceptions.RequestException as e:
    #     print(f"❌ Lỗi mạng: {e}")
    #     return None

# Nhập dữ liệu từ người dùng
if __name__ == "__main__":
    # latitude = float(input("Nhập vĩ độ (latitude): "))
    # longitude = float(input("Nhập kinh độ (longitude): "))
    # image_path = input("Nhập đường dẫn ảnh: ")
    # user_id = input("Nhập userId: ")
    # type_detection = input("Nhập loại phát hiện (VD: Ổ gà): ")

    # latitude = 16.074548
    # longitude = 108.15208
    latitude = 15.074548
    longitude = 101.15208
    image_path = '/home/jetson/hai/app_copy/_my-app/debug/images.jpg'

    # Gọi hàm gửi dữ liệu
    response = send_pothole_detection(latitude, longitude, image_path)

    # Hiển thị kết quả
    if response:
        print(response)
