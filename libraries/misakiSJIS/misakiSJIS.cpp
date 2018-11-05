// 
// 美咲フォントドライバ v1.1 by たま吉さん 2018/02/12
// 内部フラッシュメモリバージョン
// 
// 修正 2018/02/12,Arduino Uno(AVR)でのchar符号付き挙動の弊害対応
//

#include <avr/pgmspace.h>
#include <arduino.h>
#include "misakiSJIS.h"
#include "misakiSJISFontData.h"

// 全角判定
inline uint8_t isZenkaku(uint8_t c){
   return (((c>=0x81)&&(c<=0x9f))||((c>=0xe0)&&(c<=0xfc))) ? 1:0;
}

// nバイト読込
// rcvdata: 読込データ格納アドレス
// n      : 読込みデータ数
// 戻り値  : 読み込んだバイト数
//
byte Sequential_read(uint32_t address, byte* rcvdata, byte n)  {
  for (uint16_t i = 0; i < n ; i++) 
    rcvdata[i] = pgm_read_byte(address + fdata + i);
 return n;
}

// フォントコード検索
// (コードでテーブルを参照し、フォントコードを取得する)
// sjiscode(in) SJISコード
// 戻り値    該当フォントがある場合 フォントコード(0-FTABLESIZE)
//           該当フォントが無い場合 -1

int16_t findcode(uint16_t sjiscode)  {
 int16_t  t_p = 0;            //　検索範囲上限
 int16_t  e_p = FTABLESIZE-1; //  検索範囲下限
 int16_t  pos;
 uint16_t d = 0;
 uint8_t  flg_stop = 0;
 
 while(true) {
   pos = t_p + ((e_p - t_p+1)>>1);
   d = (uint16_t)pgm_read_word(ftable+pos);
   if (d == sjiscode) {
     // 等しい
     flg_stop = 1;
     break;
   } else if (sjiscode > d) {
     // 大きい
     t_p = pos + 1;
     if (t_p > e_p) {
       break;
     }
   } else {
     // 小さい
    e_p = pos -1;
    if (e_p < t_p) 
      break;
   }
 } 

 if (!flg_stop) {
    return -1;
 }
 return pos;    
}

//
// SJISに対応する美咲フォントデータ8バイトを取得する
//   data(out): フォントデータ格納アドレス
//   sjis(in): SJISコード
//   戻り値: true 正常終了１, false 異常終了
//
boolean getFontDataBySJIS(byte* fontdata, uint16_t sjis) {
  int16_t code;
  uint32_t addr;
  boolean rc = false;

  if ( 0 > (code  = findcode(sjis))) { 
    // 該当するフォントが存在しない
    code = findcode(0x81A0);  // ▢
    rc = false;  
  }
    
  addr = ((uint32_t)code)<<3;
  if ( FONT_LEN  == Sequential_read(addr, fontdata, (byte)FONT_LEN) ) 
    rc =  true;
  return rc;
}

//
// SJIS半角コードをSJIS全角コードに変換する
// (変換できない場合は元のコードを返す)
//   sjis(in): SJIS文字コード
//   戻り値: 変換コード
uint16_t HantoZen(uint16_t sjis) {
  if (sjis < 0x20 || sjis > 0xdf) 
     return sjis;
  if (sjis < 0xa1)
    return  pgm_read_word(&zentable[sjis-0x20]);
  return pgm_read_word(&zentable[sjis+95-0xa1]);
}

// 指定したSJIS文字列の先頭のフォントデータの取得
//   data(out): フォントデータ格納アドレス
//   pSJIS(in): SJIS文字列
//   h2z(in)  : true :半角を全角に変換する false: 変換しない 
//   戻り値   : 次の文字列位置、取得失敗の場合NULLを返す
//
char* getFontData(byte* fontdata, char *pSJIS, bool h2z) {
  uint16_t sjis = 0;

  if (pSJIS == NULL || *pSJIS == 0) {
    return NULL;
  }
 
  sjis = (uint8_t)*pSJIS;  
  if ( isZenkaku(sjis) ) {
    sjis<<=8;
    pSJIS++;
    if (*pSJIS == 0) {
      return NULL;
    }
    sjis += (uint8_t)*pSJIS;
  }  
  pSJIS++;

  if (h2z) {
    sjis = HantoZen(sjis);
  }
  if (false == getFontDataBySJIS(fontdata, sjis)) {
    return NULL;
  }
  return (pSJIS);
}

// フォントデータテーブル先頭アドレス取得
// 戻り値 フォントデータテーブル先頭アドレス(PROGMEM上)
const uint8_t* getFontTableAddress() {
	return fdata;
}
