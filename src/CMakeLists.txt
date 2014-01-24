add_definitions(-Wall -Wextra -Wno-unused-parameter -std=gnu99)
add_definitions(-g -O2)

aux_source_directory(asm asmSources)
aux_source_directory(vm vmSources)
aux_source_directory(common commonSources)

include_directories(.)

add_library(common ${commonSources})

add_executable(asm ${asmSources})
target_link_libraries(asm common)
add_executable(vm ${vmSources})
target_link_libraries(vm common)
