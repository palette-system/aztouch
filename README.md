# AZTOUCH (ミニトラックパッド)

<img src="/images/info_mini.jpg" width="350">

<br><br>

## 基本情報

<br>

電圧： 3V～5V<br>
消費電力： 1.7mA～2mA<br>
サイズ： 幅：25mm 高さ：21mm 厚さ：1mm<br>
I2Cアドレス：0x0A(固定)

<br><br>

## コマンド

<table>
  <tr>
    <th>コマンド</th>
    <th>内容</th>
  </tr>
  <tr>
    <td>0x30</td>
    <td>
    <b>設定値要求</b><br>
    次にデータリクエストが来た時のレスポンスが、下記の7バイトのレスポンスになる。<br>
    [<br>
    　0x23(固定),<br>
    　マウススピード設定,<br>
    　drag_touch_time_max,<br>
    　drag_interval_time_max,<br>
    　tap_touch_time_max,<br>
    　move_touch_time_start,<br>
    　read_wait_time<br>
    ]<br>
    <br>
    コマンド例：[0x30]
    </td>
  </tr>
  <tr>
    <td>0x40</td>
    <td>
    <b>マウスのスピード設定</b><br>
    次に送られてきたバイトで移動速度を設定。<br>
    0x00 ～ 0x04 の間で設定可能<br>
    デフォルト値：0x02<br>
    <br>
    コマンド例：[0x40, 0x01]
    </td>
  </tr>
  <tr>
    <td>0x41</td>
    <td>
    <b>ドラッグ1回目のタッチの最大時間(n / 5ms)</b><br>
    次に送られてきたバイトで <b>drag_touch_time_max</b> の値を設定。<br>
    ※ 1=5ミリ秒、デフォルト値：0x64(500ミリ秒)<br>
    <br>
    コマンド例：[0x41, 0x64]
    </td>
  </tr>
  <tr>
    <td>0x42</td>
    <td>
    <b>ドラッグ2回目のタッチまでの最大時間(n / 5ms)</b><br>
    次に送られてきたバイトで <b>drag_interval_time_max</b> の値を設定。<br>
    ※ 1=5ミリ秒、デフォルト値：0x28(200ミリ秒)<br>
    <br>
    コマンド例：[0x42, 0x28]
    </td>
  </tr>
  <tr>
    <td>0x43</td>
    <td>
    <b>タップのタッチの最大時間(n / 5ms)</b><br>
    次に送られてきたバイトで <b>tap_touch_time_max</b> の値を設定。<br>
    ※ 1=5ミリ秒、デフォルト値：0x14(100ミリ秒)<br>
    <br>
    コマンド例：[0x43, 0x14]
    </td>
  </tr>
  <tr>
    <td>0x44</td>
    <td>
    <b>移動開始するまでの時間(n / 5ms)</b><br>
    次に送られてきたバイトで <b>move_touch_time_start</b> の値を設定。<br>
    ※ 1=5ミリ秒、デフォルト値：0x14(100ミリ秒)<br>
    <br>
    コマンド例：[0x44, 0x14]
    </td>
  </tr>
  <tr>
    <td>0x45</td>
    <td>
    <b>アナログ値取得待ち時間</b><br>
    次に送られてきたバイトで <b>read_wait_time</b> の値を設定。<br>
    ※ デフォルト値：0x28(40 NOP)<br>
    <br>
    コマンド例：[0x45, 0x28]
    </td>
  </tr>
  <tr>
    <td>0x4C</td>
    <td>
    <b>スリープ</b><br>
    AZTOUCHをスリープ状態にします。<br>
    スリープ状態の間消費電力は0.1mAになります。<br>
    I2Cのリクエストが飛ぶとスリープ状態から復帰します。<br>
    <br>
    コマンド例：[0x4C]
    </td>
  </tr>
</table>
※ EEPROM に保存されるため電源を落としても保持されます。<br>

<br><br>

## データレスポンス
AZ1UBALLやPIM447と同じフォーマットで操作情報を取得できます。<br>

<table>
  <tr>
    <th>バイト</th>
    <th>内容</th>
  </tr>
  <tr>
    <td>0x00</td>
    <td>左方向への移動距離</td>
  </tr>
  <tr>
    <td>0x01</td>
    <td>右方向への移動距離</td>
  </tr>
  <tr>
    <td>0x02</td>
    <td>上方向への移動距離</td>
  </tr>
  <tr>
    <td>0x03</td>
    <td>下方向への移動距離</td>
  </tr>
  <tr>
    <td>0x04</td>
    <td>タッチ情報<br>
    下記ビットを足し合わせた数値を返します。<br>
    <br>
    0x01 = 縦に2点タッチしている<br>
    0x02 = 横に2点タッチしている<br>
    0x04 = タッチしている<br>
    0x08 = ドラッグしている<br>
    0x10 = 未使用<br>
    0x20 = 未使用<br>
    0x40 = 2点タップ (右クリック)<br>
    0x80 = 1点タップ (左クリック)<br>
    </td>
  </tr>
</table>

<br><br>

## データ取得フロー
<br>
１．データリクエストを行ったタイミングでタッチ情報を取得するため、リードサイクルを少なくするほど省電力化できます。<br>
２．データリクエストの結果は前のサイクルのタイミングで取得したタッチ情報となります。<br>
３．タッチ情報取得のタイミングは割り込み禁止となるので、その間はI2Cのリクエストを受け取れません。<br>

<br>
<br>
<img src="/images/i2c_flow.jpg" width="700"><br>

<br><br>

<b>サイクル間隔と各イベントの感度</b><br>
<table>
  <tr>
    <th>間隔(ミリ秒)</th>
    <th>消費電力(mA)</th>
    <th>移動</th>
    <th>2点タッチ</th>
    <th>タップ</th>
    <th>ダブルタップ</th>
  </tr>
  <tr>
    <td>10</td>
    <td>2.0</td>
    <td>〇</td>
    <td>〇</td>
    <td>〇</td>
    <td>〇</td>
  </tr>
  <tr>
    <td>20</td>
    <td>1.9</td>
    <td>〇</td>
    <td>〇</td>
    <td>〇</td>
    <td>△</td>
  </tr>
  <tr>
    <td>30</td>
    <td>1.8</td>
    <td>〇</td>
    <td>〇</td>
    <td>〇</td>
    <td>△</td>
  </tr>
  <tr>
    <td>40</td>
    <td>1.8</td>
    <td>〇</td>
    <td>〇</td>
    <td>△</td>
    <td>△</td>
  </tr>
  <tr>
    <td>50</td>
    <td>1.8</td>
    <td>〇</td>
    <td>〇</td>
    <td>△</td>
    <td>△</td>
  </tr>
  <tr>
    <td>60</td>
    <td>1.8</td>
    <td>〇</td>
    <td>〇</td>
    <td>△</td>
    <td>△</td>
  </tr>
  <tr>
    <td>70</td>
    <td>1.7</td>
    <td>〇</td>
    <td>〇</td>
    <td>×</td>
    <td>×</td>
  </tr>
  <tr>
    <td>80</td>
    <td>1.7</td>
    <td>△</td>
    <td>△</td>
    <td>×</td>
    <td>×</td>
  </tr>
  <tr>
    <td>90</td>
    <td>1.7</td>
    <td>△</td>
    <td>△</td>
    <td>×</td>
    <td>×</td>
  </tr>
  <tr>
    <td>100</td>
    <td>1.7</td>
    <td>△</td>
    <td>△</td>
    <td>×</td>
    <td>×</td>
  </tr>
</table>


<br><br>


## ピン情報取得フロー
<br>
１．割り込み禁止の間にタッチしているかどうかのアナログ値を取得します。<br>
２．読み込み時の待ち時間を <b>read_wait_time</b> で設定できます。<br>
<br>
<br>
<img src="/images/read_flow.jpg" width="700"><br>

<br><br>


## アクション設定
<br>
<b>ドラッグ判定</b><br>
<img src="/images/drag_flow.jpg" width="700"><br>

<br>

<b>タップ判定</b><br>
<img src="/images/tap_flow.jpg" width="700"><br>

<br>

<b>マウス移動判定</b><br>
タップの時にマウスが移動してしまわないように、タッチしてから少しの間マウス移動はしないようになっています。<br>
<img src="/images/move_flow.jpg" width="700"><br>

<br>



