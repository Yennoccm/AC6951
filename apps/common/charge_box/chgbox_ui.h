#ifndef _CHGBOX_UI_H_
#define _CHGBOX_UI_H_

#include "typedef.h"
//���ڲ�ui��˵������Ϊ��������
//1.ui״̬��
//2.ui�м��
//3.ui������
//״̬����Ҫ�����ⲿ�Ѳֵ�״̬���������м����һ�����ɣ��粻���ñ������������Լ������м��
//����ֻʹ�ñ�������������ʹ��
/////////////////////////////////////////////////////////////////////////////////////////////
//ui״̬��
typedef enum {
    CHGBOX_UI_NULL = 0,

    CHGBOX_UI_ALL_OFF,
    CHGBOX_UI_ALL_ON,

    CHGBOX_UI_POWER,
    CHGBOX_UI_EAR_FULL,
    CHGBOX_UI_LOCAL_FULL,
    CHGBOX_UI_LOWPOWER,

    CHGBOX_UI_EAR_L_IN,
    CHGBOX_UI_EAR_R_IN,
    CHGBOX_UI_EAR_L_OUT,
    CHGBOX_UI_EAR_R_OUT,

    CHGBOX_UI_KEY_CLICK,
    CHGBOX_UI_KEY_LONG,
    CHGBOX_UI_PAIR_START,
    CHGBOX_UI_PAIR_SUCC,
    CHGBOX_UI_PAIR_STOP,

    CHGBOX_UI_OPEN_LID,
    CHGBOX_UI_CLOSE_LID,

    CHGBOX_UI_USB_IN,
    CHGBOX_UI_USB_OUT,

    CHGBOX_UI_OVER_CURRENT,
} UI_STATUS;

enum {
    UI_MODE_CHARGE,
    UI_MODE_COMM,
    UI_MODE_LOWPOWER,
};

void  chgbox_ui_manage_init(void);
void chgbox_ui_update_status(u8 mode, u8 status);
void chgbox_ui_set_power_on(u8 flag);
u8 chgbox_get_ui_power_on(void);



/////////////////////////////////////////////////////////////////////////////////////////////
//ui�м��

//���ģʽ
enum {
    CHGBOX_LED_RED_OFF,//������
    CHGBOX_LED_RED_FAST_OFF,//ֱ����
    CHGBOX_LED_RED_ON,//������
    CHGBOX_LED_RED_FAST_ON,//ֱ����
    CHGBOX_LED_RED_SLOW_FLASH,//����
    CHGBOX_LED_RED_FLAST_FLASH,//����
    CHGBOX_LED_RED_SLOW_BRE,//��������
    CHGBOX_LED_RED_FAST_BRE,//��������

    CHGBOX_LED_GREEN_OFF,
    CHGBOX_LED_GREEN_FAST_OFF,
    CHGBOX_LED_GREEN_ON,
    CHGBOX_LED_GREEN_FAST_ON,
    CHGBOX_LED_GREEN_SLOW_FLASH,
    CHGBOX_LED_GREEN_FAST_FLASH,
    CHGBOX_LED_GREEN_SLOW_BRE,
    CHGBOX_LED_GREEN_FAST_BRE,

    CHGBOX_LED_BLUE_ON,
    CHGBOX_LED_BLUE_FAST_ON,
    CHGBOX_LED_BLUE_OFF,
    CHGBOX_LED_BLUE_FAST_OFF,
    CHGBOX_LED_BLUE_SLOW_FLASH,
    CHGBOX_LED_BLUE_FAST_FLASH,
    CHGBOX_LED_BLUE_SLOW_BRE,
    CHGBOX_LED_BLUE_FAST_BRE,

    CHGBOX_LED_ALL_OFF,
    CHGBOX_LED_ALL_FAST_OFF,
    CHGBOX_LED_ALL_ON,
    CHGBOX_LED_ALL_FAST_ON,
};
void chgbox_led_set_mode(u8 mode);


/////////////////////////////////////////////////////////////////////////////////////////////
//ui������
//����n����
enum {
    CHG_LED_RED,
    CHG_LED_GREEN,
    CHG_LED_BLUE,
    CHG_LED_MAX,
};

//��˸����
enum {
    LED_FLASH_FAST,
    LED_FLASH_SLOW,
};

//led������ʼ��
void chgbox_led_init(void);
void chgbox_set_led_stu(u8 led_type, u8 on_off, u8 sp_flicker, u8 fade);
void chgbox_set_led_bre(u8 led_type, u8 speed_mode, u8 is_bre, u16 time);
void chgbox_set_led_all_off(u8 fade);
void chgbox_set_led_all_on(u8 fade);

#endif    //_APP_CHARGEBOX_H_

