# Look for ptxas from CUDA Toolkit if available
find_program(PTXAS_EXECUTABLE
    NAMES ptxas
    PATHS
        /usr/local/cuda/bin
        /usr/local/cuda-12/bin
        /usr/local/cuda-11/bin
        /opt/cuda/bin
        ENV CUDA_PATH
        PATH_SUFFIXES bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PTXAS
    DEFAULT_MSG
    PTXAS_EXECUTABLE
)

if(PTXAS_FOUND)
    message(STATUS "Found ptxas: ${PTXAS_EXECUTABLE}")
else()
    message(STATUS "ptxas not found (optional - sample PTX files are pre-generated)")
endif()
