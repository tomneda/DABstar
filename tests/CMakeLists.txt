add_executable(fast_math_test fast_log_test.cpp)
target_include_directories(fast_math_test PRIVATE ${PROJECT_SOURCE_DIR})
add_test(NAME fast_math_test COMMAND fast_math_test)

add_executable(map_projection_test map_projection_test.cpp)
target_include_directories(map_projection_test PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/common)
add_test(NAME map_projection_test COMMAND map_projection_test)

add_executable(spi_decoder_test spi_decoder_test.cpp ${PROJECT_SOURCE_DIR}/src/base/backend/data/spi/spi_decoder.cpp)
target_include_directories(spi_decoder_test PRIVATE ${PROJECT_SOURCE_DIR}/src/base/backend/data/spi ${PROJECT_SOURCE_DIR}/src/common)
target_link_libraries(spi_decoder_test PRIVATE Qt6::Core)
add_test(NAME spi_decoder_test COMMAND spi_decoder_test)

add_executable(setting_reset_test setting_reset_test.cpp ${PROJECT_SOURCE_DIR}/src/common/setting_helper.cpp)
target_include_directories(setting_reset_test PRIVATE ${PROJECT_SOURCE_DIR}/src/common)
target_link_libraries(setting_reset_test PRIVATE Qt6::Widgets)
add_test(NAME setting_reset_test COMMAND setting_reset_test)
