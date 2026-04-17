cmake_minimum_required(VERSION 3.16)

add_executable(arxrender_test test_basic.cpp)
target_link_libraries(arxrender_test PRIVATE ArxRender)

# Дефайн для headless-режима (опционально)
target_compile_definitions(arxrender_test PRIVATE ARXRENDER_TEST_HEADLESS)

# Убедимся, что тест видит заголовки библиотеки
target_include_directories(arxrender_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Копируем в папку билда для удобства запуска
set_target_properties(arxrender_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)