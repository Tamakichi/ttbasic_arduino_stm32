/*
 TOYOSHIKI Tiny BASIC for Arduino
 (C)2012 Tetsuya Suzuki
 GNU General Public License
   2017/03/22, Modified by Tamakichi、for Arduino STM32
 */

// 2017/07/25 豊四季Tiny BASIC for Arduino STM32 V0.84 NTSC・ターミナル統合バージョン
// 2017/07/26 変数名を英字1文字+数字0～9の2桁対応:例 X0,X1 ... X9
// 2017/07/31 putnum()の値が-32768の場合の不具合対応(オリジナル版の不具合)
// 2017/07/31 toktoi()の定数の変換仕様の変更(-32768はオーバーフローエラーとしない)
// 2017/07/31 SDカードからテキスト形式プログラムロード時の中間コード変換不具合の対応(loadPrgText)
// 2017/08/03 比較演算子 "<>"の追加("!="と同じ)
// 2017/08/04 RENUMで振り直しする行範囲を指定可能機能追加、行番号0,1は除外に修正
// 2017/08/04 LOADコマンドでSDカードからの読み込みで追記機能を追加
// 2017/08/12 RENUMに重大な不具合あり、V0.83版の機能に一時差し換え
// 2017/08/13 TFT(ILI9341)モジュールの暫定対応
// 2017/08/19 SAVE,ERASE,LOAD,LRUN,CONFIGコマンドのプログラム番号範囲チェックミス不具合対応
// 2017/08/23 NTSC利用しない場合のピン機能利用テーブルの定義の対応
// 2017/08/24 tone(),フォント汎用利用のための修正(tTVscreen依存しないための修正)
// 2017/08/28 CLVが2文字変数の初期化しない不具合の修正
// 2017/08/28 PWM出力の16Hz未満の出力対応
// 2017/10/09 定数ON,OFFの追加
// 2017/10/14 TFT版のDWBMPの引数チェック不具合対応
// 2017/10/15 コンパイルワーニング対応（未使用変数の削除、キャスト、定数重定義）
// 2017/10/17 仮想EEPROMとプログラム保存領域の競合不具合対応
// 2017/10/23 OLEDディスプレイで表示が反映されない不具合対応(=>iprint()の修正）
// 2017/10/23 FILESでフラッシュメモリリスト表示の範囲指定引数追加
// 2017/10/23 OLEDのコンソールモード時表示不具合の対応
// 2017/10/25 sound.hの定義により、tone関連のプロトタイプ宣言を削除
// 2017/10/27 CONSLEコマンドの不具合修正
// 2017/10/27 LIST表示時にIF文中で')'の後の変数名が来る場合に空白文字が入るように修正
// 2017/10/27 NTSC版でデフォルト画面サイズが指定出来るように修正
// 2017/11/03 起動直後JTRST(PB4)がHIGHになっている不具合対応 
// 2017/11/04 RTCのクロックソースをttconfig.hで設定可能に修正
// 2017/11/07 フラッシュメモリ操作部を統合し、クラスモジュール化（tFlashManクラス)
// 2017/11/08 デバイス画面部コードの共通化,エラーメッセージ定義をTTBAS_LIB/ttbasic_error.h,cppに外だし
// 2017/11/08 RTClock仕様変更対応
// 2017/11/19 OLED,TFT版でコンソール画面利用時にOLED,TFTにグラフィック表示可能に修正
// 2017/11/10 PULSEIN()関数、RGB()関数の追加
// 2018/01/08 CLS,REM,"'"を直接実行した場合、OKを表示しないように修正
// 2018/01/09 INPUTの不具合対応（２桁変数対応、ENTERのみの入力禁止）
// 2018/01/09 2進数定数対応
// 2018/08/13 Arduino_STM32安定版判定はSTM32_R20160323の定義の有無のみで判定するように修正
// 2018/08/15 OLED,TFT利用時、CONFIGコマンドでキーボード設定が出来ない不具合の修正
// 2018/08/19 CONFIG 0,n,n,n でNTSC信号の縦横位置補正が出来るように修正　
// 2018/08/21 OLED版はプログラム保存本数を７に変更（スケッチサイズ肥大のため）
//

#include <Arduino.h>
#include <unistd.h>
#include <stdlib.h>
#include <wirish.h>
#include "ttconfig.h"     // コンパイル定義
#include "tscreenBase.h"  // コンソール基本
#include "tTermscreen.h"  // シリアルコンソール
#include "sound.h"        // サウンド再生(Timer4 PWM端子 PB9を利用）

#define STR_EDITION "Arduino STM32"
#ifdef STM32_R20170323
 #define STR_VARSION "Edition V0.85a"
#else
 #define STR_VARSION "Edition V0.85n"
#endif

// TOYOSHIKI TinyBASIC プログラム利用域に関する定義
#define SIZE_LINE 128    // コマンドライン入力バッファサイズ + NULL
#define SIZE_IBUF 128    // 中間コード変換バッファサイズ
#define SIZE_LIST 4096   // プログラム領域サイズ(4kバイト)
#define SIZE_VAR  210    // 利用可能変数サイズ(A-Z,A0:A6-Z0:Z6の26+26*7=208)
#define SIZE_ARRY 100    // 配列変数サイズ(@(0)～@(99)
#define SIZE_GSTK 20     // GOSUB stack size(2/nest) :10ネストまでOK
#define SIZE_LSTK 50     // FOR stack size(5/nest) :  10ネストまでOK
#define SIZE_MEM  1024   // 自由利用データ領域

// SRAMの物理サイズ(バイト)
#define SRAM_SIZE      20480 // STM32F103C8T6

// 入出力キャラクターデバイス
#define CDEV_SCREEN   0  // メインスクリーン
#define CDEV_SERIAL   1  // シリアル
#define CDEV_GSCREEN  2  // グラフィック
#define CDEV_MEMORY   3  // メモリー
#define CDEV_SDFILES  4  // ファイル

// *** フォント参照 ***************
const uint8_t* ttbasic_font = DEVICE_FONT;
uint16_t f_width  = *(ttbasic_font+0);
uint16_t f_height = *(ttbasic_font+1);
inline uint8_t* getFontAdr() { return (uint8_t*)ttbasic_font;};

// **** スクリーン管理 *************
#define CON_MODE_DEVICE    0        // コンソールモード デバイス 
#define CON_MODE_SERIAL    1        // コンソールモード シリアル
#define SCSIZE_MODE_SERIAL 0        // スクリーンサイズモード指定なし（シリアルコンソールモード）
uint8_t* workarea = NULL;           // 画面用動的獲得メモリ
uint8_t  scmode = USE_SCREEN_MODE;  // コンソール画面(0:シリアル画面、1:デバイス画面)
uint8_t  prv_scmode = scmode;       // 直前のコンソール画面
uint8_t  serialMode = DEF_SMODE;    // シリアルモード(0:USB、1:USART)
uint8_t  scSizeMode = 1;            // スクリーンサイズモード(0:シリアルターミナル,1:ノーマル,2～ 拡大表示)
uint8_t  prv_scSizeMode = 1;        // 直前のスクリーンサイズモード(0:シリアルターミナル,1:ノーマル,2～ 拡大表示)
uint8_t  scrt = 0;                  // 画面向き
uint8_t  prv_scrt = 0;              // 直前の画面向き
uint32_t defbaud = GPIO_S1_BAUD;    // シリアルボーレート

void initScreenEnv();
tscreenBase* sc;   // 利用デバイススクリーン用ポインタ
tTermscreen sc1;   // ターミナルスクリーン

#if USE_NTSC == 1
  #include "tTVscreen.h"
  tTVscreen   sc2;
#elif USE_TFT == 1
  #include "tTFTScreen.h"
  tTFTScreen  sc2;
#elif USE_OLED == 1
  #include "tOLEDScreen.h"
  tOLEDScreen sc2;
#endif

#define KEY_ENTER 13

// **** I2Cライブラリの利用設定 ****
#if defined(STM32_R20170323)
  // Wireライブラリ変更前の場合
  #if I2C_USE_HWIRE == 0
    #include <Wire.h>
    #define I2C_WIRE  Wire
  #else
    #include <HardWire.h>
    HardWire HWire(1, I2C_FAST_MODE); // I2C1を利用
    #define I2C_WIRE  HWire
  #endif
#else 
  // Wireライブラリ変更ありの場合
  #if I2C_USE_HWIRE == 0
    #include <SoftWire.h>
    TwoWire SWire(SCL, SDA, SOFT_STANDARD);
    #define I2C_WIRE  SWire
  #else
    #include <Wire.h>
    #define I2C_WIRE  Wire
  #endif
#endif

// *** SDカード管理 *****************
#include "sdfiles.h"
#if USE_SD_CARD == 1
sdfiles fs;
#endif 

// *** フラッシュメモリ管理 ***********
#include <tFlashMan.h>
#define FLASH_PAGE_NUM         128     // 全ページ数
#define FLASH_PAGE_SIZE        1024    // ページ内バイト数
#define FLASH_PAGE_PAR_PRG     4       // 1プログラム当たりの利用ページ数
#if USE_OLED == 1
  #define FLASH_SAVE_NUM       7       // プログラム保存可能数
#else
  #define FLASH_SAVE_NUM       8       // プログラム保存可能数
#endif

// フラッシュメモリ管理オブジェクト(プログラム保存、システム環境設定を管理）
tFlashMan FlashMan(FLASH_PAGE_NUM,FLASH_PAGE_SIZE, FLASH_SAVE_NUM, FLASH_PAGE_PAR_PRG); 

// システム環境設定値
SystemConfig CONFIG;
  
// プロトタイプ宣言
char* getParamFname();
int16_t getNextLineNo(int16_t lineno);
void mem_putch(uint8_t c);

unsigned char* iexe();
short iexp(void);
void error(uint8_t flgCmd);

// **** RTC用宣言 ********************
#if USE_INNERRTC == 1
  #include <RTClock.h>
  #include <time.h>
  RTClock rtc(RTC_CLOCK_SRC);
#endif

// **** PWM用設定 ********************
#define TIMER_DIV (F_CPU/1000000L)

// **** 仮想メモリ定義 ****************
#define V_VRAM_TOP  0x0000
#define V_VAR_TOP   0x1900 // V0.84で変更
#define V_ARRAY_TOP 0x1AA0 // V0.84で変更
#define V_PRG_TOP   0x1BA0 // V0.84で変更
#define V_MEM_TOP   0x2BA0 // V0.84で変更
#define V_FNT_TOP   0x2FA0 // V0.84で変更
#define V_GRAM_TOP  0x37A0 // V0.84で変更

// 定数
#define CONST_HIGH   1
#define CONST_LOW    0
#define CONST_ON     1
#define CONST_OFF    0
#define CONST_LSB    LSBFIRST
#define CONST_MSB    MSBFIRST
#define SRAM_TOP     0x20000000

// **** GPIOピンに関する定義 **********

// GPIOピンモードの設定
const WiringPinMode pinType[] = {
  OUTPUT_OPEN_DRAIN, OUTPUT, INPUT_PULLUP, INPUT_PULLDOWN, INPUT_ANALOG, INPUT, PWM,
};

#define FNC_IN_OUT  1  // デジタルIN/OUT
#define FNC_PWM     2  // PWM
#define FNC_ANALOG  4  // アナログIN

#define IsPWM_PIN(N) IsUseablePin(N,FNC_PWM)      // 指定ピンPWM利用可能判定
#define IsADC_PIN(N) IsUseablePin(N,FNC_ANALOG)   // 指定ピンADC利用可能判定
#define IsIO_PIN(N)  IsUseablePin(N,FNC_IN_OUT)   // 指定ピンデジタル入出力利用可能判定

// ピン機能チェックテーブル
#if USE_TFT == 1 || (USE_OLED == 1 && OLED_IFMODE == 1) // TFT/OLED(SPI) 利用専用環境
const uint8_t pinFunc[]  = {
  5,5,5,5,5,5,7,7,3,3,  //  0 -  9: PA0,PA1,PA2,PA3,PA4,PA5,PA6,PA7,PA8,PA9,
  3,0,0,1,1,1,7,7,1,1,  // 10 - 19: PA10,PA11,PA12,PA13,PA14,PA15,PB0,PB1,PB2,PB3, 
  0,0,0,0,1,0,1,0,0,0,  // 20 - 29: PB4,PB5,PB6,PB7,PB8,PB9,PB10,PB11,PB12,PB13,
  0,0,1,0,0,            // 30 - 34: PB14,PB15,PC13,PC14,PC15,
};
#elif USE_NTSC == 1  // NTSC利用環境
const uint8_t pinFunc[]  = {
  5,0,5,5,5,5,7,7,3,3,  //  0 -  9: PA0,PA1,PA2,PA3,PA4,PA5,PA6,PA7,PA8,PA9,
  3,0,0,1,1,1,7,7,1,1,  // 10 - 19: PA10,PA11,PA12,PA13,PA14,PA15,PB0,PB1,PB2,PB3, 
  0,0,0,0,1,0,1,1,1,1,  // 20 - 29: PB4,PB5,PB6,PB7,PB8,PB9,PB10,PB11,PB12,PB13,
  1,0,1,0,0,            // 30 - 34: PB14,PB15,PC13,PC14,PC15,
};
#else // ターミナルコンソールのみ利用環境 または OLED(I2C)
const uint8_t pinFunc[] = {
 5,5,5,5,5,5,7,7,3,3,   //  0 -  9: PA0,PA1,PA2,PA3,PA4,PA5,PA6,PA7,PA8,PA9, (変更) PA1...5
 3,0,0,1,1,1,7,7,1,1,   // 10 - 19: PA10,PA11,PA12,PA13,PA14,PA15,PB0,PB1,PB2,PB3, 
 1,1,0,0,1,1,1,1,1,1,   // 20 - 29: PB4,PB5,PB6,PB7,PB8,PB9,PB10,PB11,PB12,PB13, (変更) PB4,5,9...1
 1,1,1,0,0,             // 30 - 34: PB14,PB15,PC13,PC14,PC15,　(変更) PB15...1
};
#endif

// ピン利用可能チェック
inline uint8_t IsUseablePin(uint8_t pinno, uint8_t fnc) {
  return pinFunc[pinno] & fnc;
}

// Terminal control(文字の表示・入力は下記の3関数のみ利用)
#define c_getch( ) sc->get_ch()
#define c_kbhit( ) sc->isKeyIn()

// 指定デバイスへの文字の出力
//  c     : 出力文字
//  devno : デバイス番号 0:メインスクリーン 1:シリアル 2:グラフィック 3:、メモリー 4:ファイル
inline void c_putch(uint8_t c, uint8_t devno = CDEV_SCREEN) {
  if (devno == CDEV_SCREEN )
    sc->putch(c); // メインスクリーンへの文字出力
  else if (devno == CDEV_SERIAL)
   sc->Serial_write(c); // シリアルへの文字列出力
#if USE_NTSC == 1 || USE_TFT ==1 || USE_OLED == 1
  else if (devno == CDEV_GSCREEN )
    sc2.gputch(c); // グラフィック画面へのグラフィック文字出力
#endif
  else if (devno == CDEV_MEMORY)
   mem_putch(c); // メモリーへの文字列出力
#if USE_SD_CARD == 1
  else if (devno == CDEV_SDFILES )
    fs.putch(c); // ファイルへの文字列出力
#endif
} 

//  改行
//  devno : デバイス番号 0:メインスクリーン 1:シリアル 2:グラフィック 3:、メモリー 4:ファイル
inline void newline(uint8_t devno=CDEV_SCREEN) {
 if (devno== CDEV_SCREEN )
   sc->newLine();        // メインスクリーンへの文字出力
  else if (devno == CDEV_SERIAL )
   sc->Serial_newLine(); // シリアルへの文字列出力
#if USE_NTSC == 1
  else if (devno == CDEV_GSCREEN )
    ((tTVscreen*)sc)->gputch('\n'); // グラフィック画面へのグラフィック文字出力
#endif
  else if (devno == CDEV_MEMORY )
    mem_putch('\n'); // メモリーへの文字列出力
#if USE_SD_CARD == 1
  else if (devno == CDEV_SDFILES ) {
    fs.putch('\x0d'); // ファイルへの文字列出力
    fs.putch('\x0a'); 
  }
#endif
}

// tick用支援関数
void iclt() {
  systick_uptime_millis = 0;
}

// 乱数
short getrnd(short value) {
  return random(value);
}

// キーワードテーブル
const char *kwtbl[] __FLASH__  = {
 "GOTO", "GOSUB", "RETURN", "FOR", "TO", "STEP", "NEXT", "IF", "END", "ELSE",       // 制御命令(10)
 ",", ";", ":", "\'","-", "+", "*", "/", "%", "(", ")", "$", "`","<<", ">>", "|", "&",  // 演算子・記号(31)
 ">=", "#", ">", "=", "<=", "!=", "<>","<", "AND", "OR", "!", "~", "^", "@",     
 "CLT", "WAIT",  // 時間待ち・時間計測コマンド(2) 
 "POKE",         // 記憶領域操作コマンド(1)
 "PRINT", "?", "INPUT", "CLS", "COLOR", "ATTR" ,"LOCATE", "REDRAW", "CSCROLL", // キャラクタ表示コマンド(9) 
 "CHR$", "BIN$", "HEX$", "DMP$", "STR$",               // 文字列関数(5)
 "ABS", "MAP", "ASC", "FREE", "RND",  "INKEY", "LEN",  // 数値関数(20)
 "TICK", "PEEK", "VPEEK", "GPEEK", "GINP", "RGB",
 "I2CW", "I2CR", "IN", "ANA", "SHIFTIN",
 "SREADY", "SREAD", "EEPREAD",
 "PSET","LINE","RECT","CIRCLE", "BITMAP", "GPRINT", "GSCROLL",  // グラフィック表示コマンド(7)
 "GPIO", "OUT", "POUT", "SHIFTOUT", "PULSEIN",                  // GPIO・入出力関連コマンド(5)
 "SMODE", "SOPEN", "SCLOSE", "SPRINT", "SWRITE",                // シリアル通信関連コマンド(5)
 "LDBMP","MKDIR","RMDIR",/*"RENAME",*/ "FCOPY","CAT", "DWBMP", "REMOVE", // SDカード関連コマンド
 "HIGH", "LOW", "ON", "OFF",  // 定数
 "PA0", "PA1", "PA2", "PA3", "PA4", "PA5", "PA6", "PA7", "PA8", "PA9",
 "PA10","PA11", "PA12", "PA13","PA14","PA15",
 "PB0", "PB1", "PB2", "PB3", "PB4", "PB5", "PB6", "PB7", "PB8", "PB9",
 "PB10","PB11", "PB12", "PB13","PB14","PB15", "PC13", "PC14","PC15", 
 "CW", "CH","GW","GH", "LSB", "MSB",
 "MEM", "VRAM", "VAR", "ARRAY","PRG","FNT","GRAM",
 "UP", "DOWN", "RIGHT", "LEFT",
 "OUTPUT_OD", "OUTPUT", "INPUT_PU", "INPUT_PD", "ANALOG", "INPUT_FL", "PWM",
 "TONE", "NOTONE",                         // サウンドコマンド(2)
 "DATE", "GETDATE", "GETTIME", "SETDATE",  // RTC関連コマンド(4)
 "EEPFORMAT", "EEPWRITE",                  // 仮想EEPROM関連コマンド(2)
 "LOAD", "SAVE", "BLOAD", "BSAVE", "LIST", "NEW", "REM", "LET", "CLV",  // プログラム関連 コマンド(16)
 "LRUN", "FILES","EXPORT", "CONFIG", "SAVECONFIG", "ERASE", "SYSINFO",
 "SCREEN", "WIDTH", "CONSOLE", // 表示切替
 "RENUM", "RUN", "DELETE", "OK",           // システムコマンド(4)
};

// Keyword count
#define SIZE_KWTBL (sizeof(kwtbl) / sizeof(const char*))

// i-code(Intermediate code) assignment
enum ICode:uint8_t { 
 I_GOTO, I_GOSUB, I_RETURN, I_FOR, I_TO, I_STEP, I_NEXT, I_IF, I_END, I_ELSE,   // 制御命令(10)
 I_COMMA, I_SEMI, I_COLON, I_SQUOT, I_MINUS, I_PLUS, I_MUL, I_DIV, I_DIVR, I_OPEN, I_CLOSE, I_DOLLAR, I_APOST,  // 演算子・記号(31)
 I_LSHIFT, I_RSHIFT, I_OR, I_AND, I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_NEQ, I_NEQ2, I_LT, I_LAND, I_LOR, I_LNOT,
 I_BITREV, I_XOR,  I_ARRAY, 
 I_CLT, I_WAIT,  // 時間待ち・時間計測コマンド(2)
 I_POKE,         // 記憶領域操作コマンド(1)
 I_PRINT, I_QUEST, I_INPUT, I_CLS, I_COLOR, I_ATTR, I_LOCATE,  I_REFLESH, I_CSCROLL,  // キャラクタ表示コマンド(9)  
 I_CHR, I_BIN, I_HEX, I_DMP, I_STRREF,   // 文字列関数(5)
 I_ABS, I_MAP, I_ASC, I_FREE, I_RND, I_INKEY, I_LEN,   // 数値関数(20)
 I_TICK, I_PEEK, I_VPEEK, I_GPEEK, I_GINP, I_RGB,
 I_I2CW, I_I2CR, I_DIN, I_ANA, I_SHIFTIN,
 I_SREADY, I_SREAD, I_EEPREAD,
 I_PSET, I_LINE, I_RECT, I_CIRCLE, I_BITMAP, I_GPRINT, I_GSCROLL, // グラフィック表示コマンド(7)
 I_GPIO, I_DOUT, I_POUT, I_SHIFTOUT, I_PULSEIN,                   // GPIO・入出力関連コマンド(5)
 I_SMODE, I_SOPEN, I_SCLOSE, I_SPRINT, I_SWRITE,                  // シリアル通信関連コマンド(5)
 I_LDBMP, I_MKDIR, I_RMDIR, /*I_RENAME,*/ I_FCOPY, I_CAT, I_DWBMP, I_REMOVE,  // SDカード関連コマンド
 I_HIGH, I_LOW, I_ON, I_OFF,// 定数
 I_PA0, I_PA1, I_PA2, I_PA3, I_PA4, I_PA5, I_PA6, I_PA7, I_PA8, I_PA9,
 I_PA10, I_PA11, I_PA12, I_PA13,I_PA14,I_PA15,
 I_PB0, I_PB1, I_PB2, I_PB3, I_PB4, I_PB5, I_PB6, I_PB7, I_PB8, I_PB9, 
 I_PB10,  I_PB11, I_PB12, I_PB13,I_PB14,I_PB15, I_PC13, I_PC14,I_PC15,
 I_CW, I_CH, I_GW, I_GH,
 I_LSB, I_MSB, 
 I_MEM, I_VRAM, I_MVAR, I_MARRAY,I_MPRG,I_MFNT,I_GRAM,
 I_UP, I_DOWN, I_RIGHT, I_LEFT,
 I_OUTPUT_OPEN_DRAIN, I_OUTPUT, I_INPUT_PULLUP, I_INPUT_PULLDOWN, I_INPUT_ANALOG, I_INPUT_F,  I_PWM,  
 I_TONE, I_NOTONE,                          // サウンドコマンド(2)
 I_DATE, I_GETDATE, I_GETTIME, I_SETDATE,   // RTC関連コマンド(4)
 I_EEPFORMAT, I_EEPWRITE,                   // 仮想EEPROM関連コマンド(2)
 I_LOAD, I_SAVE, I_BLOAD, I_BSAVE, I_LIST, I_NEW, I_REM, I_LET, I_CLV,  // プログラム関連 コマンド(16)
 I_LRUN, I_FILES, I_EXPORT, I_CONFIG, I_SAVECONFIG, I_ERASE, I_INFO,
 I_SCREEN, I_WIDTH, I_CONSOLE, // 表示切替
 I_RENUM, I_RUN, I_DELETE, I_OK,  // システムコマンド(4)

// 内部利用コード
  I_NUM, I_STR, I_HEXNUM, I_BINNUM, I_VAR,
  I_EOL, 
};

// List formatting condition
// 後ろに空白を入れない中間コード
const uint8_t i_nsa[] = {
  I_RETURN, I_END, 
  I_CLT,
  I_HIGH, I_LOW,  I_ON, I_OFF,I_CW, I_CH, I_GW, I_GH, 
  I_UP, I_DOWN, I_RIGHT, I_LEFT,
  I_INKEY,I_VPEEK, I_CHR, I_ASC, I_HEX, I_BIN,I_LEN, I_STRREF,
  I_COMMA, I_SEMI, I_COLON, I_SQUOT,I_QUEST,
  I_MINUS, I_PLUS, I_MUL, I_DIV, I_DIVR, I_OPEN, I_CLOSE, I_DOLLAR, I_APOST,I_LSHIFT, I_RSHIFT, I_OR, I_AND,
  I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_NEQ, I_NEQ2, I_LT, I_LNOT, I_BITREV, I_XOR,
  I_ARRAY, I_RND, I_ABS, I_FREE, I_TICK, I_PEEK, I_I2CW, I_I2CR,
  I_OUTPUT_OPEN_DRAIN, I_OUTPUT, I_INPUT_PULLUP, I_INPUT_PULLDOWN, I_INPUT_ANALOG, I_INPUT_F, I_PWM,
  I_DIN, I_ANA, I_SHIFTIN, I_PULSEIN, I_MAP, I_DMP,
  I_PA0, I_PA1, I_PA2, I_PA3, I_PA4, I_PA5, I_PA6, I_PA7, I_PA8, 
  I_PA9, I_PA10, I_PA11, I_PA12, I_PA13,I_PA14,I_PA15,
  I_PB0, I_PB1, I_PB2, I_PB3, I_PB4, I_PB5, I_PB6, I_PB7, I_PB8, 
  I_PB9, I_PB10, I_PB11, I_PB12, I_PB13,I_PB14,I_PB15,
  I_PC13, I_PC14,I_PC15,
  I_LSB, I_MSB, I_MEM, I_VRAM, I_MVAR, I_MARRAY, I_EEPREAD, I_MPRG, I_MFNT,I_GRAM,
  I_SREAD, I_SREADY, I_GPEEK, I_GINP,I_RGB,
};

// 前が定数か変数のとき前の空白をなくす中間コード
const uint8_t  i_nsb[] = {
  I_MINUS, I_PLUS, I_MUL, I_DIV, I_DIVR, I_OPEN, I_CLOSE, I_LSHIFT, I_RSHIFT, I_OR, I_AND,
  I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_NEQ, I_NEQ2,I_LT, I_LNOT, I_BITREV, I_XOR,
  I_COMMA, I_SEMI, I_COLON, I_SQUOT, I_EOL
};

// 必ず前に空白を入れる中間コード
const uint8_t i_sf[]  = {
  I_ATTR, I_CLS, I_COLOR, I_DATE, I_END, I_FILES, I_TO, I_STEP,I_QUEST,I_LAND, I_LOR,
  I_GETDATE,I_GETTIME,I_GOSUB,I_GOTO,I_GPIO,I_INKEY,I_INPUT,I_LET,I_LIST,I_ELSE,
  I_LOAD,I_LOCATE,I_NEW,I_DOUT,I_POKE,I_PRINT,I_REFLESH,I_REM,I_RENUM,I_CLT,
  I_RETURN,I_RUN,I_SAVE,I_SETDATE,I_SHIFTOUT,I_WAIT,I_EEPFORMAT, I_EEPWRITE, 
  I_PSET, I_LINE, I_RECT, I_CIRCLE, I_BITMAP, I_SWRITE, I_SPRINT,  I_SOPEN, I_SCLOSE,I_SMODE,
  I_TONE, I_NOTONE, I_CSCROLL, I_GSCROLL,I_EXPORT,
};

// 例外検索関数
inline char sstyle(uint8_t code,
  const uint8_t *table, uint8_t count) {
  while(count--) //中間コードの数だけ繰り返す
    if (code == table[count]) //もし該当の中間コードがあったら
      return 1; //1を持ち帰る
  return 0; //（なければ）0を持ち帰る
}

// キーワード並び例外チェックマクロ
#define nospacea(c) sstyle(c, i_nsa, sizeof(i_nsa))  // 後ろに空白を入れない中間コードか？
#define nospaceb(c) sstyle(c, i_nsb, sizeof(i_nsb))  // 前が定数か変数のとき前の空白をなくす中間コードか？
#define spacef(c) sstyle(c, i_sf, sizeof(i_sf))      // 必ず前に空白を入れる中間コードか？

// エラーメッセージ定義
uint8_t err;// Error message index
#include "ttbasic_error.h"
  
// RAM mapping
char lbuf[SIZE_LINE];          // コマンド入力バッファ
char tbuf[SIZE_LINE];          // テキスト表示用バッファ
int16_t tbuf_pos = 0;
unsigned char ibuf[SIZE_IBUF];    // i-code conversion buffer
short var[SIZE_VAR];              // 変数領域
short arr[SIZE_ARRY];             // 配列領域
unsigned char listbuf[SIZE_LIST]; // プログラムリスト領域
uint8_t mem[SIZE_MEM];            // 自由利用データ領域

unsigned char* clp;               // Pointer current line
unsigned char* cip;               // Pointer current Intermediate code
unsigned char* gstk[SIZE_GSTK];   // GOSUB stack
unsigned char gstki;              // GOSUB stack index
unsigned char* lstk[SIZE_LSTK];   // FOR stack
unsigned char lstki;              // FOR stack index

uint8_t prevPressKey = 0;         // 直前入力キーの値(INKEY()、[ESC]中断キー競合防止用)
uint8_t lfgSerial1Opened = false;  // Serial1のオープン設定フラグ

// メモリへの文字出力
inline void mem_putch(uint8_t c) {
  if (tbuf_pos < SIZE_LINE) {
   tbuf[tbuf_pos] = c;
   tbuf_pos++;
  }
}

// メモリ書き込みポインタのクリア
inline void cleartbuf() {
  tbuf_pos=0;
  memset(tbuf,0,SIZE_LINE);
}

// 仮想アドレスを実アドレスに変換
//  引数   :  vadr 仮想アドレス
//  戻り値 :  NULL以外 実アドレス、NULL 範囲外
//
uint8_t* v2realAddr(uint16_t vadr) {
  uint8_t* radr = NULL; 
  if (vadr < sc->getScreenByteSize()) {   // VRAM領域
    radr = vadr+sc->getScreen();
  } else if ((vadr >= V_VAR_TOP) && (vadr < V_ARRAY_TOP)) { // 変数領域
    radr = vadr-V_VAR_TOP+(uint8_t*)var;
  } else if ((vadr >= V_ARRAY_TOP) && (vadr < V_PRG_TOP)) { // 配列領域
    radr = vadr - V_ARRAY_TOP+(uint8_t*)arr;
  } else if ((vadr >= V_PRG_TOP) && (vadr < V_MEM_TOP)) {   // プログラム領域
    radr = vadr - V_PRG_TOP + (uint8_t*)listbuf;
  } else if ((vadr >= V_MEM_TOP) && (vadr < V_FNT_TOP)) {   // ユーザーワーク領域
    radr = vadr - V_MEM_TOP + mem;    
  } else if ((vadr >= V_FNT_TOP) && (vadr < V_GRAM_TOP)) {  // フォント領域
    radr = vadr - V_FNT_TOP + getFontAdr()+3;
  } else if ((vadr >= V_GRAM_TOP) && (vadr < V_GRAM_TOP+6048)) { // グラフィク表示用メモリ領域
//    if ( (scmode >= 1) && (scmode <= 3) )
      if ( scmode ) // 2017/10/27
#if USE_NTSC == 1 || USE_OLED == 1
      radr = vadr - V_GRAM_TOP + ((tGraphicScreen*)sc)->getGRAM();
#else
      radr = NULL;
#endif
    else
      radr = NULL;
  }
  return radr;
}

// Standard C libraly (about) same functions
inline char c_toupper(char c) {
  return(c <= 'z' && c >= 'a' ? c - 32 : c);
}
inline char c_isprint(char c) {
  //return(c >= 32 && c <= 126);
  //return(c >= 32 && c!=127 );  // 2017/08/24 修正
  return (c);
}
inline char c_isspace(char c) {
  return(c == ' ' || (c <= 13 && c >= 9));
}

// 文字列の右側の空白文字を削除する
char* tlimR(char* str) {
  uint16_t len = strlen(str);
  for (uint16_t i = len - 1; i>0 ; i--) {
    if (str[i] == ' ') {
      str[i] = 0;
    } else {
      break;
    }
  }
  return str;
}

// コマンド引数取得(int16_t,引数チェックあり)
inline uint8_t getParam(int16_t& prm, int16_t  v_min,  int16_t  v_max, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err &&  (prm < v_min || prm > v_max)) 
    err = ERR_VALUE;
  else if (flgCmma && *cip++ != I_COMMA) {
    err = ERR_SYNTAX;
 }
  return err;
}

// コマンド引数取得(int16_t,引数チェックなし)
inline uint8_t getParam(uint16_t& prm, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err && flgCmma && *cip++ != I_COMMA) {
   err = ERR_SYNTAX;
  }
  return err;
}

// コマンド引数取得(uint16_t,引数チェックなし)
inline uint8_t getParam(int16_t& prm, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err && flgCmma && *cip++ != I_COMMA) {
   err = ERR_SYNTAX;
  }
  return err;
}

// コマンド引数取得(int32_t,引数チェックなし)
inline uint8_t getParam(int32_t& prm, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err && flgCmma && *cip++ != I_COMMA) {
   err = ERR_SYNTAX;
  }
  return err;
}

// '('チェック関数
inline uint8_t checkOpen() {
  if (*cip != I_OPEN)  err = ERR_PAREN;
  else cip++;
  return err;
}

// ')'チェック関数
inline uint8_t checkClose() {
  if (*cip != I_CLOSE)  err = ERR_PAREN;
  else cip++;
  return err;
}

// 1桁16進数文字を整数に変換する
uint16_t hex2value(char c) {
  if (c <= '9' && c >= '0')
    return c - '0';
  else if (c <= 'f' && c >= 'a')
    return c - 'a' + 10;
  else if (c <= 'F' && c >= 'A')
    return c - 'A' + 10;
  return 0;
}

// 文字列出力
inline void c_puts(const char *s, uint8_t devno=0) {
  uint8_t prev_curs = sc->IsCurs();
  if (prev_curs) sc->show_curs(0);
  while (*s) c_putch(*s++, devno); //終端でなければ出力して繰り返す
  if (prev_curs) sc->show_curs(1);
}

// Print numeric specified columns
// 引数
//  value : 出力対象数値
//  d     : 桁指定(0で指定無し)
//  devno : 出力先デバイスコード
// 機能
// 'SNNNNN' S:符号 N:数値 or 空白 
//  dで桁指定時は空白補完する
//
void putnum(int16_t value, int16_t d, uint8_t devno=0) {
  uint8_t dig;  // 桁位置
  uint8_t sign; // 負号の有無（値を絶対値に変換した印）
  uint16_t new_value;
  char c = ' ';
  if (d < 0) {
    d = -d;
    c = '0';
  }

  if (value < 0) {     // もし値が0未満なら
    sign = 1;          // 負号あり
    //value = -value;    // 値を絶対値に変換
    new_value = -value;
  } else {
    sign = 0;          // 負号なし
    new_value = value;
  }

  lbuf[6] = 0;         // 終端を置く
  dig = 6;             // 桁位置の初期値を末尾に設定
  do { //次の処理をやってみる
    lbuf[--dig] = (new_value % 10) + '0'; // 1の位を文字に変換して保存
    new_value /= 10;                      // 1桁落とす
  } while (new_value > 0);                // 値が0でなければ繰り返す

  if (sign) //もし負号ありなら
    lbuf[--dig] = '-'; // 負号を保存

  while (6 - dig < d) { // 指定の桁数を下回っていれば繰り返す
    c_putch(c,devno);   // 桁の不足を空白で埋める
    d--;                // 指定の桁数を1減らす
  }
  c_puts(&lbuf[dig],devno);   // 桁位置からバッファの文字列を表示
}

// 16進数の出力
// 引数
//  value : 出力対象数値
//  d     : 桁指定(0で指定無し)
//  devno : 出力先デバイスコード
// 機能
// 'XXXX' X:数値
//  dで桁指定時は0補完する
//  符号は考慮しない
// 
void putHexnum(short value, uint8_t d, uint8_t devno=0) {
  uint16_t  hex = (uint16_t)value; // 符号なし16進数として参照利用する
  uint16_t  h;
  uint16_t dig;

  // 表示に必要な桁数を求める
  if (hex >= 0x1000) 
    dig = 4;
  else if (hex >= 0x100) 
    dig = 3;
  else if (hex >= 0x10) 
    dig = 2;
  else 
    dig = 1;

  if (d != 0 && d > dig) 
    dig = d;

  for (uint8_t i = 0; i < 4; i++) {
    h = ( hex >> (12 - i * 4) ) & 0x0f;
    lbuf[i] = (h >= 0 && h <= 9) ? h + '0': h + 'A' - 10;
  }
  lbuf[4] = 0;
  c_puts(&lbuf[4-dig],devno);
}

// 2進数の出力
// 引数
//  value : 出力対象数値
//  d     : 桁指定(0で指定無し)
//  devno : 出力先デバイスコード
// 機能
// 'BBBBBBBBBBBBBBBB' B:数値
//  dで桁指定時は0補完する
//  符号は考慮しない
// 
void putBinnum(short value, uint8_t d, uint8_t devno=0) {
  uint16_t  bin = (uint16_t)value; // 符号なし16進数として参照利用する
  uint16_t  b;
  uint16_t  dig = 0;
  
  for (uint8_t i = 0; i < 16; i++) {
    b =(bin>>(15-i)) & 1;
    lbuf[i] = b ? '1':'0';
    if (!dig && b) 
      dig = 16-i;
  }
  lbuf[16] = 0;

  if (d > dig)
    dig = d;
  c_puts(&lbuf[16-dig],devno);
}

// 数値の入力
int16_t getnum() {
  int16_t value, tmp; //値と計算過程の値
  char c; //文字
  uint8_t len; //文字数
  uint8_t sign; //負号

  len = 0; //文字数をクリア
  while(1) {
    c = c_getch();
    if (c == KEY_ENTER && len) {
        break;
    } else if (c == SC_KEY_CTRL_C || c==27) {
      err = ERR_CTR_C;
        break;
    } else 
    //［BackSpace］キーが押された場合の処理（行頭ではないこと）
    if (((c == 8) || (c == 127)) && (len > 0)) {
      len--; //文字数を1減らす
      //c_putch(8); c_putch(' '); c_putch(8); //文字を消す
      sc->movePosPrevChar();
      sc->delete_char();
    } else
    //行頭の符号および数字が入力された場合の処理（符号込みで6桁を超えないこと）
    if ((len == 0 && (c == '+' || c == '-')) ||
      (len < 6 && isDigit(c))) {
      lbuf[len++] = c; //バッファへ入れて文字数を1増やす
      c_putch(c); //表示
    } else {
      sc->beep();
    }
  }
  newline(); //改行
  lbuf[len] = 0; //終端を置く

  switch (lbuf[0]) { //先頭の文字で分岐
  case '-': //「-」の場合
    sign = 1; //負の値
    len = 1;  //数字列はlbuf[1]以降
    break;
  case '+': //「+」の場合
    sign = 0; //正の値
    len = 1;  //数字列はlbuf[1]以降
    break;
  default:  //どれにも該当しない場合
    sign = 0; //正の値
    len = 0;  //数字列はlbuf[0]以降
    break;
  }

  value = 0; //値をクリア
  tmp = 0; //計算過程の値をクリア
  while (lbuf[len]) { //終端でなければ繰り返す
    tmp = 10 * value + lbuf[len++] - '0'; //数字を値に変換
    if (value > tmp) { //もし計算過程の値が前回より小さければ
      err = ERR_VOF; //オーバーフローを記録
    }
    value = tmp; //計算過程の値を記録
  }

  if (sign) //もし負の値なら
    return -value; //負の値に変換して持ち帰る

  return value; //値を持ち帰る
}

// キーワード検索
//[戻り値]
//  該当なし   : -1
//  見つかった : キーワードコード
int16_t lookup(char* str, uint16_t len) {
  int16_t fd_id;
  int16_t prv_fd_id = -1;
  uint16_t fd_len,prv_len;

  for (uint16_t j = 1; j <= len; j++) {
    fd_id = -1;
    for (uint16_t i = 0; i < SIZE_KWTBL; i++) {
      if (!strnicmp(kwtbl[i], str, j)) {
        fd_id = i;
        fd_len = j;        
        break;
      }
    }
    if (fd_id >= 0) {
      prv_fd_id = fd_id;
      prv_len = fd_len;
    } else {
      break;
    }
  }
  if (prv_fd_id >= 0) {
    prv_fd_id = -1;
    for (uint16_t i = 0; i < SIZE_KWTBL; i++) {
      if ( (strlen(kwtbl[i]) == prv_len) && !strnicmp(kwtbl[i], str, prv_len) ) {
        prv_fd_id = i;
        break;
      }
    }
  }
  return prv_fd_id;
}

//
// テキストを中間コードに変換
// [戻り値]
//   0 または 変換中間コードバイト数
//
uint8_t toktoi() {
  int16_t i;
  int16_t key;
  uint8_t len = 0;  // 中間コードの並びの長さ
  char* ptok;             // ひとつの単語の内部を指すポインタ
  char* s = lbuf;         // 文字列バッファの内部を指すポインタ
  char c;                 // 文字列の括りに使われている文字（「"」または「'」）
  uint32_t value;         // 定数
  uint32_t tmp;           // 変換過程の定数
  uint16_t hex;           // 16進数定数
  uint16_t hcnt;          // 16進数桁数
  uint16_t bin;           // 2進数定数
  uint16_t bcnt;          // 2進数桁数
  uint8_t var_len;        // 変数名長さ
  char var_name[3];       // 変数名
  
  while (*s) {                  //文字列1行分の終端まで繰り返す
    while (c_isspace(*s)) s++;  //空白を読み飛ばす

    key = lookup(s, strlen(s));
    if (key >= 0) {    
      // 該当キーワードあり
      if (len >= SIZE_IBUF - 1) {      // もし中間コードが長すぎたら
        err = ERR_IBUFOF;              // エラー番号をセット
        return 0;                      // 0を持ち帰る
      }
      ibuf[len++] = key;                 // 中間コードを記録
      s+= strlen(kwtbl[key]);

    } else {
      //err = ERR_SYNTAX; //エラー番号をセット
      //return 0;
    }

    // 16進数の変換を試みる $XXXX
    if (key == I_DOLLAR) {
      if (isHexadecimalDigit(*s)) {   // もし文字が16進数文字なら
        hex = 0;              // 定数をクリア
        hcnt = 0;             // 桁数
        do { //次の処理をやってみる          
          hex = (hex<<4) + hex2value(*s++); // 数字を値に変換
          hcnt++;
        } while (isHexadecimalDigit(*s)); //16進数文字がある限り繰り返す

        if (hcnt > 4) {      // 桁溢れチェック
          err = ERR_VOF;     // エラー番号オバーフローをセット
          return 0;          // 0を持ち帰る
        }
  
        if (len >= SIZE_IBUF - 3) { // もし中間コードが長すぎたら
          err = ERR_IBUFOF;         // エラー番号をセット
          return 0;                 // 0を持ち帰る
        }
        //s = ptok; // 文字列の処理ずみの部分を詰める
        len--;    // I_DALLARを置き換えるために格納位置を移動
        ibuf[len++] = I_HEXNUM;  //中間コードを記録
        ibuf[len++] = hex & 255; //定数の下位バイトを記録
        ibuf[len++] = hex >> 8;  //定数の上位バイトを記録
      }      
    }
 
    // 2進数の変換を試みる $XXXX
    if (key == I_APOST) {
      if ( *s == '0'|| *s == '1' ) {    // もし文字が2進数文字なら
        bin = 0;              // 定数をクリア
        bcnt = 0;             // 桁数
        do { //次の処理をやってみる
          bin = (bin<<1) + (*s++)-'0' ; // 数字を値に変換
          bcnt++;
        } while ( *s == '0'|| *s == '1' ); //16進数文字がある限り繰り返す

        if (bcnt > 16) {      // 桁溢れチェック
          err = ERR_VOF;     // エラー番号オバーフローをセット
          return 0;          // 0を持ち帰る
        }
  
        if (len >= SIZE_IBUF - 3) { // もし中間コードが長すぎたら
          err = ERR_IBUFOF;         // エラー番号をセット
          return 0;                 // 0を持ち帰る
        }
        len--;    // I_APOSTを置き換えるために格納位置を移動
        ibuf[len++] = I_BINNUM;  //中間コードを記録
        ibuf[len++] = bin & 255; //定数の下位バイトを記録
        ibuf[len++] = bin >> 8;  //定数の上位バイトを記録
      }      
    }

    //コメントへの変換を試みる
    if(key == I_REM|| key == I_SQUOT) {       // もし中間コードがI_REMなら
      while (c_isspace(*s)) s++;         // 空白を読み飛ばす
      ptok = s;                          // コメントの先頭を指す

      for (i = 0; *ptok++; i++);         // コメントの文字数を得る
      if (len >= SIZE_IBUF - 2 - i) {    // もし中間コードが長すぎたら
        err = ERR_IBUFOF;                // エラー番号をセット
        return 0;                        // 0を持ち帰る
      }

      ibuf[len++] = i;                   // コメントの文字数を記録
      while (i--) {                      // コメントの文字数だけ繰り返す
        ibuf[len++] = *s++;              // コメントを記録
      }
      break;                             // 文字列の処理を打ち切る（終端の処理へ進む）
    }

   if (key >= 0)                            // もしすでにキーワードで変換に成功していたら以降はスキップ
     continue;
   
    //定数への変換を試みる
    ptok = s;                            // 単語の先頭を指す
    if (isDigit(*ptok)) {              // もし文字が数字なら
      value = 0;                         // 定数をクリア
      tmp = 0;                           // 変換過程の定数をクリア
      do { //次の処理をやってみる
        tmp = 10 * value + *ptok++ - '0'; // 数字を値に変換
        if (tmp > 32768) {                // もし32768より大きければ          
          err = ERR_VOF;                  // エラー番号をセット
          return 0;                       // 0を持ち帰る
        }      
        value = tmp; //0を持ち帰る
      } while (isDigit(*ptok)); //文字が数字である限り繰り返す

      if (len >= SIZE_IBUF - 3) { //もし中間コードが長すぎたら
        err = ERR_IBUFOF; //エラー番号をセット
        return 0; //0を持ち帰る
      }

      if ( (value == 32768) && (len > 0) && (ibuf[len-1] != I_MINUS)) {
        // valueが32768のオーバーフローエラー☑
        err = ERR_VOF;                  // エラー番号をセット
        return 0;                       // 0を持ち帰る       
      }

      s = ptok; //文字列の処理ずみの部分を詰める      
      ibuf[len++] = I_NUM; //中間コードを記録
      ibuf[len++] = value & 255; //定数の下位バイトを記録
      ibuf[len++] = value >> 8; //定数の上位バイトを記録
    }
    else

    //文字列への変換を試みる
    if (*s == '\"' ) { //もし文字が '\"'
      c = *s++; //「"」か「'」を記憶して次の文字へ進む
      ptok = s; //文字列の先頭を指す
      //文字列の文字数を得る
      for (i = 0; (*ptok != c) && c_isprint(*ptok); i++)
        ptok++;
      if (len >= SIZE_IBUF - 1 - i) { //もし中間コードが長すぎたら
        err = ERR_IBUFOF; //エラー番号をセット
        return 0; //0を持ち帰る
      }
      ibuf[len++] = I_STR; //中間コードを記録
      ibuf[len++] = i; //文字列の文字数を記録
      while (i--) { //文字列の文字数だけ繰り返す
        ibuf[len++] = *s++; //文字列を記録
      }
      if (*s == c) s++; //もし文字が「"」か「'」なら次の文字へ進む
    }
    else

    //変数への変換を試みる(2017/07/26 A～Z9:対応)
    //  1文字目
    if (isAlpha(*ptok)) { //もし文字がアルファベットなら
      var_len = 0;
      if (len >= SIZE_IBUF - 2) { //もし中間コードが長すぎたら
        err = ERR_IBUFOF; 
        return 0;
      }
      var_name[var_len] = c_toupper(*ptok) - 'A';
      var_len++;

      //  2文字目('0'～'6'までが有効)
      if(isDigit(*(ptok+1)) && *(ptok+1) <='6' ) { //もしも文字が数字なら
         var_name[var_len] = *(ptok+1) - '0' + 1;
         var_len++;  
       } else {
         var_name[1] = 0;         
       }

      //もし変数が3個並んだら
      if (len >= 4 && ibuf[len - 2] == I_VAR && ibuf[len - 4] == I_VAR) {
         err = ERR_SYNTAX; //エラー番号をセット
         return 0; //0を持ち帰る
       }

      // 中間コードに変換
      ibuf[len++] = I_VAR; //中間コードを記録
      ibuf[len++] = var_name[0]+var_name[1]*26;
      s+=var_len; //次の文字へ進む
    }
    else

    //どれにも当てはまらなかった場合
    {
      err = ERR_SYNTAX; //エラー番号をセット
      return 0; //0を持ち帰る
    }
  } //文字列1行分の終端まで繰り返すの末尾

  ibuf[len++] = I_EOL; //文字列1行分の終端を記録
  return len; //中間コードの長さを持ち帰る
}


// Return free memory size
short getsize() {
  unsigned char* lp; //ポインタ

  for (lp = listbuf; *lp; lp += *lp); //ポインタをリストの末尾へ移動
  return listbuf + SIZE_LIST - lp - 1; //残りを計算して持ち帰る
}

// Get line numbere by line pointer
short getlineno(unsigned char *lp) {
  if(*lp == 0) //もし末尾だったら
    return -1;
  return *(lp + 1) | *(lp + 2) << 8; //行番号を持ち帰る
}

// Search line by line number
unsigned char* getlp(short lineno) {
  unsigned char *lp; //ポインタ

  for (lp = listbuf; *lp; lp += *lp) //先頭から末尾まで繰り返す
    if (getlineno(lp) >= lineno) //もし指定の行番号以上なら
      break; //繰り返しを打ち切る

  return lp; //ポインタを持ち帰る
}

// ラベルでラインポインタを取得する
// pLabelは [I_STR][長さ][ラベル名] であること
unsigned char* getlpByLabel(uint8_t* pLabel) {
  unsigned char *lp; //ポインタ
  uint8_t len;
  pLabel++;
  len = *pLabel; // 長さ取得
  pLabel++;      // ラベル格納位置
  
  for (lp = listbuf; *lp; lp += *lp)  { //先頭から末尾まで繰り返す
    if ( *(lp+3) == I_STR ) {
       if (len == *(lp+4)) {
           if (strncmp((char*)pLabel, (char*)(lp+5), len) == 0) {
              return lp;
           }
       }
    }  
  }
  return NULL;
}

// 行番号から行インデックスを取得する
uint16_t getlineIndex(uint16_t lineno) {
  unsigned char *lp; //ポインタ
  uint16_t index = 0;	
  uint16_t rc = 32767;
  for (lp = listbuf; *lp; lp += *lp) { // 先頭から末尾まで繰り返す
  	if (getlineno(lp) >= lineno) {     // もし指定の行番号以上なら
        rc = index;
  		break;                         // 繰り返しを打ち切る
  	}
  	index++;
  }
  return rc; 
}	

// ELSE中間コードをサーチする
// 引数   : 中間コードプログラムポインタ
// 戻り値 : NULL 見つからない
//          NULL以外 LESEの次のポインタ
//
uint8_t* getELSEptr(uint8_t* p) {
 uint8_t* rc = NULL;
 uint8_t* lp;
 
 // ブログラム中のGOTOの飛び先行番号を付け直す
  for (lp = p; *lp != I_EOL ; ) {
    switch(*lp) {
    case I_IF:    // IF命令
      goto DONE;
        break;
    case I_ELSE:  // ELSE命令
      rc = lp+1;
      goto DONE;
        break;
      break;
    case I_STR:     // 文字列
      lp += lp[1]+1;            
      break;
    case I_NUM:     // 定数
    case I_HEXNUM: 
    case I_BINNUM:
      lp+=3;        // 整数2バイト+中間コード1バイト分移動
      break;
    case I_VAR:     // 変数
      lp+=2;        // 変数名
      break;
    default:        // その他
      lp++;
      break;
    }
  }  
DONE:
  return rc;
}

// プログラム行数を取得する
uint16_t countLines(int16_t st=0, int16_t ed=32767) {
  unsigned char *lp; //ポインタ
  uint16_t cnt = 0;  
  int16_t lineno;
  for (lp = listbuf; *lp; lp += *lp)  {
    lineno = getlineno(lp);
    if (lineno < 0)
      break;
    if ( (lineno >= st) && (lineno <= ed)) 
      cnt++;
  }
  return cnt;   
}

// Insert i-code to the list
// [listbuf]に[ibuf]を挿入
//  [ibuf] : [1:データ長][1:I_NUM][2:行番号][中間コード]
//
void inslist() {
  unsigned char *insp;     // 挿入位置ポインタ
  unsigned char *p1, *p2;  // 移動先と移動元ポインタ
  short len;               // 移動の長さ

  // 空きチェク(これだと、空き不足時に行番号だけ入力時の行削除が出来ないかも.. @たま吉)
  if (getsize() < *ibuf) { // もし空きが不足していたら
    err = ERR_LBUFOF;      // エラー番号をセット
    return;                // 処理を打ち切る
  }

  insp = getlp(getlineno(ibuf)); // 挿入位置ポインタを取得

  // 同じ行番号の行が存在したらとりあえず削除
  if (getlineno(insp) == getlineno(ibuf)) { // もし行番号が一致したら
    p1 = insp;                              // p1を挿入位置に設定
    p2 = p1 + *p1;                          // p2を次の行に設定
    while ((len = *p2) != 0) {              // 次の行の長さが0でなければ繰り返す
      while (len--)                         // 次の行の長さだけ繰り返す
        *p1++ = *p2++;                      // 前へ詰める
    }
    *p1 = 0; // リストの末尾に0を置く
  }

  // 行番号だけが入力された場合はここで終わる
  if (*ibuf == 4) // もし長さが4（[長さ][I_NUM][行番号]のみ）なら
    return;

  // 挿入のためのスペースを空ける

  for (p1 = insp; *p1; p1 += *p1); // p1をリストの末尾へ移動
  len = p1 - insp + 1;             // 移動する幅を計算
  p2 = p1 + *ibuf;                 // p2を末尾より1行の長さだけ後ろに設定
  while (len--)                    // 移動する幅だけ繰り返す
    *p2-- = *p1--;                 // 後ろへズラす

  // 行を転送する
  len = *ibuf;     // 中間コードの長さを設定
  p1 = insp;       // 転送先を設定
  p2 = ibuf;       // 転送元を設定
  while (len--)    // 中間コードの長さだけ繰り返す
    *p1++ = *p2++; // 転送
}

//指定中間コード行レコードのテキスト出力
void putlist(unsigned char* ip, uint8_t devno=0) {
  unsigned char i;  // ループカウンタ
  uint8_t var_code; // 変数コード
  
  while (*ip != I_EOL) { //行末でなければ繰り返す
    //キーワードの処理
    if (*ip < SIZE_KWTBL) { //もしキーワードなら    
      c_puts(kwtbl[*ip],devno); //キーワードテーブルの文字列を表示
      if (*(ip+1) != I_COLON) 
        if ( ((!nospacea(*ip) || spacef(*(ip+1))) && (*ip != I_COLON) && (*ip != I_SQUOT))
        || ((*ip == I_CLOSE)&& (*(ip+1) != I_COLON  && *(ip+1) != I_EOL && !nospaceb(*(ip+1)))) ) //もし例外にあたらなければ
          c_putch(' ',devno); //空白を表示

      if (*ip == I_REM||*ip == I_SQUOT) { //もし中間コードがI_REMなら
        ip++; //ポインタを文字数へ進める
        i = *ip++; //文字数を取得してポインタをコメントへ進める
        while (i--) //文字数だけ繰り返す
          c_putch(*ip++,devno); //ポインタを進めながら文字を表示
        return;
      }
      ip++;//ポインタを次の中間コードへ進める
    }
    else

    //定数の処理
    if (*ip == I_NUM) { //もし定数なら
      ip++; //ポインタを値へ進める
      putnum(*ip | *(ip + 1) << 8, 0,devno); //値を取得して表示
      ip += 2; //ポインタを次の中間コードへ進める
      if (!nospaceb(*ip)) //もし例外にあたらなければ
        c_putch(' ',devno); //空白を表示
    }
    else

    //16進定数の処理
    if (*ip == I_HEXNUM) { //もし16進定数なら
      ip++; //ポインタを値へ進める
      c_putch('$',devno); //空白を表示
      putHexnum(*ip | *(ip + 1) << 8, 2,devno); //値を取得して表示
      ip += 2; //ポインタを次の中間コードへ進める
      if (!nospaceb(*ip)) //もし例外にあたらなければ
        c_putch(' ',devno); //空白を表示
    }
    else

    //2進定数の処理
    if (*ip == I_BINNUM) { //もし2進定数なら
      ip++; //ポインタを値へ進める
      c_putch('`',devno); //"`"を表示
      if (*(ip + 1))
          putBinnum(*ip | *(ip + 1) << 8, 16,devno); //値を取得して16桁で表示
      else
          putBinnum(*ip , 8,devno);  //値を取得して8桁で表示
      ip += 2; //ポインタを次の中間コードへ進める
      if (!nospaceb(*ip)) //もし例外にあたらなければ
        c_putch(' ',devno); //空白を表示
    }
    else     
    //変数の処理(2017/07/26 変数名 A～Z 9対応)
    if (*ip == I_VAR) { //もし定数なら
      ip++; //ポインタを変数番号へ進める
      var_code = *ip++;      
      c_putch( (var_code%26) + 'A',devno); //変数名を取得して表示
      if (var_code/26)
         c_putch( (var_code/26)+'0'-1, devno);

      if (!nospaceb(*ip)) //もし例外にあたらなければ
        c_putch(' ',devno); //空白を表示
    }
    else

    //文字列の処理
    if (*ip == I_STR) { //もし文字列なら
      char c; //文字列の括りに使われている文字（「"」または「'」）

      //文字列の括りに使われている文字を調べる
      c = '\"'; //文字列の括りを仮に「"」とする
      ip++; //ポインタを文字数へ進める
      for (i = *ip; i; i--) //文字数だけ繰り返す
        if (*(ip + i) == '\"') { //もし「"」があれば
          c = '\''; //文字列の括りは「'」
          break; //繰り返しを打ち切る
        }

      //文字列を表示する
      c_putch(c,devno); //文字列の括りを表示
      i = *ip++; //文字数を取得してポインタを文字列へ進める
      while (i--) //文字数だけ繰り返す
        c_putch(*ip++,devno); //ポインタを進めながら文字を表示
      c_putch(c,devno); //文字列の括りを表示
      if (*ip == I_VAR || *ip ==I_ELSE) //もし次の中間コードが変数だったら
        c_putch(' ',devno); //空白を表示
    }

    else { //どれにも当てはまらなかった場合
      err = ERR_SYS; //エラー番号をセット
      return;
    }
  }
}

// Get argument in parenthesis
short getparam() {
  short value; //値
  if (checkOpen()) return 0;
  if (getParam(value,false) )  return 0;
  if (checkClose()) return 0;
  return value; //値を持ち帰る
}

// INPUT handler
void iinput() {
  short value;          // 値
  short index;          // 配列の添え字or変数番号
  unsigned char i;      // 文字数
  unsigned char prompt; // プロンプト表示フラグ
  short ofvalue;        // オーバーフロー時の設定値
  uint8_t flgofset =0;  // オーバーフロ時の設定値指定あり

  sc->show_curs(1);
  prompt = 1;       // まだプロンプトを表示していない

  // プロンプトが指定された場合の処理
  if(*cip == I_STR){   // もし中間コードが文字列なら
    cip++;             // 中間コードポインタを次へ進める
    i = *cip++;        // 文字数を取得
    while (i--)        // 文字数だけ繰り返す
      c_putch(*cip++); // 文字を表示
    prompt = 0;        // プロンプトを表示した

    if (*cip != I_COMMA) {
      err = ERR_SYNTAX;
      goto DONE;
    }
    cip++;
  }

  // 値を入力する処理
  switch (*cip++) {         // 中間コードで分岐
  case I_VAR:             // 変数の場合
    index = *cip;         // 変数番号の取得
    cip++;
   
    // オーバーフロー時の設定値
    if (*cip == I_COMMA) {
      cip++;
      ofvalue = iexp();
      if (err) {
        goto DONE;
      }
      flgofset = 1;
    }
    
    if (prompt) {          // もしまだプロンプトを表示していなければ
      if (index >=26) {
       c_putch('A'+index%26);    // 変数名を表示
       c_putch('0'+index/26-1);  // 変数名を表示
      } else {
        c_putch('A'+index);  // 変数名を表示
      }
      c_putch(':');        //「:」を表示
    }
    
    value = getnum();     // 値を入力
    if (err) {            // もしエラーが生じたら
      if (err == ERR_VOF && flgofset) {
        err = ERR_OK;
        value = ofvalue;
      } else {
        return;            // 終了
      }
    }
    var[index] = value;  // 変数へ代入
    break;               // 打ち切る

  case I_ARRAY: // 配列の場合
    index = getparam();       // 配列の添え字を取得
    if (err)                  // もしエラーが生じたら
      goto DONE;

    if (index >= SIZE_ARRY) { // もし添え字が上限を超えたら
      err = ERR_SOR;          // エラー番号をセット
      goto DONE;
    }

    // オーバーフロー時の設定値
    if (*cip == I_COMMA) {
      cip++;
      ofvalue = iexp();
      if (err) {
        goto DONE;
      }
      flgofset = 1;
    }

    if (prompt) { // もしまだプロンプトを表示していなければ
      c_puts("@(");     //「@(」を表示
      putnum(index, 0); // 添え字を表示
      c_puts("):");     //「):」を表示
    }
    value = getnum(); // 値を入力
    if (err) {           // もしエラーが生じたら
      if (err == ERR_VOF && flgofset) {
        err = ERR_OK;
        value = ofvalue;
      } else {
        goto DONE;
      }
    }
    arr[index] = value; //配列へ代入
    break;              // 打ち切る

  default: // 以上のいずれにも該当しなかった場合
    err = ERR_SYNTAX; // エラー番号をセット
    goto DONE;
  } // 中間コードで分岐の末尾
DONE:  
  sc->show_curs(0);
}

// Variable assignment handler
void ivar() {
  short value; //値
  short index; //変数番号

  index = *cip++; //変数番号を取得して次へ進む

  if (*cip != I_EQ) { //もし「=」でなければ
    err = ERR_VWOEQ; //エラー番号をセット
    return;
  }
  cip++; //中間コードポインタを次へ進める
  if (*cip == I_STR) {
    cip++;
    value = (int16_t)((uint32_t)cip - (uint32_t)listbuf + V_PRG_TOP);
    cip += *cip+1;
  } else {
  //値の取得と代入
  value = iexp(); //式の値を取得
  if (err) //もしエラーが生じたら
    return; //終了
  }
  var[index] = value; //変数へ代入
}

// Array assignment handler
void iarray() {
  short value; //値
  short index; //配列の添え字

  index = getparam(); //配列の添え字を取得
  if (err) //もしエラーが生じたら
    return; //終了

  if (index >= SIZE_ARRY || index < 0 ) { //もし添え字が上下限を超えたら
    err = ERR_SOR; //エラー番号をセット
    return; //終了
  }

  if (*cip != I_EQ) { //もし「=」でなければ
    err = ERR_VWOEQ; //エラー番号をセット
    return; //終了
  }

  // 例: @(n)=1,2,3,4,5 の連続設定処理
  do {
    cip++; 
    if (*cip == I_STR) {
      cip++;
      value = (int16_t)((uint32_t)cip - (uint32_t)listbuf + V_PRG_TOP);
      cip += *cip+1;
    } else {
      value = iexp(); // 式の値を取得
      if (err)        // もしエラーが生じたら
        return;       // 終了
    }
    if (index >= SIZE_ARRY) { // もし添え字が上限を超えたら
      err = ERR_SOR;          // エラー番号をセット
      return; 
    }
    arr[index] = value; //配列へ代入
    index++;
  } while(*cip == I_COMMA);
} 

// LET handler
void ilet() {
  switch (*cip) { //中間コードで分岐
  case I_VAR: // 変数の場合
    cip++;     // 中間コードポインタを次へ進める
    ivar();    // 変数への代入を実行
    break;

  case I_ARRAY: // 配列の場合
    cip++;      // 中間コードポインタを次へ進める
    iarray();   // 配列への代入を実行
    break;
  
  default:      // 以上のいずれにも該当しなかった場合
    err = ERR_LETWOV; // エラー番号をセット
    break;            // 打ち切る
  }
}

// RUN command handler
void irun(uint8_t* start_clp = NULL) {
  uint8_t*   lp;     // 行ポインタの一時的な記憶場所
  gstki = 0;         // GOSUBスタックインデクスを0に初期化
  lstki = 0;         // FORスタックインデクスを0に初期化

  if (start_clp != NULL) {
    clp = start_clp;
  } else { 
    clp = listbuf;   // 行ポインタをプログラム保存領域の先頭に設定
  }

  while (*clp) {     // 行ポインタが末尾を指すまで繰り返す
    cip = clp + 3;   // 中間コードポインタを行番号の後ろに設定
    lp = iexe();     // 中間コードを実行して次の行の位置を得る
    if (err)         // もしエラーを生じたら
      return;    
    clp = lp;         // 行ポインタを次の行の位置へ移動
  }
}

// LISTコマンド
//  devno : デバイス番号 0:メインスクリーン 1:シリアル 2:グラフィック 3:、メモリー 4:ファイル
void ilist(uint8_t devno=0) {
  int16_t lineno = 0;          // 表示開始行番号
  int16_t endlineno = 32767;   // 表示終了行番号
  int16_t prnlineno;           // 出力対象行番号
  int16_t c;
  
  //表示開始行番号の設定
  if (*cip != I_EOL && *cip != I_COLON) {
    // 引数あり
    if (getParam(lineno,0,32767,false)) return;                // 表示開始行番号
    if (*cip == I_COMMA) {
      cip++;                         // カンマをスキップ
      if (getParam(endlineno,lineno,32767,false)) return;      // 表示終了行番号
     }
   }
 
  //行ポインタを表示開始行番号へ進める
  for ( clp = listbuf; *clp && (getlineno(clp) < lineno); clp += *clp); 
  
  //リストを表示する
  while (*clp) {               // 行ポインタが末尾を指すまで繰り返す

    //強制的な中断の判定
    c = c_kbhit();
    if (c) { // もし未読文字があったら
        if (c == SC_KEY_CTRL_C || c==27 ) { // 読み込んでもし[ESC],［CTRL_C］キーだったら
          err = ERR_CTR_C;                  // エラー番号をセット
          prevPressKey = 0;
          break;
        } else {
          prevPressKey = c;
        }
     }
    
    prnlineno = getlineno(clp);// 行番号取得
    if (prnlineno > endlineno) // 表示終了行番号に達したら抜ける
       break; 
    putnum(prnlineno, 0,devno);// 行番号を表示
    c_putch(' ',devno);        // 空白を入れる
    putlist(clp + 3,devno);    // 行番号より後ろを文字列に変換して表示
    if (err)                   // もしエラーが生じたら
      break;                   // 繰り返しを打ち切る
    newline(devno);            // 改行
    clp += *clp;               // 行ポインタを次の行へ進める
  }
}

// フラッシュメモリ内保存プログラムのエクスポート
// EXPORT [sno[,eno]]
void iexport() {
  uint8_t* exclp;
  int16_t endlineno = 32767;   // 表示終了行番号
  int16_t prnlineno;           // 出力対象行番号
  int16_t s_pno = 0;
  int16_t e_pno = FLASH_SAVE_NUM-1;

  if (*cip != I_EOL && *cip != I_COLON) {
    // 引数あり
    if (getParam(s_pno,0,FLASH_SAVE_NUM-1,false)) return;
    e_pno = s_pno;
    if (*cip == I_COMMA) {
      cip++;                         // カンマをスキップ
      if (getParam(e_pno,s_pno,FLASH_SAVE_NUM-1,false)) return;        
    }
  }
  
  for (uint16_t i =s_pno; i <= e_pno;i++) {
    exclp = FlashMan.getPrgAddress(i); // プログラム保存アドレスの取得
    if (!FlashMan.isExistPrg(i)) {
      // プログラムが保存されていない場合は、スキップ
      continue;
    }
    // リストの表示
    c_puts("NEW"); newline();       // "NEWの表示"

    //リストを表示する
    while (*exclp) {                // 行ポインタが末尾を指すまで繰り返す
      prnlineno = getlineno(exclp); // 行番号取得
      if (prnlineno > endlineno)    // 表示終了行番号に達したら抜ける
         break; 
      putnum(prnlineno, 0);         // 行番号を表示
      c_putch(' ');                 // 空白を入れる
      putlist(exclp + 3);           // 行番号より後ろを文字列に変換して表示
      if (err)                      // もしエラーが生じたら
        break;                      // 繰り返しを打ち切る
      newline();                    // 改行
      exclp += *exclp;              // 行ポインタを次の行へ進める
    }
    err = 0;
    c_puts("SAVE "); putnum(i, 0); newline(); // "SAVE XX"の表示
    newline();
  }
}

// プログラム消去
// 引数 0:全消去、1:プログラムのみ消去、2:変数領域のみ消去
void inew(uint8_t mode = 0) {
  unsigned char i; //ループカウンタ

  //変数と配列の初期化
  if (mode == 0|| mode == 2) {
    for (i = 0; i < SIZE_VAR; i++) //変数の数だけ繰り返す
      var[i] = 0; //変数を0に初期化
    
    for (i = 0; i < SIZE_ARRY; i++) //配列の数だけ繰り返す
      arr[i] = 0; //配列を0に初期化
  }
  //実行制御用の初期化
  if (mode !=2) {
    gstki = 0; //GOSUBスタックインデクスを0に初期化
    lstki = 0; //FORスタックインデクスを0に初期化
    *listbuf = 0; //プログラム保存領域の先頭に末尾の印を置く
    clp = listbuf; //行ポインタをプログラム保存領域の先頭に設定
  }
}

// RENUME command handler
void irenum() {
  uint16_t startLineNo = 10;  // 開始行番号
  uint16_t increase = 10;     // 増分
  uint8_t* ptr;               // プログラム領域参照ポインタ
  uint16_t len;               // 行長さ
  uint16_t i;                 // 中間コード参照位置
  uint16_t newnum;            // 新しい行番号
  uint16_t num;               // 現在の行番号
  uint16_t index;             // 行インデックス
  uint16_t cnt;               // プログラム行数
  
  // 開始行番号、増分引数チェック
  if (*cip == I_NUM) {               // もしRENUMT命令に引数があったら
    startLineNo = getlineno(cip);    // 引数を読み取って開始行番号とする
    cip+=3;
    if (*cip == I_COMMA) {
        cip++;                        // カンマをスキップ
        if (*cip == I_NUM) {          // 増分指定があったら
           increase = getlineno(cip); // 引数を読み取って増分とする
        } else {
           err = ERR_SYNTAX;          // カンマありで引数なしの場合はエラーとする
           return;
       }
    }
  }

  // 引数の有効性チェック
  cnt = countLines()-1;
  if (startLineNo <= 0 || increase <= 0) {
    err = ERR_VALUE;
    return;   
  }
  if (startLineNo + increase * cnt > 32767) {
    err = ERR_VALUE;
    return;       
  }

  // ブログラム中のGOTOの飛び先行番号を付け直す
  for (  clp = listbuf; *clp ; clp += *clp) {
     ptr = clp;
     len = *ptr;
     ptr++;
     i=0;
     // 行内検索
     while( i < len-1 ) {
        switch(ptr[i]) {
        case I_GOTO:  // GOTO命令
        case I_GOSUB: // GOSUB命令
          i++;
          if (ptr[i] == I_NUM) {
            num = getlineno(&ptr[i]);    // 現在の行番号を取得する
            index = getlineIndex(num);   // 行番号の行インデックスを取得する
            if (index == 32767) {
               // 該当する行が見つからないため、変更は行わない
               i+=3;
               continue;
            } else {
               // とび先行番号を付け替える
               newnum = startLineNo + increase*index;
               ptr[i+2] = newnum>>8;
               ptr[i+1] = newnum&0xff;
               i+=3;
               continue;
            }
          } 
          break;
      case I_STR:  // 文字列
        i++;
        i+=ptr[i]; // 文字列長分移動
        break;
      case I_NUM:  // 定数
      case I_HEXNUM: 
      case I_BINNUM: 
        i+=3;      // 整数2バイト+中間コード1バイト分移動
        break;
      case I_VAR:  // 変数
        i+=2;      // 変数名
        break;
      default:     // その他
        i++;
        break;
      }
    }
  }
  
  // 各行の行番号の付け替え
  index = 0;
  for (  clp = listbuf; *clp ; clp += *clp ) {
     newnum = startLineNo + increase * index;
     *(clp+1)  = newnum&0xff;
     *(clp+2)  = newnum>>8;
     index++;
  }
}

// CONFIGコマンド
// CONFIG 項目番号,設定値
void iconfig() {
  int16_t itemNo;
  int16_t value,value2 = 0,value3 = 0;

  if ( getParam(itemNo, true) ) return;  
  if ( getParam(value, false) ) return;  
  switch(itemNo) {
#if USE_NTSC == 1
  case 0: // NTSC補正: 垂直同期[,横位置補正,縦位置補正]
    if (value <-3 || value >3)  {
      err = ERR_VALUE;
      break;
    }
    if (*cip == I_COMMA) {
       cip++;
       if ( getParam(value2, -32,32, true) ) return;  // 横位置補正
       if ( getParam(value3, -32,32, false) ) return; // 縦位置補正
    }
    
    ((tTVscreen*)sc)->adjustNTSC(value,value2,value3);
    CONFIG.NTSC = value;
    CONFIG.NTSC_HPOS = value2;
    CONFIG.NTSC_VPOS = value3;
    if (scSizeMode != SCSIZE_MODE_SERIAL) {
        sc->end();
        ((tGraphicScreen*)sc)->init(ttbasic_font, SIZE_LINE, CONFIG.KEYBOARD,workarea, 
                                              scSizeMode, CONFIG.NTSC,CONFIG.NTSC_HPOS,CONFIG.NTSC_VPOS,0);
        sc->locate(0,0);
     }    
    break;
#endif
#if USE_NTSC == 1 || USE_OLED == 1 || USE_TFT == 1
  case 1: // キーボード補正
    if (value <0 || value >1)  {
      err = ERR_VALUE;
    } else {
      ((tGraphicScreen*)sc)->reset_kbd(value);
      CONFIG.KEYBOARD = value;
    }
    break;
#endif
  case 2: // プログラム自動起動番号設定
    if (value < -1 || value >FLASH_SAVE_NUM-1)  {
      err = ERR_VALUE;
    } else {
      CONFIG.STARTPRG = value;
    }
    break;
  default:
    err = ERR_VALUE;
    break;     
 }
}

// システム環境設定の保存
void isaveconfig() {
  err = FlashMan.saveConfig(CONFIG);  
}

// プログラム保存 SAVE 保存領域番号|"ファイル名"
void isave() {
  int16_t prgno = 0;
  int16_t ascii = 1;
  uint32_t flash_adr[FLASH_PAGE_PAR_PRG];
  char* fname;
  uint8_t mode = 0;
  int8_t rc;
  
  if (*cip == I_EOL) {
    prgno = 0;
  } else 
  // ファイル名またはプログラム番号の取得
  if ( *cip == I_STR || *cip == I_STRREF || *cip == I_CHR ||
       *cip == I_HEX || *cip == I_BIN || *cip == I_DMP
  ) {
    if(!(fname = getParamFname())) {
      return;
    }  
    mode = 1;
    if (*cip == I_COMMA) {
      cip++;
      if ( getParam(ascii, 0, 1, false) ) return;       
    }
  } else if ( getParam(prgno, 0, FLASH_SAVE_NUM-1, false) ) return;  
  if (mode == 1) {
#if USE_SD_CARD == 1
    // SDカードへの保存
    if (ascii) {
      rc = fs.tmpOpen(fname,1);
      if (rc == SD_ERR_INIT) {
        err = ERR_SD_NOT_READY;
        return;
      } else if (rc == SD_ERR_OPEN_FILE) {
        err =  ERR_FILE_OPEN;
        return;
      }
      ilist(4);
      fs.tmpClose();
    } else {
      // 通常のバイナリー保存
      if( fs.save(fname, listbuf, SIZE_LIST) ) {
        err = ERR_FILE_WRITE;
      }
    }
#endif
  } else {
    // 内部フラッシュメモリへの保存
    FlashMan.saveProgram(prgno, listbuf);
  }
}

// フラッシュメモリ上のプログラム消去 ERASE[プログラム番号[,プログラム番号]
void ierase() {
  int16_t  s_prgno, e_prgno;
  uint32_t flash_adr;

  if ( getParam(s_prgno, 0, FLASH_SAVE_NUM-1, false) ) return;
  e_prgno = s_prgno;
  if (*cip == I_COMMA) {
    cip++;
    if ( getParam(e_prgno, 0, FLASH_SAVE_NUM-1, false) ) return;
  }
  
  for (uint8_t prgno = s_prgno; prgno <= e_prgno; prgno++) {
     FlashMan.eraseProgram(prgno);
  }

}

// テキスト形式のプログラムのロード
// 引数
//   fname  :  ファイル名
//   newmode:  初期化モード 0:初期化する 1:変数を初期化しない 2:追記モード
// 戻り値
//   0:正常終了
//   1:異常終了
uint8_t loadPrgText(char* fname, uint8_t newmode = 0) {
  int16_t rc;
  int16_t len;
#if USE_SD_CARD == 1
  rc = fs.tmpOpen(fname,0);
  if (rc == SD_ERR_INIT) {
    err = ERR_SD_NOT_READY;
    return 1;
  } else if (rc == SD_ERR_OPEN_FILE) {
    err =  ERR_FILE_OPEN;
    return 1;
  }

  if (newmode != 2)
    inew(newmode);
  while(fs.readLine(lbuf)) {
     tlimR(lbuf); // 2017/07/31 追記
     len = toktoi();
     if (err) {
       c_puts(lbuf);
       newline(); 
       error(false);     
       continue;
     }
     if (*ibuf == I_NUM) {
        *ibuf = len; 
        inslist();
        if (err)
          error(true);
        continue; 
     }
  }
  fs.tmpClose();
#endif
  return 0;
}

// フラシュメモリからのプログラムロード
// 引数
//  progno:    プログラム番号   
//  newmmode:  初期化モード 0:初期化する 1:初期化しない
// 戻り値
//  0:正常終了
//  1:異常終了

uint8_t loadPrg(uint16_t prgno, uint8_t newmode=0) {
  if (!FlashMan.isExistPrg(prgno)) {
    err = ERR_NOPRG;
    return 1;
  }
  
  // 現在のプログラムの削除とロード
  inew(newmode);
  FlashMan.loadProgram(prgno, listbuf);
  return 0;
}

// 指定行の削除
// DELETE 行番号
// DELETE 開始行番号,終了行番号
void idelete() {
  int16_t sNo;
  int16_t eNo;
  uint8_t  *lp;      // 削除位置ポインタ 
  uint8_t *p1, *p2;  // 移動先と移動元ポインタ
  int16_t len;       // 移動の長さ

  if ( getParam(sNo, false) ) return;
  if (*cip == I_COMMA) {
     cip++;
     if ( getParam(eNo, false) ) return;  
  } else {
     eNo = sNo;
  }

  if (eNo < sNo) {
    err = ERR_VALUE;
    return;
  }

  if (eNo == sNo) {
    lp = getlp(eNo); // 削除位置ポインタを取得    
    if (getlineno(lp) == sNo) {
      // 削除
      p1 = lp;                              // p1を挿入位置に設定
      p2 = p1 + *p1;                        // p2を次の行に設定
      while ((len = *p2) != 0) {            // 次の行の長さが0でなければ繰り返す
        while (len--)                       // 次の行の長さだけ繰り返す
          *p1++ = *p2++;                    // 前へ詰める
      }
      *p1 = 0; // リストの末尾に0を置く
    }
  } else {
    for (uint16_t i = sNo; i <= eNo;i++) {
      lp = getlp(i); // 削除位置ポインタを取得
      if (getlineno(lp) == i) {               // もし行番号が一致したら
        p1 = lp;                              // p1を挿入位置に設定
        p2 = p1 + *p1;                        // p2を次の行に設定
        while ((len = *p2) != 0) {            // 次の行の長さが0でなければ繰り返す
          while (len--)                       // 次の行の長さだけ繰り返す
            *p1++ = *p2++;                    // 前へ詰める
        }
        *p1 = 0; // リストの末尾に0を置く
      }
    }
  }
}

// プログラムファイル一覧表示 FILES ["ファイルパス"]
void ifiles() {
  uint32_t flash_adr;
  uint8_t* save_clp;
  char* fname;
  char wildcard[SD_PATH_LEN];
  char* wcard = NULL;
  char* ptr = NULL;
  uint8_t flgwildcard = 0;
  int16_t StartNo, endNo; // プログラム番号開始、終了
  int16_t rc;

  // 引数がファイル名（文字列関数、文字列）の場合SDカードの一覧表示
  if ( *cip == I_STR || *cip == I_STRREF || *cip == I_CHR ||
       *cip == I_HEX || *cip == I_BIN || *cip == I_DMP
  ) {
    if(!(fname = getParamFname())) {
      return;
    }  

   for (uint8_t i = 0; i < strlen(fname); i++) {
      if (fname[i] >='a' && fname[i] <= 'z') {
         fname[i] = fname[i] - 'a' + 'A';
      }
   }
    
    if (strlen(fname) > 0) {
      for (int8_t i = strlen(fname)-1; i >= 0; i--) {
        if (fname[i] == '/') {
          ptr = &fname[i];
          break;
        }
        if (fname[i] == '*' || fname[i] == '?' || fname[i] == '.') 
          flgwildcard = 1;
      }       
      if (ptr != NULL && flgwildcard == 1) {
         strcpy(wildcard, ptr+1);
         wcard = wildcard;
         *(ptr+1) = 0;
      } else if (ptr == NULL && flgwildcard == 1) {
         strcpy(wildcard, fname);
         wcard = wildcard;
         strcpy(fname,"/");
      }
    }
#if USE_SD_CARD == 1
    rc = fs.flist(fname, wcard, sc->getWidth()/14);
    if (rc == SD_ERR_INIT) {
      err = ERR_SD_NOT_READY;
    } else if (rc == SD_ERR_OPEN_FILE) { 
      err = ERR_FILE_OPEN;
    }    
#endif
    return;
  }
  
  // フラッシュメモリのプログラムリスト
  
  // 引数が数値または、無しの場合、フラッシュメモリのリスト表示
  if (*cip == I_EOL || *cip == I_COLON) {
   StartNo = 0;
   endNo = FLASH_SAVE_NUM-1;
  } else {
   if ( getParam(StartNo, 0, FLASH_SAVE_NUM-1, false) ) return;
   // 第2引数チェック
   if (*cip == I_COMMA) {
     cip++;
     if ( getParam(endNo, false) ) return;  
    } else {
     endNo = StartNo;
    }
    if (StartNo > endNo) {
      err = ERR_VALUE;
      return;    
    }
  }     
  save_clp = clp;
  for (uint8_t i=StartNo ; i <= endNo; i++) {    
    flash_adr = (uint32_t)FlashMan.getPrgAddress(i); // プログラム保存アドレスの取得
    putnum(i,1);  c_puts(":");
    if(!FlashMan.isExistPrg(i)) {  //  プログラム有無のチェック
      c_puts("(none)");        
    } else {
      clp = (uint8_t*)flash_adr;
      if (*clp) {
        putlist(clp + 3);         // 行番号より後ろを文字列に変換して表示
      } 
    } 
    newline();
  }
  clp = save_clp;
}

// 画面クリア
void icls() {
  int16_t mode = 0;
#if USE_OLED || USE_TFT
  if (*cip != I_EOL && *cip != I_COLON) {
    // 引数あり
    if (getParam(mode,0,1,false)) return; // モードの取得
   }
#endif
  if (mode == 0) {
    sc->cls();
    sc->locate(0,0);
  }
#if USE_OLED || USE_TFT
  else if (mode == 1) {
    sc2.cls();
  }
#endif  
}

// 時間待ち
void iwait() {
  int16_t tm;
  if ( getParam(tm, 0, 32767, false) ) return;
  delay(tm);
}

// カーソル移動 LOCATE x,y
void ilocate() {
  int16_t x,  y;
  if ( getParam(x, true) ) return;
  if ( getParam(y, false) ) return;
  if ( x >= sc->getWidth() )   // xの有効範囲チェック
     x = sc->getWidth() - 1;
  else if (x < 0)  x = 0;  
  if( y >= sc->getHeight() )   // yの有効範囲チェック
     y = sc->getHeight() - 1;
  else if(y < 0)   y = 0;

  // カーソル移動
  sc->locate((uint16_t)x, (uint16_t)y);
}

// 文字色の指定 COLOLR fc,bc
void icolor() {
 int16_t fc,  bc = 0;
#if USE_TFT  == 1 || USE_OLED == 1
 if ( getParam(fc,false) ) return;
 if(*cip == I_COMMA) {
    cip++;
    if ( getParam(bc,false) ) return;  
 }
#else
 if ( getParam(fc, 0, 8, false) ) return;
 if(*cip == I_COMMA) {
    cip++;
    if ( getParam(bc, 0, 8, false) ) return;  
 }
#endif
  // 文字色の設定
  sc->setColor((uint16_t)fc, (uint16_t)bc);  
}

// 文字属性の指定 ATTRコマンド
void iattr() {
  int16_t attr;
  if ( getParam(attr, 0, 4, false) ) return;
  sc->setAttr(attr); 
}

// キー入力文字コードの取得 INKEY()関数
int16_t iinkey() {
  int16_t rc = 0;
  
  if (prevPressKey) {
    // 一時バッファに入力済キーがあればそれを使う
    rc = prevPressKey;
    prevPressKey = 0;
  } else if (c_kbhit( )) {
    // キー入力
    rc = c_getch();
  }
  return rc;
}

// メモリ参照　PEEK(adr[,bnk])
int16_t ipeek() {
  int16_t value =0, vadr;
  uint8_t* radr;

  if (checkOpen()) return 0;
  if ( getParam(vadr, false) )  return 0;
  if (checkClose()) return 0;  
  radr = v2realAddr(vadr);
  if (radr)
    value = *radr;
  else 
    err = ERR_RANGE;
  return value;
}

// スクリーン座標の文字コードの取得 'VPEEK(X,Y)'
int16_t ivpeek() {
  int16_t value; // 値
  int16_t x, y;  // 座標

  if (checkOpen()) return 0;
  if ( getParam(x, true) )  return 0;
  if ( getParam(y, false) ) return 0;
  if (checkClose()) return 0;
  value = (x < 0 || y < 0 || x >=sc->getWidth() || y >=sc->getHeight()) ? 0: sc->vpeek(x, y);
  return value;
}

// ピンモード設定(タイマー操作回避版)
//  この関数は、ArduinoSTM3 2R20170323のpinMode()不具合修正バージョンです。
//  最新のバージョンでは対応されています。
void Fixed_pinMode(uint8 pin, WiringPinMode mode) {
    gpio_pin_mode outputMode;
    bool pwm = false;

    if (pin >= BOARD_NR_GPIO_PINS) {
        return;
    }

    switch(mode) {
    case OUTPUT:
        outputMode = GPIO_OUTPUT_PP;
        break;
    case OUTPUT_OPEN_DRAIN:
        outputMode = GPIO_OUTPUT_OD;
        break;
    case INPUT:
    case INPUT_FLOATING:
        outputMode = GPIO_INPUT_FLOATING;
        break;
    case INPUT_ANALOG:
        outputMode = GPIO_INPUT_ANALOG;
        break;
    case INPUT_PULLUP:
        outputMode = GPIO_INPUT_PU;
        break;
    case INPUT_PULLDOWN:
        outputMode = GPIO_INPUT_PD;
        break;
    case PWM:
        outputMode = GPIO_AF_OUTPUT_PP;
        pwm = true;
        break;
    case PWM_OPEN_DRAIN:
        outputMode = GPIO_AF_OUTPUT_OD;
        pwm = true;
        break;
    default:
        return;
    }    
    gpio_set_mode(PIN_MAP[pin].gpio_device, PIN_MAP[pin].gpio_bit, outputMode);
   
    if (PIN_MAP[pin].timer_device != NULL) {
        if ( pwm ) { // we're switching into PWM, enable timer channels
        timer_set_mode(PIN_MAP[pin].timer_device,
                       PIN_MAP[pin].timer_channel,
                       TIMER_PWM );
        } else {  // disable channel output in non pwm-Mode             
            timer_cc_disable(PIN_MAP[pin].timer_device, 
                            PIN_MAP[pin].timer_channel); 
        }
    }
}

// GPIO ピン機能設定
void igpio() {
  int16_t pinno;       // ピン番号
  WiringPinMode pmode; // 入出力モード

  // 入出力ピンの指定
  if ( getParam(pinno, 0, I_PC15-I_PA0, true) ) return; // ピン番号取得
  pmode = (WiringPinMode)iexp();  if(err) return ;      // 入出力モードの取得

  // ピンモードの設定
  if (pmode == PWM) {
    // PWMピンとして利用可能かチェック
    if (!IsPWM_PIN(pinno)) {
      err = ERR_GPIO;
      return;    
    }
   
    Fixed_pinMode(pinno, pmode);
    pwmWrite(pinno,0);
  } else if (pmode == INPUT_ANALOG ) {
    // アナログ入力として利用可能かチェック
    if (!IsADC_PIN(pinno)) {
      err = ERR_GPIO;
      return;    
    }    
    Fixed_pinMode(pinno, pmode);
  } else {
    // デジタル入出力として利用可能かチェック
    if (!IsIO_PIN(pinno)) {
      err = ERR_GPIO;
      return;    
    }    
    Fixed_pinMode(pinno, pmode);    
  }
}

// GPIO ピンデジタル出力
void idwrite() {
  int16_t pinno,  data;

  if ( getParam(pinno, 0, I_PC15-I_PA0, true) ) return; // ピン番号取得
  if ( getParam(data, false) ) return;                  // データ指定取得
  data = data ? HIGH: LOW;

  if (! IsIO_PIN(pinno)) {
    err = ERR_GPIO;
    return;
  }
  
  // ピンモードの設定
  digitalWrite(pinno, data);
}

//
// PWM出力
// 引数
//   pin     PWM出力ピン
//   freq    出力パルス周波数(0 ～ 65535)
//   dcycle  デューティ比 (0～ 4095:4095で100%)
// 戻り値
//   0 正常
//   1 異常(PWMを利用出来ないピンを利用した)
//
uint8_t pwm_out(uint8_t pin, uint16_t freq, uint16_t duty) {
  uint32_t dc,f,base_div;
  timer_dev *dev = PIN_MAP[pin].timer_device;     // ピン対応のタイマーデバイスの取得 
  uint8 cc_channel = PIN_MAP[pin].timer_channel;  // ピン対応のタイマーチャンネルの取得
  if (! (dev && cc_channel) ) 
    return 1;  
    
  if (freq >=2048) {
    // 周波数が2048Hz以上の場合
    f =1000000/(uint32_t)freq;     // 周波数をカウント値に換算    
    base_div = TIMER_DIV;
  } else {
    // 周波数が2048Hz未満の場合
    f =1000000/16/(uint32_t)freq;  // 周波数をカウント値に換算    
    base_div = TIMER_DIV*16;
  }
  dc = f*(uint32_t)duty/4095;
  timer_set_prescaler(dev, base_div);          // システムクロックを1MHzに分周
  timer_set_reload(dev, f);                    // リセットカウント値を設定 
  timer_set_mode(dev, cc_channel,TIMER_PWM);
  timer_set_compare(dev,cc_channel,dc);        // 比較レジスタの初期値指定(デューティ比 0)
  return 0;
}

// PWMコマンド
// PWM ピン番号, DutyCycle, [周波数]
void ipwm() {
  int16_t pinno;      // ピン番号
  int16_t duty;       // デューティー値 0～4095
  int16_t freq = 490; // 周波数

  if ( getParam(pinno, 0, I_PC15-I_PA0, true) ) return;  // ピン番号取得
  if ( getParam(duty,  0, 4095, false) ) return;         // デューティー値

  if (*cip == I_COMMA) {
    cip++;
    if ( getParam(freq,  0, 32767, false) ) return;      // 周波数の取得
  }

  // PWMピンとして利用可能かチェック
  if (!IsPWM_PIN(pinno)) {
    err = ERR_GPIO;
    return;    
  }
    
  if (pwm_out(pinno, freq, duty))
      err = ERR_VALUE; 
}

// shiftOutコマンド SHIFTOUT dataPin, clockPin, bitOrder, value 
void ishiftOut() {
  int16_t dataPin, clockPin;
  int16_t bitOrder;
  int16_t data;

  if (getParam(dataPin, 0,I_PC15-I_PA0, true)) return;
  if (getParam(clockPin,0,I_PC15-I_PA0, true)) return;
  if (getParam(bitOrder,0,1, true)) return;
  if (getParam(data, 0,255,false)) return;

  if ( !IsIO_PIN(dataPin) ||  !IsIO_PIN(clockPin) ) {
    err = ERR_GPIO;
    return;
  }

  shiftOut(dataPin, clockPin, bitOrder, data);
}

// 16進文字出力 'HEX$(数値,桁数)' or 'HEX$(数値)'
void ihex(uint8_t devno=CDEV_SCREEN) {
  short value; // 値
  short d = 0; // 桁数(0で桁数指定なし)

  if (checkOpen()) return;
  if (getParam(value,false)) return;  
  if (*cip == I_COMMA) {
     cip++;
     if (getParam(d,0,4,false)) return;  
  }
  if (checkClose()) return;  
  putHexnum(value, d, devno);    
}

// 2進数出力 'BIN$(数値, 桁数)' or 'BIN$(数値)'
void ibin(uint8_t devno=CDEV_SCREEN) {
  int16_t value; // 値
  int16_t d = 0; // 桁数(0で桁数指定なし)

  if (checkOpen()) return;
  if (getParam(value,false)) return;  
  if (*cip == I_COMMA) {
     cip++;
     if (getParam(d,0,16,false)) return;  
  }
  if (checkClose()) return;
  putBinnum(value, d, devno);    
}

// 小数点数値出力 DMP$(数値) or DMP(数値,小数部桁数) or DMP(数値,小数部桁数,整数部桁指定)
void idmp(uint8_t devno=CDEV_SCREEN) {
  int32_t value;     // 値
  int32_t v1,v2;
  int16_t n = 2;    // 小数部桁数
  int16_t dn = 0;   // 整数部桁指定
  int32_t base=1;
  
  if (checkOpen()) return;
  if (getParam(value, false)) return;
  if (*cip == I_COMMA) { 
    cip++; 
    if (getParam(n, 0,4,false)) return;
    if (*cip == I_COMMA) { 
       cip++; 
      if (getParam(dn,-6,6,false)) return;
    }  
  }
  if (checkClose()) return;
  
  for (uint16_t i=0; i<n;i++) {
    base*=10;
  }
  v1 = value / base;
  v2 = value % base;
  if (v1 == 0 && value <0)
    c_putch('-',devno);  
  putnum(v1, dn, devno);
  if (n) {
    c_putch('.',devno);
    putnum(v2<0 ?-v2:v2, -n, devno);
  }
}

// 文字列参照 STR$(変数)
// STR(文字列参照変数|文字列参照配列変数|文字列定数,[pos,n])
// ※変数,配列は　[LEN][文字列]への参照とする
// 引数
//  devno: 出力先デバイス番号
// 戻り値
//  なし
//
void istrref(uint8_t devno=CDEV_SCREEN) {
  int16_t len;
  int16_t top;
  int16_t n;
  int16_t index;
  uint8_t *ptr;
  if (checkOpen()) return;
  if (*cip == I_VAR) {
    cip++;
    ptr = v2realAddr(var[*cip]);
    len = *ptr;
    ptr++;
    cip++;
  } else if (*cip == I_ARRAY) {
    cip++; 
    if (getParam(index, 0, SIZE_ARRY-1, false)) return;
    ptr = v2realAddr(arr[index]);
    len = *ptr;
    ptr++;    
  } else if (*cip == I_STR) {
    cip++;
    len = *cip;
    cip++;
    ptr = cip;
    cip+=len;    
  } else {
    err = ERR_SYNTAX;
    return;
  }
  top = 1;
  n = len;
  if (*cip == I_COMMA) { 
    cip++;
    if (getParam(top, 1,len,true)) return;
    if (getParam(n,1,len-top+1,false)) return;
  }
  if (checkClose()) return;
  for (uint16_t i = top-1; i <top-1+n; i++) {
    c_putch(ptr[i], devno);
  }
  return;
}

// POKEコマンド POKE ADR,データ[,データ,..データ]
void ipoke() {
  uint8_t* adr;
  int16_t value;
  int16_t vadr;
  
  // アドレスの指定
  vadr = iexp(); if(err) return ; 
  if (vadr < 0 ) { err = ERR_VALUE; return; }
  if(*cip != I_COMMA) { err = ERR_SYNTAX; return; }
  if(vadr>=V_FNT_TOP && vadr < V_GRAM_TOP) { err = ERR_RANGE; return; }
  
  // 例: 1,2,3,4,5 の連続設定処理
  do {
    adr = v2realAddr(vadr);
    if (!adr) {
      err = ERR_RANGE;
      break;
    }
    cip++;          // 中間コードポインタを次へ進める
    if (getParam(value,false)) return; 
    *((uint8_t*)adr) = (uint8_t)value;
    vadr++;
  } while(*cip == I_COMMA);
}

// I2CW関数  I2CW(I2Cアドレス, コマンドアドレス, コマンドサイズ, データアドレス, データサイズ)
int16_t ii2cw() {
  int16_t  i2cAdr, ctop, clen, top, len;
  uint8_t* ptr;
  uint8_t* cptr;
  int16_t  rc;

  if (checkOpen()) return 0;
  if (getParam(i2cAdr, 0, 0x7f, true)) return 0;
  if (getParam(ctop, 0, 32767, true)) return 0;
  if (getParam(clen, 0, 32767, true)) return 0;
  if (getParam(top, 0, 32767, true)) return 0;
  if (getParam(len, 0, 32767,false)) return 0;
  if (checkClose()) return 0;
  ptr  = v2realAddr(top);
  cptr = v2realAddr(ctop);
  if (ptr == 0 || cptr == 0 || v2realAddr(top+len) == 0 || v2realAddr(ctop+clen) == 0) 
     { err = ERR_RANGE; return 0; }

  // I2Cデータ送信
  I2C_WIRE.beginTransmission(i2cAdr);
  if (clen) {
    for (uint16_t i = 0; i < clen; i++)
      I2C_WIRE.write(*cptr++);
  }
  if (len) {
    for (uint16_t i = 0; i < len; i++)
      I2C_WIRE.write(*ptr++);
  }
  rc =  I2C_WIRE.endTransmission();
  return rc;
}

// I2CR関数  I2CR(I2Cアドレス, 送信データアドレス, 送信データサイズ,受信データアドレス,受信データサイズ)
int16_t ii2cr() {
  int16_t  i2cAdr, sdtop, sdlen,rdtop,rdlen;
  uint8_t* sdptr;
  uint8_t* rdptr;
  int16_t  rc;
  int16_t  n;

  if (checkOpen()) return 0;
  if (getParam(i2cAdr, 0, 0x7f, true)) return 0;
  if (getParam(sdtop, 0, 32767, true)) return 0;
  if (getParam(sdlen, 0, 32767, true)) return 0;
  if (getParam(rdtop, 0, 32767, true)) return 0;
  if (getParam(rdlen, 0, 32767,false)) return 0;
  if (checkClose()) return 0;
  sdptr = v2realAddr(sdtop);
  rdptr = v2realAddr(rdtop);
  if (sdptr == 0 || rdptr == 0 || v2realAddr(sdtop+sdlen) == 0 || v2realAddr(rdtop+rdlen) == 0) 
     { err = ERR_RANGE; return 0; }
 
  // I2Cデータ送受信
  I2C_WIRE.beginTransmission(i2cAdr);
  
  // 送信
  if (sdlen) {
    I2C_WIRE.write(sdptr, sdlen);
  }
  rc = I2C_WIRE.endTransmission();
  if (rdlen) {
    if (rc!=0)
      return rc;
    n = I2C_WIRE.requestFrom(i2cAdr, rdlen);
    while (I2C_WIRE.available()) {
      *(rdptr++) = I2C_WIRE.read();
    }
  }  
  return rc;
}

uint8_t _shiftIn( uint8_t ulDataPin, uint8_t ulClockPin, uint8_t ulBitOrder, uint8_t lgc){
  uint8_t value = 0 ;
  uint8_t i ;
  for ( i=0 ; i < 8 ; ++i ) {
    digitalWrite( ulClockPin, lgc ) ;
    if ( ulBitOrder == LSBFIRST )  value |= digitalRead( ulDataPin ) << i ;
    else  value |= digitalRead( ulDataPin ) << (7 - i) ;
    digitalWrite( ulClockPin, !lgc ) ;
  }
  return value ;
}

  
// SHIFTIN関数 SHIFTIN(データピン, クロックピン, オーダ[,ロジック])
int16_t ishiftIn() {
  int16_t rc;
  int16_t dataPin, clockPin;
  int16_t bitOrder;
  int16_t lgc = HIGH;

  if (checkOpen()) return 0;
  if (getParam(dataPin, 0,I_PC15-I_PA0, true)) return 0;
  if (getParam(clockPin,0,I_PC15-I_PA0, true)) return 0;
  if (getParam(bitOrder,0,1, false)) return 0;
  if (*cip == I_COMMA) {
    cip++;
    if (getParam(lgc,LOW, HIGH, false)) return 0;
  }
  if (checkClose()) return 0;
  rc = _shiftIn((uint8_t)dataPin, (uint8_t)clockPin, (uint8_t)bitOrder, lgc);
  return rc;
}

// PULSEIN関数 PULSEIN(ピン, 検出モード, タイムアウト時間[,単位指定])
int16_t ipulseIn() {
  int32_t rc=0;
  int16_t dataPin;       // ピン
  int16_t mode;          // 検出モード
  int16_t tmout;         // タイムアウト時間(0:ミリ秒 1:マイクロ秒)
  int16_t scale =1;      // 戻り値単位指定 (1～32767)

  // コマンドライン引数の取得
  if (checkOpen()) return 0;                              // '('のチェック
  if (getParam(dataPin, 0,I_PC15-I_PA0, true)) return 0;  // ピン
  if (getParam(mode, LOW, HIGH, true)) return 0;          // 検出モード
  if (getParam(tmout,0,32767, false)) return 0;           // タイムアウト時間(ミリ秒)
  if (*cip == I_COMMA) {
    cip++;
    if (getParam(scale,1, 32767, false)) return 0;        // 戻り値単位指定 (1～32767)
  }
  if (checkClose()) return 0;                             // ')'のチェック
  
  rc = pulseIn(dataPin, mode, ((uint32_t)tmout)*1000)/scale;  // パルス幅計測
  if (rc > 32767) rc = -1; // オーバーフロー時は-1を返す
  
  return rc;
}
  
  
// SETDATEコマンド  SETDATE 年,月,日,時,分,秒
void isetDate() {
#if USE_INNERRTC == 1
 #if defined(STM32_R20170323) // <<< RTClock R20170323安定版対応 >>>
  struct tm t;
  int16_t p_year, p_mon, p_day;
  int16_t p_hour, p_min, p_sec;

  if ( getParam(p_year, 1900,2036, true) ) return; // 年 
  if ( getParam(p_mon,     1,  12, true) ) return; // 月
  if ( getParam(p_day,     1,  31, true) ) return; // 日
  if ( getParam(p_hour,    0,  23, true) ) return; // 時
  if ( getParam(p_min,     0,  59, true) ) return; // 分
  if ( getParam(p_sec,     0,  61, false)) return; // 秒

  // RTCの設定
  t.tm_isdst = 0;             // サーマータイム [1:あり 、0:なし]
  t.tm_year  = p_year - 1900; // 年   [1900からの経過年数]
  t.tm_mon   = p_mon - 1;     // 月   [0-11] 0から始まることに注意
  t.tm_mday  = p_day;         // 日   [1-31]
  t.tm_hour  = p_hour;        // 時   [0-23]
  t.tm_min   = p_min;         // 分   [0-59]  
  t.tm_sec   = p_sec;         // 秒   [0-61] うるう秒考慮
  rtc.setTime(&t);            // 時刻の設定
 #else // <<< RTClock 更新版対応 >>>
  struct tm_t t;
  int16_t p_year, p_mon, p_day;
  int16_t p_hour, p_min, p_sec;

  if ( getParam(p_year, 1970,2105, true) ) return; // 年 
  if ( getParam(p_mon,     1,  12, true) ) return; // 月
  if ( getParam(p_day,     1,  31, true) ) return; // 日
  if ( getParam(p_hour,    0,  23, true) ) return; // 時
  if ( getParam(p_min,     0,  59, true) ) return; // 分
  if ( getParam(p_sec,     0,  61, false)) return; // 秒

  // RTCの設定
  t.year     = p_year - 1970; // 年   [1970からの経過年数]
  t.month    = p_mon;         // 月   [1-12]
  t.day      = p_day;         // 日   [1-31]
  t.hour     = p_hour;        // 時   [0-23]
  t.minute   = p_min;         // 分   [0-59]  
  t.second   = p_sec;         // 秒   [0-61] うるう秒考慮
  rtc.setTime(t);             // 時刻の設定
 #endif
#else
  err = ERR_SYNTAX; return;
#endif
}

// GETDATEコマンド  SETDATE 年格納変数,月格納変数, 日格納変数, 曜日格納変数
void igetDate() {
#if USE_INNERRTC == 1
 #if OLD_RTC_LIB == 1 || defined(STM32_R20170323) // <<< RTClock R20170323安定版対応 >>>
  int16_t index;  
  time_t tt; 
  struct tm* st;
  tt = rtc.getTime();   // 時刻取得
  st = localtime(&tt);  // 時刻型変換

  int16_t v[] = {
   (int16_t)st->tm_year+1900, 
   (int16_t)st->tm_mon+1,
   (int16_t)st->tm_mday,
   (int16_t)st->tm_wday
  };

  for (uint8_t i=0; i <4; i++) {    
    if (*cip == I_VAR) {          // 変数の場合
      cip++; index = *cip;        // 変数インデックスの取得
      var[index] = v[i];          // 変数に格納
      cip++;
    } else if (*cip == I_ARRAY) { // 配列の場合      
      cip++;
      index = getparam();         // 添え字の取得
      if (err) return;  
      if (index >= SIZE_ARRY || index < 0 ) {
         err = ERR_SOR;
         return; 
      }
      arr[index] = v[i];          // 配列に格納
    } else {
      err = ERR_SYNTAX;           // 変数・配列でない場合はエラーとする
      return;   
    }     
    if(i != 3) {
      if (*cip != I_COMMA) {      // ','のチェック
         err = ERR_SYNTAX;
         return; 
      }
      cip++;
    }
  }
 #else  // <<< RTClock 更新版対応 >>>
  int16_t index;  
  struct tm_t st;
  rtc.getTime(st);   // 時刻取得
  int16_t v[] = {
    st.year+1970, 
    st.month,
    st.day,
    st.weekday
  };

  for (uint8_t i=0; i <4; i++) {    
    if (*cip == I_VAR) {          // 変数の場合
      cip++; index = *cip;        // 変数インデックスの取得
      var[index] = v[i];          // 変数に格納
      cip++;
    } else if (*cip == I_ARRAY) { // 配列の場合      
      cip++;
      index = getparam();         // 添え字の取得
      if (err) return;  
      if (index >= SIZE_ARRY || index < 0 ) {
         err = ERR_SOR;
         return; 
      }
      arr[index] = v[i];          // 配列に格納
    } else {
      err = ERR_SYNTAX;           // 変数・配列でない場合はエラーとする
      return;   
    }     
    if(i != 3) {
      if (*cip != I_COMMA) {      // ','のチェック
         err = ERR_SYNTAX;
         return; 
      }
      cip++;
    }
  }
 #endif
#else
  err = ERR_SYNTAX;
#endif 
}

// GETDATEコマンド  SETDATE 時格納変数,分格納変数, 秒格納変数
void igetTime() {
#if USE_INNERRTC == 1
 #if OLD_RTC_LIB == 1 || defined(STM32_R20170323) // <<< RTClock R20170323安定版対応 >>>
  int16_t index;  
  time_t tt; 
  struct tm* st;
  tt = rtc.getTime();   // 時刻取得
  st = localtime(&tt);  // 時刻型変換

  int16_t v[] = {
    (int16_t)st->tm_hour,        // 時
    (int16_t)st->tm_min,         // 分
    (int16_t)st->tm_sec          // 秒
  };

  for (uint8_t i=0; i <3; i++) {    
    if (*cip == I_VAR) {          // 変数の場合
      cip++; index = *cip;        // 変数インデックスの取得
      var[index] = v[i];          // 変数に格納
      cip++;
    } else if (*cip == I_ARRAY) { // 配列の場合      
      cip++;
      index = getparam();         // 添え字の取得
      if (err) return;  
      if (index >= SIZE_ARRY || index < 0 ) {
         err = ERR_SOR;
         return; 
      }
      arr[index] = v[i];          // 配列に格納
    } else {
      err = ERR_SYNTAX;           // 変数・配列でない場合はエラーとする
      return;   
    }     
    if(i != 2) {
      if (*cip != I_COMMA) {      // ','のチェック
         err = ERR_SYNTAX;
         return; 
      }
      cip++;
    }
  }
 #else  // <<< RTClock 更新版対応 >>>
  int16_t index;  
  struct tm_t st;
  rtc.getTime(st);      // 時刻取得
  int16_t v[] = {
    st.hour,        // 時
    st.minute,      // 分
    st.second       // 秒
  };

  for (uint8_t i=0; i <3; i++) {    
    if (*cip == I_VAR) {          // 変数の場合
      cip++; index = *cip;        // 変数インデックスの取得
      var[index] = v[i];          // 変数に格納
      cip++;
    } else if (*cip == I_ARRAY) { // 配列の場合      
      cip++;
      index = getparam();         // 添え字の取得
      if (err) return;  
      if (index >= SIZE_ARRY || index < 0 ) {
         err = ERR_SOR;
         return; 
      }
      arr[index] = v[i];          // 配列に格納
    } else {
      err = ERR_SYNTAX;           // 変数・配列でない場合はエラーとする
      return;   
    }     
    if(i != 2) {
      if (*cip != I_COMMA) {      // ','のチェック
         err = ERR_SYNTAX;
         return; 
      }
      cip++;
    }
  }
 #endif
#else
  err = ERR_SYNTAX;
#endif  
}

// DATEコマンド
void idate() {
#if USE_INNERRTC == 1
 #if OLD_RTC_LIB == 1 || defined(STM32_R20170323) // <<< RTClock R20170323安定版対応 >>>
  static const char *wday[] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
   time_t tt; 
   struct tm* st;
   tt = rtc.getTime();   // 時刻取得
   st = localtime(&tt);  // 時刻型変換
   
   putnum((int16_t)st->tm_year+1900, -4);
   c_putch('/');
   putnum((int16_t)st->tm_mon+1, -2);
   c_putch('/');
   putnum((int16_t)st->tm_mday, -2);
   c_puts(" [");
   c_puts(wday[(int16_t)st->tm_wday]);
   c_puts("] ");
   putnum((int16_t)st->tm_hour, -2);
   c_putch(':');
   putnum((int16_t)st->tm_min, -2);
   c_putch(':');
   putnum((int16_t)st->tm_sec, -2);
   newline();  
 #else
  static const char *wday[] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
   struct tm_t st;
   rtc.getTime(st);      // 時刻取得  
   putnum(st.year+1970, -4);
   c_putch('/');
   putnum(st.month, -2);
   c_putch('/');
   putnum(st.day, -2);
   c_puts(" [");
   c_puts(wday[st.weekday]);
   c_puts("] ");
   putnum(st.hour, -2);
   c_putch(':');
   putnum(st.minute, -2);
   c_putch(':');
   putnum(st.second, -2);
   newline();
 #endif 
#else
  err = ERR_SYNTAX;
#endif  
 }

// EEPFORMAT コマンド
void ieepformat() {
  err = FlashMan.EEPFormat();
}

// EEPWRITE アドレス,データ コマンド
void ieepwrite() {
  int16_t adr;     // 書込みアドレス
  uint16_t data;   // データ

  if ( getParam(adr, 0, 32767, true) ) return; // アドレス
  if ( getParam(data, false) ) return;         // データ
     
  // データの書込み
  err = FlashMan.EEPWrite(adr, data);
}

// EEPREAD(アドレス) 関数
int16_t ieepread(uint16_t adr) {
  uint16_t data;

  if (adr > 32767) {
    err = ERR_VALUE;
    return 0;
 }
 // データの読み込み 
 err = FlashMan.EEPRead(adr, &data);
 return data;
}

// ドットの描画 PSET X,Y,C
void ipset() {
#if USE_NTSC == 1 || USE_TFT == 1 || USE_OLED == 1
 int16_t x,y,c;
 if (scmode||USE_TFT||USE_OLED) { // コンソールがデバイスコンソールの場合
    if (getParam(x,true)||getParam(y,true)||getParam(c,false)) 
    if (x < 0) x =0;
    if (y < 0) y =0;
    if (x >= sc2.getGWidth())  x = sc2.getGWidth()-1;
    if (y >= sc2.getGHeight()) y = sc2.getGHeight()-1;
  #if USE_NTSC == 1 || USE_OLED == 1
    if (c < 0 || c > 2) c = 1;
  #endif
    sc2.pset(x,y,c);
 } else {
    err = ERR_NOT_SUPPORTED;
 }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// 直線の描画 LINE X1,Y1,X2,Y2,C
void iline() {
#if USE_NTSC == 1 || USE_TFT == 1 || USE_OLED == 1 
 int16_t x1,x2,y1,y2,c;
  if (scmode||USE_TFT||USE_OLED) { // コンソールがデバイスコンソールの場合
    if (getParam(x1,true)||getParam(y1,true)||getParam(x2,true)||getParam(y2,true)||getParam(c,false)) 
    if (x1 < 0) x1 =0;
    if (y1 < 0) y1 =0;
    if (x2 < 0) x1 =0;
    if (y2 < 0) y1 =0;
    if (x1 >= sc2.getGWidth())  x1 = sc2.getGWidth()-1;
    if (y1 >= sc2.getGHeight()) y1 = sc2.getGHeight()-1;
    if (x2 >= sc2.getGWidth())  x2 = sc2.getGWidth()-1;
    if (y2 >= sc2.getGHeight()) y2 = sc2.getGHeight()-1;
  #if USE_NTSC == 1 || USE_OLED == 1
    if (c < 0 || c > 2) c = 1;
  #endif
    sc2.line(x1, y1, x2, y2, c);
  } else {
   err = ERR_NOT_SUPPORTED;
  }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// 円の描画 CIRCLE X,Y,R,C,F
void icircle() {
#if USE_NTSC == 1 || USE_TFT == 1 || USE_OLED == 1
  int16_t x,y,r,c,f;
  if (scmode||USE_TFT||USE_OLED) { // コンソールがデバイスコンソールの場合
    if (getParam(x,true)||getParam(y,true)||getParam(r,true)||getParam(c,true)||getParam(f,false)) 
    if (x < 0) x =0;
    if (y < 0) y =0;
    if (x >= sc2.getGWidth())  x = sc2.getGWidth()-1;
    if (y >= sc2.getGHeight()) y = sc2.getGHeight()-1;
  #if USE_NTSC == 1 || USE_OLED == 1
    if (c < 0 || c > 2) c = 1;
  #endif
    if (r < 0) r = 1;
    sc2.circle(x, y, r, c, f);
  } else {
   err = ERR_NOT_SUPPORTED;
  }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// 四角の描画 RECT X1,Y1,X2,Y2,C,F
void irect() {
#if USE_NTSC == 1 || USE_TFT == 1 || USE_OLED == 1
  int16_t x1,y1,x2,y2,c,f;
  if (scmode||USE_TFT||USE_OLED) { // コンソールがデバイスコンソールの場合
    if (getParam(x1,true)||getParam(y1,true)||getParam(x2,true)||getParam(y2,true)||getParam(c,true)||getParam(f,false)) 
      return;
    if (x1 < 0 || y1 < 0 || x2 < x1 || y2 < y1 || x2 >= sc2.getGWidth() || y2 >= sc2.getGHeight())  {
      err = ERR_VALUE;
      return;      
    }
  #if USE_NTSC == 1 || USE_OLED == 1
    if (c < 0 || c > 2) c = 1;
  #endif
    sc2.rect(x1, y1, x2-x1+1, y2-y1+1, c, f);
  }else {
   err = ERR_NOT_SUPPORTED;
  }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// ビットマップの描画 BITMAP 横座標, 縦座標, アドレス, インデックス, 幅, 高さ [,倍率]
void ibitmap() {
#if USE_NTSC == 1 || USE_TFT == 1 || USE_OLED == 1
  int16_t  x,y,w,h,d = 1,rgb = 0;
  int16_t  index;
  int16_t  vadr;
  uint8_t* adr;
  if (scmode||USE_TFT||USE_OLED) { // コンソールがデバイスコンソールの場合
    if (getParam(x,true)||getParam(y,true)||getParam(vadr,true)||getParam(index,true)||getParam(w,true)||getParam(h,false)) 
      return;
    if (*cip == I_COMMA) {
      cip++;
      if (getParam(d,false)) return;
    }
    if (*cip == I_COMMA) {
      cip++;
      if (getParam(rgb,0,1,false)) return;
    }

    adr = v2realAddr(vadr);
    if (!adr) {
      err = ERR_RANGE;
      return;
    }
  
    if (x < 0) x =0;
    if (y < 0) y =0;
    if (x >= sc2.getGWidth())  x = sc2.getGWidth()-1;
    if (y >= sc2.getGHeight()) y = sc2.getGHeight()-1;
    if (index < 0) index = 0;
    if (w < 0) w =1;
    if (h < 0) h =1; 
    if (d < 0) d = 1;
    sc2.bitmap(x, y, (uint8_t*)adr, index, w, h, d, rgb);
  } else {
   err = ERR_NOT_SUPPORTED;
  }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// キャラクタスクロール CSCROLL X1,Y1,X2,Y2,方向
// 方向 0: 上, 1: 下, 2: 右, 3: 左
void  icscroll() {
#if USE_NTSC == 1 || USE_OLED == 1
  int16_t  x1,y1,x2,y2,d;
  if (scmode||USE_OLED) {  // コンソールがデバイスコンソールの場合 
    if (getParam(x1,true)||getParam(y1,true)||getParam(x2,true)||getParam(y2,true)||getParam(d,false))
      return;
    if (x1 < 0 || y1 < 0 || x2 < x1 || y2 < y1 || x2 >= sc->getWidth() || y2 >= sc->getHeight())  {
      err = ERR_VALUE;
      return;      
    }
    if (d < 0 || d > 3) d = 0;
    sc2.cscroll(x1, y1, x2-x1+1, y2-y1+1, d);
  } else {
   err = ERR_NOT_SUPPORTED;
  }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// グラフィックスクロール GSCROLL X1,Y1,X2,Y2,方向
void igscroll() {
#if USE_NTSC == 1 || USE_OLED == 1
  int16_t  x1,y1,x2,y2,d;
  if (scmode||USE_OLED) {  // コンソールがデバイスコンソールの場合 
    if (getParam(x1,true)||getParam(y1,true)||getParam(x2,true)||getParam(y2,true)||getParam(d,false))
      return;
    if (x1 < 0 || y1 < 0 || x2 < x1 || y2 < y1 || x2 >= sc2.getGWidth() || y2 >= sc2.getGHeight()) {
      err = ERR_VALUE;
      return;      
    }
    if (d < 0 || d > 3) d = 0; 
    sc2.gscroll(x1,y1,x2-x1+1, y2-y1+1, d);
  } else {
 err = ERR_NOT_SUPPORTED;
  }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// シリアル1バイト出力 : SWRITE データ
void iswrite() {
  int16_t c; 
  if ( getParam(c, false) ) return;
   sc->Serial_write((uint8_t)c);
}

// シリアルモード設定: SMODE MODE [,"通信速度"]
// MODE 0                     : USB=コンソール、Serial1=データ通信
// MODE 1,"通信速度"          : USB=データ通信、Serial1=コンソール
// MODE 3,制御コード無効/有効 : ポートの機能変更なし
void ismode() {
  int16_t c;          // モード
  int16   flg;        // 制御コード有効/無効指定
  uint16_t ln;        // 通信速度文字数
  uint32_t baud = 0;  // 通信速度

  // 第1引数の取得
  if ( getParam(c, 0, 3, false) ) return;  
  
  if (c == 1) {
    // MODE 1 の場合,第2引数を取得する
    if(*cip != I_COMMA) {
      err = ERR_SYNTAX;
      return;      
    }
    cip++;
    if (*cip != I_STR) {
      err = ERR_VALUE;
      return;
    }

    cip++;        //中間コードポインタを次へ進める
    ln = *cip++;  //文字数を取得
  
    // 第2引数の文字列の評価
    for (uint16_t i=0; i < ln; i++) {
       if (*cip >='0' && *cip <= '9') {
          baud = baud*10 + *cip - '0';
       } else {
          err = ERR_VALUE;
          return;
       }
       cip++;
    }
  }
  else if (c == 3) {
    // MODE 3の場合、第2引数を取得する
    cip++;
    if ( getParam(flg, 0, 1, false) ) return;
    // 制御コード処理の設定
    sc->set_allowCtrl(flg);
    return;
  }
  
  // モードの設定
  sc->Serial_mode((uint8_t)c, baud);
  serialMode =c;
  defbaud = baud;
}

// シリアル1クローズ
void isclose() {
  delay(500);
  if(lfgSerial1Opened == true)
    //Serial1.end();
   sc->Serial_close();
  lfgSerial1Opened = false;    
}

// シリアル1オープン
void isopen() {
  uint16_t ln;
  uint32_t baud = 0;

  if(lfgSerial1Opened) {
    isclose();
  }

  if (*cip != I_STR) {
    err = ERR_VALUE;
    return;
  }
  
  cip++;        //中間コードポインタを次へ進める
  ln = *cip++;  //文字数を取得

  // 引数のチェック
  for (uint16_t i=0; i < ln; i++) {
     if (*cip >='0' && *cip <= '9') {
        baud = baud*10 + *cip - '0';
     } else {
        err = ERR_VALUE;
        return;
     }
     cip++;
  }
 sc->Serial_open(baud);
  lfgSerial1Opened = true;
}

// TONE 周波数 [,音出し時間]
void itone() {
  int16_t freq;   // 周波数
  int16_t tm = 0; // 音出し時間

  if ( getParam(freq, 0, 32767, false) ) return;
  if(*cip == I_COMMA) {
    cip++;
    if ( getParam(tm, 0, 32767, false) ) return;
  }
  dev_tone(freq, tm);
}

//　NOTONE
void inotone() {
  dev_notone();  
}

// GPEEK(X,Y)関数の処理
int16_t igpeek() {
#if USE_NTSC == 1 || USE_OLED == 1
  short x, y;  // 座標
  if (scmode||USE_OLED) { // コンソールがデバイスコンソールの場合
    if (checkOpen()) return 0;
    if ( getParam(x,true) || getParam(y,false) ) return 0; 
    if (checkClose()) return 0;
    if (x < 0 || y < 0 || x >= sc2.getGWidth() || y >= sc2.getGHeight()) return 0;
    return sc2.gpeek(x,y);  
  } else {
   err = ERR_NOT_SUPPORTED;
   return 0;
  }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// GINP(X,Y,H,W,C)関数の処理
int16_t iginp() {
#if USE_NTSC == 1 || USE_OLED == 1
  int16_t x,y,w,h,c;
  if (scmode||USE_OLED) { // コンソールがデバイスコンソールの場合
    if (checkOpen())  return 0;
    if ( getParam(x,true)||getParam(y,true)||getParam(w,true)||getParam(h,true)||getParam(c,false) ) return 0; 
    if (checkClose()) return 0;
    if (x < 0 || y < 0 || x >= sc2.getGWidth() || y >= sc2.getGHeight() || h < 0 || w < 0) return 0;    
    if (x+w >= sc2.getGWidth() || y+h >= sc2.getGHeight() ) return 0;     
    return sc2.ginp(x, y, w, h, c);  
  } else {
    err = ERR_NOT_SUPPORTED;
    return 0;
  }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// MAP(V,L1,H1,L2,H2)関数の処理
int16_t imap() {
  int32_t value,l1,h1,l2,h2,rc;
  if (checkOpen()) return 0;
  if ( getParam(value,true)||getParam(l1,true)||getParam(h1,true)||getParam(l2,true)||getParam(h2,false) ) 
    return 0;
  if (checkClose()) return 0;
  if (l1 >= h1 || l2 >= h2 || value < l1 || value > h1) {
    err = ERR_VALUE;
    return 0;
  }
  rc = (value-l1)*(h2-l2)/(h1-l1)+l2;
  return rc;  
}

// ASC(文字列)
// ASC(文字列,文字位置)
// ASC(変数,文字位置)
int16_t iasc() {
  int16_t value =0;
  int16_t len;     // 文字列長
  int16_t pos =1;  // 文字位置
  int16_t index;   // 配列添え字
  uint8_t* str;    // 文字列先頭位置
  
  if (checkOpen()) return 0;
  if ( *cip == I_STR) {  // 文字列定数の場合
     cip++;  len = *cip; // 文字列長の取得  
     cip++;  str = cip;  // 文字列先頭の取得
     cip+=len;
  } else if ( *cip == I_VAR) {   // 変数の場合
     cip++;   str = v2realAddr(var[*cip]);
     len = *str;
     str++;
     cip++;     
  } else if ( *cip == I_ARRAY) { // 配列変数の場合
     cip++; 
     if (getParam(index, 0, SIZE_ARRY-1, false)) return 0;
     str = v2realAddr(arr[index]);
     len = *str;
     str++;
  } else {
    err = ERR_SYNTAX;
    return 0;
  }
  if ( *cip == I_COMMA) {
    cip++;
    if (getParam(pos,1,len,false)) return 0;
  }
  value = str[pos-1];
  checkClose();
  return value;
}

// RGB(r,g,b)関数
int16 iRGB() {
  int16_t color_R,color_G,color_B; // 引数
  uint16_t rc; // 関数値
  
  // 引数の取得
  if (checkOpen())  return 0;   // '('のチェック
  if ( getParam(color_R, 0,31, true) ||  // 赤 5ビット
       getParam(color_G, 0,32, true) ||  // 緑 5ビット
       getParam(color_B, 0,31, false)    // 青 5ビット
  ) return 0; 
    if (checkClose()) return 0; // ')'のチェック
  
  // 緑のみ6ビットのため赤・青と同等に扱うため補正を行う
  if (color_G > 31) {
    color_G = 63;
  } else {
    color_G<<=1;
  }
  rc = ((color_R & B11111)<<11) | ((color_G & B111111) << 5) | (color_B & B11111);
  return (int16_t)rc;
}    

// PRINT handler
void iprint(uint8_t devno=0,uint8_t nonewln=0) {
  short value;     //値
  short len;       //桁数
  unsigned char i; //文字数
  
  len = 0; //桁数を初期化
  while (*cip != I_COLON && *cip != I_EOL) { //文末まで繰り返す
    switch (*cip) { //中間コードで分岐
    case I_STR:     //文字列
      cip++;
      i = *cip++; //文字数を取得
      while (i--) //文字数だけ繰り返す
        c_putch(*cip++, devno); //文字を表示
      break; 

    case I_SHARP: //「#
      cip++;
      len = iexp(); //桁数を取得
      if (err) {
        return;
      }
      break; 

    case I_CHR: // CHR$()関数
      cip++;
      if (getParam(value, 0,255,false)) break;   // 括弧の値を取得
//     if (!err)
      c_putch(value, devno);
     break;

    case I_HEX:  cip++; ihex(devno); break; // HEX$()関数
    case I_BIN:  cip++; ibin(devno); break; // BIN$()関数
    case I_DMP:  cip++; idmp(devno); break; // DMP$()関数
    case I_STRREF:cip++; istrref(devno); break; // STR$()関数
    case I_ELSE:        // ELSE文がある場合は打ち切る
       newline(devno);
       //return;
       goto END_PRINT;
       break;
       
    default: //以上のいずれにも該当しなかった場合（式とみなす）
      value = iexp();   // 値を取得
      if (err) {
        newline();
        return;
      }
      putnum(value, len,devno); // 値を表示
      break;
    } //中間コードで分岐の末尾
    
    if (err)  {
        newline(devno);
        return;
    }
    if (nonewln && *cip == I_COMMA) { // 文字列引数流用時はここで終了
        return;
    }
    if (*cip == I_ELSE) {
        newline(devno); 
        goto END_PRINT;
        //return;
    } else if (*cip == I_COMMA || *cip == I_SEMI) { // もし',' ';'があったら
      cip++;
      if (*cip == I_COLON || *cip == I_EOL || *cip == I_ELSE) //もし文末なら      
        //return; 
        goto END_PRINT;
    } else {    //',' ';'がなければ
      if (*cip != I_COLON && *cip != I_EOL) { //もし文末でなければ
        err = ERR_SYNTAX;
        newline(devno); 
        return;
      }
    }
  }
  if (!nonewln) {
    newline(devno);
  }
END_PRINT:
  if (USE_OLED ==1 && devno == 0 && 0 != scSizeMode ) {
#if USE_OLED == 1
    ((tOLEDScreen*)sc)->update();
#endif
  }
}

// GPRINT x,y,..
void igprint() {
#if USE_NTSC == 1 || USE_TFT ==1 || USE_OLED == 1
  int16_t x,y;
  if (scmode||USE_TFT||USE_OLED) { // コンソールがデバイスコンソールの場合
    if ( getParam(x, 0, sc2.getGWidth(), true) )  return;
    if ( getParam(y, 0, sc2.getGHeight(),true) )  return;
    sc2.set_gcursor(x,y);    iprint(2);
  } else {
    err = ERR_NOT_SUPPORTED;
  }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// ファイル名引数の取得
char* getParamFname() {
  cleartbuf(); // メモリバッファのクリア
  iprint(3,1);
  if (strlen(tbuf) >= SD_PATH_LEN)
      err = ERR_LONGPATH;   
  if (err) {
    if (err == ERR_RANGE)
      err = ERR_LONGPATH;
      return NULL;
  }
  return tbuf;
}

// LDBMP "ファイル名" ,アドレス, X, Y, W, H [,Mode]
void ildbmp() {
  char* fname;
  int16_t adr;
  int16_t x =0,y = 0,w = 0, h = 0,mode = 0;
  uint8_t* ptr;
  uint8_t rc;

  if(!(fname = getParamFname())) {
    return;
  }
  if (*cip != I_COMMA) {
    err = ERR_SYNTAX;
    return;    
  }
  cip++;
  
  if ( getParam(adr,0, 32767, true) ) return;   // アドレス
  if ( getParam(x,  0, 32767, true) ) return;   // x
  if ( getParam(y,  0, 32767, true) ) return;   // y
  if ( getParam(w,  0, 32767, true) ) return;   // w
  if ( getParam(h,  0, 32767, false) ) return;  // h
  if (*cip == I_COMMA) {
     cip++; 
     if ( getParam(mode,  0, 1, false) ) return;  // mode 
  }
  
  // 仮想アドレスから実アドレスへの変換
  ptr = v2realAddr(adr);
  if (ptr == NULL) {
    err = ERR_RANGE;
    return;
  }
  // 画像のロード
#if USE_SD_CARD == 1
rc = fs.loadBitmap(fname, ptr, x, y, w, h, mode);
  if (rc == SD_ERR_INIT) {
    err = ERR_SD_NOT_READY;
  } else if (rc == SD_ERR_OPEN_FILE) {
    err =  ERR_FILE_OPEN;
  } else if (rc == SD_ERR_READ_FILE) {
    err =  ERR_FILE_READ;
  }
#else
 err = ERR_NOT_SUPPORTED;
#endif 
}

// DWBMP  "ファイル名" ,X,Y,BX,BY,W,H[,mode]
void idwbmp() {
#if USE_NTSC == 1 || USE_OLED == 1
  int16_t x,y,bx,by,w, h, mode;
  uint8_t* ptr;
  uint8_t rc = 0; 
  int16_t bw;
  char* fname;
  if (scmode||USE_OLED) { // コンソールがデバイスコンソールの場合
    if(!(fname = getParamFname())) {
      return;
    }
  
    if (*cip != I_COMMA) {
      err = ERR_SYNTAX;
      return;    
    }
    cip++;
  
    // 引数取得
    if ( getParam(x,  0, sc2.getGWidth(), true) ) return;       // x
    if ( getParam(y,  0, ((tGraphicScreen*)sc)->getGHeight(), true) ) return;      // y
    if ( getParam(bx, 0, 32767, true) ) return;                // bx
    if ( getParam(by, 0, 32767, true) ) return;                // by
    if ( getParam(w,  0, sc2.getGWidth(), true) ) return;       // w
    if ( getParam(h,  0, sc2.getGHeight(), false) ) return;     // h
    if (*cip == I_COMMA) {
       cip++; if ( getParam(mode,  0, 1, false) ) return;      // mode     
    }
  
    // サイズチェック
    if (x + w > sc2.getGWidth() || y + h > sc2.getGHeight()) {
      err = ERR_RANGE;
      return;
    }
  
    // 画像のロード
  #if USE_SD_CARD == 1
   #if USE_TFT == 1
    bw  = sc2.getGWidth()/8;
    ptr = sc2.getGRAM() + bw*y + x/8;
    rc  = fs.loadBitmapToGVRAM(fname, ptr, x, y, bw, bx, by, w, h, mode);
   #elif USE_OLED == 1 
    bw  = sc2.getGWidth();
    ptr = sc2.getGRAM();
    rc  = fs.loadBitmapToGVRAM(fname, ptr, x, y, bw, bx, by, w, h, mode,1);
    sc2.update();
   #endif
    if (rc == SD_ERR_INIT) {
      err = ERR_SD_NOT_READY;
    } else if (rc == SD_ERR_OPEN_FILE) {
      err =  ERR_FILE_OPEN;
    } else if (rc == SD_ERR_READ_FILE) {
      err =  ERR_FILE_READ;
    }
  #endif

  } else {
   err = ERR_NOT_SUPPORTED;
  }
#elif USE_TFT ==1 && USE_SD_CARD == 1 
  // DWBMP  "ファイル名" ,X,Y
  int16_t x,y,bx,by,w, h;
  uint8_t* ptr;
  uint8_t rc; 
  char* fname;
  if (scmode||USE_TFT) { // コンソールがデバイスコンソールの場合
    if(!(fname = getParamFname())) {
      return;
    }
    if (*cip != I_COMMA) {
      err = ERR_SYNTAX;
      return;    
    }
    cip++;

    // 引数取得
    if ( getParam(x,  0, sc2.getGWidth()-1, true) ) return;       // x
    if ( getParam(y,  0, sc2.getGHeight()-1, false) ) return;      // y
    if (*cip == I_COMMA) {
       cip++;
       if ( getParam(bx, 0, 32767, true) ) return;   // bx
       if ( getParam(by, 0, 32767, true) ) return;   // by
       if ( getParam(w,  0, sc2.getGWidth(), true) ) return;       // w
       if ( getParam(h,  0, sc2.getGHeight(), false) ) return;     // h
    } else {
      bx = 0; by = 0; w = sc2.getGWidth()-x; h = sc2.getGHeight()-y;
    }
    // サイズチェック
    if (x+w  > sc2.getGWidth() || y+h > sc2.getGHeight()) {
      err = ERR_RANGE;
      return;
    }
    // 画像のロード
#if USE_TFT == 1
    rc = sc2.bmpDraw(fname,x,y,bx,by,w,h);
#elif USE_OLED == 1
    rc = sc2.bmpDraw(fname,x,y,bx,by,w,h);
#endif    
    if (rc == SD_ERR_INIT) {
      err = ERR_SD_NOT_READY;
    } else if (rc == SD_ERR_OPEN_FILE) {
      err =  ERR_FILE_OPEN;
    } else if (rc == SD_ERR_READ_FILE) {
      err =  ERR_FILE_READ;
    } else if (rc) {
      err = ERR_BAD_FNAME;
    }
  } else {
   err = ERR_NOT_SUPPORTED;
  }
#else
  err = ERR_NOT_SUPPORTED;
#endif
}

// MKDIR "ファイル名"
void imkdir() {
  uint8_t rc;
  char* fname;

  if(!(fname = getParamFname())) {
    return;
  }
  
#if USE_SD_CARD == 1
  rc = fs.mkdir(fname);
  if (rc == SD_ERR_INIT) {
    err = ERR_SD_NOT_READY;
  } else if (rc == SD_ERR_OPEN_FILE) {
    err = ERR_BAD_FNAME;
  }
#endif
}

// RMDIR "ファイル名"
void irmdir() {
  char* fname;
  uint8_t rc;

  if(!(fname = getParamFname())) {
    return;
  }

#if USE_SD_CARD == 1
  rc = fs.rmdir(fname);
  if (rc == SD_ERR_INIT) {
    err = ERR_SD_NOT_READY;
  } else if (rc == SD_ERR_OPEN_FILE) {
    err = ERR_BAD_FNAME;
  }
#endif
}
/****
// RENAME "現在のファイル名","新しいファイル名"
void irename() {
  char old_fname[SD_PATH_LEN];
  char new_fname[SD_PATH_LEN];
  uint8_t rc;
  
  if (*cip != I_STR) {
    err = ERR_SYNTAX;
    return;
  }

  cip++;

  // ファイル名指定
  if (*cip >= SD_PATH_LEN) {
    err = ERR_LONGPATH;
    return;
  }  

  // 現在のファイル名の取得
  strncpy(old_fname, (char*)(cip+1), *cip);
  old_fname[*cip]=0;
  cip+=*cip;
  cip++;

  if (*cip != I_COMMA) {
    err = ERR_SYNTAX;
    return;    
  }
  cip++;
  if (*cip!= I_STR) {
    err = ERR_SYNTAX;
    return;
  }

  cip++;

  // ファイル名指定
  if (*cip >= SD_PATH_LEN) {
    err = ERR_LONGPATH;
    return;
  }  

  // 新しいのファイル名の取得
  strncpy(new_fname, (char*)(cip+1), *cip);
  new_fname[*cip]=0;
  cip+=*cip;
  cip++;

  rc = fs.rename(old_fname,new_fname);
  if (rc) {
    err = ERR_FILE_WRITE;
    return;    
  }
}
**/
// REMOVE "ファイル名"
void iremove() {
  char* fname;
  uint8_t rc;

  if(!(fname = getParamFname())) {
    return;
  }

#if USE_SD_CARD == 1
  rc = fs.remove(fname);
  if (rc) {
    err = ERR_FILE_WRITE;
    return;    
  }
#endif
}

// BSAVE "ファイル名", アドレス
void ibsave() {
  uint8_t*radr; 
  int16_t vadr, len; 
  char* fname;
  uint8_t rc;

  if(!(fname = getParamFname())) {
    return;
  }

  if (*cip != I_COMMA) {
    err = ERR_SYNTAX;
    return;    
  }
  cip++;
  if ( getParam(vadr,  0, V_GRAM_TOP+6048-1, true) ) return; // アドレスの取得
  if ( getParam(len,  0, 32767, false) ) return;             // データ長の取得

  // アドレスの範囲チェック
  if ( (uint32_t)vadr+(uint32_t)len > (uint32_t)(V_GRAM_TOP+6048) ) {
    err = ERR_RANGE;
    return;
  }

  // ファイルオープン
#if USE_SD_CARD == 1
  rc = fs.tmpOpen(fname,1);
  if (rc == SD_ERR_INIT) {
    err = ERR_SD_NOT_READY;
    return;
  } else if (rc == SD_ERR_OPEN_FILE) {
    err =  ERR_FILE_OPEN;
    return;
  }
  
  // データの書込み
  for (uint16_t i = 0 ; i < len; i++) {
    radr = v2realAddr(vadr);
    if (radr == NULL) {
      goto DONE;
    }
    if(fs.putch(*radr)) {
      err = ERR_FILE_WRITE;
      goto DONE;
    }
    vadr++;
  }

DONE:
  fs.tmpClose();
#endif
  return;
}

void ibload() {
  uint8_t*radr; 
  int16_t vadr, len ,c;
  char* fname;
  uint8_t rc;

  if(!(fname = getParamFname())) {
    return;
  }

  if (*cip != I_COMMA) {
    err = ERR_SYNTAX;
    return;    
  }
  cip++;
  if ( getParam(vadr,  0, V_GRAM_TOP+6048-1, true) ) return;  // アドレスの取得
  if ( getParam(len,  0, 32767, false) ) return;              // データ長の取得

  // アドレスの範囲チェック
  if ( (uint32_t)vadr+(uint32_t)len > (uint32_t)(V_GRAM_TOP+6048) ) {
    err = ERR_RANGE;
    return;
  }
#if USE_SD_CARD == 1
  // ファイルオープン
  rc = fs.tmpOpen(fname,0);
  if (rc == SD_ERR_INIT) {
    err = ERR_SD_NOT_READY;
    return;
  } else if (rc == SD_ERR_OPEN_FILE) {
    err =  ERR_FILE_OPEN;
    return;
  }

  // データの読込み
  for (uint16_t i = 0 ; i < len; i++) {
    radr = v2realAddr(vadr);
    if (radr == NULL) {
      goto DONE;
    }
    c = fs.read();
    if (c <0 ) {
      err = ERR_FILE_READ;
      goto DONE;      
    }
    *radr = c;
    vadr++;
  }

DONE:
  fs.tmpClose();
#endif  
  return;
}

// TYPE "ファイル名"
void  icat() {
  int16_t line = 0;
  uint8_t c;

  char* fname;
  int16_t rc;

  if(!(fname = getParamFname())) {
    return;
  }

#if USE_SD_CARD == 1
  while(1) {
    rc = fs.textOut(fname, line, sc->getHeight()); 
    if (rc < 0) {
      if (rc == -SD_ERR_OPEN_FILE) {
        err = ERR_FILE_OPEN;
        return;
      } else if (rc == -SD_ERR_INIT) {
        err = ERR_SD_NOT_READY;
        return;
      } else if (rc == -SD_ERR_NOT_FILE) {
        err =  ERR_BAD_FNAME;
        return;
      }
    } else if (rc == 0) {
      break;
    }
   
    c_puts("== More?('Y' or SPACE key) =="); newline();
    c = c_getch();
    if (c != 'y' && c!= 'Y' && c != 32)
      break;      
    line += sc->getHeight();
  }
#endif
}

// ターミナルスクリーンの画面サイズ指定 WIDTH W,H
void iwidth() {
  int16_t w, h;

  // 引数チェック
  if ( getParam(w,  16, SIZE_LINE, true) ) return;   // w
  if ( getParam(h,  10,  45, false) ) return;        // h
  if (scmode == 0) { // コンソールがシリアルコンソールの場合
    // 現在、ターミナルモードの場合は画面をクリアして、再設定する
    sc->cls();
    sc->locate(0,0);
    sc->end();
    sc->init(w, h, SIZE_LINE, workarea); // スクリーン初期設定
    sc->Serial_mode(serialMode, defbaud); // シリアルコンソールをUSARTに設定する
  }
}

// スクリーンモード指定 SCREEN M
void iscreen() {
#if USE_SCREEN_MODE == 1 // <<< デバイスコンソール利用可能定義開始 >>>
  int16_t mode;            // スクリーンサイズモード
  int16_t rt = DEV_RTMODE; // 画面向きのデフォルト指定

  if ( getParam(mode, 1, MAX_SCMODE, false) ) return;   // m
 
 #if USE_TFT == 1 || USE_OLED == 1 
  if (*cip == I_COMMA) {
    cip++;
    if ( getParam(rt,0, 3, false) ) return;   // 画面回転 0～3
  }
 #endif

  if (scSizeMode == mode && scrt == rt) {
    return; // 画面サイズの変更が無いので終了
  }
  
  if (scSizeMode == SCSIZE_MODE_SERIAL) {
    // 直前の画面がシリアルコンソールの場合、シリアルコンソールを開放する
    sc->cls();            // 画面クリア
    sc->show_curs(true);  // カーソル表示
    sc->locate(0,0);      // ホームポジションに移動
    sc->end();            // 終了・資源開放
    sc = &sc2;            // カレントスクリーンの切替
  }
 #if USE_NTSC == 1 
  else {
    // NTSC画面 資源開放
    sc->end();
  }
 #endif
  
  // NTSCスクリーン設定
  scrt = rt;
  scSizeMode = mode; // 新しい画面サイズモードの保持
  scmode = 1;        // シリアルコンソールOFF
 #if USE_NTSC == 1   
  ((tGraphicScreen*)sc)->init(ttbasic_font, SIZE_LINE, CONFIG.KEYBOARD,workarea, 
                              scSizeMode, CONFIG.NTSC,CONFIG.NTSC_HPOS,CONFIG.NTSC_VPOS,0); 
 #elif USE_TFT == 1 || USE_OLED == 1 
  ((tGraphicScreen*)sc)->setScreen(scSizeMode,scrt);
 #endif  
  ((tGraphicScreen*)sc)->cls();
  ((tGraphicScreen*)sc)->show_curs(false);
  ((tGraphicScreen*)sc)->draw_cls_curs();
  ((tGraphicScreen*)sc)->locate(0,0);
  ((tGraphicScreen*)sc)->refresh();    
  sc->Serial_mode(serialMode, defbaud);  // デバイスコンソールのシリアル設定
  
#else
   err = ERR_NOT_SUPPORTED;
#endif
}

//
// コンソールモード指定 console N
// 機能 コンソールをデバイスコンソール、シリアルコンソール間で切り替える
// 引数
//  useParam false: コマンドラインから引数を取得する、true: 引数はparamArgを使用する
//  paramArg コンソールモード(コマンドラインではなく関数呼び出しで引数渡) 
//  設定値 CON_MODE_DEVICE:デバイスコンソールモード、CON_MODE_SERIAL:シリアルコンソールモード
//

void iconsole(uint8_t useParam=false, uint8_t paramArg=CON_MODE_DEVICE) {
#if USE_SCREEN_MODE == 1 // <<< デバイスコンソール利用可能定義開始 >>>

  int16_t mode;        // コマンドライン引数  コンソールモード
  int16_t rt = scrt;   // 画面の向き

  if (useParam == false) {
    // コマンドラインから引数：コンソールモード指定 を取得する
    if ( getParam(mode,  CON_MODE_DEVICE, CON_MODE_SERIAL, false) ) {
      return;   // // 引数の値取得とチェック
    }
    if ( ( mode == CON_MODE_DEVICE && scSizeMode != SCSIZE_MODE_SERIAL) ||
         ( mode == CON_MODE_SERIAL && scSizeMode == SCSIZE_MODE_SERIAL)) {
      return; // 指定したモードが現在と変化なしの場合は終了する
    }
  } else {
    // 関数引数でのコンソールモードしえ値
    mode = paramArg;
  }

  if (mode == CON_MODE_SERIAL) {     // デバイスコンソール => シリアルコンソール切り替え    
    prv_scrt = scrt;                 // 現在の画面向きを保存
    prv_scSizeMode = scSizeMode;     // 現時点の画面サイズモードを保存する
    scSizeMode = SCSIZE_MODE_SERIAL; // 画面サイズモードに リアルコンソールをセッ
    scmode = 0;                      // シリアルコンソールON
    sc->cls();                       // 画面クリア
   
  #if USE_NTSC == 1
    sc->end();  // NTSCデバイスの場合 資源開放
  #endif
    
    sc = &sc1;  // カレントスクリーンをシリアルコンソールし、初期化と資源獲得
    ((tTermscreen*)sc)->init(TERM_W,TERM_H,SIZE_LINE, workarea); 
    sc->Serial_mode(serialMode, defbaud); // シリアルコンソールをUSARTに設定
    sc->cls();     
  } else {  // シリアルコンソール => デバイススクリーン設定
    // シリアルコンソール画面のクリア・終了・資源開放
    sc->cls();            // 画面クリア
    sc->show_curs(true);  // カーソル表示
    sc->locate(0,0);      // ホームポジションに移動
    sc->end();            // 終了・資源開放
    // カレントスクリーンをNTSCスクリーンにし、初期化
    sc = &sc2;                                  // スクルーン切替
    scrt = prv_scrt;                            // 画面向きの設定
    scSizeMode = prv_scSizeMode;                // 前回の画面サーズモードをセット        
    scmode = 1;                                 // シリアルコンソールOFF
    if (!scSizeMode) scSizeMode = DEV_SCMODE;   // 画面サイズモードの設定
    prv_scSizeMode = SCSIZE_MODE_SERIAL;        // 直前の画面サーズモードにシリアルコンソールをセット

  #if USE_NTSC == 1
    // NTSCの場合、指定した画面サイズでのビデオ信号を生成する
    ((tGraphicScreen*)sc)->init(ttbasic_font, SIZE_LINE, CONFIG.KEYBOARD, workarea, scSizeMode,
                                CONFIG.NTSC, CONFIG.NTSC_HPOS,CONFIG.NTSC_VPOS,0);
  #elif USE_TFT == 1 || USE_OLED == 1
    ((tGraphicScreen*)sc)->setScreen(scSizeMode, scrt);  // 画面表示設定
  #endif   
    ((tGraphicScreen*)sc)->cls();              // 画面クリア
    ((tGraphicScreen*)sc)->show_curs(false);   // カーソル非表示おモード
    ((tGraphicScreen*)sc)->draw_cls_curs();    // カーソル消去
    ((tGraphicScreen*)sc)->locate(0,0);        // ホームポジション
    ((tGraphicScreen*)sc)->refresh();          // 画面リフレッシュ
     sc->Serial_mode(serialMode, defbaud);  // デバイスコンソールのシリアルコンソールをUSARTに切り替える
  }
#else //<<< デバイスコンソール利用可能定義終了 >>>
   err = ERR_NOT_SUPPORTED;
#endif
}
  
//
// プログラムのロード・実行 LRUN/LOAD
// LRUN プログラム番号
// LRUN プログラム番号,行番号
// LRUN プログラム番号,"ラベル"
// LRUN "ファイル名"
// LRUN "ファイル名",行番号
// LRUN "ファイル名","ラベル"
// LOAD プログラム番号
// LOAD "ファイル名"

// 戻り値
//  1:正常 0:異常
//
uint8_t ilrun() {
  int16_t prgno, lineno = -1;
  uint8_t *lp;
  //char fname[SD_PATH_LEN];  // ファイル名
  uint8_t label[34];
  uint8_t len;
  uint8_t mode = 0;        // ロードモード 0:フラッシュメモリ 1:SDカード
  int8_t fg;               // ファイル形式 0:バイナリ形式  1:テキスト形式  
  uint8_t rc;
  uint8_t islrun = 1;
  uint8_t newmode = 1;
  char* fname;
  int16_t flgMerge = 0;    // マージモード
  uint8_t* ptr;
  uint16_t sz;
  uint8_t flgPrm2 = 0;    // 第2引数の有無
  
  // コマンド識別
  if (*(cip-1) == I_LOAD) {
     islrun  = 0;
     lineno  = 0;
     newmode = 0;
  }
  
  // ファイル名またはプログラム番号の取得
  if ( *cip == I_STR || *cip == I_STRREF || *cip == I_CHR ||
       *cip == I_HEX || *cip == I_BIN || *cip ==I_DMP
  ) {
    if(!(fname = getParamFname())) {
      return 0;
    }
    mode = 1;
  } else {
    // 内部フラッシュメモリからの読込＆実行
    if (*cip == I_EOL) {
     prgno = 0;
    } else { if ( getParam(prgno, 0, FLASH_SAVE_NUM-1, false) )
     return 0; // プログラム番号
    }
  }
 
 if (islrun) {
   // LRUN
   // 第2引数 行番号またはラベルの取得
   if(*cip == I_COMMA) {   // 第2引数の処理
      cip++;
      if (*cip == I_STR) { // ラベルの場合 
        cip++;
        len = *cip <= 32 ? *cip : 32;
        label[0] = I_STR;
        label[1] = len;
        strncpy((char*)label+2, (char*)cip+1, len); 
        cip+=*cip+1;
      } else {             // 行番号の場合
        if ( getParam(lineno, 0, 32767, false) ) return 0;
      }
    } else {
      lineno = 0;
    }
  } else {
   // LOAD
   if(*cip == I_COMMA) {   // 第2引数の処理
      cip++;
      if ( getParam(flgMerge, 0, 1, false) ) return 0;
      flgPrm2 = 1;
      if (flgMerge == 0) {
        newmode = 0; // 上書きモード   
      } else {
        newmode = 2; // 追記モード
      }
    }
  }
  
  // プログラムのロード処理
  if (mode == 0) {
    // フラッシュメモリからプログラムのロード
    if (!islrun && flgPrm2) {
      // LOADコマンド時、フラッシュメモリからのロードで第2引数があった場合はエラーとする
      err = ERR_SYNTAX;
      return 0;
    }
    if ( loadPrg(prgno,newmode) ) {
      return 0;
    }
  } else {
#if USE_SD_CARD == 1
    // SDカードからプログラムのロード
    // SDカードからのロード
    fg = fs.IsText(fname); // 形式チェック
    if (fg < 0) {
      // 形式異常
      rc = -fg;
      if( rc == SD_ERR_INIT ) {
        err = ERR_SD_NOT_READY;
      } else if (rc == SD_ERR_OPEN_FILE) {
        err = ERR_FILE_OPEN;        
      } else if (rc == SD_ERR_READ_FILE) {
        err = ERR_FILE_READ;
      } else if (rc == SD_ERR_NOT_FILE) {
        err = ERR_BAD_FNAME;
      }
    } else if (fg == 0) {
      // SDカードからのバイナリ形式ロード
      ptr = listbuf;
      sz  = SIZE_LIST;
      if (newmode != 2) {
        inew(newmode);
      } else {
        // ロード位置を追記開始位置に修正
        
        for (ptr = listbuf; *ptr; ptr += *ptr); //ポインタをリストの末尾へ移動
        sz  = listbuf + SIZE_LIST - ptr - 1;
      }
      rc = fs.load(fname, ptr, sz); 
      if( rc == SD_ERR_INIT ) {
        err = ERR_SD_NOT_READY;
      } else if (rc == SD_ERR_OPEN_FILE) {
        err = ERR_FILE_OPEN;        
      } else if (rc == SD_ERR_READ_FILE) {
        err = ERR_FILE_READ;
      }
    } else if (fg == 1) {
      // SDカードからのテキスト形式ロード
      if( loadPrgText(fname,newmode)) {
        err = ERR_FILE_READ;
      }
    }
    if (err)
      return 0;  
#endif
  }

  // 行番号・ラベル指定の処理
  if (lineno == 0) {
    clp = listbuf; // 行ポインタをプログラム保存領域の先頭に設定
  } else if (lineno == -1) {
    lp = getlpByLabel(label);
    if (lp == NULL) {
      err = ERR_ULN;
      return 0;
    }
    clp = lp;     // 行ポインタをプログラム保存領域の指定行先頭に設定
  } else {
    // 指定行にジャンプする
    lp = getlp(lineno);   // 分岐先のポインタを取得
    if (lineno != getlineno(lp)) {
      err = ERR_ULN;
      return 0;
    }  
    clp = lp;     // 行ポインタをプログラム保存領域の指定ラベル先頭に設定
  }
  if (!err) {
    if (islrun || (cip >= listbuf && cip < listbuf+SIZE_LIST)) {
       cip = clp+3;
    }
  }
  return 1;
}

// エラーメッセージ出力
// 引数: dlfCmd プログラム実行時 false、コマンド実行時 true
void error(uint8_t flgCmd = false) {
  sc->show_curs(0);
  if (err) { 
    // もしプログラムの実行中なら（cipがリストの中にあり、clpが末尾ではない場合）
    if (cip >= listbuf && cip < listbuf + SIZE_LIST && *clp && !flgCmd) {
      // エラーメッセージを表示
      c_puts(errmsg[err]);       
      c_puts(" in ");
      putnum(getlineno(clp), 0); // 行番号を調べて表示
      newline();

      // リストの該当行を表示
      putnum(getlineno(clp), 0);
      c_puts(" ");
      putlist(clp + 3);          
      newline();
    } else {                   // 指示の実行中なら
      c_puts(errmsg[err]);     // エラーメッセージを表示
      newline();               // 改行
    }
  } 
  c_puts(errmsg[0]);           //「OK」を表示
  newline();                   // 改行
  err = 0;                     // エラー番号をクリア
  sc->show_curs(1);
}

// Get value
int16_t ivalue() {
  int16_t value; // 値

  switch (*cip++) { //中間コードで分岐

  //定数の取得
  case I_NUM:    // 定数
  case I_HEXNUM: // 16進定数
  case I_BINNUM: // 2進数定数
    value = *cip | *(cip + 1) << 8; //定数を取得
    cip += 2;
    break; 

  //+付きの値の取得
  case I_PLUS: //「+」
    value = ivalue(); //値を取得
    break; 

  //負の値の取得
  case I_MINUS: //「-」
    value = 0 - ivalue(); //値を取得して負の値に変換
    break; 

  case I_LNOT: //「!」
    value = !ivalue(); //値を取得してNOT演算
    break; 

  case I_BITREV: // 「~」 ビット反転
    //cip++; //中間コードポインタを次へ進める
    value = ~((uint16_t)ivalue()); //値を取得してNOT演算
    break;
  
  //変数の値の取得
  case I_VAR: //変数
    value = var[*cip++]; //変数番号から変数の値を取得して次を指し示す
    break;

  //括弧の値の取得
  case I_OPEN: //「(」
     cip--;
    value = getparam(); //括弧の値を取得
    break;

  //配列の値の取得
  case I_ARRAY: //配列
    value = getparam(); //括弧の値を取得
    if (!err) {
      if (value >= SIZE_ARRY)
         err = ERR_SOR;         // 添え字が範囲を超えた
      else 
         value = arr[value];    // 配列の値を取得
    }
    break;
  case I_RND: //関数RND
    value = getparam(); //括弧の値を取得
    if (!err) {
      if (value < 0 )
        err = ERR_VALUE;
      else
       value = getrnd(value); //乱数を取得
    }
    break;

  case I_ABS: //関数ABS
    value = getparam(); //括弧の値を取得
    if (value == -32768)
      err = ERR_VOF;
    if (err)
      break;
    if (value < 0) 
      value *= -1; //正負を反転
    break;

  case I_FREE: //関数FREE
   if (checkOpen()||checkClose()) break;
    value = getsize(); //プログラム保存領域の空きを取得
    break;

  case I_INKEY: //関数INKEY
   if (checkOpen()||checkClose()) break;   
    value = iinkey(); // キー入力値の取得
    break;

  case I_VPEEK: value = ivpeek();  break; //関数VPEEK
  case I_GPEEK: value = igpeek();  break; //関数GPEEK(X,Y)
  case I_GINP:  value = iginp();   break; //関数GINP(X,Y,W,H,C)
  case I_MAP:   value = imap();    break; //関数MAP(V,L1,H1,L2,H2)
  case I_ASC:   value = iasc();    break;// 関数ASC(文字列)
  case I_RGB:   value = iRGB();    break;// 関数RGB(r,g,b)

  case I_LEN:  // 関数LEN(変数)
    if (checkOpen()) break;
    if ( *cip == I_VAR)  {
      cip++;
      value = *v2realAddr(var[*cip]);
      cip++;
    } else if ( *cip == I_ARRAY) {
      cip++;
      if (getParam(value, 0, SIZE_ARRY-1, false)) return 0;
      value = *v2realAddr(arr[value]);
    } else if ( *cip == I_STR) {
      cip++;
      value = *cip;
      cip+=*cip+1;
    } else
      err = ERR_SYNTAX;
    checkClose();
    break;
   
  case I_TICK: // 関数TICK()
    if ((*cip == I_OPEN) && (*(cip + 1) == I_CLOSE)) {
      // 引数無し
      value = 0;
      cip+=2;
    } else {
      value = getparam(); // 括弧の値を取得
      if (err)
        break;
    }
    if(value == 0) {
        value = (millis()) & 0x7FFF;            // 0～32767msec(0～32767)
    } else if (value == 1) {
        value = (millis()/1000) & 0x7FFF;       // 0～32767sec(0～32767)
    } else {
      value = 0;                                // 引数が正しくない
      err = ERR_VALUE;
    }
    break;

  case I_PEEK: value = ipeek();   break;     // PEEK()関数
  case I_I2CW:  value = ii2cw();   break;    // I2CW()関数
  case I_I2CR:  value = ii2cr();   break;    // I2CR()関数
  case I_SHIFTIN: value = ishiftIn(); break; // SHIFTIN()関数
  case I_PULSEIN: value = ipulseIn();  break;// PLUSEIN()関数
  
  // 定数
  case I_HIGH:  value = CONST_HIGH; break;
  case I_LOW:   value = CONST_LOW;  break;
  case I_ON:    value = CONST_ON;   break;
  case I_OFF:   value = CONST_OFF;  break;
  case I_LSB:   value = CONST_LSB;  break;
  case I_MSB:   value = CONST_MSB;  break;

  case I_VRAM:  value = V_VRAM_TOP;  break;
  case I_MVAR:  value = V_VAR_TOP;   break;
  case I_MARRAY:value = V_ARRAY_TOP; break; 
  case I_MPRG:  value = V_PRG_TOP;   break;
  case I_MEM:   value = V_MEM_TOP;   break; 
  case I_MFNT:  value = V_FNT_TOP;   break;
  case I_GRAM:  value = V_GRAM_TOP;  break;
  
  case I_DIN: // DIN(ピン番号)
    if (checkOpen()) break;
    if (getParam(value,0,I_PC15 - I_PA0, false)) break;
    if (checkClose()) break;
    if ( !IsIO_PIN(value) ) {
      err = ERR_GPIO;
      break;
    }
    value = digitalRead(value);  // 入力値取得
    break;

  case I_ANA: // ANA(ピン番号)
    if (checkOpen()) break;
    if (getParam(value,0,I_PC15 - I_PA0, false)) break;
    if (checkClose()) break;
    value = analogRead(value);    // 入力値取得
    break;

  case I_EEPREAD: // EEPREAD(アドレス)の場合
    value = getparam(); 
    if (err)  break;
    value = ieepread(value);   // 入力値取得
    break;

  case I_SREAD: // SREAD() シリアルデータ1バイト受信
    if (checkOpen()||checkClose()) break;   
    value =sc->Serial_read();
    break; //ここで打ち切る
  
  case I_SREADY:// SREADY() シリアルデータデータチェック
    if (checkOpen()||checkClose()) break;   
    value =sc->Serial_available();
    break; //ここで打ち切る

  // 画面サイズ定数の参照
  case I_CW: value = sc->getWidth()   ; break;
  case I_CH: value = sc->getHeight()  ; break;
#if USE_NTSC == 1 || USE_TFT == 1 || USE_OLED == 1
  case I_GW: value = scmode||USE_TFT||USE_OLED ? sc2.getGWidth():0  ; break;
  case I_GH: value = scmode||USE_TFT||USE_OLED ? sc2.getGHeight():0 ; break;
#else
  case I_GW: value = 0 ; break;
  case I_GH: value = 0 ; break;
#endif
  // カーソル・スクロール等の方向
  case I_UP:    value = 0   ; break;
  case I_DOWN:  value = 1   ; break;
  case I_RIGHT: value = 2   ; break;
  case I_LEFT:  value = 3   ; break;

  default: //以上のいずれにも該当しなかった場合
    // 定数ピン番号
    cip--;
    if (*cip >= I_PA0 && *cip <= I_PC15) {
      value = *cip - I_PA0; 
      cip++;
      return value;
    // 定数GPIOモード
    } else  if (*cip >= I_OUTPUT_OPEN_DRAIN && *cip <= I_PWM) {
      value = pinType[*cip - I_OUTPUT_OPEN_DRAIN]; 
      cip++;
      return value;  
    }
    err = ERR_SYNTAX; //エラー番号をセット
    break; //ここで打ち切る
  }
  return value; //取得した値を持ち帰る
}

// multiply or divide calculation
short imul() {
  short value, tmp; //値と演算値

  value = ivalue(); //値を取得
  if (err) 
    return -1;

  while (1) //無限に繰り返す
  switch(*cip){ //中間コードで分岐

  case I_MUL: //掛け算の場合
    cip++; //中間コードポインタを次へ進める
    tmp = ivalue(); //演算値を取得
    value *= tmp; //掛け算を実行
    break;

  case I_DIV: //割り算の場合
    cip++; //中間コードポインタを次へ進める
    tmp = ivalue(); //演算値を取得
    if (tmp == 0) { //もし演算値が0なら
      err = ERR_DIVBY0; //エラー番号をセット
      return -1;
    }
    value /= tmp; //割り算を実行
    break; 
    
  case I_DIVR: //剰余の場合
    cip++; //中間コードポインタを次へ進める
    tmp = ivalue(); //演算値を取得
    if (tmp == 0) { //もし演算値が0なら
      err = ERR_DIVBY0; //エラー番号をセット
      return -1; //終了
    }
    value %= tmp; //割り算を実行
    break; 

  case I_LSHIFT: // シフト演算 "<<" の場合
    cip++; //中間コードポインタを次へ進める
    tmp = ivalue(); //演算値を取得
    value =((uint16_t)value)<<tmp;
    break;

  case I_RSHIFT: // シフト演算 ">>" の場合
    cip++; //中間コードポインタを次へ進める
    tmp = ivalue(); //演算値を取得
    value =((uint16_t)value)>>tmp;
    break; 

   case I_AND:  // 算術積(ビット演算)
    cip++; //中間コードポインタを次へ進める
    tmp = ivalue(); //演算値を取得
    value =((uint16_t)value)&((uint16_t)tmp);
    break; //ここで打ち切る

   case I_OR:   //算術和(ビット演算)
    cip++; //中間コードポインタを次へ進める
    tmp = ivalue(); //演算値を取得
    value =((uint16_t)value)|((uint16_t)tmp);
    break; 

   case I_XOR: //非排他OR(ビット演算)
    cip++; //中間コードポインタを次へ進める
    tmp = ivalue(); //演算値を取得
    value =((uint16_t)value)^((uint16_t)tmp);
   
  default: //以上のいずれにも該当しなかった場合
    return value; //値を持ち帰る
  } //中間コードで分岐の末尾
}

// add or subtract calculation
short iplus() {
  short value, tmp; //値と演算値
  value = imul(); //値を取得
  if (err) 
    return -1;

  while (1) 
  switch(*cip){
  case I_PLUS: //足し算の場合
    cip++; //中間コードポインタを次へ進める
    tmp = imul(); //演算値を取得
    value += tmp; //足し算を実行
    break;

  case I_MINUS: //引き算の場合
    cip++; //中間コードポインタを次へ進める
    tmp = imul(); //演算値を取得
    value -= tmp; //引き算を実行
    break;

  default: //以上のいずれにも該当しなかった場合
    return value; //値を持ち帰る
  } //中間コードで分岐の末尾
}

// The parser
short iexp() {
  short value, tmp; //値と演算値

  value = iplus(); //値を取得
  if (err) //もしエラーが生じたら
    return -1; //終了

  // conditional expression 
  while (1) //無限に繰り返す
  switch(*cip++){ //中間コードで分岐

  case I_EQ: //「=」の場合
    tmp = iplus(); //演算値を取得
    value = (value == tmp); //真偽を判定
    break; 
  case I_NEQ:   //「!=」の場合
  case I_NEQ2:  //「<>」の場合
  case I_SHARP: //「#」の場合
    tmp = iplus(); //演算値を取得
    value = (value != tmp); //真偽を判定
    break;
  case I_LT: //「<」の場合
    tmp = iplus(); //演算値を取得
    value = (value < tmp); //真偽を判定
    break;
  case I_LTE: //「<=」の場合
    tmp = iplus(); //演算値を取得
    value = (value <= tmp); //真偽を判定
    break;
  case I_GT: //「>」の場合
    tmp = iplus(); //演算値を取得
    value = (value > tmp); //真偽を判定
    break;
  case I_GTE: //「>=」の場合
    tmp = iplus(); //演算値を取得
    value = (value >= tmp); //真偽を判定
    break;
 case I_LAND: // AND (論理積)
    tmp = iplus(); //演算値を取得
    value = (value && tmp); //真偽を判定
    break;
 case I_LOR: // OR (論理和)
    tmp = iplus(); //演算値を取得
    value = (value || tmp); //真偽を判定
    break; 
  default: //以上のいずれにも該当しなかった場合
    cip--;
    return value; //値を持ち帰る
  } //中間コードで分岐の末尾
}

// 左上の行番号の取得
int16_t getTopLineNum() {
  uint8_t* ptr = sc->getScreen();
  uint32_t n = 0;
  int rc = -1;  
  while (isDigit(*ptr)) {
    n *= 10;
    n+= *ptr-'0';
    if (n>32767) {
      n = 0;
      break;
    }
    ptr++;
  }
  if (!n)
    rc = -1;
  else
    rc = n;
  return rc;
}

// 左下の行番号の取得
int16_t getBottomLineNum() {
  uint8_t* ptr = sc->getScreen()+sc->getWidth()*(sc->getHeight()-1);
  uint32_t n = 0;
  int rc = -1;  
  while (isDigit(*ptr)) {
    n *= 10;
    n+= *ptr-'0';
    if (n>32767) {
      n = 0;
      break;
    }
    ptr++;
  }
  if (!n)
    rc = -1;
  else
    rc = n;
  return rc;  
}

// 指定した行の前の行番号を取得する
int16_t getPrevLineNo(int16_t lineno) {
  uint8_t* lp, *prv_lp = NULL;
  int16_t rc = -1;
  for ( lp = listbuf; *lp && (getlineno(lp) < lineno); lp += *lp) {
    prv_lp = lp;
  }
  if (prv_lp)
    rc = getlineno(prv_lp);
  return rc;
}

// 指定した行の次の行番号を取得する
int16_t getNextLineNo(int16_t lineno) {
  uint8_t* lp;
  int16_t rc = -1;
  
  lp = getlp(lineno); 
  if (lineno == getlineno(lp)) { 
    // 次の行に移動
    lp+=*lp;
    rc = getlineno(lp);
  }
  return rc;
}

// 指定した行のプログラムテキストを取得する
char* getLineStr(int16_t lineno) {
    uint8_t* lp = getlp(lineno);
    if (lineno != getlineno(lp)) 
      return NULL;
    
    // 行バッファへの指定行テキストの出力
    cleartbuf();
    putnum(lineno, 0,3); // 行番号を表示
    c_putch(' ',3);    // 空白を入れる
    putlist(lp+3,3);   // 行番号より後ろを文字列に変換して表示        
    c_putch(0,3);      // \0を入れる
    return tbuf;
}

// システム情報の表示
void iinfo() {
  char top = 't';
  uint32_t adr = (uint32_t)&top;
  uint8_t* tmp = (uint8_t*)malloc(1);
  uint32_t hadr = (uint32_t)tmp;
  free(tmp);

  // スタック領域先頭アドレスの表示
  c_puts("Stack Top:");
  putHexnum((int16_t)(adr>>16),4);putHexnum((int16_t)(adr&0xffff),4);
  newline();
  
  // ヒープ領域先頭アドレスの表示
  c_puts("Heap Top :");
  putHexnum((int16_t)(hadr>>16),4);putHexnum((int16_t)(hadr&0xffff),4);
  newline();

  // SRAM未使用領域の表示
  c_puts("SRAM Free:");
  putnum((int16_t)(adr-hadr),0);
  newline();
#if 0
  // スクリーン関連
  c_puts("scmode:");putnum(scmode,1);newline();
  c_puts("scSizeMode:");putnum(scSizeMode,1);newline();
  c_puts("serialMode:");putnum(serialMode,1);newline();
#endif  
}

// ラベル
void ilabel() {
   cip+= *cip+1;   
}

// GOTO
void igoto() {
  uint8_t* lp;       // 飛び先行ポインタ
  int16_t lineno;    // 行番号

  if (*cip == I_STR) { 
    // ラベル参照による分岐先取得 
    lp = getlpByLabel(cip);                   
    if (lp == NULL) {
      err = ERR_ULN;                          // エラー番号をセット
      return;
    }  
  } else {
    // 引数の行番号取得
    lineno = iexp();                          
    if (err)  return;         
    lp = getlp(lineno);                       // 分岐先のポインタを取得
    if (lineno != getlineno(lp)) {            // もし分岐先が存在しなければ
      err = ERR_ULN;                          // エラー番号をセット
      return; 
    }
  }
  clp = lp;        // 行ポインタを分岐先へ更新
  cip = clp + 3;   // 中間コードポインタを先頭の中間コードに更新  
}

// GOSUB
void igosub() {
  uint8_t* lp;       // 飛び先行ポインタ
  int16_t lineno;    // 行番号

  if (*cip == I_STR) {
    // ラベル参照による分岐先取得 
    lp = getlpByLabel(cip);                   
    if (lp == NULL) {
      err = ERR_ULN;  // エラー番号をセット
      return; 
    }  
  } else {
    // 引数の行番号取得
    lineno = iexp();
    if (err)
      return;  

    lp = getlp(lineno);                       // 分岐先のポインタを取得
    if (lineno != getlineno(lp)) {            // もし分岐先が存在しなければ
      err = ERR_ULN;                          // エラー番号をセット
      return; 
    }
  }
  
  //ポインタを退避
  if (gstki > SIZE_GSTK - 2) {              // もしGOSUBスタックがいっぱいなら
    err = ERR_GSTKOF;                       // エラー番号をセット
      return; 
  }
  gstk[gstki++] = clp;                      // 行ポインタを退避
  gstk[gstki++] = cip;                      // 中間コードポインタを退避

  clp = lp;                                 // 行ポインタを分岐先へ更新
  cip = clp + 3;                            // 中間コードポインタを先頭の中間コードに更新
}

// RETURN
void ireturn() {
  if (gstki < 2) {    // もしGOSUBスタックが空なら
    err = ERR_GSTKUF; // エラー番号をセット
    return; 
  }
  cip = gstk[--gstki]; //行ポインタを復帰
  clp = gstk[--gstki]; //中間コードポインタを復帰
  return;  
}

// FOR
void ifor() {
  short index, vto, vstep; // FOR文の変数番号、終了値、増分
  
  // 変数名を取得して開始値を代入（例I=1）
  if (*cip++ != I_VAR) { // もし変数がなかったら
    err = ERR_FORWOV;    // エラー番号をセット
    return;
  }
  index = *cip; // 変数名を取得
  ivar();       // 代入文を実行
  if (err)      // もしエラーが生じたら
    return;

  // 終了値を取得（例TO 5）
  if (*cip == I_TO) { // もしTOだったら
    cip++;             // 中間コードポインタを次へ進める
    vto = iexp();      // 終了値を取得
  } else {             // TOではなかったら
    err = ERR_FORWOTO; //エラー番号をセット
    return;
  }

  // 増分を取得（例STEP 1）
  if (*cip == I_STEP) { // もしSTEPだったら
    cip++;              // 中間コードポインタを次へ進める
    vstep = iexp();     // 増分を取得
  } else                // STEPではなかったら
    vstep = 1;          // 増分を1に設定

  // もし変数がオーバーフローする見込みなら
  if (((vstep < 0) && (-32767 - vstep > vto)) ||
    ((vstep > 0) && (32767 - vstep < vto))){
    err = ERR_VOF; //エラー番号をセット
    return;
  }

  // 繰り返し条件を退避
  if (lstki > SIZE_LSTK - 5) { // もしFORスタックがいっぱいなら
    err = ERR_LSTKOF;          // エラー番号をセット
    return;
  }
  lstk[lstki++] = clp; // 行ポインタを退避
  lstk[lstki++] = cip; // 中間コードポインタを退避

  // FORスタックに終了値、増分、変数名を退避
  // Special thanks hardyboy
  lstk[lstki++] = (unsigned char*)(uintptr_t)vto;
  lstk[lstki++] = (unsigned char*)(uintptr_t)vstep;
  lstk[lstki++] = (unsigned char*)(uintptr_t)index;  
}

// NEXT
void inext() {
  short index, vto, vstep; // FOR文の変数番号、終了値、増分

  if (lstki < 5) {    // もしFORスタックが空なら
    err = ERR_LSTKUF; // エラー番号をセット
    return;
  }

  // 変数名を復帰
  index = (short)(uintptr_t)lstk[lstki - 1]; // 変数名を復帰
  if (*cip++ != I_VAR) {                     // もしNEXTの後ろに変数がなかったら
    err = ERR_NEXTWOV;                       // エラー番号をセット
    return;
  }
  if (*cip++ != index) { // もし復帰した変数名と一致しなかったら
    err = ERR_NEXTUM;    // エラー番号をセット
    return;
  }

  vstep = (short)(uintptr_t)lstk[lstki - 2]; // 増分を復帰
  var[index] += vstep;                       // 変数の値を最新の開始値に更新
  vto = (short)(uintptr_t)lstk[lstki - 3];   // 終了値を復帰

  // もし変数の値が終了値を超えていたら
  if (((vstep < 0) && (var[index] < vto)) ||
    ((vstep > 0) && (var[index] > vto))) {
    lstki -= 5;  // FORスタックを1ネスト分戻す
    return;
  }

  // 開始値が終了値を超えていなかった場合
  cip = lstk[lstki - 4]; //行ポインタを復帰
  clp = lstk[lstki - 5]; //中間コードポインタを復帰
}

// IF
void iif() {
  int16_t condition;    // IF文の条件値
  uint8_t* newip;       // ELSE文以降の処理対象ポインタ

  condition = iexp(); // 真偽を取得
  if (err) {          // もしエラーが生じたら
    err = ERR_IFWOC;  // エラー番号をセット
    return;
  }
  if (condition) {    // もし真なら
    return;
  } else { 
    // 偽の場合の処理
    // ELSEがあるかチェックする
    // もしELSEより先にIFが見つかったらELSE無しとする        
    // ELSE文が無い場合の処理はREMと同じ
    newip = getELSEptr(cip);
    if (newip != NULL) {
      cip = newip;
      return;
    }
    while (*cip != I_EOL) // I_EOLに達するまで繰り返す
    cip++;                // 中間コードポインタを次へ進める        
  }
}

// スキップ
void iskip() {
  while (*cip != I_EOL) // I_EOLに達するまで繰り返す
    cip++;              // 中間コードポインタを次へ進める  
}

// END
void iend() {
  while (*clp)    // 行の終端まで繰り返す
    clp += *clp;  // 行ポインタを次へ進める  
}

// 中間コードの実行
// 戻り値      : 次のプログラム実行位置(行の先頭)
unsigned char* iexe() {
  uint8_t c;               // 入力キー
  err = 0;

  while (*cip != I_EOL) { //行末まで繰り返す
  
  //強制的な中断の判定
  c = c_kbhit();
  if (c) { // もし未読文字があったら
      if (c == SC_KEY_CTRL_C || c==27 ) { // 読み込んでもし[ESC],［CTRL_C］キーだったら
        err = ERR_CTR_C;                  // エラー番号をセット
        prevPressKey = 0;
        break;
      } else {
        prevPressKey = c;
      }
    }

    //中間コードを実行
    switch (*cip++) {
    case I_STR:    ilabel();          break;  // 文字列の場合(ラベル)
    case I_GOTO:   igoto();           break;  // GOTOの場合
    case I_GOSUB:  igosub();          break;  // GOSUBの場合
    case I_RETURN: ireturn();         break;  // RETURNの場合
    case I_FOR:    ifor();            break;  // FORの場合
    case I_NEXT:   inext();           break;  // NEXTの場合
    case I_IF:     iif();             break;  // IFの場合
    case I_ELSE:   iskip();           break;  // 単独のELSEの場合     
    case I_SQUOT:  iskip();           break;  // 'の場合
    case I_REM:    iskip();           break;  // REMの場合
    case I_END:    iend();            break;  // ENDの場合
    case I_CLS:    icls();            break;  // CLS
    case I_WAIT:   iwait();           break;  // WAIT
    case I_LOCATE: ilocate();         break;  // LOCATE
    case I_COLOR:  icolor();          break;  // COLOR
    case I_ATTR:   iattr();           break;  // ATTR
    case I_VAR:    ivar();            break;  // 変数（LETを省略した代入文）
    case I_ARRAY:  iarray();          break;  // 配列（LETを省略した代入文）
    case I_LET:    ilet();            break;  // LET
    case I_QUEST:  iprint();          break;  // PRINT
    case I_PRINT:   iprint();         break;  // PRINT
    case I_INPUT:   iinput();         break;  // INPUT
    case I_GPIO:    igpio();          break;  // GPIO
    case I_DOUT:    idwrite();        break;  // OUT    
    case I_POUT:    ipwm();           break;  // PWM   
    case I_SHIFTOUT: ishiftOut();     break;  // ShiftOut
    case I_POKE:    ipoke();          break;  // POKEコマンド
    case I_SETDATE: isetDate();       break;  // SETDATEコマンド    
    case I_GETDATE: igetDate();       break;  // GETDATEコマンド
    case I_GETTIME: igetTime();       break;  // GETDATEコマンド
    case I_DATE:    idate();          break;  // DATEコマンド
    case I_CLT:     iclt();           break;  // CLTコマンド
    case I_REFLESH:   sc->refresh();   break;  // REFLESHコマンド 画面再表示
    case I_EEPFORMAT: ieepformat();   break;  // EPPFORMAT EEPROM(エミュレーション)の初期化
    case I_EEPWRITE:  ieepwrite();    break;  // EEPWRITE コマンド
    case I_PSET:      ipset();        break;  // PSETコマンド ドットの描画
    case I_LINE:      iline();        break;  // LINEコマンド 直線の描画
    case I_CIRCLE:    icircle();      break;  // CIRCLEコマンド 円の描画
    case I_RECT:      irect();        break;  // RECT四角の表示
    case I_BITMAP:    ibitmap();      break;  // BITMAPビットマップの描画
    case I_CSCROLL:   icscroll();     break;  // CSCROLLキャラクタスクロール
    case I_GSCROLL:   igscroll();     break;  // GSCROLLグラフィックスクロール    
    case I_SWRITE:    iswrite();      break;  // シリアル1バイト出力
    case I_SPRINT:    iprint(1);      break;  // SPRINT
    case I_GPRINT:    igprint();      break;  // GPRINT
    case I_SOPEN:     isopen();       break;  // SOPEN
    case I_SCLOSE:    isclose();      break;  // SCLOSE
    case I_SMODE:     ismode();       break;  // SMODE 
    case I_TONE:      itone();        break;  // TONE
    case I_NOTONE:    inotone();      break;  // NOTONE
    case I_CLV:       inew(2);        break;  // CLV 変数領域消去
    case I_INFO:      iinfo();        break;  // システム情報の表示(デバッグ用)
    case I_LDBMP:      ildbmp();      break;  // LDBMP ビットマップファイルのロード
    case I_MKDIR:      imkdir();      break;  // MKDIR ディレクトリの作成
    case I_RMDIR:      irmdir();      break;  // RMDIR ディレクトリの削除
    //case I_RENAME:   irename();     break;  // RENAME ファイル名の変更
    case I_REMOVE:     iremove();     break;  // REMOVE ファイル削除
    case I_BSAVE:      ibsave();      break;  // BSAVE メモリ領域の保存
    case I_BLOAD:      ibload();      break;  // BLOAD メモリ領域へのロード
    case I_DWBMP:      idwbmp();      break;  // DWBMP ビットマップファイルの直接描画
    case I_CAT:        icat();        break;  // CAT テキストファイル表示
    case I_LRUN:       ilrun();       break;   
    case I_LIST:       sc->show_curs(0); ilist();  sc->show_curs(1);break;  // LIST
    case I_EXPORT:     iexport();     break;  // EXPORTコマンド
    case I_FILES:      ifiles();      break;
    case I_CONFIG:     iconfig();     break;
    case I_SAVECONFIG: isaveconfig(); break;
    case I_ERASE:      ierase();      break; 
    case I_NEW:        inew();        break;   // NEW
    case I_LOAD:       ilrun();       break;
    case I_SAVE:       isave();       break;
    case I_WIDTH:      iwidth();      break;
    case I_SCREEN:     iscreen();     break;
    case I_CONSOLE:    iconsole();    break;

    case I_RUN:    // RUN
    case I_RENUM:  // RENUM
    case I_DELETE: // DELETE
      err = ERR_COM; //エラー番号をセット
      return NULL; //終了

    case I_COLON: // 中間コードが「:」の場合
      break; 

    default:
     cip--;
     if (*cip >= I_PA0 && *cip <= I_PC15) {
       igpio();
       break; 
     } 
    
     // 以上のいずれにも該当しない場合
     err = ERR_SYNTAX; //エラー番号をセット
     break;
    }  //中間コードで分岐の末尾
  
    if (err)
      return NULL;
  }
  return clp + *clp;
}

//Command precessor
uint8_t icom() {
  uint8_t rc = 1;
  cip = ibuf;          // 中間コードポインタを中間コードバッファの先頭に設定

  switch (*cip++) {    // 中間コードポインタが指し示す中間コードによって分岐
  case I_LOAD:  ilrun(); break; 
  case I_LRUN:  if(ilrun()) {  sc->show_curs(0); irun(clp);  sc->show_curs(1);  }  break; 
  case I_RUN:   sc->show_curs(0); irun();  sc->show_curs(1);   break; // RUN命令
  case I_RENUM: irenum(); break; // I_RENUMの場合
  case I_DELETE:idelete();  break;
  case I_CLS:icls();
  case I_REM:
  case I_SQUOT:    
  case I_OK:    rc = 0;     break; // I_OKの場合
  default:    // どれにも該当しない場合
    cip--;
    sc->show_curs(0);
    iexe();           // 中間コードを実行
    sc->show_curs(1);
    break;
  }
  return rc;
}

/*
  TOYOSHIKI Tiny BASIC
  The BASIC entry point
*/

void basic() {
  unsigned char len; // 中間コードの長さ
  uint8_t rc;        // 関数戻り値受け取り用

  // SWD・JTAGの利用禁止
  disableDebugPorts();
  pinMode(PB4, INPUT); // 禁止後,JTRSTがHIGHのままのため、入力モードに変更

  // 起動時のモード指定チェック
#if FLG_CHK_BOOT1 == 1
  // BOOT1がHIGHの場合、シリアルコンソールモードで起動する
  // さらに、SWCLKがLOWならUSBシリアル、HIGHならGPIOシリアルポートえを使う
  if (digitalRead(PB2)) {
      scmode = 0;
     if (digitalRead(PA14)) {
        serialMode = 1;
     }
  }
#endif

  // 環境設定のロード
  FlashMan.loadConfig(CONFIG);
  
  // プログラム領域の初期化
  inew();              

// ワークエリアの獲得
#if USE_NTSC == 1
  workarea = (uint8_t*)malloc(7048); // SCREEN0で128x50まで
#elif USE_OLED == 1
  workarea = (uint8_t*)malloc(5760); // SCREEN0で128x45まで
#else
  workarea = (uint8_t*)malloc(5760); // SCREEN0で128x45まで
#endif

  // デバイススクリーンの初期化設定
#if USE_SCREEN_MODE== 0 // シリアルコンソール利用
  sc = &sc1;
  ((tTermscreen*)sc)->init(TERM_W,TERM_H,SIZE_LINE, workarea); // スクリーン初期設定
#elif USE_NTSC == 1 || USE_TFT == 1 || USE_OLED == 1 // デバイスコンソール利用
  sc = &sc2;
  scSizeMode = DEV_SCMODE;
  scrt = DEV_RTMODE;
  ((tGraphicScreen*)sc)->init(ttbasic_font, SIZE_LINE, CONFIG.KEYBOARD, workarea, scSizeMode, 
                              DEV_RTMODE, CONFIG.NTSC_HPOS, CONFIG.NTSC_VPOS, DEV_IFMODE);
#endif
  sc->Serial_mode(serialMode, defbaud); // デバイススクリーンのシリアル出力の設定
  prv_scSizeMode = scSizeMode;
  prv_scrt = scrt;
#if USE_SCREEN_MODE== 1
 // 起動時の設定がコンソール指定の場合、切り替える
 if (scmode == 0) {
   iconsole(true, CON_MODE_SERIAL);
 }
#endif

 // PWM単音出力初期化
 dev_toneInit();

#if USE_SD_CARD == 1
  // SDカード利用
  fs.init(); // この処理ではGPIOの操作なし
#endif

  I2C_WIRE.begin();  // I2C利用開始
  
  char* textline;    // 入力行

  // 起動メッセージ  
  icls();
  sc->show_curs(0);                // 高速描画のためのカーソル非表示指定
  c_puts("TOYOSHIKI TINY BASIC "); // 「TOYOSHIKI TINY BASIC」を表示
  newline();                       // 改行
  c_puts(STR_EDITION);             // 版を区別する文字列「EDITION」を表示
  c_puts(" " STR_VARSION);         // バージョンの表示
  newline();                       // 改行
  err = 0;
  error();                         // 「OK」またはエラーメッセージを表示してエラー番号をクリア
  sc->show_curs(1);                // カーソル表示
    
  // プログラム自動起動
  if (CONFIG.STARTPRG >=0  && loadPrg(CONFIG.STARTPRG) == 0) {
    // ロードに成功したら、プログラムを実行する
    sc->show_curs(0);        // カーソル非表示
    irun();                  // RUN命令を実行

    // 起動したプログラムの情報を表示
    newline();               // 改行
    c_puts("Autorun No."); 
    putnum(CONFIG.STARTPRG,0);c_puts(" done.");
    newline();
    err = 0; 
  }
  
  // 端末から1行を入力して実行（メインループ）
  sc->show_curs(1);
  while (1) { //無限ループ
    rc = sc->edit();  // エディタ入力
    if (rc) {
      textline = (char*)sc->getText(); // スクリーンバッファからテキスト取得
      if (!strlen(textline) ) {
        // 改行のみ
        newline();
        continue;
      }
      
      if (strlen(textline) >= SIZE_LINE) {
        // 入力文字が有効文字長を超えている
         err = ERR_LONG;
         newline();
         error();
         continue;  
      }
      // 行バッファに格納し、改行する
      strcpy(lbuf, textline);
      tlimR((char*)lbuf); //文末の余分空白文字の削除
      newline();
    } else {
      // 入力なし
      continue;
    }
    
    // 1行の文字列を中間コードの並びに変換
    len = toktoi();      // 文字列を中間コードに変換して長さを取得
    if (err) {           // もしエラーが発生したら
      error(true);       // エラーメッセージを表示してエラー番号をクリア
      continue;          // 繰り返しの先頭へ戻ってやり直し
    }

    //中間コードの並びがプログラムと判断される場合
    if (*ibuf == I_NUM) { // もし中間コードバッファの先頭が行番号なら
      *ibuf = len;        // 中間コードバッファの先頭を長さに書き換える
      inslist();          // 中間コードの1行をリストへ挿入
      if (err)            // もしエラーが発生したら
        error();          // エラーメッセージを表示してエラー番号をクリア
      continue;           // 繰り返しの先頭へ戻ってやり直し
    }

    // 中間コードの並びが命令と判断される場合
    if (icom())           // 実行する
        error(false);     // エラーメッセージを表示してエラー番号をクリア
  } // 無限ループの末尾
}

