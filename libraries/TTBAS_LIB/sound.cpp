//
// file: sound.cpp
// サウンド再生(Timer4 PWM端子 PB9を利用）
//
// 作成日 2017/08/25 by たま吉さん (tvutil.cpp からの移行)
// 修正日 2018/09/12 by たま吉さん, 音量対応
//

#include "sound.h"
const int pwmOutPin = PB9;      // tone用 PWM出力ピン

//
// 音の停止
// 引数
// pin     : PWM出力ピン (現状はPB9固定)
//
void dev_notone() {
    Timer4.pause();
  Timer4.setCount(0xffff);
}

//
// PWM単音出力初期設定
//
void dev_toneInit() {
  pinMode(pwmOutPin, PWM);
  dev_notone();
}

//
// 音出し
// 引数
//  pin     : PWM出力ピン (現状はPB9固定)
//  freq    : 出力周波数 (Hz) 15～ 50000
//  duration: 出力時間(msec)
//  vol     : 音量 (0:無音 ～ 15)
//
void dev_tone(uint16_t freq, uint16_t duration, uint16_t vol) {
  if ( (freq < 15 || freq > 50000) || (vol == 0 || vol > 15) ) {
    dev_notone();
  } else {
    uint32_t f =1000000/(uint16_t)freq;
    Timer4.setPrescaleFactor(F_CPU/1000000L);  // 周波数を1MHzに設定
  	Timer4.setOverflow(f);
    Timer4.refresh();
    Timer4.resume(); 
  	uint32_t v;
  	if (vol == 15) {
  	  v = f/2;
  	} else {
  	  v = 1L<<((uint32_t)vol);
  	  if( v > f/2 )
  		v = f/2;
  	}
  	//    pwmWrite(pwmOutPin, f/2);
    pwmWrite(pwmOutPin, v);
    if (duration) {
      delay(duration);
      Timer4.pause(); 
      Timer4.setCount(0xffff);
    }
  }
}
