/**
 *
 * audio_policy.h
 *
 **/

#include <hardware/audio.h>

#define MAX_OUTPUTS 32
#define MAX_INPUTS 32

/* stream descriptor used for volume control */
struct stream_descriptor {
	audio_stream_type_t id;
	int indexMin;      // min volume index, inited as 0
	int indexMax;      // max volume index, inited as 1
	int indexCur;      // current volume index, inited as 1
	bool canBeMuted;   // true is the stream can be muted, inited as true
	int refCount;

	int strategy;
	int device;
};

struct output_descriptor {
	audio_io_handle_t id;
	audio_stream_type_t stream;
	uint32_t samplingRate;
	uint32_t format;
	uint32_t channels;
	audio_policy_output_flags_t flags;
	uint32_t latency;
	uint32_t device;
	uint32_t refCount;
	uint32_t stopTime;

	struct output_descriptor *next;
};

struct input_descriptor {
	audio_io_handle_t id;
	int      inputSource;
	uint32_t samplingRate;
	uint32_t format;
	uint32_t channels;
	audio_in_acoustics_t acoustics;
	uint32_t device;
	uint32_t refCount;

	struct input_descriptor *next;
};

#define MAX_A2DP_ADD_LEN 128
#define MAX_SCO_ADD_LEN  128
struct audio_policy_state {
	uint32_t availableOutputDevices;               // bit field of all available output devices
	uint32_t availableInputDevices;                // bit field of all available input devices
	char a2dpDeviceAddress[MAX_A2DP_ADD_LEN];                                         // A2DP device MAC address
	char scoDeviceAddress[MAX_SCO_ADD_LEN];                                          // SCO device MAC address
	int      phoneState;                                // current phone state
	uint32_t ringerMode;                           // current ringer mode
	bool     forceVolumeReeval;
	audio_io_handle_t hardwareOutput;              // hardware output handler
	audio_io_handle_t a2dpOutput;                  // A2DP output handler
	audio_io_handle_t duplicatedOutput;            // duplicated output handler: outputs to hardware and A2DP.
};

enum routing_strategy {
	STRATEGY_MEDIA,
	STRATEGY_PHONE,
	STRATEGY_SONIFICATION,
	STRATEGY_DTMF,
	STRATEGY_ENFORCED_AUDIBLE,
	NUM_STRATEGIES
};
