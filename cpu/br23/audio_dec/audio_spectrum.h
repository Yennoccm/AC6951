#ifndef _AUDIO_SPECTRUM_H_
#define _AUDIO_SPECTRUM_H_

#include "app_config.h"
#include "clock_cfg.h"
#include "media/includes.h"
#include "asm/includes.h"
#include "spectrum/spectrum_eq.h"
#include "spectrum/spectrum_fft.h"


/*----------------------------------------------------------------------------*/
/**@brief   ��Ƶ��ͳ��
   @param   sr:������
   @return  hdl:���
   @note
*/
/*----------------------------------------------------------------------------*/
spectrum_fft_hdl *spectrum_open_demo(u32 sr, u8 channel);

/*----------------------------------------------------------------------------*/
/**@brief   �ر�Ƶ��ͳ��
   @param  hdl:���
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void spectrum_close_demo(spectrum_fft_hdl *hdl);

/*----------------------------------------------------------------------------*/
/**@brief   Ƶ���������
   @return
   @note   ��λ�ȡƵ��ֵ���ο��ú���
*/
/*----------------------------------------------------------------------------*/
void spectrum_get_demo(void *p);


/*----------------------------------------------------------------------------*/
/**@brief  �л�Ƶ�����
   @param  en:0 ����Ƶ����㣬 1 ʹ��Ƶ����㣨ͨ��ģʽ����ر�Ƶ����㣩
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void spectrum_switch_demo(u8 en);

#endif




