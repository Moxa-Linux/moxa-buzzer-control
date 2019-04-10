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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/file.h>
#include <pthread.h>
#include <errno.h>
#include <json-c/json.h>
#include <moxa/mx_gpio.h>
#include <moxa/mx_errno.h>

#include "mx_buzzer.h"

#define CONF_FILE "/etc/moxa-configs/moxa-buzzer-control.json"
#define CONF_VER_SUPPORTED "1.0.*"
#define MAX_FILEPATH_LEN 256	/* reserved length for file path */

struct buzzer_struct {
	int gpio_num;
	pthread_t thread;
	int is_playing;
	unsigned long duration;
};

static int lib_initialized;
static struct json_object *config;
static struct buzzer_struct buzzer;
extern char mx_errmsg[256];

/*
 * json-c utilities
 */

static inline int obj_get_obj(struct json_object *obj, char *key, struct json_object **val)
{
	if (!json_object_object_get_ex(obj, key, val)) {
		sprintf(mx_errmsg, "json-c: can\'t get key: \"%s\"", key);
		return -1;
	}
	return 0;
}

static int obj_get_int(struct json_object *obj, char *key, int *val)
{
	struct json_object *tmp;

	if (obj_get_obj(obj, key, &tmp) < 0)
		return -1;

	*val = json_object_get_int(tmp);
	return 0;
}

static int obj_get_str(struct json_object *obj, char *key, const char **val)
{
	struct json_object *tmp;

	if (obj_get_obj(obj, key, &tmp) < 0)
		return -1;

	*val = json_object_get_string(tmp);
	return 0;
}

static int obj_get_arr(struct json_object *obj, char *key, struct array_list **val)
{
	struct json_object *tmp;

	if (obj_get_obj(obj, key, &tmp) < 0)
		return -1;

	*val = json_object_get_array(tmp);
	return 0;
}

static int arr_get_obj(struct array_list *arr, int idx, struct json_object **val)
{
	if (arr == NULL || idx >= arr->length) {
		sprintf(mx_errmsg, "json-c: can\'t get index: %d", idx);
		return -1;
	}

	*val = array_list_get_idx(arr, idx);
	return 0;
}

static int arr_get_int(struct array_list *arr, int idx, int *val)
{
	struct json_object *tmp;

	if (arr_get_obj(arr, idx, &tmp) < 0)
		return -1;

	*val = json_object_get_int(tmp);
	return 0;
}

static int arr_get_str(struct array_list *arr, int idx, const char **val)
{
	struct json_object *tmp;

	if (arr_get_obj(arr, idx, &tmp) < 0)
		return -1;

	*val = json_object_get_string(tmp);
	return 0;
}

static int arr_get_arr(struct array_list *arr, int idx, struct array_list **val)
{
	struct json_object *tmp;

	if (arr_get_obj(arr, idx, &tmp) < 0)
		return -1;

	*val = json_object_get_array(tmp);
	return 0;
}

/*
 * static functions
 */

static int check_config_version_supported(const char *conf_ver)
{
	int cv[2], sv[2];

	if (sscanf(conf_ver, "%d.%d.%*s", &cv[0], &cv[1]) < 0) {
		sprintf(mx_errmsg, "sscanf: %s: %s", conf_ver, strerror(errno));
		return E_SYSFUNCERR;
	}

	if (sscanf(CONF_VER_SUPPORTED, "%d.%d.%*s", &sv[0], &sv[1]) < 0) {
		sprintf(mx_errmsg, "sscanf: %s: %s", CONF_VER_SUPPORTED, strerror(errno));
		return E_SYSFUNCERR;
	}

	if (cv[0] != sv[0] || cv[1] != sv[1]) {
		sprintf(mx_errmsg, "Config version not supported, need to be %s", CONF_VER_SUPPORTED);
		return E_UNSUPCONFVER;
	}
	return E_SUCCESS;
}

static void *wait_and_stop(void *arg)
{
	unsigned long waiting_time = *((int *) arg);
	time_t ts_start, ts;

	ts_start = time(NULL);
	do {
		sleep(1);
		ts = time(NULL);
	} while ((ts - ts_start) < waiting_time);

	if (mx_gpio_set_value(buzzer.gpio_num, GPIO_VALUE_LOW) < 0) {
		fprintf(stderr, "failed to stop buzzer\n");
		return NULL;
	}

	buzzer.is_playing = 0;
	return NULL;
}

/*
 * APIs
 */

int mx_buzzer_init(void)
{
	int ret;
	const char *conf_ver;

	if (lib_initialized)
		return E_SUCCESS;

	config = json_object_from_file(CONF_FILE);
	if (config == NULL) {
		sprintf(mx_errmsg, "json-c: load file %s failed", CONF_FILE);
		return E_CONFERR;
	}

	if (obj_get_str(config, "CONFIG_VERSION", &conf_ver) < 0)
		return E_CONFERR;

	ret = check_config_version_supported(conf_ver);
	if (ret < 0)
		return ret;

	if (obj_get_int(config, "GPIO_NUM", &buzzer.gpio_num) < 0)
		return E_CONFERR;

	if (!mx_gpio_is_exported(buzzer.gpio_num)) {
		ret = mx_gpio_export(buzzer.gpio_num);
		if (ret < 0)
			return ret;
	}

	ret = mx_gpio_set_direction(buzzer.gpio_num, GPIO_DIRECTION_OUT);
	if (ret < 0)
		return ret;

	buzzer.is_playing = 0;
	lib_initialized = 1;
	return E_SUCCESS;
}

int mx_buzzer_play_sound(unsigned long duration)
{
	int ret;

	if (!lib_initialized) {
		sprintf(mx_errmsg, "Library is not initialized");
		return E_LIBNOTINIT;
	}

	if (buzzer.is_playing) {
		sprintf(mx_errmsg, "Buzzer is already playing");
		return E_BUZZER_PLAYING;
	}

	if (duration > 60) {
		sprintf(mx_errmsg, "Duration out of range: %ld", duration);
		return E_INVAL;
	}

	ret = mx_gpio_set_value(buzzer.gpio_num, GPIO_VALUE_HIGH);
	if (ret < 0)
		return ret;

	buzzer.duration = duration;
	if (buzzer.duration != DURATION_KEEP) {
		if (pthread_create(&buzzer.thread, NULL, wait_and_stop,
			&buzzer.duration) < 0) {
			sprintf(mx_errmsg, "create thread: %s", strerror(errno));
			return E_SYSFUNCERR;
		}
	}

	buzzer.is_playing = 1;
	return E_SUCCESS;
}

int mx_buzzer_stop_sound(void)
{
	int ret;

	if (!lib_initialized) {
		sprintf(mx_errmsg, "Library is not initialized");
		return E_LIBNOTINIT;
	}

	if (!buzzer.is_playing)
		return E_SUCCESS;

	if (buzzer.duration != DURATION_KEEP)
		pthread_cancel(buzzer.thread);

	ret = mx_gpio_set_value(buzzer.gpio_num, GPIO_VALUE_LOW);
	if (ret < 0)
		return ret;

	buzzer.is_playing = 0;
	return E_SUCCESS;
}
