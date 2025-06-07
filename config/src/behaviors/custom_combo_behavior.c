/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/modifiers_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/behavior_queue.h>
#include <zmk/keymap.h>

struct custom_combo_config {
    struct zmk_behavior_binding binding;
    uint32_t quick_release;
    uint32_t slow_release;
};

struct custom_combo_state {
    bool active;
    uint8_t pressed_count;
    bool first_key_released;
};

static int custom_combo_init(const struct device *dev) {
    return 0;
}

static int on_custom_combo_binding_pressed(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct custom_combo_state *state = dev->data;
    struct custom_combo_config *config = dev->config;

    if (!state->active) {
        // Activate both bindings on first key press
        zmk_behavior_keymap_binding_pressed(&config->binding, event);
        state->active = true;
    }
    state->pressed_count++;
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_custom_combo_binding_released(struct zmk_behavior_binding *binding,
                                          struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct custom_combo_state *state = dev->data;
    struct custom_combo_config *config = dev->config;

    state->pressed_count--;
    
    if (state->pressed_count == 0) {
        // Release both bindings when all keys are released
        zmk_behavior_keymap_binding_released(&config->binding, event);
        state->active = false;
        state->first_key_released = false;
    } else if (!state->first_key_released && config->quick_release) {
        // Release only the quick binding when first key is released
        zmk_behavior_keymap_binding_released(&config->binding, event);
        state->first_key_released = true;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api custom_combo_driver_api = {
    .binding_pressed = on_custom_combo_binding_pressed,
    .binding_released = on_custom_combo_binding_released,
};

#define CUSTOM_COMBO_INST(n) \
    static struct custom_combo_config custom_combo_config_##n = { \
        .binding = { \
            .behavior_dev = DT_INST_PROP(n, binding), \
            .param1 = DT_INST_PROP(n, param1), \
        }, \
        .quick_release = DT_INST_PROP(n, quick_release), \
        .slow_release = DT_INST_PROP(n, slow_release), \
    }; \
    static struct custom_combo_state custom_combo_state_##n = { \
        .active = false, \
        .pressed_count = 0, \
        .first_key_released = false, \
    }; \
    DEVICE_DT_INST_DEFINE(n, custom_combo_init, NULL, \
                         &custom_combo_state_##n, \
                         &custom_combo_config_##n, \
                         APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, \
                         &custom_combo_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CUSTOM_COMBO_INST) 