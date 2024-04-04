include(GNUInstallDirs)
find_package(Threads REQUIRED)
list(APPEND THREAD_LIB Threads::Threads)
list(APPEND ALLSPARK_DEFINITION -DAS_OMP=1 -DAS_TBB=2)
if(${RUNTIME_THREAD} STREQUAL "TBB")
    list(APPEND ALLSPARK_DEFINITION "-DAS_RUNTIME_THREAD=AS_TBB")
    set(THREAD_INCLUDE ${TBB_INCLUDE})
    list(APPEND THREAD_LIB ${TBB_IMPORTED_TARGETS})
elseif(${RUNTIME_THREAD} STREQUAL "OMP")
    list(APPEND ALLSPARK_DEFINITION "-DAS_RUNTIME_THREAD=AS_OMP")
    find_package(OpenMP)
    set(THREAD_INCLUDE ${OpenMP_CXX_INCLUDE_DIRS})
    list(APPEND THREAD_LIB OpenMP::OpenMP_CXX)
else()
    message(FATAL_ERROR "Unsupport CPU Threading runtime: ${RUNTIME_THREAD}")
endif()
message(STATUS "CPU Threading runtime: ${RUNTIME_THREAD}")
