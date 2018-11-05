// 
// misakiSJIS.h 美咲フォントドライバ ヘッダーファイル v1.0 by たま吉さん 2018/01/31
// 内部フラッシュメモリバージョン
//

#ifndef misakiSJIS_h
#define misakiSJIS_h

#include <avr/pgmspace.h>
#include <arduino.h>

#define FTABLESIZE     1710      // フォントテーブルデータサイズ
#define FONT_LEN       8         // 1フォントのバイト数

int16_t findcode(uint16_t  sjiscode) ;						              // フォントコード検索
boolean getFontDataBySJIS(byte* fontdata, uint16_t sjis) ;      // SJISに対応する美咲フォントデータ8バイトを取得
uint16_t HantoZen(uint16_t sjis); 				      	              // SJIS半角コードをSJIS全角コードに変換
char* getFontData(byte* fontdata,char *pSJIS,bool h2z=false);   // フォントデータ取得
const uint8_t*  getFontTableAddress();						              // フォントデータテーブル先頭アドレス取得

#endif
