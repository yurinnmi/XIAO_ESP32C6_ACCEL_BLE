#include <Arduino.h>
#include <Wire.h>
#include <NimBLEDevice.h>
#include <NimBLEBeacon.h>

// ADXL345 I2Cアドレス（ALT ADDRESS=GNDの場合）
#define ADXL345_ADDR 0x53

// ADXL345 レジスタ定義
#define REG_BW_RATE    0x2C
#define REG_POWER_CTL  0x2D
#define REG_DATA_FORMAT 0x31
#define REG_DATAX0     0x32

#define SCALE_FACTOR 0.004f // g/LSB

#define MOVING_THRESHOLD 30

//iBeacon発生インターバル：100ms　（基準は0.625ms間隔）
#define BEACON_ADVERTISING_TIME	160   // 160 * 0.625ms = 100ms

// TODO: Hey, you! Replace this with your own unique UUID with something like https://www.uuidgenerator.net/
//const char* iBeaconUUID = "26D0814C-F81C-4B2D-AC57-032E2AFF8642";
const char* iBeaconUUID = "86350f97-914f-43a3-9428-39a3dd747865";

bool isMovingflag = false;  // ← このフラグをMajorに反映
static bool prevFlag = isMovingflag;
NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void readRegisters(uint8_t reg, uint8_t count, uint8_t *buf) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ADXL345_ADDR, count);
  for (uint8_t i = 0; i < count; i++) {
    if (Wire.available()) {
      buf[i] = Wire.read();
    }
  }
}

bool isMoving(int x, int y, int z, int threshold = MOVING_THRESHOLD) {
  static int prevX = 0, prevY = 0, prevZ = 0;
  int deltaX = abs(x - prevX);
  int deltaY = abs(y - prevY);
  int deltaZ = abs(z - prevZ);
  prevX = x;
  prevY = y;
  prevZ = z;
  return (deltaX > threshold || deltaY > threshold || deltaZ > threshold);
}

void initIBeacon() {
	NimBLEDevice::init("MyESP32C6 iBeacon");
}

void startIBeacon() {
  NimBLEBeacon beacon;
  NimBLEAdvertisementData beaconAdvertisementData;

	// Create beacon object
  beacon.setMajor(isMovingflag ? 1 : 0);
	beacon.setMinor(0x0123);
	beacon.setSignalPower(0xC5); // Optional
	beacon.setProximityUUID(BLEUUID(iBeaconUUID)); // Unlike Bluedroid, you do not need to reverse endianness here

	// Create advertisement data
	beaconAdvertisementData.setFlags(0x04); // BR_EDR_NOT_SUPPORTED
	beaconAdvertisementData.setManufacturerData(beacon.getData());

	// Start advertising
	advertising->setAdvertisingInterval(BEACON_ADVERTISING_TIME);
	advertising->setAdvertisementData(beaconAdvertisementData);
	advertising->start();

}

void setup() {
  Serial.begin(115200);
  Wire.begin();  // SDA/SCL ピンはESP32C6のデフォルトピンを使用

  initIBeacon();
  startIBeacon();

  delay(100);
  
  // レジスタ設定
  writeRegister(REG_BW_RATE, 0x0B);      // 出力データレート設定
  writeRegister(REG_DATA_FORMAT, 0x29);  // ±4g, FULL_RES, right-justified
  writeRegister(REG_POWER_CTL, 0x08);    // 測定モード有効

  Serial.println("ADXL345 Initialized");
}

void loop() {
  uint8_t buf[6];
  int16_t x_raw, y_raw, z_raw;
  float x_g, y_g, z_g;

  // X, Y, Z 各軸 2byteずつ読み取り
  readRegisters(REG_DATAX0, 6, buf);

  x_raw = (int16_t)((buf[1] << 8) | buf[0]);
  y_raw = (int16_t)((buf[3] << 8) | buf[2]);
  z_raw = (int16_t)((buf[5] << 8) | buf[4]);

    // isMovingflagを監視して必要なら再設定
  isMovingflag = isMoving(x_raw, y_raw, z_raw);

  if (prevFlag != isMovingflag) {
  	advertising->stop();
    startIBeacon();
    prevFlag = isMovingflag;
  }

  delay(100); // 10msごと
}
