#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <HardwareSerial.h>
#include <esp_now.h>
#include <WiFi.h>

#define MPU_THRESHOLD 5.0            // Ngưỡng gia tốc (theo g) để xác định va chạm
#define PHONE_NUMBER "+84856195168"  // Số điện thoại nhận tin nhắn

// Khởi tạo đối tượng cho MPU6500 và SIM808
Adafruit_MPU6050 mpu;
HardwareSerial sim808(2);  // Sử dụng UART2 (GPIO16 và GPIO17) cho SIM808
String message;

typedef struct struct_message {
  bool alert;  // Biến cảnh báo CO từ Dự án 1
} struct_message;

struct_message receivedData;  // Dữ liệu nhận qua ESP-NOW
bool coAlert = false;         // Biến trạng thái cảnh báo CO từ Dự án 1

void setup() {
  Serial.begin(115200);  // Serial debug
  sim808.begin(9600, SERIAL_8N1, 16, 17);  // Serial cho SIM808 (9600 baud)
  Wire.begin();
  mpu.begin();

  Serial.println("MPU6500 enabled!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Khởi tạo mô-đun SIM808
  if (initializeSIM808()) {
    Serial.println("SIM808 enabled!");
  } else {
    Serial.println("SIM808 không thể khởi tạo!");
  }

  // Khởi tạo ESP-NOW
  WiFi.mode(WIFI_STA);  // ESP-NOW yêu cầu chế độ WIFI_STA
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW khởi tạo thất bại!");
    return;
  }

  // Đăng ký callback để nhận dữ liệu
  esp_now_register_recv_cb(onDataReceive);
}

void loop() {
  // Đọc dữ liệu từ MPU6500
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Chuyển giá trị gia tốc từ m/s² sang g
  float gX = a.acceleration.x / 9.81;
  float gY = a.acceleration.y / 9.81;
  float gZ = a.acceleration.z / 9.81;

  Serial.print("Acc X: "); Serial.print(gX); Serial.print(", ");
  Serial.print("Acc Y: "); Serial.print(gY); Serial.print(", ");
  Serial.print("Acc Z: "); Serial.println(gZ);

  // Kiểm tra va chạm dựa trên gia tốc
  if (abs(gX) > MPU_THRESHOLD || abs(gY) > MPU_THRESHOLD || abs(gZ) > MPU_THRESHOLD) {
    Serial.println("Impact detected!");
    if (getGPSLocation() != "GPS signal unavailable"){
      message = "CANH BAO: phat hien xe co va cham! Vi tri hien tai: https://www.google.com/maps?q=loc:" + getGPSLocation();
    }
    else{
      message = "CANH BAO: phat hien xe co va cham! Vi tri hien tai: " + getGPSLocation();
    }
    sendSMS(message);
    Serial.print(message);
    delay(5000);  // Đợi 5 giây trước khi kiểm tra lại
  }

  // Kiểm tra nếu nhận cảnh báo CO từ Dự án 1
  if (coAlert) {
    Serial.println("Received CO alert from Project 1!");
    if (getGPSLocation() != "GPS signal unavailable"){
      message = "CANH BAO: phat hien CO2 cao, vui long kiem tra ngay lap tuc! Vi tri hien tai: https://www.google.com/maps?q=loc:" + getGPSLocation();
    }
    else{
      message = "CANH BAO: phat hien CO2 cao, vui long kiem tra ngay lap tuc! Vi tri hien tai: " + getGPSLocation();
    }
    sendSMS(message);
    Serial.print(message);
    coAlert = false;  // Reset trạng thái cảnh báo
  }

  delay(500);
}

// Callback khi nhận dữ liệu từ ESP-NOW
void onDataReceive(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  coAlert = receivedData.alert;  // Cập nhật trạng thái cảnh báo CO
}

// Hàm khởi tạo SIM808
bool initializeSIM808() {
  sim808.println("AT");
  delay(1000);
  sim808.println("AT+CGNSPWR=1");
  if (readResponse("OK")) {
    sim808.println("AT+CSMINS?");
    delay(1000);
    if (readResponse("1")) {  // Thẻ SIM được nhận diện
      return true;
    }
  }  // Bật GPS
  delay(1000);
  return readResponse("OK");
}

// Hàm gửi tin nhắn SMS qua SIM808
void sendSMS(const String &message) {
  sim808.println("AT+CMGF=1");  // Đặt chế độ tin nhắn văn bản
  delay(1000);
  if (!readResponse("OK")) {
    Serial.println("Error: No text for SIM808.");
    return;
  }

  sim808.print("AT+CMGS=\"");    // Câu lệnh để gửi tin nhắn
  sim808.print(PHONE_NUMBER);    // Số điện thoại
  sim808.println("\"");
  delay(1000);

  sim808.println(message);  // Nội dung tin nhắn
  sim808.write(26);         // Ký tự kết thúc tin nhắn (Ctrl+Z)
  delay(5000);

  if (readResponse("OK")) {
    Serial.println("Message sent.");
  } else {
    Serial.println("Error: Message not sent.");
  }
}

// Hàm đọc phản hồi từ SIM808 và kiểm tra chuỗi mong muốn
bool readResponse(const char *expected) {
  unsigned long start = millis();
  while (millis() - start < 5000) {  // Đợi phản hồi trong 5 giây
    if (sim808.available()) {
      String response = sim808.readString();
      if (response.indexOf(expected) != -1) {
        return true;
      }
    }
  }
  return false;
}

String getGPSLocation() {
  sim808.println("AT+CGNSINF");  // Lấy thông tin GPS
  delay(1000);

  if (sim808.available()) {
    String gpsData = sim808.readString();
    if (gpsData.indexOf("+CGNSINF: 1,1") != -1) {  // GPS hoạt động và có tín hiệu
      // Lấy vĩ độ
      int latIndex = gpsData.indexOf(",", gpsData.indexOf(",", gpsData.indexOf(",") + 1) + 1) + 1;
      String latitude = gpsData.substring(latIndex, gpsData.indexOf(",", latIndex));
      
      // Lấy kinh độ
      int lonIndex = gpsData.indexOf(",", latIndex) + 1;
      String longitude = gpsData.substring(lonIndex, gpsData.indexOf(",", lonIndex));
      
      return latitude + "," + longitude;
    }
  }
  return "GPS signal unavailable";  // Không có tín hiệu GPS
}

