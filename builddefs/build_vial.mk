# Vial forward-port for modern QMK
# Minimal feature set for initial bring-up.

QMK_SETTINGS ?= yes
CAPS_WORD_ENABLE ?= yes
TAP_DANCE_ENABLE ?= yes
ifeq ($(strip $(TAP_DANCE_ENABLE)), yes)
    OPT_DEFS += -DTAPPING_TERM_PER_KEY
endif

COMBO_ENABLE ?= yes
KEY_OVERRIDE_ENABLE ?= yes
LAYER_LOCK_ENABLE ?= yes
REPEAT_KEY_ENABLE ?= yes

SRC += $(QUANTUM_DIR)/vial.c

# The magic serial prefix is used by Vial to identify compatible devices.
OPT_DEFS += -DVIAL_ENABLE -DSERIAL_NUMBER=\"vial:f64c2b3c\"

ifeq ($(strip $(VIAL_INSECURE)), yes)
    OPT_DEFS += -DVIAL_INSECURE
endif

ifeq ($(strip $(VIALRGB_ENABLE)), yes)
    SRC += $(QUANTUM_DIR)/vialrgb.c
    OPT_DEFS += -DVIALRGB_ENABLE

    ifeq ($(strip $(VIALRGB_NO_DIRECT)), yes)
        OPT_DEFS += -DVIALRGB_NO_DIRECT
    endif
endif

ifeq ($(strip $(QMK_SETTINGS)), yes)
    AUTO_SHIFT_ENABLE := yes
    SRC += $(QUANTUM_DIR)/qmk_settings.c
    OPT_DEFS += -DQMK_SETTINGS \
        -DAUTO_SHIFT_NO_SETUP -DAUTO_SHIFT_REPEAT_PER_KEY -DAUTO_SHIFT_NO_AUTO_REPEAT_PER_KEY \
        -DPERMISSIVE_HOLD_PER_KEY -DHOLD_ON_OTHER_KEY_PRESS_PER_KEY -DQUICK_TAP_TERM_PER_KEY -DRETRO_TAPPING_PER_KEY \
        -DCOMBO_TERM_PER_COMBO -DCHORDAL_HOLD -DFLOW_TAP_TERM=321
endif

# Generate compressed Vial keyboard definition from vial.json.
$(QUANTUM_DIR)/vial.c: $(INTERMEDIATE_OUTPUT)/src/vial_generated_keyboard_definition.h

$(INTERMEDIATE_OUTPUT)/src/vial_generated_keyboard_definition.h: $(KEYMAP_PATH)/vial.json
	python3 util/vial_generate_definition.py $(KEYMAP_PATH)/vial.json $(INTERMEDIATE_OUTPUT)/src/vial_generated_keyboard_definition.h
