//
// file: tTermscreen.h
// ターミナルスクリーン制御ライブラリ ヘッダファイル for Arduino STM32
// V1.0 作成日 2017/03/22 by たま吉さん
//  修正日 2017/03/26, 色制御関連関数の追加
//  修正日 2017/03/30, moveLineEnd()の追加,[HOME],[END]の編集キーの仕様変更
//  修正日 2017/04/02, getScreenByteSize()の追加
//  修正日 2017/04/03, getScreenByteSize()の不具合対応
//  修正日 2017/04/06, beep()の追加
//  修正日 2017/06/27, 汎用化のための修正
//  修正日 2018/08/29 editLine()（全角対応版）の追加
//

#ifndef __tTermscreen_h__
#define __tTermscreen_h__

#include <Arduino.h>
#include "tscreenBase.h"
//#include "tSerialDev.h"
#include <mcurses.h>
//#undef erase()

//class tTermscreen : public tscreenBase, public tSerialDev {
class tTermscreen : public tscreenBase  {
	protected:
    void INIT_DEV();                             // デバイスの初期化
    void MOVE(uint8_t y, uint8_t x);             // キャラクタカーソル移動
    void WRITE(uint8_t x, uint8_t y, uint8_t c); // 文字の表示
    void CLEAR();                                // 画面全消去
    void CLEAR_LINE(uint8_t l);                  // 行の消去
    void SCROLL_UP();                            // スクロールアップ
    void SCROLL_DOWN();                          // スクロールダウン
    void INSLINE(uint8_t l);                     // 指定行に1行挿入(下スクロール)
    
  public:
    void beep() {addch(0x07);};                  // BEEP音の発生
    void show_curs(uint8_t flg);                 // カーソルの表示/非表示
    void draw_cls_curs();                        // カーソルの消去
    void setColor(uint16_t fc, uint16_t bc);     // 文字色指定
    void setAttr(uint16_t attr);                 // 文字属性
    uint8_t get_ch();                            // 文字の取得
    uint8_t isKeyIn();                           // キー入力チェック
    int16_t peek_ch();                           // キー入力チェック(文字参照)
    inline uint8_t getSerialMode()               // シリアルモードの取得
      { return serialMode; };

    // 全角文字対応のためのベースクラスメンバ関数の再定義・追加
    uint8_t isShiftJIS(uint8_t  c) {             // シフトJIS1バイト目チェック
      return (((c>=0x81)&&(c<=0x9f))||((c>=0xe0)&&(c<=0xfc)))?1:0;
    };
  
    void moveLineEnd();                          // カーソルを行末に移動（シフトJIS対応)
    void movePosNextChar();                      // カーソルを1文字分次に移動(全角対応)
    void movePosNextLineChar();                  // カーソルを次行に移動(全角対応)
    void movePosPrevLineChar();                  // カーソルを前行に移動(全角対応)
    void movePosPrevChar();                      // カーソルを1文字分前に移動(全角対応)
    void refresh_line(uint16_t l);               // 行の再表示
    void deleteLine(uint16_t l);                 // 指定行を削除(全角対応)
    void delete_char();                          // 現在のカーソル位置の文字削除(全角対応)
    uint16_t get_wch();                          // 文字の取得（シフトJIS対応)
    void splitLine();                            // カーソル位置で行を分割する
    void margeLine();                            // 現在行の末尾に次の行を結合する
    void putch(uint8_t c);                       // 文字の出力
    void putwch(uint16_t c);                     // 文字の出力（シフトJIS対応)
    void Insert_char(uint16_t c);                // 文字の挿入
    uint8_t edit();                              // スクリーン編集
    virtual uint8_t editLine();                  // ライン編集    
    void WRITE(uint8_t c) { addch(c);};          // 文字の表示
};

#endif
