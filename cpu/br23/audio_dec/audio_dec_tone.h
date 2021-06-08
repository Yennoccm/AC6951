

#ifndef _AUDIO_DEC_TONE_H_
#define _AUDIO_DEC_TONE_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "media/file_decoder.h"
#include "system/includes.h"
#include "media/audio_decoder.h"
#include "application/audio_dec_app.h"
#include "tone_player.h"

// ��ʾ���������Ե��
#define TONE_DEC_STOP_NOR				0 // �������Źر�
#define TONE_DEC_STOP_BY_OTHER_PLAY		1 // ���������Ŵ�Ϲر�

/*
 * ���ڱ���file_list����
 * ԭ�����ڵ�ǰ���Ž�����������һ���յĽ���ռסres��
 * ���ⲥ��file_list��һ��ʱ��������������
 */
#define TONE_DEC_PROTECT_LIST_PLAY		1


/*
 * ��ʾ��file_list����ṹ��
 * ���β���"char **file_list"�����ж�����ļ���ֱ��file_list[n]ΪNULL
 * �� file_list[0]=123.*; file_list[1]=456.*; file_list[2]=NULL;
 * ֧���ļ������롢���Ҳ�����������루��Ҫʵ��get_sine()�ӿڻ�ȡ������
 * ���Ҳ����鲥�Ÿ�ʽʾ����file_list[n]=DEFAULT_SINE_TONE(SINE_WTONE_NORAML);
 * ֧��ѭ�����ţ�ѭ��������Ҫָ��������ʼ�������Ǵ���ѭ���������ͽ�����
 * �磺file_list[n]=TONE_REPEAT_BEGIN(-1); file_list[n+1]=456.*; file_list[n+2]=TONE_REPEAT_END();
 * ����Ҫʹ���ⲿ�Զ���������ʱ������ͨ��ʵ��*stream_handler�ӿ�������
 */
struct tone_dec_list_handle {
    struct list_head list_entry;	// ����
    u8 preemption : 1;		// ���
    u8 idx;					// ѭ���������
    u8 repeat_begin;		// ѭ��������ʼ���
    u8 dec_ok_cnt;			// �������ż���
    u16 loop;				// ѭ�����Ŵ���
    int sync_confirm_time;
    char **file_list;		// �ļ���
    const char *evt_owner;				// �¼���������
    void (*evt_handler)(void *priv, int flag);	// �¼��ص�
    void *evt_priv;						// �¼��ص�˽�о��
    void (*stream_handler)(void *priv, int event, struct audio_dec_app_hdl *);	// ���������ûص�
    void *stream_priv;						// ���������ûص�˽�о��
#if TONE_DEC_PROTECT_LIST_PLAY
    void *list_protect;
#endif
};

/*
 * ��ʾ������ṹ��
 * ����ʵ�ֶ��tone_dec_list_handle���ţ�ͨ���ṹ���е���������
 * �������������Ҳ����鲥��ʱ������Ҫʵ��*get_sine�ӿ�
 */
struct tone_dec_handle {
    struct list_head head;	// ����ͷ
    struct audio_dec_sine_app_hdl *dec_sin;		// �ļ����ž��
    struct audio_dec_file_app_hdl *dec_file;	// sine���ž��
    struct tone_dec_list_handle *cur_list;		// ��ǰ����list
    struct sin_param *(*get_sine)(u8 id, u8 *num);	// �����кŻ�ȡsine����
    OS_MUTEX mutex;		// ����
};

/*----------------------------------------------------------------------------*/
/**@brief    ������ʾ�����ž��
   @param
   @return   ��ʾ�����
   @note
*/
/*----------------------------------------------------------------------------*/
struct tone_dec_handle *tone_dec_create(void);

/*----------------------------------------------------------------------------*/
/**@brief    ����sine�����ȡ�ص�
   @param    *dec: ��ʾ�����
   @param    *get_sine: sine�����ȡ
   @return
   @note     �������������Ҳ����鲥��ʱ���������øýӿ�
*/
/*----------------------------------------------------------------------------*/
void tone_dec_set_sin_get_hdl(struct tone_dec_handle *dec, struct sin_param * (*get_sine)(u8 id, u8 *num));

/*----------------------------------------------------------------------------*/
/**@brief    ������ʾ������list���
   @param    *dec: ��ʾ�����
   @param    **file_list: �ļ���
   @param    preemption: ��ϱ��
   @param    *evt_handler: �¼��ص��ӿ�
   @param    *evt_priv: �¼��ص�˽�о��
   @param    *stream_handler: tone���������ûص�
   @param    *stream_priv: tone���������ûص�˽�о��
   @return   list���
   @note
*/
/*----------------------------------------------------------------------------*/
struct tone_dec_list_handle *tone_dec_list_create(struct tone_dec_handle *dec,
        const char **file_list,
        u8 preemption,
        void (*evt_handler)(void *priv, int flag),
        void *evt_priv,
        void (*stream_handler)(void *priv, int event, struct audio_dec_app_hdl *app_dec),
        void *stream_priv);

/*----------------------------------------------------------------------------*/
/**@brief    ��ʾ��list��ʼ����
   @param    *dec: ��ʾ�����
   @param    *dec_list: list���
   @return   true: �ɹ�
   @return   false: �ɹ�
   @note     ��ǰû�в��ţ����Ͽ�ʼ���š���ǰ�в��ţ����ص��������ȴ�����
*/
/*----------------------------------------------------------------------------*/
int tone_dec_list_add_play(struct tone_dec_handle *dec, struct tone_dec_list_handle *dec_list);

/*----------------------------------------------------------------------------*/
/**@brief    ��ʾ������ֹͣ
   @param    **ppdec: ��ʾ�����
   @param    push_event: ��ͨ��ʾ���Ƿ�������Ϣ
   @param    end_flag: ��������
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void tone_dec_stop(struct tone_dec_handle **ppdec,
                   u8 push_event,
                   u8 end_flag);

/*----------------------------------------------------------------------------*/
/**@brief    ָ����ʾ������ֹͣ
   @param    **ppdec: ��ʾ�����
   @param    push_event: ��ͨ��ʾ���Ƿ�������Ϣ
   @param    end_flag: ��������
   @return
   @note     �������ʾ�����ڲ���ֹͣ���Ų��Ҳ�����һ����������ڲ��ţ�ֻ��������ɾ��
*/
/*----------------------------------------------------------------------------*/
void tone_dec_stop_spec_file(struct tone_dec_handle **ppdec,
                             char *file_name,
                             u8 push_event,
                             u8 end_flag);


#endif /*_AUDIO_DEC_TONE_H_*/

