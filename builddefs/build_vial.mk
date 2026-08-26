# Vial forward-port for modern QMK
# Minimal feature set for initial bring-up.

QMK_SETTINGS ?= no
TAP_DANCE_ENABLE ?= yes
ifeq ($(strip $(TAP_DANCE_ENABLE)), yes)
    OPT_DEFS += -DTAPPING_TERM_PER_KEY
endif

COMBO_ENABLE ?= yes
KEY_OVERRIDE_ENABLE ?= yes
LAYER_LOCK_ENABLE ?= no
REPEAT_KEY_ENABLE ?= no

SRC += $(QUANTUM_DIR)/vial.c

# The magic serial prefix is used by Vial to identify compatible devices.
OPT_DEFS += -DVIAL_ENABLE -DSERIAL_NUMBER=\"vial:f64c2b3c\"

ifeq ($(strip $(VIAL_INSECURE)), yes)
    OPT_DEFS += -DVIAL_INSECURE
endif

# Generate compressed Vial keyboard definition from vial.json.
$(QUANTUM_DIR)/vial.c: $(INTERMEDIATE_OUTPUT)/src/vial_generated_keyboard_definition.h

$(INTERMEDIATE_OUTPUT)/src/vial_generated_keyboard_definition.h: $(KEYMAP_PATH)/vial.json
	python3 util/vial_generate_definition.py $(KEYMAP_PATH)/vial.json $(INTERMEDIATE_OUTPUT)/src/vial_generated_keyboard_definition.h
