idf_component_register(
    SRCS "withrottle_client.cpp"
    INCLUDE_DIRS "."
    REQUIRES withrottle wifi freertos log param
)
