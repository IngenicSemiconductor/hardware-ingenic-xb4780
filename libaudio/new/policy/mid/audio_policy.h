/**
 *
 * audio_policy.h
 *
 **/

#include <hardware/audio.h>
#include <system/audio_policy.h>

#define AUDIO_ATTACHEDOUTPUTDEVICES AUDIO_DEVICE_OUT_SPEAKER

#define MAX_DEVICE_ADDRESS_LEN 20
// Attenuation applied to STRATEGY_SONIFICATION streams when a headset is connected: 6dB
#define SONIFICATION_HEADSET_VOLUME_FACTOR 0.5
// Min volume for STRATEGY_SONIFICATION streams when limited by music volume: -36dB
#define SONIFICATION_HEADSET_VOLUME_MIN  0.016
// Time in milliseconds during which we consider that music is still active after a music
// track was stopped - see computeVolume()
#define SONIFICATION_HEADSET_MUSIC_DELAY  5000
// Time in milliseconds after media stopped playing during which we consider that the
// sonification should be as unobtrusive as during the time media was playing.
#define SONIFICATION_RESPECTFUL_AFTER_MUSIC_DELAY 5000
// Time in milliseconds during witch some streams are muted while the audio path
// is switched
#define MUTE_TIME_MS 2000
#define NUM_TEST_OUTPUTS 5

#define NUM_VOL_CURVE_KNEES 2
enum {
    OK                = 0,    // Everything's swell.
    NO_ERROR          = 0,    // No errors.
    NO_MEMORY           = -ENOMEM,
    INVALID_OPERATION   = -ENOSYS,
    BAD_VALUE           = -EINVAL,
    PERMISSION_DENIED   = -EPERM,
    NO_INIT             = -ENODEV,
    ALREADY_EXISTS      = -EEXIST,
};

enum routing_strategy {
	STRATEGY_MEDIA,
	STRATEGY_PHONE,
	STRATEGY_SONIFICATION,
	STRATEGY_SONIFICATION_RESPECTFUL,
	STRATEGY_DTMF,
	STRATEGY_ENFORCED_AUDIBLE,
	NUM_STRATEGIES
};



/* stream descriptor used for volume control */
struct out_stream_descriptor {
	audio_stream_type_t id;
	enum routing_strategy strategy;
	int indexMin;      // min volume index, inited as 0
	int indexMax;      // max volume index, inited as 1
	int indexCur;      // current volume index, inited as 1
	bool canBeMuted;   // true is the stream can be muted, inited as true
	bool isMuted;      // true if the stream is muted
	int  refCount;     // refcount
};

/* request to open a direct output with get_output() (by opposition to
 * sharing an output with other AudioTracks)
 */

struct output_descriptor {
	audio_io_handle_t id;
	audio_module_handle_t handle;
	struct out_stream_descriptor *oStream[AUDIO_STREAM_CNT];   // each point point one stream
	uint32_t samplingRate;
	uint32_t format;
	uint32_t channels;
	audio_output_flags_t flags;
	uint32_t device;
	uint32_t latency;
	uint32_t refCount;
	uint32_t stopTime[AUDIO_STREAM_CNT];                      //each stream stop time of this output
	struct output_descriptor *next;
	struct output_descriptor *output1;
	struct output_descriptor *output2;
	uint32_t support_device;
};

struct input_descriptor {
	audio_io_handle_t id;
	audio_module_handle_t handle;
	int      inputSource;              // each intput has one inputSource
	uint32_t samplingRate;
	uint32_t format;
	uint32_t channels;
	audio_in_acoustics_t acoustics;
	uint32_t device;
	uint32_t refCount;
	struct input_descriptor *next;
};

#define MAX_A2DP_ADD_LEN 128
#define MAX_SCO_ADD_LEN	 20

struct module_profile {
	audio_output_flags_t	flags;
	audio_devices_t		devices;
	audio_channel_mask_t	in_channels;
	audio_channel_mask_t	out_channels;
	audio_format_t		format;
	uint32_t		*in_samplerates;
	uint32_t		*out_samplerates;
	struct module_profile	*next;
};

struct hardware_module {
	audio_module_handle_t	handle;
	struct module_profile	*profile;
	struct hardware_module	*next;
};

struct audio_policy_property {
	struct out_stream_descriptor ap_streams[AUDIO_STREAM_CNT];
	struct output_descriptor     *ap_outputs;
	struct input_descriptor      *ap_inputs;
	struct output_descriptor     *primaryOutput;          // hardware output handler
	struct output_descriptor     *a2dpOutput;              // A2DP output handler
	struct output_descriptor	 *usbOutput;			   // usb output handler
	struct output_descriptor     *duplicatedOutput;        // duplicated output handler: outputs to hardware and A2DP.
	audio_policy_forced_cfg_t    forceUseConf[AUDIO_POLICY_FORCE_USE_CNT];
	uint32_t availableOutputDevices;					// bit field of all available output devices
	uint32_t availableInputDevices;						// bit field of all available input devices
	char     a2dpDeviceAddress[MAX_A2DP_ADD_LEN];		// A2DP device MAC address
	char     scoDeviceAddress[MAX_SCO_ADD_LEN];			// SCO device MAC address
	uint32_t ringerMode;								// current ringer mode
	audio_mode_t phoneState;							// current phone state
	audio_devices_t strategy_cache[NUM_STRATEGIES];		// device for strategy cache
	float	lastVoiceVolume;								// last volume for call?
	bool    forceVolumeReeval;
	bool	a2dp_is_suspend;
	struct hardware_module  *hwmodule;
};

struct xb47xx_ap_module {
    struct audio_policy_module module;
};

struct xb47xx_ap_device {
    struct audio_policy_device device;
};

struct xb47xx_audio_policy {
    struct audio_policy policy;
    struct audio_policy_service_ops *aps_ops;
	struct audio_policy_property ap_prop;
    void *service;
};

#define MAX_EFFECTS_MEMORY		512
#define MAX_EFFECTS_CPU_LOAD	1000

struct effect_descriptor {
	struct effect_descriptor_s desc;
	audio_io_handle_t io;
	enum routing_strategy strategy;
	int session;
	int id;
	bool enable;
	struct effect_descriptor *next;
};
