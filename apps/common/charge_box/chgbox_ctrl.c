#include "system/includes.h"
#include "app_config.h"
#include "app_task.h"
#include "chargeIc_manage.h"
#include "chgbox_ctrl.h"
#include "chgbox_box.h"
#include "chgbox_det.h"
#include "chgbox_wireless.h"
#include "key_event_deal.h"
#include "device/chargebox.h"
#include "chgbox_ui.h"
#include "chgbox_handshake.h"
#include "asm/pwm_led.h"
#include "le_smartbox_module.h"

#if(TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[CHG_CTRL]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

//ǿ�Ƹ���������ʱ�䣬Ϊ0����Ҫǿ�Ƴ��
#define CHGBOX_FORCE_CHARGE_TIMES 240 //������ز���������������2min

//�������������Զ��ر�
#define CHGBOX_BT_AUTO_OFF_TIMES  360 //3minδ��⵽�����͹ر�����

//��������������������ѹ
#define CHG_EAR_FULL_DET_CNT       6 //ע�����λ�ã��ټ�����ʱ��
#define CHG_EAR_FULL_DET_LEVEL     100 //��ѹֵ
#define TEMP_PROTECT_TIMEOUT       300000 //�¶ȱ�����ʱδ�ָ��ػ�ʱ��

void chargebox_set_newstatus(u8 newstatus);

SYS_INFO sys_info;
EAR_INFO ear_info;
static int temp_protect_timer;
static u32 bt_auto_off_cnt;
static int auto_shutdown_timer = 0;//timer ���
extern void chgbox_enter_soft_power_off(void);
extern void usb_key_check_entry(u32 timeout);
extern u16 get_curr_channel_state();
extern OS_MUTEX power_mutex;
/*------------------------------------------------------------------------------------*/
/**@brief    �Զ��ػ�����
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
static void sys_auto_shutdown_deal(void *priv)
{
#if TCFG_APP_BT_EN
    if (bt_auto_off_cnt) {
        return;
    }
#endif
    sys_info.life_cnt++;
    if (sys_info.life_cnt > TCFG_CHARGGBOX_AUTO_SHUT_DOWN_TIME) {
        log_info("charegebox enter soft poweroff\n");
        chgbox_enter_soft_power_off();
    }
}

void sys_auto_shutdown_reset(void)
{
    sys_info.life_cnt = 0;
}

/*------------------------------------------------------------------------------------*/
/**@brief    �Զ��ػ�ʹ��
   @param    ��
   @return   ��
   @note     ���Զ��ػ�������ע�ᵽtimer��
*/
/*------------------------------------------------------------------------------------*/
void sys_auto_shutdown_enable(void)
{
    if (!auto_shutdown_timer) {
        sys_info.life_cnt = 0;
        auto_shutdown_timer = sys_timer_add(NULL, sys_auto_shutdown_deal, 1000);
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    �Զ��ػ��ر�
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void sys_auto_shutdown_disable(void)
{
    if (auto_shutdown_timer) {
        sys_timer_del(auto_shutdown_timer);
        auto_shutdown_timer = 0;
        sys_info.life_cnt = 0;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��ʱʱ�ر��Զ���ѹ
   @param    ��
   @return   ��
   @note     ��USB���߻������߳��������ӳ���ʱʱ��
*/
/*------------------------------------------------------------------------------------*/
static void chargebox_temp_protect_timeout(void *priv)
{
    temp_protect_timer = 0;
    if (sys_info.status[USB_DET] == STATUS_ONLINE || sys_info.status[WIRELESS_DET] == STATUS_ONLINE) {
        temp_protect_timer = sys_timeout_add(NULL, chargebox_temp_protect_timeout, TEMP_PROTECT_TIMEOUT);
    } else {
        chargeIc_boost_ctrl(0);
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ͨ��IO״̬�ж�����Ƿ��·
   @param    ��
   @return   0:û�з�����Ϣ, 1:���δ�����·/��������������Ϣ
   @note     �ڿ�����ѹ���ǰ����
*/
/*------------------------------------------------------------------------------------*/
u8 chargebox_check_output_short(void)
{
    if (sys_info.current_limit) {
        return 0;
    }
    gpio_set_pull_up(TCFG_CHARGEBOX_L_PORT, 1);
    gpio_set_pull_up(TCFG_CHARGEBOX_R_PORT, 1);
    gpio_direction_input(TCFG_CHARGEBOX_L_PORT);
    gpio_direction_input(TCFG_CHARGEBOX_R_PORT);
    if ((gpio_read(TCFG_CHARGEBOX_L_PORT) == 0) || (gpio_read(TCFG_CHARGEBOX_R_PORT) == 0)) {
        log_error("gpio status err, mabe short, L:%d, R:%d!\n", gpio_read(TCFG_CHARGEBOX_L_PORT), gpio_read(TCFG_CHARGEBOX_R_PORT));
        sys_info.current_limit = 1;
        app_chargebox_event_to_user(CHGBOX_EVENT_ENTER_CURRENT_PROTECT);
        return 1;
    }
    return 0;
}

/*------------------------------------------------------------------------------------*/
/**@brief    �������ж�·�����ı�������ʱ����
   @param    ��
   @return   ��
   @note     �ڻ����ж��������
*/
/*------------------------------------------------------------------------------------*/
void chargebox_set_output_short(void)
{
    if (sys_info.current_limit) {
        return;
    }
    chargeIc_boost_ctrl(0);//�ر���ѹ
    chargeIc_pwr_ctrl(0);//�رճ�翪��
    sys_info.current_limit = 1;
    app_chargebox_event_to_user(CHGBOX_EVENT_ENTER_CURRENT_PROTECT);
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���չ�����Ϣ����ӿ�
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
static int chargebox_common_event_handler(struct chargebox_event *e)
{
    switch (e->event) {
#if (TCFG_WIRELESS_ENABLE)
    case CHGBOX_EVENT_WL_DATA_OVER:
        wireless_data_over_run();
        break;
    case CHGBOX_EVENT_WIRELESS_ONLINE:
        log_info("WL_ONLINE_1\n");
        wireless_api_open();
        sys_auto_shutdown_reset();
        break;
    case CHGBOX_EVENT_WIRELESS_OFFLINE:
        log_info("WL_OFF_1\n");
        wireless_api_close();
        sys_auto_shutdown_reset();
        break;
#endif
#if TCFG_TEMPERATURE_ENABLE
    case CHGBOX_EVENT_ENTER_TEMP_PROTECT:
        //�رո��������
        if (sys_info.charge) {
            chargeIc_pwr_ctrl(0);
            chargebox_api_open_port(EAR_L);
            chargebox_api_open_port(EAR_R);
            if (temp_protect_timer) {
                sys_timer_del(temp_protect_timer);
            }
            temp_protect_timer = sys_timeout_add(NULL, chargebox_temp_protect_timeout, TEMP_PROTECT_TIMEOUT);
        }
        //�رո���س��
        if (sys_info.status[USB_DET] == STATUS_ONLINE) {
            chargebox_charge_close();
        }
        break;
    case CHGBOX_EVENT_EXIT_TEMP_PROTECT:
        //�ָ����������
        if (sys_info.charge) {
            os_mutex_pend(&power_mutex, 0);
            u8 msg_flag = chargebox_check_output_short();
            if (sys_info.current_limit) {
                chargeIc_boost_ctrl(1);
                chargebox_api_shutdown_port(EAR_L);
                chargebox_api_shutdown_port(EAR_R);
                chargeIc_pwr_ctrl(1);
            } else if (msg_flag == 0) {
                sys_info.force_charge = 0;
                sys_info.earfull  = 1;
                app_chargebox_event_to_user(CHGBOX_EVENT_EAR_FULL);//�ý�������
            }
            os_mutex_post(&power_mutex);
            if (temp_protect_timer) {
                sys_timer_del(temp_protect_timer);
                temp_protect_timer = 0;
            }
        }
        //�ָ�����س��
        if (sys_info.status[USB_DET] == STATUS_ONLINE) {
            chargebox_charge_start();
        }
        break;
#endif
    case CHGBOX_EVENT_USB_IN:
        if (sys_info.current_limit) {
            sys_info.current_limit = 0;
            app_chargebox_event_to_user(CHGBOX_EVENT_EXIT_CURRENT_PROTECT);
        }
        //�жϵ�ǰ���߳��Ƿ����ߣ����߲���Ӧ
        if ((sys_info.status[WIRELESS_DET] == STATUS_OFFLINE) && (app_get_curr_task() != APP_PC_TASK)) {
#if TCFG_USB_KEY_UPDATE_ENABLE
            //�ȼ���Ƿ�USB�������߲���
            usb_key_check_entry(3);
#endif
#if TCFG_HANDSHAKE_ENABLE
            chgbox_handshake_run_app();
            chgbox_handshake_set_repeat(2);//�����ּ���
#else
            app_chargebox_event_to_user(CHGBOX_EVENT_HANDSHAKE_OK);
#endif
        } else {
            app_chargebox_event_to_user(CHGBOX_EVENT_HANDSHAKE_OK);
        }
        break;
    case CHGBOX_EVENT_USB_OUT:
        chargebox_charge_close();
#if TCFG_HANDSHAKE_ENABLE
        chgbox_handshake_set_repeat(0);
#endif
#if TCFG_APP_PC_EN
        if (app_get_curr_task() != APP_IDLE_TASK) {
            app_task_switch_to(APP_IDLE_TASK);
        }
#endif
        break;
    case CHGBOX_EVENT_HANDSHAKE_OK:
        if ((sys_info.status[USB_DET] == STATUS_ONLINE) && (sys_info.temperature_limit == 0)) {
            chargebox_charge_start();
        }
#if TCFG_APP_PC_EN
        if (app_get_curr_task() != APP_PC_TASK) {
            app_task_switch_to(APP_PC_TASK);
        }
#endif
        break;
    }
    return true;
}

/******************************************************************************/
/*************************�ϸǳ��ģʽ��ش���**********************************/
/******************************************************************************/
static u8 ear_power_check_time;//��������������

/*------------------------------------------------------------------------------------*/
/**@brief    �ϸǳ��ģʽ  �ϸǴ���
   @param    ��
   @return   ��
   @note     �ȷ��ϸ����������(������м��������ߵļ��)���ٴ���ѹ���г��
*/
/*------------------------------------------------------------------------------------*/
static void charge_app_lid_close_deal(void)
{
    if (sys_info.lid_cnt & BIT(7)) {
        if (sys_info.lid_cnt & (~BIT(7))) {
            sys_info.lid_cnt--;
            app_chargebox_send_mag(CHGBOX_MSG_SEND_CLOSE_LID);
        } else {
            sys_info.lid_cnt = 0;
            ear_power_check_time = 0;
            u8 msg_flag = chargebox_check_output_short();
            if (sys_info.current_limit == 0) {
                sys_info.charge = 1;//����и������ٿ���ѹ
                //�ȹر�IO,�ڴ򿪿������5V
                if (sys_info.temperature_limit == 0) {
                    os_mutex_pend(&power_mutex, 0);
                    chargeIc_boost_ctrl(1);
                    chargebox_api_shutdown_port(EAR_L);
                    chargebox_api_shutdown_port(EAR_R);
                    chargeIc_pwr_ctrl(1);
                    os_mutex_post(&power_mutex);
                } else {
                    //���±���,�и�����һ����Ϣ
                    app_chargebox_event_to_user(CHGBOX_EVENT_ENTER_TEMP_PROTECT);
                }
                //�����ֻ�������ܼ���ڲ֣�˵���������е磬����Ҫǿ�Ƴ��
                if (ear_info.online[EAR_L] && ear_info.online[EAR_R]) {
                    sys_info.force_charge = 0;
                }
            } else if (msg_flag == 0) {
                //�����쳣��������ػ�����
                sys_info.force_charge = 0;
                sys_info.earfull  = 1;
                app_chargebox_event_to_user(CHGBOX_EVENT_EAR_FULL);//�ý�������
            }
        }
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    �ϸǳ��ģʽ �ö�������ػ� ����
   @param    ��
   @return   ��
   @note     �ȷ������ö����ػ����ٹر����IO
*/
/*------------------------------------------------------------------------------------*/
static void charge_app_shut_down_deal(void)
{
    if (sys_info.shut_cnt & BIT(7)) {
        if (sys_info.shut_cnt & (~BIT(7))) {
            sys_info.shut_cnt--;
            app_chargebox_send_mag(CHGBOX_MSG_SEND_SHUTDOWN);
        } else {
            //�ػ������,��Դ�߶ϵ�
            sys_info.shut_cnt = 0;
            chargebox_api_shutdown_port(EAR_L);
            chargebox_api_shutdown_port(EAR_R);
        }
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    �������������
   @param    ��
   @return   ��
   @note     �����������������������¼�
*/
/*------------------------------------------------------------------------------------*/
void app_chargebox_ear_full_det(void *priv)
{
    if (sys_info.chgbox_status != CHG_STATUS_CHARGE) {
        return;
    }

    /* log_info("L:%d,F:%d,C:%d\n",ear_info.online[EAR_L],sys_info.ear_l_full,ear_info.full_cnt[EAR_L]); */
    if (ear_info.online[EAR_L]) { //����
        if ((ear_info.power[EAR_L] & 0x7f) == CHG_EAR_FULL_DET_LEVEL && sys_info.ear_l_full == 0) { //power�����bitΪ��־λ
            ear_info.full_cnt[EAR_L]++;
            if (ear_info.full_cnt[EAR_L] >= CHG_EAR_FULL_DET_CNT) {
                sys_info.ear_l_full = 1;       //������־��λ
            }
        } else {
            ear_info.full_cnt[EAR_L] = 0;
        }
    } else {
        ear_info.full_cnt[EAR_L] = 0;  //�������0
        sys_info.ear_l_full = 0;       //�������־��0
    }

    if (ear_info.online[EAR_R]) { //����
        if ((ear_info.power[EAR_R] & 0x7f) == CHG_EAR_FULL_DET_LEVEL && sys_info.ear_r_full == 0) { //power�����bitΪ��־λ
            ear_info.full_cnt[EAR_R]++;
            if (ear_info.full_cnt[EAR_R] >= CHG_EAR_FULL_DET_CNT) {
                sys_info.ear_r_full = 1;       //������־��λ
            }
        } else {
            ear_info.full_cnt[EAR_R] = 0;
        }
    } else {
        ear_info.full_cnt[EAR_R] = 0;  //�Ҽ�����0
        sys_info.ear_r_full = 0;       //�ҳ�����־��0
    }

    if (sys_info.earfull == 0) {
        //ͬʱ�����������߶����ˡ��������ߵ�����
        if ((sys_info.ear_r_full && sys_info.ear_l_full)
            || (sys_info.ear_l_full && ear_info.online[EAR_R] == 0)
            || (sys_info.ear_r_full && ear_info.online[EAR_L] == 0)) {
            if (sys_info.force_charge == 0) { //ǿ�Ƴ���ѹ�
                sys_info.earfull  = 1;
                log_info("ear online full\n");
                app_chargebox_event_to_user(CHGBOX_EVENT_EAR_FULL);
            }
        }
    } else { //û��������
        if ((!sys_info.ear_l_full && ear_info.online[EAR_L])
            || (!sys_info.ear_r_full && ear_info.online[EAR_R])) {
            sys_info.earfull  = 0;//�ܱ�־��0
        }
    }

    ///�ѹ�ǿ�Ƴ��ʱ�䣬������������,��full��������жϲ��Ƿ���
    if (ear_info.online[EAR_L] == 0 && ear_info.online[EAR_R] == 0 && sys_info.force_charge == 0) {
        log_info("no ear and force charge end\n");
        sys_info.earfull  = 1;
        app_chargebox_event_to_user(CHGBOX_EVENT_EAR_FULL);
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��ȡtws����
   @param    ��
   @return   ��
   @note     �����������������ȡtws����
*/
/*------------------------------------------------------------------------------------*/
static void charge_app_send_power(void)
{
    if (sys_info.shut_cnt || sys_info.lid_cnt || sys_info.earfull || sys_info.force_charge) {
        return;
    }
    ear_power_check_time++;
    if (ear_power_check_time > 25) { //5s
        ear_power_check_time = 0;
        app_chargebox_send_mag(CHGBOX_MSG_SEND_POWER_CLOSE);
        //���������Ҫ�ȶԷ��ظ����̻߳��л�����ʱȥ��ѯ
        sys_timeout_add(NULL, app_chargebox_ear_full_det, 200);
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    �Զ��ػ����
   @param    ��
   @return   ��
   @note     ���������ж��Ƿ�ʹ���Զ��ػ�
*/
/*------------------------------------------------------------------------------------*/
static void charge_app_check_enable_auto_shutdown(void)
{
    //1����粻����ʱ,�ж϶����Ƿ����,������ػ�
    //2�������ػ�ʹ�ܺ�,USB����ʱ�����ͳ��ն�������ػ�
#if TCFG_WIRELESS_ENABLE
    if ((sys_info.status[USB_DET] == STATUS_OFFLINE) && (sys_info.status[WIRELESS_DET] == STATUS_OFFLINE)) {
#else
    if (sys_info.status[USB_DET] == STATUS_OFFLINE) {
#endif
        if (sys_info.earfull) {
            sys_auto_shutdown_enable();
            return;
        }
    } else {
#if TCFG_CHARGE_FULL_ENTER_SOFTOFF
        if (sys_info.status[USB_DET] == STATUS_ONLINE) {
            if (sys_info.localfull && sys_info.earfull) {
                sys_auto_shutdown_enable();
            }
        }
#endif
    }
}

#define LDO_NOT_SUCC_TIMES  20   //������ע��ʱ��߶�
static u8 ldo_not_succ_cnt = 0;  //LDO�޷���ѹ ����
/*------------------------------------------------------------------------------------*/
/**@brief    �ϸǳ����봦��
   @param    ��
   @return   ��
   @note     �����ѹ���ɹ�����ǿ�Ƴ�糬ʱ����
*/
/*------------------------------------------------------------------------------------*/
static void charge_deal_half_second(void)
{
    //û�нӳ���ߣ����IC����ѹ����ʱ��������
#if TCFG_WIRELESS_ENABLE
    if ((sys_info.status[USB_DET] == STATUS_OFFLINE) && (sys_info.status[WIRELESS_DET] == STATUS_OFFLINE)) {
#else
    if (sys_info.status[USB_DET] == STATUS_OFFLINE) {
#endif
        if (sys_info.status[LDO_DET] == STATUS_OFFLINE) { //�޷���ѹ
            if (ldo_not_succ_cnt < LDO_NOT_SUCC_TIMES) {
                ldo_not_succ_cnt++;
                if (ldo_not_succ_cnt == LDO_NOT_SUCC_TIMES) {
                    log_info("auto shutdown by ldo not succ\n");
                    if (sys_info.force_charge || sys_info.temperature_limit) {
                        sys_info.force_charge = 0;
                        sys_info.earfull  = 1;
                        //�������֧������ֻ��ڳ��͵Ȳ����ٹ�
                        app_chargebox_event_to_user(CHGBOX_EVENT_EAR_FULL);//��������
                    } else {
                        sys_auto_shutdown_enable();
                    }
                }
            }
        } else {
            ldo_not_succ_cnt = 0;
        }
    }

    //ǿ�Ƴ�糬ʱ�������п����Ƕ��������ߣ��п����Ƕ�����ȫû��,�ȱ��ֳ��һ��ʱ�䣩
    if (sys_info.force_charge) {
        sys_info.force_charge--;
    }

    if (ear_info.online[EAR_L] && ear_info.online[EAR_R]) {
        sys_info.force_charge = 0;
    }

#if TCFG_APP_BT_EN
    if (get_curr_channel_state()) {
        bt_auto_off_cnt = CHGBOX_BT_AUTO_OFF_TIMES;
    } else if (bt_auto_off_cnt) {
        bt_auto_off_cnt--;
        if (bt_auto_off_cnt == 0) {
            app_task_switch_to(APP_IDLE_TASK);
        }
    }
#endif
}

static int charge_chargebox_event_handler(struct chargebox_event *e)
{
    switch (e->event) {
    case CHGBOX_EVENT_200MS:
        charge_app_lid_close_deal();
        charge_app_send_power();
        charge_app_shut_down_deal();
        break;
    case CHGBOX_EVENT_500MS:
        charge_deal_half_second();
        break;
#if TCFG_WIRELESS_ENABLE
    case CHGBOX_EVENT_WIRELESS_ONLINE:
        sys_auto_shutdown_disable();
        break;
    case CHGBOX_EVENT_WIRELESS_OFFLINE:
        charge_app_check_enable_auto_shutdown();
        break;
#endif
    case CHGBOX_EVENT_USB_IN:
        log_info("USB_IN_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_USB_IN);
        sys_auto_shutdown_disable();
        if (sys_info.status[LID_DET] == STATUS_ONLINE) {
            //���ǲ���USB
            chargebox_set_newstatus(CHG_STATUS_COMM);     //������ģʽ
        }
        break;
    case CHGBOX_EVENT_USB_OUT:
        log_info("USB_OUT_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_USB_OUT);
        charge_app_check_enable_auto_shutdown();
        if (sys_info.status[LID_DET] == STATUS_ONLINE) {
            //���ǰγ�USB
            chargebox_set_newstatus(CHG_STATUS_COMM);     //������ģʽ
        }
        break;
    case CHGBOX_EVENT_OPEN_LID:
        log_info("OPEN_LID_1\n");
#if SMART_BOX_EN
        bt_ble_rcsp_adv_enable();
#endif
        chargebox_set_newstatus(CHG_STATUS_COMM);     //������ģʽ
        break;
    case CHGBOX_EVENT_CLOSE_LID:
        log_info("CLOSE_LID_1\n");
        //���ǳ�ʱ������,���Բ����κβ���
#if SMART_BOX_EN
        bt_ble_rcsp_adv_disable();
#endif
        break;
    case CHGBOX_EVENT_EAR_L_ONLINE:
        log_info("EAR_L_IN_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_EAR_L_IN);
        break;
    case CHGBOX_EVENT_EAR_L_OFFLINE:
        log_info("EAR_L_OUT_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_EAR_L_OUT);
        break;
    case CHGBOX_EVENT_EAR_R_ONLINE:
        log_info("EAR_R_IN_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_EAR_R_IN);
        break;
    case CHGBOX_EVENT_EAR_R_OFFLINE:
        log_info("EAR_R_OUT_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_EAR_R_OUT);
        break;
    case CHGBOX_EVENT_ENTER_LOWPOWER:
    case CHGBOX_EVENT_NEED_SHUTDOWN:
        //�ر���ѹ,����͵�ģʽ
        chargebox_set_newstatus(CHG_STATUS_LOWPOWER);     //������ģʽ
        break;
    case CHGBOX_EVENT_EAR_FULL:
        //������������shutdownָ��
        log_info("EAR_FULL_1\n");
        if (!sys_info.current_limit) {
            chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_EAR_FULL);
        }
        //������ʱ,�ȹر�����ٴ�IO
        chargeIc_pwr_ctrl(0);
        chargebox_api_open_port(EAR_L);
        chargebox_api_open_port(EAR_R);
        chargeIc_boost_ctrl(0);
        sys_info.shut_cnt = BIT(7) | TCFG_SEND_SHUT_DOWN_MAX;
        sys_info.lid_cnt = 0;
        sys_info.charge = 0;//��ʱ������
        charge_app_check_enable_auto_shutdown();
        break;
    case CHGBOX_EVENT_LOCAL_FULL:
        log_info("LOCAL_FULL_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_LOCAL_FULL);
        charge_app_check_enable_auto_shutdown();
        break;
    case CHGBOX_EVENT_ENTER_CURRENT_PROTECT:
        log_info("CHGBOX_EVENT_OVER_CURRENT");
        chargeIc_pwr_ctrl(0);//�رճ�翪��
        chargeIc_boost_ctrl(0);//�ر���ѹ
        sys_info.force_charge = 0;
        sys_info.earfull  = 1;
        app_chargebox_event_to_user(CHGBOX_EVENT_EAR_FULL);//�ý�������
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_OVER_CURRENT);
        break;
    case CHGBOX_EVENT_EXIT_CURRENT_PROTECT:
        ear_power_check_time = 0;
        sys_info.shut_cnt = 0;
        sys_info.lid_cnt = BIT(7) | TCFG_SEND_CLOSE_LID_MAX;
        sys_info.charge = 0;//�ȹر�,�Ⱥϸ������Ŵ���ѹ
        sys_info.ear_l_full = 0;
        sys_info.ear_r_full = 0;
        sys_info.earfull = 0;
        sys_info.force_charge = CHGBOX_FORCE_CHARGE_TIMES;
        sys_auto_shutdown_disable();
        break;
    }
    return 0;
}







/******************************************************************************/
/*************************����ͨ��ģʽ��ش���**********************************/
/******************************************************************************/
#define KEY_PAIR_CNT    10
#define COMM_LIFE_MAX   (60*2)//һ���ӳ�ʱ
static u8 goto_pair_cnt;//��Թ��ܰ���ʱ�����
static u8 auto_exit_cnt;//�˳�����ͨ��ģʽ����

/*------------------------------------------------------------------------------------*/
/**@brief    ���״̬��⴦��
   @param    ��
   @return   ��
   @note     �� pair_status==2ʱ���ж��Ƿ���Գɹ�
*/
/*------------------------------------------------------------------------------------*/
static void comm_pair_connecting(void)
{
    if (sys_info.pair_status == 2) {
        if (sys_info.pair_succ) {
            sys_info.pair_status = 0;
            chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_PAIR_SUCC);
        } else {
            app_chargebox_send_mag(CHGBOX_MSG_SEND_PAIR);
        }
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ����ͨ��ģʽ��ʱ
   @param    ��
   @return   ��
   @note     ����ʱ��������л�ģʽ
*/
/*------------------------------------------------------------------------------------*/
static void comm_app_auto_exit(void)
{
    if (auto_exit_cnt++ > COMM_LIFE_MAX) {
#if SMART_BOX_EN
        bt_ble_rcsp_adv_disable();
#endif
        if (sys_info.lowpower_flag) {
            chargebox_set_newstatus(CHG_STATUS_LOWPOWER);     //������ģʽ
        } else {
            chargebox_set_newstatus(CHG_STATUS_CHARGE);     //������ģʽ
        }
    }
#if TCFG_APP_BT_EN
    if (get_curr_channel_state()) {
        bt_auto_off_cnt = CHGBOX_BT_AUTO_OFF_TIMES;
    } else if (bt_auto_off_cnt) {
        bt_auto_off_cnt--;
        if (bt_auto_off_cnt == 0) {
            app_task_switch_to(APP_IDLE_TASK);
        }
    }
#endif
}

static int comm_chargebox_event_handler(struct chargebox_event *e)
{
    switch (e->event) {
    case CHGBOX_EVENT_200MS:
        if (chgbox_adv_addr_scan()) {
            app_chargebox_send_mag(CHGBOX_MSG_SEND_POWER_OPEN);
        }
        break;
    case CHGBOX_EVENT_500MS:
        comm_pair_connecting();
        comm_app_auto_exit();
        break;
    case CHGBOX_EVENT_USB_IN:
        log_info("USB_IN_2\n");
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_USB_IN);
        app_chargebox_send_mag(CHGBOX_MSG_SEND_POWER_OPEN);
        auto_exit_cnt = 0;
        break;
    case CHGBOX_EVENT_USB_OUT:
        log_info("USB_OUT_2\n");
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_USB_OUT);
        app_chargebox_send_mag(CHGBOX_MSG_SEND_POWER_OPEN);
        auto_exit_cnt = 0;
        break;
    case CHGBOX_EVENT_LOCAL_FULL:
        log_info("LOCAL_FULL_2\n");
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_LOCAL_FULL);
        break;
    case CHGBOX_EVENT_CLOSE_LID:
        log_info("CLOSE_LID_2\n");
#if SMART_BOX_EN
        bt_ble_rcsp_adv_disable();
#endif
        if (sys_info.lowpower_flag) {
            chargebox_set_newstatus(CHG_STATUS_LOWPOWER);     //������ģʽ
        } else {
            chargebox_set_newstatus(CHG_STATUS_CHARGE);     //������ģʽ
        }
        break;
    case CHGBOX_EVENT_NEED_SHUTDOWN:
        chargebox_set_newstatus(CHG_STATUS_LOWPOWER);     //������ģʽ
        break;
    case CHGBOX_EVENT_EAR_L_ONLINE:
        log_info("EAR_L_IN_2\n");
        auto_exit_cnt = 0;
        if (chgbox_get_ui_power_on() == 0) {
            chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_EAR_L_IN);
        }
        break;
    case CHGBOX_EVENT_EAR_L_OFFLINE:
        log_info("EAR_L_OUT_2\n");
        auto_exit_cnt = 0;
#if SMART_BOX_EN
        bt_ble_rcsp_adv_disable();
#endif
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_EAR_L_OUT);
        break;
    case CHGBOX_EVENT_EAR_R_ONLINE:
        log_info("EAR_R_IN_2\n");
        auto_exit_cnt = 0;
        if (chgbox_get_ui_power_on() == 0) {
            chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_EAR_R_IN);
        }
        break;
    case CHGBOX_EVENT_EAR_R_OFFLINE:
        log_info("EAR_R_OUT_2\n");
        auto_exit_cnt = 0;
#if SMART_BOX_EN
        bt_ble_rcsp_adv_disable();
#endif
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_EAR_R_OUT);
        break;
    }
    return 0;
}


/******************************************************************************/
/*************************�͵�ģʽ����*****************************************/
/******************************************************************************/
/*------------------------------------------------------------------------------------*/
/**@brief   �͵����߼���
   @param    ��
   @return   ��
   @note     �ȷ��ػ�ָ�������
*/
/*------------------------------------------------------------------------------------*/
static void lowpower_shut_down_deal(void)
{
    if (sys_info.shut_cnt & BIT(7)) {
        if (sys_info.shut_cnt & (~BIT(7))) {
            sys_info.shut_cnt--;
            app_chargebox_send_mag(CHGBOX_MSG_SEND_SHUTDOWN);
        } else {
            //�ػ������,��Դ�߶ϵ�
            sys_info.charge = 0;
            sys_info.shut_cnt = 0;
            chargebox_api_shutdown_port(EAR_L);
            chargebox_api_shutdown_port(EAR_R);
        }
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    �͵�ϸ��������
   @param    ��
   @return   ��
   @note     lid_cnt��־��λ���ȷ��ϸ��������λshut_cnt��־
*/
/*------------------------------------------------------------------------------------*/
static void lowpower_lid_close_deal(void)
{
    if (sys_info.lid_cnt & BIT(7)) {
        if (sys_info.lid_cnt & (~BIT(7))) {
            sys_info.lid_cnt--;
            app_chargebox_send_mag(CHGBOX_MSG_SEND_CLOSE_LID);
        } else {
            sys_info.lid_cnt = 0;
            sys_info.shut_cnt = BIT(7) | TCFG_SEND_SHUT_DOWN_MAX;
        }
    }
}

static int lowpower_chargebox_event_handler(struct chargebox_event *e)
{
    switch (e->event) {
    case CHGBOX_EVENT_200MS:
        lowpower_lid_close_deal();
        lowpower_shut_down_deal();
        break;
    case CHGBOX_EVENT_500MS:
        break;
#if TCFG_WIRELESS_ENABLE
    case CHGBOX_EVENT_WIRELESS_ONLINE:
        sys_auto_shutdown_disable();
        break;
    case CHGBOX_EVENT_WIRELESS_OFFLINE:
        sys_auto_shutdown_enable();
        break;
#endif
    case CHGBOX_EVENT_USB_IN:
        log_info("USB_IN_3\n");
        sys_auto_shutdown_disable();
        chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_USB_IN);
        break;
    case CHGBOX_EVENT_USB_OUT:
        log_info("USB_OUT_3\n");
#if TCFG_WIRELESS_ENABLE
        if (sys_info.status[WIRELESS_DET] == STATUS_OFFLINE)
#endif
        {
            sys_auto_shutdown_enable();
        }
        chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_USB_OUT);
        break;
    case CHGBOX_EVENT_OPEN_LID:
        log_info("OPEN_LID_3\n");
        chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_OPEN_LID);
#if SMART_BOX_EN
        bt_ble_rcsp_adv_enable();
#endif
        chargebox_set_newstatus(CHG_STATUS_COMM);     //������ģʽ
        break;
    case CHGBOX_EVENT_CLOSE_LID:
        log_info("CLOSE_LID_3\n");
        chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_CLOSE_LID);
#if SMART_BOX_EN
        bt_ble_rcsp_adv_disable();
#endif
        break;
    case CHGBOX_EVENT_NEED_SHUTDOWN:
        if ((!sys_info.lid_cnt) && (sys_info.shut_cnt)) {
            power_set_soft_poweroff();
        }
        break;
    case CHGBOX_EVENT_EXIT_LOWPOWER:
        log_info("exit lower\n");
        if (sys_info.status[LID_DET] == STATUS_ONLINE) {
            chargebox_set_newstatus(CHG_STATUS_COMM);     //������ģʽ
        } else {
            chargebox_set_newstatus(CHG_STATUS_CHARGE);     //������ģʽ
        }
        break;
    }
    return 0;
}

/*------------------------------------------------------------------------------------*/
/**@brief    �����¼�����
   @param    event:������¼��ṹ�壬Я���¼���Ϣ
   @return   ��
   @note     ��ģʽ������ظ����¼�����ͬ�¼��ڲ�ͬģʽ���ܻ��в�ͬ����
*/
/*------------------------------------------------------------------------------------*/
int charge_box_ctrl_event_handler(struct chargebox_event *chg_event)
{
    chargebox_common_event_handler(chg_event);
    if (sys_info.chgbox_status == CHG_STATUS_CHARGE) {
        charge_chargebox_event_handler(chg_event);
    } else if (sys_info.chgbox_status == CHG_STATUS_COMM) {
        comm_chargebox_event_handler(chg_event);
    } else if (sys_info.chgbox_status == CHG_STATUS_LOWPOWER) {
        lowpower_chargebox_event_handler(chg_event);
    }
    return 0;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ְ�������
   @param    event:�����¼�
   @return   ��
   @note     ��ģʽ������ذ���
*/
/*------------------------------------------------------------------------------------*/
int charge_box_key_event_handler(u16 event)
{
    if (sys_info.chgbox_status == CHG_STATUS_CHARGE) {
        sys_info.life_cnt = 0;
        switch (event) {
        case KEY_BOX_POWER_CLICK:
            log_info("KEY_POWER_CLICK_chg\n");
            if (sys_info.status[LID_DET] == STATUS_ONLINE) {
                //���ǰγ�USB
                chargebox_set_newstatus(CHG_STATUS_COMM);     //������ģʽ
            } else {
                chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_KEY_CLICK);
            }
            break;
        case KEY_BOX_POWER_FIVE:
            log_info("KEY_POWER_FIVE_chg\n");
#if TCFG_APP_BT_EN
            if (app_get_curr_task() == APP_IDLE_TASK) {
                bt_auto_off_cnt = CHGBOX_BT_AUTO_OFF_TIMES;
                app_task_switch_to(APP_BT_TASK);
            }
#endif
            break;
        default:
            break;
        }
    } else if (sys_info.chgbox_status == CHG_STATUS_COMM) {
        auto_exit_cnt = 0;
        switch (event) {
        case KEY_BOX_POWER_CLICK:
            log_info("KEY_POWER_CLICK_comm\n");
            chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_KEY_CLICK);
            break;
        case KEY_BOX_POWER_LONG:
            log_info("KEY_POWER_LONG\n");
            if (sys_info.pair_status == 0) {
                sys_info.pair_status = 1;
                goto_pair_cnt = 0;
                chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_KEY_LONG);
            }
            break;
        case KEY_BOX_POWER_HOLD:
            /* log_info("KEY_POWER_HOLD\n"); */
            if (sys_info.pair_status == 1) {
                goto_pair_cnt++;
                if (goto_pair_cnt >= KEY_PAIR_CNT) {
                    sys_info.pair_status = 2;
                    sys_info.pair_succ = 0;
                    chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_PAIR_START);
                }
            }
            break;
        case KEY_BOX_POWER_UP:
            log_info("KEY_POWER_UP\n");
            if (sys_info.pair_status != 2) {
                sys_info.pair_status = 0;
                chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_PAIR_STOP);
            }
            break;
        case KEY_BOX_POWER_FIVE:
            log_info("KEY_POWER_FIVE_comm\n");
#if TCFG_APP_BT_EN
            if (app_get_curr_task() == APP_IDLE_TASK) {
                bt_auto_off_cnt = CHGBOX_BT_AUTO_OFF_TIMES;
                app_task_switch_to(APP_BT_TASK);
            }
#endif
            break;
        default:
            break;
        }
    } else if (sys_info.chgbox_status == CHG_STATUS_LOWPOWER) {
        switch (event) {
        case KEY_BOX_POWER_CLICK:
            log_info("KEY_POWER_CLICK_low\n");
            if (sys_info.status[USB_DET] == STATUS_OFFLINE) {
                chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_KEY_CLICK);
            }
            break;
        default:
            break;
        }
    }
    return 0;
}


/*------------------------------------------------------------------------------------*/
/**@brief    ����ģʽ���ú���
   @param    newstatus:�µ�ģʽ
   @return   ��
   @note     �˳���ǰģʽ�������µ�ģʽ�Ĳ���
*/
/*------------------------------------------------------------------------------------*/
void chargebox_set_newstatus(u8 newstatus)
{
    ///���˳���ǰ״̬
    log_info("chargebbox exit:%d\n", sys_info.chgbox_status);
    if (newstatus == sys_info.chgbox_status) {
        log_info("chargebbox status same\n");
        return;
    }

    if (sys_info.chgbox_status  == CHG_STATUS_COMM) {
        sys_auto_shutdown_enable();
        if (sys_info.pair_status) {
            sys_info.pair_status = 0;
            chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_ALL_OFF);
        }
    } else if (sys_info.chgbox_status == CHG_STATUS_CHARGE) {
        sys_info.lid_cnt = 0;
        sys_info.shut_cnt = 0;
        sys_info.charge = 0;//���ڳ��״̬
        //�رճ�����ʱ,��Ҫ��IO��
        chargeIc_boost_ctrl(0);
        chargeIc_pwr_ctrl(0);
        chargebox_api_open_port(EAR_L);
        chargebox_api_open_port(EAR_R);
        sys_auto_shutdown_enable();
    } else if (sys_info.chgbox_status == CHG_STATUS_LOWPOWER) {
        sys_info.shut_cnt = 0;
        sys_info.lid_cnt = 0;
    }

    ///������״̬
    sys_info.chgbox_status = newstatus;
    if (newstatus  == CHG_STATUS_COMM) {
        sys_auto_shutdown_disable();
        sys_info.shut_cnt = 0;
        sys_info.pair_status = 0;
        sys_info.pair_succ = 0;
        sys_info.ear_l_full = 0;
        sys_info.ear_r_full = 0;
        sys_info.earfull = 0;
        goto_pair_cnt = 0;
        auto_exit_cnt = 0;
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_OPEN_LID);
    } else if (newstatus == CHG_STATUS_CHARGE) {
        ear_power_check_time = 0;
        sys_info.shut_cnt = 0;
        sys_info.lid_cnt = BIT(7) | TCFG_SEND_CLOSE_LID_MAX;
        sys_info.charge = 0;//�ȹر�,�Ⱥϸ������Ŵ���ѹ
        sys_info.ear_l_full = 0;
        sys_info.ear_r_full = 0;
        sys_info.earfull = 0;
        sys_info.force_charge = CHGBOX_FORCE_CHARGE_TIMES;
        sys_auto_shutdown_disable();
        if (sys_info.current_limit == 0) {
            chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_CLOSE_LID);
        }
    } else if (newstatus == CHG_STATUS_LOWPOWER) {
        sys_info.shut_cnt = 0;
        sys_info.lid_cnt = 0;
        if (sys_info.status[USB_DET] == STATUS_ONLINE) {
            sys_auto_shutdown_disable();
            chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_USB_IN);
        } else {
            sys_auto_shutdown_enable();
            chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_LOWPOWER);
        }

        if (!sys_info.earfull) {
            sys_info.lid_cnt = BIT(7) | TCFG_SEND_CLOSE_LID_MAX;
        }
#if TCFG_APP_BT_EN
        app_task_switch_to(APP_IDLE_TASK);
        bt_auto_off_cnt = 0;
#endif
    }
    log_info("chargebbox newstatus:%d\n", newstatus);
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��ȡ�Զ�������״̬,�Ƿ�ͬʱ����
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
u8 get_tws_ear_status(void)
{
    return (ear_info.online[EAR_L] && ear_info.online[EAR_R]);
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��ȡ�ֵĺϸ�״̬
   @param    ��
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
u8 get_chgbox_lid_status(void)
{
    return (sys_info.status[LID_DET] == STATUS_ONLINE);
}


/*------------------------------------------------------------------------------------*/
/**@brief    �ֿ��Ƴ�ʼ������
   @param    ��
   @return   ��
   @note     �����ϵ�״̬ѡ����뿪��ͨ�š��͵�����ϸǳ��ģʽ����ʼ��һЩ������
*/
/*------------------------------------------------------------------------------------*/
void charge_box_ctrl_init(void)
{
    //���IC��ʼ���ɹ�,��������(��ѹ�͵�ԭ��
    if (!sys_info.init_ok) {
        log_error("chargeIc not ok, need softoff!\n");
        chgbox_enter_soft_power_off();
        return;
    }

    sys_info.power_on = 0;
    sys_info.shut_cnt = 0;
    sys_info.pair_status = 0;
    sys_info.pair_succ = 0;
    sys_info.charge = 0;//�ȹر�,�Ⱥϸ������Ŵ���ѹ
    sys_info.ear_l_full = 0;
    sys_info.ear_r_full = 0;
    sys_info.earfull = 0;

    goto_pair_cnt = 0;
    auto_exit_cnt = 0;

    ///����ģʽ�ж�
    if (sys_info.status[LID_DET] == STATUS_ONLINE) {
        sys_info.chgbox_status = CHG_STATUS_COMM;                   //����
        sys_auto_shutdown_disable();
        if (sys_info.wireless_wakeup == 0) {
            chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_OPEN_LID);
        }
    } else {
        if (sys_info.lowpower_flag) {
            sys_info.chgbox_status = CHG_STATUS_LOWPOWER;           //�͵���
            sys_info.lid_cnt = 0;
            if (sys_info.status[USB_DET] == STATUS_ONLINE) {
                sys_auto_shutdown_disable();
                chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_USB_IN);
            } else {
                sys_auto_shutdown_enable();
                if (sys_info.wireless_wakeup == 0) {
                    chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_LOWPOWER);
                }
            }
        } else {
            sys_info.chgbox_status = CHG_STATUS_CHARGE;            //�ϸǳ��
            ear_power_check_time = 0;
            sys_info.lid_cnt = BIT(7) | TCFG_SEND_CLOSE_LID_MAX;
            sys_info.force_charge = CHGBOX_FORCE_CHARGE_TIMES;
            sys_auto_shutdown_disable();
            if (sys_info.wireless_wakeup == 0) {
                chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_POWER);
            }
        }
    }

    chgbox_ui_set_power_on(1);//ui�ϵ��־

    if (sys_info.status[USB_DET] == STATUS_ONLINE) {
        app_chargebox_event_to_user(CHGBOX_EVENT_USB_IN);
    }
    log_info("chgbox_poweron_status:%d\n", sys_info.chgbox_status);
    sys_info.wireless_wakeup = 0;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ܳ��ֳ�ʼ������
   @param    ��
   @return   ��
   @note     ���ic�����������������߳䡢lighting���֡�ui�����̿��Ƶĳ�ʼ��
*/
/*------------------------------------------------------------------------------------*/
void chgbox_init_app(void)
{
    //ע�⣺��ǰ��ʼ�������ݷ����� __initcall(chargebox_advanced_init);
    chargeIc_init();
    chargebox_det_init();
#if (TCFG_WIRELESS_ENABLE)
    wireless_init_api();
#endif
#if (TCFG_HANDSHAKE_ENABLE)
    chgbox_handshake_init();
#endif
#if TCFG_CHARGE_BOX_UI_ENABLE
    chgbox_ui_manage_init();
#endif
    charge_box_ctrl_init();
}

#endif
