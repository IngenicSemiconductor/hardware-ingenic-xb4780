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

#define LOG_TAG "audio_hw_xb47xx_primary"
//#define LOG_NDEBUG 0
//#define DUMP_INPUT_DATA
//#define DUMP_OUTPUT_DATA

static int debug_output_fd;
static int debug_input_fd;
#define DUMP_INPUT_FILE "/data/dump_input.pcm"
#define DUMP_OUTPUT_FILE "/data/dump_output.pcm"

#define FUNC_ENTER() //ALOGD("[FUNC TRACE] %s: %d ----\n", __FUNCTION__, __LINE__)
#define FUNC_LEAVE() //ALOGV("[FUNC TRACE] %s: %d ----\n", __FUNCTION__, __LINE__)

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>

#include <cutils/log.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/soundcard.h>
#include "audio_hw.h"


/*device operation function*/

enum direction {
	STREAM_OUT,
	STREAM_IN,
};

static int set_device_format(int fd , audio_format_t *format)
{
	uint32_t format_dev = -1;
	int ret = 0;

	if (fd <= 0)
		return -ENODEV;
	switch (*format) {
	case AUDIO_FORMAT_PCM_16_BIT:
		format_dev = AFMT_S16_LE;
		break;
	case AUDIO_FORMAT_PCM_8_BIT:
		format_dev = AFMT_U8;
		break;
	default:
		ALOGW("set format is unsupport %x set as defalut %x.\n",*format,AUDIO_FORMAT_PCM_16_BIT);
		*format =  AUDIO_FORMAT_PCM_16_BIT;
		format_dev = AFMT_S16_LE;
		break;
	}

	ret = ioctl(fd,SNDCTL_DSP_SETFMT,&format_dev);
	if (ret)
		ret = -errno;
	return ret;
}

static int set_device_samplerate(int fd,uint32_t* samplerate)
{
	int ret = 0;

	if (fd <= 0)
		return -ENODEV;

	ret = ioctl(fd,SNDCTL_DSP_SPEED,samplerate);
	if (ret)
		ret = -errno;

	return ret;
}


static int set_device_channels(int fd, audio_channel_mask_t *channel_mask, enum direction dir)
{
	uint32_t channel_count = -1;
	int ret = -1;
	if (fd <= 0)
		return -ENODEV;
	channel_count = popcount(*channel_mask);
	if (channel_count > 2 && dir == STREAM_IN) {
		ALOGW("record channel_count %d is not support use mono default",channel_count);
		channel_count = 1;
	}
	ret = ioctl(fd,SNDCTL_DSP_CHANNELS,&channel_count);
	if (ret) {
		ret = -errno;
		return ret;
	}
	if (channel_count != (uint32_t)popcount(*channel_mask)) {
		if (dir == STREAM_OUT)
			*channel_mask = audio_channel_out_mask_from_count(channel_count);
		else if (dir == STREAM_IN)
			*channel_mask = audio_channel_in_mask_from_count(channel_count);
	}

	return ret;
}

static bool isInCall(struct xb47xx_audio_device *ladev)
{
	if (ladev == NULL)
		return false;

	if (ladev->dState.adevMode == AUDIO_MODE_IN_CALL ||
			ladev->dState.adevMode == AUDIO_MODE_IN_COMMUNICATION) {
		return true;
	}

	return false;
}
/* #################################################################### *\
|* struct audio_stream_out
\* #################################################################### */
/***************************\
 * struct audio_stream
\***************************/
/**
 * sampling rate is in Hz - eg. 44100
 */

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
	FUNC_ENTER();

	const struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;
	return out->outState.sState.sampleRate;
}

/**
 * currently unused - use set_parameters with key
 * AUDIO_PARAMETER_STREAM_SAMPLING_RATE
 */
static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
	FUNC_ENTER();

	/* set sample rate */
	int fd = -1;
	int ret = -ENODEV;
	uint32_t tmp_rate = rate;
	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	if (out == NULL) {
		ALOGW("%s(line:%d): set out sample rate error, no device", __func__, __LINE__);
		return ret;
	}

	if (out->outDevPrope->dev_fd != -1) {
		if ((ret = set_device_samplerate(out->outDevPrope->dev_fd , &rate)) < 0) {
			ALOGW("%s(line:%d): set out sample rate error", __func__, __LINE__);
			return ret;
		}
		out->outState.sState.sampleRate = rate;
	} else
		ALOGW("%s(line:%d): set out sample rate error, no device", __func__, __LINE__);

	return ret;
}

/**
 * size of output buffer in bytes - eg. 4800
 */
static size_t out_get_buffer_size(const struct audio_stream *stream)
{
	FUNC_ENTER();

	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	/*buffersize is all the same*/
	return out->outState.sState.bufferSize;
}

/**
 * the channel mask -
 * e.g. AUDIO_CHANNEL_OUT_STEREO or AUDIO_CHANNEL_IN_STEREO
 */
static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
	FUNC_ENTER();

	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	/*channels is all the same*/
	return out->outState.sState.channels;
}

/**
 * audio format - eg. AUDIO_FORMAT_PCM_16_BIT
 */
static audio_format_t out_get_format(const struct audio_stream *stream)
{
	FUNC_ENTER();

	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	return out->outState.sState.format;
}

/**
 * currently unused - use set_parameters with key
 * AUDIO_PARAMETER_STREAM_FORMAT
 */
static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
	FUNC_ENTER();

	/* set format */
	int ret = -ENODEV;
	int tmp_format = -1;
	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;
	int mformat = -1;

	if (out == NULL) {
		ALOGW("%s(line:%d): set out format error no device", __func__, __LINE__);
		return ret;
	}

	if (out->outDevPrope->dev_fd != -1) {
		if ((ret = set_device_format(out->outDevPrope->dev_fd, &format)) < 0) {
			ALOGW("%s(line:%d): set out format error", __func__, __LINE__);
			return ret;
		}
		out->outState.sState.format = format;
	} else
		ALOGW("%s(line:%d): set out format error no device", __func__, __LINE__);


	return ret;
}

/**
 * Put the audio hardware input/output into standby mode.
 * Returns 0 on success and <0 on failure.
 */
static int out_standby(struct audio_stream *stream)
{
	FUNC_ENTER();

	/* set standby */
	int ret = -EIO;
	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;
	int standby = 1;
#if 0
	if (out->outDevPrope->dev_fd != -1 && out->outState.sState.standby == false) {
		ALOGV("%s(line:%d):audio will be standby", __func__, __LINE__);
		if ((ret = ioctl(out->outDevPrope->dev_fd, SNDCTL_EXT_SET_STANDBY, &standby)) < 0)
			ALOGE("%s(line:%d): set out standby error %d", __func__, __LINE__,ret);
		else {
			out->outState.sState.standby = true;
		}
	}
#endif

	return 0;
}

/**
 * Release the audio hardware input/output from standby mode.
 * Returns 0 on success and <0 on failure.
 */
static int out_check_and_release_standby(struct audio_stream *stream)
{
	//FUNC_ENTER();
#if 0
	/* release standby */
	int ret = -EIO;
	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;
	bool needstandby = 0;

	if (out->outDevPrope->dev_fd != -1) {
		if (out->outState.sState.standby == true) {
			if ((ret = ioctl(out->outDevPrope->dev_fd, SNDCTL_EXT_SET_STANDBY, &needstandby)) < 0)
				ALOGE("%s(line:%d): release out standby error", __func__, __LINE__);
			else
				out->outState.sState.standby = needstandby;
		}
	}
#endif
	return 0;
}

/**
 * dump the state of the audio input/output device
 */
static int out_dump(const struct audio_stream *stream, int fd)
{
	FUNC_ENTER();

	//struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	return 0;
}

static audio_devices_t out_get_device(const struct audio_stream *stream)
{
	FUNC_ENTER();

	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	/*device is all the same*/
	return out->outState.sState.devices;
}

static audio_devices_t  old_devices;

static int out_set_device(struct audio_stream *stream, audio_devices_t device)
{
	FUNC_ENTER();

	/* set route */
	int fd = -1;
	int ret = -ENODEV;
	uint32_t kdevice = -1;
	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;
	struct xb47xx_audio_device *ladev = NULL;
	int speaker_en = 0;
	enum snd_device_t device_mode;

	if (out == NULL)
		return ret;
	ladev = out->ladev;
	if (ladev == NULL)
		ALOGW("ladev is null maybe some bug");

	if (device & AUDIO_DEVICE_OUT_SPEAKER)
		speaker_en = 1;

	/* turn the device to kdevice,which the driver can be recognised */
	if (device & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
		kdevice = AUDIO_DEVICE_OUT_AUX_DIGITAL;
	} else if (device & AUDIO_DEVICE_OUT_ALL_SCO) {
		kdevice = AUDIO_DEVICE_OUT_ALL_SCO;
	} else if (device & AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET) {
		kdevice = AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET;
	} else if (device & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) {
		kdevice = AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET;
	} else if (device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
		kdevice = AUDIO_DEVICE_OUT_WIRED_HEADSET;
		if (speaker_en)
			kdevice |= AUDIO_DEVICE_OUT_SPEAKER;
	} else if (device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
		kdevice = AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
		if (speaker_en)
			kdevice |= AUDIO_DEVICE_OUT_SPEAKER;
	} else if (device & AUDIO_DEVICE_OUT_SPEAKER) {
		kdevice = AUDIO_DEVICE_OUT_SPEAKER;
	} else if (device & AUDIO_DEVICE_OUT_EARPIECE) {
		kdevice = AUDIO_DEVICE_OUT_EARPIECE;
	} else if (device & AUDIO_DEVICE_OUT_DEFAULT) {
		kdevice = AUDIO_DEVICE_OUT_DEFAULT;
	} else {
		kdevice = -1;
		ALOGE("%s(line:%d): unknown device!", __func__, __LINE__);
		return -EINVAL;
	}

	switch (kdevice) {
		case AUDIO_DEVICE_OUT_AUX_DIGITAL:
			ALOGV("AUDIO_DEVICE_OUT_AUX_DIGITAL");
			device_mode = SND_DEVICE_HDMI;
			break;
		case AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_SPEAKER:
		case AUDIO_DEVICE_OUT_WIRED_HEADPHONE|AUDIO_DEVICE_OUT_SPEAKER:
			ALOGV("AUDIO_DEVICE_OUT_SPEAKER_AND_HEADSET");
			if (isInCall(ladev))
				device_mode = SND_DEVICE_HEADSET_DOWNLINK;
			else
				device_mode = SND_DEVICE_HEADSET_AND_SPEAKER;
			break;

		case AUDIO_DEVICE_OUT_WIRED_HEADSET:
			ALOGV("AUDIO_DEVICE_OUT_WIRED_HEADSET");
			if (isInCall(ladev))
				device_mode = SND_DEVICE_HEADSET_DOWNLINK;
			else
				device_mode = SND_DEVICE_HEADSET;
			break;

		case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
			ALOGV("AUDIO_DEVICE_OUT_WIRED_HEADPHONE");
			if (isInCall(ladev))
				device_mode = SND_DEVICE_HEADPHONE_DOWNLINK;
			else
				device_mode = SND_DEVICE_HEADPHONE;
			break;
		case AUDIO_DEVICE_OUT_EARPIECE:
			ALOGV("AUDIO_DEVICE_OUT_EARPIECE");
			if (isInCall(ladev)) {
				device_mode = SND_DEVICE_HANDSET_DOWNLINK;
				break;
			}
		case AUDIO_DEVICE_OUT_SPEAKER:
			ALOGV("AUDIO_DEVICE_OUT_SPEAKER");
		default :
		case AUDIO_DEVICE_OUT_DEFAULT:
			ALOGV("AUDIO_DEVICE_OUT_DEFAULT");
			if (isInCall(ladev))
				device_mode = SND_DEVICE_SPEAKER_DOWNLINK;
			else
				device_mode = SND_DEVICE_SPEAKER;
			break;
	}
	if (out->outDevPrope->dev_fd != -1) {
		if ((ret = ioctl(out->outDevPrope->dev_fd, SNDCTL_EXT_SET_DEVICE, &device_mode)) < 0) {
			ALOGE("%s(line:%d): set out device error", __func__, __LINE__);
			ret = -errno;
		}
		else {
			out->outState.sState.devices = kdevice;
			ALOGV("%s(line:%d): set out device %x success", __func__, __LINE__,out->outState.sState.devices);
		}
	}

	return ret;
}

/**
 * set/get audio stream parameters. The function accepts a list of
 * parameter key value pairs in the form: key1=value1;key2=value2;...
 *
 * Some keys are reserved for standard parameters (See AudioParameter class)
 *
 * If the implementation does not accept a parameter change while
 * the output is active but the parameter is acceptable otherwise, it must
 * return -ENOSYS.
 *
 * The audio flinger will put the stream in standby and then change the
 * parameter value.
 */
static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
	FUNC_ENTER();

	char *kvpairs_t;
	char *pair;
	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	ALOGD("%s(line:%d): kvpairs = [%s]", __func__, __LINE__, kvpairs);

	kvpairs_t = strdup(kvpairs);

	pair = strtok(kvpairs_t, ";");
	while (pair != NULL) {
		if (strlen(pair) != 0) {
			char *key;
			char *value;
			size_t eqIdx;

			/* get key and value */
			eqIdx = strcspn(pair, "=");

			key = pair;
			if (eqIdx == strlen(pair)) {
				value = NULL;
			} else {
				value = (pair + eqIdx + 1);
			}
			*(key + eqIdx) = '\0';

			ALOGD("%s(line:%d): key = [%s], value = [%s]", __func__, __LINE__, key, value);

			/* process */
			if (!strcmp(key, AUDIO_PARAMETER_STREAM_ROUTING)) {
				// do routing
				audio_devices_t device = -1;

				if (value != NULL)
					device = atoi(value);

				ALOGD("%s(line:%d): device = %d", __func__, __LINE__, device);

				if ((out_set_device(stream, device)) < 0) {
					ALOGE("%s(line:%d): do routing error!\n", __func__, __LINE__);
					return -EIO;
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_FORMAT)) {
				// set format
				int format = -1;

				if (value != NULL)
					format = atoi(value);

				ALOGD("%s(line:%d): format = %d", __func__, __LINE__, format);

				if ((out_set_format(stream, format)) < 0) {
					ALOGE("%s(line:%d): set format error!\n", __func__, __LINE__);
					return -EIO;
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_CHANNELS)) {
				// set channels
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_FRAME_COUNT)) {
				// set frame_count
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_SAMPLING_RATE)) {
				// set samplerate
				int samplerate = 0;

				if (value != NULL)
					samplerate = atoi(value);

				ALOGD("%s(line:%d): samplerate = %d", __func__, __LINE__, samplerate);

				if ((out_set_sample_rate(stream, samplerate)) < 0) {
					ALOGE("%s(line:%d): set samplerate error!\n", __func__, __LINE__);
					return -EIO;
				}
			} else {
				ALOGE("%s(line:%d): unknow key", __func__, __LINE__);
			}
		} else {
			ALOGE("%s(line:%d): empty key value pair", __func__, __LINE__);
		}
		pair = strtok(NULL, ";");
	}

	free(kvpairs_t);
	kvpairs_t == NULL;

	return 0;
}

/**
 * Returns a pointer to a heap allocated string. The caller is responsible
 * for freeing the memory for it.
 */
static char *out_get_parameters(const struct audio_stream *stream, const char *keys)
{
	FUNC_ENTER();

	char *keys_t;
	char *key;
	char ret[100] = "\0";
	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	ALOGD("%s(line:%d): keys = [%s]", __func__, __LINE__, keys);

	keys_t = strdup(keys);

	key = strtok(keys_t, ";");
	while (key != NULL) {
		if (strlen(key) != 0) {
			ALOGD("%s(line:%d): key = [%s]", __func__, __LINE__, key);
			strcat(ret, key);
			strcat(ret, "=");

			/* process */
			if (!strcmp(key, AUDIO_PARAMETER_STREAM_ROUTING)) {
				if (popcount(out->outState.sState.devices) > 0) {
					char valume[10] = "\0";
					sprintf(valume, "%d", out->outState.sState.devices);
					strcat(ret, valume);
					strcat(ret, ";");
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_FORMAT)) {
				if (out->outState.sState.format > 0) {
					char valume[10] = "\0";
					sprintf(valume, "%d", out->outState.sState.format);
					strcat(ret, valume);
					strcat(ret, ";");
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_CHANNELS)) {
				if (out->outState.sState.channels > 0) {
					char valume[10] = "\0";
					sprintf(valume, "%d", out->outState.sState.channels);
					strcat(ret, valume);
					strcat(ret, ";");
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_FRAME_COUNT)) {
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_SAMPLING_RATE)) {
				if (out->outState.sState.sampleRate > 0) {
					char valume[10] = "\0";
					sprintf(valume, "%d", out->outState.sState.sampleRate);
					strcat(ret, valume);
					strcat(ret, ";");
				}
			} else {
				ALOGE("%s(line:%d): unknow key", __func__, __LINE__);
			}
		} else {
			ALOGE("%s(line:%d): empty key", __func__, __LINE__);
		}
		key = strtok(NULL, ";");
	}

	free(keys_t);
	keys_t = NULL;

	if (ret[strlen(ret) - 1] == ';')
		ret[strlen(ret) - 1] = '\0';

	ALOGD("%s(line:%d): ret = [%s]", __func__, __LINE__, ret);

	return strdup(ret);
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	FUNC_ENTER();

	//struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	FUNC_ENTER();

	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	return 0;
}

/***************************\
 * struct audio_stream_out
 \***************************/
/**
 * return the audio hardware driver latency in milli seconds.
 */
static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
	FUNC_ENTER();

	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	return 0;
}

/**
 * Use this method in situations where audio mixing is done in the
 * hardware. This method serves as a direct interface with hardware,
 * allowing you to directly set the volume as apposed to via the framework.
 * This method might produce multiple PCM outputs or hardware accelerated
 * codecs, such as MP3 or AAC.
 */
static int out_set_volume(struct audio_stream_out *stream, float left,
		float right)
{
	FUNC_ENTER();

	/* set playback volume */
	struct audio_volume {
		int vol_l;
		int vol_r;
	} vol;
	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	if (left < 0.0) {
		ALOGW("%s(line:%d): set out left volume (%f) under 0.0, assuming 0.0\n",
				__func__, __LINE__, left);
		left = 0.0;
	} else if (left > 1.0) {
		ALOGW("%s(line:%d): set out left volume (%f) over 1.0, assuming 1.0\n",
				__func__, __LINE__, left);
		left = 1.0;
	}

	if (right < 0.0) {
		ALOGW("%s(line:%d): set out right volume (%f) under 0.0, assuming 0.0\n",
				__func__, __LINE__, right);
		right = 0.0;
	} else if (right > 1.0) {
		ALOGW("%s(line:%d): set out right volume (%f) over 1.0, assuming 1.0\n",
				__func__, __LINE__, right);
		right = 1.0;
	}

	vol.vol_l = ceil(left * 100.0);
	vol.vol_r = ceil(right * 100.0);

	/* NOTE: here we do not support set volume by hardware, so here we do nothing */
	//if (ioctl(fd, SOUND_MIXER_WRITE_VOLUME, &vol) < 0)
	//ALOGW("%s(line:%d): set out left volume error", __func__, __LINE__);

	out->outState.lVolume = left;
	out->outState.rVolume = right;

	return 0;
}

/**
 * write audio buffer to driver. Returns number of bytes written
 */
static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
		size_t bytes)
{
	/* write data to device */
	int written = 0;
	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	out_check_and_release_standby(&stream->common);

	if (!out->outDevPrope->dev_mapdata_start) {
		written = write(out->outDevPrope->dev_fd, buffer , bytes);
	} else {
		ALOGW("%s(line:%d): has been mapped, can not write here!\n",
				__func__, __LINE__);
		return -EIO;
	}
#ifdef DUMP_OUTPUT_DATA
	{
		if (debug_output_fd > 0) {
			ALOGD("[dump] written = %d, debug_output_fd = %d", written, debug_output_fd);
			if (write(debug_output_fd, buffer, written) != written) {
				ALOGE("[dump] write error !");
			}
		}
	}
#endif
	return written;
}

/**
 * return the number of audio frames written by the audio dsp to DAC since
 * the output has exited standby
 */
static int out_get_render_position(const struct audio_stream_out *stream,
		uint32_t *dsp_frames)
{
	FUNC_ENTER();

	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	return out->outState.renderPosition;
}

/* #################################################################### *\
   |* struct audio_stream_in
   \* #################################################################### */

/***************************\
 * struct audio_stream
 \***************************/
/**
 * sampling rate is in Hz - eg. 44100
 */
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
	FUNC_ENTER();

	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;
	if (in == NULL)
		return -ENODEV;
    return in->inState.sState.sampleRate;
}

/**
 * currently unused - use set_parameters with key
 * AUDIO_PARAMETER_STREAM_SAMPLING_RATE
 */
static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
	FUNC_ENTER();

	/* set sample rate */
	int ret = -EIO;
	uint32_t tmp_rate = rate;

	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;
	if (in == NULL) {
		ALOGW("%s(line:%d): set in sample rate error, no device", __func__, __LINE__);
		return ret;
	}
	if (in->inDevPrope->dev_fd != -1) {
		if ((ret = set_device_samplerate(in->inDevPrope->dev_fd, &rate)) < 0) {
			ALOGW("%s(line:%d): set in sample rate error", __func__, __LINE__);
			return ret;
		}
		in->inState.sState.sampleRate = rate;
	} else
		ALOGW("%s(line:%d): set in sample rate error, no device", __func__, __LINE__);


	return ret;
}

/**
 * size of output buffer in bytes - eg. 4800
 */
static size_t in_get_buffer_size(const struct audio_stream *stream)
{
	FUNC_ENTER();

	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	if (in == NULL)
		return -ENODEV;
	return in->inState.sState.bufferSize;
}

/**
 * the channel mask -
 * e.g. AUDIO_CHANNEL_OUT_STEREO or AUDIO_CHANNEL_IN_STEREO
 */
static uint32_t in_get_channels(const struct audio_stream *stream)
{
	FUNC_ENTER();

	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	if (in == NULL)
		return -ENODEV;
	return in->inState.sState.channels;
}

/**
 * audio format - eg. AUDIO_FORMAT_PCM_16_BIT
 */
static audio_format_t in_get_format(const struct audio_stream *stream)
{
	FUNC_ENTER();

	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	if (in == NULL)
		return -ENODEV;
	return in->inState.sState.format;
}

/**
 * currently unused - use set_parameters with key
 * AUDIO_PARAMETER_STREAM_FORMAT
 */
static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
	FUNC_ENTER();

	/* set format */
	int ret = -EIO;
	int tmp_format =-1;
	int mformat = -1;
	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	if (in == NULL) {
		ALOGW("%s(line:%d): set in format error no device", __func__, __LINE__);
		return ret;
	}

	if (in->inDevPrope->dev_fd != -1) {
		if ((ret = set_device_format(in->inDevPrope->dev_fd,&format)) < 0) {
			ALOGW("%s(line:%d): set in format error", __func__, __LINE__);
			return ret;
		}
		in->inState.sState.format = format;
	} else
		ALOGW("%s(line:%d): set in format error no device", __func__, __LINE__);

	return ret;
}

/**
 * Put the audio hardware input/output into standby mode.
 * Returns 0 on success and <0 on failure.
 */
static int in_standby(struct audio_stream *stream)
{
	FUNC_ENTER();

	/* set standby */
	int ret = -EIO;
	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;
	int standby = 1;
#if 0
	if (in == NULL)
		return -ENODEV;
	if (in->inDevPrope->dev_fd != -1) {
		if ((ret = ioctl(in->inDevPrope->dev_fd, SNDCTL_EXT_SET_STANDBY, &standby)) < 0)
			ALOGW("%s(line:%d): set in standby error", __func__, __LINE__);
		else
			in->inState.sState.standby = true;
	}
#endif
	return 0;
}

/**
 * Release the audio hardware input/output from standby mode.
 * Returns 0 on success and <0 on failure.
 */
static int in_check_and_release_standby(struct audio_stream *stream)
{
	//FUNC_ENTER();

	/* set standby */
#if 0
	int ret = -EIO;
	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;
	int standby = 0;
	if (in == NULL)
		return -ENODEV;
	if (in->inState.sState.standby) {
		if (in->inDevPrope->dev_fd != -1) {
			if ((ret = ioctl(in->inDevPrope->dev_fd, SNDCTL_EXT_SET_STANDBY, &standby)) < 0)
				ALOGW("%s(line:%d): release in standby error", __func__, __LINE__);
			else
				in->inState.sState.standby = false;
		}
	}
#endif

	return 0;
}

/**
 * dump the state of the audio input/output device
 */
static int in_dump(const struct audio_stream *stream, int fd)
{
	FUNC_ENTER();

	//struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	return 0;
}

static audio_devices_t in_get_device(const struct audio_stream *stream)
{
	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	if (in == NULL)
		return -ENODEV;
	return in->inState.sState.devices;
}

static int in_set_device(struct audio_stream *stream, audio_devices_t device)
{
	FUNC_ENTER();
	/* set route */
	int ret = -ENODEV;
	int kdevice = -1;
	enum snd_device_t device_mode;
	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;
	audio_source_t inputSource = 0;
	struct xb47xx_audio_device *ladev = NULL;
	if (in == NULL)
		return ret;
	in->inState.sState.inputSource;
	ladev = in->ladev;

	/* turn the device to kdevice, which the driver can be recognised */
	if (device & AUDIO_DEVICE_IN_VOICE_CALL) {
		kdevice = AUDIO_DEVICE_IN_VOICE_CALL;
		if (device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET)
			device |= AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
		else if (device | AUDIO_DEVICE_IN_WIRED_HEADSET)
			kdevice |= AUDIO_DEVICE_IN_WIRED_HEADSET;
		else if (device & AUDIO_DEVICE_IN_BUILTIN_MIC)
			kdevice |= AUDIO_DEVICE_IN_BUILTIN_MIC;
	} else if (device & AUDIO_DEVICE_IN_COMMUNICATION) {
		kdevice = AUDIO_DEVICE_IN_COMMUNICATION;
		if (device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET)
			device |= AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
		else if (device | AUDIO_DEVICE_IN_WIRED_HEADSET)
			kdevice |= AUDIO_DEVICE_IN_WIRED_HEADSET;
		else if (device & AUDIO_DEVICE_IN_BUILTIN_MIC)
			kdevice |= AUDIO_DEVICE_IN_BUILTIN_MIC;
	} else if (device & AUDIO_DEVICE_IN_AMBIENT) {
		kdevice = AUDIO_DEVICE_IN_AMBIENT;
	} else if (device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
		kdevice = AUDIO_DEVICE_IN_BUILTIN_MIC;
	} else if (device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
		kdevice = AUDIO_DEVICE_IN_WIRED_HEADSET;
	} else if (device & AUDIO_DEVICE_IN_BACK_MIC) {
		kdevice = AUDIO_DEVICE_IN_BACK_MIC;
	} else if (device & AUDIO_DEVICE_IN_ALL_SCO) {
		kdevice = AUDIO_DEVICE_IN_ALL_SCO;
	} else if (device & AUDIO_DEVICE_IN_DEFAULT) {
		kdevice = AUDIO_DEVICE_IN_DEFAULT;
	} else {
		kdevice = -1;
		ALOGE("%s(line:%d): unknown device!", __func__, __LINE__);
		return -EINVAL;
	}

	switch (kdevice) {
		case AUDIO_DEVICE_IN_WIRED_HEADSET:
			if (isInCall(ladev)) {
				device_mode = SND_DEVICE_HEADSET_MIC_UPLINK;
				//device_mode = SND_DEVICE_RECORD_INCALL;
			} else {
				device_mode = SND_DEVICE_HEADSET_MIC;
			}
			break;
		case AUDIO_DEVICE_IN_VOICE_CALL|AUDIO_DEVICE_IN_WIRED_HEADSET:
			if (isInCall(ladev))
				device_mode = SND_DEVICE_HEADSET_MIC_UPLINK;
		case AUDIO_DEVICE_IN_VOICE_CALL|AUDIO_DEVICE_IN_BUILTIN_MIC:
			if (isInCall(ladev))
				device_mode = SND_DEVICE_BUILDIN_MIC_UPLINK;
		case AUDIO_DEVICE_IN_VOICE_CALL|AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
		case AUDIO_DEVICE_IN_COMMUNICATION|AUDIO_DEVICE_IN_WIRED_HEADSET:
		case AUDIO_DEVICE_IN_COMMUNICATION|AUDIO_DEVICE_IN_BUILTIN_MIC:
		case AUDIO_DEVICE_IN_COMMUNICATION|AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
			if (isInCall(ladev))
				device_mode = SND_DEVICE_RECORD_INCALL;
		case AUDIO_DEVICE_IN_VOICE_CALL:
		case AUDIO_DEVICE_IN_COMMUNICATION:
		case AUDIO_DEVICE_IN_AMBIENT:
		case AUDIO_DEVICE_IN_BACK_MIC:
		case AUDIO_DEVICE_IN_ALL_SCO:
		default:
		case AUDIO_DEVICE_IN_DEFAULT:
		case AUDIO_DEVICE_IN_BUILTIN_MIC:
			if (isInCall(ladev)) {
				device_mode = SND_DEVICE_BUILDIN_MIC_UPLINK;
				//device_mode = SND_DEVICE_RECORD_INCALL;
			} else {
				device_mode = SND_DEVICE_BUILDIN_MIC;
			}
			break;
	}

	if (in->inDevPrope->dev_fd != -1) {
		if ((ret = ioctl(in->inDevPrope->dev_fd, SNDCTL_EXT_SET_DEVICE, &device_mode)) < 0) {
			ALOGE("%s(line:%d): set in device error", __func__, __LINE__);
			ret = -errno;
		}
		else
			in->inState.sState.devices = kdevice;
	}

	return ret;
}

/**
 * set/get audio stream parameters. The function accepts a list of
 * parameter key value pairs in the form: key1=value1;key2=value2;...
 *
 * Some keys are reserved for standard parameters (See AudioParameter class)
 *
 * If the implementation does not accept a parameter change while
 * the output is active but the parameter is acceptable otherwise, it must
 * return -ENOSYS.
 *
 * The audio flinger will put the stream in standby and then change the
 * parameter value.
 */
static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
	FUNC_ENTER();

	char *kvpairs_t;
	char *pair;
	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	ALOGD("%s(line:%d): kvpairs = [%s]", __func__, __LINE__, kvpairs);

	if (in == NULL)
		return -ENODEV;
	kvpairs_t = strdup(kvpairs);

	pair = strtok(kvpairs_t, ";");
	while (pair != NULL) {
		if (strlen(pair) != 0) {
			char *key;
			char *value;
			size_t eqIdx;

			/* get key and value */
			eqIdx = strcspn(pair, "=");

			key = pair;
			if (eqIdx == strlen(pair)) {
				value = NULL;
			} else {
				value = (pair + eqIdx + 1);
			}
			*(key + eqIdx) = '\0';

			ALOGV("%s(line:%d): key = [%s], value = [%s]", __func__, __LINE__, key, value);

			/* process */
			if (!strcmp(key, AUDIO_PARAMETER_STREAM_ROUTING)) {
				// do routing
				audio_devices_t device = -1;

				if (value != NULL)
					device = atoi(value);

				ALOGV("in_set_parameters route %x",device);
				if ((in_set_device(stream, device)) < 0) {
					ALOGE("%s(line:%d): do routing error!\n", __func__, __LINE__);
					return -EIO;
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_FORMAT)) {
				// set format
				int format = -1;

				if (value != NULL)
					format = atoi(value);
				ALOGV("in_set_parameters format %x",format);
				if ((in_set_format(stream, format)) < 0) {
					ALOGE("%s(line:%d): set format error!", __func__, __LINE__);
					return -EIO;
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_CHANNELS)) {
				// set channels
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_FRAME_COUNT)) {
				// set frame_count
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_INPUT_SOURCE)) {
				audio_source_t inputSource = -1;
				if (value != NULL)
					inputSource = atoi(value);
				ALOGV("in_set_parameters input source %x",inputSource);
				/*if (in_set_inputsource(stream,inputSource) < 0) {
					ALOGE("%s(line %d): set inputSource error",__func__,__LINE__);
					return -EIO;
				}*/

			} else {
				ALOGE("%s(line:%d): unknow key", __func__, __LINE__);
			}
		} else {
			ALOGE("%s(line:%d): empty key value pair", __func__, __LINE__);
		}
		pair = strtok(NULL, ";");
	}

	free(kvpairs_t);
	kvpairs_t = NULL;

    return 0;
}

/**
 * Returns a pointer to a heap allocated string. The caller is responsible
 * for freeing the memory for it.
 */
static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
	FUNC_ENTER();

	char *keys_t;
	char *key;
	char ret[100] = "\0";
	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	ALOGD("%s(line:%d): keys = [%s]", __func__, __LINE__, keys);

	if (in == NULL)
		return NULL;
	keys_t = strdup(keys);

    key = strtok(keys_t, ";");
    while (key != NULL) {
        if (strlen(key) != 0) {
			ALOGD("%s(line:%d): key = [%s]", __func__, __LINE__, key);
			strcat(ret, key);
			strcat(ret, "=");

			/* process */
			if (!strcmp(key, AUDIO_PARAMETER_STREAM_ROUTING)) {
				if (popcount(in->inState.sState.devices) > 0) {
					char valume[10] = "\0";
					sprintf(valume, "%d", in->inState.sState.devices);
					strcat(ret, valume);
					strcat(ret, ";");
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_FORMAT)) {
				if (in->inState.sState.format > 0) {
					char valume[10] = "\0";
					sprintf(valume, "%d", in->inState.sState.format);
					strcat(ret, valume);
					strcat(ret, ";");
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_CHANNELS)) {
				if (in->inState.sState.channels > 0) {
					char valume[10] = "\0";
					sprintf(valume, "%d", in->inState.sState.channels);
					strcat(ret, valume);
					strcat(ret, ";");
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_FRAME_COUNT)) {
			} else if (!strcmp(key, AUDIO_PARAMETER_STREAM_INPUT_SOURCE)) {
			} else {
				ALOGE("%s(line:%d): unknow key", __func__, __LINE__);
			}
        } else {
            ALOGE("%s(line:%d): empty key", __func__, __LINE__);
        }
        key = strtok(NULL, ";");
    }

	free(keys_t);

	if (ret[strlen(ret) - 1] == ';')
		ret[strlen(ret) - 1] = '\0';

	ALOGD("%s(line:%d): ret = [%s]", __func__, __LINE__, ret);

    return strdup(ret);
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	FUNC_ENTER();

	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	if (in == NULL)
		return -ENODEV;
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	FUNC_ENTER();

	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	if (in == NULL)
		return -ENODEV;
    return 0;
}

/***************************\
 * struct audio_stream_in
\***************************/
/**
 * set the input gain for the audio driver. This method is
 * for future use
 */
static int in_set_gain(struct audio_stream_in *stream, float gain)
{
	FUNC_ENTER();

	/* set sample rate */
	int ret = -EIO;
	int mic_gain;
	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	if (in == NULL)
		return -ENODEV;
	if (gain < 0.0) {
		ALOGW("%s(line:%d): set in gain (%f) under 0.0, assuming 0.0",
			 __func__, __LINE__, gain);
		gain = 0.0;
	} else if (gain > 1.0) {
		ALOGW("%s(line:%d): set in gain (%f) over 1.0, assuming 1.0",
			 __func__, __LINE__, gain);
		gain = 1.0;
	}

	mic_gain = ceil(gain * 100.0);

	/* NOTE: here we do not set the hardware gain */
	//if ((ret = ioctl(fd, SOUND_MIXER_WRITE_MIC, &mic_gain)) < 0)
	//ALOGW("%s(line:%d): set in gain error", __func__, __LINE__);

	in->inState.gain = gain;

	return ret;
}

/**
 * read audio buffer in from audio driver
 */
static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
		size_t bytes)
{
	FUNC_ENTER();
	/* read data form device */
	size_t count;
	int bytesRead = 0;
	uint8_t* p = (uint8_t*)buffer;
	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	if (in == NULL)
		return -ENODEV;
	in_check_and_release_standby(&stream->common);

	count = bytes;
	while (count) {
		bytesRead = read(in->inDevPrope->dev_fd, p, count);
		if (bytesRead < 0) {
			ALOGE("%s(line:%d): read error while recording, return -EIO!", __func__, __LINE__);
			close(in->inDevPrope->dev_fd);
			return -EIO;
		}

		count -= bytesRead;
		p += bytesRead;
	}

	return bytes;
}

/**
 * Return the amount of input frames lost in the audio driver since the
 * last call of this function.
 * Audio driver is expected to reset the value to 0 and restart counting
 * upon returning the current value by this function call.
 * Such loss typically occurs when the user space process is blocked
 * longer than the capacity of audio driver buffers.
 *
 * Unit: the number of input audio frames
 */
static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
	FUNC_ENTER();

	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;

	if (in == NULL)
		return -ENODEV;
    return in->inState.framesLost;
}

/* #################################################################### *\
|* struct audio_hw_device
\* #################################################################### */

/**
 * used by audio flinger to enumerate what devices are supported by
 * each audio_hw_device implementation.
 *
 * Return value is a bitmask of 1 or more values of audio_devices_t
 */
static uint32_t adev_get_supported_devices(const struct audio_hw_device *dev)
{
	FUNC_ENTER();

	struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	return (/* OUT */
			AUDIO_DEVICE_OUT_EARPIECE |
			AUDIO_DEVICE_OUT_SPEAKER |
			AUDIO_DEVICE_OUT_WIRED_HEADSET |
			AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
			AUDIO_DEVICE_OUT_AUX_DIGITAL |
			AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET |
			AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET |
			AUDIO_DEVICE_OUT_ALL_SCO |
			AUDIO_DEVICE_OUT_DEFAULT |
			/* IN */
			AUDIO_DEVICE_IN_COMMUNICATION |
			AUDIO_DEVICE_IN_BUILTIN_MIC |
			AUDIO_DEVICE_IN_VOICE_CALL |
			AUDIO_DEVICE_IN_WIRED_HEADSET |
			AUDIO_DEVICE_IN_ALL_SCO |
			AUDIO_DEVICE_IN_DEFAULT);
}

/**
 * check to see if the audio hardware interface has been initialized.
 * returns 0 on success, -ENODEV on failure.
 */
static int adev_init_check(const struct audio_hw_device *dev)
{
	FUNC_ENTER();

	//struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	FUNC_LEAVE();
	return 0;
}

/**
 * get volume methord of device, return the methord for ioctl
 */
static int adev_get_device_volume_methord(struct audio_hw_device *dev,
										  audio_devices_t device)
{
	FUNC_ENTER();

	int volumeMethord = -1;
	struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	switch (device) {
	case AUDIO_DEVICE_OUT_EARPIECE:
	case AUDIO_DEVICE_OUT_SPEAKER:
	case AUDIO_DEVICE_OUT_WIRED_HEADSET:
	case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
	case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
	case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
	case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
	case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP:
	case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES:
	case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER:
	case AUDIO_DEVICE_OUT_AUX_DIGITAL:
	case AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET:
	case AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET:
	case AUDIO_DEVICE_OUT_DEFAULT:
		volumeMethord = SOUND_MIXER_WRITE_VOLUME;
		break;
	case AUDIO_DEVICE_IN_COMMUNICATION:
	case AUDIO_DEVICE_IN_AMBIENT:
	case AUDIO_DEVICE_IN_BUILTIN_MIC:
    case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_IN_WIRED_HEADSET:
    case AUDIO_DEVICE_IN_AUX_DIGITAL:
    case AUDIO_DEVICE_IN_VOICE_CALL:
    case AUDIO_DEVICE_IN_BACK_MIC:
    case AUDIO_DEVICE_IN_DEFAULT:
		volumeMethord = SOUND_MIXER_WRITE_MIC;
		break;
	default:
		ALOGE("%s(line:%d): unknown device!", __func__, __LINE__);
	}

	return volumeMethord;
}

/**
 * set the audio volume of a voice call. Range is between 0.0 and 1.0
 */
static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
	FUNC_ENTER();

	int vol = -1;
	int volumeMethord = -1;
	struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	if (volume < 0.0) {
		ALOGW("%s(line:%d): volume (%f) under 0.0, assuming 0.0\n",
			 __func__, __LINE__, volume);
		volume = 0.0;
	} else if (volume > 1.0) {
		ALOGW("%s(line:%d): volume (%f) over 1.0, assuming 1.0\n",
			 __func__, __LINE__, volume);
		volume = 1.0;
	}

	vol = ceil(volume * 100.0);

	/* AUDIO_DEVICE_IN_COMMUNICATION */
	volumeMethord = adev_get_device_volume_methord(dev, AUDIO_DEVICE_IN_COMMUNICATION);
	if (volumeMethord >= 0) {
		/* NOTE, we do not set hardware vomlue */
		//if (ioctl(fd, volumeMethord, &vol) < 0)
		//ALOGW("%s(line:%d): set volume error", __func__, __LINE__);
	}

	return 0;
}

/**
 * set the audio volume for all audio activities other than voice call.
 * Range between 0.0 and 1.0. If any value other than 0 is returned,
 * the software mixer will emulate this capability.
 */
static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
	FUNC_ENTER();

	int vol = -1;
	int volumeMethord = -1;
	struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	if (volume < 0.0) {
		ALOGW("%s(line:%d): volume (%f) under 0.0, assuming 0.0\n",
			 __func__, __LINE__, volume);
		volume = 0.0;
	} else if (volume > 1.0) {
		ALOGW("%s(line:%d): volume (%f) over 1.0, assuming 1.0\n",
			 __func__, __LINE__, volume);
		volume = 1.0;
	}

	vol = ceil(volume * 100.0);

	/* AUDIO_DEVICE_OUT_EARPIECE */
	volumeMethord = adev_get_device_volume_methord(dev, AUDIO_DEVICE_OUT_EARPIECE);
	if (volumeMethord >= 0) {
		//if (ioctl(fd, volumeMethord, &vol) < 0)
		//ALOGW("%s(line:%d): set volume error", __func__, __LINE__);
	}

	/* AUDIO_DEVICE_OUT_WIRED_HEADSET */
	volumeMethord = adev_get_device_volume_methord(dev, AUDIO_DEVICE_OUT_WIRED_HEADSET);
	if (volumeMethord >= 0) {
		//if (ioctl(fd, volumeMethord, &vol) < 0)
		//ALOGW("%s(line:%d): set volume error", __func__, __LINE__);
	}

	/* AUDIO_DEVICE_OUT_SPEAKER */
	volumeMethord = adev_get_device_volume_methord(dev, AUDIO_DEVICE_OUT_SPEAKER);
	if (volumeMethord >= 0) {
		//if (ioctl(fd, volumeMethord, &vol) < 0)
		//ALOGW("%s(line:%d): set volume error", __func__, __LINE__);
	}

	/* AUDIO_DEVICE_OUT_BLUETOOTH_SCO */
	volumeMethord = adev_get_device_volume_methord(dev, AUDIO_DEVICE_OUT_BLUETOOTH_SCO);
	if (volumeMethord >= 0) {
		//if (ioctl(fd, volumeMethord, &vol) < 0)
		//ALOGW("%s(line:%d): set volume error", __func__, __LINE__);
	}

	/*
	  We return an error code here to let the audioflinger do in-software
	  volume on top of the maximum volume that we set through the SND API.
	  return error - software mixer will handle it
	*/
    return -ENOSYS;
}

/**
 * setMode is called when the audio mode changes. AUDIO_MODE_NORMAL mode
 * is for standard audio playback, AUDIO_MODE_RINGTONE when a ringtone is
 * playing, and AUDIO_MODE_IN_CALL when a call is in progress.
 */
static int adev_set_mode(struct audio_hw_device *dev, int mode)
{
	FUNC_ENTER();

    struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	switch (mode) {
	case AUDIO_MODE_INVALID:
		ladev->dState.adevMode = AUDIO_MODE_INVALID;
		break;
	case AUDIO_MODE_CURRENT:
		ladev->dState.adevMode = AUDIO_MODE_CURRENT;
		break;
	case AUDIO_MODE_NORMAL:
		ladev->dState.adevMode = AUDIO_MODE_NORMAL;
		break;
	case AUDIO_MODE_RINGTONE:
		ladev->dState.adevMode = AUDIO_MODE_RINGTONE;
		break;
	case AUDIO_MODE_IN_CALL:
		ladev->dState.adevMode = AUDIO_MODE_IN_CALL;
		break;
	case AUDIO_MODE_IN_COMMUNICATION:
		ladev->dState.adevMode = AUDIO_MODE_IN_COMMUNICATION;
		break;
	default:
		ALOGE("%s(line:%d): unknown audio mode!", __func__, __LINE__);
	}

    return 0;
}

/**
 * set mic mute
 */
static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
	FUNC_ENTER();

    struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	ladev->dState.micMute = state;

    return 0;
}

/**
 * get mic mute
 */
static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
	FUNC_ENTER();

    struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

    return ladev->dState.micMute;
}

/**
 * set global audio parameters
 */
static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{/*set connect*/
	FUNC_ENTER();

	char *kvpairs_t;
	char *pair;
    struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;
	struct xb47xx_stream_out *out = ladev->ostream;
	struct xb47xx_stream_in *in = ladev->istream;
	int ret = -ENOSYS;

	ALOGD("%s(line:%d): kvpairs = [%s]", __func__, __LINE__, kvpairs);

	kvpairs_t = strdup(kvpairs);

    pair = strtok(kvpairs_t, ";");
    while (pair != NULL) {
        if (strlen(pair) != 0) {
            char *key;
            char *value;
			uint32_t device;
            size_t eqIdx;

			/* get key and value */
			eqIdx = strcspn(pair, "=");

			key = pair;
            if (eqIdx == strlen(pair)) {
                value = NULL;
            } else {
                value = (pair + eqIdx + 1);
            }
			*(key + eqIdx) = '\0';

			ALOGD("%s(line:%d): key = [%s], value = [%s]", __func__, __LINE__, key, value);

			/* process */
			if (!strcmp(key, AUDIO_PARAMETER_KEY_BT_NREC)) {
				if (!strcmp(value, AUDIO_PARAMETER_VALUE_ON)) {
				} else if (!strcmp(value, AUDIO_PARAMETER_VALUE_OFF)) {
				} else {
					ALOGE("%s(line:%d): unknow bt_headset_nrec value", __func__, __LINE__);
				}
			} else if (!strcmp(key, AUDIO_PARAMETER_KEY_TTY_MODE)) {
				if (!strcmp(value, AUDIO_PARAMETER_VALUE_TTY_OFF)) {
				} else if (!strcmp(value, AUDIO_PARAMETER_VALUE_TTY_VCO)) {
				} else if (!strcmp(value, AUDIO_PARAMETER_VALUE_TTY_HCO)) {
				} else if (!strcmp(value, AUDIO_PARAMETER_VALUE_TTY_FULL)) {
				} else {
					ALOGE("%s(line:%d): unknow tty_mode value", __func__, __LINE__);
				}
			} else if (!strcmp(key,AUDIO_PARAMETER_STREAM_ROUTING)) {
				device = atoi(value);
				if (audio_is_output_device(device))
					ret = out_set_device(&out->stream.common,device);
				else if (audio_is_input_device(device))
					ret = in_set_device(&in->stream.common,device);
				else
					ALOGE("%s(line:%d):unknow device",__func__,__LINE__);
			} else if (!strcmp(key,AUDIO_PARAMETER_KEY_SCREEN_STATE)) {

			} else
				ALOGE("%s(line:%d): unknow key", __func__, __LINE__);
        } else {
           ALOGE("%s(line:%d): empty key value pair", __func__, __LINE__);
        }
        pair = strtok(NULL, ";");
    }

	free(kvpairs_t);

	return ret;
}

/**
 * get global audio parameters
 * Returns a pointer to a heap allocated string. The caller is responsible
 * for freeing the memory for it.
 */
static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
	FUNC_ENTER();

	char *keys_t;
	char *key;
	char ret[100] = "\0";
    struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	ALOGD("%s(line:%d): keys = [%s]", __func__, __LINE__, keys);

	keys_t = strdup(keys);

    key = strtok(keys_t, ";");
    while (key != NULL) {
        if (strlen(key) != 0) {
			ALOGD("%s(line:%d): key = [%s]", __func__, __LINE__, key);
			strcat(ret, key);
			strcat(ret, "=");

			/* process */
			if (!strcmp(key, AUDIO_PARAMETER_KEY_BT_NREC)) {
			} else if (!strcmp(key, AUDIO_PARAMETER_KEY_TTY_MODE)) {
			} else {
				ALOGE("%s(line:%d): unknow key", __func__, __LINE__);
			}
        } else {
            ALOGE("%s(line:%d): empty key", __func__, __LINE__);
        }
        key = strtok(NULL, ";");
    }

	free(keys_t);

	if (ret[strlen(ret) - 1] == ';')
		ret[strlen(ret) - 1] = '\0';

	ALOGD("%s(line:%d): ret = [%s]", __func__, __LINE__, ret);

    //return strdup(ret);
    return NULL;
}

/**
 * Returns audio input buffer size according to parameters passed or
 * 0 if one of the parameters is not supported
 */
static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
	FUNC_ENTER();

    struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	/* get record buffer size */
		uint32_t i = 0;
		const uint32_t inputSamplingRates[] = {
			8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
		};

		int channel_count = popcount(config->channel_mask);

		for (i = 0; i < sizeof(inputSamplingRates) / sizeof(uint32_t); i++) {
			if (config->sample_rate == inputSamplingRates[i]) {
				break;
			}
		}

		if ( i >= sizeof(inputSamplingRates) / sizeof(uint32_t)) {
			ALOGW("%s(line:%d): bad sampling rate: %d", __func__, __LINE__, config->sample_rate);
			return 0;
		}
		if (config->format != AUDIO_FORMAT_PCM_16_BIT) {
			ALOGW("%s(line:%d): bad format: %d", __func__, __LINE__, config->format);
			return 0;
		}
		if (channel_count < 1 || channel_count > 2) {
			ALOGW("%s(line:%d): bad channel count: %d", __func__, __LINE__, channel_count);
			return 0;
		}

		return (AUDIO_HW_IN_DEF_BUFFERSIZE / sizeof(short)) * channel_count;

}

/**
 * open output device
 */
static uint32_t adev_open_output_device(struct audio_hw_device *dev) {
		/* open device */
		int map_size = 0;
		int status = -1;
		struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

		FUNC_ENTER();
		ladev->oDevPrope.dev_fd = open(DSP_DEVICE, O_WRONLY);
		if (ladev->oDevPrope.dev_fd <= 0) {
		status = -errno;
		ALOGE("%s(line:%d): open write audio device error %d", __func__, __LINE__,status);
			ladev->oDevPrope.dev_fd = -1;
		return status;
		}
#ifdef MAP_OUTPUT_DEV
		status = ioctl(ladev->oDevPrope.dev_fd, SNDCTL_DSP_GETBLKSIZE, &map_size);
		if (!status) {
			if(map_size) {
				ladev->oDevPrope.dev_map_fd = open(DSP_DEVICE, O_RDWR);
				if (ladev->oDevPrope.dev_map_fd < 0) {
					ALOGE("%s(line:%d): can not mmap!", __func__, __LINE__);
				} else {
					ladev->oDevPrope.dev_mapdata_start =
						(unsigned char *)mmap(NULL,
											  map_size,
											  PROT_WRITE,
											  MAP_SHARED,
											  ladev->oDevPrope.dev_map_fd,
											  0);
					ALOGV("%s(line:%d): mapdata_start = %p size = %d",
						 __func__, __LINE__, ladev->oDevPrope.dev_mapdata_start, map_size);
				}
			}
		} else {
			ALOGE("%s(line:%d): IOCTL SNDCTL_DSP_GETBLKSIZE return error %d\n",
				 __func__, __LINE__, status);
		}
#endif
		return 0;
}

/**
 * close output device
 */
static void adev_close_output_device(struct audio_hw_device *dev)
{
    struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	if (ladev->oDevPrope.dev_map_fd != -1) {
		close(ladev->oDevPrope.dev_map_fd);
		ladev->oDevPrope.dev_mapdata_start = NULL;
	}

	if (ladev->oDevPrope.dev_fd != -1)
		close(ladev->oDevPrope.dev_fd);
}

/**
 * This method creates and opens the audio hardware output stream
 */
static int adev_open_output_stream(struct audio_hw_device *dev,
								   audio_io_handle_t handle,
                                   audio_devices_t devices,
								   audio_output_flags_t flags,
								   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
	FUNC_ENTER();

    struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;
    struct xb47xx_stream_out *out;
    int ret;
	uint32_t mchannels = -1;
	int mformat = -1;
	int need_open_dev;

	if (ladev->ostream != NULL) {
		ALOGW("Xb47xx audio HW HAL output stream is only support open one time");
		return -ENOSYS;
	}

    out = (struct xb47xx_stream_out *)calloc(1, sizeof(struct xb47xx_stream_out));
    if (!out)
        return -ENOMEM;

	ladev->ostream = out;
	if (config->channel_mask == 0)
		out->outState.sState.channels = config->channel_mask = AUDIO_HW_OUT_DEF_CHANNELS;
	else
		out->outState.sState.channels = config->channel_mask;

	if (config->format == 0)
		out->outState.sState.format = config->format = AUDIO_HW_OUT_DEF_FORMAT;
	else
		out->outState.sState.format = config->format;

	if (config->sample_rate == 0)
		out->outState.sState.sampleRate =  config->sample_rate = AUDIO_HW_OUT_DEF_SAMPLERATE;
	else
		out->outState.sState.sampleRate =  config->sample_rate;

	out->outState.sState.bufferSize = AUDIO_HW_OUT_DEF_BUFFERSIZE;
	out->outState.sState.standby = false;
		out->outState.sState.devices = AUDIO_DEVICE_OUT_DEFAULT;
	out->outState.lVolume = 1.0;
	out->outState.rVolume = 1.0;
	out->outState.renderPosition = 0;
	out->ladev = ladev;

	/* playback */
	ret = adev_open_output_device(&ladev->device);
	if (ret < 0)
		goto err_open;

	out->outDevPrope = &ladev->oDevPrope;

	ret = set_device_channels(ladev->oDevPrope.dev_fd,&config->channel_mask,STREAM_OUT);
	if (ret) {
		ALOGV("set_device_channels fail (%s)\n", strerror(ret));
		out->outState.sState.channels = 0;
		goto err_open;
	}
	ret = set_device_samplerate(ladev->oDevPrope.dev_fd,&config->sample_rate);
	if (ret) {
		ALOGV("set_device_samplerate fail (%s)\n",strerror(ret));
		out->outState.sState.sampleRate = 0;
		goto err_open;
	}
	ret = set_device_format(ladev->oDevPrope.dev_fd,&config->format);
	if (ret) {
		ALOGV("set_device_format fail (%s)", strerror(ret));
		out->outState.sState.format = 0;
		goto err_open;
	}

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
	out->stream.common.get_device = out_get_device;
	out->stream.common.set_device = out_set_device;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
	*stream_out = &out->stream;
#ifdef DUMP_OUTPUT_DATA
	if ((debug_output_fd = open(DUMP_OUTPUT_FILE, O_WRONLY | O_CREAT | O_APPEND)) <= 0) {
		ALOGE("[dump] open file error !");
	}
#endif

    return 0;

err_open:
	adev_close_output_device(&ladev->device);
    free(out);
	out = NULL;
    *stream_out = NULL;
	ladev->ostream = NULL;
	return ret;
}

/**
 * close output stream
 */
static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
	FUNC_ENTER();

    struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;
	struct xb47xx_stream_out *out = (struct xb47xx_stream_out *)stream;

	if (ladev) {
	adev_close_output_device(dev);
		if (ladev->ostream != NULL)
			if (ladev->ostream != out) {
				ALOGD("BUG: close_output_stream is error ,one device is used two or more stream");
				free(ladev->ostream);
			}
		ladev->ostream = NULL;
	} else
		ALOGW("There is no device on this input stream");


#ifdef DUMP_OUTPUT_DATA
	if (debug_output_fd > 0) {
		close(debug_output_fd);
	}
#endif

	free(out);
	out = NULL;
}

/**
 * open input device
 */
static uint32_t adev_open_input_device(struct audio_hw_device *dev)
{
	FUNC_ENTER();
	int map_size = 0;
	int status = -1;
	struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	ladev->iDevPrope.dev_fd = open(DSP_DEVICE, O_RDONLY);
	if (ladev->iDevPrope.dev_fd <= 0) {
		status = -errno;
		ALOGE("%s(line:%d): open read audio device error %d", __func__, __LINE__,status);
		ladev->iDevPrope.dev_fd = -1;
		return status;
				}

	return 0;
}

/**
 * close input device
 */
static void adev_close_input_device(struct audio_hw_device *dev)
{
	FUNC_ENTER();

	struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;

	if (ladev->iDevPrope.dev_fd != -1)
		close(ladev->iDevPrope.dev_fd);
}

/**
 * This method creates and opens the audio hardware input stream
 */
static int adev_open_input_stream(struct audio_hw_device *dev,
								  audio_io_handle_t handle,
								  audio_devices_t devices,
								  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
	FUNC_ENTER();

	struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;
	struct xb47xx_stream_in *in;
	int ret;
	uint32_t mchannels = -1;
	int mformat = -1;

	if (ladev->istream != NULL) {
		ALOGW("Xb47xx audio HW HAL input stream is only support open one time");
		return -ENOSYS;
	}

	in = (struct xb47xx_stream_in *)calloc(1, sizeof(struct xb47xx_stream_in));
	if (!in)
		return -ENOMEM;

	ladev->istream = in;
	if (config->format)
		in->inState.sState.format = config->format;
	else
		in->inState.sState.format = config->format = AUDIO_HW_IN_DEF_FORMAT;
	if (config->sample_rate)
		in->inState.sState.sampleRate = config->sample_rate;
	else
		in->inState.sState.sampleRate = config->sample_rate = AUDIO_HW_IN_DEF_SAMPLERATE;
	if (config->channel_mask)
		in->inState.sState.channels = config->channel_mask;
	else
		in->inState.sState.channels = config->channel_mask = AUDIO_HW_IN_DEF_CHANNELS;
	in->inState.sState.bufferSize = AUDIO_HW_IN_DEF_BUFFERSIZE;
	in->inState.sState.standby = false;
		in->inState.sState.devices = AUDIO_DEVICE_IN_BUILTIN_MIC;
	in->inState.gain = 1.0;
	in->inState.framesLost = 0;
	in->ladev = ladev;

	/* record */
	ret = adev_open_input_device(&ladev->device);
	if (ret)
		goto err_open;

	in->inDevPrope = &ladev->iDevPrope;

	ret = set_device_channels(ladev->iDevPrope.dev_fd,&config->channel_mask,STREAM_IN);
	if (ret) {
		ALOGV("set_device_channels fail (%s)\n", strerror(-ret));
		in->inState.sState.channels = 0;
		goto err_open;
	}
	ret = set_device_samplerate(ladev->iDevPrope.dev_fd,&config->sample_rate);
	if (ret) {
		ALOGV("set_device_samplerate fail (%s)\n",strerror(-ret));
		in->inState.sState.sampleRate = 0;
		goto err_open;
	}
	ret = set_device_format(ladev->iDevPrope.dev_fd,&config->format);
	if (ret) {
		ALOGV("set_device_format fail (%s)", strerror(-ret));
		in->inState.sState.format = 0;
		goto err_open;
	}

	in->stream.common.get_sample_rate = in_get_sample_rate;
	in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby =	in_standby;
    in->stream.common.dump = in_dump;
	in->stream.common.get_device = in_get_device;
	in->stream.common.set_device = in_set_device;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    *stream_in = &in->stream;
#ifdef DUMP_INPUT_DATA
	/* open */
	if ((debug_input_fd = open(DUMP_INPUT_FILE, O_WRONLY | O_APPEND)) <= 0) {
		ALOGE("[dump] open file error !");
	}
#endif
    return 0;

err_open:
	adev_close_input_device(&ladev->device);
    free(in);
	in = NULL;
    *stream_in = NULL;
	ladev->istream = NULL;
	return ret;
}

/**
 * close input stream
 */
static void adev_close_input_stream(struct audio_hw_device *dev,
		struct audio_stream_in *stream)
{
	FUNC_ENTER();
	struct xb47xx_stream_in *in = (struct xb47xx_stream_in *)stream;
	struct xb47xx_audio_device *ladev = (struct xb47xx_audio_device *)dev;
	if (ladev) {
	adev_close_input_device(dev);
		if (ladev->istream)
			if (ladev->istream != in) {
				ALOGD("BUG: close_input_stream is error ,one device is used two or more stream");
				free(ladev->istream);
			}
		ladev->istream = NULL;
	} else
		ALOGW("There is no device on this input stream");

#ifdef DUMP_INPUT_DATA
	/* close */
	if (debug_input_fd) {
		close(debug_input_fd);
	}
#endif

	free(in);
	in = NULL;
	return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
	FUNC_ENTER();

    return 0;
}

/* #################################################################### *\
|* module open & close
\* #################################################################### */

static int adev_close(hw_device_t *device)
{
	FUNC_ENTER();

	struct audio_hw_device *dev = (struct audio_hw_device *)device;
    struct xb47xx_audio_device *adev = (struct xb47xx_audio_device *)dev;
	/* close hw device for playback and record */
	if (adev == NULL) {
		ALOGW("adev_close : device is already closed before");
		return 0;
	}

	adev_close_output_device(dev);
	adev_close_input_device(dev);

	if (adev->ostream)
		free(adev->ostream);
	if (adev->istream)
		free(adev->istream);
    free(adev);
	adev = NULL;

    return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct xb47xx_audio_device *adev;
    int ret;

	FUNC_ENTER();

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct xb47xx_audio_device));
    if (!adev)
        return -ENOMEM;

	adev->dState.adevMode = AUDIO_MODE_NORMAL;
	adev->dState.micMute = false;

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_CURRENT;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.get_supported_devices = adev_get_supported_devices;
    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

	adev->oDevPrope.dev_fd = -1;
	adev->oDevPrope.dev_map_fd = -1;
	adev->oDevPrope.dev_mapdata_start = NULL;

	adev->iDevPrope.dev_fd = -1;


	//adev_open_input_device(&adev->device);
	//adev_open_output_device(&adev->device);
	/* open hw device for playback and record */
    *device = &adev->device.common;

	FUNC_LEAVE();
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Xb47xx audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
