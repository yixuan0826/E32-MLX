#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include <SPI.h>
#include <TFT_eSPI.h>  // 适用于ESP32的TFT显示屏驱动

// MLX90640传感器对象
Adafruit_MLX90640 mlx;

// TFT显示屏对象
TFT_eSPI tft = TFT_eSPI();

// 热成像参数
#define IMAGE_WIDTH 32
#define IMAGE_HEIGHT 24
float mlx90640Frame[IMAGE_WIDTH * IMAGE_HEIGHT];

// 温度范围设置
float minTemp = 20.0;  // 最低显示温度 (℃)
float maxTemp = 40.0;  // 最高显示温度 (℃)

// 颜色映射 (从冷到热: 蓝->青->绿->黄->红)
uint16_t thermalColors[] = {
  TFT_BLUE, TFT_DARKCYAN, TFT_CYAN, TFT_GREEN, 
  TFT_GREENYELLOW, TFT_YELLOW, TFT_ORANGE, TFT_RED
};
#define COLOR_COUNT 8

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32 MLX90640 Thermal Imaging Camera");
  
  // 初始化TFT显示屏
  initializeDisplay();
  
  // 初始化MLX90640传感器
  if (!initializeMLX90640()) {
    Serial.println("MLX90640初始化失败!");
    while(1) delay(1000);
  }
  
  // 显示启动界面
  showStartupScreen();
}

void initializeDisplay() {
  tft.init();
  tft.setRotation(3);  // 根据显示屏方向调整 (0-3)
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  Serial.println("TFT显示屏初始化完成");
}

bool initializeMLX90640() {
  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("MLX90640未找到!");
    return false;
  }
  
  Serial.println("MLX90640传感器找到!");
  
  // 设置传感器参数
  mlx.setMode(MLX90640_INTERLEAVED);
  mlx.setResolution(MLX90640_ADC_18BIT);
  mlx.setRefreshRate(MLX90640_8_HZ);
  
  Serial.println("MLX90640配置完成");
  return true;
}

void showStartupScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.setCursor(20, 50);
  tft.println("Thermal Camera");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(30, 80);
  tft.println("ESP32 + MLX90640");
  
  tft.setCursor(40, 100);
  tft.println("Initializing...");
  
  delay(2000);
}

void loop() {
  // 读取热成像数据
  if (readThermalData()) {
    // 处理温度数据
    processTemperatureData();
    
    // 显示热成像图像
    displayThermalImage();
    
    // 显示温度刻度
    displayTemperatureScale();
    
    // 显示统计数据
    displayStatistics();
    
    // 串口输出温度数据（可选）
    printSerialData();
  }
  
  delay(100);  // 控制刷新率
}

bool readThermalData() {
  if (mlx.getFrame(mlx90640Frame) != 0) {
    Serial.println("读取热成像数据失败");
    return false;
  }
  return true;
}

void processTemperatureData() {
  // 自动调整温度范围
  autoAdjustTemperatureRange();
}

void autoAdjustTemperatureRange() {
  float currentMin = 100.0;
  float currentMax = -100.0;
  
  // 查找当前帧的最小和最大温度
  for (int i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
    float temp = mlx90640Frame[i];
    if (temp < currentMin) currentMin = temp;
    if (temp > currentMax) currentMax = temp;
  }
  
  // 平滑调整温度范围
  minTemp = minTemp * 0.95 + currentMin * 0.05;
  maxTemp = maxTemp * 0.95 + currentMax * 0.05;
  
  // 确保最小温差
  if (maxTemp - minTemp < 5.0) {
    maxTemp = minTemp + 5.0;
  }
}

void displayThermalImage() {
  int displayWidth = tft.width();
  int displayHeight = tft.height() - 20;  // 为状态栏留出空间
  int pixelWidth = displayWidth / IMAGE_WIDTH;
  int pixelHeight = displayHeight / IMAGE_HEIGHT;
  
  for (int y = 0; y < IMAGE_HEIGHT; y++) {
    for (int x = 0; x < IMAGE_WIDTH; x++) {
      float temperature = mlx90640Frame[y * IMAGE_WIDTH + x];
      
      // 将温度映射到颜色
      uint16_t color = temperatureToColor(temperature);
      
      // 绘制像素块（放大显示）
      tft.fillRect(x * pixelWidth, y * pixelHeight, 
                   pixelWidth, pixelHeight, color);
    }
  }
}

uint16_t temperatureToColor(float temp) {
  // 将温度归一化到0-1范围
  float normalized = (temp - minTemp) / (maxTemp - minTemp);
  normalized = constrain(normalized, 0.0, 1.0);
  
  // 映射到颜色索引
  int colorIndex = (int)(normalized * (COLOR_COUNT - 1));
  colorIndex = constrain(colorIndex, 0, COLOR_COUNT - 1);
  
  return thermalColors[colorIndex];
}

void displayTemperatureScale() {
  int scaleX = 5;
  int scaleY = tft.height() - 15;
  int scaleWidth = tft.width() - 10;
  int scaleHeight = 10;
  
  // 绘制温度刻度条
  for (int i = 0; i < scaleWidth; i++) {
    float normalized = (float)i / scaleWidth;
    float temp = minTemp + normalized * (maxTemp - minTemp);
    uint16_t color = temperatureToColor(temp);
    tft.drawFastVLine(scaleX + i, scaleY, scaleHeight, color);
  }
  
  // 显示温度数值
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  
  tft.setCursor(scaleX, scaleY + scaleHeight + 1);
  tft.printf("%.1fC", minTemp);
  
  tft.setCursor(scaleX + scaleWidth - 20, scaleY + scaleHeight + 1);
  tft.printf("%.1fC", maxTemp);
}

void displayStatistics() {
  // 计算平均温度和热点温度
  float sumTemp = 0;
  float maxTempInFrame = -100;
  float minTempInFrame = 100;
  
  for (int i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
    float temp = mlx90640Frame[i];
    sumTemp += temp;
    if (temp > maxTempInFrame) maxTempInFrame = temp;
    if (temp < minTempInFrame) minTempInFrame = temp;
  }
  
  float avgTemp = sumTemp / (IMAGE_WIDTH * IMAGE_HEIGHT);
  
  // 在屏幕顶部显示统计信息
  tft.fillRect(0, 0, tft.width(), 15, TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  
  tft.setCursor(5, 2);
  tft.printf("Avg:%.1fC", avgTemp);
  
  tft.setCursor(70, 2);
  tft.printf("Max:%.1fC", maxTempInFrame);
  
  tft.setCursor(130, 2);
  tft.printf("Min:%.1fC", minTempInFrame);
}

void printSerialData() {
  // 可选：在串口监视器输出温度数据
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.printf("温度范围: %.1fC - %.1fC\n", minTemp, maxTemp);
    lastPrint = millis();
  }
}
