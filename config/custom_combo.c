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
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/keys.h>

struct custom_combo_config {
    uint32_t timeout_ms;
    uint32_t require_prior_idle_ms;
    uint8_t layers;
    uint8_t key_positions_len;
    uint8_t key_positions[];
};

struct custom_combo_state {
    bool active;
    bool first_binding_released;
    int64_t timestamp;
    uint8_t pressed_count;
    uint8_t key_positions_pressed[10]; // Adjust size as needed
};

static int custom_combo_init(const struct device *dev) {
    return 0;
}

static int on_custom_combo_binding_pressed(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct custom_combo_state *state = dev->data;
    const struct custom_combo_config *config = dev->config;

    if (state->active) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    // Check if we're in the right layer
    if (!(config->layers & (1 << zmk_keymap_highest_layer_active()))) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    // Check if we've been idle long enough
    if (config->require_prior_idle_ms > 0) {
        int64_t now = k_uptime_get();
        if (now - state->timestamp < config->require_prior_idle_ms) {
            return ZMK_BEHAVIOR_OPAQUE;
        }
    }

    // Check if this is one of our combo keys
    bool is_combo_key = false;
    for (int i = 0; i < config->key_positions_len; i++) {
        if (event.position == config->key_positions[i]) {
            is_combo_key = true;
            break;
        }
    }

    if (!is_combo_key) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    // Record this key press
    state->key_positions_pressed[state->pressed_count++] = event.position;
    state->timestamp = k_uptime_get();

    // If this is the first key of the combo
    if (state->pressed_count == 1) {
        // Activate both bindings
        struct zmk_behavior_binding first_binding = {
            .behavior_dev = binding->param1,
            .param1 = binding->param2,
        };
        struct zmk_behavior_binding second_binding = {
            .behavior_dev = binding->param3,
            .param1 = binding->param4,
        };

        behavior_keymap_binding_pressed(&first_binding, event);
        behavior_keymap_binding_pressed(&second_binding, event);
        state->active = true;
        state->first_binding_released = false;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_custom_combo_binding_released(struct zmk_behavior_binding *binding,
                                          struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct custom_combo_state *state = dev->data;
    const struct custom_combo_config *config = dev->config;

    if (!state->active) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    // Find and remove this key from our pressed keys
    for (int i = 0; i < state->pressed_count; i++) {
        if (state->key_positions_pressed[i] == event.position) {
            // Remove this key by shifting the rest
            for (int j = i; j < state->pressed_count - 1; j++) {
                state->key_positions_pressed[j] = state->key_positions_pressed[j + 1];
            }
            state->pressed_count--;
            break;
        }
    }

    // If this was the last key, release both bindings
    if (state->pressed_count == 0) {
        struct zmk_behavior_binding first_binding = {
            .behavior_dev = binding->param1,
            .param1 = binding->param2,
        };
        struct zmk_behavior_binding second_binding = {
            .behavior_dev = binding->param3,
            .param1 = binding->param4,
        };

        if (!state->first_binding_released) {
            behavior_keymap_binding_released(&first_binding, event);
            state->first_binding_released = true;
        }
        behavior_keymap_binding_released(&second_binding, event);
        state->active = false;
    }
    // If this wasn't the last key and we haven't released the first binding yet
    else if (!state->first_binding_released) {
        struct zmk_behavior_binding first_binding = {
            .behavior_dev = binding->param1,
            .param1 = binding->param2,
        };
        behavior_keymap_binding_released(&first_binding, event);
        state->first_binding_released = true;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api custom_combo_driver_api = {
    .binding_pressed = on_custom_combo_binding_pressed,
    .binding_released = on_custom_combo_binding_released,
};

#define CUSTOM_COMBO_INST(n) \
    static struct custom_combo_state custom_combo_state_##n = { \
        .active = false, \
        .first_binding_released = false, \
        .timestamp = 0, \
        .pressed_count = 0, \
    }; \
    static const struct custom_combo_config custom_combo_config_##n = { \
        .timeout_ms = DT_PROP(n, timeout_ms), \
        .require_prior_idle_ms = DT_PROP(n, require_prior_idle_ms), \
        .layers = DT_PROP(n, layers), \
        .key_positions_len = DT_PROP_LEN(n, key_positions), \
        .key_positions = DT_PROP(n, key_positions), \
    }; \
    DEVICE_DT_INST_DEFINE(n, custom_combo_init, NULL, \
                          &custom_combo_state_##n, \
                          &custom_combo_config_##n, \
                          APPLICATION, \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, \
                          &custom_combo_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CUSTOM_COMBO_INST) 