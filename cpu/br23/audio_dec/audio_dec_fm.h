
#ifndef _AUDIO_DEC_FM_H_
#define _AUDIO_DEC_FM_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "media/audio_decoder.h"
#include "mic_effect.h"

// fm�������
void fm_sample_output_handler(s16 *data, int len);
// fm�����ͷ�
void fm_dec_relaese();

// fm���뿪ʼ
int fm_dec_start();
// ��fm����
int fm_dec_open(u8 source, u32 sample_rate);
// �ر�fm����
void fm_dec_close(void);
// fm�������¿�ʼ
int fm_dec_restart(int magic);
// ����fm�������¿�ʼ����
int fm_dec_push_restart(void);
// ��ͣ/���� fm����mix ch���
void fm_dec_pause_out(u8 pause);

/***********************inein pcm enc******************************/
// fm¼��ֹͣ
void fm_pcm_enc_stop(void);
// fm¼����ʼ
int fm_pcm_enc_start(void);
// ���fm�Ƿ���¼��
bool fm_pcm_enc_check();

#endif
