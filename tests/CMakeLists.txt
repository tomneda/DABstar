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

add_executable(pos_and_size_test pos_and_size_test.cpp ${PROJECT_SOURCE_DIR}/src/common/setting_helper.cpp)
target_include_directories(pos_and_size_test PRIVATE ${PROJECT_SOURCE_DIR}/src/common)
target_link_libraries(pos_and_size_test PRIVATE Qt6::Widgets)
add_test(NAME pos_and_size_test COMMAND pos_and_size_test)

add_executable(list_database_test list_database_test.cpp
        ${PROJECT_SOURCE_DIR}/src/base/database/dab_database.cpp
        ${PROJECT_SOURCE_DIR}/src/base/service_list/service_db.cpp
        ${PROJECT_SOURCE_DIR}/src/base/ensemble_list/ensemble_list_db.cpp)
target_include_directories(list_database_test PRIVATE ${PROJECT_SOURCE_DIR}/src/base/database ${PROJECT_SOURCE_DIR}/src/base/service_list
        ${PROJECT_SOURCE_DIR}/src/base/ensemble_list ${PROJECT_SOURCE_DIR}/src/base/main ${PROJECT_SOURCE_DIR}/src/common)
target_link_libraries(list_database_test PRIVATE Qt6::Widgets Qt6::Sql)
add_test(NAME list_database_test COMMAND list_database_test)
set_tests_properties(list_database_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
