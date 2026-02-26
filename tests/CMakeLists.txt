# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: BSD-3-Clause

include(ECMAddTests)

ecm_qt_declare_logging_category(kdominate_ai_test_debug_srcs
    HEADER kdominate_debug.h
    IDENTIFIER KDOMINATE_LOG
    CATEGORY_NAME kdominate
    DESCRIPTION "KDominate game"
    EXPORT KDOMINATE
)

ecm_add_test(
    kdominate_ai_test.cpp
    ${CMAKE_SOURCE_DIR}/src/kdominate_board.cpp
    ${CMAKE_SOURCE_DIR}/src/kdominate_ai.cpp
    ${kdominate_ai_test_debug_srcs}
    TEST_NAME kdominate_ai_test
    LINK_LIBRARIES Qt6::Core Qt6::Test
)

target_include_directories(kdominate_ai_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(kdominate_ai_test PRIVATE
    MAPS_DIR="${CMAKE_SOURCE_DIR}/tests/maps"
)
