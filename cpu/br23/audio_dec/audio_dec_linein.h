
#ifndef _AUDIO_DEC_LINEIN_H_
#define _AUDIO_DEC_LINEIN_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "media/audio_decoder.h"

// linein�����ͷ�
void linein_dec_relaese();
// linein���뿪ʼ
int linein_dec_start();

// ��linein����
int linein_dec_open(u8 source, u32 sample_rate);
// �ر�linein����
void linein_dec_close(void);
// linein�������¿�ʼ
int linein_dec_restart(int magic);
// ����linein�������¿�ʼ����
int linein_dec_push_restart(void);

/***********************linein pcm enc******************************/
// linein¼��ֹͣ
void linein_pcm_enc_stop(void);
// linein¼����ʼ
int linein_pcm_enc_start(void);
// ���linein�Ƿ���¼��
bool linein_pcm_enc_check();

#endif
