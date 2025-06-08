/*
 * Custom ZMK behavior: dual-release-combo
 *
 *    When the combo is triggered it presses BOTH bindings (slow + quick).
 *    When the FIRST key in the combo is released, the quick binding is
 *    released, but the slow binding remains active.  Once ALL keys of the
 *    combo have been released, the slow binding is also released.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

struct drc_config {
    struct zmk_behavior_binding slow_binding;
    struct zmk_behavior_binding quick_binding;
    const int32_t *key_positions; /* unused now */
};

struct drc_state {
    bool active;
    bool quick_released;
};

static int drc_binding_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct drc_state *state = dev->data;
    const struct drc_config *cfg = dev->config;

    if (state->active) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* Press both bindings */
    zmk_behavior_keymap_binding_pressed(&cfg->slow_binding, event);
    zmk_behavior_keymap_binding_pressed(&cfg->quick_binding, event);

    state->active = true;
    state->quick_released = false;

    return ZMK_BEHAVIOR_OPAQUE;
}

static int drc_binding_released(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    /* We purposely do nothing here – releases are handled by the listener */
    return ZMK_BEHAVIOR_OPAQUE;
}

/* ---------------- Event listener ----------------- */

static int drc_event_listener(const struct zmk_event_header *eh) {
    if (!is_zmk_keycode_state_changed(eh)) {
        return 0;
    }

    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);

    /* Iterate over all instances */
#define HANDLE_INSTANCE(idx, _dev)                                                         \
    {                                                                                      \
        const struct device *dev = DEVICE_DT_INST_GET(idx);                               \
        struct drc_state *state = dev->data;                                              \
        const struct drc_config *cfg = dev->config;                                       \
        if (!state->active) {                                                             \
            ;                                                                             \
        } else if (!pos_belongs_to_combo(cfg, ev->key_position)) {                        \
            ;                                                                             \
        } else {                                                                          \
            if (ev->state == false) { /* key released within active combo */             \
                if (!state->quick_released) {                                             \
                    /* Release quick binding now */                                       \
                    struct zmk_behavior_binding_event dummy = {                           \
                        .timestamp = ev->timestamp,                                       \
                    };                                                                    \
                    zmk_behavior_keymap_binding_released(&cfg->quick_binding, dummy);      \
                    state->quick_released = true;                                         \
                    LOG_INF("DualRel: release quick");                                          \
                }                                                                         \
                else {                                                                    \
                    /* second (final) release -> release slow binding */                  \
                    struct zmk_behavior_binding_event dummy = {                           \
                        .timestamp = ev->timestamp,                                       \
                    };                                                                    \
                    zmk_behavior_keymap_binding_released(&cfg->slow_binding, dummy);       \
                    state->active = false;                                                \
                    LOG_INF("DualRel: release slow");                                   \
                }                                                                         \
            }                                                                             \
        }                                                                                  \
    }

    DT_INST_FOREACH_STATUS_OKAY(HANDLE_INSTANCE);

    return 0;
}

ZMK_LISTENER(dual_release_combo, drc_event_listener);
ZMK_SUBSCRIPTION(dual_release_combo, zmk_keycode_state_changed);

/* -------------- driver registration -------------- */

static const struct behavior_driver_api drc_driver_api = {
    .binding_pressed = drc_binding_pressed,
    .binding_released = drc_binding_released,
};

#define DRC_KEYPOS(node) DT_PROP(node, key_positions)

#define DUAL_RELEASE_COMBO_INST(n)                                                      \
    static struct drc_state drc_state_##n;                                             \
    static const int32_t keypos_##n[] = DT_INST_PROP(n, key_positions);                \
    static const struct drc_config drc_cfg_##n = {                                     \
        .slow_binding = {                                                              \
            .behavior_dev = DT_INST_PROP_BY_IDX(n, bindings, 0, phandle), \
            .param1 = DT_INST_PROP_BY_IDX(n, bindings, 0, cells), \
        },                                                                             \
        .quick_binding = {                                                             \
            .behavior_dev = DT_INST_PROP_BY_IDX(n, bindings, 1, phandle), \
            .param1 = DT_INST_PROP_BY_IDX(n, bindings, 1, cells), \
        },                                                                             \
        .key_positions = keypos_##n, /* kept for future use */                         \
    };                                                                                 \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &drc_state_##n, &drc_cfg_##n, APPLICATION,    \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &drc_driver_api); 