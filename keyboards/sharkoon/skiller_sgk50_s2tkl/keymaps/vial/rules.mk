VIA_ENABLE = yes
VIAL_ENABLE = yes
VIALRGB_ENABLE = yes

# Nur während des ersten Vial-Bring-ups.
#VIAL_INSECURE = yes

# Register VialRGB Direct through modern QMK custom-user RGB Matrix effects.
RGB_MATRIX_CUSTOM_USER = yes

LTO_ENABLE = yes

# Build a direct LED_FLAG_NONE lookup from keyboard.json. The generated include
# lives only in .build; keyboard.json remains the single source of truth.
ifeq ($(strip $(VIALRGB_ENABLE)), yes)
SHARKOON_DISABLED_LEDS_INC := $(INTERMEDIATE_OUTPUT)/src/sharkoon_disabled_leds.inc
SHARKOON_DISABLED_LEDS_GENERATOR := $(KEYMAP_PATH)/generate_disabled_leds.py
SHARKOON_KEYBOARD_JSON := $(KEYBOARD_PATH_1)/keyboard.json

$(SHARKOON_DISABLED_LEDS_INC): $(SHARKOON_KEYBOARD_JSON) $(SHARKOON_DISABLED_LEDS_GENERATOR)
	@$(SILENT) || printf "$(MSG_GENERATING) $@" | $(AWK_CMD)
	$(eval CMD=python3 $(SHARKOON_DISABLED_LEDS_GENERATOR) $(SHARKOON_KEYBOARD_JSON) $@)
	@$(BUILD_CMD)

generated-files: $(SHARKOON_DISABLED_LEDS_INC)
endif
