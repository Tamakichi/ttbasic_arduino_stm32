//
// SJIS版フォント利用ライブラリ クラス実装定義 SDSfonts.cpp
// 作成 2018/10/27 by Tamakichi
//

#define MYDEBUG 0 
#define USE_CON 0
 
#include "SDSfonts.h"
#if defined(ARDUINO_ARCH_AVR)
  #include <avr/pgmspace.h>
#endif

#define SD_CS_PIN 10               // SDカード CSピン
#define FONTFILE   "SFONT.BIN"     // フォントファイル名
#define FONT_LFILE "SFONTLCD.BIN"  // グラフィック液晶用フォントファイル名

#define OFSET_IDXA  0
#define OFSET_DATA  3
#define OFSET_FNUM  6
#define OFSET_BNUM  8
#define OFSET_W     9
#define OFSET_H    10
#define RCDSIZ     11

// フォント種別テーブル
static PROGMEM const uint8_t  _finfo[] = {
   0x00,0x00,0x00, 0x00,0x01,0x7E,  0x00,0xbf,  8,  4,  8 , // 0:s_4x8a.hex
   0x00,0x07,0x76, 0x00,0x09,0x76,  0x01,0x00, 10,  5, 10 , // 1:s_5x10a.hex
   0x00,0x13,0x76, 0x00,0x15,0x76,  0x01,0x00, 12,  6, 12 , // 2:s_6x12a.hex
   0x00,0x21,0x76, 0x00,0x23,0x30,  0x00,0xdd, 14,  7, 14 , // 3:s_7x14a.hex
   0x00,0x2F,0x46, 0x00,0x31,0x00,  0x00,0xdd, 16,  8, 16 , // 4:s_8x16a.hex
   0x00,0x3E,0xD0, 0x00,0x40,0x4C,  0x00,0xbe, 40, 10, 20 , // 5:s_10x20a.hex
   0x00,0x5D,0xFC, 0x00,0x5F,0xB6,  0x00,0xdd, 48, 12, 24 , // 6:s_12x24a.hex
   0x00,0x89,0x26, 0x00,0xBE,0xE4,  0x1a,0xdf,  8,  8,  8 , // 7:s_8x8.hex
   0x01,0x95,0xDC, 0x01,0xCB,0x96,  0x1a,0xdd, 20, 10, 10 , // 8:s_10x10.hex
   0x03,0xE4,0xDA, 0x04,0x1A,0x98,  0x1a,0xdf, 24, 12, 12 , // 9:s_12x12.hex
   0x06,0x9F,0x80, 0x06,0xD5,0x3E,  0x1a,0xdf, 28, 14, 14 , // 10:s_14x14.hex
   0x09,0xC5,0xA2, 0x09,0xFB,0x60,  0x1a,0xdf, 32, 16, 16 , // 11:s_16x16.hex
   0x0D,0x57,0x40, 0x0D,0x8C,0xFE,  0x1a,0xdf, 60, 20, 20 , // 12:s_20x20.hex
   0x13,0xD9,0x42, 0x14,0x0E,0xFC,  0x1a,0xdd, 72, 24, 24 , // 13:s_24x24.hex
};

// SJISコード 半角全角変換用テーブル
PROGMEM static const uint16_t zentable[] = {
0x8140,0x8149,0x8168,0x8194,0x8190,0x8193,0x8195,0x8166,
0x8169,0x816A,0x8196,0x817B,0x2124,0x817C,0x8144,0x815E,
0x824F,0x8250,0x8251,0x8252,0x8253,0x8254,0x8255,0x8256,
0x8257,0x8258,0x8146,0x8147,0x8183,0x8181,0x8184,0x8148,
0x8197,0x8260,0x8261,0x8262,0x8263,0x8264,0x8265,0x8266,
0x8267,0x8268,0x8269,0x826A,0x826B,0x826C,0x826D,0x826E,
0x826F,0x8270,0x8271,0x8272,0x8273,0x8274,0x8275,0x8276,
0x8277,0x8278,0x8279,0x816D,0x818F,0x816E,0x814F,0x8151,
0x8165,0x8281,0x8282,0x8283,0x8284,0x8285,0x8286,0x8287,
0x8288,0x8289,0x828A,0x828B,0x828C,0x828D,0x828E,0x828F,
0x8290,0x8291,0x8292,0x8293,0x8294,0x8295,0x8296,0x8297,
0x8298,0x8299,0x829A,0x816F,0x8162,0x8170,0x8160,0x8142,
0x8175,0x8176,0x8141,0x8145,0x8392,0x8340,0x8342,0x8344,
0x8346,0x8348,0x8383,0x8385,0x8387,0x8362,0x815B,0x8341,
0x8343,0x8345,0x8347,0x8349,0x834A,0x834C,0x834E,0x8350,
0x8352,0x8354,0x8356,0x8358,0x835A,0x835C,0x835E,0x8360,
0x8363,0x8365,0x8367,0x8369,0x836A,0x836B,0x836C,0x836D,
0x836E,0x8371,0x8374,0x8377,0x837A,0x837D,0x837E,0x8380,
0x8381,0x8382,0x8384,0x8386,0x8388,0x8389,0x838A,0x838B,
0x838C,0x838D,0x838F,0x8393,0x814A,0x814B,};

// 全角判定
uint8_t sdsjisfonts::isZenkaku(uint8_t c){
   return (((c>=0x81)&&(c<=0x9f))||((c>=0xe0)&&(c<=0xfc))) ? 1:0;
}

//
// SJIS半角コードをSJIS全角コードに変換する
// (変換できない場合は元のコードを返す)
//   sjis(in): SJIS文字コード
//   戻り値: 変換コード

uint16_t sdsjisfonts::HantoZen(uint16_t sjis) {
  if (sjis < 0x20 || sjis > 0xdf) 
     return sjis;
  if (sjis < 0xa1)
    return  pgm_read_word(&zentable[sjis-0x20]);
  return pgm_read_word(&zentable[sjis+95-0xa1]);
}

// 半角カナ判定
uint8_t sdsjisfonts::isHkana(uint16_t sjis) {
  return  ( (sjis >= 0xa1) && (sjis <= 0xdf));
}
    
// フラッシュメモリ上の3バイトをuint32_t型で取り出し
uint32_t sdsjisfonts::cnvAddres(uint8_t pos, uint8_t ln) {
  uint32_t addr;
  pos += ln*RCDSIZ;
  addr =  (((uint32_t)pgm_read_byte(_finfo+pos))<<16)+(((uint32_t)pgm_read_byte(_finfo+pos+1))<<8)+pgm_read_byte(_finfo+pos+2);
  return addr;
}

// 初期化
bool sdsjisfonts::init(uint8_t cs) {
  _CSpin = cs;
  //return SD.begin(_CSpin);
#if SDFONTS_USE_SDFAT == 1
  return _mSD.begin(_CSpin, SDFONTS_SPI_SPEED);
#else
  return _mSD.begin(_CSpin);
#endif  
}

// グラフィック液晶モードの設定
void sdsjisfonts::setLCDMode(bool flg) {
	_lcdmode = flg;
}


// 利用フォント種類の設定 fno : フォント種別番号 (0-13)
void sdsjisfonts::setFontNo(uint8_t fno) {
  _fontNo = fno;
}

// 現在の利用フォント種類の取得
uint8_t sdsjisfonts::getFontNo() {
  return _fontNo;
}

//利用サイズの設定
void sdsjisfonts::setFontSizeAsIndex(uint8_t sz) {
  _fontSize = sz;
  _fontNo = sz+FULL_OFST;
}

// 現在の利用フォントサイズの取得
uint8_t sdsjisfonts::getFontSizeIndex() {
  return _fontSize; 
}
// 利用サイズの設定
void sdsjisfonts::setFontSize(uint8_t sz) {
  if (sz < 10) 
    setFontSizeAsIndex(SDSFONT8);   
  else if (sz < 12) 
    setFontSizeAsIndex(SDSFONT10);   
  else if (sz < 14)
    setFontSizeAsIndex(SDSFONT12);   
  else if (sz < 16)
    setFontSizeAsIndex(SDSFONT14);   
  else if (sz < 20)
    setFontSizeAsIndex(SDSFONT16);   
  else if (sz < 24)
    setFontSizeAsIndex(SDSFONT20);   
  else if (sz >= 24)
    setFontSizeAsIndex(SDSFONT24); 
}

// 現在利用フォントサイズの取得                             
uint8_t sdsjisfonts::getFontSize() {
  return getHeight(); 
}

// 現在利用フォントの幅の取得
uint8_t sdsjisfonts::getWidth() {
  return pgm_read_byte(_finfo +_fontNo * RCDSIZ + OFSET_W);
}
  
// 現在利用フォントの高さの取得
uint8_t sdsjisfonts::getHeight() {
  return pgm_read_byte(_finfo +_fontNo * RCDSIZ + OFSET_H);

}
  
// 現在利用フォントのデータサイズの取得
uint8_t sdsjisfonts::getLength() { 
  return pgm_read_byte(_finfo +_fontNo * RCDSIZ + OFSET_BNUM);
}

//
// フォントファイルのオープン
//
bool sdsjisfonts::open() {
  if (_lcdmode)
	fontFile = _mSD.open(FONT_LFILE, FILE_READ);
  else	
	fontFile = _mSD.open(FONTFILE, FILE_READ);
	
	
  if (!fontFile) {
#if MYDEBUG == 1 && USE_CON == 1    
    Serial.print(F("cant open:"));
    Serial.println(F(FONTFILE));
#endif
    return false;
  }
  return true;  
}

//
// ファイルのクローズ
//
void sdsjisfonts::close(void) {
  fontFile.close(); 
}

//
// フォントファイルからのデータ取得
// pos(in) 読み込み位置
// dt(out) データ格納領域
// sz(in)  データ取得サイズ
//
bool sdsjisfonts::fontfile_read(uint32_t pos, uint8_t* dt, uint8_t sz) {
  if (!fontFile) {
    return false;  
  } else {
    if ( !fontFile.seek(pos) )   
      return false;
 /*	
    for (uint8_t i = 0 ; i < sz; i++) {
      if ( !fontFile.available() ) 
        return false;
      dt[i] = fontFile.read();  
    }
*/
  	if (fontFile.read(dt, sz) != sz)
  		return false;	

  }
  return true;  
}

// フォントコード取得
// ファイル上検索テーブルのフォントコードを取得する
// pos(in) フォントコード取得位置
// 戻り値 該当コード or 0xFFFF (該当なし)
//
uint16_t sdsjisfonts::read_code(uint16_t pos) {
  uint8_t rcv[2];
  uint32_t addr = cnvAddres(OFSET_IDXA, _fontNo) + pos+pos;
  if (!fontfile_read(addr, rcv, 2))  
    return 0xFFFF;
  return  (rcv[0]<<8)+rcv[1]; 
}

// フォントコード検索
// (コードでROM上のテーブルを参照し、フォントコードを取得する)
// sjiscode(in) SJIS コード
// 戻り値    該当フォントがある場合 フォントコード(0-FTABLESIZE)
//           該当フォントが無い場合 -1

int16_t sdsjisfonts::findcode(uint16_t  sjiscode) {
   uint16_t  t_p = 0;                        //　検索範囲上限
   uint16_t  e_p = (((uint16_t)pgm_read_byte(_finfo+_fontNo*RCDSIZ+OFSET_FNUM))<<8) 
                  + pgm_read_byte(_finfo+_fontNo*RCDSIZ+OFSET_FNUM+1) -1;   //  検索範囲下限
   uint16_t  pos;
   uint16_t  d = 0;
   int8_t flg_stop = 0;
 
 while(true) {
    pos = t_p + ((e_p - t_p+1)>>1);
    d = read_code (pos);
    if (d==0xFFFF)
      return -1;  
 	
   if (d == sjiscode) {
     // 等しい
     flg_stop = 1;
     break;
   } else if (sjiscode > d) {
     // 大きい
     t_p = pos + 1;
     if (t_p > e_p) 
       break;
   } else {
     // 小さい
    e_p = pos -1;
    if (e_p < t_p) 
      break;
   }
 } 

 if (!flg_stop)
    return -1;   
 return pos;   
}

//
// SJISに対応するフォントデータを取得する
//   fontdata(out) : フォントデータ格納アドレス
//   sjis(in)      : SJISコード
//   戻り値: true 正常終了１, false 異常終了
//
boolean sdsjisfonts::getFontData(byte* fontdata, uint16_t sjis) {
  boolean flgZenkaku = true;
   
 // 文字コードから全角、半角を判定する
 if (sjis < 0x100) {
     flgZenkaku = false;
 } 
    
  // 半角カナは全角カナに置き換える
  if (isHkana(sjis)) {
    sjis = HantoZen(sjis);
    flgZenkaku = true;
  }
  
  //フォント種別の設定
  if (flgZenkaku) 
    setFontNo(getFontSizeIndex()+FULL_OFST);  // 全角フォント指定
  else
    setFontNo(getFontSizeIndex());            // 半角フォント指定
    
  _code = sjis;
  return getFontDataBySJIS(fontdata, sjis);
}

// 指定したSJIS文字列の先頭のフォントデータの取得
//   fontdata(out) : フォントデータ格納アドレス
//   pSJIS(in)     : SJIS文字列
//   戻り値   : 次の文字列位置、取得失敗の場合NULLを返す
//
char* sdsjisfonts::getFontData(byte* fontdata, char *pSJIS) {
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
 
  if (false == getFontData(fontdata, sjis) )  {
    return NULL;
  }
  return (pSJIS);
}

//
// SJISに対応するフォントデータを取得する
//   data(out): フォントデータ格納アドレス
//   sjis(in): SJISコード
//   戻り値: true 正常終了１, false 異常終了
//
boolean sdsjisfonts::getFontDataBySJIS(byte* fontdata, uint16_t sjis) {  
  int16_t  code;
  uint32_t addr;
  uint8_t  bnum;
 
  code = findcode(sjis);
  if ( 0 > code)  {
    return false;       // 該当するフォントが存在しない
  }
    
  bnum = pgm_read_byte(_finfo+_fontNo*RCDSIZ+OFSET_BNUM);
  addr = cnvAddres(OFSET_DATA, _fontNo ) + (uint32_t)code * (uint32_t)bnum;
  return fontfile_read(addr, fontdata, bnum );
}

//
// フォントデータ1行のバイト数の取得
// 戻り値： バイト数
uint8_t sdsjisfonts::getRowLength() {
    return ( pgm_read_byte( _finfo + _fontNo * RCDSIZ + OFSET_W ) + 7 ) >>3;
}


//
// グローバルオブジェクトの宣言
//
#if SDSFONTS_USE_SDFAT == 1
  MYSDCLASS SD;
#endif
sdsjisfonts SDSfonts(SD);
