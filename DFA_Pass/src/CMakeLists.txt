# add_executable(SVFAnalysisPass svf-ex.cpp)
# target_link_libraries(SVFAnalysisPass ${llvm_libs} ${SVF_LIB})
# find_package(LLVM REQUIRED CONFIG)

# include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} 
# "${CMAKE_SOURCE_DIR}/SVF/svf/include"
# "${CMAKE_SOURCE_DIR}/SVF/svf-llvm/include"
# "${CMAKE_SOURCE_DIR}/build/SVF/include"
# "${CMAKE_SOURCE_DIR}/spdlog/include")

add_library(SVFAnalysisPass MODULE svf_analysis_pass.cpp)
target_link_libraries(SVFAnalysisPass PUBLIC ${SVF_LIB})