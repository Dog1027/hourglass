#include <SPI.h>

#define frame_delay 100
#define drop_delay 6
#define last_sensor_weight 0
#define last_sensor_weight_z 0

byte Decode = 0x9;
byte Intensity = 0xA;
byte Scan = 0xB;
byte Shutdown = 0xC;
byte Test = 0xF;

void max7219_config(byte addr, byte d, int numbers){
  digitalWrite(SS, LOW);  //選取晶片
  for(int j = 0;j < numbers;j++){
    SPI.transfer(addr);  //暫存器位址
    SPI.transfer(d);  //資料
  }
  digitalWrite(SS, HIGH);  //取消選取晶片
}

void max7219_data(byte row, byte *d,  int numbers){
  digitalWrite(SS, LOW);  //選取晶片
  for(int j = 0;j < numbers;j++){
    SPI.transfer(row);  //暫存器位址
    SPI.transfer(d[row-1+j*8]);  //資料
  }
  digitalWrite(SS, HIGH);  //取消選取晶片
}

void draw_matrix(byte *data, int numbers){
  for(int i = 1;i <= 8;i++){
      max7219_data(i, data, numbers);
  }
}

void clear(int numbers){
  for(int i = 1;i <= 8;i++){
      max7219_data(i, 0, numbers);
  }
}

byte data[16]={               //初始圖案

               0b00000000,//1
               0b00000000,//2
               0b00000000,//3
               0b00000000,//4
               0b00000000,//5
               0b00000000,//6
               0b00000000,//7
               0b00000000,//8

               0b11111111,//1
               0b11111110,//2
               0b11111100,//3
               0b11111000,//4
               0b11110000,//5
               0b11100000,//6
               0b11000000,//7
               0b10000000,//8

               };
               
bool pixel_check(byte *d, int row, int col, bool updown){
  bool c = bitRead(d[row-1+updown*8], 8-col);
  return c;
}
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

void pixel_move(byte *d,  int row, int col, bool updown, byte direct, bool spin){
  if (pixel_check(d, row, col, updown)){
    int r = row;
    int c = col;
    bool u = updown;
    if (direct_p_get(data, &r, &c, &u, direct)){//取得移動方向座標
        pixel_write(d, r, c, u, 1);
        pixel_write(d, row, col, updown, 0);
    }
    else if (!spin) {
      if (direct_p_get(data, &r, &c, &u, (direct+1) > 7 ? 0 : (direct+1))){
          pixel_write(d, r, c, u, 1);
          pixel_write(d, row, col, updown, 0);
      }
      else if (direct_p_get(data, &r, &c, &u, (direct-1) < 0 ? 7 : (direct-1))){
          pixel_write(d, r, c, u, 1);
          pixel_write(d, row, col, updown, 0);
      }
    }
    else{
      if (direct_p_get(data, &r, &c, &u, (direct-1) < 0 ? 7 : (direct-1))){
          pixel_write(d, r, c, u, 1);
          pixel_write(d, row, col, updown, 0);
      }
      else if (direct_p_get(data, &r, &c, &u, (direct+1) > 7 ? 0 : (direct+1))){
          pixel_write(d, r, c, u, 1);
          pixel_write(d, row, col, updown, 0);
      }
    }
  }
}

void pixel_write(byte *d,  int row, int col, bool updown, bool b){
  bitWrite(d[row-1+updown*8], 8-col, b);
  //Serial.println(d[row-1+updown*8], BIN);
}

bool direct_p_get(byte *d, int *row, int *col, bool *updown, byte direct){
  int r = *row;
  int c = *col;
  bool u = *updown;
  
  static int tick_goup = 0;
  static int tick_godown = 0;
  if (!(direct == 0 || direct == 1 || direct == 2)) tick_goup = 0;
  if (!(direct == 4 || direct == 5 || direct == 6)) tick_godown = 0;

  switch (direct){
    case 0:
      (*col) --;
      break;
    case 1:
      (*row) --;
      (*col) --;
      break;
    case 2:
      (*row) --;
      break;
    case 3:
      (*row) --;
      (*col) ++;
      break;
    case 4:
      (*col) ++;
      break;
    case 5:;
      (*row) ++;
      (*col) ++;
      break;
    case 6:
      (*row) ++;
      break;
    case 7:
      (*row) ++;
      (*col) --;
      break;
  }
  
  if (*row > 8 || *col > 8 || *row < 1 || *col < 1){
    if (*row > 8 && *col > 8 && *updown == 1){//上到下
      *row -= 8;
      *col -= 8;
      *updown = 0;
      tick_godown ++;
      if (tick_godown < drop_delay || pixel_check(d, *row, *col, *updown)){
        *row = r;
        *col = c;
        *updown = u;
        return false;
      }
      else{
        tick_godown = 0;
        return true;
      }
    }
    else if (*row < 1 && *col < 1 && *updown == 0){//下到上
      *row += 8;
      *col += 8;
      *updown = 1;
      tick_goup ++;
      if (tick_goup < drop_delay || pixel_check(d, *row, *col, *updown)){
        *row = r;
        *col = c;
        *updown = u;
        return false;
      }
      else{
        tick_goup = 0;
        return true;
      }
    }
    else{
      *row = r;
      *col = c;
      *updown = u;
      return false;
    }
  }
  else if (pixel_check(d, *row, *col, *updown)){
    *row = r;
    *col = c;
    *updown = u;
    return false;
  }
  else{
    return true;
  }
}

void touch_every_pixel(byte *d, byte direct, bool spin){//從下到上 從左到右
  switch (direct){
    case 0://V
      for (int k = 1;k >= 0;k --){
        for (int i = 1;i <= 8;i ++){
          for (int j = 1;j <= 8;j ++){
            if (!spin)
              pixel_move(d, j, i, k, direct, spin);
            else
              pixel_move(d, 9-j, i, k, direct, spin);
          }
        }
      }
      break;

    case 1://V
      for (int k = 1;k >= 0;k --){
        for (int i = 2;i <= 16;i ++){
          if (i <= 9){
            for (int j = 0;j < i-1;j ++){
              if (!spin)
                pixel_move(d, j+1, i-j-1, k, direct, spin);
              else
                pixel_move(d, i-j-1, j+1, k, direct, spin);
            }
          }
          else{
            for (int j = 0;j < 17-i;j ++){
              if (!spin)
                pixel_move(d, i+j-8, 8-j, k, direct, spin);
              else
                pixel_move(d, 8-j, i+j-8, k, direct, spin);
            }
          }
        }
      }
      break;

    case 2://V
      for (int k = 1;k >= 0;k --){
        for (int i = 1;i <= 8;i ++){
          for (int j = 1;j <= 8;j ++){
            if (!spin)
              pixel_move(d, i, 9-j, k, direct, spin);
            else
              pixel_move(d, i, j, k, direct, spin);
          }
        }
      }
      break;

    case 3://V
      for (int i = 1;i <= 8;i ++){
        if (!spin){
          for (int k = 0;k < 2;k ++){
            for (int j = 0;j < i;j ++)
              pixel_move(d, i-j, 8-j, k, direct, spin);
          }
        }
        else{
          for (int k = 1;k >= 0;k --){
            for (int j = 0;j < i;j ++)
              pixel_move(d, 1+j, 9-i+j, k, direct, spin);
          }
        }
      }
      for (int i = 7;i >= 1;i --){
        if (!spin){
          for (int k = 0;k < 2;k ++){
            for (int j = 0;j < i;j ++)
              pixel_move(d, 8-j, i-j, k, direct, spin);
            }
        }
        else{
          for (int k = 1;k >= 0;k --){
            for (int j = 0;j < i;j ++)
              pixel_move(d, 9-i+j, 1+j, k, direct, spin);
            }
        }
      }
      break;

    case 4://V
      for (int k = 0;k < 2;k ++){
        for (int i = 8;i > 0;i --){
          for (int j = 1;j <= 8;j ++){
            if (!spin)
              pixel_move(d, 9-j, i, k, direct, spin);
            else
              pixel_move(d, j, i, k, direct, spin);
          }
        }
      }
      break;

    case 5://V
      for (int k = 0;k < 2;k ++){
        for (int i = 16;i > 1;i --){
          if (i > 8){
            for (int j = 0;j < 17-i;j ++){
              if (!spin)
                pixel_move(d, 8-j, i+j-8, k, direct, spin);
              else
                pixel_move(d, i+j-8, 8-j, k, direct, spin);
            }
          }
          else{
            for (int j = 0;j < i-1;j ++){
              if (!spin)
                pixel_move(d, i-j-1, 1+j, k, direct, spin);
              else
                pixel_move(d, 1+j, i-j-1, k, direct, spin);
            }
          }
        }
      }
      break;
    case 6://V
      for (int k = 0;k < 2;k ++){
        for (int i = 8;i > 0;i --){
          for (int j = 1;j <= 8;j ++){
            if (!spin)
              pixel_move(d, i, j, k, direct, spin);
            else
              pixel_move(d, i, 9-j, k, direct, spin);
          }
        }
      }
      break;

    case 7:
      for (int i = 1;i <= 8;i ++){
        if (!spin){
          for (int k = 1;k >= 0;k --){
            for (int j = 0;j < i;j ++)
              pixel_move(d, 9-i+j, 1+j, k, direct, spin);
          }
        }
        else{
          for (int k = 0;k < 2;k ++){
            for (int j = 0;j < i;j ++)
              pixel_move(d, 8-j, i-j, k, direct, spin);
          }
        }
      }
      for (int i = 7;i >= 1;i --){
        if (!spin){
          for (int k = 1;k >= 0;k --){
            for (int j = 0;j < i;j ++)
              pixel_move(d, 1+j, 9-i+j, k, direct, spin);
            }
        }
        else{
          for (int k = 0;k < 2;k ++){
            for (int j = 0;j < i;j ++)
              pixel_move(d, i-j, 8-j, k, direct, spin);
            }
        }
      }
      break;
  }
}


void direct_cal(byte *direct, bool *spin){
  const int x_max = 406;
  const int x_min = 273;
  const int x_mid = (x_max + x_min) / 2;
  const int y_max = 400;
  const int y_min = 275;
  const int y_mid = (y_max + y_min) / 2;//328;
  const int z_max_l = 405;//z不能大於z_max_l
  const int z_min_l = 305;//z不能小於z_min_l
  const int x_22d5 = (x_max - x_mid)*cos(22.5 * PI / 180);
  const int y_22d5 = (y_max - y_mid)*sin(22.5 * PI / 180);
  const int x_67d5 = (x_max - x_mid)*cos(67.5 * PI / 180);
  const int y_67d5 = (y_max - y_mid)*sin(67.5 * PI / 180);
  /*
  int x = analogRead(A1);
  int y = analogRead(A2);
  int z = analogRead(A3);
  */
  static int x = x_mid, y = y_mid, z;
  x = x*last_sensor_weight + analogRead(A1)*(1-last_sensor_weight);
  y = y*last_sensor_weight + analogRead(A2)*(1-last_sensor_weight);
  z = z*last_sensor_weight_z + analogRead(A3)*(1-last_sensor_weight_z);
  
  Serial.print("x:");
  Serial.print(x);
  Serial.print("y:");
  Serial.print(y);
  Serial.print("z:");
  Serial.println(z);


  if (z < z_max_l && z > z_min_l){
    if (x > x_mid-x_22d5 && x < x_mid-x_67d5 && y > y_mid-y_67d5 && y < y_mid-y_22d5)
      *direct = 2;
    else if (x < x_mid-x_22d5 && y > y_mid-y_22d5 && y < y_mid+y_22d5)
      *direct = 3;
    else if (x > x_mid-x_22d5 && x < x_mid-x_67d5 && y < y_mid+y_67d5 && y > y_mid+y_22d5)
      *direct = 4;
    else if (x > x_mid-x_67d5 && x < x_mid+x_67d5 && y > y_mid+y_67d5)
      *direct = 5;
    else if (x < x_mid+x_22d5 && x > x_mid+x_67d5 && y < y_mid+y_67d5 && y > y_mid+y_22d5)
      *direct = 6;
    else if (x > x_mid+x_22d5 && y > y_mid-y_22d5 && y < y_mid+y_22d5)
      *direct = 7;
    else if (x < x_mid+x_22d5 && x > x_mid+x_67d5 && y > y_mid-y_67d5 && y < y_mid-y_22d5)
      *direct = 0;
    else if (x > x_mid-x_67d5 && x < x_mid+x_67d5 && y < y_mid-y_67d5)
      *direct = 1;

  }

  static int x_pre = x_mid, y_pre = y_mid;
  *spin = ((x > x_mid && y < y_pre) || (x < x_mid && y > y_pre)) ? 0 : 1;


}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  
  max7219_config(Decode, 0, 2);
  max7219_config(Intensity, 1, 2);
  max7219_config(Scan, 7, 2);
  max7219_config(Shutdown,1, 2);
  max7219_config(Test, 0, 2 );
  
  clear(2);
  draw_matrix(data, 2);
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
  static byte direct;
  static bool spin;
  delay(frame_delay);
  direct_cal(&direct, &spin);
  touch_every_pixel(data, direct, spin);
  Serial.println(direct);
  draw_matrix(data, 2);
  
}
