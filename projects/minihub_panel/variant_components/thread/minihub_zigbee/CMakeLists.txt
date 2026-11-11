# Thread firmware compatibility target. Shared MiniHub modules reference a
# small Zigbee status/control surface even though the Thread image must not
# link the Zigbee stack.
idf_component_register(
    SRCS "minihub_zigbee_stub.c"
    INCLUDE_DIRS "include"
    REQUIRES espressif__cjson
)
