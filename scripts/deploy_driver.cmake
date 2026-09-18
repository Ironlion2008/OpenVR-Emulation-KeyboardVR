# Called by CMake post-build with:
#   cmake -DDRIVER_NAME=<name> -DSRC_DIR=<build-output> -DDST_DIR=<steamvr/drivers> -P deploy_driver.cmake

set(dst "${DST_DIR}/${DRIVER_NAME}")
set(dst_bak "${DST_DIR}/${DRIVER_NAME}.bak")

# Rotate backup: remove old .bak, rename current install to .bak
if(EXISTS "${dst}")
    if(EXISTS "${dst_bak}")
        file(REMOVE_RECURSE "${dst_bak}")
    endif()
    file(RENAME "${dst}" "${dst_bak}")
endif()

# Deploy fresh build
file(COPY "${SRC_DIR}/${DRIVER_NAME}" DESTINATION "${DST_DIR}")
