#include <SPI.h>

// ===================== 腳位 =====================
// MAX7219：DIN = MOSI (D11)、CLK = SCK (D13)、CS/LOAD = SS (D10)
#define MAX7219_PIN_LOAD SS

// ADXL335：A6 / A7 只有 Nano、Pro Mini 等板子才有，Uno 沒有
#define ADXL335_PIN_X A5
#define ADXL335_PIN_Y A6
#define ADXL335_PIN_Z A7

// ===================== 時序 =====================
#define FRAME_DELAY     100 // 每一幀的間隔 (ms)
#define DROP_DELAY_TICK 6   // 沙粒穿過瓶頸前要等待的次數，越大漏得越慢

// ============ 加速度計校正值 (analogRead 讀值) ============
// 換感測器或供電電壓後，請用 Serial 監看讀值重新量測
#define ACCEL_X_MAX 406
#define ACCEL_X_MIN 273
#define ACCEL_Y_MAX 400
#define ACCEL_Y_MIN 275
#define ACCEL_Z_MAX 405 // z 介於 Z_MIN 與 Z_MAX 之間（大致直立）時才更新方向
#define ACCEL_Z_MIN 305

// 1 = 每幀從 Serial 印出加速度計讀值與方向，0 = 關閉
#define DEBUG_SERIAL 1

// ===================== MAX7219 =====================
enum Max7219Address : byte {
  NO_OP,
  DIGIT_0,
  DIGIT_1,
  DIGIT_2,
  DIGIT_3,
  DIGIT_4,
  DIGIT_5,
  DIGIT_6,
  DIGIT_7,
  DECODE,
  INTENSITY,
  SCAN_LIMIT,
  SHUTDOWN,
  DISPLAY_TEST = 0x0F,
};

const byte MATRIX_COUNT = 2; // 串接的 MAX7219 數量

// 同一個暫存器寫入相同資料到每一片 MAX7219
void max7219Config(Max7219Address addr, byte data) {
  digitalWrite(MAX7219_PIN_LOAD, LOW);
  for (byte i = 0; i < MATRIX_COUNT; i++) {
    SPI.transfer(addr);
    SPI.transfer(data);
  }
  digitalWrite(MAX7219_PIN_LOAD, HIGH);
}

// 同一個暫存器寫入不同資料，data[0] 會送到串接鏈上最遠的那一片
void max7219SendData(byte addr, const byte* data) {
  digitalWrite(MAX7219_PIN_LOAD, LOW);
  for (byte i = 0; i < MATRIX_COUNT; i++) {
    SPI.transfer(addr);
    SPI.transfer(data[i]);
  }
  digitalWrite(MAX7219_PIN_LOAD, HIGH);
}

// ===================== 方向 =====================
// 依順時針排列，相鄰的值相差 45°
enum Direction : byte {
  TOP_LEFT,
  TOP,
  TOP_RIGHT,
  RIGHT,
  BOTTOM_RIGHT,
  BOTTOM,
  BOTTOM_LEFT,
  LEFT,
};

// 每個方向在矩陣上的 (row, col) 位移
const int8_t DIR_ROW[8] = { 0, -1, -1, -1, 0, 1, 1,  1 };
const int8_t DIR_COL[8] = {-1, -1,  0,  1, 1, 1, 0, -1 };

const char* const DIRECTION_NAMES[8] = {
  "TOP_LEFT", "TOP", "TOP_RIGHT", "RIGHT",
  "BOTTOM_RIGHT", "BOTTOM", "BOTTOM_LEFT", "LEFT",
};

// 旋轉 steps 個 45°（正數為順時針）
Direction rotate(Direction dir, int8_t steps) {
  return (Direction)((dir + steps) & 7);
}

// 沿沙漏軸線（上片 → 下片）的分量：< 0 往下片、> 0 往上片、0 = 橫放
int8_t axisComponent(Direction dir) {
  return DIR_ROW[dir] + DIR_COL[dir];
}

// ===================== 沙漏 =====================
// 兩片 8x8 矩陣以角落相接：上片的 (0,0) 接下片的 (7,7)
enum Layer : byte {
  UPPER = 0, // 上片（串接鏈上較遠的那片）
  LOWER = 1, // 下片
};

PROGMEM const byte INITIAL_MATRIX[2][8] = {
  { // 上片：36 顆沙
    0b00000001,
    0b00000011,
    0b00000111,
    0b00001111,
    0b00011111,
    0b00111111,
    0b01111111,
    0b11111111,
  },
  { // 下片：空的
    0, 0, 0, 0, 0, 0, 0, 0,
  },
};

class HourGlass {
public:
  HourGlass();
  void init();
  void draw();
  void clear();
  void update(Direction gravity, bool clockwise);
  byte countGrains();

private:
  byte matrix[2][8];  // matrix[layer][row]，bit 7 = col 0
  byte waitToLower = 0; // 上片 → 下片 的等待計數
  byte waitToUpper = 0; // 下片 → 上片 的等待計數

  static bool inBounds(int8_t v) { return v >= 0 && v < 8; }
  bool pixelRead(int8_t row, int8_t col, Layer layer);
  void pixelWrite(int8_t row, int8_t col, Layer layer, bool state);
  bool tryMove(int8_t& row, int8_t& col, Layer& layer, Direction dir);
  bool passNeck(byte& wait, int8_t& row, int8_t& col, Layer& layer, Layer target, int8_t corner);
  void pixelMove(int8_t row, int8_t col, Layer layer, Direction dir, bool clockwise);
};

HourGlass::HourGlass() {
  memcpy_P(matrix, INITIAL_MATRIX, sizeof(matrix));
}

void HourGlass::init() {
  max7219Config(DECODE, 0);
  max7219Config(INTENSITY, 0);
  max7219Config(SCAN_LIMIT, 7);
  max7219Config(SHUTDOWN, 1);
  max7219Config(DISPLAY_TEST, 0);
}

void HourGlass::draw() {
  for (byte row = 0; row < 8; row++) {
    const byte data[MATRIX_COUNT] = { matrix[UPPER][row], matrix[LOWER][row] };
    max7219SendData(DIGIT_0 + row, data);
  }
}

void HourGlass::clear() {
  const byte data[MATRIX_COUNT] = { 0 };
  for (byte row = 0; row < 8; row++) {
    max7219SendData(DIGIT_0 + row, data);
  }
}

byte HourGlass::countGrains() {
  byte n = 0;
  for (byte layer = 0; layer < 2; layer++)
    for (byte row = 0; row < 8; row++)
      for (byte bits = matrix[layer][row]; bits; bits &= bits - 1)
        n++;
  return n;
}

bool HourGlass::pixelRead(int8_t row, int8_t col, Layer layer) {
  return bitRead(matrix[layer][row], 7 - col);
}

void HourGlass::pixelWrite(int8_t row, int8_t col, Layer layer, bool state) {
  bitWrite(matrix[layer][row], 7 - col, state);
}

// 沙粒穿過瓶頸：先等待 DROP_DELAY_TICK 次，目標角落是空的才過去
bool HourGlass::passNeck(byte& wait, int8_t& row, int8_t& col, Layer& layer, Layer target, int8_t corner) {
  if (wait < DROP_DELAY_TICK) {
    wait++;
    return false;
  }
  if (pixelRead(corner, corner, target))
    return false;
  row = col = corner;
  layer = target;
  wait = 0;
  return true;
}

// 嘗試往 dir 移動一格，成功時更新座標並回傳 true
bool HourGlass::tryMove(int8_t& row, int8_t& col, Layer& layer, Direction dir) {
  // 不是往瓶頸方向移動時，重設該方向的等待計數
  const int8_t axis = axisComponent(dir);
  if (axis >= 0) waitToLower = 0;
  if (axis <= 0) waitToUpper = 0;

  const int8_t newRow = row + DIR_ROW[dir];
  const int8_t newCol = col + DIR_COL[dir];

  if (inBounds(newRow) && inBounds(newCol)) {
    if (pixelRead(newRow, newCol, layer))
      return false;
    row = newRow;
    col = newCol;
    return true;
  }

  // 只有從角落斜著出去才會穿過瓶頸
  if (newRow < 0 && newCol < 0 && layer == UPPER)
    return passNeck(waitToLower, row, col, layer, LOWER, 7);
  if (newRow > 7 && newCol > 7 && layer == LOWER)
    return passNeck(waitToUpper, row, col, layer, UPPER, 0);
  return false;
}

// 移動一顆沙：先試正前方，走不通再試左右偏 45°（順序依 clockwise 決定）
void HourGlass::pixelMove(int8_t row, int8_t col, Layer layer, Direction dir, bool clockwise) {
  if (!pixelRead(row, col, layer))
    return;

  const int8_t side = clockwise ? -1 : 1;
  pixelWrite(row, col, layer, false);
  if (!tryMove(row, col, layer, dir) &&
      !tryMove(row, col, layer, rotate(dir, side)))
    tryMove(row, col, layer, rotate(dir, -side));
  pixelWrite(row, col, layer, true); // 三個方向都走不通時就寫回原位
}

// 掃描順序的排序鍵：越靠重力前方的格子值越大；同一排再依 clockwise 決定方向
static int16_t scanKey(byte cell, Direction gravity, bool clockwise) {
  const int8_t row = cell >> 3;
  const int8_t col = cell & 7;
  const int8_t along  = row * DIR_ROW[gravity] + col * DIR_COL[gravity];
  const int8_t across = row * DIR_COL[gravity] - col * DIR_ROW[gravity];
  return along * 32 + (clockwise ? -across : across);
}

void HourGlass::update(Direction gravity, bool clockwise) {
  // 由重力前方往後處理，已經移動過的沙不會在同一幀再被移動
  byte order[64];
  for (byte i = 0; i < 64; i++) {
    const int16_t key = scanKey(i, gravity, clockwise);
    byte j = i;
    for (; j > 0 && scanKey(order[j - 1], gravity, clockwise) < key; j--)
      order[j] = order[j - 1];
    order[j] = i;
  }

  // 先處理接收沙子的那一片，剛穿過瓶頸的沙子這一幀就不會再被移動
  const Layer first  = axisComponent(gravity) < 0 ? LOWER : UPPER;
  const Layer second = first == LOWER ? UPPER : LOWER;
  for (byte i = 0; i < 64; i++)
    pixelMove(order[i] >> 3, order[i] & 7, first, gravity, clockwise);
  for (byte i = 0; i < 64; i++)
    pixelMove(order[i] >> 3, order[i] & 7, second, gravity, clockwise);
}

// ===================== 加速度計 =====================
const int ACCEL_X_MID = (ACCEL_X_MAX + ACCEL_X_MIN) / 2;
const int ACCEL_Y_MID = (ACCEL_Y_MAX + ACCEL_Y_MIN) / 2;
// 22.5° 與 67.5° 的分界值
const int X_INNER = (ACCEL_X_MAX - ACCEL_X_MID) * cos(67.5 * PI / 180);
const int X_OUTER = (ACCEL_X_MAX - ACCEL_X_MID) * cos(22.5 * PI / 180);
const int Y_INNER = (ACCEL_Y_MAX - ACCEL_Y_MID) * sin(22.5 * PI / 180);
const int Y_OUTER = (ACCEL_Y_MAX - ACCEL_Y_MID) * sin(67.5 * PI / 180);

const int8_t BAND_INVALID = 99;

// 把偏移量分成 -2 ~ 2 五個區間，剛好落在分界上視為無效
int8_t band(int d, int inner, int outer) {
  const int a = abs(d);
  const int8_t sign = d < 0 ? -1 : 1;
  if (a < inner) return 0;
  if (a > inner && a < outer) return sign;
  if (a > outer) return 2 * sign;
  return BAND_INVALID;
}

// 依 x、y 偏移量判斷重力方向，判斷不出來時回傳 false
bool classifyGravity(int dx, int dy, Direction& dir) {
  const int8_t bx = band(dx, X_INNER, X_OUTER);
  const int8_t by = band(dy, Y_INNER, Y_OUTER);
  if      (bx == -1 && by == -1) dir = TOP_RIGHT;
  else if (bx == -2 && by ==  0) dir = RIGHT;
  else if (bx == -1 && by ==  1) dir = BOTTOM_RIGHT;
  else if (bx ==  0 && by ==  2) dir = BOTTOM;
  else if (bx ==  1 && by ==  1) dir = BOTTOM_LEFT;
  else if (bx ==  2 && by ==  0) dir = LEFT;
  else if (bx ==  1 && by == -1) dir = TOP_LEFT;
  else if (bx ==  0 && by == -2) dir = TOP;
  else return false;
  return true;
}

// 讀取加速度計並更新重力方向；不是直立或判斷不出來時沿用原本的方向
void readGravity(Direction& gravity, bool& clockwise) {
  const int x = analogRead(ADXL335_PIN_X);
  const int y = analogRead(ADXL335_PIN_Y);
  const int z = analogRead(ADXL335_PIN_Z);
  const int dx = x - ACCEL_X_MID;
  const int dy = y - ACCEL_Y_MID;

  if (z > ACCEL_Z_MIN && z < ACCEL_Z_MAX)
    classifyGravity(dx, dy, gravity);
  clockwise = !((dx > 0 && dy < 0) || (dx < 0 && dy > 0));

#if DEBUG_SERIAL
  Serial.print("x:");
  Serial.print(x);
  Serial.print(", y:");
  Serial.print(y);
  Serial.print(", z:");
  Serial.println(z);
  Serial.println(DIRECTION_NAMES[gravity]);
#endif
}

// ===================== 主程式 =====================
HourGlass hourGlass;

void setup() {
  Serial.begin(115200);
  SPI.begin();

  hourGlass.init();
  hourGlass.clear();
  hourGlass.draw();
}

void loop() {
  static Direction gravity = TOP_LEFT;
  bool clockwise;

  delay(FRAME_DELAY);
  readGravity(gravity, clockwise);
  hourGlass.update(gravity, clockwise);
  hourGlass.draw();
}
