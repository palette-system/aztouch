// AZTOUCH パッド上のアナログ値を取得してI2Cに流す

// 開発環境の作り方
// https://ameblo.jp/pta55/entry-12654450554.html

#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <EEPROM.h>
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

// レスポンスタイプ
short send_type;

// EEPROMバッファ
uint8_t send_eeprom_buf[7];

// 送信バッファ
uint8_t send_buf[5];

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

// 速度設定
const speed_setting speed_type[] = {
  {
    .speed_x = {8, 7, 6, 5, 4, 3, 2, 2, 1, 0, 0, 1, 2, 2, 3, 4, 5, 6, 7, 8, 8},
    .speed_y = {7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 7}
  },
  {
    .speed_x = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9},
    .speed_y = {7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 7}
  },
  {
    .speed_x = {10, 9, 8, 7, 6, 5, 4, 3, 2, 0, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10},
    .speed_y = {10, 8, 6, 5, 4, 3, 2, 0, 0, 2, 3, 4, 5, 6, 8, 10, 10}
  },
  {
    .speed_x = {12, 10, 8, 7, 6, 5, 4, 3, 2, 0, 0, 2, 3, 4, 5, 6, 7, 8, 10, 12, 12},
    .speed_y = {12, 10, 8, 6, 4, 3, 2, 0, 0, 2, 3, 4, 6, 8, 10, 12, 12}
  },
  {
    .speed_x = {14, 12, 10, 7, 6, 5, 4, 3, 2, 0, 0, 2, 3, 4, 5, 6, 7, 10, 12, 14, 14},
    .speed_y = {14, 12, 10, 6, 4, 3, 2, 0, 0, 2, 3, 4, 6, 10, 12, 14, 14}
  }
};

// 現在の速度設定
speed_setting speed_buf = {
  .speed_x = {8, 7, 6, 5, 4, 3, 2, 2, 1, 0, 0, 1, 2, 2, 3, 4, 5, 6, 7, 8, 8},
  .speed_y = {7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 7}
};

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

// タッチの情報を取得
void read_analog_data(short check_max) {
  short i;
  read_total = 0;
  // タッチのアナログ値取得
  read_analog_raw(check_max);
  // ピンごとのタッチ最大値からのタッチ割合を計算
  for (i=0; i<check_max; i++) {
    send_input[i] = ((read_org[i] - pin_def[i]) * 128) / pin_max_def[i];
    read_total += read_org[i] - pin_def[i];
  }
  if (read_total > 1023) {
    read_total = 1023;
  } else if (read_total < 0) {
    read_total = 0;
  }
}

void read_touch() {
  short c[11], i, r[2], e, tx, ty, tf, x, y;
  unsigned long t;

  // ピン情報を取得
  read_analog_data(11);

  // ピン情報からサーモを作成
  // これをした方がカーソルのブレが少なかった
  for (i=0; i<11; i++) {
    if (send_input[i] > 5) {
      // 該当ピンがタッチされている
      if (i == 0 || i == 5) {
        // 前のピンが無い
        c[i] = ((send_input[i] * 128) / (send_input[i] + send_input[i + 1])); // 次ピン との割合
      } else if (i == 4 || i == 10) {
        // 次のピンが無い
        c[i] = ((send_input[i] * 128) / (send_input[i] + send_input[i - 1])); // 前ピン との割合
      } else {
        // 前後のピンがある
        c[i] = ((send_input[i] * 128) / (send_input[i] + send_input[i - 1] + send_input[i + 1])); // 前後ピン との割合
      }
    } else {
      // 該当ピンがタッチされていない
      c[i] = 0;
    }
  }

  // サーモを元にタッチ座標を計算
  r[0] = 0;
  for (i=0; i<5; i++) {
    if (i < 2 && c[i] > 0 && c[i+1] > 0 && c[i+3] > 0) { // 4点
      r[0] = (i * 128) + ((c[i+3] * 384) / (c[i] + c[i+3]));
      i += 3;
    } else if (i < 3 && c[i] > 0 && c[i+1] > 0 && c[i+2] > 0) { // 3点
      r[0] = (i * 128) + ((c[i+2] * 256) / (c[i] + c[i+2]));
      i += 2;
    } else if (i < 4 && c[i] > 0 && c[i+1] > 0) { // 2点
      r[0] = (i * 128) + ((c[i+1] * 128) / (c[i] + c[i+1]));
      i += 1;
    } else if (c[i] > 0) { // 1点
      r[0] = i * 128;
    }
  }
  r[1] = 0;
  for (i=0; i<6; i++) {
    e = i + 5;
    if (e < 9 && c[e] > 0 && c[e+1] > 0 && c[e+2] > 0) { // 3点
      r[1] = (i * 128) + ((c[e+2] * 256) / (c[e] + c[e+2]));
      i += 2;
    } else if (i < 10 && c[e] > 0 && c[e+1] > 0) { // 2点
      r[1] = (i * 128) + ((c[e+1] * 128) / (c[e] + c[e+1]));
      i += 1;
    } else if (c[e] > 0) { // 1点
      r[1] = i * 128;
    }
  }

  // 2点タッチを判定
  tf = 0;
  ty = 0; // 3になったら2点タッチ
  tx = 0; // 3になったら2点タッチ
  if (read_total > 60) {
    for (i=0; i<5; i++) {
      if (ty==0 && c[i] > 10) { // 1つタッチを見つけた
        ty++;
      } else if (ty == 1 && c[i] == 0) { // 触ってない場所を見つけた
        ty++;
      } else if (ty == 2 && c[i] > 10) { // 2つ目タッチを見つけた
        ty++;
        double_touch_flag++; // タッチ中に2点タッチがあったかどうか
        tf |= 0x01; // 現在2点タッチかどうか
      }
    }
    for (i=5; i<11; i++) {
      if (tx==0 && c[i] > 10) { // 1つタッチを見つけた
        tx++;
      } else if (tx == 1 && c[i] == 0) { // 触ってない場所を見つけた
        tx++;
      } else if (tx == 2 && c[i] > 10) { // 2つ目タッチを見つけた
        tx++;
        double_touch_flag++; // タッチ中に2点タッチがあったかどうか
        tf |= 0x02; // 現在2点タッチかどうか
      }
    }
  }

  // タッチ操作フラグ
  touch_now_time = millis();
  if (read_total > 60) {
    // タッチしはじめ
    if (old_point[0] == 0 && old_point[1] == 0) {
      touch_start_time = touch_now_time; // タッチ開始時間を設定
      t = touch_now_time - touch_last_time; // 前のタッチからどれくらい時間が経ったか
      if (t > 40 && t < drag_interval_time_max && touch_time < drag_touch_time_max) drag_flag = 0x08; // 前のタッチからすぐタッチされた & 前のタッチ時間が短い ならドラッグ
    }
    touch_time = touch_now_time - touch_start_time; // タッチされ続けている時間 ミリ秒
    touch_last_time = touch_now_time; // 最後にタッチした時間
    tf |= 0x04 | drag_flag; // タッチ中 + ドラッグ中

  } else {
    // 離された && タッチ時間が短ければタップと判定
    if (old_point[0] > 0 && old_point[1] > 0 && touch_time > 30 && touch_time < tap_touch_time_max) { // タップ判定(前測定時タッチされていた＋タッチ時間が短い)
      if (double_touch_flag > 2) {
        // 2点でタップ
        tf |= 0x40;
      } else {
        // 1点でタップ
        tf |= 0x80;
      }
    }
    // フラグリセット
    double_touch_flag = 0; // タッチ中に2点タッチになった事があったか
    touch_start_time = 0; // タッチ開始時間
    drag_flag = 0; // ドラッグ中
  }

  // 0 = デフォルト az1uballと同じフォーマットを返す
  if (read_total > 60 && touch_time > move_touch_time_start && old_point[0] > 0 && old_point[1] > 0) { // タッチされている & タッチしてから0.1秒以上 & 前回のタッチ座標がある
    x = (r[1] / 32);
    y = (r[0] / 32);
    if (tf & 0x02) {
      // 横に2点タッチされていた場合は縦移動しない
      send_buf[0] = 0;
      send_buf[1] = 0;
    } else {
      // 横移動
      if (x > 10) {
        send_buf[0] = 0;
        send_buf[1] = speed_buf.speed_x[x];
      } else {
        send_buf[0] = speed_buf.speed_x[x];
        send_buf[1] = 0;
      }
    }
    if (tf & 0x01) {
      // 縦に2点タッチされていた場合は縦移動しない
      send_buf[2] = 0;
      send_buf[3] = 0;
    } else {
      // 縦移動
      if (y > 8) {
        send_buf[2] = 0;
        send_buf[3] = speed_buf.speed_y[y];
      } else {
        send_buf[2] = speed_buf.speed_y[y];
        send_buf[3] = 0;
      }
    }
  } else {
    send_buf[0] = 0;
    send_buf[1] = 0;
    send_buf[2] = 0;
    send_buf[3] = 0;
  }
  send_buf[4] = tf; // タッチ操作フラグ
  send_status = 1;

  if (read_total > 60) {
    // 前回の位置を保持
    old_point[0] = r[0];
    old_point[1] = r[1];
  } else {
    // タッチしていない
    old_point[0] = 0;
    old_point[1] = 0;
  }
}

// I2C コマンドを受け取った
void receiveEvent(int data_len) {
  unsigned short t, c;
  // コマンド受け取り
  if (Wire.available()) {
    t = Wire.read();
    if (t == 0x30) {
      send_type = 1;

    } else if (t == 0x40) {
      if (!Wire.available()) return;
      c = Wire.read();
      // 0x00 ～ 0x04 速度設定
      if (c <= 0x04 && c != speed_index) {
        send_eeprom_buf[EEPADD_SPEED] = c;
        speed_index = send_eeprom_buf[EEPADD_SPEED];
        EEPROM.write(EEPADD_SPEED, (speed_index & 0x0F));
        memcpy(&speed_buf, &speed_type[speed_index], sizeof(speed_setting));
      }
    } else if (t == 0x41) {
      // ドラッグ1回目のタッチ最大時間(n / 5ms)
      if (!Wire.available()) return;
      c = Wire.read();
      if (drag_touch_time_max != (c * 5)) {
        send_eeprom_buf[EEPADD_DRAG_TOUCH_TIME] = c;
        drag_touch_time_max = send_eeprom_buf[EEPADD_DRAG_TOUCH_TIME] * 5;
        EEPROM.write(EEPADD_DRAG_TOUCH_TIME, c);
      }
    } else if (t == 0x42) {
      // ドラッグ2回目のタッチまでの最大時間(n / 5ms)
      if (!Wire.available()) return;
      c = Wire.read();
      if (drag_interval_time_max != (c * 5)) {
        send_eeprom_buf[EEPADD_DRAG_INTERVAL_TIME] = c;
        drag_interval_time_max = send_eeprom_buf[EEPADD_DRAG_INTERVAL_TIME] * 5;
        EEPROM.write(EEPADD_DRAG_INTERVAL_TIME, c);
      }
    } else if (t == 0x43) {
      // タップのタッチの最大時間(n / 5ms)
      if (!Wire.available()) return;
      c = Wire.read();
      if (tap_touch_time_max != (c * 5)) {
        send_eeprom_buf[EEPADD_TAP_TOUCH_TIME] = c;
        tap_touch_time_max = send_eeprom_buf[EEPADD_TAP_TOUCH_TIME] * 5;
        EEPROM.write(EEPADD_TAP_TOUCH_TIME, c);
      }
    } else if (t == 0x44) {
      // 移動開始するまでの時間(n / 5ms)
      if (!Wire.available()) return;
      c = Wire.read();
      if (move_touch_time_start != (c * 5)) {
        send_eeprom_buf[EEPADD_MOVE_START_TIME] = c;
        move_touch_time_start = send_eeprom_buf[EEPADD_MOVE_START_TIME] * 5;
        EEPROM.write(EEPADD_MOVE_START_TIME, c);
      }
    } else if (t == 0x45) {
      // アナログ値取得待ち時間
      if (!Wire.available()) return;
      c = Wire.read();
      if (read_wait_time != c) {
        send_eeprom_buf[EEPADD_READ_WAIT_TIME] = c;
        read_wait_time = send_eeprom_buf[EEPADD_READ_WAIT_TIME];
        EEPROM.write(EEPADD_READ_WAIT_TIME, c);
      }
    } else if (t == 0x46) {
      // 設定リセット
      // カーソル移動速度を指定
      if (send_eeprom_buf[EEPADD_STATUS] != 0x03) {
        send_eeprom_buf[EEPADD_STATUS] = 0x03;
        EEPROM.write(EEPADD_SPEED, send_eeprom_buf[EEPADD_STATUS]);
        speed_index = send_eeprom_buf[EEPADD_STATUS];
        memcpy(&speed_buf, &speed_type[speed_index], sizeof(speed_setting));
      }
      // ドラッグ1回目のタッチの最大時間(n / 5ms)
      if (send_eeprom_buf[EEPADD_DRAG_TOUCH_TIME] != 0x64) {
        send_eeprom_buf[EEPADD_DRAG_TOUCH_TIME] = 0x64;
        EEPROM.write(EEPADD_DRAG_TOUCH_TIME, send_eeprom_buf[EEPADD_DRAG_TOUCH_TIME]);
        drag_touch_time_max = send_eeprom_buf[EEPADD_DRAG_TOUCH_TIME] * 5;
      }
      // ドラッグ2回目のタッチまでの最大時間(n / 5ms)
      if (send_eeprom_buf[EEPADD_DRAG_INTERVAL_TIME] != 0x28) {
        send_eeprom_buf[EEPADD_DRAG_INTERVAL_TIME] = 0x28;
        drag_interval_time_max = send_eeprom_buf[EEPADD_DRAG_INTERVAL_TIME] * 5;
        EEPROM.write(EEPADD_DRAG_INTERVAL_TIME, send_eeprom_buf[EEPADD_DRAG_INTERVAL_TIME]);
      }
      // タップのタッチの最大時間(n / 5ms)
      if (send_eeprom_buf[EEPADD_TAP_TOUCH_TIME] != 0x14) {
        send_eeprom_buf[EEPADD_TAP_TOUCH_TIME] = 0x14;
        tap_touch_time_max = send_eeprom_buf[EEPADD_TAP_TOUCH_TIME] * 5;
        EEPROM.write(EEPADD_TAP_TOUCH_TIME, send_eeprom_buf[EEPADD_TAP_TOUCH_TIME]);
      }
      // 移動開始するまでの時間(n / 5ms)
      if (send_eeprom_buf[EEPADD_MOVE_START_TIME] != 0x14) {
        send_eeprom_buf[EEPADD_MOVE_START_TIME] = 0x14;
        move_touch_time_start = send_eeprom_buf[EEPADD_MOVE_START_TIME] * 5;
        EEPROM.write(EEPADD_MOVE_START_TIME, send_eeprom_buf[EEPADD_MOVE_START_TIME]);
      }
      // アナログ値取得待ち時間
      if (send_eeprom_buf[EEPADD_READ_WAIT_TIME] != 0x28) {
        send_eeprom_buf[EEPADD_READ_WAIT_TIME] = 0x28;
        read_wait_time = send_eeprom_buf[EEPADD_READ_WAIT_TIME];
        EEPROM.write(EEPADD_READ_WAIT_TIME, send_eeprom_buf[EEPADD_READ_WAIT_TIME]);
      }

    } else if (t == 0x4C) {
      // スリープ実行フラグON
      sleep_flag = 1;
    }
  }
}


// I2C データ要求を受け取った時の処理
void requestEvent() {
  if (send_type == 1) {
    Wire.write(send_eeprom_buf, 7);
    send_type = 0;
  } else {
    Wire.write(send_buf, 5);
    send_status = 0;
  }
}

void setup() {
  uint8_t c;
  short i;

  // I2C ピンプルアップ
  pinMode(PIN_PB2, OUTPUT);
  digitalWrite(PIN_PB2, 1);
  delay(10);

  // 初めての起動の場合EPPROMにタッチ最大値設定を書き込む
  c = EEPROM.read(EEPADD_STATUS); // 最初の0バイト目を読み込む
  if (c != 0x23) {
    EEPROM.write(EEPADD_STATUS, 0x23); // 初期化したよを書き込む
    EEPROM.write(EEPADD_SPEED, 0x02); // カーソル移動速度を指定
    EEPROM.write(EEPADD_DRAG_TOUCH_TIME, 0x64); // ドラッグ1回目のタッチの最大時間(n / 5ms)
    EEPROM.write(EEPADD_DRAG_INTERVAL_TIME, 0x28); // ドラッグ2回目のタッチまでの最大時間(n / 5ms)
    EEPROM.write(EEPADD_TAP_TOUCH_TIME, 0x14); // タップのタッチの最大時間(n / 5ms)
    EEPROM.write(EEPADD_MOVE_START_TIME, 0x14); // 移動開始するまでの時間(n / 5ms)
    EEPROM.write(EEPADD_READ_WAIT_TIME, 0x28); // アナログ値取得待ち時間
  }

  send_eeprom_buf[EEPADD_STATUS] = EEPROM.read(EEPADD_STATUS);
  send_eeprom_buf[EEPADD_SPEED] = EEPROM.read(EEPADD_SPEED);
  send_eeprom_buf[EEPADD_DRAG_TOUCH_TIME] = EEPROM.read(EEPADD_DRAG_TOUCH_TIME);
  send_eeprom_buf[EEPADD_DRAG_INTERVAL_TIME] = EEPROM.read(EEPADD_DRAG_INTERVAL_TIME);
  send_eeprom_buf[EEPADD_TAP_TOUCH_TIME] = EEPROM.read(EEPADD_TAP_TOUCH_TIME);
  send_eeprom_buf[EEPADD_MOVE_START_TIME] = EEPROM.read(EEPADD_MOVE_START_TIME);
  send_eeprom_buf[EEPADD_READ_WAIT_TIME] = EEPROM.read(EEPADD_READ_WAIT_TIME);

  // カーソルの移動速度をEPPROMから読み込む
  speed_index = send_eeprom_buf[EEPADD_SPEED];
  memcpy(&speed_buf, &speed_type[speed_index], sizeof(speed_setting));

  // ドラッグ時間の設定
  drag_touch_time_max = send_eeprom_buf[EEPADD_DRAG_TOUCH_TIME] * 5;
  drag_interval_time_max = send_eeprom_buf[EEPADD_DRAG_INTERVAL_TIME] * 5;

  // タップ判定時間
  tap_touch_time_max = send_eeprom_buf[EEPADD_TAP_TOUCH_TIME] * 5;

  // 移動開始時間
  move_touch_time_start = send_eeprom_buf[EEPADD_MOVE_START_TIME] * 5;

  // アナログ値取得待ち時間
  read_wait_time = send_eeprom_buf[EEPADD_READ_WAIT_TIME];

  // センサーピン初期化
  // col : A4, A5, A6, A7, B5 
  // row : C0, C1, C2, C3, A1, A2 (10K)
  for (i=0; i<11; i++) {
    pinMode(all_pin[i], OUTPUT);
    digitalWrite(all_pin[i], 0);
    pin_def[i] = 0;
  }
  delay(10);

  // スリープ初期化
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

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
  // デフォルト値を取得するため起動してから5回だけパッド情報取得
  if (send_index < 4) {
    read_analog_data(11);
    // 起動してすぐのアナログ値を保持してこれをデフォルト値にする
    if (send_index == 3) {
      short i;
      for (i=0; i<11; i++) {
        pin_def[i] = read_org[i];
      }
    }
    send_index++;
    delay(100);
  }

  // スリープ実行
  if (sleep_flag) {
    sleep_flag = 0;
    sleep_cpu();
  }

  // 前回の読み込みデータを送信していたら次の読み込みを送信
  if (send_status == 0) {
    read_touch();
  }

}
