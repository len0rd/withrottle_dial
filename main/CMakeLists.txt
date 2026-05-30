idf_component_register(
    SRCS
        "main.cpp"
        "ui_events.cpp"
        "train_fn_screen.cpp"
        "train_main_control_screen.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        hw_config
        withrottle_client
        user_encoder_bsp
        ui
    )

target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,-u,onStopClicked")

set_source_files_properties(
    ${LV_DEMOS_SOURCES}
    PROPERTIES COMPILE_OPTIONS
    -DLV_LVGL_H_INCLUDE_SIMPLE)
