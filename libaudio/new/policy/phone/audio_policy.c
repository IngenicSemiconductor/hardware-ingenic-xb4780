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

#define LOG_TAG "audio_policy_xb47xx"
//#define LOG_NDEBUG 0
//#define LOGV LOGD

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cutils/log.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <system/audio_policy.h>
#include <hardware/audio_policy.h>

#include "audio_policy.h"

struct xb47xx_ap_module {
    struct audio_policy_module module;
};

struct xb47xx_ap_device {
    struct audio_policy_device device;
};

struct xb47xx_audio_policy {
    struct audio_policy policy;
    struct audio_policy_service_ops *aps_ops;
	struct stream_descriptor ap_streams[AUDIO_STREAM_CNT];
	struct output_descriptor *ap_outputs;
	struct input_descriptor  *ap_inputs;
	struct audio_policy_state ap_state;
	audio_policy_forced_cfg_t forceUseConf[AUDIO_POLICY_FORCE_USE_CNT];
    void *service;
};

/* #################################################################### *\
|* private functions
\* #################################################################### */

static bool ap_isStateInCall(int state) {
    return ((state == AUDIO_MODE_IN_CALL) ||
            (state == AUDIO_MODE_IN_COMMUNICATION));
}

static bool ap_isInCall(struct xb47xx_audio_policy *lpol)
{
    return ap_isStateInCall(lpol->ap_state.phoneState);
}

static uint32_t getDeviceForInputSource(int inputSource)
{
    return 0;
}

static uint32_t getNewOutputDevice() {
	return 0;
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
	ALOGV("%s(line:%d): pol = %p, device = %d, state = %d, device_address = %s",
		 __func__, __LINE__, pol, device, state, device_address);

	char param[128] = "\0";
	int delay_ms = 0;
	struct input_descriptor* inputDesc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct input_descriptor* pi = lpol->ap_inputs;

	if (audio_is_output_device(device)) {
		// store ap_state
		switch (state) {
		case AUDIO_POLICY_DEVICE_STATE_AVAILABLE:
			lpol->ap_state.availableOutputDevices |= device;
			if (audio_is_bluetooth_sco_device(device)) {
				int len = strlen(device_address);
				if (len >= MAX_A2DP_ADD_LEN) {
					return -1;
				} else {
					strcpy(lpol->ap_state.scoDeviceAddress, device_address);
				}
			}
			break;
		case AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE:
			lpol->ap_state.availableOutputDevices &= ~device;
			if (audio_is_bluetooth_sco_device(device)) {
				lpol->ap_state.scoDeviceAddress[0] = '\0';
			}
			break;
		default:
			return -1;
		}

		// set route
		{
			audio_io_handle_t io_handle = 0;
			char *kv_pairs = NULL;
			int delay_ms = 0;
			lpol->aps_ops->set_parameters(lpol->service, io_handle, kv_pairs, delay_ms);
		}
	}

	if (device == AUDIO_DEVICE_OUT_WIRED_HEADSET) {
		device = AUDIO_DEVICE_IN_WIRED_HEADSET;
	} else if (device == AUDIO_DEVICE_OUT_BLUETOOTH_SCO ||
			   device == AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET ||
			   device == AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT) {
		device = AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
	}

	if (audio_is_input_device(device)) {
		// store ap_state
		switch (state) {
		case AUDIO_POLICY_DEVICE_STATE_AVAILABLE:
			lpol->ap_state.availableInputDevices |= device;
			break;
		case AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE:
			lpol->ap_state.availableInputDevices &= ~device;
			break;
		default:
			return -1;
		}

		// set route
		while(pi) {
			if (pi->refCount >= 1) {
				inputDesc = pi;
				break;
			}
			pi = pi->next;
		}

		if (inputDesc != NULL) {
			inputDesc->device = getDeviceForInputSource(inputDesc->inputSource);
			param[0] = '\0';
			delay_ms = 0;
			sprintf(param, "%s=%d",
					AUDIO_PARAMETER_STREAM_ROUTING,	inputDesc->device);
			lpol->aps_ops->set_parameters(lpol->service,
										  inputDesc->id, param, delay_ms);
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
	ALOGV("%s(line:%d): pol = %p, device = %d, device_address = %s",
		 __func__, __LINE__, pol, device, device_address);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	int state = AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE;

	if (audio_is_output_device(device)) {
		if (device & lpol->ap_state.availableOutputDevices) {
			if (audio_is_bluetooth_sco_device(device) &&
				device_address != NULL &&
				strcmp(device_address, lpol->ap_state.scoDeviceAddress)) {
				state = AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE;
			} else {
				state = AUDIO_POLICY_DEVICE_STATE_AVAILABLE;
			}
		}
	} else if (audio_is_input_device(device)) {
		if (device & lpol->ap_state.availableInputDevices) {
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
	ALOGV("%s(line:%d): pol = %p, state = %d",
		 __func__, __LINE__, pol, state);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	if (state < 0 || state >= AUDIO_MODE_CNT) {
		ALOGW("%s(line:%d): invalid state %d",
		 __func__, __LINE__, state);
		return;
	}

	if (state == lpol->ap_state.phoneState) {
		LOGW("%s(line:%d): setting same state %d",
		 __func__, __LINE__, state);
		return;
	}

	/* if leaving call state, handle special case of active streams
	   pertaining to sonification strategy see handleIncallSonification() */
	if (ap_isInCall(lpol)) {
        ALOGV("%s(line:%d): in call state management: new state is %d",
			 __func__, __LINE__, state);
		/*
        for (int stream = 0; stream < AudioSystem::NUM_STREAM_TYPES; stream++) {
            handleIncallSonification(stream, false, true);
        }
		*/
	}

	/* store previous phone state for management of sonification strategy below */
	int oldState = lpol->ap_state.phoneState;
    lpol->ap_state.phoneState = state;
    bool force = false;

	/* are we entering or starting a call */
    if (!ap_isStateInCall(oldState) && ap_isStateInCall(state)) {
        ALOGV("%s(line:%d): Entering call", __func__, __LINE__);
        /* force routing command to audio hardware when starting a call
		  even if no device change is needed */
        force = true;
    } else if (ap_isStateInCall(oldState) && !ap_isStateInCall(state)) {
        ALOGV("%s(line:%d): Exiting call", __func__, __LINE__);
        /* force routing command to audio hardware when exiting a call
		   even if no device change is needed */
        force = true;
    } else if (ap_isStateInCall(state) && (state != oldState)) {
        ALOGV("%s(line:%d): Switching between telephony and VoIP",
			__func__, __LINE__);
        /* force routing command to audio hardware when
		   switching between telephony and VoIP
		   even if no device change is needed */
        force = true;
    }

	/* check for device and output changes triggered by new phone state */
}

/**
 * indicate a change in ringer mode
 */
static void ap_set_ringer_mode(struct audio_policy *pol, uint32_t mode,
                               uint32_t mask)
{
	ALOGV("%s(line:%d): pol = %p, mode = %d, mask = %d",
		 __func__, __LINE__, pol, mode, mask);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	lpol->ap_state.ringerMode = mode;
}

/**
 * force using a specific device category for
 * the specified usage
 */
static void ap_set_force_use(struct audio_policy *pol,
                          audio_policy_force_use_t usage,
                          audio_policy_forced_cfg_t config)
{
	ALOGV("%s(line:%d): pol = %p, usage = %d, config = %d",
		 __func__, __LINE__, pol, usage, config);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	switch(usage) {
	case AUDIO_POLICY_FORCE_FOR_COMMUNICATION:
        if (config != AUDIO_POLICY_FORCE_SPEAKER &&
			config != AUDIO_POLICY_FORCE_BT_SCO &&
			config != AUDIO_POLICY_FORCE_NONE) {
            LOGW("%s(line:%d): invalid config %d for FOR_COMMUNICATION",
				 __func__, __LINE__, config);
            return;
        }
        lpol->ap_state.forceVolumeReeval = true;
        lpol->forceUseConf[usage] = config;
		break;
    case AUDIO_POLICY_FORCE_FOR_MEDIA:
        if (config != AUDIO_POLICY_FORCE_HEADPHONES &&
			config != AUDIO_POLICY_FORCE_BT_A2DP &&
            config != AUDIO_POLICY_FORCE_WIRED_ACCESSORY &&
            config != AUDIO_POLICY_FORCE_ANALOG_DOCK &&
            config != AUDIO_POLICY_FORCE_DIGITAL_DOCK &&
			config != AUDIO_POLICY_FORCE_NONE) {
            LOGW("%s(line:%d): invalid config %d for FOR_MEDIA",
				 __func__, __LINE__, config);
            return;
        }
        lpol->forceUseConf[usage] = config;
		break;
    case AUDIO_POLICY_FORCE_FOR_RECORD:
        if (config != AUDIO_POLICY_FORCE_BT_SCO &&
			config != AUDIO_POLICY_FORCE_WIRED_ACCESSORY &&
            config != AUDIO_POLICY_FORCE_NONE) {
            LOGW("%s(line:%d): invalid config %d for FOR_RECORD",
				 __func__, __LINE__, config);
            return;
        }
        lpol->forceUseConf[usage] = config;
		break;
    case AUDIO_POLICY_FORCE_FOR_DOCK:
        if (config != AUDIO_POLICY_FORCE_BT_CAR_DOCK &&
            config != AUDIO_POLICY_FORCE_BT_DESK_DOCK &&
            config != AUDIO_POLICY_FORCE_WIRED_ACCESSORY &&
            config != AUDIO_POLICY_FORCE_ANALOG_DOCK &&
            config != AUDIO_POLICY_FORCE_DIGITAL_DOCK &&
			config != AUDIO_POLICY_FORCE_NONE) {
            LOGW("%s(line:%d): invalid config %d for FOR_DOCK",
				 __func__, __LINE__, config);
        }
        lpol->ap_state.forceVolumeReeval = true;
        lpol->forceUseConf[usage] = config;
		break;
	default:
		LOGW("%s(line:%d): invalid usage %d",
			 __func__, __LINE__, usage);
	}

	/*check for device and output changes
	  triggered by new phone state */
}

/**
 * retreive current device category forced
 * for a given usage
 */
static audio_policy_forced_cfg_t ap_get_force_use(const struct audio_policy *pol,
												  audio_policy_force_use_t usage)
{
	ALOGV("%s(line:%d): pol = %p, usage = %d",
		 __func__, __LINE__, pol, usage);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    return lpol->forceUseConf[usage];
}

/**
 * if can_mute is true, then audio streams that are
 * marked ENFORCED_AUDIBLE can still be muted.
 */
static void ap_set_can_mute_enforced_audible(struct audio_policy *pol,
                                             bool can_mute)
{
	ALOGV("%s(line:%d): pol = %p, can_mute = %s",
		 __func__, __LINE__, pol, can_mute ? "true" : "false");

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	if (can_mute) {
		lpol->ap_streams[AUDIO_STREAM_ENFORCED_AUDIBLE].canBeMuted = true;
	} else {
		lpol->ap_streams[AUDIO_STREAM_ENFORCED_AUDIBLE].canBeMuted = false;
	}
}

/**
 * check proper initialization
 */
static int ap_init_check(const struct audio_policy *pol)
{
	ALOGV("%s(line:%d): pol = %p",
		 __func__, __LINE__, pol);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    return 0;
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
                                       uint32_t format,
                                       uint32_t channels,
                                       audio_policy_output_flags_t flags)
{
	ALOGV("%s(line:%d): pol = %p, stream = %d, sampling_rate = %d, format = %d, channels = %d, flags = %d",
		 __func__, __LINE__, pol, stream, sampling_rate, format, channels, flags);

    audio_io_handle_t output = 0;
	uint32_t device = 0;
	struct output_descriptor *outputDesc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	//get device
	device = pol->get_devices_for_stream(pol, stream);

	//open a direct output
	if ((flags & AUDIO_POLICY_OUTPUT_FLAG_DIRECT) ||
		((format != 0) && !audio_is_linear_pcm(format))) {
		//output_descriptor
		outputDesc = (struct output_descriptor *)calloc(1, sizeof(*outputDesc));

		outputDesc->stream = stream;
		outputDesc->samplingRate = sampling_rate;
		outputDesc->format = format;
		outputDesc->channels = channels;
		outputDesc->device = device;
		outputDesc->flags = flags | AUDIO_POLICY_OUTPUT_FLAG_DIRECT;
		outputDesc->latency = 0;
		outputDesc->refCount = 0;
		outputDesc->stopTime = 0;
		outputDesc->next = NULL;

		output = lpol->aps_ops->open_output(lpol->service,
											&outputDesc->device,
											&outputDesc->samplingRate,
											&outputDesc->format,
											&outputDesc->channels,
											&outputDesc->latency,
											outputDesc->flags);

		if ((output == 0) ||
			((sampling_rate != 0) && (sampling_rate != outputDesc->samplingRate)) ||
			((format != 0) && (format != outputDesc->format)) ||
			((channels != 0) && (channels != outputDesc->channels))) {
			//close output
			if (output) {
				lpol->aps_ops->close_output(lpol->service, output);
			}
			free(outputDesc);
			return -1;
		}

		outputDesc->id = output;

		if (lpol->ap_outputs == NULL) {
			lpol->ap_outputs = outputDesc;
		} else {
			outputDesc->next = lpol->ap_outputs;
			lpol->ap_outputs = outputDesc;
		}

		return outputDesc->id;
	}

	//open a non direct output
	if ((channels != 0) &&
		(channels != AUDIO_CHANNEL_OUT_MONO) &&
		(channels != AUDIO_CHANNEL_OUT_STEREO)) {
		return -1;
	}

    return lpol->ap_state.hardwareOutput;
}

/**
 * indicates to the audio policy manager that the
 * output starts being used by corresponding stream.
 */
static int ap_start_output(struct audio_policy *pol, audio_io_handle_t output,
                           audio_stream_type_t stream, int session)
{
	ALOGV("%s(line:%d): pol = %p, output = %d, stream = %d, session = %d",
		 __func__, __LINE__, pol, output, stream, session);

	struct output_descriptor *outputDesc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct output_descriptor *p = lpol->ap_outputs;

	// output_descriptorp
	while(p) {
		if (output == p->id) {
			outputDesc = p;
			break;
		}
		p = p->next;
	}

	// output not be opened
	if (outputDesc == NULL)
		return -1;

    return -ENOSYS;
}

/**
 * indicates to the audio policy manager that the
 * output stops being used by corresponding stream.
 */
static int ap_stop_output(struct audio_policy *pol, audio_io_handle_t output,
                          audio_stream_type_t stream, int session)
{
	ALOGV("%s(line:%d): pol = %p, output = %d, stream = %d, session = %d",
		 __func__, __LINE__, pol, output, stream, session);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    return -ENOSYS;
}

/**
 * releases the output.
 */
static void ap_release_output(struct audio_policy *pol,
                              audio_io_handle_t output)
{
	ALOGV("%s(line:%d): pol = %p, output = %d",
		 __func__, __LINE__, pol, output);

	struct output_descriptor *outputDesc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct output_descriptor *p = lpol->ap_outputs;
	struct output_descriptor *pre = p;

	// del output
	if (output == p->id) {
		if (p->flags & AUDIO_POLICY_OUTPUT_FLAG_DIRECT) {
			lpol->ap_outputs = p->next;
			free(p);
			return;
		}
	}

	while(p) {
		pre = p;
		p = p->next;
		if (output == p->id) {
			if (p->flags & AUDIO_POLICY_OUTPUT_FLAG_DIRECT) {
				pre->next = p->next;
				free(p);
				break;
			}
		}
	}
}

/**
 * request an input appriate for record from the
 * supplied device with supplied parameters.
 */
static audio_io_handle_t ap_get_input(struct audio_policy *pol, int inputSource,
                                      uint32_t sampling_rate,
                                      uint32_t format,
                                      uint32_t channels,
                                      audio_in_acoustics_t acoustics)
{
	ALOGV("%s(line:%d): pol = %p, inputSource = %d, sampling_rate = %d, format = %d, channels = %d, acoustics = %d",
		 __func__, __LINE__, pol, inputSource, sampling_rate, format, channels, acoustics);

	audio_io_handle_t input = 0;
	uint32_t device = 0;
	struct input_descriptor *inputDesc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	// channels
	switch (inputSource) {
	case AUDIO_SOURCE_VOICE_UPLINK:
		channels = AUDIO_CHANNEL_IN_VOICE_UPLINK;
		break;
	case AUDIO_SOURCE_VOICE_DOWNLINK:
		channels = AUDIO_CHANNEL_IN_VOICE_DNLINK;
		break;
	case AUDIO_SOURCE_VOICE_CALL:
		channels = AUDIO_CHANNEL_IN_VOICE_UPLINK |
			AUDIO_CHANNEL_IN_VOICE_DNLINK;
		break;
	}

	//input_descriptor
	inputDesc = (struct input_descriptor *)calloc(1, sizeof(*inputDesc));
	if (inputDesc == NULL)
		return -1;

	inputDesc->inputSource = inputSource;
	inputDesc->samplingRate = sampling_rate;
	inputDesc->format = format;
	inputDesc->channels = channels;
	inputDesc->acoustics = acoustics;
	inputDesc->device = getDeviceForInputSource(inputSource);
	inputDesc->refCount = 0;
	inputDesc->next = NULL;

	input = lpol->aps_ops->open_input(lpol->service,
									  &inputDesc->device,
									  &inputDesc->samplingRate,
									  &inputDesc->format,
									  &inputDesc->channels,
									  inputDesc->acoustics);

	if ((input == 0) ||
		(sampling_rate != inputDesc->samplingRate) ||
		(format != inputDesc->format) ||
		(channels != inputDesc->channels)) {

		if (input) {
			lpol->aps_ops->close_input(lpol->service, input);
		}
		free(inputDesc);
		return -1;
	}

	inputDesc->id = input;

	if (lpol->ap_inputs == NULL) {
		lpol->ap_inputs = inputDesc;
	} else {
		inputDesc->next = lpol->ap_inputs;
		lpol->ap_inputs = inputDesc;
	}

    return inputDesc->id;
}

/**
 * indicates to the audio policy manager that
 * the input starts being used
 */
static int ap_start_input(struct audio_policy *pol, audio_io_handle_t input)
{
	ALOGV("%s(line:%d): pol = %p, input = %d",
		 __func__, __LINE__, pol, input);

	char param[128] = "\0";
	int delay_ms = 0;
	struct input_descriptor* inputDesc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct input_descriptor* p = lpol->ap_inputs;

	while(p) {
		// an input already started
		if (p->refCount >= 1)
			return -1;
		if (input == p->id)
			inputDesc = p;
		p = p->next;
	}

	// input not be opened
	if (inputDesc == NULL)
		return -1;

	// set route
	sprintf(param, "%s=%d;%s=%d",
			AUDIO_PARAMETER_STREAM_ROUTING,	inputDesc->device,
			AUDIO_PARAMETER_STREAM_INPUT_SOURCE, inputDesc->inputSource);
	lpol->aps_ops->set_parameters(lpol->service, input, param, delay_ms);

	// set refCount
	inputDesc->refCount += 1;

    return 0;
}

/**
 * indicates to the audio policy manager that
 * the input stops being used.
 */
static int ap_stop_input(struct audio_policy *pol, audio_io_handle_t input)
{
	ALOGV("%s(line:%d): pol = %p, input = %d",
		 __func__, __LINE__, pol, input);

	char param[128] = "\0";
	int delay_ms = 0;
	struct input_descriptor *inputDesc = NULL;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct input_descriptor *p = lpol->ap_inputs;

	while(p) {
		if (input == p->id) {
			inputDesc = p;
			break;
		}
		p = p->next;
	}

	// input not be opened
	if (inputDesc == NULL)
		return -1;

	// input not started
	if (inputDesc->refCount == 0)
		return -1;

	// set route
	sprintf(param, "%s=%d",
			AUDIO_PARAMETER_STREAM_ROUTING,	0);
	lpol->aps_ops->set_parameters(lpol->service, input, param, delay_ms);

    return 0;
}

/**
 * releases the input.
 */
static void ap_release_input(struct audio_policy *pol, audio_io_handle_t input)
{
	ALOGV("%s(line:%d): pol = %p, input = %d",
		 __func__, __LINE__, pol, input);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;
	struct input_descriptor* p = lpol->ap_inputs;
	struct input_descriptor* pre = p;

	// close input
	lpol->aps_ops->close_input(lpol->service, input);

	// del input
	if (input == p->id) {
		lpol->ap_inputs = p->next;
		free(p);
		return;
	}

	while(p) {
		pre = p;
		p = p->next;
		if (input == p->id) {
			pre->next = p->next;
			free(p);
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
	ALOGV("%s(line:%d): pol = %p, stream = %d, index_min = %d, index_max",
		 __func__, __LINE__, pol, stream, index_min, index_max);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    if (index_min < 0 || index_min >= index_max) {
        LOGW("%s(line:%d): invalid index limits for stream %d, min %d, max %d",
			 __func__, __LINE__, stream, index_min, index_max);
        return;
    }

	lpol->ap_streams[stream].indexMin = index_min;
	lpol->ap_streams[stream].indexMax = index_max;
}

/**
 * sets the new stream volume at a level corresponding
 * to the supplied index
 */
static int ap_set_stream_volume_index(struct audio_policy *pol,
                                      audio_stream_type_t stream,
                                      int index)
{
	ALOGV("%s(line:%d): pol = %p, stream = %d, index = %d",
		 __func__, __LINE__, pol, stream, index);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    if ((index < lpol->ap_streams[stream].indexMin)
		|| (index > lpol->ap_streams[stream].indexMax)) {
        return -1;
    }

    /* Force max volume if stream cannot be muted */
    if (!lpol->ap_streams[stream].canBeMuted) {
		index = lpol->ap_streams[stream].indexMax;
	}

    /* compute and apply stream volume on all
	   outputs according to connected device */

    return -ENOSYS;
}

/**
 * retreive current volume index for the
 * specified stream
 */
static int ap_get_stream_volume_index(const struct audio_policy *pol,
                                      audio_stream_type_t stream,
                                      int *index)
{
	ALOGV("%s(line:%d): pol = %p, stream = %d",
		 __func__, __LINE__, pol, stream);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

	if (index == NULL)
		return -1;

	*index = lpol->ap_streams[stream].indexCur;

    return 0;
}

/**
 * return the strategy corresponding to a
 * given stream type
 */
static uint32_t ap_get_strategy_for_stream(const struct audio_policy *pol,
                                           audio_stream_type_t stream)
{
	ALOGV("%s(line:%d): pol = %p, stream = %d",
		 __func__, __LINE__, pol, stream);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    /* stream to strategy mapping */
    switch (stream) {
    case AUDIO_STREAM_VOICE_CALL:
    case AUDIO_STREAM_BLUETOOTH_SCO:
        return STRATEGY_PHONE;
    case AUDIO_STREAM_RING:
    case AUDIO_STREAM_NOTIFICATION:
    case AUDIO_STREAM_ALARM:
        return STRATEGY_SONIFICATION;
    case AUDIO_STREAM_DTMF:
        return STRATEGY_DTMF;
    case AUDIO_STREAM_SYSTEM:
        /* NOTE: SYSTEM stream uses MEDIA strategy
		   because muting music and switching outputs
		   while key clicks are played produces a poor
		   result */
    case AUDIO_STREAM_TTS:
    case AUDIO_STREAM_MUSIC:
        return STRATEGY_MEDIA;
    case AUDIO_STREAM_ENFORCED_AUDIBLE:
        return STRATEGY_ENFORCED_AUDIBLE;
    default:
        ALOGE("%s(line:%d): unknown stream type", __func__, __LINE__);
		return -1;
    }
}

/**
 * return the enabled output devices for
 * the given stream type
 */
static uint32_t ap_get_devices_for_stream(const struct audio_policy *pol,
                                          audio_stream_type_t stream)
{
	ALOGV("%s(line:%d): pol = %p, stream = %d",
		 __func__, __LINE__, pol, stream);

	uint32_t devices;
	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    /* By checking the range of stream before calling getStrategy, we avoid
	   getStrategy's behavior for invalid streams.  getStrategy would do a LOGE
	   and then return STRATEGY_MEDIA, but we want to return the empty set.*/
    if (stream < 0 || stream >= AUDIO_STREAM_CNT) {
        devices = 0;
    } else {
        enum routing_strategy strategy = pol->get_strategy_for_stream(pol, stream);

		switch (strategy) {
		case STRATEGY_MEDIA:
			devices = 0;
			break;
		case STRATEGY_PHONE:
			devices = 0;
			break;
		case STRATEGY_SONIFICATION:
			devices = 0;
			break;
		case STRATEGY_DTMF:
			devices = 0;
			break;
		case STRATEGY_ENFORCED_AUDIBLE:
			devices = 0;
			break;
		case NUM_STRATEGIES:
			devices = 0;
			break;
		default:
			LOGE("%s(line:%d): unknown strategy", __func__, __LINE__);
		}
    }
    return devices;
}

/**
 * Audio effect management
 */
static audio_io_handle_t ap_get_output_for_effect(struct audio_policy *pol,
                                            struct effect_descriptor_s *desc)
{
	ALOGV("%s(line:%d): pol = %p, desc = %p",
		 __func__, __LINE__, pol, desc);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    return 0;
}

static int ap_register_effect(struct audio_policy *pol,
                              struct effect_descriptor_s *desc,
                              audio_io_handle_t output,
                              uint32_t strategy,
                              int session,
                              int id)
{
	ALOGV("%s(line:%d): pol = %p, desc = %p, output = %d, strategy = %d, session = %d, id = %d",
		 __func__, __LINE__, pol, desc, output, strategy, session, id);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    return -ENOSYS;
}

static int ap_unregister_effect(struct audio_policy *pol, int id)
{
	ALOGV("%s(line:%d): pol = %p, id = %d",
		 __func__, __LINE__, pol, id);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    return -ENOSYS;
}

static int ap_set_effect_enabled(struct audio_policy *pol, int id, bool enabled)
{
	ALOGV("%s(line:%d): pol = %p, enabled = %s",
		 __func__, __LINE__, pol, enabled ? "true" : "false");

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    return -ENOSYS;
}

static bool ap_is_stream_active(const struct audio_policy *pol, int stream,
                                uint32_t in_past_ms)
{
	ALOGV("%s(line:%d): pol = %p, stream = %d, in_past_ms = %d",
		 __func__, __LINE__, pol, stream, in_past_ms);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    return false;
}

/**
 * dump state
 */
static int ap_dump(const struct audio_policy *pol, int fd)
{
	ALOGV("%s(line:%d): pol = %p, fd = %d",
		 __func__, __LINE__, pol, fd);

	struct xb47xx_audio_policy *lpol = (struct xb47xx_audio_policy *)pol;

    return -ENOSYS;
}

/* #################################################################### *\
|* struct audio_policy_device
\* #################################################################### */

static int create_xb47xx_ap(const struct audio_policy_device *device,
                             struct audio_policy_service_ops *aps_ops,
                             void *service,
                             struct audio_policy **ap)
{
    const struct xb47xx_ap_device *dev;
    struct xb47xx_audio_policy *dap;
    int ret;

	ALOGV("%s(line:%d): device = %p, aps_ops = %p, service = %p, ap = %p",
		 __func__, __LINE__, device, aps_ops, service, *ap);

    *ap = NULL;

    if (!service || !aps_ops)
        return -EINVAL;

	dev = (struct xb47xx_ap_device *)device;

    dap = (struct xb47xx_audio_policy *)calloc(1, sizeof(*dap));
    if (!dap)
        return -ENOMEM;

    dap->policy.set_device_connection_state = ap_set_device_connection_state;
    dap->policy.get_device_connection_state = ap_get_device_connection_state;
    dap->policy.set_phone_state = ap_set_phone_state;
    dap->policy.set_ringer_mode = ap_set_ringer_mode;
    dap->policy.set_force_use = ap_set_force_use;
    dap->policy.get_force_use = ap_get_force_use;
    dap->policy.set_can_mute_enforced_audible =
        ap_set_can_mute_enforced_audible;
    dap->policy.init_check = ap_init_check;
    dap->policy.get_output = ap_get_output;
    dap->policy.start_output = ap_start_output;
    dap->policy.stop_output = ap_stop_output;
    dap->policy.release_output = ap_release_output;
    dap->policy.get_input = ap_get_input;
    dap->policy.start_input = ap_start_input;
    dap->policy.stop_input = ap_stop_input;
    dap->policy.release_input = ap_release_input;
    dap->policy.init_stream_volume = ap_init_stream_volume;
    dap->policy.set_stream_volume_index = ap_set_stream_volume_index;
    dap->policy.get_stream_volume_index = ap_get_stream_volume_index;
    dap->policy.get_strategy_for_stream = ap_get_strategy_for_stream;
    dap->policy.get_devices_for_stream = ap_get_devices_for_stream;
    dap->policy.get_output_for_effect = ap_get_output_for_effect;
    dap->policy.register_effect = ap_register_effect;
    dap->policy.unregister_effect = ap_unregister_effect;
    dap->policy.set_effect_enabled = ap_set_effect_enabled;
    dap->policy.is_stream_active = ap_is_stream_active;
    dap->policy.dump = ap_dump;

    dap->service = service;
    dap->aps_ops = aps_ops;

    *ap = &dap->policy;
    return 0;
}

static int destroy_xb47xx_ap(const struct audio_policy_device *ap_dev,
                              struct audio_policy *ap)
{
	ALOGV("%s(line:%d): ap_dev = %p, ap = %p",
		 __func__, __LINE__, ap_dev, ap);

    const struct xb47xx_ap_device *dev = (struct xb47xx_ap_device *)ap_dev;

    free(ap);
    return 0;
}

/* #################################################################### *\
|* module open & close
\* #################################################################### */

static int xb47xx_ap_dev_close(hw_device_t* device)
{
	ALOGV(device, "%s(line:%d): ...",
		 __func__, __LINE__);

    free(device);
    return 0;
}

static int xb47xx_ap_dev_open(const hw_module_t* module, const char* name,
                               hw_device_t** device)
{
    struct xb47xx_ap_device *dev;

	ALOGV("%s(line:%d): ...",
		 __func__, __LINE__);

    *device = NULL;

    if (strcmp(name, AUDIO_POLICY_INTERFACE) != 0)
        return -EINVAL;

    dev = (struct xb47xx_ap_device *)calloc(1, sizeof(*dev));
    if (!dev)
        return -ENOMEM;

    dev->device.common.tag = HARDWARE_DEVICE_TAG;
    dev->device.common.version = 0;
    dev->device.common.module = (hw_module_t *)module;
    dev->device.common.close = xb47xx_ap_dev_close;
    dev->device.create_audio_policy = create_xb47xx_ap;
    dev->device.destroy_audio_policy = destroy_xb47xx_ap;

    *device = &dev->device.common;

    return 0;
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
            .name           = "Xb47xx audio policy HAL",
            .author         = "The Android Open Source Project",
            .methods        = &xb47xx_ap_module_methods,
        },
    },
};
