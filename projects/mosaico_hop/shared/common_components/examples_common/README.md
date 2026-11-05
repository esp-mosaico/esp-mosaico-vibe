# Shared example Application component

`examples_common` owns the board-neutral Application/example glue shared by repository native examples. It is an application-side IDF component, not part of the Engine public API.

- `project.cmake`: pre-`project()` Application composition; maps the Board Manager selection to `RAYLIB_LITE_BOARD`, registers the selected package's adapter component, and loads package defaults/configuration.
- `native_module_main.c`: generic native example launcher.
- `include/raylib_lite_example_board.h`: abstract Board contract implemented by the selected Board component.
- `native_feedback.c` and `include/native_feedback.h`: generic haptic timing helper over that Board contract.
- `include/raylib_lite_game_module.h`: neutral Host/native module contract used only by examples and Host adapters.
- `include/raylib_lite_game_module_contract.h`: example/application bridge used by shared Game sources; historical Product ELF ABI mapping stays outside the Engine component and is not a currently supported build path.

Concrete Board packages remain under `examples/boards/<board-id>/`. Each Board root
is its application-side IDF adapter component.
Game-specific device-only implementation stays inside that Game's `main/`
component, for example `main/native/`; there is no Board×Game extension layer.

## Optional application services

`RAYLIB_LITE_NATIVE_SERVICE_COMPONENT` selects one service provider at configure
time. The shared launcher calls its boot, attach, first-present and detach methods;
ordinary examples select none. The [neutral service contract](../../../include/raylib_lite/raylib_lite_native_services.h)
contains only Engine types. Product providers own USB/update protocols and must
finish unregistering callbacks before the launcher releases Board resources.
