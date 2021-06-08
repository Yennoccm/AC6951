#ifndef _AUDIO_DEC_MIDI_CTRL_H_
#define _AUDIO_DEC_MIDI_CTRL_H_
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_config.h"
#include "audio_config.h"
#include "audio_dec.h"
#include "app_main.h"
#include "asm/dac.h"
#include "clock_cfg.h"
#include "key_event_deal.h"
#include "midi_ctrl_decoder.h"

/*----------------------------------------------------------------------------*/
/**@brief    ��midi ctrl����
   @param    sample_rate: ������
   @param    *path:��ɫ�ļ�·��
   @return   0���ɹ�
   @return   ��0��ʧ��
   @note
*/
/*----------------------------------------------------------------------------*/
int midi_ctrl_dec_open(u32 sample_rate, char *path);

/*----------------------------------------------------------------------------*/
/**@brief    �ر�midi ctrl����
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_dec_close(void);


/*----------------------------------------------------------------------------*/
/**@brief   ��������
   @param   prog:������
   @param   trk_num :���� (0~15)
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_set_porg(u8 prog, u8 trk_num);

/*----------------------------------------------------------------------------*/
/**@brief   ��������
   @param   nkey:������ţ�0~127��
   @param   nvel:�������ȣ�0~127��
   @param   chn :ͨ��(0~15)
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_note_on(u8 nkey, u8 nvel, u8 chn);


/*----------------------------------------------------------------------------*/
/**@brief   �����ɿ�
   @param   nkey:������ţ�0~127��
   @param   chn :ͨ��(0~15)
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_note_off(u8 nkey, u8 chn);


/*----------------------------------------------------------------------------*/
/**@brief  midi ���ýӿ�
   @param   cmd:����
   @param   priv:��Ӧcmd�Ľṹ��
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_confing(u32 cmd, void *priv);


/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard ���ð�����������������˥��ϵ��
   @param   obj:���ƾ��
   @param   samp:��Ӧsamplerate_tab����
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_confing_set_melody_decay(u16 val);

/*----------------------------------------------------------------------------*/
/**@brief  ����������
   @param   pitch_val:������ֵ,1 - 65535 ��256������ֵ,������������
   @param   chn :ͨ��(0~15)
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_ctrl_pitch_bend(u16 pitch_val, u8 chn);

#endif
