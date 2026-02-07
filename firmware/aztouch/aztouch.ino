// 1Uトラックボールユニットのホイールセンサーの入力をI2Cに流す

// 開発環境の作り方
// https://ameblo.jp/pta55/entry-12654450554.html

#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <Wire.h>

// AZTOUCH のアドレス
#define I2C_SLAVE_ADD 0x0A

// I2C クロック数
#define I2C_CLOCK  100000

// EEPROM のアドレス
#define EEPADD_STATUS    0x00
#define EEPADD_SPEED    0x01

// I2Cイベント
void receiveEvent(int data_len); // データを受け取った
void requestEvent(); // データ要求を受け取った


// 送信バッファ
short read_org[11];
short send_input[11];

// タッチしていない時のアナログ値
short pin_def[11];

// タッチした時の最大値(デフォルト値)
const short pin_max_def[11] = {106, 112, 110, 116, 103,  109, 97, 109, 108, 104, 104};


// 送信するデータのタイプ
short send_type;

// 読み込んだアナログ値の合計値
short read_total;

// 送信開始位置
short send_index;

// タッチしていた時間
unsigned long touch_start_time; // タッチ開始した時間
unsigned long touch_now_time; // 今の時間
unsigned long touch_last_time; // 最後にタッチした時間
unsigned long touch_time; // タッチし続けている時間

// タッチしていた時間内で2点タッチをしたかどうか(0x40=ダブルタッチしていた)
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

// スピード設定
short speed_index;
// short speed_type_x[] = {23, 15, 8, 5, 3, 2, 1, 0, 0, 1, 2, 3, 5, 8, 15, 23, 23};
// short speed_type_y[] = {12, 5, 3, 2, 1, 0, 0, 1, 2, 3, 5, 12, 12};

short speed_type_x[] = {12, 10, 8, 7, 6, 5, 4, 3, 2, 1, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 12};
short speed_type_y[] = {12, 10, 8, 6, 4, 3, 2, 1, 1, 2, 3, 4, 6, 8, 10, 12, 12};

// タッチのアナログ値取得
void read_analog() {
  short i;
  noInterrupts(); // 割り込み禁止 開始
  for (i=0; i<11; i++) {
    // 読み取りピンをHIGHにして電気を流す
    pinMode(all_pin[i], OUTPUT);
    digitalWrite(all_pin[i], 1);
    delayMicroseconds(10);
    // 読み取りピンのアナログ値を取得
    pinMode(all_pin[i], INPUT);
    read_org[i] = analogRead(all_pin[i]);
    // 読み取りピンをLOWにして残った電気を吸い取る
    pinMode(all_pin[i], OUTPUT);
    digitalWrite(all_pin[i], 0);
    delayMicroseconds(10);
  }
  interrupts(); // 割り込み禁止 解除
}

// タッチの情報を取得
void read_touch() {
  short i;
  read_total = 0;
  // タッチのアナログ値取得
  read_analog();
  // ピンごとのタッチ最大値からのタッチ割合を計算
  for (i=0; i<11; i++) {
    send_input[i] = ((read_org[i] - pin_def[i]) * 128) / pin_max_def[i];
    read_total += read_org[i] - pin_def[i];
  }
  if (read_total > 1023) {
    read_total = 1023;
  } else if (read_total < 0) {
    read_total = 0;
  }
  // 起動してすぐのアナログ値を保持してこれをデフォルト値にする
  if (send_index == 3) {
    for (i=0; i<11; i++) {
      pin_def[i] = read_org[i];
    }
  }
  send_index++;
}

// I2C コマンドを受け取った
void receiveEvent(int data_len) {
  short t;
  // コマンド受け取り
  while (Wire.available()) {
    t = Wire.read();
    if (t == 0x20) {
      send_type = 0; // AZ1UBALLのフォーマットで返す
    } else if (t == 0x21) {
      send_type = 1; // タッチのX,Y座標を返す
    } else if (t == 0x22) {
      send_type = 2; // row,colピンのアナログ値をそのまま返す
    } else if (t >= 0x30 && t <= 0x35) {
      // 0x30 ～ 0x35 速度設定
    }
  }
}

// I2C データ要求を受け取った時の処理
void requestEvent() {
  short c[11], i, r[2], e, tx, ty, tf, x, y;
  unsigned long t;

  // 送信バッファ
  uint8_t send_buf[24];

  if (send_type == 2) { // type 2. row,colピンのアナログ値をそのまま返す
    // ピン情報を取得
    read_touch();
    // ピンのアナログ値をそのまま返す
    for (i=0; i<11; i++) {
      send_buf[(i*2)] = (read_org[i] >> 8) & 0xFF;
      send_buf[(i*2) + 1] = read_org[i] & 0xFF;
    }
    Wire.write(send_buf, 22);
    return;

  }

  // ピン情報を取得
  read_touch();

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
      if (t > 40 && t < 160 && touch_time < 500) drag_flag = 0x08; // 前のタッチからすぐタッチされた & 前のタッチ時間が短い ならドラッグ
    }
    touch_time = touch_now_time - touch_start_time; // タッチされ続けている時間 ミリ秒
    touch_last_time = touch_now_time; // 最後にタッチした時間
    tf |= 0x04 | drag_flag; // タッチ中 + ドラッグ中

  } else {
    // 離された && タッチ時間が短ければタップと判定
    if (old_point[0] > 0 && old_point[1] > 0 && touch_time > 20 && touch_time < 100) { // タップ判定(前測定時タッチされていた＋タッチ時間が短い)
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

  if (send_type == 1) {
    // X,Y 座標を返す
    send_buf[0] = (r[1] >> 8) & 0xFF; // x
    send_buf[1] = r[1] & 0xFF; // x
    send_buf[2] = (r[0] >> 8) & 0xFF; // y
    send_buf[3] = r[0] & 0xFF; // y
    send_buf[4] = (read_total >> 2) & 0xFF; // タッチ量
    send_buf[5] = tf; // タッチ操作フラグ
    Wire.write(send_buf, 6);

  } else {
    // 0 デフォルト az1uballと同じフォーマットを返す
    if (read_total > 60 && touch_time > 100 && old_point[0] > 0 && old_point[1] > 0) { // タッチされている & タッチしてから0.1秒以上 & 前回のタッチ座標がある
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
          send_buf[1] = speed_type_x[x];
        } else {
          send_buf[0] = speed_type_x[x];
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
          send_buf[3] = speed_type_y[y];
        } else {
          send_buf[2] = speed_type_y[y];
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
    Wire.write(send_buf, 5);
  }

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

void setup() {
  uint8_t c;
  short i;

  // VCCチェックピン HIGHT
  pinMode(PIN_PA5, OUTPUT);
  digitalWrite(PIN_PA5, 1);
  delay(10);

  // I2C ピンプルアップ
  pinMode(PIN_PB2, OUTPUT);
  digitalWrite(PIN_PB2, 1);
  delay(10);

  // 初めての起動の場合EPPROMにタッチ最大値設定を書き込む
  c = EEPROM.read(EEPADD_STATUS); // 最初の0バイト目を読み込む
  if (c != 0x25) {
    EEPROM.write(EEPADD_STATUS, 0x25); // 初期化したよを書き込む
  }


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
  send_type = 0;
  double_touch_flag = 0;
  drag_flag = 0;

}


void loop() {
  if (send_index < 5) read_touch();

  delay(100);
}
