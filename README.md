# AZTOUCH (ミニトラックパッド)

<img src="/images/info_mini.jpg" width="700">

<br><br>

## 基本情報

<br>

電圧： 3V～5V<br>
消費電力： min：1.5mA　max：2mA<br>
サイズ： 幅：25mm 高さ：21mm 厚さ：1mm<br>
I2Cアドレス：0x0A

<br><br>

## データサイクル
<br>
１．データリクエストを行ったタイミングでタッチ情報を取得するため、リードサイクルを少なくするほど省電力化できます。<br>
２．データリクエストの結果は前のサイクルのタイミングで取得したタッチ情報となります。<br>
３．タッチ情報取得のタイミングは割り込み禁止となるので、その間はI2Cのリクエストを受け取れません。<br>

<br>
<br>
<img src="/images/i2c_flow.jpg" width="700"><br>

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



