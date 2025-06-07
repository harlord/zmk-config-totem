/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/modifiers_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct custom_combo_config {
    struct zmk_behavior_binding bindings[2];
    uint8_t key_positions_len;
    int32_t key_positions[];
};

struct custom_combo_state {
    bool active;
    bool first_released;
    uint8_t pressed_count;
    struct zmk_behavior_binding *first_binding;
    struct zmk_behavior_binding *second_binding;
};

static int custom_combo_init(const struct device *dev) {
    return 0;
}

static int on_custom_combo_binding_pressed(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    const struct custom_combo_config *cfg = dev->config;
    struct custom_combo_state *state = dev->data;

    if (state->pressed_count == 0) {
        // First key press, activate both bindings
        state->active = true;
        state->first_released = false;
        state->first_binding = &cfg->bindings[0];
        state->second_binding = &cfg->bindings[1];

        // Activate both bindings
        behavior_keymap_binding_pressed(state->first_binding, event);
        behavior_keymap_binding_pressed(state->second_binding, event);
    }
    state->pressed_count++;

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_custom_combo_binding_released(struct zmk_behavior_binding *binding,
                                          struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct custom_combo_state *state = dev->data;

    state->pressed_count--;

    if (state->pressed_count == 0) {
        // All keys released, deactivate both bindings
        if (state->active) {
            behavior_keymap_binding_released(state->first_binding, event);
            behavior_keymap_binding_released(state->second_binding, event);
            state->active = false;
            state->first_released = false;
        }
    } else if (!state->first_released) {
        // First key released, only release the first binding
        behavior_keymap_binding_released(state->first_binding, event);
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
        .bindings = { \
            DT_INST_PROP_BY_IDX(n, bindings, 0), \
            DT_INST_PROP_BY_IDX(n, bindings, 1), \
        }, \
        .key_positions_len = DT_INST_PROP_LEN(n, key_positions), \
        .key_positions = DT_INST_PROP(n, key_positions), \
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

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */ 