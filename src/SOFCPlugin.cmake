set(SOFC_PLUGIN_NAME sofc_inv)

set(SOFC_PLUGIN_SOURCES
    src/SOFCPlugin.cpp
)

set(SOFC_PLUGIN_HEADERS
    src/SOFCPlugin.h
    src/ViewOptions.h
    src/ViewConv.h
    src/TabView.h
    src/WindowPlugin.h
)

addPlugin(
    ${SOFC_PLUGIN_NAME}
    "${SOFC_PLUGIN_SOURCES}"
    "${SOFC_PLUGIN_HEADERS}"
    "${NATID_SDK_ROOT}"
)