/**
 *
 * audio_hw.h
 *
 **/

#define DSP_DEVICE	"/dev/snd/dsp"
#define DSP1_DEVICE	"/dev/snd/dsp1"
#define MIXER_DEVICE	"/dev/snd/mixer"



/**
 * sound device
 **/
enum snd_device_t {
	SND_DEVICE_DEFAULT = 0,

	SND_DEVICE_CURRENT,
	SND_DEVICE_HANDSET,
	SND_DEVICE_HEADSET,	//fix for mid pretest
	SND_DEVICE_SPEAKER,	//fix for mid pretest
	SND_DEVICE_HEADPHONE,

	SND_DEVICE_BT,
	SND_DEVICE_BT_EC_OFF,
	SND_DEVICE_HEADSET_AND_SPEAKER,
	SND_DEVICE_TTY_FULL,
	SND_DEVICE_CARKIT,

	SND_DEVICE_FM_SPEAKER,
	SND_DEVICE_FM_HEADSET,
	SND_DEVICE_BUILDIN_MIC,
	SND_DEVICE_HEADSET_MIC,
	SND_DEVICE_HDMI = 15,	//fix for mid pretest

	SND_DEVICE_LOOP_TEST = 16,

	SND_DEVICE_CALL_START,							//call route start mark
	SND_DEVICE_CALL_HEADPHONE = SND_DEVICE_CALL_START,
	SND_DEVICE_CALL_HEADSET,
	SND_DEVICE_CALL_HANDSET,
	SND_DEVICE_CALL_SPEAKER,
	SND_DEVICE_HEADSET_RECORD_INCALL,
	SND_DEVICE_BUILDIN_RECORD_INCALL,
	SND_DEVICE_CALL_END = SND_DEVICE_BUILDIN_RECORD_INCALL,	//call route end mark

	SND_DEVICE_COUNT
};

struct snd_device_config {
	uint32_t device;
	uint32_t ear_mute;
	uint32_t mic_mute;
};

#define SNDCTL_EXT_SET_BUFFSIZE				_SIOR ('P', 100, int)
#define SNDCTL_EXT_SET_DEVICE               _SIOR ('P', 99, int)
#define SNDCTL_EXT_SET_STANDBY              _SIOR ('P', 98, int)
#define SNDCTL_EXT_START_BYPASS_TRANS       _SIOW ('P', 97, struct spipe_info)
#define SNDCTL_EXT_STOP_BYPASS_TRANS        _SIOW ('P', 96, struct spipe_info)
#define SNDCTL_EXT_DIRECT_GETINODE          _SIOR ('P', 95, struct direct_info)
#define SNDCTL_EXT_DIRECT_PUTINODE          _SIOW ('P', 94, struct direct_info)
#define SNDCTL_EXT_DIRECT_GETONODE          _SIOR ('P', 93, struct direct_info)
#define SNDCTL_EXT_DIRECT_PUTONODE          _SIOW ('P', 92, struct direct_info)
#define SNDCTL_EXT_STOP_DMA					_SIOW ('P', 91, int)
#define SNDCTL_EXT_SET_REPLAY_VOLUME        _SIOR ('P', 90, int)
#define SND_MUTE_UNMUTED        0
#define SND_MUTE_MUTED          1
/*###################################################*/

#define AUDIO_HW_OUT_DEF_SAMPLERATE	44100
#define AUDIO_HW_OUT_DEF_CHANNELS	AUDIO_CHANNEL_OUT_STEREO
#define AUDIO_HW_OUT_DEF_BUFFERSIZE	4096 //(fix)  64 * N audio_mixer needed
#define AUDIO_HW_OUT_DEF_FORMAT	    AUDIO_FORMAT_PCM_16_BIT

#define AUDIO_HW_IN_DEF_SAMPLERATE	8000
#define AUDIO_HW_IN_DEF_CHANNELS	AUDIO_CHANNEL_IN_MONO
#define AUDIO_HW_IN_DEF_BUFFERSIZE	4096 //(fix)
#define AUDIO_HW_IN_DEF_FORMAT		AUDIO_FORMAT_PCM_16_BIT

/*###################################################*/

struct steam_state {
	uint32_t sampleRate;
	size_t 	bufferSize;
	uint32_t channels;
	int format;
	bool standby;
	audio_source_t  inputSource;
	audio_devices_t devices;
};

struct stream_out_state {
	struct steam_state sState;
	float lVolume;
	float rVolume;
	int	renderPosition;
	bool isused;
};

struct steam_in_state {
	struct steam_state sState;
	float gain;
	uint32_t framesLost;
};

struct device_state {
	int adevMode;
	bool btMode;
	bool micMute;
	bool earMute;
};

struct device_prope {
	int dev_fd;
	int btdev_fd;
	int dev_map_fd;
	unsigned char *dev_mapdata_start;
};

struct xb47xx_stream_out {
	struct audio_stream_out stream;
	struct stream_out_state outState;
	struct device_prope *outDevPrope;
	struct xb47xx_audio_device *ladev;
};

struct xb47xx_stream_in {
	struct audio_stream_in stream;
	struct steam_in_state inState;
	struct device_prope *inDevPrope;
	struct xb47xx_audio_device *ladev;
};

struct xb47xx_audio_device {
	struct audio_hw_device device;
	struct device_state dState;
	struct device_prope oDevPrope;
	struct device_prope iDevPrope;
	struct xb47xx_stream_in	*istream;
	struct xb47xx_stream_out *ostream;
};

/*###################################################*/
