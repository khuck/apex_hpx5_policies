# Copyright (c) 2015 University of Oregon
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

add_library(apex_coalesce_policy SHARED apex_coalesce_policy.cpp)
target_include_directories(apex_coalesce_policy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(apex_coalesce_policy ${LIBS})
set_target_properties(apex_coalesce_policy PROPERTIES OUTPUT_NAME apex_coalesce_policy)

add_executable (policy_test policy_test.cpp)
target_link_libraries(policy_test ${LIBS})
set_target_properties(policy_test PROPERTIES LINK_FLAGS "${LINK_FLAGS} ${APEX_LDFLAGS}")

INSTALL(TARGETS apex_coalesce_policy policy_test
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

