#include <SPI.h>

#define MAX7219_SS SS // 10

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
  DISPLAY_TEST,
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

void max7219Select(bool on) {
  if (on)
    digitalWrite(MAX7219_SS, LOW);  // 選取晶片
  else
    digitalWrite(MAX7219_SS, HIGH); // 取消選取晶片
}

void max7219Transfer(byte addr, byte data) {
  SPI.transfer(addr); // 暫存器位址
  SPI.transfer(data); // 資料
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
  byte (*matrix)[8];
  byte (*matrixLast)[8];
  byte _matrix[2][2][8];
  void getDirect(byte direct, byte clockwise);
  bool pixelRead(byte row, byte col, byte bottomOrTop);
  void pixelWrite(byte row, byte col, byte bottomOrTop, bool state);
  bool pixelGetNewPosition(byte& row, byte& col, byte& bottomOrTop, byte direct);
  void pixelMove(byte row, byte col, byte bottomOrTop, byte direct, bool clockwise);
public:
  HourGlass();
  void init();
  void draw();
  void clear();
};

HourGlass::HourGlass() {
  matrix = _matrix[0];
  matrixLast = _matrix[1];
  memcpy_P(matrix, MATRIX, sizeof(MATRIX));
}

void HourGlass::init() {
  max7219Select(true);
  max7219Transfer(DECODE, 0);
  max7219Transfer(INTENSITY, 1);
  max7219Transfer(SCAN_LIMIT, 7);
  max7219Transfer(SHUTDOWN, 1);
  max7219Transfer(DISPLAY_TEST, 0);
  max7219Select(false);
}

void HourGlass::draw() {
  byte (*temp)[8] = matrix;
  max7219Select(true);
  for (int i=0; i<8; i++) { // 第一排到第八排
    max7219Transfer(DIGIT_0+i, matrix[0][i]);
    max7219Transfer(DIGIT_0+i, matrix[1][i]);
  }
  matrix = matrixLast;
  matrixLast = temp;
  max7219Select(false);
}

void HourGlass::clear() {
  max7219Select(true);
  for (int i=0; i<8; i++) { // 第一排到第八排
    max7219Transfer(DIGIT_0+i, 0);
    max7219Transfer(DIGIT_0+i, 0);
  }
  max7219Select(false);
}

bool HourGlass::pixelRead(byte row, byte col, byte bottomOrTop) {
  return bitRead(matrixLast[bottomOrTop][row-1], 8-col);
}

void HourGlass::pixelWrite(byte row, byte col, byte bottomOrTop, bool state) {
  bitWrite(matrix[bottomOrTop][row-1], 8-col, state);
  //Serial.println(matrix[bottomOrTop][row-1], BIN);
}

bool HourGlass::pixelGetNewPosition(byte& row, byte& col, byte& bottomOrTop, byte direct) {
  static byte tickTopToBottom = 0;
  static byte tickBottomToTop = 0;

  if (!(direct==TOP_LEFT || direct==TOP || direct==TOP_RIGHT)) tickBottomToTop = 0;
  if (!(direct==BOTTOM_RIGHT || direct==BOTTOM || direct==BOTTOM_LEFT)) tickTopToBottom = 0;

  switch (direct) {
    case TOP_LEFT:
      col--;
      break;
    case TOP:
      row--;
      col--;
      break;
    case TOP_RIGHT:
      row--;
      break;
    case RIGHT:
      row--;
      col++;
      break;
    case BOTTOM_RIGHT:
      col++;
      break;
    case BOTTOM:;
      row++;
      col++;
      break;
    case BOTTOM_LEFT:
      row++;
      break;
    case LEFT:
      row++;
      col--;
      break;
  }

  if (row>8 || col>8 || row<1 || col<1) {
    if (row>8 && col>8 && bottomOrTop==1) { //下到上
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
    else if (row<1 && col<1 && bottomOrTop==0){ // 上到下
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
  else if (pixelRead(row, col, bottomOrTop)){
    return false;
  }
  else {
    return true;
  }
}

void HourGlass::pixelMove(byte row, byte col, byte bottomOrTop, byte direct, bool clockwise) {
  byte pre_row = row;
  byte pre_col = col;
  byte pre_bottomOrTop = bottomOrTop;

  if (!pixelRead(row, col, bottomOrTop))
    return;
    
  if (pixelGetNewPosition(row, col, bottomOrTop, direct)) {//取得移動方向座標
    pixelWrite(pre_row, pre_col, pre_bottomOrTop, 0);
    pixelWrite(row, col, bottomOrTop, 1);
  }
  else if (!clockwise) {
    if (pixelGetNewPosition(row, col, bottomOrTop, direct+1>7 ? 0 : direct+1)) {
      pixelWrite(pre_row, pre_col, pre_bottomOrTop, 0);
      pixelWrite(row, col, bottomOrTop, 1);
    }
    else if (pixelGetNewPosition(row, col, bottomOrTop, direct-1<0 ? 7 : direct-1)) {
      pixelWrite(pre_row, pre_col, pre_bottomOrTop, 0);
      pixelWrite(row, col, bottomOrTop, 1);
    }
  }
  else {
    if (pixelGetNewPosition(row, col, bottomOrTop, direct-1<0 ? 7 : direct-1)) {
      pixelWrite(pre_row, pre_col, pre_bottomOrTop, 0);
      pixelWrite(row, col, bottomOrTop, 1);
    }
    else if (pixelGetNewPosition(row, col, bottomOrTop, direct+1> 7 ? 0 : direct+1)) {
      pixelWrite(pre_row, pre_col, pre_bottomOrTop, 0);
      pixelWrite(row, col, bottomOrTop, 1);
    }
  }
}

HourGlass hourGlass;

/*
 * Direction
 * 0 左上
 * 1 上
 * 2 右上
 * 3 右
 * 4 右下
 * 5 下
 * 6 左下
 * 7 左
 */

void matrixUpdate(byte direct, bool clockwise) {//從下到上 從左到右
  switch (direct) {
    case 0://V
      for (int i=0; i<8; i++){
        for (int j=0; j<8; j++){
          if (!clockwise)
            pixelMove(j+1, i+1, 0, direct, clockwise);
            pixelMove(j+1, i+1, 1, direct, clockwise);
          else
            pixelMove(8-j, i+1, 0, direct, clockwise);
            pixelMove(j+1, i+1, 1, direct, clockwise);
        }
      }
      break;

    case 1://V
      for (int k = 1;k >= 0;k --){
        for (int i = 2;i <= 16;i ++){
          if (i <= 9){
            for (int j = 0;j < i-1;j ++){
              if (!clockwise)
                pixelMove(j+1, i-j-1, k, direct, clockwise);
              else
                pixelMove(i-j-1, j+1, k, direct, clockwise);
            }
          }
          else{
            for (int j = 0;j < 17-i;j ++){
              if (!clockwise)
                pixelMove(i+j-8, 8-j, k, direct, clockwise);
              else
                pixelMove(8-j, i+j-8, k, direct, clockwise);
            }
          }
        }
      }
      break;

    case 2://V
      for (int k = 1;k >= 0;k --){
        for (int i = 1;i <= 8;i ++){
          for (int j = 1;j <= 8;j ++){
            if (!clockwise)
              pixelMove(i, 9-j, k, direct, clockwise);
            else
              pixelMove(i, j, k, direct, clockwise);
          }
        }
      }
      break;

    case 3://V
      for (int i = 1;i <= 8;i ++){
        if (!clockwise){
          for (int k = 0;k < 2;k ++){
            for (int j = 0;j < i;j ++)
              pixelMove(i-j, 8-j, k, direct, clockwise);
          }
        }
        else{
          for (int k = 1;k >= 0;k --){
            for (int j = 0;j < i;j ++)
              pixelMove(1+j, 9-i+j, k, direct, clockwise);
          }
        }
      }
      for (int i = 7;i >= 1;i --){
        if (!clockwise){
          for (int k = 0;k < 2;k ++){
            for (int j = 0;j < i;j ++)
              pixelMove(8-j, i-j, k, direct, clockwise);
            }
        }
        else{
          for (int k = 1;k >= 0;k --){
            for (int j = 0;j < i;j ++)
              pixelMove(9-i+j, 1+j, k, direct, clockwise);
            }
        }
      }
      break;

    case 4://V
      for (int k = 0;k < 2;k ++){
        for (int i = 8;i > 0;i --){
          for (int j = 1;j <= 8;j ++){
            if (!clockwise)
              pixelMove(9-j, i, k, direct, clockwise);
            else
              pixelMove(j, i, k, direct, clockwise);
          }
        }
      }
      break;

    case 5://V
      for (int k = 0;k < 2;k ++){
        for (int i = 16;i > 1;i --){
          if (i > 8){
            for (int j = 0;j < 17-i;j ++){
              if (!clockwise)
                pixelMove(8-j, i+j-8, k, direct, clockwise);
              else
                pixelMove(i+j-8, 8-j, k, direct, clockwise);
            }
          }
          else{
            for (int j = 0;j < i-1;j ++){
              if (!clockwise)
                pixelMove(i-j-1, 1+j, k, direct, clockwise);
              else
                pixelMove(1+j, i-j-1, k, direct, clockwise);
            }
          }
        }
      }
      break;
    case 6://V
      for (int k = 0;k < 2;k ++){
        for (int i = 8;i > 0;i --){
          for (int j = 1;j <= 8;j ++){
            if (!clockwise)
              pixelMove(i, j, k, direct, clockwise);
            else
              pixelMove(i, 9-j, k, direct, clockwise);
          }
        }
      }
      break;

    case 7:
      for (int i = 1;i <= 8;i ++){
        if (!clockwise){
          for (int k = 1;k >= 0;k --){
            for (int j = 0;j < i;j ++)
              pixelMove(9-i+j, 1+j, k, direct, clockwise);
          }
        }
        else{
          for (int k = 0;k < 2;k ++){
            for (int j = 0;j < i;j ++)
              pixelMove(8-j, i-j, k, direct, clockwise);
          }
        }
      }
      for (int i = 7;i >= 1;i --){
        if (!clockwise){
          for (int k = 1;k >= 0;k --){
            for (int j = 0;j < i;j ++)
              pixelMove(1+j, 9-i+j, k, direct, clockwise);
            }
        }
        else{
          for (int k = 0;k < 2;k ++){
            for (int j = 0;j < i;j ++)
              pixelMove(i-j, 8-j, k, direct, clockwise);
            }
        }
      }
      break;
  }
}

void getDirect(byte *direct, bool *clockwise) {
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

  int x = analogRead(A1);
  int y = analogRead(A2);
  int z = analogRead(A3);
  
  Serial.print("x:");
  Serial.print(x);
  Serial.print("y:");
  Serial.print(y);
  Serial.print("z:");
  Serial.println(z);

  if (z<z_max_l && z>z_min_l) {
    if (x > x_mid-x_22d5 && x < x_mid-x_67d5 && y > y_mid-y_67d5 && y < y_mid-y_22d5)
      *direct = TOP_RIGHT;
    else if (x < x_mid-x_22d5 && y > y_mid-y_22d5 && y < y_mid+y_22d5)
      *direct = RIGHT;
    else if (x > x_mid-x_22d5 && x < x_mid-x_67d5 && y < y_mid+y_67d5 && y > y_mid+y_22d5)
      *direct = BOTTOM_RIGHT;
    else if (x > x_mid-x_67d5 && x < x_mid+x_67d5 && y > y_mid+y_67d5)
      *direct = BOTTOM;
    else if (x < x_mid+x_22d5 && x > x_mid+x_67d5 && y < y_mid+y_67d5 && y > y_mid+y_22d5)
      *direct = BOTTOM_LEFT;
    else if (x > x_mid+x_22d5 && y > y_mid-y_22d5 && y < y_mid+y_22d5)
      *direct = LEFT;
    else if (x < x_mid+x_22d5 && x > x_mid+x_67d5 && y > y_mid-y_67d5 && y < y_mid-y_22d5)
      *direct = TOP_LEFT;
    else if (x > x_mid-x_67d5 && x < x_mid+x_67d5 && y < y_mid-y_67d5)
      *direct = TOP;
  }

  *clockwise = ((x>x_mid && y<y_mid) || (x<x_mid && y>y_mid)) ? 0 : 1;
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  
  hourGlass.init();
  hourGlass.clear();
  hourGlass.draw();
  /*
  for(int i = 0; i< 8;i++){
    max7219(i+1, data[i]);
  }
  */
  //Serial.println(pixel_check(data, 5, 5, 0));
  /*
  delay(100);
  pixel_move(data, 5, 5, 0, 6);
  draw_matrix(data, 2);*/
}


void loop() {
  
  //delay(100);
  //pixel_move(data, 5, 5, 1, 0);
  
  //z_m = (z_m*4 + z)/5;
  /*
  Serial.print("x:");
  Serial.print(x);
  Serial.print("y:");
  //Serial.print(" ");
  Serial.print(y);
  Serial.print("z:");
  //Serial.print(" ");
  Serial.println(z);
  */
 /*
  static int max_x, max_y;
  max_x = max_x > x ? max_x : x;
  Serial.print(max_x);
  Serial.print(" ");
  max_y = max_y > y ? max_y : y;
  Serial.println(max_y);
  */
 /*
  static int a_x, a_y;
  a_x = a_x*0.9 + x*0.1;
  Serial.print(a_x);
  Serial.print(" ");
  a_y = a_y*0.9 + y*0.1;
  Serial.println(a_y);
  if (y > 300) direct = 5;
  else direct = 1;
 */

  
  byte direct;
  bool clockwise;
  delay(FRAME_DELAY);
  hourGlass.getDirect(&direct, &clockwise);
  //touch_every_pixel(data, direct, spin);
  Serial.println(direct);
  hourGlass.draw();
}
