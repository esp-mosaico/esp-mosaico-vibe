# ESP-Mosaico application rules

These rules supplement the [repository rules](../AGENTS.md) for applications in
`projects/`. Apply them to new or changed application-owned ESP-IDF modules;
preserve existing public API compatibility and dependency-owned code conventions.

## C module design

- Implement application modules in C, not C++. Use opaque handles for stateful
  modules: `typedef struct xxx_t *xxx_handle_t;`. Stateless utilities need no object.
- Define instance structs in `.c` files, or private implementation headers when
  shared across source files. Public headers expose only handles, configuration,
  events, callbacks and public APIs.
- Use ESP-IDF-style names such as `xxx_create/delete/start/stop/read/write/set/get`,
  following the component prefix. Instance methods take `xxx_handle_t handle` first;
  create takes `const xxx_config_t *config` and `xxx_handle_t *ret_handle`.
  Prefer `esp_err_t` for fallible public APIs.
- Prefer instance-owned mutable runtime state. Pair object resource acquisition
  and release in `create/delete`; `start/stop` may manage running-state resources.
  Roll back failures and stop tasks, timers and callbacks from accessing resources
  before freeing them. Delete must release all resources owned by the instance.
- Protect shared mutable state using suitable synchronization, such as mutexes,
  queues or atomics, or a documented single-task owner; account for ISR access.
- Use `xxx_register_cb()` for callback registration where applicable, with the
  handle, event and user context. Document callback execution context, event-data
  and user-context lifetimes, and unregister/delete behavior.
- Introduce `xxx_ops_t` function tables and first-member base structs only when
  multiple implementations need a common interface; keep them in private headers.

## Stack and allocation

- Avoid local arrays or aggregate objects larger than 128 bytes on task stacks
  unless justified by the task's stack budget, including the call chain.
  Prefer instance-owned buffers; do not move mutable buffers to hidden globals.
- Prefer reusable instance buffers, memory pools or ring buffers in high-frequency
  or latency-sensitive paths. Avoid repeated heap allocation in those paths.
