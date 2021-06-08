#include "audio_spectrum.h"


#if AUDIO_SPECTRUM_CONFIG
/*----------------------------------------------------------------------------*/
/**@brief   Ƶ���������
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void spectrum_get_demo(void *p)
{
    spectrum_fft_hdl *hdl = p;
    if (hdl) {
        u8 db_num = audio_spectrum_fft_get_num(hdl);//��ȡƵ�׸���
        short *db_data = audio_spectrum_fft_get_val(hdl);//��ȡ�洢Ƶ��ֵ�õ�ַ
        if (!db_data) {
            return;
        }
        for (int i = 0; i < db_num; i++) {
            //���db_num�� dbֵ
            printf("db_data db[%d] %d\n", i, db_data[i]);
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief   ��Ƶ��ͳ��
   @param   sr:������
   @return  hdl:���
   @note
*/
/*----------------------------------------------------------------------------*/
spectrum_fft_hdl *spectrum_open_demo(u32 sr, u8 channel)
{
    spectrum_fft_hdl *hdl = NULL;
    spectrum_fft_open_parm parm = {0};
    parm.sr = sr;
    parm.channel = channel;
    parm.attackFactor = 0.9;
    parm.releaseFactor = 0.9;
    parm.mode = 2;
    hdl = audio_spectrum_fft_open(&parm);
    /* int ret = sys_timer_add(hdl, spectrum_get_demo, 500);//Ƶ��ֵ��ȡ���� */
    clock_add(SPECTRUM_CLK);
    return hdl;
}

/*----------------------------------------------------------------------------*/
/**@brief   �ر�Ƶ��ͳ��
   @param  hdl:���
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void spectrum_close_demo(spectrum_fft_hdl *hdl)
{
    audio_spectrum_fft_close(hdl);
    clock_remove(SPECTRUM_CLK);
}


spectrum_fft_hdl *spec_hdl;
/*----------------------------------------------------------------------------*/
/**@brief  �л�Ƶ�����
   @param  en:0 ����Ƶ����㣬 1 ʹ��Ƶ����㣨ͨ��ģʽ����ر�Ƶ����㣩
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void spectrum_switch_demo(u8 en)
{
    if (spec_hdl) {
        audio_spectrum_fft_switch(spec_hdl, en);
    }
}


#else
void spectrum_switch_demo(u8 en)
{
}
#endif
