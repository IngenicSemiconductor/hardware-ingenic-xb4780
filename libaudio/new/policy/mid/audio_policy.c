/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "audio_policy_xb47xx_mid"
//#define LOG_NDEBUG 0
//#define ALOGD LOGD
//#define POLICY_DEBUG

#ifdef POLICY_DEBUG
#define FUNC_ENTER() ALOGV("audio_policy %s:[%d]. enter.\n",__func__,__LINE__)
#else
#define FUNC_ENTER()
#endif

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cutils/log.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <system/audio_policy.h>
#include <hardware/audio_policy.h>

#include "audio_policy.h"

/* #################################################################### *\
|* private functions
\* #################################################################### */
static int isStateInCall(audio_mode_t state)
{
	if (state == AUDIO_MODE_IN_CALL || state == AUDIO_MODE_IN_COMMUNICATION)
		return 1;
	return 0;
}

static int is_in_call(struct xb47xx_audio_policy *lpol)
{
	if (lpol == NULL)
		return NO_INIT;
	return isStateInCall(lpol->ap_prop.phoneState);
}

static uint32_t get_strategy(audio_stream_type_t stream)
{
	/* stream to strategy mapping */
	switch (stream) {
		case AUDIO_STREAM_VOICE_CALL:
		case AUDIO_STREAM_BLUETOOTH_SCO:
			return STRATEGY_PHONE;
		case AUDIO_STREAM_RING:
		case AUDIO_STREAM_ALARM:
			return STRATEGY_SONIFICATION;
		case AUDIO_STREAM_NOTIFICATION:
			return STRATEGY_SONIFICATION_RESPECTFUL;
		case AUDIO_STREAM_DTMF:
			return STRATEGY_DTMF;
		default:
			return STRATEGY_MEDIA;
		case AUDIO_STREAM_ENFORCED_AUDIBLE:
			return STRATEGY_ENFORCED_AUDIBLE;
	}
}
static uint32_t check_device_mute_starategies(struct output_descriptor *output ,
		uint32_t device, int32_t delay_ms)
{
	return 0;
}
static uint32_t get_device_for_input(struct audio_policy *pol, int inputSource)
{
	audio_devices_t device = 0;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	FUNC_ENTER();
	if (pol == NULL)
		return NO_INIT;
	switch (inputSource) {

	case AUDIO_SOURCE_VOICE_COMMUNICATION:
		switch (lpol->ap_prop.forceUseConf[AUDIO_POLICY_FORCE_FOR_COMMUNICATION]) {
		case AUDIO_POLICY_FORCE_BT_SCO:
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
			if (is_in_call(lpol)) {
				device |= lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_VOICE_CALL;
			} else {
				device |=  lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_COMMUNICATION;
			}
			break;
		default:
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
			if (!device)
				device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_WIRED_HEADSET;
				if (!device)
					device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BUILTIN_MIC;
			if (is_in_call(lpol)) {
				device |= lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_VOICE_CALL;
			} else {
				device |=  lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_COMMUNICATION;
			}
			break;
		}
		break;
	case AUDIO_SOURCE_VOICE_RECOGNITION:
	case AUDIO_SOURCE_CAMCORDER:
		switch (lpol->ap_prop.forceUseConf[AUDIO_POLICY_FORCE_FOR_RECORD]) {
		case AUDIO_POLICY_FORCE_DIGITAL_DOCK:
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_AUX_DIGITAL;
			break;
		case AUDIO_POLICY_FORCE_BT_SCO:
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
			break;
		default :
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
			if (device) break;
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_WIRED_HEADSET;
			if (device) break;
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BUILTIN_MIC;
			break;
		}
		break;
	case AUDIO_SOURCE_DEFAULT:
	case AUDIO_SOURCE_MIC:
		switch (lpol->ap_prop.forceUseConf[AUDIO_POLICY_FORCE_FOR_RECORD]) {
		case AUDIO_POLICY_FORCE_BT_SCO:
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_AUX_DIGITAL;
			if (device) break; /*FIXME if sco use i2s interface open it*/
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
			break;
		default :
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_AUX_DIGITAL;
			if (device) break;
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
			if (device) break;
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_WIRED_HEADSET;
			if (device) break;
		case AUDIO_POLICY_FORCE_WIRED_ACCESSORY:
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_AUX_DIGITAL;
			if (device) break;
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BUILTIN_MIC;
			break;
		}
		break;
	case AUDIO_SOURCE_VOICE_UPLINK:
	case AUDIO_SOURCE_VOICE_DOWNLINK:
	case AUDIO_SOURCE_VOICE_CALL:
		device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
		if (!device)
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_WIRED_HEADSET;
		if (!device)
			device = lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_BUILTIN_MIC;
		device |= lpol->ap_prop.availableInputDevices & AUDIO_DEVICE_IN_VOICE_CALL;

		break;
	default :
		ALOGW("getDeviceForInputSource() invalid input source %d", inputSource);
		break;
	}

	if (device == 0)
		ALOGE("AUDIO :there is no input device.\n");
	return device;
}

static struct input_descriptor *find_current_input_desctiptor(struct input_descriptor *Desc)
{
	struct input_descriptor *ret_Desc = Desc;

	while (ret_Desc) {
		if (ret_Desc->refCount > 0)
			break;
		else
			ret_Desc = ret_Desc->next;
	}

	return ret_Desc;
}

static bool strategy_isused(struct output_descriptor *output_desc ,enum routing_strategy strategy)
{
	int i;
	struct output_descriptor *tmp = output_desc;
	if (output_desc == NULL)
		return false;
	if (tmp->refCount > 0)
		for (i = 0 ; i < AUDIO_STREAM_CNT; i++) {
			//if (tmp->oStream[i]->strategy == strategy)
			if (get_strategy((audio_stream_type_t)i) == strategy)
				if (tmp->oStream[i]->refCount > 0)
					return true;
	}

	return false;

}

static enum routing_strategy get_current_out_strategy(struct output_descriptor *output_desc)
{
	if (true == strategy_isused(output_desc,STRATEGY_ENFORCED_AUDIBLE))
		return STRATEGY_ENFORCED_AUDIBLE;
	if (true == strategy_isused(output_desc,STRATEGY_PHONE))
		return STRATEGY_PHONE;
	if (true == strategy_isused(output_desc,STRATEGY_SONIFICATION))
		return STRATEGY_SONIFICATION;
	if (true == strategy_isused(output_desc,STRATEGY_SONIFICATION_RESPECTFUL))
		return STRATEGY_SONIFICATION_RESPECTFUL;
	if (true == strategy_isused(output_desc,STRATEGY_MEDIA))
		return STRATEGY_MEDIA;
	if (true == strategy_isused(output_desc,STRATEGY_DTMF))
		return STRATEGY_DTMF;
	return STRATEGY_MEDIA;
}
static bool is_stream_active(struct xb47xx_audio_policy *lpol ,
		audio_stream_type_t stream, uint32_t delayms)
{
	struct output_descriptor *p = lpol->ap_prop.ap_outputs;
	struct timespec t;
	uint32_t systime;

	t.tv_sec = t.tv_nsec = 0;
	clock_gettime(CLOCK_MONOTONIC, &t);
	systime = t.tv_sec*1000000000LL + t.tv_nsec;
	
	while(p) {
		if (p->oStream[stream] != 0 ||
				(systime - p->stopTime[stream])/1000000  < delayms) {
			return true;
		}
		p = p->next;
	}

	return false;
}
static bool has_a2dp_device(struct xb47xx_audio_policy *lpol)
{
	return lpol->ap_prop.a2dpOutput != NULL ? true : false;
}

static bool has_usb_device(struct xb47xx_audio_policy *lpol)
{
	return lpol->ap_prop.usbOutput != NULL ? true : false;
}

static uint32_t get_device_for_output(struct audio_policy *pol ,enum routing_strategy strategy)
{
	audio_devices_t device = 0;
	audio_devices_t device2 = 0;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	FUNC_ENTER();

	if (pol == NULL)
		return NO_INIT;
	switch (strategy) {
	case STRATEGY_SONIFICATION_RESPECTFUL:
		/*while media is playing (or has recently played), use the same device*/
		if (is_in_call(lpol))
			device = get_device_for_output(pol,STRATEGY_SONIFICATION);
		else if (is_stream_active(lpol,AUDIO_STREAM_MUSIC,
					SONIFICATION_RESPECTFUL_AFTER_MUSIC_DELAY))
			device = get_device_for_output(pol,STRATEGY_MEDIA);
		else
			device = get_device_for_output(pol,STRATEGY_SONIFICATION);
		break;
	case STRATEGY_DTMF:
		/*when in call DTMF is used phone strategy*/
		if (!is_in_call(lpol)) {
			device = get_device_for_output(pol,STRATEGY_MEDIA);
			break;
		}
	case STRATEGY_PHONE:
		/*in the phone strategy ,there maybe have some force setted by client*/
		switch (lpol->ap_prop.forceUseConf[AUDIO_POLICY_FORCE_FOR_COMMUNICATION]) {
		case AUDIO_POLICY_FORCE_BT_SCO:
			if (!is_in_call(lpol) || strategy != STRATEGY_DTMF) {
				device = lpol->ap_prop.availableOutputDevices &
					AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT;
				if (device) break;
			}
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET;
			if (device) break;
			device =  lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_BLUETOOTH_SCO;
			if (device) break;
			/*can not found available sco device ,so use default*/

		default:
			if (!is_in_call(lpol) && 
					has_a2dp_device(lpol) &&
					lpol->ap_prop.forceUseConf[AUDIO_POLICY_FORCE_FOR_MEDIA] != 
					AUDIO_POLICY_FORCE_NO_BT_A2DP &&
					!(lpol->ap_prop.a2dpOutput->output1 != NULL && 
						lpol->ap_prop.a2dpOutput->output2 != NULL) &&
					!lpol->ap_prop.a2dp_is_suspend) {
				device = lpol->ap_prop.availableOutputDevices &
					AUDIO_DEVICE_OUT_BLUETOOTH_A2DP;
				if (device) break;
				device = lpol->ap_prop.availableOutputDevices &
					AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES;
				if (device) break;
			}
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_WIRED_HEADSET;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_USB_ACCESSORY;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_USB_DEVICE;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_AUX_DIGITAL;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_EARPIECE;
			if (device) break;
			device = AUDIO_ATTACHEDOUTPUTDEVICES;
			if (!device)
				ALOGW("no device found for STRATEGY_PHONE, FORCE_SPEAKER");

		case AUDIO_POLICY_FORCE_SPEAKER:
			if (!is_in_call(lpol) && 
					has_a2dp_device(lpol) &&
					lpol->ap_prop.forceUseConf[AUDIO_POLICY_FORCE_FOR_MEDIA] != 
					AUDIO_POLICY_FORCE_NO_BT_A2DP &&
					!(lpol->ap_prop.a2dpOutput->output1 != NULL && 
						lpol->ap_prop.a2dpOutput->output2 != NULL) &&
					!lpol->ap_prop.a2dp_is_suspend) {
				device = lpol->ap_prop.availableOutputDevices &
					AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER;
				if (device) break;
			}
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_USB_ACCESSORY;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_USB_DEVICE;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_AUX_DIGITAL;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_SPEAKER;
			if (device) break;
			device = AUDIO_ATTACHEDOUTPUTDEVICES;
			if (!device)
				ALOGE("Audio_policy (%s):[%d] there is no available device for phone.\n",
						__func__,__LINE__);
			break;
		}
		break;

	case STRATEGY_SONIFICATION:
		/*	If incall, just select the STRATEGY_PHONE device: The rest of the behavior is handled by
		 *	handleIncallSonification().*/
		if (is_in_call(lpol)) {
			device = get_device_for_output(pol,STRATEGY_PHONE);
			break;
		}

	case STRATEGY_ENFORCED_AUDIBLE:
		/* if enforced_audible there maybe have two device replay,
		 * because device1 is fix to speaker*/
		if (strategy == STRATEGY_SONIFICATION || 
				!lpol->ap_prop.ap_streams[AUDIO_STREAM_ENFORCED_AUDIBLE].canBeMuted)
			device = lpol->ap_prop.availableOutputDevices & AUDIO_DEVICE_OUT_SPEAKER;
		if (device == 0)
			ALOGW("AUDIO_DEVICE_OUT_SPEAKER is not found");

	case STRATEGY_MEDIA:
		switch (lpol->ap_prop.forceUseConf[AUDIO_POLICY_FORCE_FOR_MEDIA]) {

		case AUDIO_POLICY_FORCE_BT_SCO:
			device2 = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET;
			if (device2) break;
		case AUDIO_POLICY_FORCE_BT_CAR_DOCK:
			device2 = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT;
			if (device2) break;
		case AUDIO_POLICY_FORCE_BT_DESK_DOCK:
			device2 = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_BLUETOOTH_SCO;
			if (device2) break;
		default:
		case AUDIO_POLICY_FORCE_BT_A2DP:
			if (!is_in_call(lpol) && 
					has_a2dp_device(lpol) &&
					lpol->ap_prop.forceUseConf[AUDIO_POLICY_FORCE_FOR_MEDIA] != 
					AUDIO_POLICY_FORCE_NO_BT_A2DP &&
					!(lpol->ap_prop.a2dpOutput->output1 != NULL && 
						lpol->ap_prop.a2dpOutput->output2 != NULL) &&
					!lpol->ap_prop.a2dp_is_suspend) {
				device2 = lpol->ap_prop.availableOutputDevices &
					AUDIO_DEVICE_OUT_BLUETOOTH_A2DP;
				if (device2) break;
				device2 = lpol->ap_prop.availableOutputDevices &
					AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES;
				if (device2) break;
				device = lpol->ap_prop.availableOutputDevices &
					AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER;
				if (device2) break;
			}
		case AUDIO_POLICY_FORCE_NO_BT_A2DP:
		case AUDIO_POLICY_FORCE_HEADPHONES:
			device2 = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_WIRED_HEADSET;
			if (device2) break;
			device = lpol->ap_prop.availableOutputDevices
				& AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
			if (device2) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_USB_ACCESSORY;
			if (device) break;
			device = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_USB_DEVICE;
			if (device) break;
		case AUDIO_POLICY_FORCE_DIGITAL_DOCK:
			device2 = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET;
			if (device2) break;
			device2 = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_AUX_DIGITAL;
			if (device2) break;
		case AUDIO_POLICY_FORCE_ANALOG_DOCK:
			device2 = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET;
			if (device2) break;
		case AUDIO_POLICY_FORCE_SPEAKER:
			if (lpol->ap_prop.a2dpOutput) {
				device2 = lpol->ap_prop.availableOutputDevices &
					AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER;
				if (device2) break;
			}
			device2 = lpol->ap_prop.availableOutputDevices &
				AUDIO_DEVICE_OUT_SPEAKER;
			break;
		}
		device = device | device2;
		if (device) break;
		device = AUDIO_ATTACHEDOUTPUTDEVICES;
		if (device == 0)
			ALOGE("Audio_policy (%s):[%d] there is no available device for media.\n",
					__func__,__LINE__);
		break;
	default:
		ALOGW("getDeviceForStrategy() unknown strategy: %d", strategy);
		break;
	}

	return device;
}

static void update_device_for_strategy(struct audio_policy *pol)
{
	int i;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	for (i = 0; i < NUM_STRATEGIES; i++) {
		lpol->ap_prop.strategy_cache[i] = get_device_for_output(pol,i);
	}
}

static uint32_t set_output_device(struct audio_policy *pol, uint32_t device, bool force, int delay_ms,
		struct output_descriptor *output)
{

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	char param[128] = "\0";
	uint32_t mute_wait_ms = 0;
	audio_devices_t predevice = 0;


	if (output == NULL || pol == NULL ||output->id == 0)
		return mute_wait_ms;
	if (output->output1 != NULL && output->output2 != NULL) {
		mute_wait_ms = set_output_device(pol,device,force,delay_ms,output->output1);
		mute_wait_ms += set_output_device(pol,device,force,delay_ms,output->output2);
		return mute_wait_ms;
	}

	predevice = output->device;
	device = output->support_device & device;
	if (device != 0)
		output->device = device;

	/*legacy put it here ,but i want put below why? FIXME CHECK IT*/
	//mute_wait_ms = check_device_mute_starategies(output,predevice,delayMs);

	if (device == 0 || (predevice == device && force == false))
		return mute_wait_ms;

	mute_wait_ms = check_device_mute_starategies(output,predevice,delay_ms);

	sprintf(param, "%s=%d",
			AUDIO_PARAMETER_STREAM_ROUTING,
			output->device);
	lpol->aps_ops->set_parameters(lpol->service,
			output->id,
			param,
			delay_ms);
	/*
	 * update stream volumes according to new device
	 *	ummute
	 *	FIXME....
	 */
	return NO_ERROR;
}

static bool checkoutputisactive(audio_io_handle_t output,struct output_descriptor *odesc)
{
	struct output_descriptor *tmp_desc = odesc;
	bool ret = false;
	while (tmp_desc) {
		if (tmp_desc->id == output) {
			ret = true;
			break;
		}
		tmp_desc = tmp_desc->next;
	}

	return ret;
}

static bool checkinputisactive(audio_io_handle_t input,struct input_descriptor *idesc)
{
	struct input_descriptor *tmp_desc = idesc;
	bool ret = false;
	while (tmp_desc) {
		if (tmp_desc->id == input) {
			ret = true;
			break;
		}
		tmp_desc = tmp_desc->next;
	}

	return ret;
}
/* #################################################################### *\
|* struct audio_policy
\* #################################################################### */
/*******************************\
 * configuration functions
\*******************************/
/**
 * indicate a change in device connection status
 */
static int ap_set_device_connection_state(struct audio_policy *pol,
		audio_devices_t device,
		audio_policy_dev_state_t state,
		const char *device_address)
{
	FUNC_ENTER();

#ifdef POLICY_DEBUG
	ALOGD("%s(line:%d): pol = %p, device = %d, state = %d, device_address = %s",
		 __func__, __LINE__, pol, device, state, device_address);
#endif

	char param[128] = "\0";
	int delay_ms = 0;
	audio_devices_t newOutDevice = 0;
	struct input_descriptor* input_desc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	enum routing_strategy strategy;
	struct input_descriptor *pi = lpol->ap_prop.ap_inputs;
	audio_devices_t predevice = 0;

	if (pol == NULL)
		return NO_INIT;
	if (audio_is_output_device(device)) {
		// store ap_state
		switch (state) {
		case AUDIO_POLICY_DEVICE_STATE_AVAILABLE:
			if (lpol->ap_prop.availableOutputDevices & device) {
				ALOGW("ap_set_device_connection_state() device [%x] is already available.\n",device);
				return INVALID_OPERATION;
			}
			lpol->ap_prop.availableOutputDevices |= device;
			if (audio_is_bluetooth_sco_device(device)) {
				/*FIXME do something*/
			}
			break;
		case AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE:
			if (!(lpol->ap_prop.availableOutputDevices & device)) {
				ALOGW("ap_set_device_connection_state() device [%x] is already unavailable.\n",device);
				return INVALID_OPERATION;
			}
			lpol->ap_prop.availableOutputDevices &= ~device;
			if (audio_is_bluetooth_sco_device(device)) {
				/*FIXME do something*/
			}
			break;
		default:
			ALOGE("Audio_policy %s:[%d] state is error.\n",
					__func__,__LINE__);
			return BAD_VALUE;
		}

		// set route
		update_device_for_strategy(pol);
		strategy = get_current_out_strategy(lpol->ap_prop.primaryOutput);
		newOutDevice = get_device_for_output(pol,strategy);

		delay_ms = 0;
		newOutDevice = newOutDevice ? newOutDevice : device;
		set_output_device(pol,newOutDevice,false, delay_ms,lpol->ap_prop.primaryOutput);
		if (device == AUDIO_DEVICE_OUT_AUX_DIGITAL) {
			device = AUDIO_DEVICE_IN_AUX_DIGITAL;
		} if (device == AUDIO_DEVICE_OUT_WIRED_HEADSET) {
			device = AUDIO_DEVICE_IN_WIRED_HEADSET;
		} else if (device & AUDIO_DEVICE_OUT_ALL_SCO) {
			device = AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
		} else {
			return NO_ERROR;
		}
	}

	if (audio_is_input_device(device)) {
		// store ap_state
		ALOGD("ap_set_device_connection_state() device [%x] is %s",device,state ? state == 1 ? "available" :"unkown":"unavailable");
		switch (state) {
		case AUDIO_POLICY_DEVICE_STATE_AVAILABLE:
			if (lpol->ap_prop.availableInputDevices & device)
				return INVALID_OPERATION;
			lpol->ap_prop.availableInputDevices |= device;
			break;
		case AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE:
			if (!(lpol->ap_prop.availableInputDevices & device))
				return INVALID_OPERATION;
			lpol->ap_prop.availableInputDevices &= ~device;
			break;
		default:
			ALOGE("Audio_policy %s:[%d] state is error.\n",
					__func__,__LINE__);
			return BAD_VALUE;
		}

		if (pi != NULL && pi->refCount) {
			input_desc = find_current_input_desctiptor(pi);
			if (input_desc != NULL) {
				predevice = input_desc->device;
				input_desc->device = get_device_for_input(pol, input_desc->inputSource);
				if (predevice != input_desc->device && input_desc->device != 0) {
					param[0] = '\0';
					delay_ms = 0;
					sprintf(param, "%s=%d",
							AUDIO_PARAMETER_STREAM_ROUTING,input_desc->device);
					lpol->aps_ops->set_parameters(lpol->service,input_desc->id, param, delay_ms);
				}
			}
		}
	}

    return 0;
}

/**
 * retreive a device connection status
 */
static audio_policy_dev_state_t ap_get_device_connection_state(const struct audio_policy *pol,
															   audio_devices_t device,
															   const char *device_address)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, device = %d, device_address = %s",
		 __func__, __LINE__, pol, device, device_address);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	int state = AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE;

	if (pol == NULL)
		return NO_INIT;

	if (audio_is_output_device(device)) {
		if (device & lpol->ap_prop.availableOutputDevices) {
			state = AUDIO_POLICY_DEVICE_STATE_AVAILABLE;
		}
	} else if (audio_is_input_device(device)) {
		if (device & lpol->ap_prop.availableInputDevices) {
			state = AUDIO_POLICY_DEVICE_STATE_AVAILABLE;
		}
	}

	return state;
}

/**
 * indicate a change in phone state. Valid phones
 * states are defined by audio_mode_t
 */
static void ap_set_phone_state(struct audio_policy *pol, int state)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, state = %d",
		 __func__, __LINE__, pol, state);
#endif

	int delay_ms = 0;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct input_descriptor* inputDesc = NULL;
	struct output_descriptor *outputDesc = NULL;
	audio_devices_t newOutDevice = 0;
	enum routing_strategy strategy;
	audio_mode_t old_state;
	bool force = false;

	if (pol == NULL)
		return;

	if (state < 0 || state > AUDIO_MODE_CNT) {
		ALOGW("ap_set_phone_state() invalid state %d", state);
		return;
	}

	if (state == lpol->ap_prop.phoneState) {
		ALOGW("ap_set_phone_state() setting same state %d", state);
	}

	// if leaving call state, handle special case of active streams
	//pertaining to sonification strategy see handleIncallSonification()

	old_state = lpol->ap_prop.phoneState;
	lpol->ap_prop.phoneState = state;

	if (isStateInCall(state) && !isStateInCall(old_state))
		force = true;
	else if (isStateInCall(old_state) && !isStateInCall(state))
		force = true;
	else if (isStateInCall(state) && isStateInCall(old_state) && state != old_state)
		force = true;
	
	update_device_for_strategy(pol);
	strategy = get_current_out_strategy(lpol->ap_prop.primaryOutput);
	newOutDevice = get_device_for_output(pol,strategy);

	if (newOutDevice == 0 && isStateInCall(old_state))
		newOutDevice = lpol->ap_prop.primaryOutput->device;

	/*FIXME*/

	set_output_device(pol,newOutDevice,force,delay_ms,lpol->ap_prop.primaryOutput);

	/*FIXME*/
	return;
}

/**
 * indicate a change in ringer mode
 */
static void ap_set_ringer_mode(struct audio_policy *pol, uint32_t mode,
                               uint32_t mask)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, mode = %d, mask = %d",
		 __func__, __LINE__, pol, mode, mask);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	if (pol == NULL)
		return;

	lpol->ap_prop.ringerMode = mode;
}

/**
 * force using a specific device category for
 * the specified usage
 */
static void ap_set_force_use(struct audio_policy *pol,
                          audio_policy_force_use_t usage,
                          audio_policy_forced_cfg_t config)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, usage = %d, config = %d",
		 __func__, __LINE__, pol, usage, config);
#endif

	char param[128] = "\0";
	int delay_ms = 0;
	audio_devices_t newOutDevice = 0;
	audio_devices_t newInDevice = 0;
	struct input_descriptor* inputDesc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct input_descriptor *pi = NULL;
	enum routing_strategy strategy;

	if (pol == NULL)
		return;
	pi = lpol->ap_prop.ap_inputs;

	switch(usage) {
	case AUDIO_POLICY_FORCE_FOR_COMMUNICATION:
		if (config != AUDIO_POLICY_FORCE_SPEAKER &&
			config != AUDIO_POLICY_FORCE_BT_SCO &&
			config != AUDIO_POLICY_FORCE_NONE) {
            ALOGW("%s(line:%d): invalid config %d for FOR_COMMUNICATION",
				 __func__, __LINE__, config);
			lpol->ap_prop.forceUseConf[usage] = AUDIO_POLICY_FORCE_NONE;
			return;
		}
        lpol->ap_prop.forceUseConf[usage] = config;
		break;
    case AUDIO_POLICY_FORCE_FOR_MEDIA:
        if (config != AUDIO_POLICY_FORCE_HEADPHONES &&
			config != AUDIO_POLICY_FORCE_BT_A2DP &&
            config != AUDIO_POLICY_FORCE_WIRED_ACCESSORY &&
            config != AUDIO_POLICY_FORCE_ANALOG_DOCK &&
            config != AUDIO_POLICY_FORCE_DIGITAL_DOCK &&
			config != AUDIO_POLICY_FORCE_NONE) {
            ALOGW("%s(line:%d): invalid config %d for FOR_MEDIA",
				 __func__, __LINE__, config);
			lpol->ap_prop.forceUseConf[usage] = AUDIO_POLICY_FORCE_NONE;
            return;
        }
        lpol->ap_prop.forceUseConf[usage] = config;
		break;
    case AUDIO_POLICY_FORCE_FOR_RECORD:
        if (config != AUDIO_POLICY_FORCE_BT_SCO &&
			config != AUDIO_POLICY_FORCE_WIRED_ACCESSORY &&
			config != AUDIO_POLICY_FORCE_NONE &&
			config != AUDIO_POLICY_FORCE_DIGITAL_DOCK){
            ALOGW("%s(line:%d): invalid config %d for FOR_RECORD",
				 __func__, __LINE__, config);
			lpol->ap_prop.forceUseConf[usage] = AUDIO_POLICY_FORCE_NONE;
            return;
        }
        lpol->ap_prop.forceUseConf[usage] = config;
		break;
    case AUDIO_POLICY_FORCE_FOR_DOCK:
        if (config != AUDIO_POLICY_FORCE_BT_CAR_DOCK &&
            config != AUDIO_POLICY_FORCE_BT_DESK_DOCK &&
            config != AUDIO_POLICY_FORCE_WIRED_ACCESSORY &&
            config != AUDIO_POLICY_FORCE_ANALOG_DOCK &&
            config != AUDIO_POLICY_FORCE_DIGITAL_DOCK &&
			config != AUDIO_POLICY_FORCE_NONE) {
            ALOGW("%s(line:%d): invalid config %d for FOR_DOCK",
				 __func__, __LINE__, config);
			lpol->ap_prop.forceUseConf[usage] = AUDIO_POLICY_FORCE_NONE;
			return;
        }
        lpol->ap_prop.forceUseConf[usage] = config;
		break;
	default:
		ALOGD("%s(line:%d): invalid usage %d",
			 __func__, __LINE__, usage);
	}

	// set out route
	update_device_for_strategy(pol);
	strategy = get_current_out_strategy(lpol->ap_prop.primaryOutput);
	newOutDevice = get_device_for_output(pol,strategy);
	delay_ms = 0;

	set_output_device(pol,newOutDevice,false,delay_ms,lpol->ap_prop.primaryOutput);

	input_desc = find_current_input_desctiptor(pi);
	if (input_desc != NULL) {
		newInDevice = get_device_for_input(pol, input_desc->inputSource);
		if (input_desc->device != newInDevice && newInDevice != 0) {
			input_desc->device = newInDevice;
			param[0] = '\0';
			delay_ms = 0;
			sprintf(param, "%s=%d",
					AUDIO_PARAMETER_STREAM_ROUTING,	input_desc->device);
			lpol->aps_ops->set_parameters(lpol->service,
										  input_desc->id, param, delay_ms);
		}
	}
}

/**
 * retreive current device category forced
 * for a given usage
 */
static audio_policy_forced_cfg_t ap_get_force_use(const struct audio_policy *pol,
												  audio_policy_force_use_t usage)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, usage = %d",
		 __func__, __LINE__, pol, usage);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	if (pol == NULL)
		return NO_INIT;
    return lpol->ap_prop.forceUseConf[usage];
}

/**
 * if can_mute is true, then audio streams that are
 * marked ENFORCED_AUDIBLE can still be muted.
 */
static void ap_set_can_mute_enforced_audible(struct audio_policy *pol,
                                             bool can_mute)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, can_mute = %s",
		 __func__, __LINE__, pol, can_mute ? "true" : "false");
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	if (pol == NULL)
		return;
	if (can_mute) {
		lpol->ap_prop.ap_streams[AUDIO_STREAM_ENFORCED_AUDIBLE].canBeMuted = true;
	} else {
		lpol->ap_prop.ap_streams[AUDIO_STREAM_ENFORCED_AUDIBLE].canBeMuted = false;
	}
}

/**
 * check proper initialization
 */
static int ap_init_check(const struct audio_policy *pol)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p",
		 __func__, __LINE__, pol);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	if (pol == NULL)
		return NO_INIT;
	if (lpol->ap_prop.primaryOutput->id == 0)
		ALOGE("Audio_policy %s:[%d] device is not init.\n",
					__func__,__LINE__);

	return lpol->ap_prop.primaryOutput->id == 0 ? NO_INIT : NO_ERROR;
}

/********************************\
 * Audio routing query functions
\********************************/
/**
 * request an output appriate for playback of
 * the supplied stream type and parameters
 */
static audio_io_handle_t ap_get_output(struct audio_policy *pol,
                                       audio_stream_type_t stream,
                                       uint32_t sampling_rate,
                                       audio_format_t format,
                                       uint32_t channels,
                                       audio_output_flags_t flags)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, stream = %d, sampling_rate = %d, format = %d, channels = %d, flags = %d",
		 __func__, __LINE__, pol, stream, sampling_rate, format, channels, flags);
#endif

	int i = 0;
	audio_io_handle_t output = 0;
	int match_flags = 0;
	audio_module_handle_t handle = 0;
	audio_devices_t device = 0;
	struct output_descriptor *output_desc = NULL,*tmp_desc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct hardware_module	*tmp_module = lpol->ap_prop.hwmodule;
	struct module_profile   *tmp_profile = NULL;
	uint32_t strategy = 0;

	//get device
	if (pol == NULL)
		return NO_INIT;
	strategy = get_strategy(stream);
	device = get_device_for_output(pol,strategy);

	while (tmp_module && handle == 0) {
		if (tmp_module->handle != 0) {
			tmp_profile = tmp_module->profile;
			while (tmp_profile) {
				if (tmp_profile->flags & AUDIO_OUTPUT_FLAG_DIRECT &&
						tmp_profile->devices & device) {
					flags |= AUDIO_OUTPUT_FLAG_DIRECT;
					handle = tmp_module->handle;
					break;
				}
				tmp_profile = tmp_profile->next;
			}
		}
		tmp_module = tmp_module->next;
	}

	//open a direct output
	if ((flags & AUDIO_OUTPUT_FLAG_DIRECT) && handle != 0) {
		//output_descriptor
		output_desc = (struct output_descriptor *)calloc(1,sizeof(*output_desc));
		if (!output_desc)
			return -ENOMEM;

		for (i = 0; i < AUDIO_STREAM_CNT; i++) {
			output_desc->oStream[i] = &lpol->ap_prop.ap_streams[i];
			output_desc->stopTime[i] = 0;
		}
		output_desc->samplingRate = sampling_rate;
		output_desc->format = format;
		output_desc->channels = channels;
		output_desc->device = device;
		output_desc->flags = flags;
		output_desc->latency = 0;
		output_desc->refCount = 0;
		output_desc->next = NULL;
		output_desc->handle = handle;
		output_desc->output1 = NULL;
		output_desc->output2 = NULL;

		output = lpol->aps_ops->open_output_on_module(lpol->service,
				output_desc->handle,
				&output_desc->device,
				&output_desc->samplingRate,
				&output_desc->format,
				&output_desc->channels,
				&output_desc->latency,
				output_desc->flags);

		if ((output == 0) ||
				((sampling_rate != 0) && (sampling_rate != output_desc->samplingRate)) ||
				((format != 0) && (format != output_desc->format)) ||
				((channels != 0) && (channels != output_desc->channels))) {
			//close output
			if (output) {
				lpol->aps_ops->close_output(lpol->service, output);
			}
			free(output_desc);
			output_desc = NULL;
			return NO_INIT;
		}

		output_desc->id = output;

		if (lpol->ap_prop.ap_outputs == NULL) {
			lpol->ap_prop.ap_outputs = output_desc;
		} else {
			output_desc->next = lpol->ap_prop.ap_outputs;
			lpol->ap_prop.ap_outputs = output_desc;
		}

		return output_desc->id;
	}

	tmp_desc = lpol->ap_prop.ap_outputs;

	while (tmp_desc) {
		if (tmp_desc->output1 != NULL &&
				tmp_desc->output2 != NULL) {
			tmp_desc = tmp_desc->next;
			continue;
		}
		if ((tmp_desc->support_device & device) == device) {
			if (popcount(flags|tmp_desc->flags) > match_flags) {
				match_flags = popcount(flags|tmp_desc->flags);
					output = tmp_desc->id;
			}
			if (output == 0)
				output = tmp_desc->id;
			if (match_flags == 0 &&
					tmp_desc->flags == AUDIO_OUTPUT_FLAG_PRIMARY) {
				output = tmp_desc->id;
				break;
			}
		}
		tmp_desc = tmp_desc->next;
	}

	return output;
}

/**
 * indicates to the audio policy manager that the
 * output starts being used by corresponding stream.
 */

static bool find_shared_module(struct output_descriptor *output1,
		struct output_descriptor *output2)
{
	if (output1->output1 != NULL && output1->output2 != NULL)
		return find_shared_module(output1->output1,output2) |
			find_shared_module(output1->output2,output2);
	else if (output2->output1 != NULL && output2->output2 != NULL)
		return find_shared_module(output1,output2->output1) |
			find_shared_module(output1,output2->output2);
	else
		return output1->handle == output2->handle;
}

static uint32_t get_latency(struct output_descriptor *p)
{
	if (p->output1 != NULL && p->output2 != NULL)
		return p->output1->latency > p->output2->latency ?
			p->output1->latency : p->output2->latency;
	return p->latency;
}

static int ap_start_output(struct audio_policy *pol, audio_io_handle_t output,
		audio_stream_type_t stream, int session)
{
	ALOGV("%s(line:%d): pol = %p, output = %d, stream = %d, session = %d",
			__func__, __LINE__, pol, output, stream, session);

	int delay_ms = 0;
	uint32_t mute_wait_ms = 0;
	uint32_t wait_ms = 0;
	bool should_wait = false;
	audio_devices_t newOutDevice = 0;
	audio_devices_t predevice = 0;
	struct output_descriptor *output_desc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct output_descriptor *p = lpol->ap_prop.ap_outputs;
	bool force = false;
	enum routing_strategy strategy;

	if (pol == NULL)
		return NO_INIT;
	// output_descriptorp
	while(p) {
		if (output == p->id) {
			output_desc = p;
			break;
		}
		p = p->next;
	}

	// output not be opened
	if (output_desc == NULL) {
		ALOGE("%s(line:%d): output %d not found!",
				__func__, __LINE__, output);
		return NO_INIT;
	}

	output_desc->oStream[stream]->refCount += 1;
	output_desc->refCount += 1;

	if (output_desc->oStream[stream]->refCount == 1) {
		// set out route
		update_device_for_strategy(pol);
		strategy	= get_current_out_strategy(output_desc);
		predevice	= output_desc->device;
		newOutDevice	= get_device_for_output(pol,strategy);
		delay_ms	= 0;
		force		= false;
		if (strategy == STRATEGY_SONIFICATION ||
				strategy == STRATEGY_SONIFICATION_RESPECTFUL)
			should_wait = true;
		p = lpol->ap_prop.ap_outputs;
		while (p) {
			if ( p->device != newOutDevice && find_shared_module(output_desc,p)) {
				force = true;
			}
			if (should_wait && p->refCount != 0 && (wait_ms < get_latency(p)))
				wait_ms = get_latency(p);
			p = p->next;
		}
		mute_wait_ms = set_output_device(pol,newOutDevice,force,delay_ms,output_desc);

		/*set_volume*/

		/*special in call when SONIFICATION came FIXME*/
		/*if (is_in_call(lpol)) {
			handleIncallSonification(stream, true, false);
		}*/

		if (output_desc->device != predevice &&
				wait_ms > mute_wait_ms) {
			usleep((wait_ms-mute_wait_ms)*2*1000);
		}
	}

	return NO_ERROR;
}

/**
 * indicates to the audio policy manager that the
 * output stops being used by corresponding stream.
 */
static int ap_stop_output(struct audio_policy *pol, audio_io_handle_t output,
		audio_stream_type_t stream, int session)
{
	LOGV("%s(line:%d): pol = %p, output = %d, stream = %d, session = %d",
			__func__, __LINE__, pol, output, stream, session);

	audio_devices_t newOutDevice = 0;
	struct output_descriptor *output_desc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct output_descriptor *p = lpol->ap_prop.ap_outputs;
	enum routing_strategy strategy;

	if (pol == NULL)
		return NO_INIT;
	// output_descriptorp
	while(p) {
		if (output == p->id) {
			output_desc = p;
			break;
		}
		p = p->next;
	}

	// output not be opened
	if (output_desc == NULL)
		return NO_INIT;

	if (output_desc->refCount > 0) {
		if (output_desc->oStream[stream]->refCount > 0) {
			output_desc->oStream[stream]->refCount -= 1;
			output_desc->refCount -= 1;
			output_desc->stopTime[stream] = 0;
			if (output_desc != lpol->ap_prop.primaryOutput) {
				strategy = get_current_out_strategy(output_desc);
				newOutDevice = get_device_for_output(pol,strategy);
				set_output_device(pol,newOutDevice,false,output_desc->latency*2,output_desc);
			}
			p = lpol->ap_prop.ap_outputs;
			while(p) {
				if (p->id != output_desc->id &&
						find_shared_module(output_desc,p) &&
						p->device != output_desc->device &&
						p->refCount > 0)
					set_output_device(pol,newOutDevice,false,
							p->latency*2,p);
				p = p->next;
			}
		} else if (output_desc->oStream[stream]->refCount <=0) {
			output_desc->oStream[stream]->refCount = 0;
			return INVALID_OPERATION;
		}
	}

    return NO_ERROR;
}

/**
 * releases the output.
 */
static void ap_release_output(struct audio_policy *pol,
		audio_io_handle_t output)
{
	ALOGV("%s(line:%d): pol = %p, output = %d",
			__func__, __LINE__, pol, output);

	struct output_descriptor *output_desc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct output_descriptor *p = NULL;
	struct output_descriptor *pre_p= NULL;

	if (pol == NULL)
		return;
	for ( p = pre_p = lpol->ap_prop.ap_outputs;
			p != NULL;
			p = p->next,pre_p = p) {
		if (output == p->id &&
				p->refCount == 0 &&
				!(p->flags & AUDIO_OUTPUT_FLAG_PRIMARY)) {
			/*FIXME*/
			lpol->aps_ops->close_output(lpol->service,output);
			if (lpol->ap_prop.a2dpOutput == p)
				lpol->ap_prop.a2dpOutput = NULL;
			if (lpol->ap_prop.duplicatedOutput == p)
				lpol->ap_prop.duplicatedOutput = NULL;
			if (pre_p == p) {
				lpol->ap_prop.ap_outputs = p->next;
			} else {
				pre_p->next = p->next;
			}
			free(p);
			p = NULL;
			break;
		}
	}

	return;
}

/**
 * request an input appriate for record from the
 * supplied device with supplied parameters.
 */
static audio_io_handle_t ap_get_input(struct audio_policy *pol, audio_source_t inputSource,
                                      uint32_t sampling_rate,
                                      audio_format_t format,
                                      uint32_t channels,
                                      audio_in_acoustics_t acoustics)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, inputSource = %d, sampling_rate = %d, format = %d, channels = %d, acoustics = %d",__func__, __LINE__, pol, inputSource, sampling_rate, format, channels, acoustics);
#endif

	audio_io_handle_t input = 0;

	audio_devices_t device = 0;
	struct input_descriptor *input_desc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	audio_module_handle_t handle = 0;
	struct hardware_module *tmp_module = NULL;
	struct module_profile *tmp_profile = NULL;

	if (pol == NULL)
		return 0;

	tmp_module = lpol->ap_prop.hwmodule;

	device = get_device_for_input(pol, inputSource);
	if (device == 0) {
		ALOGW("there was no device support this inputSource %d", inputSource);
		return 0;
	}

	while (tmp_module && handle == 0) {
		if (tmp_module->handle != 0) {
			tmp_profile = tmp_module->profile;
			while (tmp_profile) {
				if (tmp_profile->devices & device) {
					handle = tmp_module->handle;
					break;
				}
				tmp_profile = tmp_profile->next;
			}
		}
		tmp_module = tmp_module->next;
	}

	if (handle != 0) {
		//input_descriptor
		input_desc = (struct input_descriptor *)calloc(1, sizeof(*input_desc));
		if (input_desc == NULL)
			return 0;
		switch (inputSource) {
		case AUDIO_SOURCE_VOICE_UPLINK :
			channels = AUDIO_CHANNEL_IN_VOICE_UPLINK;
			break;
		case AUDIO_SOURCE_VOICE_DOWNLINK:
			channels = AUDIO_CHANNEL_IN_VOICE_DNLINK;
			break;
		case AUDIO_SOURCE_VOICE_CALL:
			channels = AUDIO_CHANNEL_IN_VOICE_UPLINK | AUDIO_CHANNEL_IN_VOICE_DNLINK;
			break;
		default:
			break;
		}
		input_desc->inputSource = inputSource;
		input_desc->samplingRate = sampling_rate;
		input_desc->format = format;
		input_desc->channels = channels;
		input_desc->acoustics = acoustics;
		input_desc->device = device;
		input_desc->refCount = 0;
		input_desc->next = NULL;
		input_desc->handle = handle;

		input = lpol->aps_ops->open_input_on_module(lpol->service,
				input_desc->handle,
				&input_desc->device,
				&input_desc->samplingRate,
				&input_desc->format,
				&input_desc->channels);

		if ((input == 0) ||
				(sampling_rate != input_desc->samplingRate) ||
				(format != input_desc->format) ||
				(channels != input_desc->channels)) {

			if (input) {
				lpol->aps_ops->close_input(lpol->service, input);
			}
			free(input_desc);
			input_desc = NULL;
			return 0;
		}

		input_desc->id = input;

		if (lpol->ap_prop.ap_inputs == NULL) {
			lpol->ap_prop.ap_inputs = input_desc;
		} else {
			input_desc->next = lpol->ap_prop.ap_inputs;
			lpol->ap_prop.ap_inputs = input_desc;
		}

		return input_desc->id;
	}

	return 0;
}

/**
 * indicates to the audio policy manager that
 * the input starts being used
 */
static int ap_start_input(struct audio_policy *pol, audio_io_handle_t input)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, input = %d",
		 __func__, __LINE__, pol, input);
#endif

	char param[128] = "\0";
	int delay_ms = 0;
	struct input_descriptor* input_desc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct input_descriptor* p = NULL;
	audio_devices_t predevice = 0;

	if (pol == NULL)
		return NO_INIT;

	p = lpol->ap_prop.ap_inputs;

	while(p) {
		if (p->refCount > 0) {
			return INVALID_OPERATION;
		}
		if (input == p->id)
			input_desc = p;
		p = p->next;
	}

	// input not be opened
	if (input_desc == NULL) {
		ALOGW("input device is no init\n");
		return NO_INIT;
	}

	predevice = input_desc->device;
	input_desc->device = get_device_for_input(pol, input_desc->inputSource);
	ALOGV("device %x source %x available %x\n", input_desc->device,input_desc->inputSource,lpol->ap_prop.availableInputDevices);
	// set route
	if (input_desc->device != 0 && predevice != input_desc->device) {
		sprintf(param, "%s=%d;%s=%d",
				AUDIO_PARAMETER_STREAM_ROUTING,	input_desc->device,
				AUDIO_PARAMETER_STREAM_INPUT_SOURCE, input_desc->inputSource);
		lpol->aps_ops->set_parameters(lpol->service, input, param, delay_ms);
	}
	input_desc->refCount = 1;

    return NO_ERROR;
}

/**
 * indicates to the audio policy manager that
 * the input stops being used.
 */
static int ap_stop_input(struct audio_policy *pol, audio_io_handle_t input)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, input = %d",
		 __func__, __LINE__, pol, input);
#endif

	char param[128] = "\0";
	int delay_ms = 0;
	struct input_descriptor *input_desc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct input_descriptor *p = NULL;

	if (pol == NULL)
		return NO_INIT;

	p = lpol->ap_prop.ap_inputs;

	while(p) {
		if (input == p->id) {
			input_desc = p;
			break;
		}
		p = p->next;
	}

	// input not be opened
	if (input_desc == NULL)
		return NO_INIT;

	if (input_desc->refCount > 0) {
		// set route
		sprintf(param, "%s=%d",
				AUDIO_PARAMETER_STREAM_ROUTING,	0);
		lpol->aps_ops->set_parameters(lpol->service, input, param, delay_ms);
		input_desc->device = 0;
		// change refCount
		input_desc->refCount = 0;
	}
	return NO_ERROR;
}

/**
 * releases the input.
 */
static void ap_release_input(struct audio_policy *pol, audio_io_handle_t input)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, input = %d",
		 __func__, __LINE__, pol, input);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct input_descriptor* p = NULL;
	struct input_descriptor* pre_p = NULL;

	if (pol == NULL)
		return;
	// close input
	lpol->aps_ops->close_input(lpol->service, input);

	// del input
	p = lpol->ap_prop.ap_inputs;
	for (p = pre_p = lpol->ap_prop.ap_inputs; p != NULL ; p = p->next,pre_p = p) {
		if (input == p->id && p->refCount == 0) {
			if (pre_p == p)
				lpol->ap_prop.ap_inputs = p->next;
			else
				pre_p->next = p->next;
			free(p);
			p = NULL;
			break;
		}
	}
}

/*******************************\
 * volume control functions
\*******************************/
/**
 * initialises stream volume conversion parameters
 * by specifying volume index range.
 */
static void ap_init_stream_volume(struct audio_policy *pol,
                                  audio_stream_type_t stream, int index_min,
                                  int index_max)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, stream = %d, index_min = %d, index_max = %d",
		 __func__, __LINE__, pol, stream, index_min, index_max);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	if (pol == NULL)
		return;

    if (index_min < 0 || index_min >= index_max) {
        ALOGE("%s(line:%d): invalid index limits for stream %d, min %d, max %d",
			 __func__, __LINE__, stream, index_min, index_max);
        return;
    }

	lpol->ap_prop.ap_streams[stream].indexMin = index_min;
	lpol->ap_prop.ap_streams[stream].indexMax = index_max;
}

/**
 * sets the new stream volume at a level corresponding
 * to the supplied index
 */
static int ap_set_stream_volume_index(struct audio_policy *pol,
                                      audio_stream_type_t stream,
                                      int index)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, stream = %d, index = %d",
		 __func__, __LINE__, pol, stream, index);
#endif

	float volume = 1.0;
	int delay_ms = 0;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct output_descriptor *p = NULL;
	float voice_volume;

	if (pol == NULL)
		return NO_INIT;

	p = lpol->ap_prop.ap_outputs;

	if (p == NULL)
		return NO_INIT;

    if ((index < lpol->ap_prop.ap_streams[stream].indexMin)
		|| (index > lpol->ap_prop.ap_streams[stream].indexMax)) {
		ALOGE("%s(line:%d): invalid index limits for stream %d, index = %d",
			 __func__, __LINE__, stream, index);
        return BAD_VALUE;
    }

	lpol->ap_prop.ap_streams[stream].indexCur = index;

    /* compute and apply stream volume on all
	   outputs according to connected device */

#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): stream = %d, index = %d, volume = %f",
		 __func__, __LINE__, stream, index, volume);
#endif

	// set stream volume
	if ((stream == AUDIO_STREAM_VOICE_CALL &&
		lpol->ap_prop.forceUseConf[AUDIO_POLICY_FORCE_FOR_COMMUNICATION] == AUDIO_POLICY_FORCE_BT_SCO) ||
		(stream == AUDIO_STREAM_BLUETOOTH_SCO &&
		lpol->ap_prop.forceUseConf[AUDIO_POLICY_FORCE_FOR_COMMUNICATION]!= AUDIO_POLICY_FORCE_BT_SCO)) {
		return INVALID_OPERATION;
	}

	while(p) {
		if (p->oStream[stream]->isMuted)
			continue;
		volume = ((lpol->ap_prop.ap_streams[stream].indexCur - lpol->ap_prop.ap_streams[stream].indexMin) * 1.0) /
			(lpol->ap_prop.ap_streams[stream].indexMax - lpol->ap_prop.ap_streams[stream].indexMin);
		if (p->oStream[stream]->indexCur == index) {
			p->oStream[stream]->indexCur = index;
			if (stream == AUDIO_STREAM_VOICE_CALL ||
				stream == AUDIO_STREAM_BLUETOOTH_SCO ||
				stream == AUDIO_STREAM_DTMF) {
				volume = 0.01 + 0.09 * volume;
				if (stream == AUDIO_STREAM_BLUETOOTH_SCO)
					lpol->aps_ops->set_stream_volume(lpol->service,
													AUDIO_STREAM_VOICE_CALL,
													volume,
													p->id,
													p->latency);
			}
			lpol->aps_ops->set_stream_volume(lpol->service,
											stream,
											volume,
											p->id,
											p->latency);
		}

		if (stream == AUDIO_STREAM_VOICE_CALL ||
			stream == AUDIO_STREAM_BLUETOOTH_SCO) {
			if (stream == AUDIO_STREAM_VOICE_CALL)
				voice_volume = (float)index /p->oStream[stream]->indexMax;
			else
				voice_volume = 1.0;

			if (voice_volume != lpol->ap_prop.lastVoiceVolume &&
				p == lpol->ap_prop.primaryOutput)
				lpol->aps_ops->set_voice_volume(lpol->service,
												voice_volume,
												p->latency);
				lpol->ap_prop.lastVoiceVolume = voice_volume;
		}
		p = p->next;
	}

    return NO_ERROR;
}

/**
 * retreive current volume index for the
 * specified stream
 */
static int ap_get_stream_volume_index(const struct audio_policy *pol,
                                      audio_stream_type_t stream,
                                      int *index)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, stream = %d",
		 __func__, __LINE__, pol, stream);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	if (pol == NULL)
		return NO_INIT;
	if (index == NULL) {
		ALOGE("%s(line:%d): no index .\n",
			 __func__, __LINE__);
		return BAD_VALUE;
	}

	*index = lpol->ap_prop.ap_streams[stream].indexCur;

    return NO_ERROR;
}

/**
 * return the strategy corresponding to a
 * given stream type
 */
static uint32_t ap_get_strategy_for_stream(const struct audio_policy *pol,
                                           audio_stream_type_t stream)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, stream = %d",
		 __func__, __LINE__, pol, stream);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	if (pol == NULL)
		return NO_INIT;
	
	ALOGV("%s(line:%d): pol = %p, stream = %d,strategy = %d",
			__func__, __LINE__, pol, stream,get_strategy(stream));
	return get_strategy(stream);
}

/**
 * return the enabled output devices for
 * the given stream type
 */
static uint32_t ap_get_devices_for_stream(const struct audio_policy *pol,
                                          audio_stream_type_t stream)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, stream = %d",__func__, __LINE__, pol, stream);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	enum routing_strategy strategy;

	if (pol == NULL)
		return NO_INIT;

    if (stream < 0 || stream >= AUDIO_STREAM_CNT) {
        return 0;
    }

    /* stream to devices mapping */
	strategy = ap_get_strategy_for_stream(pol,stream);

	return lpol->ap_prop.strategy_cache[strategy];
}

/**
 * Audio effect management
 */
static audio_io_handle_t ap_get_output_for_effect(struct audio_policy *pol,
                                            struct effect_descriptor_s *desc)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, desc = %p",
		 __func__, __LINE__, pol, desc);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	enum routing_strategy strategy = STRATEGY_MEDIA;
	audio_devices_t device = 0;
	struct output_descriptor *tmp_desc = lpol->ap_prop.ap_outputs;

	device = get_device_for_output(pol,strategy);

	while (tmp_desc) {
		if (tmp_desc->flags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER)
			break;
		if (tmp_desc->next == NULL)
			break;
		tmp_desc = tmp_desc->next;
	}

    return tmp_desc->id;
}

/*mutex*/
static uint32_t totaleffectsmemory = 0;
static uint32_t totaleffectscpuload = 0;
static struct effect_descriptor *g_effect_desp = NULL;

static int ap_register_effect(struct audio_policy *pol,
                              struct effect_descriptor_s *desc,
                              audio_io_handle_t output,
                              uint32_t strategy,
                              int session,
                              int id)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, desc = %p, output = %d, strategy = %d, session = %d, id = %d",
		 __func__, __LINE__, pol, desc, output, strategy, session, id);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct effect_descriptor *pdesc = NULL;
	struct output_descriptor *odesc = lpol->ap_prop.ap_outputs;
	struct input_descriptor  *idesc = lpol->ap_prop.ap_inputs;

	if (false == checkoutputisactive(output,odesc)) {
		if (false == checkinputisactive(output,idesc)) {
			ALOGW("registereffect() unknown io %d", output);
			return INVALID_OPERATION;
		}
	}

	if (totaleffectsmemory + desc->memoryUsage > MAX_EFFECTS_MEMORY) {
		ALOGW("registerEffect() memory limit exceeded for Fx %s, Memory %d KB",
				desc->name, desc->memoryUsage);
		return INVALID_OPERATION;
	}
	totaleffectsmemory += desc->memoryUsage;
	ALOGV("registerEffect() effect %s, io %d, strategy %d session %d id %d",
			desc->name, output, strategy, session, id);
	ALOGV("registerEffect() memory %d, total memory %d", desc->memoryUsage,totaleffectsmemory);

	pdesc = (struct effect_descriptor *)calloc(1,sizeof(struct effect_descriptor));
	if (pdesc == NULL)
		return NO_MEMORY;
	memcpy(&pdesc->desc,desc,sizeof(struct effect_descriptor_s));
	pdesc->io = output;
	pdesc->strategy = (enum routing_strategy)strategy;
	pdesc->session = session;
	pdesc->id = id;
	pdesc->enable = false;

	if (g_effect_desp == NULL) {
		pdesc->next = NULL;
		g_effect_desp = pdesc;
	} else {
		pdesc->next = g_effect_desp;
		g_effect_desp = pdesc;
	}

    return NO_ERROR;
}

static int ap_unregister_effect(struct audio_policy *pol, int id)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, id = %d",__func__, __LINE__, pol, id);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct effect_descriptor *pdesc = g_effect_desp;
	struct effect_descriptor *tmp_desc = g_effect_desp;
	bool found = false;

	while (pdesc) {
		if (pdesc->id == id) {
			found = true;
			break;
		}
		tmp_desc = pdesc;
		pdesc = pdesc->next;
	}

	if (found == false) {
		ALOGW("unregisterEffect() unknown effect ID %d", id);
		return INVALID_OPERATION;
	}

	if (totaleffectsmemory < pdesc->desc.memoryUsage) {
		ALOGW("unregisterEffect() memory %d too big for total %d",
				pdesc->desc.memoryUsage, totaleffectsmemory);
		pdesc->desc.memoryUsage = totaleffectsmemory;
	}
	totaleffectsmemory -= pdesc->desc.memoryUsage;

	if (pdesc == g_effect_desp)
		g_effect_desp = pdesc->next;
	else
		tmp_desc->next = pdesc->next;

	free(pdesc);

    return NO_ERROR;
}

static int ap_set_effect_enabled(struct audio_policy *pol, int id, bool enabled)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, enabled = %s",
		 __func__, __LINE__, pol, enabled ? "true" : "false");
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct effect_descriptor *pdesc = NULL;
	int found = false;

	while (pdesc) {
		if (pdesc->id == id) {
			found = true;
			break;
		}
		pdesc = pdesc->next;
	}

	if (found == false) {
		ALOGW("ap_set_effect_enabled() unknown effect ID %d", id);
		return INVALID_OPERATION;
	}

	if (pdesc->enable == enabled) {
		ALOGV("ap_set_effect_enabled effect id %d is already %s.\n",id,enabled ? "enabled":"disabled");
		return INVALID_OPERATION;
	}

	if (enabled) {
		if (totaleffectscpuload + pdesc->desc.cpuLoad > MAX_EFFECTS_CPU_LOAD)
			return INVALID_OPERATION;
		totaleffectscpuload += pdesc->desc.cpuLoad;
	} else {
		if (totaleffectscpuload < pdesc->desc.cpuLoad)
			pdesc->desc.cpuLoad = totaleffectscpuload;
		totaleffectscpuload -= pdesc->desc.cpuLoad;
	}

	pdesc->enable = enabled;

    return NO_ERROR;
}

static bool ap_is_stream_active(const struct audio_policy *pol, int stream,
                                uint32_t in_past_ms)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, stream = %d, in_past_ms = %d",
		 __func__, __LINE__, pol, stream, in_past_ms);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct output_descriptor *p = NULL;
	/*FIXME systime*/

	if (pol == NULL)
		return NO_INIT;

	p = lpol->ap_prop.ap_outputs;

	while (p) {
		if (p->refCount > 0) {
			if (p->oStream[stream]->refCount > 0) {
				return true;
			}
		}
		p = p->next;
	}
    ALOGE("%s:[%d] stream %d is not active.\n",__func__,__LINE__,stream);

    return false;
}

/**
 * dump state
 */
static int ap_dump(const struct audio_policy *pol, int fd)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): pol = %p, fd = %d",
		 __func__, __LINE__, pol, fd);
#endif

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	return INVALID_OPERATION;
}

static const char * const audio_interfaces[] = {
	AUDIO_HARDWARE_MODULE_ID_PRIMARY,
	//AUDIO_HARDWARE_MODULE_ID_A2DP,
	//AUDIO_HARDWARE_MODULE_ID_USB,
};

#define ARRAY_SIZE(x) (sizeof((x))/sizeof(((x)[0])))
static int32_t init_hardware_module(struct xb47xx_audio_policy *lpol,const char *name)
{
	struct hardware_module *tmp_module = NULL;
	struct hardware_module *new_module = NULL;
	if (lpol == NULL)
		return -1;
	new_module = (struct hardware_module *)calloc(1,sizeof(struct hardware_module));
	new_module->profile = NULL;
	new_module->next = NULL;
	new_module->handle = 0;
	if (tmp_module)
		return NO_MEMORY;

	if (!strcmp(AUDIO_HARDWARE_MODULE_ID_PRIMARY,name)) {
		new_module->profile = (struct module_profile *)calloc(1, sizeof(struct module_profile));
		new_module->profile->next = NULL;
		new_module->profile->flags = AUDIO_OUTPUT_FLAG_PRIMARY;
		new_module->profile->devices =
			(AUDIO_DEVICE_OUT_ALL & (~AUDIO_DEVICE_OUT_ALL_A2DP) & (~AUDIO_DEVICE_OUT_ALL_USB)) |
			(AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET | AUDIO_DEVICE_IN_VOICE_CALL| AUDIO_DEVICE_IN_COMMUNICATION);
		new_module->profile->in_channels = AUDIO_CHANNEL_IN_MONO | AUDIO_CHANNEL_IN_STEREO;
		new_module->profile->out_channels = AUDIO_CHANNEL_OUT_STEREO;
	} else if (!strcmp(AUDIO_HARDWARE_MODULE_ID_A2DP,name)) {
		/*....*/
		free(new_module);
		new_module = NULL;
	} else if (!strcmp(AUDIO_HARDWARE_MODULE_ID_USB,name)) {
		/*....*/
		free(new_module);
		new_module = NULL;
	} else {
		ALOGW("Unsupport hwmodule");
		free(new_module);
		new_module = NULL;
	}
	if (new_module != NULL) {
		if (lpol->ap_prop.hwmodule == NULL)
			lpol->ap_prop.hwmodule = new_module;
		else {
			new_module = lpol->ap_prop.hwmodule->next;
			lpol->ap_prop.hwmodule = new_module;
			new_module->next = tmp_module;
		}
	}
	return 0;
}

/* #################################################################### *\
|* struct audio_policy_device
\* #################################################################### */

static int create_xb47xx_ap(const struct audio_policy_device *device,
                             struct audio_policy_service_ops *aps_ops,
                             void *service,
                             struct audio_policy **ap)
{
	const struct xb47xx_ap_device *dev = NULL;
	struct xb47xx_audio_policy *lpol = NULL;
	struct output_descriptor *output_desc = NULL;
	audio_module_handle_t handle_tmp = 0;
	int ret = 0;
	uint32_t i = 0;
	uint32_t retry_time = 0;

#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): device = %p, aps_ops = %p, service = %p, ap = %p",
		 __func__, __LINE__, device, aps_ops, service, *ap);
#endif

	if (device == NULL)
		return NO_INIT;
    *ap = NULL;

    if (!service || !aps_ops) {
		ALOGE("%s:[%d] service & aps_ops is null.\n",__func__,__LINE__);
        return BAD_VALUE;
	}

	dev = (struct xb47xx_ap_device *)device;

    lpol = (struct xb47xx_audio_policy *)calloc(1, sizeof(*lpol));
    if (!lpol)
        return NO_MEMORY;

	lpol->policy.set_device_connection_state = ap_set_device_connection_state;
	lpol->policy.get_device_connection_state = ap_get_device_connection_state;
	lpol->policy.set_phone_state = ap_set_phone_state;
	lpol->policy.set_ringer_mode = ap_set_ringer_mode;
	lpol->policy.set_force_use = ap_set_force_use;
	lpol->policy.get_force_use = ap_get_force_use;
	lpol->policy.set_can_mute_enforced_audible = ap_set_can_mute_enforced_audible;
	lpol->policy.init_check = ap_init_check;
	lpol->policy.get_output = ap_get_output;
	lpol->policy.start_output = ap_start_output;
	lpol->policy.stop_output = ap_stop_output;
	lpol->policy.release_output = ap_release_output;
	lpol->policy.get_input = ap_get_input;
	lpol->policy.start_input = ap_start_input;
	lpol->policy.stop_input = ap_stop_input;
	lpol->policy.release_input = ap_release_input;
	lpol->policy.init_stream_volume = ap_init_stream_volume;
	lpol->policy.set_stream_volume_index = ap_set_stream_volume_index;
	lpol->policy.get_stream_volume_index = ap_get_stream_volume_index;
	lpol->policy.get_strategy_for_stream = ap_get_strategy_for_stream;
	lpol->policy.get_devices_for_stream = ap_get_devices_for_stream;
	lpol->policy.get_output_for_effect = ap_get_output_for_effect;
	lpol->policy.register_effect = ap_register_effect;
	lpol->policy.unregister_effect = ap_unregister_effect;
	lpol->policy.set_effect_enabled = ap_set_effect_enabled;
	lpol->policy.is_stream_active = ap_is_stream_active;
	lpol->policy.dump = ap_dump;
	lpol->service = service;
	lpol->aps_ops = aps_ops;

    *ap = &lpol->policy;

	for (i = 0; i < AUDIO_STREAM_CNT; i++) {
		lpol->ap_prop.ap_streams[i].id = i;
		lpol->ap_prop.ap_streams[i].indexMin = 0;
		lpol->ap_prop.ap_streams[i].indexMax = 0;
		lpol->ap_prop.ap_streams[i].indexCur = 0;
		lpol->ap_prop.ap_streams[i].canBeMuted = false;
		lpol->ap_prop.ap_streams[i].refCount = 0;
	}

	lpol->ap_prop.ap_outputs = NULL;
	lpol->ap_prop.ap_inputs = NULL;
	lpol->ap_prop.primaryOutput = NULL;
	lpol->ap_prop.a2dpOutput = NULL;
	lpol->ap_prop.duplicatedOutput = NULL;
	for (i = 0; i < AUDIO_POLICY_FORCE_USE_CNT; i++)
		lpol->ap_prop.forceUseConf[i] = AUDIO_POLICY_FORCE_NONE;
	lpol->ap_prop.availableOutputDevices = AUDIO_DEVICE_OUT_SPEAKER;
	lpol->ap_prop.availableInputDevices = AUDIO_DEVICE_IN_BUILTIN_MIC;
	lpol->ap_prop.scoDeviceAddress[0] = '0';
	lpol->ap_prop.a2dpDeviceAddress[0] = '0';
	lpol->ap_prop.ringerMode = AUDIO_MODE_RINGTONE;
	lpol->ap_prop.phoneState = AUDIO_MODE_NORMAL;
	lpol->ap_prop.forceVolumeReeval = false;
	lpol->ap_prop.lastVoiceVolume = -1.0f;
	lpol->ap_prop.a2dp_is_suspend = true;

	for (i = 0 ;i < ARRAY_SIZE(audio_interfaces);i++) {
		if (init_hardware_module(lpol,audio_interfaces[i]))
			continue;
		lpol->ap_prop.hwmodule->handle =
			aps_ops->load_hw_module(service,audio_interfaces[i]);
		output_desc = (struct output_descriptor *)calloc(1,sizeof(*output_desc));
		if (!output_desc)
			return NO_MEMORY;
		output_desc->handle = lpol->ap_prop.hwmodule->handle;
		output_desc->flags = lpol->ap_prop.hwmodule->profile->flags;
		output_desc->support_device = lpol->ap_prop.hwmodule->profile->devices;
		output_desc->output1 = NULL;
		output_desc->output2 = NULL;

		if (output_desc->support_device & AUDIO_ATTACHEDOUTPUTDEVICES) {
			output_desc->device = output_desc->support_device & AUDIO_ATTACHEDOUTPUTDEVICES;
open_again:
			output_desc->id = aps_ops->open_output_on_module(service,
						output_desc->handle,
						&output_desc->device,
						&output_desc->samplingRate,
						&output_desc->format,
						&output_desc->channels,
						&output_desc->latency,
						output_desc->flags);
			if (output_desc->id == 0) {
				if (retry_time < 5) {
					usleep(100000);
					retry_time++;
					/*Android restart release can be after open,
					 *So the device is busy,
					 *We just try again after 100ms.*/
					goto open_again;
				}
				ALOGE("%s:[%d] open_output is fail.\n",__func__,__LINE__);
				free(output_desc);
				output_desc = NULL;
				return NO_INIT;
			} else {
				retry_time = 0;
				for (i = 0; i < NUM_STRATEGIES ; i++)
					lpol->ap_prop.strategy_cache[i] = output_desc->support_device &
							AUDIO_ATTACHEDOUTPUTDEVICES;
				for (i = 0; i < AUDIO_STREAM_CNT; i++) {
					output_desc->oStream[i] = &lpol->ap_prop.ap_streams[i];
					output_desc->stopTime[i] = 0;
				}
				lpol->ap_prop.availableOutputDevices = lpol->ap_prop.availableOutputDevices |
					(output_desc->support_device & AUDIO_ATTACHEDOUTPUTDEVICES);
				output_desc->refCount = 0;
				output_desc->next = NULL;

				if (output_desc->flags & AUDIO_OUTPUT_FLAG_PRIMARY &&
						lpol->ap_prop.primaryOutput == NULL)
					lpol->ap_prop.primaryOutput = output_desc;

				if (lpol->ap_prop.ap_outputs == NULL)
					lpol->ap_prop.ap_outputs = output_desc;
				else
					lpol->ap_prop.ap_outputs->next = output_desc;

				set_output_device(&lpol->policy,
						output_desc->device,
						true,
						0,
						output_desc);
			}
		} else {
			free(output_desc);
			output_desc = NULL;
		}
	}

	update_device_for_strategy(*ap);

	return NO_ERROR;
}

static int destroy_xb47xx_ap(const struct audio_policy_device *ap_dev,
                              struct audio_policy *ap)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): ap_dev = %p, ap = %p",
			__func__, __LINE__, ap_dev, ap);
#endif

    const struct xb47xx_ap_device *dev = (struct xb47xx_ap_device *)ap_dev;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)ap;
	struct output_descriptor *po;
	struct input_descriptor *pi;

	if (ap_dev == NULL || ap == NULL)
		return NO_INIT;
	while (lpol->ap_prop.ap_inputs) {
		pi = lpol->ap_prop.ap_inputs;
		lpol->ap_prop.ap_inputs = lpol->ap_prop.ap_inputs->next;
		free(pi);
		pi = NULL;
	}
	while (lpol->ap_prop.ap_outputs) {
		po = lpol->ap_prop.ap_outputs;
		lpol->ap_prop.ap_outputs = lpol->ap_prop.ap_outputs->next;
		free(po);
		po = NULL;
	}

    free(lpol);
	lpol =	NULL;
    return NO_ERROR;
}

/* #################################################################### *\
|* module open & close
\* #################################################################### */

static int xb47xx_ap_dev_close(hw_device_t* device)
{
#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): device = %p",
		 __func__, __LINE__, device);
#endif

    free(device);
	device = NULL;
    return NO_ERROR;
}

static int xb47xx_ap_dev_open(const hw_module_t* module, const char* name,
                               hw_device_t** device)
{
    struct xb47xx_ap_device *dev;

#ifdef POLICY_DEBUG
	ALOGV("%s(line:%d): ...",__func__, __LINE__);
#endif

    *device = NULL;

    if (strcmp(name, AUDIO_POLICY_INTERFACE) != 0) {
		ALOGE("%s:[%d] moudle is not AUDIO_POLICY_INTERFACE.\n",__func__,__LINE__);
        return BAD_VALUE;
	}

    dev = (struct xb47xx_ap_device *)calloc(1, sizeof(*dev));
    if (!dev)
        return NO_MEMORY;

    dev->device.common.tag = HARDWARE_DEVICE_TAG;
    dev->device.common.version = 0;
    dev->device.common.module = (hw_module_t *)module;
    dev->device.common.close = xb47xx_ap_dev_close;
    dev->device.create_audio_policy = create_xb47xx_ap;
    dev->device.destroy_audio_policy = destroy_xb47xx_ap;

    *device = &dev->device.common;

    return NO_ERROR;
}

static struct hw_module_methods_t xb47xx_ap_module_methods = {
    .open = xb47xx_ap_dev_open,
};

struct xb47xx_ap_module HAL_MODULE_INFO_SYM = {
    .module = {
        .common = {
            .tag            = HARDWARE_MODULE_TAG,
            .version_major  = 1,
            .version_minor  = 0,
            .id             = AUDIO_POLICY_HARDWARE_MODULE_ID,
            .name           = "Xb47xx audio policy HAL for MID",
            .author         = "ingenic",
            .methods        = &xb47xx_ap_module_methods,
        },
    },
};
