
#ifndef _AUDIO_DEC_PC_H_
#define _AUDIO_DEC_PC_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "media/audio_decoder.h"

// pc�������¿�ʼ
int uac_dec_restart(int magic);
// ����pc�������¿�ʼ����
int uac_dec_push_restart(void);

#endif /* TCFG_APP_PC_EN */

