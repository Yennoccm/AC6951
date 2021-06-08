#ifndef _CHGBOX_CTRL_H_
#define _CHGBOX_CTRL_H_
#include "event.h"

#define STATUS_OFFLINE  0
#define STATUS_ONLINE   1

enum {
    USB_DET,        //usb���
    LID_DET,        //���Ӽ��
    LDO_DET,        //��ѹ�ɹ����
    WIRELESS_DET,   //���߳���
    DET_MAX,
};

enum {
    KEY_POWER_CLICK,
    KEY_POWER_LONG,
    KEY_POWER_HOLD,
    KEY_POWER_UP,
    KEY_POWER_DOUBLE,
    KEY_POWER_THIRD,
};

//���ֵ�Ȼ״̬
enum {
    CHG_STATUS_COMM,      //����ͨ��
    CHG_STATUS_CHARGE,    //�ϸǳ��
    CHG_STATUS_LOWPOWER,  //���ֵ�ѹ��
};


//��⼸�β����ߺ����Ϊ�����γ�
#define TCFG_EAR_OFFLINE_MAX     4
//����shutdown�ĸ���
#define TCFG_SEND_SHUT_DOWN_MAX  5
//����closelid�ĸ��� -- ��Ҫ EAR_OFFLINE_MAX ��
#define TCFG_SEND_CLOSE_LID_MAX  5


typedef struct _SYS_INFO {
    volatile u8 charge: 1;        //�Ƿ��ڳ��״̬
        volatile u8 ear_l_full: 1;     //������Ƿ����
        volatile u8 ear_r_full: 1;     //�Ҷ����Ƿ����
        volatile u8 earfull: 1;     //�Ƿ����
        volatile u8 localfull: 1;   //�����Ƿ����
        volatile u8 led_flag: 1;    //led��Ծ״̬
        volatile u8 lowpower_flag: 1; //�͵���
        volatile u8 power_on: 1;    //�ϵ�/����

        volatile u8 pair_succ: 1;   //��Գɹ�
        volatile u8 init_ok: 1;     //���IC�Ƿ��ʼ���ɹ�
        volatile u8 chg_addr_ok: 1;     //��ȡ�㲥��ַ�ɹ�
        volatile u8 current_limit: 1; //����
        volatile u8 temperature_limit: 1;//���ȹ���
        volatile u8 wireless_wakeup: 1; //������߳份��
        volatile u8 reserev: 2; //����bit

        volatile u8 pair_status;    //�������״̬
        volatile u8 shut_cnt;       //�ػ����������
        volatile u8 lid_cnt;        //�ظ����������
        volatile u8 life_cnt;       //��ʱ����
        volatile u8 force_charge;       //ǿ�Ƴ�磨������ȫû��ʱ��Ҫ�ȳ�磩
        volatile u8 chgbox_status;     //����״̬�����ǡ��ϸǡ��͵��
        volatile u8 status[DET_MAX];
    } SYS_INFO;

    typedef struct _EAR_INFO {
    volatile u8 online[2];    //�������߼���
    volatile u8 power[2];     //����
    volatile u8 full_cnt[2];  //������������
} EAR_INFO;


extern SYS_INFO sys_info;
extern EAR_INFO ear_info;

u8 chargebox_check_output_short(void);
void chargebox_set_output_short(void);
void app_charge_box_ctrl_init(void);
int charge_box_ctrl_event_handler(struct chargebox_event *chg_event);
int charge_box_key_event_handler(u16 event);

void  chgbox_init_app(void);
#endif
