//
// file: tTFTScreen.cpp
// ILI9341利用ターミナルスクリーン制御クラス
//
// 2017/08/12 修正 SPI2を利用に修正
// 2017/08/25 修正 グラフィック描画対応
// 2017/08/28 スクリーン用メモリに確保済領域指定対応
// 2017/11/05 デバッグ用出力消し忘れミスの対応
// 2018/08/18 修正 init()に横位置補正、縦位置補正引数の追加（抽象クラスとのインタフェース互換のため）
// 2018/08/30 修正 bmpDraw()でモノラルBMP暫定対応
// 2018/08/31 修正 Arduino STM32最新版（mastarブランチ）の場合、Adafruit_ILI9341_STM(修正版)利用に修正
// 2018/08/31 修正 gpeek(),ginp()の実装
// 2018/09/03 修正 WRITE(),drawCus()の高速化（v0.85より2.75倍)
// 2018/09/03 修正 drawFont()の追加、refresh_line()の高層化（V0.85よりスクロール速度10倍),cscroll()のサポート
// 2018/09/14 修正 revt()で色コード0～8が利用できない不具合を対応
//

#include <string.h>
#include "tTFTScreen.h"

#include <SD.h>
#define SD_CS       (PA4)      // SDカードモジュールCS
#define BUFFPIXEL 20
#define SD_BEGIN() SD.begin(F_CPU/4,SD_CS)

#define SD_ERR_INIT       1    // SDカード初期化失敗
#define SD_ERR_OPEN_FILE  2    // ファイルオープン失敗
#define SD_ERR_READ_FILE  3    // ファイル読込失敗
#define SD_ERR_NOT_FILE   4    // ファイルでない(ディレクトリ)
#define SD_ERR_WRITE_FILE 5    //  ファイル書込み失敗

#define TFT_CS      PB11
#define TFT_DC      PB12
#define TFT_RST     -1

#define BOT_FIXED_AREA 0 // Number of lines in bottom fixed area (lines counted from bottom of screen)
#define TOP_FIXED_AREA 0 // Number of lines in top fixed area (lines counted from top of screen)
#define ILI9341_VSCRDEF  0x33
#define ILI9341_VSCRSADD 0x37

// PS/2キーボードドライバプロトタイプ宣言
void setupPS2(uint8_t kb_type);
uint8_t ps2read();
void    endPS2();

// カラーコードテーブル(0 ～ 8:シリアルコンソール色互換）
static const uint16_t tbl_color[]  =
     {  ILI9341_BLACK, ILI9341_RED, ILI9341_GREEN, ILI9341_MAROON, ILI9341_BLUE, ILI9341_MAGENTA, ILI9341_CYAN, ILI9341_WHITE, ILI9341_YELLOW};

// 初期化
void tTFTScreen::init(const uint8_t* fnt, uint16_t ln, uint8_t kbd_type, uint8_t* extmem, uint8_t vmode, int8_t rt, int8_t Hajst, int8_t Vajst,uint8_t ifmode) {
  this->font = (uint8_t*)fnt; // フォントテーブル
#ifdef STM32_R20170323
  // Arduino STM32安定版利用の場合のオブジェクト作成
  this->tft = new Adafruit_ILI9341_STM_TT(TFT_CS, TFT_DC, TFT_RST,2); // Use hardware SPI
#else
  // Arduino STM32 マスタブランチ利用の場合のオブジェクト作成
  pSPI = new SPIClass(2);
  this->tft = new Adafruit_ILI9341_STM(TFT_CS, TFT_DC, TFT_RST, *pSPI); // Use hardware SPI
#endif  
  this->tft->begin();   // TFT利用開始
  setScreen(vmode, rt); // スクリーンモード,画面回転指定
  // コンソールの初期化
  if (extmem == NULL) {
    tscreenBase::init(this->width,this->height, ln);           // テキストフレームバッファを動的獲得して初期化
  } else {
    tscreenBase::init(this->width,this->height, ln, extmem);   // 指定された領域をテキストフレームバッファとして利用
  }	
 
#if PS2DEV == 1
  // PS/2キーボードの初期化
  setupPS2(kbd_type);
#endif
 // シリアルからの制御文字許可
  this->allowCtrl = true;
}


// 依存デバイスの初期化
void tTFTScreen::INIT_DEV() {

}


// スクリーンモード設定
//  引数 
//   mode :スクリーンモード 1 ～ 6
//   rt   :画面回転        0 ～ 3
//
void tTFTScreen::setScreen(uint8_t mode, uint8_t rt) {
  this->tft->setRotation(rt);                      // 画面回転
  this->g_width  = this->tft->width();             // 横ドット数
  this->g_height = this->tft->height();            // 縦ドット数

  this->fontEx = mode;                             // フォント倍率
  this->f_width  = *(font+0)*this->fontEx;         // 横フォントドット数
  this->f_height = *(font+1)*this->fontEx;         // 縦フォントドット数
  this->width  = this->g_width  / this->f_width;   // 横文字数
  this->height = this->g_height / this->f_height;  // 縦文字数
  this->fgcolor = ILI9341_WHITE;                   // 文字色
  this->bgcolor = ILI9341_BLACK;                   // 背景色
  this->tft->setCursor(0, 0);                      // TFTグラフィックカーソル原点
  pos_gx =0;  pos_gy =0;                           // グラフィック座標原点
}

// 文字の表示(※フォント横サイズは8ドットまで対応)    
// 引数
//  cx     : 横座標 0 ～ width-1
//  cy     : 縦座標 0 ～ height-1
//  code   : キャラクターコード
//  fgcolor: 文字色
//  gbcolor: 背景色
// 
//
#ifndef STM32_R20170323 
void  tTFTScreen::drawFont(int16_t cx, int16_t cy, uint8_t code, uint16_t _fgcolor, uint16_t _bgcolor) {
  int16_t gx = cx*f_width;                       // フォント描画開始位置x
  int16_t gy = cy*f_height;                      // フォント描画開始位置y
  int16_t bf_w = *(font+0);                      // ベース横フォントドット数
  int16_t bf_h = *(font+1);                      // ベース縦フォントドット数
  uint8_t* fadr = font+3+((uint16_t)code)*8;     // フォント格納アドレス
  uint16_t bpos = 0;                             // バッファポインタ
  uint8_t font_line;                             // 1行分のデータ
  uint16_t font_color;
  
  // 描画
  tft->setAddrWindow(gx, gy, gx+f_width-1, gy+f_height-1);           // ウィンドウ設定
  for ( uint16_t row = 0; row < bf_h; row++ ) {                      // フォント行処理ループ
    for ( uint8_t hex = 0; hex < fontEx; hex++) {                    // 倍率分同じ行データを送信ループ
      for ( uint16_t col = 0 ; col < bf_w; col++ ) {                 // フォント列処理ループ
        font_color = ( (*fadr) & (0x80>>col) ) ? _fgcolor:_bgcolor;  // フォントドットの取り出し
        for ( uint8_t wex = 0; wex < fontEx; wex++) {                // 倍率分同じ列データを送信ループ
           buf[bpos++] = font_color;
           if (bpos == TFTBUFSIZE ) {
              // バッファ一杯のため、データを送信
              tft->pushColors(buf, TFTBUFSIZE, 0);
              bpos = 0;
           }
        } // 倍率分同じ列データを送信ループ
      } // フォント列処理ループ
    } // 倍率分同じ行データを送信ループ
    fadr++; // フォントデータ取り出し位置 インクリメント
  } // フォント行処理ループ
  
  // バッファ内見送信データの送信
  if (bpos) {
    tft->pushColors(buf, bpos, 0);
  }
}
#endif
  
// 行のリフレッシュ表示
// 引数 
//  l : テキスト行 0 ～
#ifndef STM32_R20170323
  void tTFTScreen::refresh_line(uint16_t l) {
  int16_t bf_w = *(font+0);                      // ベース横フォントドット数
  int16_t bf_h = *(font+1);                      // ベース縦フォントドット数
  uint16_t font_color;                           // フォント色
  uint16_t bpos = 0;                             // バッファポインタ
  uint16_t line = 0;                             // 描画ライン数
  uint16_t* pbuf[2] = {                          // バッファポインタ(書込・転送ローテーション利用のために分割）
    &buf[0], 
    &buf[TFTBUFSIZE/2],
  };
  
  // 1行分のウィンドウ設定
  tft->setAddrWindow(0, f_height*l , f_width*width-1, f_height*l+f_height-1);           
  for (uint8_t row = 0; row < bf_h; row++) {            // フォント高さ分ライン毎のループ
    for (uint8_t hex = 0; hex < fontEx; hex++) {        // 倍率分同じ行データ送信ループ
      for (uint16_t col = 0; col < width; col++) {      // １文字処理毎のループ
        uint8_t c = VPEEK(col, l);                      // フォントの取得
        uint8_t* fadr = font+3+((uint16_t)c)*8+row;     // フォントデータ格納アドレス
        for (uint8_t px = 0; px < bf_w; px++) {         // フォント1ドット取り出しループ
          font_color = ( (*fadr) & (0x80>>px) ) ? fgcolor: bgcolor;  // フォントドットの取り出し      
          for (uint8_t wex = 0; wex < fontEx; wex++) {    // 倍率分同じ横ドットデータ送信ループ
             (pbuf[line&1])[bpos++] = font_color; // バッファに色コード保存
          } // 倍率分同じデータ送信ループ
        } // フォント1ドット取り出しループ
      } // フォント横ドット毎毎のループ
       tft->pushColors(pbuf[line&1], f_width*width, 1);  // バッファ内データのDMA転送
      bpos = 0; // バッファ内インデックスのクリア
      line++;
    } // 倍率分同じ行データ送信ループ
  } // フォントライン毎のルー
  while ((dma_get_isr_bits(DMA1, DMA_CH5) & DMA_ISR_TCIF1)==0); // DMA転送待ち(SPI2 DMA1 CH5)
}
#endif

// キャラクタ画面スクロール
// x: スクロール開始位置 x
// y: スクロール開始位置 y
// w: スクロール幅
// h: スクロール高さ
// d:方向

void tTFTScreen::cscroll(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t d) {
#ifndef STM32_R20170323
  tGraphicScreen::cscroll(x, y, w,h,d);
  //refresh();
#endif
}

  
// カーソル表示
// 引数
//  x : 横座標 0 ～ width-1
//  y : 縦座標 0 ～ height-1
//
uint8_t tTFTScreen::drawCurs(uint8_t x, uint8_t y) {
  uint8_t c= VPEEK(x, y);                                        // カーソル位置のキャラクターコード取得
#ifdef STM32_R20170323
  tft->fillRect(x*f_width,y*f_height,f_width,f_height,fgcolor);  // カーソル位置矩形塗りつぶし
  if (fontEx == 1) 
     // 1倍サイズのフォント描画
     tft->drawBitmap(x*f_width,y*f_height,font+3+((uint16_t)c)*8,f_width,f_height, bgcolor);
  else
     // 2倍サイズ以上のフォント描画
 	   drawBitmap_x2(x*f_width,y*f_height,font+3+((uint16_t)c)*8,f_width/fontEx, f_height/fontEx, bgcolor, fontEx);
#else
  drawFont(x, y, c, bgcolor, fgcolor);
#endif
  return 0;
}

// 文字の表示
// 引数
//  x : 横座標 0 ～ width-1
//  y : 縦座標 0 ～ height-1
//  c : キャラクターコード
//
void tTFTScreen::WRITE(uint8_t x, uint8_t y, uint8_t c) {
#ifdef STM32_R20170323
  tft->fillRect(x*f_width,y*f_height,f_width,f_height, bgcolor);
  if (fontEx == 1)
    tft->drawBitmap(x*f_width,y*f_height,font+3+((uint16_t)c)*8,f_width,f_height, fgcolor);
  else
    drawBitmap_x2(x*f_width,y*f_height,font+3+((uint16_t)c)*8,f_width/fontEx,f_height/fontEx, fgcolor, fontEx);
#else
  drawFont(x, y, c, fgcolor, bgcolor);
#endif
}

// グラフィックカーソル設定
void tTFTScreen::set_gcursor(uint16_t x, uint16_t y) {
  this->pos_gx = x;
  this->pos_gy = y;
}

// グラフィック文字表示
void  tTFTScreen::gputch(uint8_t c) {
  drawBitmap_x2( this->pos_gx, this->pos_gy, this->font+3+((uint16_t)c)*8,
    	           this->f_width/ this->fontEx,  this->f_height/this->fontEx,  this->fgcolor, this->fontEx);

  this->pos_gx += this->f_width;
  if (this->pos_gx + this->f_width >= this->g_width) {
     this->pos_gx = 0;
     this->pos_gy += this->f_height;
  }
};


// 画面全消去
void tTFTScreen::CLEAR() {
  tft->fillScreen( bgcolor);
  pos_gx =0;  pos_gy =0;
}

// 行の消去
void tTFTScreen::CLEAR_LINE(uint8_t l) {
  tft->fillRect(0,l*f_height,g_width,f_height,bgcolor);
}

void tTFTScreen::scrollFrame(uint16_t vsp) {
  tft->writecommand(ILI9341_VSCRSADD);
  tft->writedata(vsp >> 8);
  tft->writedata(vsp);
}

// スクロールアップ
void tTFTScreen::SCROLL_UP() {
 refresh();
}

// スクロールダウン
void tTFTScreen::SCROLL_DOWN() {
  INSLINE(0);
}

// 指定行に1行挿入(下スクロール)
void tTFTScreen::INSLINE(uint8_t l) {
 refresh();
}

// 文字色指定
void tTFTScreen::setColor(uint16_t fc, uint16_t bc) {
  if (fc>8)
	  fgcolor = fc;
	else
	  fgcolor = tbl_color[fc];
	if (bc>8)
    bgcolor = bc;
	else
   bgcolor = tbl_color[bc];
  
}

// 文字属性
void tTFTScreen::setAttr(uint16_t attr) {

}

// ビットマップの拡大描画
// 引数
//  x       : 横描画位置 
//  y       : 縦描画位置
//  bitmap  : モノラルビットマップ画像格納アドレス
//  w       : モノラルビットマップ画像の幅
//  h       : モノラルビットマップ画像の高さ 
//  color   : 描画色
//  ex      : 倍率
//  f       : 背景色描画フラグ 0: 描画しない(デフォルト) 1:描画する
//
void tTFTScreen::drawBitmap_x2(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h,uint16_t color, uint16_t ex, uint8_t f) {
  int16_t b=(w+7)/8; // 横バイト数  
  for(int16_t row = 0; row < h; row++) { // 縦ドット数分ループ
    for(int16_t col = 0; col < w; col++ ) { // 横ドット数分ループ
      if(*(bitmap + row*b + (col>>3)) & (0x80 >> (col & 7))) {
        if (ex == 1) {  // ドットあり
           this->tft->drawPixel(x+col, y+row, color);  //1倍
        } else {
           tft->fillRect(x + col * ex, y + row * ex, ex, ex, color); // ex倍
        }
      } else {
        // ドットなし
        if (f) {
          // 黒を透明扱いしない
          if (ex == 1) {
            this->tft->drawPixel(x+col, y+row, bgcolor);
          } else {
            tft->fillRect(x + col * ex, y + row * ex, ex, ex, bgcolor);
          }
       }
     }
   }
 }
}

// カラービットマップの拡大描画
// 8ビット色: RRRGGGBB 
void tTFTScreen::colorDrawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t ex, uint8_t f) {
  int16_t i, j;
  uint16_t color8,color16,r,g,b;
  for( j = 0; j < h; j++) {
    for(i = 0; i < w; i++ ) { 
      color8   = *(bitmap+w*j+i);
      r =  color8 >>5;
      g = (color8>>2)&7;
      b =  color8 & 3;
      color16 = this->tft->color565(r<<5,g<<5,b<<6); // RRRRRGGGGGGBBBBB
      if ( color16 || (!color16 && f) ) {
        // ドットあり
        if (ex == 1) {
          // 1倍
           this->tft->drawPixel(x+i, y+j, color16);
        } else {
            this->tft->fillRect(x + i * ex, y + j * ex, ex, ex, color16);
        }
     }
   }
 }
}


// ドット描画
void tTFTScreen::pset(int16_t x, int16_t y, uint16_t c) {
  if (c<=8)
    c = tbl_color[c];
    
  this->tft->drawPixel(x, y, c);
}

// 線の描画
void tTFTScreen::line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t c){
  if (c<=8)
    c = tbl_color[c];
  this->tft->drawLine(x1, y1, x2, y2, c);
}

// 円の描画
void tTFTScreen::circle(int16_t x, int16_t y, int16_t r, uint16_t c, int8_t f) {
 if (c<=8)
    c = tbl_color[c];
 if(f)
   this->tft->fillCircle(x, y, r, c);
 else
   this->tft->drawCircle(x, y, r, c);
}

// 四角の描画
void tTFTScreen::rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, int8_t f) {
  if (c<=8)
    c = tbl_color[c];
 if(f)
   this->tft->fillRect(x, y, w, h, c);
 else
   this->tft->drawRect(x, y, w, h, c);
}

// ビットマップの描画
void  tTFTScreen::bitmap(int16_t x, int16_t y, uint8_t* adr, uint16_t index, uint16_t w, uint16_t h, uint16_t d, uint16_t rgb,uint8_t mode) {
  uint8_t*bmp;
  if (mode == 0) {
    // モノラルビットマップ画像の描画
    if (rgb <= 8)
      rgb = tbl_color[rgb];
    bmp = adr + ((w + 7) / 8) * h * index;
    this->drawBitmap_x2(x, y, (const uint8_t*)bmp, w, h, rgb, d, 1);
  } else {
    // 16ビット色ビットマップ画像の描画
    bmp = adr + w * h * index;
    this->colorDrawBitmap(x, y, (const uint8_t*)bmp, w, h, d, 1);
  }  
}


// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.
// 本関数はAdafruit_ILI9341_STMライブラリのサンプルspitftbitmapを利用しています
//
static uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

static uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

//
// ビットマックロード
// 本関数はAdafruit_ILI9341_STMライブラリのサンプルspitftbitmapを利用しています
//

uint8_t tTFTScreen::bmpDraw(char *filename, uint8_t x, uint16_t y, uint16_t bx, uint16_t by, uint16_t bw, uint16_t bh, uint8_t mode) {
  File     bmpFile;               // ファイルディスクリプタ
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0;
  uint8_t rc = 0;
  uint16_t bc,fc;
  
  // 2色BITMAP時の色の設定
  if (mode) {
    bc = fgcolor;
    fc = bgcolor;
  } else {
    bc = bgcolor;
    fc = fgcolor;
  }
    
  //uint8_t pixelSize;               // ピクセルサイズ係数(1:1ビット、3:24ビット)
  
  if((x >= this->tft->width()) || (y >= this->tft->height())) 
    return 10;               // 表示位置がＴＦＴ領域を超えている

  // ファイルオープン
  if (SD_BEGIN() == false) 
    return SD_ERR_INIT;      // SDカードが初期化失敗
  if ((bmpFile = SD.open(filename)) == NULL) {
    rc = SD_ERR_OPEN_FILE;   // ファイルオープン失敗
  	goto ERROR;              // SDカード利用終了処理へジャンプ
  }
  
  // BMPヘッダーのチェック
  if(read16(bmpFile) == 0x4D42) {     // BMP signature
  	read32(bmpFile);
  	(void)read32(bmpFile);            // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data

  	// DIBヘッダーの読み込み
  	read32(bmpFile);
    bmpWidth  = read32(bmpFile);      // ビットマップ画像横ドット数
    bmpHeight = read32(bmpFile);      // ビットマック画像縦ドット数
    if(read16(bmpFile) == 1) {        // プレーン数チェック  必ず1でなければならない
      bmpDepth = read16(bmpFile);     // 1ピクセル当たりのビット数の取得
      if((bmpDepth == 24 || bmpDepth == 1) && (read32(bmpFile) == 0)) { // 0 = uncompressed
        goodBmp = true;               // サポート条件 true
        // １ラインのバイト数の計算
        if (bmpDepth == 24) {
          rowSize = (bmpWidth * 3 + 3) & ~3;       // 24ビットの場合
          //pixelSize = 3;
        } else {
          rowSize = (((bmpWidth+7)>>3) + 3) & ~3;  // 1ビットの場合
          //pixelSize = 1;
        } 
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if (bx+bw >= w) bw=w-bx;
        if (by+bh >= h) bh=h-by;
        w=bx+bw;
        h=by+bh;
        
        if((x+w-1) >= this->tft->width())  w = this->tft->width()  - x;
        if((y+h-1) >= this->tft->height()) h = this->tft->height() - y;
       
        // Set TFT address window to clipped image bounds
        this->tft->setAddrWindow(x, y, x+bw-1, y+bh-1);
       
        for (row = by; row < h; row++) { 
          // ビットマップ画像のライン毎のデータ処理
          
          // 格納画像の向きの補正
          if(flip) {
            if (bmpDepth == 24)
              pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize + bx*3;
            else
              pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize + (bx>>3);
          } else {
            if (bmpDepth == 24)
              pos = bmpImageoffset + row * rowSize + bx*3;
            else
              pos = bmpImageoffset + row * rowSize + (bx>>3);            
          }
          if(bmpFile.position() != pos) {
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); 
          }

          // 1ライン分のデータの処理
          for ( col = bx; col < w; col++ ) {
            // ドット毎の処理
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            if (bmpDepth == 24) {
              b = sdbuffer[buffidx++];
              g = sdbuffer[buffidx++];
              r = sdbuffer[buffidx++];
              this->tft->pushColor(this->tft->color565(r,g,b));
            } else {
              uint8_t px = sdbuffer[buffidx++];
              //Serial.print(px,HEX);Serial.write(',');
              for ( uint8_t i = 0; i < 8; i++) {
                  if (px & (0x80>>i)) {
                      this->tft->pushColor(fc);
                  }  else {
                      this->tft->pushColor(bc);
                  }
              }
              col+=7;
            }
          } // end pixel
          //Serial.println();
        } // end scanline
      } // end goodBmp
    }
  }

  bmpFile.close();
ERROR:
  SD.end();
	
  if(!goodBmp) {
    rc =  10;
  }
 return rc;
}

// 指定座標の色コード取得
uint16_t  tTFTScreen::gpeek(int16_t x, int16_t y) {
#ifndef STM32_R20170323
  return this->tft->readPixel(x,y);
#else
  return 0;
#endif  
}

// 指定範囲の指定色コード有無のチェック
int16_t  tTFTScreen::ginp(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
#ifndef STM32_R20170323
  for (int16_t i = y ; i < y+h; i++) {
    for (int16_t j= x; j < x+w; j++) {
      if (this->gpeek(x,y) == c) {
          return 1;
      }
    }
  }
#endif
  return 0;	
}  
