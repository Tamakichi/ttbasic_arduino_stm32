//
// SJIS版フォント利用ライブラリ クラス定義 SDSfonts.h 
// 作成 2018/10/27 by たま吉さん
// 修正 2018/11/08 isHkana(), isZenkaku(),HantoZen()をpublicに変更

#ifndef ___SDSfonts_h___
#define ___SDSfonts_h___

#include "SDSfontsConfig.h"
#include <Arduino.h>
#include <SPI.h>

#if SDSFONTS_USE_SDFAT == 1
  #include <SdFat.h>
  #if ENABLE_EXTENDED_TRANSFER_CLASS == 1
    #define MYSDCLASS SdFatEX
  #else
    #define MYSDCLASS SdFat
  #endif
#else
  #include <SD.h>
  #define MYSDCLASS SDClass
#endif


#define SDSFONTNUM  14   // 登録フォント数
#define FULL_OFST   7    // フォントサイズから全角フォント種類変換用オフセット値
#define MAXFONTLEN  72   // 最大フォントバイトサイズ(=24x24フォント)
#define MAXSIZETYPE 7    // フォントサイズの種類数

// フォントサイズ
#define  SDSFONT8    0   // 8ドット美咲フォント
#define  SDSFONT10   1   // 10ドット nagaフォント
#define  SDSFONT12   2   // 12ドット東雲フォント
#define  SDSFONT14   3   // 14ドット東雲フォント
#define  SDSFONT16   4   // 16ドット東雲フォント
#define  SDSFONT20   5   // 20ドットJiskanフォント
#define  SDSFONT24   6   // 24ドットXフォント

// フォント管理テーブル
typedef struct FontInfo {
  uint32_t    idx_addr;  // インデックス格納先頭アドレス
  uint32_t    dat_addr;  // フォントデータ格納先頭アドレス
  uint16_t    f_num;     // フォント格納数
  uint8_t     b_num;     // フォントあたりのバイト数
  uint8_t     w;         // 文字幅
  uint8_t     h;         // 文字高さ
} FontInfo;

class sdsjisfonts {
  // メンバー変数
  private:
    uint8_t   _fontNo;      // 利用フォント種類
    uint8_t   _fontSize;    // 利用フォントサイズ
    uint8_t   _CSpin;       // SDカードCSピン
    uint16_t  _code;        // 直前に処理した文字コード(utf16)
    bool	    _lcdmode;     // グラフィック液晶モード
    MYSDCLASS &_mSD;        // SDオブジェクトの参照
    File	    fontFile;     // ファイル操作オブジェクト
    
  // メンバー関数
  public:
#if SDSFONTS_USE_SDFAT == 1
  sdsjisfonts(SdFat& rsd) : _mSD(rsd) {
#else
  sdsjisfonts(MYSDCLASS& rsd = SD) : _mSD(rsd) {
#endif
    _fontSize = SDSFONT8;
      _fontNo   = SDSFONT8+FULL_OFST;
      _code     = 0;
      _lcdmode  = false;
    };
    
    bool init(uint8_t cs)  ;                               // 初期化
	  void setLCDMode(bool flg);                             // グラフィック液晶モードの設定
	  void setFontSizeAsIndex(uint8_t sz);                   // 利用サイズを番号で設定
    uint8_t getFontSizeIndex();                            // 現在利用フォントサイズの番号取得      
    void setFontSize(uint8_t sz);                          // 利用サイズの設定
    uint8_t getFontSize();                                 // 現在利用フォントサイズの取得      

    boolean getFontData(byte* fontdata,uint16_t sjis);     // 指定したSJISコードに該当するフォントデータの取得
    char*   getFontData(byte* fontdata,char *pSJIS);       // 指定したSJIS文字列の先頭のフォントデータの取得
    uint8_t getRowLength();                                // 1行のバイト数
    uint8_t getWidth();                                    // 現在利用フォントの幅の取得
    uint8_t getHeight();                                   // 現在利用フォントの高さの取得
    uint8_t getLength();                                   // 現在利用フォントのデータサイズ
    bool open(void);                                       // フォントファイルのオープン
    void close(void);                                      // フォントファイルのクローズ
    uint16_t getCode() {return _code;};                    // 直前に処理した文字コード
    uint8_t isHkana(uint16_t sjis);                        // 半角カナ判定
    uint8_t isZenkaku(uint8_t c);                          // 全角判定
    uint16_t HantoZen(uint16_t sjis);                      // 半角全角変換
   
  private:
    void setFontNo(uint8_t fno);                              // 利用フォント種類の設定 fno : フォント種別番号 (0-13)
    uint8_t getFontNo();                                      // 現在の利用フォント種類の取得
    uint16_t read_code(uint16_t pos);                         // ROM上検索テーブルのフォントコードを取得する
    int16_t findcode(uint16_t  sjiscode);                     // SJISコードに該当するテーブル上のフォントコードを取得する
    boolean getFontDataBySJIS(byte* fontdata, uint16_t sjis); // 種類に該当するフォントデータの取得
    uint32_t cnvAddres(uint8_t pos, uint8_t ln);  
    bool fontfile_read(uint32_t pos, uint8_t* dt, uint8_t sz) ;
};

extern sdsjisfonts SDSfonts;

#endif
