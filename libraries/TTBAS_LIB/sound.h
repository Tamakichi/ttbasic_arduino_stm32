//
// file: sound.h
// サウンド再生(Timer4 PWM端子 PB9を利用）
//
// 作成日 2017/10/25 by たま吉さん
// 修正日 2018/09/12 by たま吉さん,音量対応
//
  

#ifndef __sound_h__
#define __sound_h__

#include <Arduino.h>

// PWM単音出力初期設定
void dev_toneInit() ;                              

// 音の再生
void dev_tone(uint16_t freq, uint16_t duration, uint16_t vol = 15) ;  

// 音の停止
void dev_notone() ;                                
 
#endif
  