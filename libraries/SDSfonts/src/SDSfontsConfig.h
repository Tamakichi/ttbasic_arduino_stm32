//
// SJIS版フォント利用ライブラリ クラス定義 SDSfontsConfig.h 
// 作成 2018/10/27 by たま吉さん
//

#ifndef ___SDSfontsConfig_h___
#define ___SDSfontsConfig_h___

// SDカード用ライブラリの選択
#define SDSFONTS_USE_SDFAT   0 // 0:SDライブラリ利用, 1:SdFatライブラリ利用

// Sdfat利用時 SPI速度
#define SDSFONTS_SPI_SPEED SD_SCK_MHZ(18)

#endif
