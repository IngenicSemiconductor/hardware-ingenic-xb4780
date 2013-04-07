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

#ifndef _ANDROID_AUDIO_HARDWARE_H_
#define _ANDROID_AUDIO_HARDWARE_H_

#include <stdint.h>
#include <sys/types.h>

#include <utils/threads.h>
#include <utils/String8.h>

#include <hardware_legacy/AudioHardwareBase.h>

#include <utils/KeyedVector.h>
#include <linux/soundcard.h>

namespace android_audio_legacy {
	using android::KeyedVector;
	using android::DefaultKeyedVector;
	using android::Mutex;
	using android::String8;
// ----------------------------------------------------------------------------
// Kernel driver interface
//

#define SAMP_RATE_INDX_8000	0
#define SAMP_RATE_INDX_11025	1
#define SAMP_RATE_INDX_12000	2
#define SAMP_RATE_INDX_16000	3
#define SAMP_RATE_INDX_22050	4
#define SAMP_RATE_INDX_24000	5
#define SAMP_RATE_INDX_32000	6
#define SAMP_RATE_INDX_44100	7
#define SAMP_RATE_INDX_48000	8

#define CODEC_TYPE_PCM		0
#define AUDIO_HW_NUM_OUT_BUF	1 // Number of buffers in audio driver for output

// TODO: determine actual audio DSP and hardware latency
#define AUDIO_HW_OUT_LATENCY_MS	0 // Additionnal latency introduced by audio DSP and hardware in ms

#define AUDIO_HW_IN_SAMPLERATE	8000	// Default audio input sample rate
#define AUDIO_HW_IN_CHANNELS	1	// Default audio input number of channels
#define AUDIO_HW_IN_BUFFERSIZE	4096	// Default audio input buffer size
#define AUDIO_HW_IN_FORMAT	(AudioSystem::PCM_16_BIT)	// Default audio input sample format

#define DSP_DEVICE		"/dev/snd/dsp"
#define MIXER_DEVICE		"/dev/snd/mixer"


/* IOCTL */
struct snd_device_config {
	uint32_t device;
	uint32_t ear_mute;
	uint32_t mic_mute;
};

#define SND_IOCTL_MAGIC		's'
#define SND_SET_DEVICE		_IOW(SND_IOCTL_MAGIC, 2, struct snd_device_config *)
#define SND_SET_STANDBY		_IOW(SND_IOCTL_MAGIC, 4, unsigned int *)
#define SND_MUTE_UNMUTED	0
#define SND_MUTE_MUTED		1



// ----------------------------------------------------------------------------

class AudioHardware : public  AudioHardwareBase {

	class AudioStreamOutJz47xx;
	class AudioStreamInJz47xx;

public:
	AudioHardware();
	virtual ~AudioHardware();
	virtual status_t	initCheck();

	virtual status_t	setVoiceVolume(float volume);
	virtual status_t	setMasterVolume(float volume);

	// For phone state
	virtual status_t	setMode(int mode);

	// mic mute
	virtual status_t	setMicMute(bool state);
	virtual status_t	getMicMute(bool* state);

	// Temporary interface, do not use
	// TODO: Replace with a more generic key:value get/set mechanism
	virtual status_t	setParameters(const String8& keyValuePairs);
	virtual String8		getParameters(const String8& keys);

	/** This method creates and opens the audio hardware output stream */
	virtual AudioStreamOut* openOutputStream(
					uint32_t devices,
					int *format=0,
					uint32_t *channels=0,
					uint32_t *sampleRate=0,
					status_t *status=0);
	/** This method creates and opens the audio hardware input stream */
	virtual AudioStreamIn* openInputStream(
					uint32_t devices,
					int *format,
					uint32_t *channels,
					uint32_t *sampleRate,
					status_t *status,
					AudioSystem::audio_in_acoustics acoustics);

	virtual void closeOutputStream(AudioStreamOut* out);
	virtual void closeInputStream(AudioStreamIn* in);
	virtual size_t getInputBufferSize(uint32_t sampleRate, int format, int channelCount);

protected:
	virtual status_t    dump(int fd, const Vector<String16>& args);

private:
	status_t doRouting(AudioStreamInJz47xx *input);
	status_t doAudioRouteOrMute(uint32_t device);
	status_t setMicMute_nosync(bool state);
	status_t checkMicMute();
	status_t dumpInternals(int fd, const Vector<String16>& args);
	status_t checkInputSampleRate(uint32_t sampleRate);
	uint32_t getInputSampleRate(uint32_t sampleRate);
	bool checkOutputStandby();

	class AudioStreamDevice	{
	public:
		enum open_type {
			UNKOWN		= -1,
			WRITEMODE	= 0,
			READMODE
		};

		AudioStreamDevice();
		virtual ~AudioStreamDevice();

		status_t openDevice(enum open_type flag);
		status_t closeDevice();
		status_t set(int format, int channelCount, uint32_t sampleRate);
		status_t setStandby(bool val);
		status_t setVolume(float volume){ return INVALID_OPERATION; }
		uint32_t sampleRate() const	{ return mSampleRate; }
		size_t	bufferSize() const	{ return mBufferSize; }
		int	channelCount() const	{ return mChannels; }
		int	format() const		{ return mFormat; }
		bool	checkStandby() const	{ return mStandby; }
		int	getDeviceHandle()	{ return mFd; }
		bool checkDevice() { return (mFd > 0); };
		int Write(const void* buffer, size_t bytes);

		status_t GetBuffer(AudioStreamOut::BufferInfo *binfo);
		status_t SetBuffer(AudioStreamOut::BufferInfo *binfo);
		uint32_t	mDevices;
		int		mFd;
		int		mChannels;
		int		mFormat;
		uint32_t	mSampleRate;
		size_t		mBufferSize;
		bool		mStandby;

	private:
		enum open_type	mOpenType;
		int map_fd;
		unsigned char  *mapdata_start;

	};

	class AudioStreamOutJz47xx : public AudioStreamOut, AudioStreamDevice {
	public:
		AudioStreamOutJz47xx();
		virtual ~AudioStreamOutJz47xx();

		status_t set(AudioHardware* hw,
			     uint32_t devices,
			     int *pFormat,
			     uint32_t *pChannels,
			     uint32_t *pRate);

		// must be 32-bit aligned - driver only seems to like 4800
		virtual size_t		bufferSize() const { /*return 4800;*/ return 4096 * 2; }
		virtual uint32_t	sampleRate()  const { return 44100; }
		virtual uint32_t	channels() const { return AudioSystem::CHANNEL_OUT_STEREO; }
		virtual int		format() const { return AudioSystem::PCM_16_BIT; }
#if 1
		virtual status_t    ioctrl(int cmd,void *arg){
			switch(cmd) {
			case AudioStreamOut_GetBuffer:{
				return AudioStreamDevice::GetBuffer((AudioStreamOut::BufferInfo *)arg);
			}
				break;
			case AudioStreamOut_SetBuffer:{
				return AudioStreamDevice::SetBuffer((AudioStreamOut::BufferInfo *)arg);
			}
			}
			return AudioStreamOut::ioctrl(cmd,arg);
		}
#endif
		virtual uint32_t	latency() const
		{
			//return (1000*AUDIO_HW_NUM_OUT_BUF*(bufferSize()/frameSize()))/sampleRate()+AUDIO_HW_OUT_LATENCY_MS;
			//mix write buffer
			return  (1000 * 1024 /  frameSize()) / sampleRate()+AUDIO_HW_OUT_LATENCY_MS;
		}

		/* 2010-03-02 Jason
		 * set single channel volume temprorarily
		 */
		virtual status_t	setVolume(float leftVolume, float rightVolume)
		{
			return AudioStreamDevice::setVolume(leftVolume);
		}

		virtual ssize_t		write(const void* buffer, size_t bytes);
		virtual status_t	standby()
		{
			printf("AudioStreamOutJz47xx ... standby()\n");
			return AudioStreamDevice::setStandby(true);
		}

		virtual status_t	dump(int fd, const Vector<String16>& args);
		bool		checkStandby() { return AudioStreamDevice::checkStandby(); }

		virtual status_t	setParameters(const String8& keyValuePairs);
		virtual String8		getParameters(const String8& keys);

		uint32_t getDevices() { return AudioStreamDevice::mDevices; }
		status_t getRenderPosition(uint32_t *p);

	private:
		AudioHardware*	mHardware;
		int		mStartCount;
		int		mRetryCount;
	};

	class AudioStreamInJz47xx : public AudioStreamIn, AudioStreamDevice {
	public:
		enum input_state {
			AUDIO_INPUT_CLOSED,
			AUDIO_INPUT_OPENED,
			AUDIO_INPUT_STARTED
		};

		AudioStreamInJz47xx();
		virtual		~AudioStreamInJz47xx();

		status_t set(AudioHardware* hw,
			     uint32_t devices,
			     int *pFormat,
			     uint32_t *pChannels,
			     uint32_t *pRate,
			     AudioSystem::audio_in_acoustics acoustic_flags);

		virtual size_t		bufferSize() const	{ return AudioStreamDevice::bufferSize(); }
		virtual uint32_t	sampleRate()  const	{ return AudioStreamDevice::sampleRate(); }
		// must be 32-bit aligned - driver only seems to like 4800
		virtual uint32_t	channels() const	{ return AudioStreamDevice::channelCount(); }
		virtual int		format() const		{ return AudioStreamDevice::format(); }
		virtual status_t	setGain(float gain)	{ return INVALID_OPERATION; }
		virtual ssize_t		read(void* buffer, ssize_t bytes);
		virtual status_t	dump(int fd, const Vector<String16>& args);
		virtual status_t	standby()
		{
			printf("AudioStreamInJz47xx ... standby()\n");
			return NO_ERROR;
		}

		bool			checkStandby()		{ return AudioStreamDevice::checkStandby(); }
		uint32_t		getDevices()		{ return AudioStreamDevice::mDevices; }

		virtual status_t	setParameters(const String8& keyValuePairs);
		virtual String8		getParameters(const String8& keys);
		virtual unsigned int  getInputFramesLost() const { return 0; }
		virtual status_t addAudioEffect(effect_handle_t effect) { return 0; };
		virtual status_t removeAudioEffect(effect_handle_t effect) { return 0; };

	private:
		AudioHardware*	mHardware;
		int		mState;
		int		mRetryCount;
	};

	class AudioCtrlDevice {
	public:
		enum device_t {
			SND_DEVICE_DEFAULT = 0,
			SND_DEVICE_CURRENT,
			SND_DEVICE_HANDSET,
			SND_DEVICE_HEADSET,
			SND_DEVICE_SPEAKER,
			SND_DEVICE_BT,
			SND_DEVICE_BT_EC_OFF,
			SND_DEVICE_HEADSET_AND_SPEAKER,
			SND_DEVICE_TTY_FULL,
			SND_DEVICE_CARKIT,
			SND_DEVICE_FM_SPEAKER,
			SND_DEVICE_FM_HEADSET,
			SND_DEVICE_NO_MIC_HEADSET
		};
		AudioCtrlDevice(enum device_t device);

		status_t setVoiceVolume(float f);
		status_t doAudioRouteOrMute(int mute);
	private:
		enum device_t mDevice;
		int mMethodVolume;
		int mCurrentVolume;
		static unsigned int mCurrentRoute;
	};

	static const uint32_t inputSamplingRates[];

	int		mCurSndDevice;
	bool		mInit;
	bool		mMicMute;
	bool		mBluetoothNrec;
	uint32_t	mBluetoothId;

	AudioStreamOutJz47xx*	mOutput;
	AudioStreamInJz47xx*	mInput;

	Mutex		mLock;

	DefaultKeyedVector<unsigned int,AudioCtrlDevice*> *mDevice;
};

// ----------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_AUDIO_HARDWARE_H
