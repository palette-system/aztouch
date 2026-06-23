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


// 読み込んだパッドの数値
short read_org[11]; // 読み込んだアナログ値まま
short send_input[11]; // アナログ値から計算した入力値

// タッチしていない時のアナログ値
short pin_def[11];

// タッチした時の最大値(デフォルト値)
const short pin_max_def[11] = {106, 112, 110, 116, 103,  109, 97, 109, 108, 104, 104};

// 読み込んだアナログ値の合計値
short read_total;

// 送信開始位置
short send_index;

// タッチしていた時間
unsigned long touch_start_time; // タッチ開始した時間
unsigned long touch_now_time; // 今の時間
unsigned long touch_last_time; // 最後にタッチした時間
unsigned long touch_time; // タッチし続けている時間

//ドラッグ判定時間の設定
unsigned short drag_touch_time_max; // ダブルタップの1回目のタッチの時間の最大
unsigned short drag_interval_time_max; // ダブルタップの間の離している時間の最大

// タップ判定時間
unsigned short tap_touch_time_max;

// 移動開始までの時間
unsigned short move_touch_time_start; // 移動開始までの時間

// アナログ値取得時のウェイトタイム(clock)
unsigned short read_wait_time;

// 読み込みしてからどれくらい時間が経ったか
unsigned long check_time;

// タッチしていた時間内で2点タッチをどれくらいのサイクル行っていたか
short double_touch_flag;

// ドラッグ中かどうか(0x08=ドラッグ中)
uint8_t drag_flag;

// 前回測定した座標
short old_point[2];

// ピン設定
short all_pin[11] = {
  PIN_PC0, PIN_PC2, PIN_PB4, PIN_PC3, PIN_PA4, // row 5ピン
  PIN_PB5, PIN_PA7, PIN_PA6, PIN_PA1, PIN_PC1, PIN_PA2 // col 6ピン
};

// 9四方
short read_pin[9] = {PIN_PB4, PIN_PC0, PIN_PC1,  PIN_PB5, PIN_PC3, PIN_PC2,  PIN_PA7, PIN_PA1, PIN_PA2};
// short pin_wrk[9] = {114, 114, 130, 125, 124, 124, 101, 118, 119};
short pin_wrk[9] = {113, 115, 115,  124, 122, 125,  104, 122, 125};
// short pin_min[9] = {622, 638, 599, 606, 622, 607, 640, 609, 594};
short pin_min[9] = {617, 628, 617,  600, 599, 599,  637, 603, 588};
// short pin_max[9] = {736, 752, 729, 731, 746, 731, 741, 727, 713};
short pin_max[9] = {730, 743, 732,  724, 721, 724,  741, 725, 713};
short xy_old[5][3] = {{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}};

// レスポンスタイプ
short send_type;

// EEPROMバッファ
uint8_t send_eeprom_buf[7];

// 送信バッファ
uint8_t send_buf[24];

// バッファを送ったフラグ
short send_status;

// スピード設定
unsigned short speed_index;

// 速度設定用の構造体
struct speed_setting {
  short speed_x[21];
  short speed_y[17];
};

uint16_t millis_my() {
  while (RTC.STATUS & RTC_CNTBUSY_bm);
  return RTC.CNT;
}





// スリープフラグ
short sleep_flag;

// タッチのアナログ値取得
void read_analog_raw(unsigned short check_max) {
  unsigned short i, t;
  noInterrupts(); // 割り込み禁止 開始
  for (i=0; i<check_max; i++) {
    // 読み取りピンをHIGHにして電気を流す
    pinMode(all_pin[i], OUTPUT);
    digitalWrite(all_pin[i], 1);
    for (t=0; t<read_wait_time; t++) _NOP();

    // 読み取りピンのアナログ値を取得
    pinMode(all_pin[i], INPUT);
    read_org[i] = analogRead(all_pin[i]);
    // 読み取りピンをLOWにして残った電気を吸い取る
    pinMode(all_pin[i], OUTPUT);
    digitalWrite(all_pin[i], 0);
    for (t=0; t<read_wait_time; t++) _NOP();
  }
  interrupts(); // 割り込み禁止 解除
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

  tp = yt + ym + yb;

  xl_p = (xl * 128) / (xc + xl);
  xr_p = (xr * 128) / (xc + xr);
  x = xr_p - xl_p;

  yt_p = (yt * 128) / (ym + yt);
  yb_p = (yb * 128) / (ym + yb);
  y = yb_p - yt_p;

  if (tp < 120 ||
      (xc + xl) < 100 || (xc + xr) < 100 ||
      (yt + ym) < 100 || (yb + ym) < 100) {
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
        if (ch_x > -100 && ch_y > -100 && ch_x < 100 && ch_y < 100
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

  // I2C ピンプルアップ
  pinMode(PIN_PB2, OUTPUT);
  digitalWrite(PIN_PB2, 1);
  delay(10);


  // センサーピン初期化
  // col : A4, A5, A6, A7, B5 
  // row : C0, C1, C2, C3, A1, A2 (10K)
  for (i=0; i<11; i++) {
    pinMode(all_pin[i], OUTPUT);
    digitalWrite(all_pin[i], 0);
    pin_def[i] = 0;
  }
  delay(10);

  // I2C スレーブ初期化
  Wire.begin(I2C_SLAVE_ADD); // アドレス
  Wire.setClock(I2C_CLOCK); // クロック数
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  send_index = 0;
  touch_now_time = 0;
  double_touch_flag = 0;
  drag_flag = 0;
  sleep_flag = 0;
  send_status = 0;
  send_type = 0;

}


void loop() {

}
