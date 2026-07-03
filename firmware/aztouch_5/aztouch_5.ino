// AZTOUCH パッド上のアナログ値を取得してI2Cに流す

// 開発環境の作り方
// https://ameblo.jp/pta55/entry-12654450554.html

#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
// #include <EEPROM.h>
#include <Wire.h>

// AZTOUCH のアドレス
#define I2C_SLAVE_ADD 0x0A

// I2C クロック数
#define I2C_CLOCK  100000

// EEPROM のアドレス
#define EEPADD_STATUS              0x00
#define EEPADD_SPEED               0x01
#define EEPADD_DRAG_TOUCH_TIME     0x02
#define EEPADD_DRAG_INTERVAL_TIME  0x03
#define EEPADD_TAP_TOUCH_TIME      0x04
#define EEPADD_MOVE_START_TIME     0x05
#define EEPADD_READ_WAIT_TIME      0x06

// I2Cイベント
void receiveEvent(int data_len); // データを受け取った
void requestEvent(); // データ要求を受け取った




// 9四方
short read_pin[9] = {PIN_PB4, PIN_PC0, PIN_PC1,  PIN_PB5, PIN_PC3, PIN_PC2,  PIN_PA7, PIN_PA1, PIN_PA2};
// short pin_wrk[9] = {114, 114, 130, 125, 124, 124, 101, 118, 119};
short pin_wrk[9] = {113, 115, 115,  124, 122, 125,  104, 122, 125};
// short pin_min[9] = {622, 638, 599, 606, 622, 607, 640, 609, 594};
short pin_min[9] = {617, 628, 617,  600, 599, 599,  637, 603, 588};
// short pin_max[9] = {736, 752, 729, 731, 746, 731, 741, 727, 713};
short pin_max[9] = {730, 743, 732,  724, 721, 724,  741, 725, 713};
short xy_old[5][3] = {{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}};


// 送信バッファ
uint8_t send_buf[24];




uint16_t millis_my() {
  while (RTC.STATUS & RTC_CNTBUSY_bm);
  return RTC.CNT;
}







// I2C コマンドを受け取った
void receiveEvent(int data_len) {
  // コマンド受け取り
  while (Wire.available()) {
    Wire.read();
  }
}


short xl, xc, xr,  yt, ym, yb, tp;
short xl_p, xr_p, yt_p, yb_p;
short x, y;
short p[9];
short r[9];
short ch_i, ch_x, ch_y, mv_x, mv_y;
short rx, ry;

// I2C データ要求を受け取った時の処理
void requestEvent() {
  short i, t, rp;

  noInterrupts(); // 割り込み禁止 開始
  for (i=0; i<9; i++) {
    rp = read_pin[i];
    pinMode(rp, OUTPUT);
    digitalWrite(rp, 1);
    for (t=0; t<80; t++) _NOP();
    pinMode(rp, INPUT);
    for (t=0; t<80; t++) _NOP();
    r[i] = analogRead(rp);
    pinMode(rp, OUTPUT);
    digitalWrite(rp, 0);
  }
  interrupts(); // 割り込み禁止 解除
  for (i=0; i<9; i++) {
    if (r[i] < pin_min[i]) r[i] = pin_min[i];
    if (r[i] > pin_max[i]) r[i] = pin_max[i];
    p[i] = (r[i] - pin_min[i]) * 128 / pin_wrk[i];
  }
  xl = p[0] + p[3] + p[6];
  xc = p[1] + p[4] + p[7];
  xr = p[2] + p[5] + p[8];

  yt = p[0] + p[1] + p[2];
  ym = p[3] + p[4] + p[5];
  yb = p[6] + p[7] + p[8];


  if ((xc + xl) < 100 || (xc + xr) < 100 || (yt + ym) < 100 || (yb + ym) < 100) {
    x = 0;
    y = 0;
    tp = 0;
  } else {
    xl_p = (xl * 128) / (xc + xl);
    xr_p = (xr * 128) / (xc + xr);
    x = xr_p - xl_p;

    yt_p = (yt * 128) / (ym + yt);
    yb_p = (yb * 128) / (ym + yb);
    y = yb_p - yt_p;

    tp = yt + ym + yb;
  }

  if (tp < 120) {
    x = 0;
    y = 0;
    tp = 0;
  }

  for (i=0; i<4; i++) {
    xy_old[i][0] = xy_old[i + 1][0];
    xy_old[i][1] = xy_old[i + 1][1];
    xy_old[i][2] = xy_old[i + 1][2];
  }
  xy_old[4][0] = x;
  xy_old[4][1] = y;
  xy_old[4][2] = tp;

  ch_i = mv_x = mv_y = 0;
  for (i=0; i<5; i++) {
    if (xy_old[i][2] > 120) {
      if (ch_i > 0 && xy_old[i - 1][2] > 120 && xy_old[i][2] > 120) {
        ch_x = xy_old[i][0] - xy_old[i - 1][0];
        ch_y = xy_old[i][1] - xy_old[i - 1][1];
        if (ch_x > -200 && ch_y > -200 && ch_x < 200 && ch_y < 200
            // && !((ch_x > -2 && ch_x < 2) || (ch_y > -2 && ch_y < 2))
            ) {
              mv_x += ch_x;
              mv_y += ch_y;
        }
      }
      ch_i++;
    } else {
      ch_i = 0;
    }
  }
  rx = mv_x / 3;
  ry = mv_y / 3;

  if (rx < 0) {
    send_buf[0] = abs(rx);
    send_buf[1] = 0;
  } else {
    send_buf[0] = 0;
    send_buf[1] = rx;
  }
  if (ry < 0) {
    send_buf[2] = abs(ry);
    send_buf[3] = 0;
  } else {
    send_buf[2] = 0;
    send_buf[3] = ry;
  }
  send_buf[4] = 0;
  Wire.write(send_buf, 5);

/*
  send_buf[0] = (r[0] >> 8) & 0xFF;
  send_buf[1] = r[0] & 0xFF;
  send_buf[2] = (r[1] >> 8) & 0xFF;
  send_buf[3] = r[1] & 0xFF;
  send_buf[4] = (r[2] >> 8) & 0xFF;
  send_buf[5] = r[2] & 0xFF;
  send_buf[6] = (r[3] >> 8) & 0xFF;
  send_buf[7] = r[3] & 0xFF;
  send_buf[8] = (r[4] >> 8) & 0xFF;
  send_buf[9] = r[4] & 0xFF;
  send_buf[10] = (r[5] >> 8) & 0xFF;
  send_buf[11] = r[5] & 0xFF;
  send_buf[12] = (r[6] >> 8) & 0xFF;
  send_buf[13] = r[6] & 0xFF;
  send_buf[14] = (r[7] >> 8) & 0xFF;
  send_buf[15] = r[7] & 0xFF;
  send_buf[16] = (r[8] >> 8) & 0xFF;
  send_buf[17] = r[8] & 0xFF;
  Wire.write(send_buf, 18);
*/

}

void setup() {
  short i;



  // センサーピン初期化
  for (i=0; i<9; i++) {
    pinMode(read_pin[i], OUTPUT);
    digitalWrite(read_pin[i], 0);
  }
  delay(10);

  // I2C スレーブ初期化
  Wire.begin(I2C_SLAVE_ADD); // アドレス
  Wire.setClock(I2C_CLOCK); // クロック数
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);


}


void loop() {

}
