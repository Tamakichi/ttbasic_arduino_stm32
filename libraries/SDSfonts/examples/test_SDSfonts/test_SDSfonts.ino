// SJIS版フォントライブラリ利用サンプル
// 作成 2018/10/27  by Tamakichi
// 修正 2018/11/07  by Tamakichi
//

#include "SDSfonts.h"

// ビットパターン表示
// d: 8ビットパターンデータ
//
void bitdisp(uint8_t d) {
  for (uint8_t i=0; i<8;i++) {
    if (d & 0x80>>i) 
      Serial.print("#");
    else
      Serial.print(".");
  }
}

//
// フォントデータの表示
// buf(in) : フォント格納アドレス
//
void fontdisp2(uint8_t* buf) {
  uint8_t bn= SDSfonts.getRowLength();                // 1行当たりのバイト数取得
  Serial.print(SDSfonts.getWidth(),DEC);             // フォントの幅の取得
  Serial.print("x");      
  Serial.print(SDSfonts.getHeight(),DEC);            // フォントの高さの取得
  Serial.print(" ");      
  Serial.println((uint16_t)SDSfonts.getCode(), HEX); // 直前し処理したフォントのコード表示

  for (uint8_t i = 0; i < SDSfonts.getLength(); i += bn ) {
      for (uint8_t j = 0; j < bn; j++) {
        bitdisp(buf[i+j]);
      }
      Serial.println("");
  }
  Serial.println("");
} 

//
// 指定した文字列を指定したサイズで表示する
// pSJIS(in) SJIS文字列
// sz(in)    フォントサイズ(8,10,12,14,16,20,24)
//
void fontDump(char* pSJIS, uint8_t sz) {
  uint8_t buf[MAXFONTLEN]; // フォントデータ格納アドレス(最大24x24/8 = 72バイト)

  SDSfonts.open();                                   // フォントのオープン
  SDSfonts.setFontSize(sz);                          // フォントサイズの設定
  while ( pSJIS = SDSfonts.getFontData(buf, pSJIS) ) // フォントの取得
    fontdisp2(buf);                                 // フォントパターンの表示
  SDSfonts.close();                                  // フォントのクローズ
}

void setup() {
  Serial.begin(115200);                     // シリアル通信の初期化
  while(!Serial); 
  SDSfonts.init(10);                        // フォント管理の初期化
  Serial.println(F("sdfonts liblary"));
  char str[] = {0x7a,0x7b,0x7c,0x7d,0x7e,0xa2,0xb1,0xb2,0xb3,0};// z{|}~｢ｱｲｳ
  fontDump(str,14);
  fontDump("\x82\xa0\x82\xa2\x82\xa4\x82\xa6\x82\xa8",16);//あいうえお
  fontDump("\x82\xa0\x82\xa2\x82\xa4\x82\xa6\x82\xa8",20);//あいうえお
  fontDump("\x8d\xe9\x8B\xCA\x81\x99\x82\xB3\x82\xA2\x82\xBD\x82\xDC", 24); // 埼玉☆さいたま
}

void loop() {
}
