
#ifndef _ENCODE_WRITE_H
#define _ENCODE_WRITE_H

#include "app_config.h"
#include "system/includes.h"
#include "system/os/os_cpu.h"
#include "system/fs/fs.h"
#include "dev_manager.h"


enum {
    ENC_WRITE_FILE_EVT_WRITE_ERR = 0x10,
    ENC_WRITE_FILE_EVT_FILE_CLOSE,	// 0-err, ��0-��ʼ�غ�

    ENC_WRITE_FLASH_EVT_WRITE_ERR = 0x20,
    ENC_WRITE_FLASH_EVT_OUTOF_LEN,
};

struct audio_enc_write_input {
    int (*get)(void *, s16 **frame, u16 frame_len);
    void (*put)(void *, s16 *frame);
};

//////////////////////////////////////////////////////////////////////////////
int enc_write_file_resume(void *hdl);	// ���øú�������д�ļ�

void enc_write_file_close(void *hdl);
void *enc_write_file_open(char *logo, const char *folder, const char *filename);

int enc_write_file_start(void *hdl);
void enc_write_file_stop(void *hdl, u32 delay_ms); // delay_ms:��ʱ�ȴ����

// ¼����������Ҫ����дͷ������
void enc_write_file_set_head_handler(void *hdl, int (*set_head)(void *, char **head), void *set_head_hdl);
void enc_write_file_set_evt_handler(void *hdl, void (*evt_cb)(void *, int, int), void *evt_hdl);
void enc_write_file_set_input(void *hdl, struct audio_enc_write_input *input, void *input_hdl, u32 input_frame_len);

int get_enc_file_len(void *hdl);
// cut_size:�����󿳵���β������
// limit_size:�ļ�С�ڸó���ʱ���������ļ�
void enc_write_file_set_limit(void *hdl, u32 cut_size, u32 limit_size);


//////////////////////////////////////////////////////////////////////////////
int enc_write_flash_resume(void *hdl);	// ���øú�������д

void enc_write_flash_close(void *hdl);
void *enc_write_flash_open(const char *dev_name, void *arg, u32 addr_start, u32 max_len);

int enc_write_flash_start(void *hdl);
void enc_write_flash_stop(void *hdl, u32 delay_ms); // delay_ms:��ʱ�ȴ����

// ¼����������Ҫ����дͷ������
void enc_write_flash_set_head_handler(void *hdl, int (*set_head)(void *, char **head), void *set_head_hdl);
void enc_write_flash_set_evt_handler(void *hdl, void (*evt_cb)(void *, int, int), void *evt_hdl);
void enc_write_flash_set_input(void *hdl, struct audio_enc_write_input *input, void *input_hdl, u32 input_frame_len);

// cut_size:�����󿳵���β������
// limit_size:�ļ�С�ڸó���ʱ���������ļ�
void enc_write_flash_set_limit(void *hdl, u32 cut_size, u32 limit_size);

void last_enc_file_codeing_type_save(u32 type);
int last_enc_file_path_get(char path[64]);


void *get_wfil_head_hdl(void *enc_whdl);
FILE *get_wfil_file(void *enc_whdl);

#endif

