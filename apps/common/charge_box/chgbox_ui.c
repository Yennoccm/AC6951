#include "typedef.h"
#include "asm/pwm_led.h"
#include "system/includes.h"
#include "chgbox_ctrl.h"
#include "chargeIc_manage.h"
#include "chgbox_ui.h"
#include "app_config.h"

#if (TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[CHGBOXUI]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#if (TCFG_CHARGE_BOX_UI_ENABLE)

//���ڲ�ui��˵������Ϊ��������
//1.ui״̬��
//2.ui�м��
//3.ui������
//״̬����Ҫ�����ⲿ�Ѳֵ�״̬���������м����һ�����ɣ��粻���ñ������������Լ������м��
//����ֻʹ�ñ�������������ʹ��

/////////////////////////////////////////////////////////////////////////////////////////////
//ui״̬��
typedef struct _chgbox_ui_var_ {
    int ui_timer;
    u8  ui_power_on; //�ϵ��־
} _chgbox_ui_var;

static _chgbox_ui_var chgbox_ui_var;
#define __this  (&chgbox_ui_var)

/*------------------------------------------------------------------------------------*/
/**@brief    UI��ʱ����
   @param    priv:ui״̬
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chgbox_ui_update_timeout(void *priv)
{
    u8 ledmode = (u8)priv;
    __this->ui_timer = 0;
    chgbox_led_set_mode(ledmode);
}

/*------------------------------------------------------------------------------------*/
/**@brief    UI��ʱ����
   @param    priv:����func�Ĳ���
             func:��ʱ��Ļص�����
             msec:N��������func
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
u16 chgbox_ui_timeout_add(int priv, void (*func)(void *priv), u32 msec)
{
    if (__this->ui_timer) {
        sys_timer_del(__this->ui_timer);
        __this->ui_timer = 0;
    }
    if (func != NULL) {
        __this->ui_timer = sys_timeout_add((void *)priv, func, msec);
    }
    return __this->ui_timer;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ����ui�ϵ��־λ
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chgbox_ui_set_power_on(u8 flag)
{
    __this->ui_power_on = flag;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��ȡui�ϵ��־λ
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
u8 chgbox_get_ui_power_on(void)
{
    return __this->ui_power_on;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ֵ���̬ui����
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chgbox_ui_update_local_power(void)
{
    //���״̬,����ʾ�յ���UI
    if (sys_info.pair_status) {
        return;
    }
    if (sys_info.status[USB_DET] == STATUS_ONLINE) {
        chgbox_ui_timeout_add(0, NULL, 0);
        if (sys_info.localfull) {
            chgbox_led_set_mode(CHGBOX_LED_GREEN_ON);//�������̵Ƴ���
        } else {
            chgbox_led_set_mode(CHGBOX_LED_RED_SLOW_BRE);//����е�����
        }
    } else {
        if (sys_info.lowpower_flag) {
            chgbox_led_set_mode(CHGBOX_LED_RED_FAST_BRE); //����4��
            chgbox_ui_timeout_add(CHGBOX_LED_RED_OFF, chgbox_ui_update_timeout, 4000);
        } else {
            chgbox_led_set_mode(CHGBOX_LED_GREEN_ON);
            chgbox_ui_timeout_add(CHGBOX_LED_GREEN_OFF, chgbox_ui_update_timeout, 8000);
        }
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ֹ���̬ui����
   @param    status:UI״̬
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chgbox_ui_updata_default_status(u8 status)
{
    switch (status) {
    case CHGBOX_UI_ALL_OFF:
        chgbox_ui_timeout_add(0, NULL, 0);
        chgbox_led_set_mode(CHGBOX_LED_ALL_OFF);
        break;
    case CHGBOX_UI_ALL_ON:
        chgbox_ui_timeout_add(0, NULL, 0);
        chgbox_led_set_mode(CHGBOX_LED_ALL_ON);
        break;
    case CHGBOX_UI_POWER:
        chgbox_ui_update_local_power();
        break;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ֺϸǳ��ui����
   @param    status:UI״̬
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chgbox_ui_updata_charge_status(u8 status)
{
    switch (status) {
    case CHGBOX_UI_USB_IN:
    case CHGBOX_UI_KEY_CLICK:
    case CHGBOX_UI_LOCAL_FULL:
        chgbox_ui_update_local_power();
        break;
    case CHGBOX_UI_USB_OUT:
        chgbox_ui_timeout_add(0, NULL, 0);
        chgbox_led_set_mode(CHGBOX_LED_ALL_OFF);
        break;
    case CHGBOX_UI_CLOSE_LID:
        if (sys_info.status[USB_DET] == STATUS_ONLINE) {
            chgbox_ui_update_local_power();
        } else {
            chgbox_ui_timeout_add(0, NULL, 0);
            chgbox_led_set_mode(CHGBOX_LED_GREEN_OFF);
        }
        break;
    case CHGBOX_UI_EAR_FULL:
        break;
    case CHGBOX_UI_OVER_CURRENT:
        if (sys_info.status[USB_DET] == STATUS_ONLINE) {
            chgbox_ui_update_local_power();
        } else {
            chgbox_led_set_mode(CHGBOX_LED_RED_FAST_BRE); //����4��
            chgbox_ui_timeout_add(CHGBOX_LED_RED_OFF, chgbox_ui_update_timeout, 4000);
        }
        break;
    default:
        chgbox_ui_updata_default_status(status);
        break;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ֿ���ͨ��ui����
   @param    status:UI״̬
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chgbox_ui_updata_comm_status(u8 status)
{
    switch (status) {
    case CHGBOX_UI_USB_IN:
    case CHGBOX_UI_LOCAL_FULL:
    case CHGBOX_UI_KEY_CLICK:
    case CHGBOX_UI_OPEN_LID:
        chgbox_ui_update_local_power();
        break;
    case CHGBOX_UI_USB_OUT:
        if (!sys_info.pair_status) {
            chgbox_ui_timeout_add(0, NULL, 0);
            chgbox_led_set_mode(CHGBOX_LED_RED_OFF);
        }
        break;
    case CHGBOX_UI_EAR_L_IN:
    case CHGBOX_UI_EAR_R_IN:
    case CHGBOX_UI_EAR_L_OUT:
    case CHGBOX_UI_EAR_R_OUT:
        if (!sys_info.pair_status) {
            if (sys_info.status[USB_DET] == STATUS_ONLINE) {
                if (sys_info.localfull) {
                    chgbox_led_set_mode(CHGBOX_LED_RED_OFF);
                    chgbox_ui_timeout_add(CHGBOX_LED_RED_ON, chgbox_ui_update_timeout, 500);
                }
            } else {
                chgbox_led_set_mode(CHGBOX_LED_GREEN_ON);
                chgbox_ui_timeout_add(CHGBOX_LED_GREEN_OFF, chgbox_ui_update_timeout, 500);
            }
        }
        break;
    case CHGBOX_UI_KEY_LONG:
        chgbox_ui_timeout_add(0, NULL, 0);
        chgbox_led_set_mode(CHGBOX_LED_BLUE_ON);
        break;
    case CHGBOX_UI_PAIR_START:
        chgbox_ui_timeout_add(0, NULL, 0);
        chgbox_led_set_mode(CHGBOX_LED_BLUE_FAST_FLASH);
        break;
    case CHGBOX_UI_PAIR_SUCC:
        if (sys_info.status[USB_DET] == STATUS_OFFLINE) {
            chgbox_ui_timeout_add(CHGBOX_LED_BLUE_OFF, chgbox_ui_update_timeout, 500);
        } else {
            if (!sys_info.localfull) {
                chgbox_ui_timeout_add(CHGBOX_LED_RED_SLOW_FLASH, chgbox_ui_update_timeout, 500);
            } else {
                chgbox_ui_timeout_add(0, NULL, 0);
                chgbox_led_set_mode(CHGBOX_LED_BLUE_ON);
            }
        }
        break;
    case CHGBOX_UI_PAIR_STOP:
        if (sys_info.status[USB_DET] == STATUS_ONLINE) {
            chgbox_ui_update_local_power();
        } else {
            chgbox_ui_timeout_add(0, NULL, 0);
            chgbox_led_set_mode(CHGBOX_LED_BLUE_OFF);
        }
        break;
    default:
        chgbox_ui_updata_default_status(status);
        break;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ֵ͵���ui����
   @param    status:UI״̬
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chgbox_ui_updata_lowpower_status(u8 status)
{
    switch (status) {
    case CHGBOX_UI_LOCAL_FULL:
    case CHGBOX_UI_LOWPOWER:
    case CHGBOX_UI_KEY_CLICK:
    case CHGBOX_UI_OPEN_LID:
    case CHGBOX_UI_CLOSE_LID:
    case CHGBOX_UI_USB_OUT:
    case CHGBOX_UI_USB_IN:
        chgbox_ui_update_local_power();
        break;
    default:
        chgbox_ui_updata_default_status(status);
        break;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ����UI���²�״̬
   @param    mode:  ���ֵ�ǰ��UIģʽ������ֵ�����ģʽ��Ӧ��
             status:���ֵ�ǰ״̬
   @return
   @note     ����ģʽ����״̬����ui�仯
*/
/*------------------------------------------------------------------------------------*/
void chgbox_ui_update_status(u8 mode, u8 status)
{
    switch (mode) {
    case UI_MODE_CHARGE:
        chgbox_ui_updata_charge_status(status);
        break;
    case UI_MODE_COMM:
        chgbox_ui_updata_comm_status(status);
        break;
    case UI_MODE_LOWPOWER:
        chgbox_ui_updata_lowpower_status(status);
        break;
    }
    chgbox_ui_set_power_on(0);
}

/*------------------------------------------------------------------------------------*/
/**@brief    ����UI��ʼ��
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void chgbox_ui_manage_init(void)
{
    chgbox_led_init();
}


/////////////////////////////////////////////////////////////////////////////////////////////
//ui�м��

/*------------------------------------------------------------------------------------*/
/**@brief    �������ú�����ģʽ
   @param    mode: ��ģʽ
   @return   ��
   @note     ���ó��ֵƵ�״̬,��ͬ����������˸���Ե���
*/
/*------------------------------------------------------------------------------------*/
void chgbox_led_set_mode(u8 mode)
{
    u8 i;
    log_info("CHG_LED_mode:%d\n", mode);
    switch (mode) {
    case CHGBOX_LED_RED_OFF://���
        chgbox_set_led_stu(CHG_LED_RED, 0, 0, 1);
        break;
    case CHGBOX_LED_RED_FAST_OFF:
        chgbox_set_led_stu(CHG_LED_RED, 0, 0, 0);
        break;
    case CHGBOX_LED_RED_ON://���
        chgbox_set_led_stu(CHG_LED_RED, 1, 0, 1);
        break;
    case CHGBOX_LED_RED_FAST_ON:
        chgbox_set_led_stu(CHG_LED_RED, 1, 0, 0);
        break;
    case CHGBOX_LED_RED_SLOW_FLASH:
        chgbox_set_led_bre(CHG_LED_RED, LED_FLASH_SLOW, 0, 0xffff);
        break;
    case CHGBOX_LED_RED_FLAST_FLASH:
        chgbox_set_led_bre(CHG_LED_RED, LED_FLASH_FAST, 0, 0xffff);
        break;
    case CHGBOX_LED_RED_SLOW_BRE:
        chgbox_set_led_bre(CHG_LED_RED, LED_FLASH_SLOW, 1, 0xffff);
        break;
    case CHGBOX_LED_RED_FAST_BRE:
        chgbox_set_led_bre(CHG_LED_RED, LED_FLASH_FAST, 1, 0xffff);
        break;
    case CHGBOX_LED_GREEN_OFF:
        chgbox_set_led_stu(CHG_LED_GREEN, 0, 1, 1);
        break;
    case CHGBOX_LED_GREEN_FAST_OFF:
        chgbox_set_led_stu(CHG_LED_GREEN, 0, 0, 0);
        break;
    case CHGBOX_LED_GREEN_ON:
        chgbox_set_led_stu(CHG_LED_GREEN, 1, 1, 1);
        break;
    case CHGBOX_LED_GREEN_FAST_ON:
        chgbox_set_led_stu(CHG_LED_GREEN, 1, 0, 0);
        break;
    case CHGBOX_LED_GREEN_SLOW_FLASH:
        chgbox_set_led_bre(CHG_LED_GREEN, LED_FLASH_SLOW, 0, 0xffff);
        break;
    case CHGBOX_LED_GREEN_FAST_FLASH:
        chgbox_set_led_bre(CHG_LED_GREEN, LED_FLASH_FAST, 0, 0xffff);
        break;
    case CHGBOX_LED_GREEN_SLOW_BRE:
        chgbox_set_led_bre(CHG_LED_GREEN, LED_FLASH_SLOW, 1, 0xffff);
        break;
    case CHGBOX_LED_GREEN_FAST_BRE:
        chgbox_set_led_bre(CHG_LED_GREEN, LED_FLASH_FAST, 1, 0xffff);
        break;
    case CHGBOX_LED_BLUE_OFF:
        chgbox_set_led_stu(CHG_LED_BLUE, 0, 0, 1);
        break;
    case CHGBOX_LED_BLUE_FAST_OFF:
        chgbox_set_led_stu(CHG_LED_BLUE, 0, 0, 0);
        break;
    case CHGBOX_LED_BLUE_ON:
        chgbox_set_led_stu(CHG_LED_BLUE, 1, 0, 1);
        break;
    case CHGBOX_LED_BLUE_FAST_ON:
        chgbox_set_led_stu(CHG_LED_BLUE, 1, 0, 0);
        break;
    case CHGBOX_LED_BLUE_SLOW_FLASH:
        chgbox_set_led_bre(CHG_LED_BLUE, LED_FLASH_SLOW, 0, 0xffff);
        break;
    case CHGBOX_LED_BLUE_FAST_FLASH:
        chgbox_set_led_bre(CHG_LED_BLUE, LED_FLASH_FAST, 0, 0xffff);
        break;
    case CHGBOX_LED_BLUE_SLOW_BRE:
        chgbox_set_led_bre(CHG_LED_BLUE, LED_FLASH_SLOW, 1, 0xffff);
        break;
    case CHGBOX_LED_BLUE_FAST_BRE:
        chgbox_set_led_bre(CHG_LED_BLUE, LED_FLASH_FAST, 1, 0xffff);
        break;
    case CHGBOX_LED_ALL_OFF:
        chgbox_set_led_all_off(1);
        break;
    case CHGBOX_LED_ALL_FAST_OFF:
        chgbox_set_led_all_off(0);
        break;
    case CHGBOX_LED_ALL_ON:
        chgbox_set_led_all_on(1);
        break;
    case CHGBOX_LED_ALL_FAST_ON:
        chgbox_set_led_all_on(0);
        break;
    }
}

#else

void chgbox_ui_set_power_on(u8 flag)
{
}

u8 chgbox_get_ui_power_on(void)
{
    return 0;
}

void chgbox_ui_update_status(u8 mode, u8 status)
{
}

void chgbox_ui_manage_init(void)
{
}

#endif

#endif
