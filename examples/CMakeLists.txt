# =============================================================================
# Examples.  Each is a single-source executable that links against `astara`
# and writes its trace to a CSV file in the working directory at run-time.
# =============================================================================

function(astara_add_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE astara astara_warnings)
endfunction()

astara_add_example(reactivity_step_transient reactivity_step_transient.cpp)
astara_add_example(pump_trip                 pump_trip.cpp)
astara_add_example(pressurizer_heatup        pressurizer_heatup.cpp)
astara_add_example(helical_sg_transient      helical_sg_transient.cpp)
astara_add_example(primary_loop_atws         primary_loop_atws.cpp)