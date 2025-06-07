/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/modifiers_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/keys.h>

struct custom_combo_config {
    uint8_t quick_release_binding;
    uint8_t hold_release_binding;
    struct zmk_behavior_binding quick_binding;
    struct zmk_behavior_binding hold_binding;
};

struct custom_combo_state {
    bool active;
    bool first_released;
    uint8_t pressed_count;
};

static int custom_combo_init(const struct device *dev) {
    return 0;
}

static int on_custom_combo_binding_pressed(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct custom_combo_config *config = (struct custom_combo_config *)dev->config;
    struct custom_combo_state *state = (struct custom_combo_state *)dev->data;

    if (state->pressed_count == 0) {
        // First key press - activate both bindings
        behavior_keymap_binding_pressed(&config->quick_binding, event);
        behavior_keymap_binding_pressed(&config->hold_binding, event);
        state->active = true;
    }
    state->pressed_count++;

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_custom_combo_binding_released(struct zmk_behavior_binding *binding,
                                          struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct custom_combo_config *config = (struct custom_combo_config *)dev->config;
    struct custom_combo_state *state = (struct custom_combo_state *)dev->data;

    state->pressed_count--;

    if (state->pressed_count == 0) {
        // All keys released
        if (state->active) {
            // Release the hold binding
            behavior_keymap_binding_released(&config->hold_binding, event);
            state->active = false;
            state->first_released = false;
        }
    } else if (state->pressed_count == 1 && !state->first_released) {
        // First key release - only release the quick binding
        behavior_keymap_binding_released(&config->quick_binding, event);
        state->first_released = true;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api custom_combo_driver_api = {
    .binding_pressed = on_custom_combo_binding_pressed,
    .binding_released = on_custom_combo_binding_released,
};

#define CUSTOM_COMBO_INST(n) \
    static struct custom_combo_config custom_combo_config_##n = { \
        .quick_release_binding = DT_INST_PROP(n, quick_release_binding), \
        .hold_release_binding = DT_INST_PROP(n, hold_release_binding), \
        .quick_binding = { \
            .behavior_dev = DT_INST_PROP_BY_IDX(n, bindings, DT_INST_PROP(n, quick_release_binding)), \
            .param1 = DT_INST_PROP_BY_IDX(n, binding_param1, DT_INST_PROP(n, quick_release_binding)), \
            .param2 = DT_INST_PROP_BY_IDX(n, binding_param2, DT_INST_PROP(n, quick_release_binding)), \
        }, \
        .hold_binding = { \
            .behavior_dev = DT_INST_PROP_BY_IDX(n, bindings, DT_INST_PROP(n, hold_release_binding)), \
            .param1 = DT_INST_PROP_BY_IDX(n, binding_param1, DT_INST_PROP(n, hold_release_binding)), \
            .param2 = DT_INST_PROP_BY_IDX(n, binding_param2, DT_INST_PROP(n, hold_release_binding)), \
        }, \
    }; \
    static struct custom_combo_state custom_combo_state_##n = { \
        .active = false, \
        .first_released = false, \
        .pressed_count = 0, \
    }; \
    DEVICE_DT_INST_DEFINE(n, custom_combo_init, NULL, \
                         &custom_combo_state_##n, \
                         &custom_combo_config_##n, \
                         APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, \
                         &custom_combo_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CUSTOM_COMBO_INST) 