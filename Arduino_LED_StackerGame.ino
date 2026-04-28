#include <MD_MAX72xx.h>
#include <SPI.h>

// --- 硬體與腳位定義 ---
#define MAX_DEVICES 4   // 使用 4 合 1 顯示器 (4 個 8x8 模組串聯)
#define CLK_PIN     11  // 時脈腳位
#define DATA_PIN    12  // 數據腳位
#define CS_PIN      10  // 片選腳位
#define BUTTON_PIN  2   // 按鈕腳位 (連接 GND，使用內部上拉電阻)

// 初始化矩陣對象，多數 4合1 模組使用 FC16_HW 類型
MD_MAX72XX matrix = MD_MAX72XX(MD_MAX72XX::FC16_HW, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// --- 遊戲尺寸設定 ---
#define WIDTH  8     // 顯示器寬度 (8 列)
#define HEIGHT 32    // 顯示器高度 (4 個模組 x 8 行 = 32 行)

// --- 遊戲變數 ---
int blockPos = 0;           // 移動方塊的最左側 X 座標
int BLOCK_WIDTH = 4;        // 方塊當前寬度 (隨遊戲進行縮小)
int blockRow = 0;           // 當上方塊所在的行數 (0 為最底部)
bool blockMoving = true;    // 方塊是否正在左右移動
int stack[HEIGHT][WIDTH];   // 儲存已固定方塊的狀態 (1:有燈, 0:無燈)
int stackHeight = 0;        // 目前堆疊的最高有效高度
bool gameOver = false;      // 遊戲結束旗標
unsigned long lastMove = 0; // 紀錄上次方塊移動的時間
int moveDelay = 200;        // 方塊移動的速度 (毫秒)，越小越快
bool moveRight = true;      // 目前移動的方向 (向右或向左)

// --- 偵錯與測試模式 ---
bool testMode = false;      // 測試模式開關
int testRow = 0;            // 測試模式中顯示的行號
const bool ROTATE_DISPLAY = true; // 旋轉校正 (針對 FC-16 模組縱向擺放調整)

void setup() {
  Serial.begin(9600);
  Serial.println(F("遊戲啟動中..."));

  matrix.begin();           // 初始化矩陣
  matrix.clear();           // 清空畫面

  pinMode(BUTTON_PIN, INPUT_PULLUP); // 設定按鈕為輸入模式
  randomSeed(analogRead(A0));        // 讀取懸空腳位產生隨機亂數種子
  
  resetGame();
  Serial.println(F("遊戲已就緒。在序列埠輸入 't' 可切換測試模式。"));
}

// 重置遊戲狀態
void resetGame() {
  BLOCK_WIDTH = 4;          // 回復初始方塊寬度
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      stack[row][col] = 0;  // 清空地圖陣列
    }
  }
  blockPos = random(0, WIDTH - BLOCK_WIDTH + 1); // 隨機初始位置
  blockRow = 0;             // 從最底層開始
  stackHeight = 0;
  gameOver = false;
  blockMoving = true;
  moveDelay = 200;          // 初始速度
  moveRight = true;
  testMode = false;
  updateDisplay();
  Serial.println(F("遊戲重置完成"));
}

// 更新 LED 顯示畫面
void updateDisplay() {
  matrix.clear();

  // 若在測試模式，僅亮起該行以確認硬體方向
  if (testMode) {
    for (int col = 0; col < WIDTH; col++) {
      ROTATE_DISPLAY ? matrix.setPoint(col, testRow, true) : matrix.setPoint(testRow, col, true);
    }
    return;
  }

  // 1. 繪製已經固定在地圖上的方塊 (Stack)
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      if (stack[row][col]) {
        ROTATE_DISPLAY ? matrix.setPoint(col, row, true) : matrix.setPoint(row, col, true);
      }
    }
  }

  // 2. 繪製正在左右移動的方塊
  if (!gameOver && blockMoving) {
    for (int col = blockPos; col < blockPos + BLOCK_WIDTH; col++) {
      if (col >= 0 && col < WIDTH) {
        ROTATE_DISPLAY ? matrix.setPoint(col, blockRow, true) : matrix.setPoint(blockRow, col, true);
      }
    }
  }
}

// 按下按鈕，固定方塊位置
void dropBlock() {
  blockMoving = false;
  int currentLayerCount = 0; // 計算此層成功對齊的數量

  // 核心邏輯：檢查目前移動方塊的每一格點位
  for (int col = blockPos; col < blockPos + BLOCK_WIDTH; col++) {
    // 條件：若是第 0 層(最底層)直接固定；或是其下方一格已經有方塊支撐
    if (blockRow == 0 || stack[blockRow - 1][col] == 1) {
      stack[blockRow][col] = 1;
      currentLayerCount++; // 成功對齊
    }
  }

  // 判斷是否完全沒對齊
  if (currentLayerCount == 0) {
    gameOver = true;
    Serial.println(F("遊戲結束：未對齊下方方塊！"));
    return;
  }

  // 成功對齊後，更新遊戲數值
  BLOCK_WIDTH = currentLayerCount; // 重要：方塊變窄了 (剩下有對齊的部分)
  stackHeight = blockRow + 1;      // 更新目前堆疊高度

  // 檢查是否贏得遊戲 (到達最高層)
  if (stackHeight >= HEIGHT) {
    gameOver = true;
    Serial.println(F("恭喜破關！已堆疊至頂端。"));
    return;
  }

  // 進入下一層準備
  blockRow++; 
  blockPos = random(0, WIDTH - BLOCK_WIDTH + 1); // 隨機產生下一層移動位置
  blockMoving = true;

  // 加快速度，增加遊戲難度
  moveDelay = max(50, moveDelay - 10); 

  Serial.print(F("第 ")); Serial.print(blockRow - 1);
  Serial.print(F(" 層完成。目前剩餘寬度: ")); Serial.println(BLOCK_WIDTH);
}

void loop() {
  // --- 序列埠輸入處理 (偵錯用) ---
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't') { // 切換測試模式
      testMode = !testMode;
      if (testMode) {
        testRow = 0;
        Serial.println(F("進入測試模式。按 'n' 或按鈕換行，'t' 退出。"));
      } else {
        resetGame();
      }
    } else if (testMode && c == 'n') { // 測試模式下一行
      testRow = (testRow + 1) % HEIGHT;
    }
    updateDisplay();
  }

  // 測試模式下不執行遊戲邏輯
  if (testMode) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50); // 防彈跳
      if (digitalRead(BUTTON_PIN) == LOW) {
        testRow = (testRow + 1) % HEIGHT;
        updateDisplay();
        while (digitalRead(BUTTON_PIN) == LOW); // 等待按鈕放開
      }
    }
    return;
  }

  // --- 遊戲結束狀態 ---
  if (gameOver) {
    // 閃爍效果
    matrix.clear(); delay(500);
    updateDisplay(); delay(500);
    
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if (digitalRead(BUTTON_PIN) == LOW) {
        resetGame();
        while (digitalRead(BUTTON_PIN) == LOW);
      }
    }
    return;
  }

  // --- 方塊移動邏輯 ---
  if (blockMoving && millis() - lastMove >= moveDelay) {
    blockPos += moveRight ? 1 : -1;
    
    // 碰到邊界時反彈
    if (blockPos <= 0) {
      blockPos = 0;
      moveRight = true;
    } else if (blockPos >= WIDTH - BLOCK_WIDTH) {
      blockPos = WIDTH - BLOCK_WIDTH;
      moveRight = false;
    }
    
    lastMove = millis();
    updateDisplay();
  }

  // --- 按鈕偵測 ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // 防彈跳
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println(F("按鈕按下！"));
      dropBlock();
      updateDisplay();
      while (digitalRead(BUTTON_PIN) == LOW); // 防止連擊
    }
  }
}