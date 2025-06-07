target_sources(app PRIVATE
    src/behaviors/dual_release_combo.c
)

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dts/bindings)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/dts/bindings/behaviors/dual_release_combo.yaml
     DESTINATION ${ZEPHYR_BASE}/dts/bindings/behaviors) 