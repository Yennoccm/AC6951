#include "typedef.h"
#include "asm/pwm_led.h"
#include "system/includes.h"
#include "chgbox_ctrl.h"
#include "chargeIc_manage.h"
#include "chgbox_ui.h"
#include "app_config.h"

//ʹ��PEMLED�Ƶ�,֧�ֵ͹���

#if (TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[CHGBOXUI]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#if (TCFG_CHARGE_BOX_UI_ENABLE && TCFG_CHGBOX_UI_PWMLED)

#define CFG_LED_LIGHT						200  	//10 ~ 500, ֵԽ��, ����Խ��

#define CFG_SINGLE_FAST_FLASH_FREQ			500		//LED���������ٶ�, ms��˸һ��(100 ~ 1000)
#define CFG_SINGLE_FAST_LIGHT_TIME 			150  	//���ƿ�����������ʱ��, ��λms

#define CFG_SINGLE_SLOW_FLASH_FREQ			1000	//LED���������ٶ�, ms��˸һ��(1000 ~ 20000)
#define CFG_SINGLE_SLOW_LIGHT_TIME 			300  	//����������������ʱ��, ��λms
/***************** LED0/LED1����ÿ��5S����ʱ, �ɹ����ڲ��� ********************/
#define CFG_LED_5S_FLASH_LIGHT_TIME			100		//LED 5S ��˸ʱ��������ʱ��, ��λms
/***************** ����ģʽ���ò���, �ɹ����ڲ��� ********************/
#define CFG_LED_SLOW_BREATH_TIME			1000	//����ʱ����->��->��, ��λms
#define CFG_LED_FAST_BREATH_TIME			500		//����ʱ����->��->��, ��λms
#define CFG_LED_BREATH_BRIGHT 				300		//��������, ��Χ: 0 ~ 500
#define CFG_LED_BREATH_BLINK_TIME 			100		//�����ʱ, ��λms

const pwm_led_on_para pwm_led_on_para_table[] = {
    /*
    u16 led0_bright;//led0_bright, LED0����: 0 ~ 500
    u16 led1_bright;//led1_bright, LED1����: 0 ~ 500
    */
    {CFG_LED_LIGHT, CFG_LED_LIGHT},//PWM_LED_ON	��
};

const pwm_led_one_flash_para pwm_led_one_flash_para_table[] = {
    /*
    u16 led0_bright;//led0_bright: led0����(0 ~ 500),
    u16 led1_bright;//led1_bright: led1����(0 ~ 500),
    u32 period;//period: ��������(ms), ����ms��һ��(100 ~ 20000), 100ms - 20S,
    u32 start_light_time;//start_light_time: �������п�ʼ���Ƶ�ʱ��, -1: �����������, Ĭ����-1����,
    u32 light_time;//light_time: ��������ʱ��,
    */
    {CFG_LED_LIGHT, CFG_LED_LIGHT, CFG_SINGLE_SLOW_FLASH_FREQ, -1, CFG_SINGLE_SLOW_LIGHT_TIME},//PWM_LED_SLOW_FLASH   ����
    {CFG_LED_LIGHT, CFG_LED_LIGHT, CFG_SINGLE_FAST_FLASH_FREQ, -1, CFG_SINGLE_FAST_LIGHT_TIME},//PWM_LED_FAST_FLASH   ����
    {CFG_LED_LIGHT, CFG_LED_LIGHT, 5000, 10, CFG_LED_5S_FLASH_LIGHT_TIME},//PWM_LED_ONE_FLASH_5S  5������1��
};

const pwm_led_double_flash_para pwm_led_double_flash_para_table[] = {
    /*
    u16 led0_bright;//led0_bright: led0����,
    u16 led1_bright;//led1_bright: led1����,
    u32 period;//period: ��������(ms), ����ms��һ��
    u32 first_light_time;//first_light_time: ��һ�����Ƴ���ʱ��,
    u32 gap_time;//gap_time: ��������ʱ����,
    u32 second_light_time;//second_light_time: �ڶ������Ƴ���ʱ��,
    */
    {CFG_LED_LIGHT, CFG_LED_LIGHT, 5000, 100, 200, 100}, //PWM_LED_DOUBLE_FLASH_5S	5����������
};

const pwm_led_breathe_para pwm_led_breathe_para_table[] = {
    /*
    u16 breathe_time;//breathe_time: ��������(��->����->��), ���÷�Χ: 500ms����;
    u16 led0_bright;//led0_bright: led0����������������(0 ~ 500);
    u16 led1_bright;//led1_bright: led1����������������(0 ~ 500);
    u32 led0_light_delay_time;//led0_light_delay_time: led0���������ʱ(0 ~ 100ms);
    u32 led1_light_delay_time;//led1_light_delay_time: led1���������ʱ(0 ~ 100ms);
    u32 led_blink_delay_time;//led_blink_delay_time: led0��led1�����ʱ(0 ~ 20000ms), 0 ~ 20S;
    */
    {CFG_LED_SLOW_BREATH_TIME, CFG_LED_BREATH_BRIGHT, CFG_LED_BREATH_BRIGHT, 0, 0, CFG_LED_BREATH_BLINK_TIME},//PWM_LED_BREATHE  ��������ģʽ
    {CFG_LED_FAST_BREATH_TIME, CFG_LED_BREATH_BRIGHT, CFG_LED_BREATH_BRIGHT, 0, 0, CFG_LED_BREATH_BLINK_TIME},//PWM_LED_BREATHE  �������ģʽ
};

static void chgbox_led_set_enable(u8 gpio)
{
    gpio_set_pull_up(gpio, 1);
    gpio_set_pull_down(gpio, 1);
    gpio_set_die(gpio, 1);
    gpio_set_output_value(gpio, 1);
    gpio_set_direction(gpio, 0);
}

static void chgbox_led_set_disable(u8 gpio)
{
    gpio_set_pull_down(gpio, 0);
    gpio_set_pull_up(gpio, 0);
    gpio_direction_input(gpio);
}

static void chgbox_ui_mode_set(u8 display, u8 sub)
{
    pwm_led_para para = {0};
    if (display == PWM_LED_NULL) {
        return;
    }
    switch (display) {
//�Ƴ���
    case PWM_LED0_OFF:
        break;
//�Ƴ���
    case PWM_LED0_ON:
        para.on = pwm_led_on_para_table[0];
        break;

//���Ƶ���
    case PWM_LED0_SLOW_FLASH:
        para.one_flash = pwm_led_one_flash_para_table[0];
        break;
    case PWM_LED0_FAST_FLASH:
        para.one_flash = pwm_led_one_flash_para_table[1];
        break;
    case PWM_LED0_ONE_FLASH_5S:
        para.one_flash = pwm_led_one_flash_para_table[2];
        break;

//����˫��
    case PWM_LED0_DOUBLE_FLASH_5S:
        para.double_flash = pwm_led_double_flash_para_table[0];
        break;

//����ģʽ
    case PWM_LED0_BREATHE:
        para.breathe = pwm_led_breathe_para_table[sub];
        break;
    }
    pwm_led_mode_set_with_para(display, para);
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���õ�Ϊ����������ģʽ
   @param    led_type:  �����
             on_off:    1-->���� 0-->����
             sp_flicker:�Ƿ���˸��־ 1-->����--�� ��--��������ȡ��һ��
             fade:      1-->������Ӧ 0-->������Ӧ
   @return   ��
   @note     �ѵ�����Ϊ ����/���� ģʽ(Ĭ��ͬʱ����������Ϊ����)
*/
/*------------------------------------------------------------------------------------*/
void chgbox_set_led_stu(u8 led_type, u8 on_off, u8 sp_flicker, u8 fade)
{
    chgbox_led_set_disable(CHG_RED_LED_IO);
    chgbox_led_set_disable(CHG_GREEN_LED_IO);
    chgbox_led_set_disable(CHG_BLUE_LED_IO);
    if (on_off) {
        chgbox_ui_mode_set(PWM_LED0_ON, 0);
    } else {
        chgbox_ui_mode_set(PWM_LED0_OFF, 0);
    }
    switch (led_type) {
    case CHG_LED_RED:
        chgbox_led_set_enable(CHG_RED_LED_IO);
        break;
    case CHG_LED_GREEN:
        chgbox_led_set_enable(CHG_GREEN_LED_IO);
        break;
    case CHG_LED_BLUE:
        chgbox_led_set_enable(CHG_BLUE_LED_IO);
        break;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ���õ�Ϊ����ģʽ
   @param    led_type:  �����
             speed_mode:��˸����ѡ������Ĭ��Ϊ�졢������
             is_bre:�Ƿ�Ϊ����ģʽ
             time:����/��˸���ٴ�
   @return   ��
   @note     �ѵ�����Ϊ����ģʽ(Ĭ��ͬʱ������Ϊ����)
*/
/*------------------------------------------------------------------------------------*/
void chgbox_set_led_bre(u8 led_type, u8 speed_mode, u8 is_bre, u16 time)
{
    chgbox_led_set_disable(CHG_RED_LED_IO);
    chgbox_led_set_disable(CHG_GREEN_LED_IO);
    chgbox_led_set_disable(CHG_BLUE_LED_IO);
    if (is_bre) {
        if (speed_mode == LED_FLASH_FAST) {
            chgbox_ui_mode_set(PWM_LED0_BREATHE, 1);
        } else {
            chgbox_ui_mode_set(PWM_LED0_BREATHE, 0);
        }
    } else {
        if (speed_mode == LED_FLASH_FAST) {
            chgbox_ui_mode_set(PWM_LED0_FAST_FLASH, 0);
        } else {
            chgbox_ui_mode_set(PWM_LED0_SLOW_FLASH, 0);
        }
    }
    switch (led_type) {
    case CHG_LED_RED:
        chgbox_led_set_enable(CHG_RED_LED_IO);
        break;
    case CHG_LED_GREEN:
        chgbox_led_set_enable(CHG_GREEN_LED_IO);
        break;
    case CHG_LED_BLUE:
        chgbox_led_set_enable(CHG_BLUE_LED_IO);
        break;
    }
}

/*------------------------------------------------------------------------------------*/
/**@brief    ������ȫ��
   @param    fade:�Ƿ���
   @return   ��
   @note     �����еĵ�����Ϊ����ģʽ
*/
/*------------------------------------------------------------------------------------*/
void chgbox_set_led_all_off(u8 fade)
{
    chgbox_led_set_disable(CHG_RED_LED_IO);
    chgbox_led_set_disable(CHG_GREEN_LED_IO);
    chgbox_led_set_disable(CHG_BLUE_LED_IO);
    chgbox_ui_mode_set(PWM_LED0_OFF, 0);
}

/*------------------------------------------------------------------------------------*/
/**@brief    ������ȫ��
   @param    fade:�Ƿ���
   @return   ��
   @note     �����еĵ�����Ϊ����ģʽ
*/
/*------------------------------------------------------------------------------------*/
void chgbox_set_led_all_on(u8 fade)
{
    chgbox_led_set_disable(CHG_RED_LED_IO);
    chgbox_led_set_disable(CHG_GREEN_LED_IO);
    chgbox_led_set_disable(CHG_BLUE_LED_IO);
    chgbox_ui_mode_set(PWM_LED0_ON, 0);
    chgbox_led_set_enable(CHG_RED_LED_IO);
    chgbox_led_set_enable(CHG_GREEN_LED_IO);
    chgbox_led_set_enable(CHG_BLUE_LED_IO);
}

/*------------------------------------------------------------------------------------*/
/**@brief    led�����Ƴ�ʼ��
   @param    ��
   @return   ��
   @note     ��ʼ��ÿ��led:��������������������������ȣ���ӦIO�ĳ�ʼ��.mc_clk�ĳ�ʼ��������
             ����pwm
*/
/*-----------------------------------------------------------------------------------*/
extern const struct led_platform_data chgbox_pwm_led_data;
void chgbox_led_init(void)
{
    chgbox_led_set_disable(CHG_RED_LED_IO);
    chgbox_led_set_disable(CHG_GREEN_LED_IO);
    chgbox_led_set_disable(CHG_BLUE_LED_IO);
    pwm_led_init(&chgbox_pwm_led_data);
}

#endif

#endif




