//
// file:tTermscreen.cpp
// ターミナルスクリーン制御ライブラリ for Arduino STM32
//  V1.0 作成日 2017/03/22 by たま吉さん
//  修正日 2017/03/26, 色制御関連関数の追加
//  修正日 2017/03/30, moveLineEnd()の追加,[HOME],[END]の編集キーの仕様変更
//  修正日 2017/06/27, 汎用化のための修正
//  修正日 2018/08/22, KEY_F(n)をKEY_F1,KEY_F2 .. の定義に変更対応
//  修正日 2018/08/23, SC_KEY_XXX をKEY_XXXに変更
//  修正日 2018/08/23, 全角文字(SJIS)対応
//  修正日 2018/08/29 editLine()（全角対応版）の追加
//  修正日 2018/09/14 edit() [F1]でのクリア時、ホーム戻り追加

#include <string.h>
#include "tTermscreen.h"

// http://katsura-kotonoha.sakura.ne.jp/prog/c/tip00010.shtml
//*********************************************************
// 文字列 str の str[nPos] について、
//   ０ …… １バイト文字
//   １ …… ２バイト文字の一部（第１バイト）
//   ２ …… ２バイト文字の一部（第２バイト）
// のいずれかを返す。
//*********************************************************
#define jms1(c) (((0x81<=c)&&(c<=0x9F))||((0xE0<=c)&&(c<=0xFC))) 
#define jms2(c) ((0x7F!=c)&&(0x40<=c)&&(c<=0xFC))
int isJMS( uint8_t *str, uint16_t nPos ) {
	int i;
	int state; // { 0, 1, 2 }

	state = 0;
	for( i = 0; str[i] != '\0'; i++ )	{
		if      ( ( state == 0 ) && ( jms1( str[i] ) ) ) state = 1; // 0 -> 1
		else if ( ( state == 1 ) && ( jms2( str[i] ) ) ) state = 2; // 1 -> 2
		else if ( ( state == 2 ) && ( jms1( str[i] ) ) ) state = 1; // 2 -> 1
		else                                             state = 0; // 2 -> 0, その他
		// str[nPos] での状態を返す。
		if ( i == nPos ) return state;
	}
	return 0;
}//isJMS

//******* mcurses用フック関数の定義(開始)  *****************************************
static tTermscreen* tsc = NULL;

// シリアル経由1文字出力

static void Arduino_putchar(uint8_t c) {
  if (tsc->getSerialMode() == 0)
     Serial.write(c);
  else if (tsc->getSerialMode() == 1)
     Serial1.write(c);   
}

// シリアル経由1文字入力
static char Arduino_getchar() {
  if (tsc->getSerialMode() == 0) {
    while (!Serial.available());
    return Serial.read();
  } else if (tsc->getSerialMode() == 1) {
    while (!Serial1.available());
    return Serial1.read();    
  } else {
    while (!Serial.available());
    return Serial.read();    
  }
}
//******* mcurses用フック関数の定義(終了)  *****************************************

//****** シリアルターミナルデバイス依存のメンバー関数のオーバーライド定義(開始) ****

// カーソルの移動
// pos_x,pos_yは本関数のみでのみ変更可能
// カーソルの表示も行う
void tTermscreen::MOVE(uint8_t y, uint8_t x) {
  ::move(y,x);
  pos_x = x;
  pos_y = y;
};

// 文字の表示
void tTermscreen::WRITE(uint8_t x, uint8_t y, uint8_t c) {
  ::move(y,x);
  ::addch(c);
  ::move(pos_y, pos_x);
}
    
void tTermscreen::CLEAR() {
  ::clear();
}

// 行の消去
void tTermscreen::CLEAR_LINE(uint8_t l) {
  ::move(l,0);  ::clrtoeol();  // 依存関数  
}

// スクロールアップ
void tTermscreen::SCROLL_UP() {
  ::scroll();
}

// スクロールダウン
void tTermscreen::SCROLL_DOWN() {
  INSLINE(0);
}

// 指定行に1行挿入(下スクロール)
void tTermscreen::INSLINE(uint8_t l) {
  ::move(l,0);
  ::insertln();
  ::move(pos_y,pos_x);
}

// 依存デバイスの初期化
// シリアルコンソール mcursesの設定
void tTermscreen::INIT_DEV() {
  // mcursesの設定
  ::setFunction_putchar(Arduino_putchar);  // 依存関数
  ::setFunction_getchar(Arduino_getchar);  // 依存関数
  ::initscr();                             // 依存関数
  ::setscrreg(0,height-1);
  serialMode = 0;
  tsc = this;
}

// キー入力チェック
uint8_t tTermscreen::isKeyIn() {
 if(serialMode == 0) {
	if (Serial.available())
       return get_ch();
    else
       return 0;
  } else if (serialMode == 1) {
	if (Serial1.available())
       return get_ch();
    else
       return 0;
	}
  return 0;
}

// 文字入力
uint8_t tTermscreen::get_ch() {
  uint8_t c = getch();
  return c;
}

// キー入力チェック(文字参照)
int16_t tTermscreen::peek_ch() {
 if(serialMode == 0)
    return Serial.peek();
 if(serialMode == 1)
    return Serial1.peek();
  return 0;
}

// カーソルの表示/非表示
// flg: カーソル非表示 0、表示 1、強調表示 2
void tTermscreen::show_curs(uint8_t flg) {
    flgCur = flg;
    ::curs_set(flg);  // 依存関数
}

// カーソルの消去
void tTermscreen::draw_cls_curs() {  

}

// 文字色指定
void tTermscreen::setColor(uint16_t fc, uint16_t bc) {
  static const uint16_t tbl_fcolor[]  =
     { F_BLACK,F_RED,F_GREEN,F_BROWN,F_BLUE,F_MAGENTA,F_CYAN,A_NORMAL,F_YELLOW};
  static const uint16_t tbl_bcolor[]  =
     { B_BLACK,B_RED,B_GREEN,B_BROWN,B_BLUE,B_MAGENTA,B_CYAN,B_WHITE,B_YELLOW};

  if ( fc <= 8 && bc <= 8 )
     attrset(tbl_fcolor[fc]|tbl_bcolor[bc]);  // 依存関数
}

// 文字属性
void tTermscreen::setAttr(uint16_t attr) {
  static const uint16_t tbl_attr[]  =
    { A_NORMAL, A_UNDERLINE, A_REVERSE, A_BLINK, A_BOLD };
  
  if ( attr <= 4 )
     attrset(tbl_attr[attr]);  // 依存関数
}


//****** シリアルターミナルデバイス依存のメンバー関数のオーバーライド定義(終了) ****

//****** 全角文字対応のための関数再定義・追加**************************************

// 文字の取得（シフトJIS対応)
uint16_t tTermscreen::get_wch() {
  uint8_t ch1,ch2;           // 入力文字
  uint16_t wch;              // 2バイト文字コード
  ch1 = get_ch();
  if (isShiftJIS(ch1)) {
     ch2 = get_ch();
     wch = ch1<<8 | ch2;
  } else {
    wch = ch1;
  }
  return wch;
}

// カーソルを1文字分次に移動(全角対応)
void tTermscreen::movePosNextChar() {
  if (pos_x+1 < width) {
    if ( IS_PRINT( VPEEK(pos_x ,pos_y)) ) {
      if ( isShiftJIS(VPEEK(pos_x ,pos_y)) ) {
        // 現在位置が全角1バイト目の場合,2バイト分移動する
        if (pos_x+2 < width) {
          MOVE(pos_y, pos_x+2);
        } else {
          if (pos_y+1 < height) {
            if ( IS_PRINT(VPEEK(0, pos_y + 1)) ) {
              MOVE(pos_y+1, 0);
            }
          }
        }
      } else { 
        MOVE(pos_y, pos_x+1);
      }
    }
  } else {
    if (pos_y+1 < height) {
      if ( IS_PRINT(VPEEK(0, pos_y + 1)) ) {
        MOVE(pos_y+1, 0);
      }
    }
  }
}

// カーソルを次行に移動(全角対応)
void tTermscreen::movePosNextLineChar() {
  if (pos_y+1 < height) {
    if ( IS_PRINT(VPEEK(pos_x, pos_y + 1)) ) {
      // カーソルを真下に移動
      if ( (isJMS(&VPEEK(0,pos_y+1),pos_x) == 2) &&  (pos_x > 0) ) {
         // 真下が全角2バイト目の場合、全角1バイト目にカーソルを移動する 
         MOVE(pos_y+1, pos_x-1);
      } else {
         MOVE(pos_y+1, pos_x);
      }
    } else {
      // カーソルを次行の行末文字に移動
      int16_t x = pos_x;
      for(;;) {
        if (IS_PRINT(VPEEK(x, pos_y + 1)) ) 
           break;  
        if (x > 0)
          x--;
        else
          break;
      }
      if ( !isShiftJIS(VPEEK(x ,pos_y+1)) && x > 0 && isShiftJIS(VPEEK(x-1 ,pos_y+1)) ) {
         MOVE(pos_y+1, x-1);
       } else {
         MOVE(pos_y+1, x);      
       }
    }
  } else if (pos_y+1 == height) {
    edit_scrollUp();    
  }
}

// カーソルを1文字分前に移動(全角対応)
void tTermscreen::movePosPrevChar() {
  if (pos_x > 0) {
    if ( IS_PRINT(VPEEK(pos_x-1 , pos_y)) ) {
        // 1つ前の文字が全角2バイト目かをチェック
        if ( (pos_x -2 >= 0) && (isJMS(&VPEEK(0,pos_y),pos_x-1) !=2 ) ) {
          MOVE(pos_y, pos_x-1);
        } else if ( (pos_x -2 >= 0) && isShiftJIS(VPEEK(pos_x-2 , pos_y)) ) {
          // 全角文字対応
          MOVE(pos_y, pos_x-2);
        } else {
          MOVE(pos_y, pos_x-1);
        }
    }
  } else {
   if(pos_y > 0) {
      if ( IS_PRINT(VPEEK(width-1, pos_y-1)) ) {
        if ( isShiftJIS(VPEEK(width-2 , pos_y-1)) ) {
          // 全角文字対応
          MOVE(pos_y-1, width - 2);
        } else {
          MOVE(pos_y-1, width - 1);
        }
      } 
    }
  }
}

// カーソルを前行に移動(全角対応)
void tTermscreen::movePosPrevLineChar() {
  if (pos_y > 0) {
    if ( IS_PRINT(VPEEK(pos_x, pos_y-1)) ) {
      // カーソルを真上に移動
      if ( (isJMS(&VPEEK(0,pos_y-1),pos_x) == 2) &&  (pos_x > 0) ) {
         // 真上が全角2バイト目の場合、全角1バイト目にカーソルを移動する 
         MOVE(pos_y-1, pos_x-1);
      } else {
         MOVE(pos_y-1, pos_x);
      }
    } else {
      // カーソルの真上に文字が無い場合は、前行の行末文字に移動する
      int16_t x = pos_x;
      for(;;) {
        if (IS_PRINT(VPEEK(x, pos_y - 1)) ) 
           break;  
        if (x > 0)
          x--;
        else
          break;
      }      
      if ( !isShiftJIS(VPEEK(x ,pos_y-1)) && (x > 0) && isShiftJIS(VPEEK(x-1 ,pos_y-1)) ) {
         // 行末が全角2バイト目の場合、カーソルを全角1バイト目に移動する
         MOVE(pos_y-1, x-1);
       } else {
         MOVE(pos_y-1, x);      
       }
    }
  } else if (pos_y == 0){
    edit_scrollDown();
  }
}

// カーソルを行末に移動(全角対応)
void tTermscreen::moveLineEnd() {
  int16_t x = width-1;
  for(;;) {
    if (IS_PRINT(VPEEK(x, pos_y)) ) 
       break;  
    if (x > 0)
      x--;
    else
      break;
  }
  if (x>1 && (isJMS(&VPEEK(0,pos_y),x) == 2) ) {
    // シフトJIS２バイト目の場合、カーソルを１バイト目に移動する 
    x--;
  }
  MOVE(pos_y, x);     
}

// 指定行を削除
void tTermscreen::deleteLine(uint16_t l) {
  if (l < height-1) {
    memmove(&VPEEK(0,l), &VPEEK(0,l+1), width*(height-1-l));
  }
  memset(&VPEEK(0,height-1), 0, width);
  refresh();
}

// 現在のカーソル位置の文字削除(全角対応)

void tTermscreen::delete_char() {
  uint8_t* start_adr = &VPEEK(pos_x,pos_y);
  uint8_t* top = start_adr;
  uint16_t ln = 0;

  if (!*top) {
    if (pos_y < height-1 && pos_x == 0) {
       // 空白行を詰める
      deleteLine(pos_y);
      refresh();
      return;
    } else {
       return; // 0文字削除不能
    }
  }
    
  while( *top ) { ln++; top++; } // 行端,長さ調査
  if (isShiftJIS(*start_adr) && ln>=2) {
    memmove(start_adr, start_adr + 2, ln-2); // 2文字詰める
    *(top-1) = 0;
    *(top-2) = 0;
  } else if ( ln >=1 ) {
    memmove(start_adr, start_adr + 1, ln-1); // 1文字詰める
    *(top-1) = 0; 
  }

  for (uint8_t i=0; i < (pos_x+ln)/width+1; i++)
    refresh_line(pos_y+i);   
  MOVE(pos_y,pos_x);
  return;
}

// 行の再表示
void tTermscreen::refresh_line(uint16_t l) {
  CLEAR_LINE(l);
  for (uint16_t j = 0; j < width; j++) {
    if( IS_PRINT( VPEEK(j,l) )) { 
      WRITE(VPEEK(j,l));
    }
  }
}

// 文字の出力
void tTermscreen::putch(uint8_t c) {
  VPOKE(pos_x, pos_y, c); // VRAMへの書込み
  WRITE(c);
  movePosNextNewChar();
}

// 文字の出力（シフトJIS対応)
void tTermscreen::putwch(uint16_t c) {
  if (c>0xff) { // 2バイト文字
   VPOKE(pos_x, pos_y, c>>8);     // VRAMへの書込み
   VPOKE(pos_x+1, pos_y, c&0xff); // VRAMへの書込み
   WRITE(c>>8); WRITE(c&0xff);
   
   movePosNextNewChar();
   movePosNextNewChar();
  } else {     // 1バイト文字
   VPOKE(pos_x, pos_y, c);        // VRAMへの書込み
   WRITE(c);
   movePosNextNewChar();
  }  
}

// 文字の挿入
void tTermscreen::Insert_char(uint16_t c) {  
  uint8_t* start_adr = &VPEEK(pos_x,pos_y);
  uint8_t* last = start_adr;
  uint16_t ln = 0;
  uint8_t clen = (c>0xff) ? 2:1 ; // 文字バイト数
  
  // 入力位置の既存文字列長(カーソル位置からの長さ)の参照
  while( *last ) {
    ln++;
    last++;
  }
  if (ln == 0 || flgIns == false) {
     // 文字列長さが0または上書きモードの場合、そのまま1文字表示
    if (pos_y + (pos_x+ln+clen)/width >= height) {
      // 最終行を超える場合は、挿入前に1行上にスクロールして表示行を確保
      scroll_up();
      start_adr-=width;
      MOVE(pos_y-1, pos_x);
    } else  if ( (pos_x + ln >= width-1) && !VPEEK(width-1,pos_y) ) {
       // 画面左端に1文字を書く場合で、次行と連続でない場合は下の行に1行空白を挿入する
       Insert_newLine(pos_y+(pos_x+ln)/width);       
    }
    putwch(c);
  } else {
     // 挿入処理が必要の場合
    if (pos_y + (pos_x+ln+clen)/width >= height) {
      // 最終行を超える場合は、挿入前に1行上にスクロールして表示行を確保
      scroll_up();
      start_adr-=width;
      MOVE(pos_y-1, pos_x);
    } else  if ( ((pos_x + ln +clen)%width == width-clen) && !VPEEK(pos_x + ln , pos_y) ) {
       // 画面左端に1文字を書く場合で、次行と連続でない場合は下の行に1行空白を挿入する
          Insert_newLine(pos_y+(pos_x+ln)/width);
    }
    // 1文字挿入のために1文字分のスペースを確保
    memmove(start_adr+clen, start_adr, ln);
    if (clen ==1) {
      *start_adr=c; // 確保したスペースに1文字表示
      movePosNextNewChar();

    } else {
      *start_adr     = (c>>8);   // 確保したスペースに1バイト目
      *(start_adr+1) = c & 0xff; // 確保したスペースに2バイト目
      movePosNextNewChar();
      movePosNextNewChar();
    }
    
    // 挿入した行の再表示
    for (uint8_t i=0; i < (pos_x+ln)/width+1; i++)
       refresh_line(pos_y+i);   
    MOVE(pos_y,pos_x);
  }
}

// ライン編集（全角対応版）
// 中断の場合、0を返す
uint8_t tTermscreen::editLine() {
  uint16_t basePos_x = pos_x;
  uint16_t basePos_y = pos_y;
  uint16_t ch;  // 入力文字  
  
  show_curs(true);
  for(;;) {
    ch = get_wch();
    switch(ch) {
      case KEY_CR:         // [Enter]キー
        show_curs(false);
        text = &VPEEK(basePos_x, basePos_y);
        return 1;
        break;
 
      case KEY_HOME:       // [HOMEキー] 行先頭移動
        locate(basePos_x, basePos_y);
        break;
        
      case KEY_F5:         // [F5],[CTRL_R] 画面更新
        //beep();
        refresh();  break;

      case KEY_END:        // [ENDキー] 行の右端移動
         moveLineEnd();
         break;

      case KEY_IC:         // [Insert]キー
        flgIns = !flgIns;
        break;        

      case KEY_BACKSPACE:  // [BS]キー
        if (pos_x > basePos_x) {
          movePosPrevChar();
          delete_char();
        }
        break;        

      case KEY_RIGHT:      // [→]キー
        if (pos_x < width-1) {
          movePosNextChar();
        }
        break;

      case KEY_LEFT:       // [←]キー
        if (pos_x > basePos_x) {
          movePosPrevChar();
        }
        break;

      case KEY_DC:         // [Del]キー
      case KEY_CTRL_X:
        delete_char();
        break;        
    
      case KEY_CTRL_C:   // [CTRL_C] 中断
      case KEY_ESCAPE:
        return 0;

      default:               // その他
      if (IS_PRINT(ch) && (pos_x <width-1) ) {
          Insert_char(ch);
        }  
        break;
    }
  }
}

// スクリーン編集
uint8_t tTermscreen::edit() {
  uint16_t ch;  // 入力文字  
  for(;;) {
    ch = get_wch();   
    show_curs(false);
    switch(ch) {
      case KEY_CR:         // [Enter]キー
        show_curs(true);
        return enter_text();
        break;

      case KEY_F1:        // [F1],[CTRL+L] 画面クリア
        cls();
        locate(0,0);
        break;
 
      case KEY_HOME:      // [HOME]キー 行先頭移動
        locate(0, pos_y);
        break;
        
      case KEY_NPAGE:      // [PageDown] 表示プログラム最終行に移動
        if (pos_x == 0 && pos_y == height-1) {
          edit_scrollUp();
        } else {
          moveBottom();
        }
        break;
      
      case KEY_PPAGE:     // [PageUP] 画面(0,0)に移動
        if (pos_x == 0 && pos_y == 0) {
          edit_scrollDown();
        } else {
          locate(0, 0);
        }  
        break;
        
      case KEY_F5:         // [F5],[CTRL_R] 画面更新
        beep();
        refresh();  break;

      case KEY_END:        // [END]キー 行の右端移動
         moveLineEnd();
         break;

      case KEY_IC:         // [Insert]キー
        flgIns = !flgIns;
        break;        

      case KEY_BACKSPACE:  // [BS]キー
        movePosPrevChar();
        delete_char();
        break;        

      case KEY_DC:         // [Del]キー
      case KEY_CTRL_X:
        delete_char();
        break;        
      
      case KEY_RIGHT:      // [→]キー
        movePosNextChar();
        break;

      case KEY_LEFT:       // [←]キー
        movePosPrevChar();
        break;

      case KEY_DOWN:       // [↓]キー
        movePosNextLineChar();
        break;
      
      case KEY_UP:         // [↑]キー
        movePosPrevLineChar();
        break;

      case KEY_F3:        // [F3],[CTRL N] 行挿入
        Insert_newLine(pos_y);       
        break;

      case KEY_F2:        // [F2], [CTRL D] 行削除
        clerLine(pos_y);
        break;

      case KEY_F7:        // [F7] 行の分割
        splitLine();
        break;

      case KEY_F8:        // [F8] 行の結合
        margeLine();
        break;
      
      default:            // その他 可視可能文字は挿入表示
      
      if (IS_PRINT(ch)) {
        Insert_char(ch);
      }  
      break;
    }
    show_curs(true);
  };
   show_curs(true);
}

