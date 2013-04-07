/*
** Copyright 2008, Google Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <math.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <fcntl.h>

// hardware specific functions
#include "AudioHardware.h"

//#define DUMP_INPUT_DATA
//#define DUMP_OUTPUT_DATA
//#define LOG_NDEBUG		0

#ifdef DUMP_INPUT_DATA
	/* dump debug */
static int debug_input_fd;
#endif

#undef LOG_TAG
#define LOG_TAG			"AudioHardwareJz47xx"

//#define LOGCAT_2_STDOUT	1
#ifdef LOGCAT_2_STDOUT
  #undef ALOGE
  #define ALOGE(x,y...)		printf(x,##y)
  #undef ALOGV
  #define ALOGV(x,y...)		printf(x,##y)
  #undef ALOGI
  #define ALOGI(x,y...)		printf(x,##y)
  #undef LOGW
  #define LOGW(x,y...)		printf(x,##y)
#endif

// Set to 1 to log sound RPC's
#define LOG_SND_RPC		1

//#define SHOW_FUNC_TRACE	1
#ifdef SHOW_FUNC_TRACE
  #define FUNC_TRACE()		printf("[FUNC TRACE] %s: %d ----\n", __FUNCTION__, __LINE__)
#else
  #define FUNC_TRACE()
#endif

namespace android_audio_legacy {

static int audpre_index, tx_iir_index;
static void * acoustic;
const uint32_t AudioHardware::inputSamplingRates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

// ----------------------------------------------------------------------------
#define ADD_DEVICE(key)			\
do{								\
	AudioCtrlDevice *current = new AudioCtrlDevice(key);	\
	mDevice->add(key, current);								\
															\
}while(0)

#define REPLAY_MODE_BIT		0
#define RECORD_MODE_BIT		1
#define REPLAY_MODE		(1 << REPLAY_MODE_BIT)
#define RECORD_MODE		(1 << RECORD_MODE_BIT)

unsigned int AudioHardware::AudioCtrlDevice::mCurrentRoute = REPLAY_MODE; // for mic & speaker

AudioHardware::AudioCtrlDevice::AudioCtrlDevice(enum device_t device)
{
	FUNC_TRACE();

	mDevice = device;
	mCurrentVolume = 0;
	switch (device) {
	case AudioCtrlDevice::SND_DEVICE_CURRENT:
		mMethodVolume = SOUND_MIXER_WRITE_MIC;
		break;
	case AudioCtrlDevice::SND_DEVICE_HEADSET:
		mMethodVolume = SOUND_MIXER_WRITE_VOLUME;
		break;
	case AudioCtrlDevice::SND_DEVICE_SPEAKER:
		mMethodVolume = SOUND_MIXER_WRITE_VOLUME;
		break;
	case AudioCtrlDevice::SND_DEVICE_HEADSET_AND_SPEAKER:
		mMethodVolume = SOUND_MIXER_WRITE_VOLUME;
		break;
	default:
		mMethodVolume = -1;
		break;
	}
}

status_t AudioHardware::AudioCtrlDevice::setVoiceVolume(float f)
{
	FUNC_TRACE();

	if (f < 0.0) {
		LOGW("setVoiceVolume(%f) under 0.0, assuming 0.0\n", f);
		f = 0.0;
	} else if (f > 1.0) {
		LOGW("setVoiceVolume(%f) over 1.0, assuming 1.0\n", f);
		f = 1.0;
	}

	int vol = ceil(f * 100.0);
	if (mMethodVolume != -1) {
		int fd;
		fd = open(MIXER_DEVICE, O_RDWR);
		if (fd < 0) {
			LOGW("Can not open snd device\n");
			return -EPERM;
		}

		if (ioctl(fd, mMethodVolume, &vol) < 0) {
			LOGW("snd_set_volume error\n");
			close(fd);
				return -EIO;
		}
		close(fd);
	}
	mCurrentVolume = vol;

	return NO_ERROR;
}

status_t AudioHardware::AudioCtrlDevice::doAudioRouteOrMute(int mute)
{
	FUNC_TRACE();

	static unsigned int pre_route = REPLAY_MODE;
	ALOGV("up pre_route = %x mCurrentRoute = %x\n",pre_route,mCurrentRoute);

	if ((mDevice != AudioCtrlDevice::SND_DEVICE_CURRENT) && (mCurrentRoute == pre_route)) {
		return NO_ERROR;
	}

	if ((mDevice == AudioCtrlDevice::SND_DEVICE_CURRENT) && (mCurrentRoute == pre_route)) {
		return NO_ERROR;
	}

	if (mDevice != AudioCtrlDevice::SND_DEVICE_CURRENT) {
		unsigned int curstate = mute ? 0 : 1;
		int fd;

		if (((mCurrentRoute & REPLAY_MODE) >> REPLAY_MODE_BIT) == curstate) {
			return NO_ERROR;
		}

		fd = open(MIXER_DEVICE, O_RDWR);
		if (fd <= 0) {
			ALOGE("Can not open snd device\n");
			return -EPERM;
		}

		if (ioctl(fd, SOUND_MIXER_WRITE_MUTE, curstate) < 0) {
			ALOGE("snd_set_volume error\n");
			close(fd);
			return -EIO;
		}

		close(fd);

		if (mute) {
			mCurrentRoute &= ~REPLAY_MODE;
		} else {
			mCurrentRoute |= REPLAY_MODE;
		}
	} else {
		unsigned int curstate = mute ? 0 : 1;

		if (((mCurrentRoute & RECORD_MODE) >> RECORD_MODE_BIT) == curstate) {
			return NO_ERROR;
		}

		if (mute) {
			mCurrentRoute &= ~RECORD_MODE;
		} else {
			mCurrentRoute |= RECORD_MODE;
		}
	}

	return NO_ERROR;
}

AudioHardware::AudioHardware() :
	mInit(false), mMicMute(true), mBluetoothNrec(true), mBluetoothId(0), mOutput(0), mInput(0)
{
	FUNC_TRACE();

	mDevice = new DefaultKeyedVector<unsigned int,AudioCtrlDevice*>(0);

	ALOGV("%s mDevice : %p\n", __FUNCTION__, mDevice);

	ADD_DEVICE(AudioCtrlDevice::SND_DEVICE_CURRENT);
	ADD_DEVICE(AudioCtrlDevice::SND_DEVICE_HEADSET);
	ADD_DEVICE(AudioCtrlDevice::SND_DEVICE_SPEAKER);
	ADD_DEVICE(AudioCtrlDevice::SND_DEVICE_HEADSET_AND_SPEAKER);

	mInit = true;
}

AudioHardware::~AudioHardware()
{
	FUNC_TRACE();

	AudioCtrlDevice* dev;

	delete mInput;
	delete mOutput;

	for(unsigned int i = 0; i < mDevice->size(); i++) {
		dev = mDevice->valueAt(i);
		delete dev;
	}

	mDevice->clear();
	delete mDevice;
	mInit = false;
}

status_t AudioHardware::initCheck()
{
	FUNC_TRACE();
	return mInit ? NO_ERROR : NO_INIT;
}

AudioStreamOut* AudioHardware::openOutputStream(
        uint32_t devices, int *format, uint32_t *channels, uint32_t *sampleRate, status_t *status)
{
	FUNC_TRACE();

	ALOGV("openOutputStream: format = %d, channels = %d, sampleRate = %d\n",
	     *format, *channels, *sampleRate);

	{ // scope for the lock
		Mutex::Autolock lock(mLock);

		// only one output stream allowed
		if (mOutput) {
			if (status) {
				*status = INVALID_OPERATION;
			}
			return 0;
		}

		// create new output stream
		AudioStreamOutJz47xx* out = new AudioStreamOutJz47xx();
		status_t lStatus = out->set(this, devices, format, channels, sampleRate);
		if (status) {
			*status = lStatus;
		}
		if (lStatus == NO_ERROR) {
			mOutput = out;
		} else {
			delete out;
		}
	}

	return mOutput;
}

void AudioHardware::closeOutputStream(AudioStreamOut* out)
{
	FUNC_TRACE();
	Mutex::Autolock lock(mLock);
	if (mOutput != out) {
		LOGW("Attempt to close invalid output stream");
	} else {
		mOutput = 0;
	}
}

AudioStreamIn* AudioHardware::openInputStream(
        uint32_t devices, int *format, uint32_t *channels, uint32_t *sampleRate, status_t *status,
        AudioSystem::audio_in_acoustics acoustic_flags)
{
	FUNC_TRACE();

	// check for valid input source
	if (!AudioSystem::isInputDevice((AudioSystem::audio_devices)devices)) {
		return 0;
	}

	mLock.lock();

	// input stream already open?
	if (mInput) {
		if (status) {
			*status = INVALID_OPERATION;
		}
		mLock.unlock();
		return 0;
	}

	AudioStreamInJz47xx* in = new AudioStreamInJz47xx();
	status_t lStatus = in->set(this, devices, format, channels, sampleRate, acoustic_flags);
	if (status) {
		*status = lStatus;
	}

	if (lStatus != NO_ERROR) {
		mLock.unlock();
		delete in;
		return 0;
	}
	mInput = in;
	mLock.unlock();
#ifdef DUMP_INPUT_DATA
	/* dump debug */
	if ((debug_input_fd = open("/data/dump_input.pcm", O_WRONLY | O_CREAT | O_APPEND)) <= 0) {
		ALOGE("[dump] open file error !");
	}
#endif


	ALOGV("AudioHardware::openInputStream -- status = %d\n", *status);
	return mInput;
}

void AudioHardware::closeInputStream(AudioStreamIn* in)
{
	FUNC_TRACE();
	Mutex::Autolock lock(mLock);

	if (mInput != in) {
		LOGW("Attempt to close invalid input stream");
	} else {
		ALOGV("@@@@ AudioHardware::closeInputStream\n");
		delete mInput;
		mInput = 0;
	}
#ifdef DUMP_INPUT_DATA
				/* dump debug */
				if (debug_input_fd) {
					close(debug_input_fd);
				}
#endif
}

status_t AudioHardware::setMode(int mode)
{
	status_t status = AudioHardwareBase::setMode(mode);
	if (status == NO_ERROR) {
		// make sure that doAudioRouteOrMute() is called by doRouting()
		// even if the new device selected is the same as current one.
		mCurSndDevice = -1;
	}
	return status;
}

bool AudioHardware::checkOutputStandby()
{
	FUNC_TRACE();

	if (mOutput) {
		if (!mOutput->checkStandby()) {
			return false;
		}
	}

	return true;
}

status_t AudioHardware::setMicMute(bool state)
{
	FUNC_TRACE();
	Mutex::Autolock lock(mLock);
	return setMicMute_nosync(state);
}

// always call with mutex held
status_t AudioHardware::setMicMute_nosync(bool state)
{
	FUNC_TRACE();
	if (mMicMute != state) {
		mMicMute = state;
		return doAudioRouteOrMute(AudioCtrlDevice::SND_DEVICE_CURRENT);
	}
	return NO_ERROR;
}

status_t AudioHardware::getMicMute(bool* state)
{
	FUNC_TRACE();
	*state = mMicMute;
	return NO_ERROR;
}

status_t AudioHardware::setParameters(const String8& keyValuePairs)
{
	FUNC_TRACE();

	String8 value;
	String8 key;
	AudioParameter param = AudioParameter(keyValuePairs);
	const char BT_NREC_KEY[] = "bt_headset_nrec";
	const char BT_NAME_KEY[] = "bt_headset_name";
	const char BT_NREC_VALUE_ON[] = "on";

	ALOGV("setParameters() %s\n", keyValuePairs.string());

	if (keyValuePairs.length() == 0) return BAD_VALUE;

	key = String8(BT_NREC_KEY);
	if (param.get(key, value) == NO_ERROR) {
		if (value == BT_NREC_VALUE_ON) {
			mBluetoothNrec = true;
		} else {
			mBluetoothNrec = false;
			ALOGI("Turning noise reduction and echo cancellation off for BT headset\n");
		}
	}

	key = String8(BT_NAME_KEY);
	if (param.get(key, value) == NO_ERROR) {
		mBluetoothId = 0;

#if 0
		for (int i = 0; i < mNumSndEndpoints; i++) {
			if (!strcasecmp(value.string(), mSndEndpoints[i].name)) {
				mBluetoothId = mSndEndpoints[i].id;
				ALOGI("Using custom acoustic parameters for %s\n", value.string());
				break;
			}
		}
#endif

		if (mBluetoothId == 0) {
			ALOGI("Using default acoustic parameters (%s not in acoustic database)\n",
			     value.string());
			doRouting(NULL);
		}
	}

	return NO_ERROR;
}

String8 AudioHardware::getParameters(const String8& keys)
{
	FUNC_TRACE();
	AudioParameter param = AudioParameter(keys);
	return param.toString();
}

static unsigned calculate_audpre_table_index(unsigned index)
{
	FUNC_TRACE();

	switch (index) {
        case 48000:
		return SAMP_RATE_INDX_48000;
        case 44100:
		return SAMP_RATE_INDX_44100;
        case 32000:
		return SAMP_RATE_INDX_32000;
        case 24000:
		return SAMP_RATE_INDX_24000;
        case 22050:
		return SAMP_RATE_INDX_22050;
        case 16000:
		return SAMP_RATE_INDX_16000;
        case 12000:
		return SAMP_RATE_INDX_12000;
        case 11025:
		return SAMP_RATE_INDX_11025;
        case 8000:
		return SAMP_RATE_INDX_8000;
        default:
		return -1;
	}
}

size_t AudioHardware::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
	FUNC_TRACE();

	if (checkInputSampleRate(sampleRate) != NO_ERROR) {
		LOGW("getInputBufferSize bad sampling rate: %d\n", sampleRate);
		return 0;
	}
	if (format != AudioSystem::PCM_16_BIT) {
		LOGW("getInputBufferSize bad format: %d\n", format);
		return 0;
	}
	if (channelCount < 1 || channelCount > 2) {
		LOGW("getInputBufferSize bad channel count: %d\n", channelCount);
		return 0;
	}

	return (AUDIO_HW_IN_BUFFERSIZE / sizeof(short)) * channelCount;
}


#define setDeviceVolume(d, v)						\
do {									\
	AudioCtrlDevice *dev = mDevice->valueFor(d);			\
	if (!dev) {							\
		ALOGE("Could not find device in setDeviceVolume\n");		\
	} else {							\
		dev->setVoiceVolume(v);					\
	}								\
} while(0)

status_t AudioHardware::setVoiceVolume(float v)
{
	FUNC_TRACE();
	Mutex::Autolock lock(mLock);

	ALOGV("SND_DEVICE_CURRENT = %d\n", AudioCtrlDevice::SND_DEVICE_CURRENT);
	ALOGV("%s mDevice : %p\n", __FUNCTION__, mDevice);

	setDeviceVolume(AudioCtrlDevice::SND_DEVICE_CURRENT, v);
	return NO_ERROR;
}

status_t AudioHardware::setMasterVolume(float v)
{
	FUNC_TRACE();
	Mutex::Autolock lock(mLock);
	ALOGI("Set master volume to %f\n", v);

	ALOGI("set device volume ----------- SND_DEVICE_HANDSET\n");
	setDeviceVolume(AudioCtrlDevice::SND_DEVICE_HANDSET, v);
	ALOGI("set device volume ----------- SND_DEVICE_SPEAKER\n");
	setDeviceVolume(AudioCtrlDevice::SND_DEVICE_SPEAKER, v);
	ALOGI("set device volume ----------- SND_DEVICE_BT\n");
	setDeviceVolume(AudioCtrlDevice::SND_DEVICE_BT, v);
	ALOGI("set device volume ----------- SND_DEVICE_HEADSET\n");
	setDeviceVolume(AudioCtrlDevice::SND_DEVICE_HEADSET, v);
	ALOGI("set device volume ----------- finish\n");
	// We return an error code here to let the audioflinger do in-software
	// volume on top of the maximum volume that we set through the SND API.
	// return error - software mixer will handle it
	return NO_ERROR;
}

unsigned int g_in_call = 0;

static status_t do_route_audio_rpc(uint32_t device,
                                   bool ear_mute, bool mic_mute)
{
	FUNC_TRACE();

	if (device == -1UL)
		return NO_ERROR;

	int fd;

#if LOG_SND_RPC
	ALOGD("rpc_snd_set_device(%d, %d, %d)\n", device, ear_mute, mic_mute);
#endif

	fd = open(MIXER_DEVICE, O_RDWR);
	if (fd < 0) {
		ALOGE("Can not open snd device");
		return -EPERM;
	}
	// RPC call to switch audio path
	/* rpc_snd_set_device(
	 *     device,            # Hardware device enum to use
	 *     ear_mute,          # Set mute for outgoing voice audio
	 *                        # this should only be unmuted when in-call
	 *     mic_mute,          # Set mute for incoming voice audio
	 *                        # this should only be unmuted when in-call or
	 *                        # recording.
	 *  )
	 */
	struct snd_device_config args;
	args.device = device;
	args.ear_mute = ear_mute ? SND_MUTE_MUTED : SND_MUTE_UNMUTED;
	args.mic_mute = mic_mute ? SND_MUTE_MUTED : SND_MUTE_UNMUTED;

	if (ioctl(fd, SND_SET_DEVICE, &args) < 0) {
		ALOGE("snd_set_device error.");
		close(fd);
		return -EIO;
	}

	close(fd);

	if (!ear_mute) {
		g_in_call = 1;
	} else {
		g_in_call = 0;
	}

	return NO_ERROR;
}

// always call with mutex held
status_t AudioHardware::doAudioRouteOrMute(uint32_t device)
{
	FUNC_TRACE();

	AudioCtrlDevice *dev;
	if (device == (uint32_t)AudioCtrlDevice::SND_DEVICE_BT) {
		if (mBluetoothId) {
			device = mBluetoothId;
		} else if (!mBluetoothNrec) {
			device = AudioCtrlDevice::SND_DEVICE_BT_EC_OFF;
		}
	}

	ALOGV("doAudioRouteOrMute() device %x, mMode %d, mMicMute %d\n", device, mMode, mMicMute);
	return do_route_audio_rpc(device,
				  mMode != AudioSystem::MODE_IN_CALL, mMicMute);
}

static int count_bits(uint32_t vector)
{
	FUNC_TRACE();
	int bits;
	for (bits = 0; vector; bits++) {
		vector &= vector - 1;
	}
	return bits;
}

status_t AudioHardware::doRouting(AudioStreamInJz47xx *input)
{
	/* debug */
	FUNC_TRACE();

//	printf("\n\n==============================\n");
//	printf("==============================\n\n\n");

	Mutex::Autolock lock(mLock);
	uint32_t outputDevices = mOutput->getDevices();
	status_t ret = NO_ERROR;

	int sndDevice = -1;

	if (input != NULL) {
		uint32_t inputDevice = input->getDevices();
		ALOGI("do input routing device %x\n", inputDevice);
		if (inputDevice != 0) {
			if (inputDevice & AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
				ALOGI("Routing audio to Bluetooth PCM\n");
				sndDevice = AudioCtrlDevice::SND_DEVICE_BT;
			} else if (inputDevice & AudioSystem::DEVICE_IN_WIRED_HEADSET) {
				if ((outputDevices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) &&
				    (outputDevices & AudioSystem::DEVICE_OUT_SPEAKER)) {
					ALOGI("Routing audio to Wired Headset and Speaker\n");
					sndDevice = AudioCtrlDevice::SND_DEVICE_HEADSET_AND_SPEAKER;
				} else {
					ALOGI("Routing audio to Wired Headset\n");
					sndDevice = AudioCtrlDevice::SND_DEVICE_HEADSET;
				}
			} else {
				if (outputDevices & AudioSystem::DEVICE_OUT_SPEAKER) {
					ALOGI("Routing audio to Speakerphone\n");
					sndDevice = AudioCtrlDevice::SND_DEVICE_SPEAKER;
				} else {
					ALOGI("Routing audio to Handset\n");
					sndDevice = AudioCtrlDevice::SND_DEVICE_HANDSET;
				}
			}
		}
		// if inputDevice == 0, restore output routing
	}

	if (sndDevice == -1) {
		if (outputDevices & (outputDevices - 1)) {
			if ((outputDevices & AudioSystem::DEVICE_OUT_SPEAKER) == 0) {
				LOGW("Hardware does not support requested route combination (%#X),"
				     " picking closest possible route...", outputDevices);
			}
		}

		if (outputDevices &
		    (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET)) {
			ALOGI("Routing audio to Bluetooth PCM\n");
			sndDevice = AudioCtrlDevice::SND_DEVICE_BT;
		} else if (outputDevices & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT) {
			ALOGI("Routing audio to Bluetooth PCM\n");
			sndDevice = AudioCtrlDevice::SND_DEVICE_CARKIT;
		} else if ((outputDevices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) &&
			   (outputDevices & AudioSystem::DEVICE_OUT_SPEAKER)) {
			ALOGI("Routing audio to Wired Headset and Speaker\n");
			sndDevice = AudioCtrlDevice::SND_DEVICE_HEADSET_AND_SPEAKER;
		}else if (outputDevices & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE) {
			if (outputDevices & AudioSystem::DEVICE_OUT_SPEAKER) {
				ALOGI("Routing audio to No microphone Wired Headset and Speaker (%d,%x)\n",
				     mMode, outputDevices);
				sndDevice = AudioCtrlDevice::SND_DEVICE_HEADSET_AND_SPEAKER;
			} else {
				ALOGI("Routing audio to No microphone Wired Headset (%d,%x)\n", mMode, outputDevices);
				sndDevice = AudioCtrlDevice::SND_DEVICE_NO_MIC_HEADSET;
			}
		} else if (outputDevices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) {
			ALOGI("Routing audio to Wired Headset\n");
			sndDevice = AudioCtrlDevice::SND_DEVICE_HEADSET;
		} else if (outputDevices & AudioSystem::DEVICE_OUT_SPEAKER) {
			ALOGI("Routing audio to Speakerphone\n");
			sndDevice = AudioCtrlDevice::SND_DEVICE_SPEAKER;
		} else {
			ALOGI("Routing audio to Handset\n");
			sndDevice = AudioCtrlDevice::SND_DEVICE_HANDSET;
		}
	}

	if (sndDevice != -1 && sndDevice != mCurSndDevice) {
		ret = doAudioRouteOrMute(sndDevice);
		mCurSndDevice = sndDevice;
	}

	return ret;

}

status_t AudioHardware::checkMicMute()
{
	FUNC_TRACE();
	Mutex::Autolock lock(mLock);
	if (mMode != AudioSystem::MODE_IN_CALL) {
		setMicMute_nosync(true);
	}
	return NO_ERROR;
}

status_t AudioHardware::dumpInternals(int fd, const Vector<String16>& args)
{
	FUNC_TRACE();

	const size_t SIZE = 256;
	char buffer[SIZE];
	String8 result;

	result.append("AudioHardware::dumpInternals\n");
	snprintf(buffer, SIZE, "\tmInit: %s\n", mInit? "true": "false");
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmMicMute: %s\n", mMicMute? "true": "false");
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmBluetoothNrec: %s\n", mBluetoothNrec? "true": "false");
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmBluetoothId: %d\n", mBluetoothId);
	result.append(buffer);

	::write(fd, result.string(), result.size());

	return NO_ERROR;
}

status_t AudioHardware::dump(int fd, const Vector<String16>& args)
{
	FUNC_TRACE();
	dumpInternals(fd, args);

	if (mInput) {
		mInput->dump(fd, args);
	}
	if (mOutput) {
		mOutput->dump(fd, args);
	}

	return NO_ERROR;
}

uint32_t AudioHardware::getInputSampleRate(uint32_t sampleRate)
{
	FUNC_TRACE();

	uint32_t i;
	uint32_t prevDelta;
	uint32_t delta;

	for (i = 0, prevDelta = 0xFFFFFFFF;
	     i < sizeof(inputSamplingRates) / sizeof(uint32_t);
	     i++, prevDelta = delta) {
		delta = abs(sampleRate - inputSamplingRates[i]);
		if (delta > prevDelta) {
			break;
		}
	}

	// i is always > 0 here
	return inputSamplingRates[i-1];
}

status_t AudioHardware::checkInputSampleRate(uint32_t sampleRate)
{
	FUNC_TRACE();

	for (uint32_t i = 0; i < sizeof(inputSamplingRates) / sizeof(uint32_t); i++) {
		if (sampleRate == inputSamplingRates[i]) {
			return NO_ERROR;
		}
	}

	return BAD_VALUE;
}

// ----------------------------------------------------------------------------
AudioHardware::AudioStreamDevice::AudioStreamDevice(){
	FUNC_TRACE();

	mBufferSize = AUDIO_HW_IN_BUFFERSIZE;
	mChannels = AudioSystem::CHANNEL_OUT_STEREO;
	mSampleRate = 44100;
	mFormat = AudioSystem::PCM_16_BIT;
	mStandby = true;
	mOpenType = UNKOWN;
	mFd = -1;
	map_fd = -1;
}

AudioHardware::AudioStreamDevice::~AudioStreamDevice()
{
	FUNC_TRACE();
	closeDevice();
}

status_t AudioHardware::AudioStreamDevice::openDevice(enum open_type flag)
{
	FUNC_TRACE();
	int map_size;
	int fd = -1;
	mapdata_start = NULL;

	if (flag == READMODE) {
		fd = ::open(DSP_DEVICE, O_RDONLY);
	}
	if (flag == WRITEMODE) {
		fd = ::open(DSP_DEVICE, O_WRONLY);
	}

	if (fd <= 0) {
		ALOGE("open %s audio device error\n", (flag == READMODE) ? "Read" : "Write");
		return -1;
	} else {
		ALOGV("open %s audio device finish\n", (flag == READMODE) ? "Read" : "Write");
	}

	if(flag == WRITEMODE) {
		status_t	status = NO_INIT;
		status = ::ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &map_size);
		if (status) {
			ALOGE("IOCTL SNDCTL_DSP_GETBLKSIZE return error %d\n", status);
		}else
		{
			if(map_size) {
				map_fd = ::open(DSP_DEVICE, O_RDWR);
				if(map_fd < 0) {
					ALOGE("can not mmap!");
				}else
				{
					mapdata_start = (unsigned char *)::mmap(NULL,map_size,PROT_WRITE | PROT_READ,MAP_SHARED,map_fd,0);
					ALOGV("map out memory size = %d",map_size);
					ALOGE("mapdata_start = %p size = %d",mapdata_start,map_size);
				}
			}
		}
	}
	mOpenType = flag;
	mFd = fd;

	return NO_ERROR;
}
status_t AudioHardware::AudioStreamDevice::closeDevice()
{
	FUNC_TRACE();
	if(map_fd >= 0) {
		::close(map_fd);
	}
	if (mFd >= 0) {
		::close(mFd);
	}

	mFd = -1;
	return NO_ERROR;
}

status_t AudioHardware::AudioStreamDevice::setStandby(bool val)
{
	FUNC_TRACE();

	mStandby = val;


	int fd = open(MIXER_DEVICE, O_RDWR);
	if (fd < 0) {
		ALOGE("Can not open snd device\n");
		return -EPERM;
	}

	unsigned int sw = val ? 1 : 0;

	if (ioctl(fd, SND_SET_STANDBY, &sw) < 0) {
		ALOGE("Ioctl SND_SET_STANDBY error.\n");
		close(fd);
		return -EIO;
	}
	close(fd);


	return NO_ERROR;
}
status_t AudioHardware::AudioStreamDevice::GetBuffer(AudioStreamOut::BufferInfo *binfo)
{
	int fd = mFd;
	direct_info info;
	if(map_fd < 0) return NO_INIT;
	if (::read(map_fd, &info,sizeof(direct_info)) < 0) {
			ALOGE("Ioctl SNDCTL_EXT_DIRECT_GETNODE error.\n");
			close(map_fd);
			mapdata_start = 0;
			return NO_INIT;
	}
	binfo->data = (char *)mapdata_start + info.offset;
	binfo->bytes = info.bytes;
	//ALOGE("GetBuffer = %p",binfo->data);
	//printf("info.offset = %x size = %d\n",info.offset,info.bytes);
	return NO_ERROR;
}
status_t AudioHardware::AudioStreamDevice::SetBuffer(AudioStreamOut::BufferInfo *binfo)
{
	int fd = mFd;
	direct_info info;

	if (checkStandby()) {
		setStandby(false);
	}

	info.offset = ((unsigned int)binfo->data - (unsigned int)mapdata_start);
	info.bytes = binfo->bytes;
	if (write(map_fd,&info,sizeof(direct_info)) < 0) {
		ALOGE("Ioctl SNDCTL_EXT_DIRECT_SETNODE error.\n");
		close(map_fd);
		mapdata_start = 0;
		return NO_INIT;
	}
	return NO_ERROR;
}
int AudioHardware::AudioStreamDevice::Write(const void* buffer, size_t bytes){
	int fd = mFd;
	int written = 0;

	if (checkStandby()) {
		setStandby(false);
	}

	if(!mapdata_start)
		written = ::write(fd,buffer,bytes);
	else{
		status_t	status = NO_INIT;
		unsigned char *data;
		direct_info info;
		if (::read(map_fd, &info,sizeof(direct_info)) < 0) {
			ALOGE("Ioctl SNDCTL_EXT_DIRECT_GETNODE error.\n");
			close(map_fd);
			mapdata_start = 0;
			return -EIO;
		}
		written = info.bytes;
		if(written > (int)bytes)
			written = bytes;
		data = mapdata_start + info.offset;
		memcpy(data,buffer,written);
		info.bytes = written;

#ifdef DUMP_OUTPUT_DATA
		{
			int debug_output_fd;
			/* open */
			if ((debug_output_fd = open("/data/dump_output.pcm", O_WRONLY | O_CREAT | O_APPEND)) <= 0) {
				ALOGE("[dump] open file error !");
			}
			/* write */
			ALOGD("written = %d, debug_output_fd = %d\n", written, debug_output_fd);
			int ret = write(debug_output_fd, buffer, written);
			if (ret != written) {
				ALOGE("[dump] write error !");
			}
			/* close */
			if (debug_output_fd) {
				close(debug_output_fd);
			}
		}
#endif

		if (write(map_fd,&info,sizeof(direct_info)) < 0) {
			ALOGE("Ioctl SNDCTL_EXT_DIRECT_SETNODE error.\n");
			close(map_fd);
			mapdata_start = 0;
			return -EIO;
		}

	}
	return written;
}
/**
 * Config driver by doing ioctl
 *
 * NOTE: Make sure that the parameters of ioctl should be recongnized by driver without convert
 *
 */
status_t AudioHardware::AudioStreamDevice::set(int format, int channels, uint32_t rate)
{
	status_t	status = NO_INIT;
	uint32_t	samplerate;
	uint32_t	channelCount;
	int		format_tmp;
	int		format_cvt;
	size_t		buffersize;

	FUNC_TRACE();
	ALOGD("@@@@ AudioHardware::AudioStreamDevice::set format = %d, channels = %d, rate = %d\n", format, channels, rate);

	if (mFd <= 0) {
		ALOGE("Devices has not opened\n");
		return -1;
	}

	/* IOCTL set channels */
	status = ioctl(mFd, SOUND_PCM_READ_CHANNELS, &channelCount);
	if (status) {
		ALOGE("IOCTL SOUND_PCM_READ_CHANNELS return error %d\n", status);
		goto _L_IOC_ERROR;
	}
	if (channelCount != AudioSystem::popCount(channels)) {
		//printf("@@@@ set channelCount to %d\n", AudioSystem::popCount(channels));

		channelCount = AudioSystem::popCount(channels);
		status = ioctl(mFd, SNDCTL_DSP_CHANNELS, &channelCount);
		if (status) {
			ALOGE("IOCTL SNDCTL_DSP_CHANNELS return error %d\n", status);
			goto _L_IOC_ERROR;
		}
		mChannels = channels;
	}

	/* IOCTL set rate */
	status = ioctl(mFd, SOUND_PCM_READ_RATE, &samplerate);
	if (status) {
		ALOGE("IOCTL SOUND_PCM_READ_RATE return error %d\n", status);
		goto _L_IOC_ERROR;
	}
	if (samplerate != rate) {
		status = ioctl(mFd, SNDCTL_DSP_SPEED, &rate);
		if(status) {
			ALOGE("IOCTL SNDCTL_DSP_SPEED return error %d\n", status);
			goto _L_IOC_ERROR;
		}
		mSampleRate = rate;
	}

	/* IOCTL set format */
	switch (format) {
	case AudioSystem::PCM_16_BIT:
		format_cvt = AFMT_S16_LE;
		break;
	case AudioSystem::PCM_8_BIT:
		// May be ...
		format_cvt = AFMT_U8;
		break;
	default:
		ALOGE("Unkown format to set\n");
		goto _L_IOC_ERROR;
	}

	status = ioctl(mFd, SOUND_PCM_READ_BITS, &format_tmp);
	if (status) {
		ALOGE("\n---- IOCTL SOUND_PCM_READ_BITS return error %d\n", status);
		goto _L_IOC_ERROR;
	}
	if (format_tmp != format_cvt) {
		status = ioctl(mFd, SNDCTL_DSP_SETFMT, &format_cvt);
		if(status) {
			ALOGE("\n---- IOCTL SNDCTL_DSP_SETFMT return error %d\n", status);
			goto _L_IOC_ERROR;
		}
	}

	ALOGD("@@@@ AudioHardware::AudioStreamDevice::set format_cvt = %d, channels = %d, rate = %d\n", format_tmp, channelCount, mSampleRate);
	return NO_ERROR;

_L_IOC_ERROR:
	ALOGE("Audio HAL open device error!\n");
	if (mFd != -1) {
		::close(mFd);
	}

	mFd = status = -1;
	return status;
}

AudioHardware::AudioStreamOutJz47xx::AudioStreamOutJz47xx() :
	mHardware(0), mStartCount(0), mRetryCount(0)
{
	int status;

	openDevice(AudioStreamDevice::WRITEMODE);

	// Set parameters at the beginning
	status = AudioStreamDevice::set(mFormat, mChannels, mSampleRate);
	if (status != NO_ERROR) {
		ALOGE("Cound not create AudioStreamOutJz47xx, AudiostreamDevice::set return ERROR");
	}
}

status_t AudioHardware::AudioStreamOutJz47xx::set(
        AudioHardware* hw, uint32_t devices, int *pFormat, uint32_t *pChannels, uint32_t *pRate)
{
	ALOGV("@@@@ AudioHardware::AudioStreamOutJz47xx::set - format = %d, channels = %d, rate = %d\n",
	       *pFormat, *pChannels, *pRate);

	int status;
	int lFormat = pFormat ? *pFormat : 0;
	uint32_t lChannels = pChannels ? *pChannels : 0;
	uint32_t lRate = pRate ? *pRate : 0;

	// fix up defaults
	if (lFormat == 0) {
		lFormat = format();
	}
	if (lChannels == 0) {
		lChannels = channels();
	}
	if (lRate == 0) {
		lRate = sampleRate();
	}

	ALOGV("@@@@ AudioHardware::AudioStreamOutJz47xx::set - after fix - format = %d, channels = %d, rate = %d\n",
	       lFormat, lChannels, lRate);
	ALOGV("@@@@ AudioHardware::AudioStreamOutJz47xx::set - default - format = %d, channels = %d, rate = %d\n",
	       format(), channels(), sampleRate());

	// check values
	if ((lFormat != format()) ||
	    (lChannels != channels()) ||
	    (lRate != sampleRate())) {

		if (pFormat) {
			*pFormat = format();
		}
		if (pChannels) {
			*pChannels = channels();
		}
		if (pRate) {
			*pRate = sampleRate();
		}

		ALOGV("---- out set ret format = %d, channels = %d, rate = %d\n", *pFormat, *pChannels, *pRate);
		return BAD_VALUE;
	}

	// write back
	if (pFormat) {
		*pFormat = lFormat;
	}
	if (pChannels) {
		*pChannels = channels();
	}
	if (pRate) {
		*pRate = lRate;
	}

	mDevices = devices;
	mHardware = hw;

	return NO_ERROR;
}

status_t AudioHardware::AudioStreamOutJz47xx::getRenderPosition(uint32_t *p)
{
	// copy from msm7k, need fix
    return INVALID_OPERATION;
}

AudioHardware::AudioStreamOutJz47xx::~AudioStreamOutJz47xx()
{
	FUNC_TRACE();
	closeDevice();
	mHardware->closeOutputStream(this);
}

ssize_t AudioHardware::AudioStreamOutJz47xx::write(const void* buffer, size_t bytes)
{
	if (g_in_call) {
		/* Ignore any write operations when calling. */
		return bytes;
	}

	//ALOGD("AudioStreamOutJz47xx::write(%p, %u)", buffer, bytes);
	status_t status = NO_INIT;
	size_t count = bytes;
	const uint8_t* p = static_cast<const uint8_t*>(buffer);
	int fd = getDeviceHandle();

	if (!AudioStreamDevice::checkDevice()) {
		goto Error;
	}

	while (count) {
		//printf("write count=%d\n",count);
		ssize_t written = AudioStreamDevice::Write(p, count);
		//printf("written = %d\n",written);

		if (written >= 0) {
			count -= written;
			p += written;
		} else {
			if (errno != EAGAIN) {
				return written;
			}
			mRetryCount++;
			//LOGW("EAGAIN - retry");
		}
	}
	// printf("return bytes\n");
	return bytes;

Error:
	// Simulate audio output timing in case of error
	usleep(bytes * 1000000 / frameSize() / sampleRate());
	return bytes;
}

status_t AudioHardware::AudioStreamOutJz47xx::dump(int fd, const Vector<String16>& args)
{
	FUNC_TRACE();
	const size_t SIZE = 256;
	char buffer[SIZE];
	String8 result;

	result.append("AudioStreamOutJz47xx::dump\n");
	snprintf(buffer, SIZE, "\tsample rate: %d\n", sampleRate());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tbuffer size: %d\n", bufferSize());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tchannel count: %d\n", channelCount());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tformat: %d\n", format());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmHardware: %p\n", mHardware);
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmFd: %d\n", getDeviceHandle());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmStartCount: %d\n", mStartCount);
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmRetryCount: %d\n", mRetryCount);
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmStandby: %s\n", AudioStreamDevice::checkStandby()? "true": "false");
	result.append(buffer);

	::write(fd, result.string(), result.size());

	return NO_ERROR;
}

status_t AudioHardware::AudioStreamOutJz47xx::setParameters(const String8& keyValuePairs)
{
	AudioParameter param = AudioParameter(keyValuePairs);
	String8 key = String8(AudioParameter::keyRouting);
	status_t status = NO_ERROR;
	int device;
	ALOGV("AudioStreamOutJz47xx::setParameters() %s\n", keyValuePairs.string());

	if (param.getInt(key, device) == NO_ERROR) {
		mDevices = device;
		ALOGV("-----------\nset output routing 0x%08x\n-----------\n", mDevices);
		status = mHardware->doRouting(NULL);
		param.remove(key);
	}

	if (param.size()) {
		status = BAD_VALUE;
	}

	return status;
}

String8 AudioHardware::AudioStreamOutJz47xx::getParameters(const String8& keys)
{
	AudioParameter param = AudioParameter(keys);
	String8 value;
	String8 key = String8(AudioParameter::keyRouting);

	if (param.get(key, value) == NO_ERROR) {
		ALOGV("get routing %x\n", mDevices);
		param.addInt(key, (int)mDevices);
	}

	ALOGV("AudioStreamOutJz47xx::getParameters() %s", param.toString().string());
	return param.toString();
}

// ----------------------------------------------------------------------------

AudioHardware::AudioStreamInJz47xx::AudioStreamInJz47xx() :
	mHardware(0), mState(AUDIO_INPUT_CLOSED), mRetryCount(0)
{
	mFd = -1;
	mFormat = AUDIO_HW_IN_FORMAT;
	mChannels = AUDIO_HW_IN_CHANNELS;
	mSampleRate = AUDIO_HW_IN_SAMPLERATE;
	mBufferSize = AUDIO_HW_IN_BUFFERSIZE;
	mDevices = 0;
}

status_t AudioHardware::AudioStreamInJz47xx::set(
	AudioHardware* hw, uint32_t devices, int *pFormat, uint32_t *pChannels, uint32_t *pRate,
	AudioSystem::audio_in_acoustics acoustic_flags)
{
	status_t status = NO_INIT;

	// Check config
	if (pFormat == 0 || *pFormat != AUDIO_HW_IN_FORMAT) {
		*pFormat = AUDIO_HW_IN_FORMAT;
		return BAD_VALUE;
	}
	if (pRate == 0) {
		return BAD_VALUE;
	}

	uint32_t rate = hw->getInputSampleRate(*pRate);
	if (rate != *pRate) {
		*pRate = rate;
		return BAD_VALUE;
	}
	if (pChannels == 0 || (*pChannels != AudioSystem::CHANNEL_IN_MONO &&
			       *pChannels != AudioSystem::CHANNEL_IN_STEREO)) {
		*pChannels = AUDIO_HW_IN_CHANNELS;
		return BAD_VALUE;
	}

	// Open device and do configuration
	if (getDeviceHandle() <= 0) {
		status = openDevice(AudioStreamDevice::READMODE);
	}
	if (status != NO_ERROR) {
		goto Error;
	}

	ALOGV("@@@@ before AudioStreamDevice::set %d, %d, %d\n", *pFormat, *pChannels, *pRate);
	status = AudioStreamDevice::set(*pFormat, *pChannels, *pRate);
	if (status != NO_ERROR) {
		goto Error;
	}

	mHardware = hw;

	return NO_ERROR;
Error:
	return status;
}

AudioHardware::AudioStreamInJz47xx::~AudioStreamInJz47xx()
{
	ALOGV("@@@@ AudioHardware::AudioStreamInJz47xx::~AudioStreamInJz47xx()\n");
}

ssize_t AudioHardware::AudioStreamInJz47xx::read(void* buffer, ssize_t bytes)
{
	FUNC_TRACE();
	//ALOGV("AudioStreamInJz47xx::read(%p, %ld)\n", buffer, bytes);

	if (!mHardware) {
		return -1;
	}


	size_t count = bytes;
	uint8_t* p = static_cast<uint8_t*>(buffer);
	int fd = getDeviceHandle();

	if (fd <= 0) {
		return -1;
	}

	if (checkStandby()) {
		setStandby(false);
	}

	while (count) {
		//ALOGV("AudioStreamInJz47xx: before read, count = %d\n", count);
		ssize_t bytesRead = ::read(fd, p, count);
		//ALOGV("AudioStreamInJz47xx: read dev ret %d\n", bytesRead);
		if (bytesRead >= 0) {
			count -= bytesRead;
			p += bytesRead;

		} else {
			if (errno != EAGAIN) {
				return bytesRead;
			}
			mRetryCount++;
			LOGW("EAGAIN - retrying");
		}
	}

#ifdef DUMP_INPUT_DATA
			/* dump debug */
			int ret = write(debug_input_fd, buffer, bytes);
			if (ret != bytes) {
				ALOGE("[dump] write error !");
			}
			//printf("bytesRead = %d\n", bytesRead);
#endif
	//ALOGD("AudioStreamInJz47xx: read return bytes = %d", bytes);
	return bytes;
}

status_t AudioHardware::AudioStreamInJz47xx::dump(int fd, const Vector<String16>& args)
{
	FUNC_TRACE();
	const size_t SIZE = 256;
	char buffer[SIZE];
	String8 result;

	result.append("AudioStreamInJz47xx::dump\n");
	snprintf(buffer, SIZE, "\tsample rate: %d\n", sampleRate());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tbuffer size: %d\n", bufferSize());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tchannel count: %d\n", channelCount());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tformat: %d\n", format());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmHardware: %p\n", mHardware);
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmFd count: %d\n", getDeviceHandle());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmState: %d\n", checkStandby());
	result.append(buffer);
	snprintf(buffer, SIZE, "\tmRetryCount: %d\n", mRetryCount);
	result.append(buffer);

	::write(fd, result.string(), result.size());

	return NO_ERROR;
}

status_t AudioHardware::AudioStreamInJz47xx::setParameters(const String8& keyValuePairs)
{
	AudioParameter param = AudioParameter(keyValuePairs);
	String8 key = String8(AudioParameter::keyRouting);
	status_t status = NO_ERROR;
	int device;
	ALOGV("AudioStreamInJz47xx::setParameters() %s\n", keyValuePairs.string());

	if (param.getInt(key, device) == NO_ERROR) {
		mDevices = device;
		ALOGV("-----------\nset input routing 0x%08x\n-----------\n", mDevices);
		status = mHardware->doRouting(NULL);
		param.remove(key);
	}

	if (param.size()) {
		status = BAD_VALUE;
	}
	return status;
}

String8 AudioHardware::AudioStreamInJz47xx::getParameters(const String8& keys)
{
	AudioParameter param = AudioParameter(keys);
	String8 value;
	String8 key = String8(AudioParameter::keyRouting);

	if (param.get(key, value) == NO_ERROR) {
		ALOGV("get routing %x", mDevices);
		param.addInt(key, (int)mDevices);
	}

	ALOGV("AudioStreamInJz47xx::getParameters() %s", param.toString().string());
	return param.toString();
}

// ----------------------------------------------------------------------------
extern "C" AudioHardwareInterface* createAudioHardware(void) {
	ALOGD("Creating AudioHardware\n");
	return new AudioHardware();
}

}; // namespace android
