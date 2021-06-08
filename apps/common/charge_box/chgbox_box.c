#include "app_config.h"
#include "system/includes.h"
#include "chgbox_box.h"
#include "user_cfg.h"
#include "device/vm.h"
#include "app_task.h"
#include "app_main.h"
#include "chargeIc_manage.h"
#include "chgbox_ctrl.h"
#include "device/chargebox.h"
#include "asm/power/p33.h"
#include "chgbox_det.h"

#if(TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[APP_CBOX]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"


#define CHGBOX_THR_NAME   "chgbox_n"
OS_MUTEX power_mutex;

//��ز�������ز�ͬ��������ͬ
//ʹ�ò�ͬ�ĵ��Ҫ���´˱���Ȼ���ݿ��ܻ᲻׼ȷ
#define POWER_TOP_LVL   4200
#define POWER_BOOT_LVL  3100
#define POWER_LVL_MAX   11

const u16  voltage_table[2][POWER_LVL_MAX] = {
    {0,              10,   20,   30,   40,   50,   60,   70,   80,   90,   100},
    {POWER_BOOT_LVL, 3600, 3660, 3720, 3780, 3840, 3900, 3950, 4000, 4050, POWER_TOP_LVL},
};

/*------------------------------------------------------------------------------------*/
/**@brief    ��ȡ���ֵ���ֵ
   @param    ��
   @return   0~100 �ĵ���ֵ
   @note     �������ݵ�ص��������0~100
*/
/*------------------------------------------------------------------------------------*/
u8 get_box_power_lvl()
{
    u16 max, min, power;
    u8 i;

    power = get_vbat_voltage();

    if (power <= POWER_BOOT_LVL) {
        return 0;
    }
    if (power >= POWER_TOP_LVL) {
        return 100;
    }

    for (i = 0; i < POWER_LVL_MAX; i++) {
        if (power < voltage_table[1][i]) {
            break;
        }
    }
    min = voltage_table[1][i - 1];
    max = voltage_table[1][i];
    return ((u8)(((power - min) * 10 / (max - min)) + voltage_table[0][i - 1]));
}

//ͨ�ŵ�ʱ���ܽ���͹���
static volatile u8 is_comm_active = 0;
static u8 comm_idle_query(void)
{
    return (!is_comm_active);
}
REGISTER_LP_TARGET(comm_lp_target) = {
    .name = "chgbox_comm",
    .is_idle = comm_idle_query,
};

/*------------------------------------------------------------------------------------*/
/**@brief    ���뷢�����ݹش���
   @param    ��
   @return   ��
   @note     ͨ��ǰҪ������ؽӿ�
*/
/*------------------------------------------------------------------------------------*/
void enter_hook(void)
{
    //���뷢������ǰ,�ȹر���ѹ���
    os_mutex_pend(&power_mutex, 0);
    if (sys_info.charge) {
        chargeIc_pwr_ctrl(0);
    }
    chargebox_api_open_port(EAR_L);
    chargebox_api_open_port(EAR_R);
    is_comm_active = 1;
}

/*------------------------------------------------------------------------------------*/
/**@brief    �������ݺ�ָ�����
   @param    ��
   @return   ��
   @note     ����״̬�ָ�ͨ�ſ�
*/
/*------------------------------------------------------------------------------------*/
void exit_hook(void)
{
    //�˳��������ݺ�,�Ƿ���Ҫ����ѹ���
    if (sys_info.charge && (sys_info.temperature_limit == 0)) {
        chargebox_check_output_short();
        chargebox_api_shutdown_port(EAR_L);
        chargebox_api_shutdown_port(EAR_R);
        if (sys_info.current_limit == 0) {
            chargeIc_pwr_ctrl(1);
        }
    } else {
        chargebox_api_close_port(EAR_L);
        chargebox_api_close_port(EAR_R);
    }
    is_comm_active = 0;
    os_mutex_post(&power_mutex);
}

/*------------------------------------------------------------------------------------*/
/**@brief    ����Ϣ��ͨ�Ŵ����߳�
   @param    msg:��������Ϣ
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void app_chargebox_send_mag(int msg)
{
    //��������Ҫ����,�Զ��ػ���ʱ����λ
    sys_info.life_cnt = 0;
    os_taskq_post_msg(CHGBOX_THR_NAME, 1, msg);
}

/*------------------------------------------------------------------------------------*/
/**@brief    �����¼����ͺ���
   @param    event:�������¼�
   @return   ��
   @note
*/
/*------------------------------------------------------------------------------------*/
void app_chargebox_event_to_user(u8 event)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)DEVICE_EVENT_FROM_CHARGEBOX;
    e.u.dev.event = event;
    sys_event_notify(&e);
}

/*------------------------------------------------------------------------------------*/
/**@brief    �������߼��
   @param    ret_l:��������Ӧ��־
             ret_r:�Ҷ������Ӧ��־
   @return   ��
   @note     ��Ⲣ����Ӧ�ĳ�����¼�
*/
/*------------------------------------------------------------------------------------*/
void app_chargebox_api_check_online(bool ret_l, bool ret_r)
{
    if (ret_l == TRUE) {
        if (ear_info.online[EAR_L] == 0) {
            //���¼�,�������
            app_chargebox_event_to_user(CHGBOX_EVENT_EAR_L_ONLINE);
        }
        ear_info.online[EAR_L] = TCFG_EAR_OFFLINE_MAX;
    } else {
        if (ear_info.online[EAR_L]) {
            ear_info.online[EAR_L]--;
            if (ear_info.online[EAR_L] == 0) {
                ear_info.power[EAR_L] = 0xff;
                //���¼�,�������
                app_chargebox_event_to_user(CHGBOX_EVENT_EAR_L_OFFLINE);
            }
        }
    }
    if (ret_r == TRUE) {
        if (ear_info.online[EAR_R] == 0) {
            //���¼�,�������
            app_chargebox_event_to_user(CHGBOX_EVENT_EAR_R_ONLINE);
        }
        ear_info.online[EAR_R] = TCFG_EAR_OFFLINE_MAX;
    } else {
        if (ear_info.online[EAR_R]) {
            ear_info.online[EAR_R]--;
            if (ear_info.online[EAR_R] == 0) {
                ear_info.power[EAR_R] = 0xff;
                //���¼�,�������
                app_chargebox_event_to_user(CHGBOX_EVENT_EAR_R_OFFLINE);
            }
        }
    }

    if ((ear_info.online[EAR_L] == 0) && (ear_info.online[EAR_R] == 0)) {
        chargebox_api_reset();
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���� �����ػ� ָ��
   @param    ��
   @return   TRUE:���ͳɹ�  FALSE:����ʧ��
   @note
*/
/*------------------------------------------------------------------------------------*/
u8 app_chargebox_api_send_shutdown(void)
{
    u8 ret0, ret1;
    enter_hook();
    ret0 = chargebox_send_shut_down(EAR_L);
    ret1 = chargebox_send_shut_down(EAR_R);
    exit_hook();
    if ((ret0 == TRUE) && (ret1 == TRUE)) {
        log_debug("send shutdown ok!\n");
        return TRUE;
    } else {
        log_error("shut down, L:%d,R:%d\n", ret0, ret1);
    }
    return FALSE;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ͺϸ�ָ��
   @param    ��
   @return   ��
   @note     ��ָ��������
*/
/*------------------------------------------------------------------------------------*/
u8 app_chargebox_api_send_close_cid(void)
{
    u8 online_cnt = 0;;
    u8 ret0, ret1;
    if (ear_info.online[EAR_L]) {
        online_cnt += 1;
    }
    if (ear_info.online[EAR_R]) {
        online_cnt += 1;
    }
    enter_hook();
    ret0 = chargebox_send_close_cid(EAR_L, online_cnt);
    ret1 = chargebox_send_close_cid(EAR_R, online_cnt);
    exit_hook();
    app_chargebox_api_check_online(ret0, ret1);
    if ((ret0 == TRUE) && (ret1 == TRUE)) {
        log_debug("send close CID ok\n");
        return TRUE;
    } else {
        log_error("LID close, L:%d,R:%d\n", ret0, ret1);
    }
    return FALSE;
}

//������ҡ�������ַ
static u8 adv_addr_tmp_buf[3][6];
/*------------------------------------------------------------------------------------*/
/**@brief    ��¼��/�Ҷ�����ַ
   @param    lr:1--�����  0--�Ҷ���
             inbuf:�����ַ��bufָ��
   @return   ��
   @note     ��¼��ַ����Ӧ��buf
*/
/*------------------------------------------------------------------------------------*/
void get_lr_adr_cb(u8 lr, u8 *inbuf)
{
    if (lr) {
        memcpy(&adv_addr_tmp_buf[1][0], inbuf, 6);
    } else {
        memcpy(&adv_addr_tmp_buf[0][0], inbuf, 6);
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    �޸Ĺ㲥��ַ�ص�
   @param
   @return   ��
   @note     �޸Ĺ㲥��ַ�����ѹ㲥��ַ���µ���Ӧ��buf
*/
/*------------------------------------------------------------------------------------*/
void exchange_addr_succ_cb(void)
{
    u8 i;
    for (i = 0; i < 6; i++) {
        adv_addr_tmp_buf[2][i] = adv_addr_tmp_buf[0][i] + adv_addr_tmp_buf[1][i];
    }
    sys_info.chg_addr_ok = 1;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��ȡ�㲥��ַ
   @param
   @return   NULL:�޹㲥��ַ  ���������������ַ
   @note
*/
/*------------------------------------------------------------------------------------*/
u8 *get_chargebox_adv_addr(void)
{
    if (sys_info.chg_addr_ok) {
        return &adv_addr_tmp_buf[2][0];
    } else {
        return NULL;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ��vm��ȡ�㲥��ַ
   @param    ��
   @return   TRUE:��ȡ�ɹ�  0:��ȡ������ַ
   @note     ��ȡ�ɹ��ͼ�¼�ڶ�Ӧ��������
*/
/*------------------------------------------------------------------------------------*/
u8 chgbox_addr_read_from_vm(void)
{
    if (6 == syscfg_read(CFG_CHGBOX_ADDR, &adv_addr_tmp_buf[2][0], 6)) {
        sys_info.chg_addr_ok = 1;
        log_info("Read adv addr OK:");
        put_buf(&adv_addr_tmp_buf[2][0], 6);
        return TRUE;
    } else {
        sys_info.chg_addr_ok = 0;
        log_error("Read adv addr error\n");
        return FALSE;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ֻ�ȡ��������ַɨ��
   @param    ��
   @return   1:�޲���  0:�����
   @note     û�м�⵽�㲥��ַ��chg_addr_ok==0�������ɨ�裬��ȡ�㲥��ַ(���Ҷ������ҿ���)
*/
/*------------------------------------------------------------------------------------*/
u8 chgbox_addr_save_to_vm(void)
{
    if (6 == syscfg_write(CFG_CHGBOX_ADDR, &adv_addr_tmp_buf[2][0], 6)) {
        log_info("Write adv addr OK!\n");
        return TRUE;
    } else {
        log_error("Write adv addr error!\n");
        return FALSE;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ֻ�ȡ��������ַɨ��
   @param    ��
   @return   1:�޲���  0:�����
   @note     û�м�⵽�㲥��ַ��chg_addr_ok==0�������ɨ�裬��ȡ�㲥��ַ(���Ҷ������ҿ���)
*/
/*------------------------------------------------------------------------------------*/
u8 chgbox_adv_addr_scan(void)
{
    //ע�⣺���ֶ���Ҫ�յ�open_powerָ��������������,�����õ���ַ
    static u8 caa_cnt = 0;
    if (!sys_info.chg_addr_ok) {
        if (ear_info.online[EAR_L] && ear_info.online[EAR_R] && sys_info.chgbox_status == CHG_STATUS_COMM) {
            caa_cnt++;
            if (caa_cnt < 8) {
                //0~4��ֱ�ӷ���1���������������������200Ms������open_powerָ��
                if (caa_cnt == 5) { //������ַ(���)
                    log_debug("ss-0\n");
                    app_chargebox_send_mag(CHGBOX_MSG_SEND_PAIR);
                }
                if (caa_cnt > 4) {
                    log_debug("ss-1\n");
                    return 0;//�����ָ��󲻷���������,�������
                }
            } else {
                caa_cnt = 0; //����ò�����ַ(chg_addr_ok==0)����0ѭ��
            }
        }
    }
    log_debug("ss-2\n");
    return 1;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ֽ�����ַapp
   @param    ��
   @return   ������ַ�Ƿ�ɹ�
   @note     ������Ҷ������ߣ��򽻻���ַ���������ɹ��ͼ�¼�µ�ַ
*/
/*------------------------------------------------------------------------------------*/
u8 app_chargebox_api_exchange_addr(void)
{
    u8 ret = FALSE;
    if (ear_info.online[EAR_L] && ear_info.online[EAR_R]) {
        enter_hook();
        ret = chargebox_exchange_addr(get_lr_adr_cb, exchange_addr_succ_cb);
        exit_hook();
    }

    if (ret) {
        //������ַ�ɹ����¼��ַ
        chgbox_addr_save_to_vm();
    }
    return ret;
}

/*------------------------------------------------------------------------------------*/
/**@brief    ˽����������
 * @param    ��
 * @return   ��
 * @note     1���Զ������������0xC0��0xFE֮��
 *           2�����ͳ��ȱ���С��32�ֽ�
 *           3����������������lr_buf ������lr_len
 */
/*------------------------------------------------------------------------------------*/
void app_chargebox_api_send_cmd_demo(void)
{
    extern u8 lr_buf[2][32];
    extern u8 lr_len[2];
    u8 buf[3];
    buf[0] = CMD_USER;
    buf[1] = 0x12;
    buf[2] = 0x34;
    enter_hook();
    if (chargebox_api_write_read(EAR_L, buf, 3, 4) == TRUE) {
        //�����лظ�����
        log_dump(lr_buf[EAR_L], lr_len[EAR_L]);
    }
    if (chargebox_api_write_read(EAR_R, buf, 3, 4) == TRUE) {
        //�����лظ�����
        log_dump(lr_buf[EAR_R], lr_len[EAR_R]);
    }
    exit_hook();
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���ַ��͵��������ȡ����
   @param    flag:1 �ϸ�   0 ����
   @return   ��
   @note     ���Ͳֵ��������̬��A�����ĵ��� ��B��������ȡB�����ĵ��������ݻ�Ӧ�ж�B�Ƿ����� (��������������)
*/
/*------------------------------------------------------------------------------------*/
void app_chargebox_api_send_power(u8 flag)
{
    u8 power;
    u8 ret0, ret1, is_charge = 0;
    power = get_box_power_lvl();//��ȡ�ֵĵ���
    if (sys_info.status[USB_DET] == STATUS_ONLINE) {
        is_charge = 1;
    }
    enter_hook();
    if (flag == 0) {
        ret0 = chargebox_send_power_open(EAR_L, power, is_charge, ear_info.power[EAR_R]);
        ret1 = chargebox_send_power_open(EAR_R, power, is_charge, ear_info.power[EAR_L]);
    } else {
        ret0 = chargebox_send_power_close(EAR_L, power, is_charge, ear_info.power[EAR_R]);
        ret1 = chargebox_send_power_close(EAR_R, power, is_charge, ear_info.power[EAR_L]);
    }
    exit_hook();
    app_chargebox_api_check_online(ret0, ret1);
    if (ret0 == TRUE) {
        ear_info.power[EAR_L] = chargebox_get_power(EAR_L);
        log_info("Ear_L:%d_%d", ear_info.power[EAR_L]&BIT(7) ? 1 : 0, ear_info.power[EAR_L] & (~BIT(7)));
    } else {
        /* log_error("Can't got L\n"); */
    }
    if (ret1 == TRUE) {
        ear_info.power[EAR_R] = chargebox_get_power(EAR_R);
        log_info("Ear_R:%d_%d", ear_info.power[EAR_R]&BIT(7) ? 1 : 0, ear_info.power[EAR_R] & (~BIT(7)));
    } else {
        /* log_error("Can't got R\n"); */
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ����ͨ���߳�
   @param    priv:��չ����
   @return   ��
   @note     ���̸߳�������Ҷ���������
*/
/*------------------------------------------------------------------------------------*/
void app_chargebox_task_handler(void *priv)
{
    int msg[32];
    log_info("data thread running! \n");

    while (1) {
        if (os_task_pend("taskq", msg, ARRAY_SIZE(msg)) != OS_TASKQ) {
            continue;
        }
        switch (msg[1]) {
        case CHGBOX_MSG_SEND_POWER_OPEN:
            app_chargebox_api_send_power(0);
            break;
        case CHGBOX_MSG_SEND_POWER_CLOSE:
            app_chargebox_api_send_power(1);
            break;
        case CHGBOX_MSG_SEND_CLOSE_LID:
            app_chargebox_api_send_close_cid();
            break;
        case CHGBOX_MSG_SEND_SHUTDOWN:
            app_chargebox_api_send_shutdown();
            break;
        case CHGBOX_MSG_SEND_PAIR:
            log_info("CHANGE ear ADDR\n");
            if (app_chargebox_api_exchange_addr() == TRUE) {
                sys_info.pair_succ = 1;
            } else {
                log_error("pair_fail\n");
            }
            break;
        default:
            log_info("default msg: %d\n", msg[1]);
            break;
        }
    }
}

CHARGEBOX_PLATFORM_DATA_BEGIN(chargebox_data)
.L_port = TCFG_CHARGEBOX_L_PORT,
 .R_port = TCFG_CHARGEBOX_R_PORT,
  CHARGEBOX_PLATFORM_DATA_END()

  /*------------------------------------------------------------------------------------*/
  /**@brief    ������ǰ��ʼ������
     @param    priv:��չ����
     @return   ��
     @note
  */
  /*------------------------------------------------------------------------------------*/
  void app_chargebox_timer_handle(void *priv)
{
    static u8 ms200_cnt = 0;
    static u8 ms500_cnt = 0;

    ms200_cnt++;
    if (ms200_cnt >= 2) {
        ms200_cnt = 0;
        app_chargebox_event_to_user(CHGBOX_EVENT_200MS);
    }
    ms500_cnt++;
    if (ms500_cnt >= 5) {
        ms500_cnt = 0;
        app_chargebox_event_to_user(CHGBOX_EVENT_500MS);
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ������ǰ��ʼ������
   @param    ��
   @return   ��
   @note     ��ȡ�㲥��ַ����ʼ��ģ�顢����ͨ���̡߳�ע����غ�����timer
*/
/*------------------------------------------------------------------------------------*/
int chargebox_advanced_init(void)
{
    chgbox_addr_read_from_vm();
    chargebox_api_init(&chargebox_data);
    task_create(app_chargebox_task_handler, NULL, CHGBOX_THR_NAME);
    os_mutex_create(&power_mutex);
    sys_timer_add(NULL, app_chargebox_timer_handle, 100);//���¼�����
    return 0;
}

__initcall(chargebox_advanced_init);

#endif
