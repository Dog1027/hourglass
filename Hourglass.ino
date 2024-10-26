#include <SPI.h>

#define MAX7219_PIN_CS   SS   // 10
#define MAX7219_PIN_DIN  MOSI // 11
#define MAX7219_PIN_CLK  SCK  // 13
#define MAX7219_PIN_LOAD MAX7219_PIN_CS

#define ADXL335_PIN_X A5
#define ADXL335_PIN_Y A6
#define ADXL335_PIN_Z A7

#define FRAME_DELAY 100
#define DROP_DELAY_TICK 6

enum Max7219Address {
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

PROGMEM const byte MATRIX[2][8] = { //初始圖案
  { // 上
    0b00000001, //1
    0b00000011, //2
    0b00000111, //3
    0b00001111, //4
    0b00011111, //5
    0b00111111, //6
    0b01111111, //7
    0b11111111, //8
  },
  { // 下
    0b00000000, //1
    0b00000000, //2
    0b00000000, //3
    0b00000000, //4
    0b00000000, //5
    0b00000000, //6
    0b00000000, //7
    0b00000000, //8
  },
};

void max7219Config(Max7219Address addr, byte data, byte num) {
  digitalWrite(MAX7219_PIN_LOAD, LOW);
  for (int i=0; i<num; i++) {
    SPI.transfer(addr); // 暫存器位址
    SPI.transfer(data); // 資料
  }
  digitalWrite(MAX7219_PIN_LOAD, HIGH);
}

void max7219SendData(byte addr, byte *data, byte num) {
  digitalWrite(MAX7219_PIN_LOAD, LOW);
  for (int i=0; i<num; i++) {
    SPI.transfer(addr); // 暫存器位址
    SPI.transfer(data[i]); // 資料
  }
  digitalWrite(MAX7219_PIN_LOAD, HIGH);
}

enum Direction {
  TOP_LEFT,
  TOP,
  TOP_RIGHT,
  RIGHT,
  BOTTOM_RIGHT,
  BOTTOM,
  BOTTOM_LEFT,
  LEFT,
};

class HourGlass {
  byte matrix[2][8];
  bool pixelRead(byte row, byte col, byte bottomOrTop);
  void pixelWrite(byte row, byte col, byte bottomOrTop, bool state);
  bool pixelGetNewPosition(byte& row, byte& col, byte& bottomOrTop, char direct);
  void pixelMove(byte row, byte col, byte bottomOrTop, char direct, bool clockwise);
public:
  HourGlass();
  void init();
  void draw();
  void clear();
  void getDirect(char& direct, bool& clockwise);
  void matrixUpdate(char direct, bool clockwise);
  void debug();
};

HourGlass::HourGlass() {
  memcpy_P(matrix, MATRIX, sizeof(MATRIX));
}

void HourGlass::init() {
  max7219Config(DECODE, 0, 2);
  max7219Config(INTENSITY, 0, 2);
  max7219Config(SCAN_LIMIT, 7, 2);
  max7219Config(SHUTDOWN, 1, 2);
  max7219Config(DISPLAY_TEST, 0, 2);
}

void HourGlass::draw() {
  for (int i=0; i<8; i++) { // 第一排到第八排
    byte data[2] = {matrix[0][i], matrix[1][i]};
    max7219SendData(DIGIT_0+i, data, 2);
  }
}

void HourGlass::clear() {
  for (int i=0; i<8; i++) { // 第一排到第八排
    byte data[2] = {0};
    max7219SendData(DIGIT_0+i, data, 2);
  }
}

bool HourGlass::pixelRead(byte row, byte col, byte bottomOrTop) {
  return bitRead(matrix[bottomOrTop][row-1], 8-col);
}

void HourGlass::pixelWrite(byte row, byte col, byte bottomOrTop, bool state) {
  bitWrite(matrix[bottomOrTop][row-1], 8-col, state);
}

bool HourGlass::pixelGetNewPosition(byte& row, byte& col, byte& bottomOrTop, char direct) {
  static byte tickTopToBottom = 0;
  static byte tickBottomToTop = 0;
  byte _row = row;
  byte _col = col;

  if (!(direct==TOP_LEFT || direct==TOP || direct==TOP_RIGHT)) tickTopToBottom = 0;
  if (!(direct==BOTTOM_RIGHT || direct==BOTTOM || direct==BOTTOM_LEFT)) tickBottomToTop = 0;

  switch (direct) {
    case TOP_LEFT:
      _col--;
      break;
    case TOP:
      _row--;
      _col--;
      break;
    case TOP_RIGHT:
      _row--;
      break;
    case RIGHT:
      _row--;
      _col++;
      break;
    case BOTTOM_RIGHT:
      _col++;
      break;
    case BOTTOM:;
      _row++;
      _col++;
      break;
    case BOTTOM_LEFT:
      _row++;
      break;
    case LEFT:
      _row++;
      _col--;
      break;
  }

  if (_row>8 || _col>8 || _row<1 || _col<1) {
    if (_row>8 && _col>8 && bottomOrTop==1) { //下到上
      if (tickBottomToTop < DROP_DELAY_TICK) {
        tickBottomToTop++;
        return false;
      }
      else if (pixelRead(1, 1, 0)) { // 位置是否被占用
        return false;
      }
      else {
        row = 1;
        col = 1;
        bottomOrTop = 0;
        tickBottomToTop = 0;
        return true;
      }
    }
    else if (_row<1 && _col<1 && bottomOrTop==0){ // 上到下
      if (tickTopToBottom < DROP_DELAY_TICK) {
        tickTopToBottom++;
        return false;
      }
      else if (pixelRead(8, 8, 1)) { // 位置是否被占用
        return false;
      }
      else {
        row = 8;
        col = 8;
        bottomOrTop = 1;
        tickTopToBottom = 0;
        return true;
      }
    }
    else{
      return false;
    }
  }
  else if (pixelRead(_row, _col, bottomOrTop)) {
    return false;
  }
  else {
    row = _row;
    col = _col;
    return true;
  }
}

void HourGlass::debug() {
  int x = 0;
  for (int i=1; i<=8; i++) {
    for (int j=1; j<=8; j++) {
      if (pixelRead(i, j, 0))
        x++;
      if (pixelRead(i, j, 1))
        x++;
    }
  }
  Serial.print("x: ");
  Serial.println(x);
}

void HourGlass::pixelMove(byte row, byte col, byte bottomOrTop, char direct, bool clockwise) {
  if (!pixelRead(row, col, bottomOrTop))
    return;

  pixelWrite(row, col, bottomOrTop, 0);
  if (pixelGetNewPosition(row, col, bottomOrTop, direct)) {//取得移動方向座標
    pixelWrite(row, col, bottomOrTop, 1);
  }
  else if (!clockwise) {
    if (pixelGetNewPosition(row, col, bottomOrTop, direct+1>7 ? 0 : direct+1)) {
      pixelWrite(row, col, bottomOrTop, 1);
    }
    else if (pixelGetNewPosition(row, col, bottomOrTop, direct-1<0 ? 7 : direct-1)) {
      pixelWrite(row, col, bottomOrTop, 1);
    }
    else {
      pixelWrite(row, col, bottomOrTop, 1);
    }
  }
  else {
    if (pixelGetNewPosition(row, col, bottomOrTop, direct-1<0 ? 7 : direct-1)) {
      pixelWrite(row, col, bottomOrTop, 1);
    }
    else if (pixelGetNewPosition(row, col, bottomOrTop, direct+1> 7 ? 0 : direct+1)) {
      pixelWrite(row, col, bottomOrTop, 1);
    }
    else {
      pixelWrite(row, col, bottomOrTop, 1);
    }
  }
}

/*
void HourGlass::matrixUpdate(char direct, bool clockwise) {
  for (int i=1; i<=8; i++) {
    for (int j=1; j<=8; j++) {
        pixelMove(i, j, 0, direct, clockwise);
        pixelMove(i, j, 1, direct, clockwise);
    }
  }
}*/

void HourGlass::getDirect(char& direct, bool& clockwise) {
  const int x_max = 406;
  const int x_min = 273;
  const int x_mid = (x_max + x_min) / 2;
  const int y_max = 400;
  const int y_min = 275;
  const int y_mid = (y_max + y_min) / 2;//328;
  const int z_max_l = 405;//z不能大於z_max_l
  const int z_min_l = 305;//z不能小於z_min_l
  const int x_22d5 = (x_max - x_mid) * cos(22.5 * PI / 180);
  const int y_22d5 = (y_max - y_mid) * sin(22.5 * PI / 180);
  const int x_67d5 = (x_max - x_mid) * cos(67.5 * PI / 180);
  const int y_67d5 = (y_max - y_mid) * sin(67.5 * PI / 180);

  int x = analogRead(ADXL335_PIN_X);
  int y = analogRead(ADXL335_PIN_Y);
  int z = analogRead(ADXL335_PIN_Z);
  
  Serial.print("x:");
  Serial.print(x);
  Serial.print(", y:");
  Serial.print(y);
  Serial.print(", z:");
  Serial.println(z);

  if (z<z_max_l && z>z_min_l) {
    if (x > x_mid-x_22d5 && x < x_mid-x_67d5 && y > y_mid-y_67d5 && y < y_mid-y_22d5)
      direct = TOP_RIGHT;
    else if (x < x_mid-x_22d5 && y > y_mid-y_22d5 && y < y_mid+y_22d5)
      direct = RIGHT;
    else if (x > x_mid-x_22d5 && x < x_mid-x_67d5 && y < y_mid+y_67d5 && y > y_mid+y_22d5)
      direct = BOTTOM_RIGHT;
    else if (x > x_mid-x_67d5 && x < x_mid+x_67d5 && y > y_mid+y_67d5)
      direct = BOTTOM;
    else if (x < x_mid+x_22d5 && x > x_mid+x_67d5 && y < y_mid+y_67d5 && y > y_mid+y_22d5)
      direct = BOTTOM_LEFT;
    else if (x > x_mid+x_22d5 && y > y_mid-y_22d5 && y < y_mid+y_22d5)
      direct = LEFT;
    else if (x < x_mid+x_22d5 && x > x_mid+x_67d5 && y > y_mid-y_67d5 && y < y_mid-y_22d5)
      direct = TOP_LEFT;
    else if (x > x_mid-x_67d5 && x < x_mid+x_67d5 && y < y_mid-y_67d5)
      direct = TOP;
  }

  switch (direct) {
    case TOP_LEFT:
      Serial.println("TOP_LEFT");
      break;
    case TOP:
      Serial.println("TOP");
      break;
    case TOP_RIGHT:
      Serial.println("TOP_RIGHT");
      break;
    case RIGHT:
      Serial.println("RIGHT");
      break;
    case BOTTOM_RIGHT:
      Serial.println("BOTTOM_RIGHT");
      break;
    case BOTTOM:;
      Serial.println("BOTTOM");
      break;
    case BOTTOM_LEFT:
      Serial.println("BOTTOM_LEFT");
      break;
    case LEFT:
      Serial.println("LEFT");
      break;
  }

  clockwise = ((x>x_mid && y<y_mid) || (x<x_mid && y>y_mid)) ? 0 : 1;
}

void HourGlass::matrixUpdate(char direct, bool clockwise) {
  switch (direct) {
    case TOP_LEFT:
      for (int k=1; k>=0; k--) {
        for (int i=1; i<=8; i++) {
          for (int j=1; j<=8; j++) {
            if (!clockwise)
              pixelMove(j, i, k, direct, clockwise);
            else
              pixelMove(9-j, i, k, direct, clockwise);
          }
        }
      }
      break;
    case TOP:
      for (int k=1; k>=0; k--) {
        for (int i=2; i<=16; i++) {
          if (i <= 9) {
            for (int j=0; j<i-1; j++) {
              if (!clockwise)
                pixelMove(j+1, i-j-1, k, direct, clockwise);
              else
                pixelMove(i-j-1, j+1, k, direct, clockwise);
            }
          }
          else {
            for (int j = 0;j < 17-i;j ++) {
              if (!clockwise)
                pixelMove(i+j-8, 8-j, k, direct, clockwise);
              else
                pixelMove(8-j, i+j-8, k, direct, clockwise);
            }
          }
        }
      }
      break;
    case TOP_RIGHT:
      for (int k=1; k>=0; k--) {
        for (int i=1; i<=8; i++) {
          for (int j=1; j<=8; j++) {
            if (!clockwise)
              pixelMove(i, 9-j, k, direct, clockwise);
            else
              pixelMove(i, j, k, direct, clockwise);
          }
        }
      }
      break;
    case RIGHT:
      for (int i=1; i<=8; i++) {
        if (!clockwise) {
          for (int k=0; k<2; k++) {
            for (int j=0; j<i; j++)
              pixelMove(i-j, 8-j, k, direct, clockwise);
          }
        }
        else {
          for (int k=1; k>=0; k--) {
            for (int j=0; j<i; j++)
              pixelMove(1+j, 9-i+j, k, direct, clockwise);
          }
        }
      }
      for (int i=7; i>=1; i--) {
        if (!clockwise) {
          for (int k=0; k<2; k++) {
            for (int j=0; j<i; j++)
              pixelMove(8-j, i-j, k, direct, clockwise);
          }
        }
        else{
          for (int k = 1;k >= 0;k --) {
            for (int j = 0;j < i;j ++)
              pixelMove(9-i+j, 1+j, k, direct, clockwise);
          }
        }
      }
      break;
    case BOTTOM_RIGHT://V
      for (int k=0; k<2; k++) {
        for (int i=8; i>0; i--) {
          for (int j=1; j<=8; j++) {
            if (!clockwise)
              pixelMove(9-j, i, k, direct, clockwise);
            else
              pixelMove(j, i, k, direct, clockwise);
          }
        }
      }
      break;
    case BOTTOM://V
      for (int k=0; k<2; k++) {
        for (int i=16; i>1; i--) {
          if (i > 8) {
            for (int j=0; j<17-i; j++) {
              if (!clockwise)
                pixelMove(8-j, i+j-8, k, direct, clockwise);
              else
                pixelMove(i+j-8, 8-j, k, direct, clockwise);
            }
          }
          else {
            for (int j=0; j<i-1; j++) {
              if (!clockwise)
                pixelMove(i-j-1, 1+j, k, direct, clockwise);
              else
                pixelMove(1+j, i-j-1, k, direct, clockwise);
            }
          }
        }
      }
      break;
    case BOTTOM_LEFT://V
      for (int k=0; k<2; k++) {
        for (int i=8; i>0; i--) {
          for (int j=1; j<=8; j++) {
            if (!clockwise)
              pixelMove(i, j, k, direct, clockwise);
            else
              pixelMove(i, 9-j, k, direct, clockwise);
          }
        }
      }
      break;
    case LEFT:
      for (int i=1; i<=8; i++) {
        if (!clockwise) {
          for (int k=1; k>=0; k--) {
            for (int j=0; j<i; j++)
              pixelMove(9-i+j, 1+j, k, direct, clockwise);
          }
        }
        else {
          for (int k=0; k<2; k++) {
            for (int j=0; j<i; j++)
              pixelMove(8-j, i-j, k, direct, clockwise);
          }
        }
      }
      for (int i=7; i>=1; i--) {
        if (!clockwise) {
          for (int k=1; k>=0; k--) {
            for (int j=0; j<i; j++)
              pixelMove(1+j, 9-i+j, k, direct, clockwise);
            }
        }
        else {
          for (int k=0; k<2; k++) {
            for (int j=0; j<i; j++)
              pixelMove(i-j, 8-j, k, direct, clockwise);
            }
        }
      }
      break;
  }
}

HourGlass hourGlass;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  //SPI.setClockDivider(SPI_CLOCK_DIV8);
  
  hourGlass.init();
  hourGlass.clear();
  hourGlass.draw();
}

void loop() {
  //if (Serial.available() && Serial.read()) {
    static char direct = 0;
    bool clockwise;
    delay(FRAME_DELAY);
    hourGlass.getDirect(direct, clockwise);
    hourGlass.matrixUpdate(direct, clockwise);
    hourGlass.draw();
    //hourGlass.debug();
  //}
}
