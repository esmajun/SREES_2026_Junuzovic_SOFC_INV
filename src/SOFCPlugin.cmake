set(SOFC_PLUGIN_NAME sofc_inv)
file(GLOB SOFC_PLUGIN_SOURCES  ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB SOFC_PLUGIN_INCS  ${CMAKE_CURRENT_LIST_DIR}/src/*.h)
file(GLOB SOFC_INC_GUI  ${NATID_SDK_INC}/gui/*.h)
file(GLOB SOFC_INC_TD  ${NATID_SDK_INC}/td/*.h)
file(GLOB SOFC_INC_CNT  ${NATID_SDK_INC}/cnt/*.h)
file(GLOB SOFC_INC_MU  ${NATID_SDK_INC}/mu/*.h)
file(GLOB SOFC_INC_MEM  ${NATID_SDK_INC}/mem/*.h)
file(GLOB SOFC_INC_FO ${NATID_SDK_INC}/fo/*.h)
file(GLOB SOFC_INC_SC ${NATID_SDK_INC}/sc/*.h)
file(GLOB SOFC_INC_SYST ${NATID_SDK_INC}/syst/*.h)
file(GLOB SOFC_INC_DENSE ${NATID_SDK_INC}/dense/*.h)
file(GLOB SOFC_INC_SPARSE ${NATID_SDK_INC}/sparse/*.h)

add_library(${SOFC_PLUGIN_NAME} SHARED ${SOFC_PLUGIN_SOURCES} ${SOFC_INC_GUI} ${SOFC_PLUGIN_INCS}
                        ${SOFC_INC_TD} ${SOFC_INC_SYST}
                        ${SOFC_INC_CNT} ${SOFC_INC_MU} ${SOFC_INC_MEM} ${SOFC_INC_FO}
                        ${SOFC_INC_SC} ${SOFC_INC_DENSE} ${SOFC_INC_SPARSE})

source_group("inc\\inc"     FILES ${SOFC_PLUGIN_INCS})
source_group("inc\\gui"     FILES ${SOFC_INC_GUI})
source_group("inc\\td"      FILES ${SOFC_INC_TD})
source_group("inc\\cnt"     FILES ${SOFC_INC_CNT})
source_group("inc\\dense"   FILES ${SOFC_INC_DENSE})
source_group("inc\\mu"      FILES ${SOFC_INC_MU})
source_group("inc\\mem"     FILES ${SOFC_INC_MEM})
source_group("inc\\fo"      FILES ${SOFC_INC_FO})
source_group("inc\\sc"      FILES ${SOFC_INC_SC})
source_group("inc\\sparse"  FILES ${SOFC_INC_SPARSE})
source_group("inc\\syst"    FILES ${SOFC_INC_SYST})
source_group("src\\cpp"     FILES ${SOFC_PLUGIN_SOURCES})

target_link_libraries(${SOFC_PLUGIN_NAME} 
    debug ${MU_LIB_DEBUG} optimized ${MU_LIB_RELEASE}
    debug ${MATRIX_LIB_DEBUG} optimized ${MATRIX_LIB_RELEASE}
    debug ${NATGUI_LIB_DEBUG} optimized ${NATGUI_LIB_RELEASE})

target_compile_definitions(${SOFC_PLUGIN_NAME} PUBLIC PLUGIN_EXPORTS)