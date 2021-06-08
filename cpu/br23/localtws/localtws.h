#ifndef __LOCALTWS_H_
#define __LOCALTWS_H_

#include "application/audio_localtws.h"
#include "media/localtws_decoder.h"

#define LOCALTWS_ENC_FLAG_STREAM		BIT(0)	// ����Դ��������


// localtws����Ƿ�ʹ��
int localtws_check_enable(void);

// localtws�����¼�����
int localtws_bt_event_deal(struct bt_event *evt);

// ��localtws����
int localtws_enc_api_open(struct audio_fmt *pfmt, u32 flag);
// �ر�localtws����
void localtws_enc_api_close(void);
// localtws����д��
int localtws_enc_api_write(s16 *data, int len);

// localtws���õȴ�a2dp״̬
void localtws_set_wait_a2dp_start(u8 flag);

// localtws��������豸�������ã�
void localtws_start(struct audio_fmt *pfmt);
// localtwsֹͣ����豸�������ã�
void localtws_stop(void);

// ��localtws����
int localtws_dec_open(u32 value);
// �ر�localtws����
int localtws_dec_close(u8 drop_frame_start);
// localtws�Ѿ���
u8 localtws_dec_is_open(void);
// localtws���뼤��
void localtws_dec_resume(void);
// localtws��������
int localtws_media_dat_abandon(void);
// localtws��ͣ
void localtws_dec_pause(void);
// localtws�Ѿ���ʼ����
int localtws_dec_out_is_start(void);

// localtws��ͣ����
void localtws_decoder_pause(u8 pause);


#endif /*__LOCALTWS_H_*/

