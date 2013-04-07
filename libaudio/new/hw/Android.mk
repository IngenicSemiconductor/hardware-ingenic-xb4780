#
# Audio HW
#

ifneq ($(AUDIO_HW_USE_LEGACY), true)

include $(call all-subdir-makefiles)

endif #AUDIO_HW_USE_LEGACY
