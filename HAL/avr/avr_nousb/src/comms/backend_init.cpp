#include "comms/backend_init.hpp"

#include "comms/GamecubeBackend.hpp"
#include "comms/N64Backend.hpp"
#include "core/CommunicationBackend.hpp"
#include "core/mode_selection.hpp"
#include "core/pinout.hpp"
#include "util/config_util.hpp"

#include <config.pb.h>

size_t initialize_backends(
    CommunicationBackend **&backends,
    InputState &inputs,
    InputSource **input_sources,
    size_t input_source_count,
    Config &config,
    const Pinout &pinout,
    backend_config_selector_t get_backend_config,
    usb_backend_getter_t get_usb_backend_config,
    detect_console_t detect_console,
    secondary_backend_initializer_t init_secondary_backends,
    primary_backend_initializer_t init_primary_backend
) {
    // Make sure required function pointers are not null.
    if (get_backend_config == nullptr || init_primary_backend == nullptr ||
        detect_console == nullptr) {
        return 0;
    }

    CommunicationBackendConfig backend_config = CommunicationBackendConfig_init_zero;
    get_backend_config(backend_config, inputs, config);

    /* If no match found for button hold, use default backend config. */
    if (backend_config.backend_id == COMMS_BACKEND_UNSPECIFIED &&
        config.default_backend_config > 0) {
        backend_config = config.communication_backend_configs[config.default_backend_config - 1];
    }

    /* If no default backend config use GameCube backend. */
    if (backend_config.backend_id == COMMS_BACKEND_UNSPECIFIED) {
        backend_config = backend_config_from_id(
            COMMS_BACKEND_GAMECUBE,
            config.communication_backend_configs,
            config.communication_backend_configs_count
        );
    }

    CommunicationBackend *primary_backend = nullptr;

    init_primary_backend(
        primary_backend,
        backend_config.backend_id,
        inputs,
        input_sources,
        input_source_count,
        config,
        pinout
    );

    size_t backend_count = 1;
    if (init_secondary_backends != nullptr) {
        backend_count = init_secondary_backends(
            backends,
            primary_backend,
            backend_config.backend_id,
            inputs,
            input_sources,
            input_source_count,
            config,
            pinout
        );
    }

    if (backend_config.default_mode_config > 0) {
        GameModeConfig &mode_config =
            config.game_mode_configs[backend_config.default_mode_config - 1];
        for (size_t i = 0; i < backend_count; i++) {
            set_mode(backends[i], mode_config, config);
        }
    }

    return backend_count;
}

void init_primary_backend(
    CommunicationBackend *&primary_backend,
    CommunicationBackendId backend_id,
    InputState &inputs,
    InputSource **input_sources,
    size_t input_source_count,
    Config &config,
    const Pinout &pinout
) {
    switch (backend_id) {
        case COMMS_BACKEND_N64:
            delete primary_backend;
            primary_backend =
                new N64Backend(inputs, input_sources, input_source_count, 60, pinout.joybus_data);
            break;
        case COMMS_BACKEND_GAMECUBE:
        case COMMS_BACKEND_UNSPECIFIED: // Fall back to GameCube if invalid backend selected.
        default:
            delete primary_backend;
            primary_backend = new GamecubeBackend(
                inputs,
                input_sources,
                input_source_count,
                inputs.rt1 ? 0 : 125,
                pinout.joybus_data
            );
    }
}

size_t init_secondary_backends(
    CommunicationBackend **&backends,
    CommunicationBackend *&primary_backend,
    CommunicationBackendId backend_id,
    InputState &inputs,
    InputSource **input_sources,
    size_t input_source_count,
    Config &config,
    const Pinout &pinout
) {
    return 0;
}

// clang-format off

/* Default is to first check button holds for a matching comms backend config. */
backend_config_selector_t get_backend_config_default = [](
    CommunicationBackendConfig &backend_config,
    const InputState &inputs,
    Config &config
) {
    backend_config = backend_config_from_buttons(
        inputs,
        config.communication_backend_configs,
        config.communication_backend_configs_count
    );
};

usb_backend_getter_t get_usb_backend_config_default = nullptr;

// clang-format on

primary_backend_initializer_t init_primary_backend_default = &init_primary_backend;
secondary_backend_initializer_t init_secondary_backends_default = nullptr;
