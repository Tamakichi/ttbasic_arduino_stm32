//
// file: sound.h
// �T�E���h�Đ�(Timer4 PWM�[�q PB9�𗘗p�j
//
// �쐬�� 2017/10/25 by ���܋g����
//
  

#ifndef __sound_h__
#define __sound_h__

#include <Arduino.h>

void dev_toneInit() ;                              // PWM�P���o�͏����ݒ�
void dev_tone(uint16_t freq, uint16_t duration) ;  // ���̒�~
void dev_notone() ;                                // ���̒�~
 
#endif
  