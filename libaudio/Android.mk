#
# Audio HAL
#

# if AUDIO_HW_USE_LEGACY is true, the code in ./legacy will be used,
# else the code in ./new/hw/primary will be used
# if AUDIO_POLICY_USE_LEGACY is true, use android default audio policy,
# else the code in ./new/policy will be used

#AUDIO_HW_USE_LEGACY := true
AUDIO_HW_USE_LEGACY := false
AUDIO_POLICY_USE_LEGACY := true
#AUDIO_POLICY_USE_LEGACY := false
ifneq ($(AUDIO_POLICY_USE_LEGACY), true)
AUDIO_POLICY_FOR_MID := true
endif

include $(call all-subdir-makefiles)
