# main: device composition root

Initializes platform services, starts Recovery, loads the UI bundle, prepares the
display, starts GSP, then creates the MiniHub client and panel controller. It
contains no HTTP, input, or drawing rules and exports no headers.

Private dependencies: `app_display`, `app_hub`, `app_panel`, Iris, Recovery,
bundle loading, screen mirroring, NVS and logging. Shared product/platform
components retain their names. Central dependency pins/overrides live in
`idf_component.yml`.

ESP-IDF invokes `app_main()` once in task context; it takes no parameters and
returns after startup. Its private composition context retains only owned opaque
handles; UI/controller lifetime extends until reboot. Bundle or display
initialization failure leaves Recovery reachable;
other unrecoverable initialization errors use ESP-IDF's error-check policy.
Build/run the project through the workspace launcher described in the root README.
