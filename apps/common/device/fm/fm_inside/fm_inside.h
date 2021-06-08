#ifndef _FM_INSIDE_H_
#define _FM_INSIDE_H_

#if(TCFG_FM_INSIDE_ENABLE == ENABLE)

#if DAC2IIS_EN
#define FM_DAC_OUT_SAMPLERATE  44100L
#else
#define FM_DAC_OUT_SAMPLERATE  44100L
#endif

/************************************************************
*                           FM����˵��
*��̨�٣�                     ��̨�ࣺ 				      ��̨�ࣺ
*��С  FMSCAN_CNR(��)         �Ӵ�  FMSCAN_CNR(��)        ��С��FMSCAN_AGC
*��С  FMSCAN_P_DIFFER(��)    �Ӵ�  FMSCAN_P_DIFFER(��)
*�Ӵ�  FMSCAN_N_DIFFER(��)    ��С  FMSCAN_N_DIFFER(��)
*
*ע�⣺��Ҫ�崮�ڲ�����̨��
*************************************************************/

#define FMSCAN_SEEK_CNT_MIN  400 //��С������� 400����
#define FMSCAN_SEEK_CNT_MAX  600 //��������� 600����
#define FMSCAN_960_CNR       34  //г��96M�Ļ���cnr 30~40
#define FMSCAN_1080_CNR      34  //г��108M�Ļ���cnr 30~40
#define FMSCAN_AGC 			 -1  //AGC��ֵ  -55����,Ĭ���������
#define FMSCAN_ADD_DIFFER 	 -67 //���ڴ�ֵ����noise differ, -67����

#define FMSCAN_CNR           2   //cnr  1����
#define FMSCAN_P_DIFFER		 2   //power differ  1����
#define FMSCAN_N_DIFFER   	 8   //noise differ  8����

#define FM_IF                3   //0,1.875; 1,2.143; 2,1.5; 3,cnr�͵���Ƶ��̨



void fm_inside_init(void *priv);
bool fm_inside_set_fre(void *priv, u16 fre);
bool fm_inside_read_id(void *priv);
void fm_inside_powerdown(void *priv);
void fm_inside_mute(void *priv, u8 flag);

#endif
#endif // _FM_INSIDE_H_

