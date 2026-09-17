add_executable(LedControlActivityTest
  LedControlActivityTest.cpp
  HomeMenuMappingTest.cpp
  HomeMenuGeometryTest.cpp
  TestServices.cpp
  ${REPO_ROOT}/src/activities/util/LedControlActivity.cpp
  ${REPO_ROOT}/src/activities/UiListActivity.cpp
  ${REPO_ROOT}/src/activities/home/HomeActivity.cpp
  ${REPO_ROOT}/src/util/ButtonNavigator.cpp
  ${REPO_ROOT}/freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp
)

target_include_directories(LedControlActivityTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/hal
)
target_include_directories(LedControlActivityTest SYSTEM PRIVATE
  ${REPO_ROOT}/freeink-sdk/libs/ui/FreeInkUI/include
)
target_link_libraries(LedControlActivityTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(LedControlActivityTest)
