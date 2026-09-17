foreach(target HalLedTest HalLedUnsupportedTest HalLedDisabledTest)
  if(target STREQUAL "HalLedTest")
    set(test_source HalLedTest.cpp)
  else()
    set(test_source HalLedUnsupportedTest.cpp)
  endif()
  add_executable(${target} ${test_source} ${REPO_ROOT}/lib/hal/HalLed.cpp)
  target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${REPO_ROOT}/lib/hal
  )
  target_link_libraries(${target} PRIVATE crosspoint_test_common GTest::gtest_main)
endforeach()

target_compile_definitions(HalLedTest PRIVATE FREEINK_DEVICE_PAPERMONO=1 FREEINK_CAP_LED=1)
target_compile_definitions(HalLedUnsupportedTest PRIVATE FREEINK_DEVICE_PAPERMONO=0 FREEINK_CAP_LED=1 ESP32S3=1)
target_compile_definitions(HalLedDisabledTest PRIVATE FREEINK_DEVICE_PAPERMONO=1 FREEINK_CAP_LED=0)
gtest_discover_tests(HalLedTest)
gtest_discover_tests(HalLedUnsupportedTest TEST_PREFIX "OtherBoard.")
gtest_discover_tests(HalLedDisabledTest TEST_PREFIX "CapabilityDisabled.")
