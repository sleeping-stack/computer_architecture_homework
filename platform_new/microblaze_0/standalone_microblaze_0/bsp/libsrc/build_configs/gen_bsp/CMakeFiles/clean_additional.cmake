# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "D:\\computer_architecture_homework\\MicroBlaze_AC850\\MicroBlaze_AC850.sw\\platform_new\\microblaze_0\\standalone_microblaze_0\\bsp\\include\\sleep.h"
  "D:\\computer_architecture_homework\\MicroBlaze_AC850\\MicroBlaze_AC850.sw\\platform_new\\microblaze_0\\standalone_microblaze_0\\bsp\\include\\xiltimer.h"
  "D:\\computer_architecture_homework\\MicroBlaze_AC850\\MicroBlaze_AC850.sw\\platform_new\\microblaze_0\\standalone_microblaze_0\\bsp\\include\\xtimer_config.h"
  "D:\\computer_architecture_homework\\MicroBlaze_AC850\\MicroBlaze_AC850.sw\\platform_new\\microblaze_0\\standalone_microblaze_0\\bsp\\lib\\libxiltimer.a"
  )
endif()
