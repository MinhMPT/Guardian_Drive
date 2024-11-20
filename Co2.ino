#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <esp_now.h>
#include <WiFi.h>

#define LCD_ADDRESS 0x27  // Địa chỉ I2C của LCD
#define MQ7_PIN 34        // Chân ADC kết nối với MQ-7
#define BUZZER_PIN 25     // Chân điều khiển Buzzer
#define THRESHOLD 1000     // Ngưỡng khí CO để bật Buzzer

LiquidCrystal_I2C lcd(LCD_ADDRESS, 20, 4);

typedef struct struct_message {
  bool alert;  // Biến cảnh báo CO
} struct_message;

struct_message myData;  // Dữ liệu sẽ gửi qua ESP-NOW

// Địa chỉ MAC của Dự án 2
uint8_t peerAddress[] = { 0x14, 0x2B, 0x2F, 0xC4, 0xD8, 0xB4 };

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  pinMode(MQ7_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.setCursor(0, 0);
  lcd.print("CO Sensor Ready");

  // Khởi tạo ESP-NOW
  WiFi.mode(WIFI_STA);  // ESP-NOW yêu cầu chế độ WIFI_STA
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW khởi tạo thất bại");
    return;
  }

  // Thêm peer (Dự án 2)
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, peerAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Thêm peer thất bại");
    return;
  }
}

void loop() {
  int mq7Value = analogRead(MQ7_PIN);  // Đọc giá trị từ MQ-7

  lcd.setCursor(0, 1);
  lcd.print("CO Level: ");
  lcd.print(mq7Value);
  lcd.print("     "); // Xóa dư ký tự nếu có

  if (mq7Value > THRESHOLD) {
    digitalWrite(BUZZER_PIN, HIGH);  // Bật Buzzer nếu vượt ngưỡng
    lcd.setCursor(0, 2);
    lcd.print("Warning: High CO!");

    // Gửi cảnh báo qua ESP-NOW
    myData.alert = true;
    esp_now_send(peerAddress, (uint8_t *)&myData, sizeof(myData));
    Serial.println("Cảnh báo CO cao đã gửi đến Dự án 2");
  } else {
    digitalWrite(BUZZER_PIN, LOW);   // Tắt Buzzer
    lcd.setCursor(0, 2);
    lcd.print("                   "); // Xóa dòng cảnh báo nếu có

    // Gửi trạng thái bình thường qua ESP-NOW
    myData.alert = false;
    esp_now_send(peerAddress, (uint8_t *)&myData, sizeof(myData));
  }

  delay(1000);  // Đọc lại mỗi giây
}
