#ifndef __STREAM_SYNC_H__
#define __STREAM_SYNC_H__

#include "system/includes.h"
#include "media/includes.h"

struct stream_sync_info {
    u16 i_sr;
    u16 o_sr;
    u8	ch_num;
    int begin_per;		// ��ʼ�ٷֱ�
    int top_per;		// ���ٷֱ�
    int bottom_per;		// ��С�ٷֱ�
    u8 inc_step;		// ÿ�ε������Ӳ���
    u8 dec_step;		// ÿ�ε������ٲ���
    u8 max_step;		// ����������
    void *priv;			// get_total,get_size˽�о��
    int (*get_total)(void *priv);	// ��ȡbuf�ܳ�
    int (*get_size)(void *priv);	// ��ȡbuf���ݳ���
};

struct __stream_sync_cb {
    void *priv;			// get_total,get_size˽�о��
    int (*get_total)(void *priv);	// ��ȡbuf�ܳ�
    int (*get_size)(void *priv);	// ��ȡbuf���ݳ���
};


struct __stream_sync {
    s16 *out_buf;
    int out_points;
    int out_total;
    u16 sample_rate;
    struct __stream_sync_cb cb;
    struct audio_buf_sync_hdl sync;
    struct audio_stream_entry entry;
};

struct __stream_sync *stream_sync_open(struct stream_sync_info *info, u8 always);
void stream_sync_close(struct __stream_sync **hdl);
void stream_sync_resume(struct __stream_sync *hdl);

#endif// __STREAM_SYNC_H__

