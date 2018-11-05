//
// 美咲フォントライブラリサンプルプログラム by たま吉さん
//  2018/01/31 作成
//

#include <misakiSJIS.h>

//
// フォントパターンをコンソールに表示する
// 引数
//  pSJIS 表示する文字列
//  fore  ドットに使用するキャラクタ
//  back  空白に使用するキャラクタ
//
// ※半角文字は全角文字に置き換えを行う
//
void banner(char * pSJIS, char* fore, char* back) {
  int n=0;
  byte buf[20][8];  //160x8ドットのバナー表示パターン

  // バナー用パターン作成
  while(*pSJIS)
    pSJIS = getFontData(&buf[n++][0], pSJIS,0);  // フォントデータの取得
    
  // バナー表示
  for (byte i=0; i < 8; i++) {
    for (byte j=0; j < n; j++) 
        for (byte k=0; k<8;k++)
          Serial.print(bitRead(buf[j][i],7-k) ? fore: back);
    Serial.println();    
  }
  Serial.println();
}

void setup() {
  char str[] = {0x7a,0x7b,0x7c,0x7d,0x7e,0xa2,0xb1,0xb2,0xb3,0};// z{|}~｢ｱｲｳ
  Serial.begin(115200);
  banner("Z[\\]^_`abc","$"," ");
  banner(str,"$"," ");
  banner("\x82\xa0\x82\xa2\x82\xa4\x82\xa6\x82\xa8","##","  "); //あいうえお
  banner("\x8d\xe9\x8B\xCA\x81\x99\x82\xB3\x82\xA2\x82\xBD\x82\xDC","[]","  "); // 埼玉☆さいたま  
}

void loop() {
}



