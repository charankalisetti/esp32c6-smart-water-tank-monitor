cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "components/services" "components/drivers" "components/cloud")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32c6_water_monitor)
