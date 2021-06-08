
#ifndef _AUDIO_DEC_BT_H_
#define _AUDIO_DEC_BT_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "media/audio_decoder.h"

// a2dp����esco���ڲ���
u8 bt_audio_is_running(void);
// a2dp���ڲ���
u8 bt_media_is_running(void);
// esco���ڲ���
u8 bt_phone_dec_is_running();;

// ��a2dp����
int a2dp_dec_open(int media_type);
// �ر�a2dp����
int a2dp_dec_close();

// ��esco����
int esco_dec_open(void *, u8);
// �ر�esco����
void esco_dec_close();

// a2dp������ֹ
void __a2dp_drop_frame(void *p);

#endif

