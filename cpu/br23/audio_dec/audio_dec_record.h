#ifndef _AUDIO_DEC_RECORD_H_
#define _AUDIO_DEC_RECORD_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "media/audio_decoder.h"

// ¼���ļ�����
int record_file_play(void);
// ָ��·������¼���ļ�
int record_file_play_by_path(char *path);
// �ر�¼���ļ�����
void record_file_close(void);
// ��ȡ¼���������¼�
int record_file_get_total_time(void);
// ��ȡ¼�����ŵ�ǰʱ��
int record_file_dec_get_cur_time(void);

#endif /*_AUDIO_DEC_RECORD_H_*/

