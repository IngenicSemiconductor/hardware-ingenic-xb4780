#
# Audio Policy
#

ifneq ($(AUDIO_POLICY_USE_LEGACY), true)

include $(call all-subdir-makefiles)

endif #AUDIO_POLICY_USE_LEGACY
