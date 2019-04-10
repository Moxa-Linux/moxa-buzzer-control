/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Name:
 *	MOXA Buzzer Library
 *
 * Description:
 *	Library for controling Buzzer to play or stop.
 *
 * Authors:
 *	2018	Ken CJ Chou	<KenCJ.Chou@moxa.com>
 */

#ifndef _MOXA_BUZZER_H
#define _MOXA_BUZZER_H

#define DURATION_KEEP 0

#ifdef __cplusplus
extern "C" {
#endif

extern int mx_buzzer_init(void);
extern int mx_buzzer_play_sound(unsigned long duration);
extern int mx_buzzer_stop_sound(void);

#ifdef __cplusplus
}
#endif

#endif /* _MOXA_BUZZER_H */
