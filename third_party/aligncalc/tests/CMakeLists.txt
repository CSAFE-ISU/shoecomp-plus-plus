# aligncalc tests (ctest): small executables on synthetic data, no
# framework; a test fails by returning nonzero (check.h).
set(ALIGNCALC_TESTS
    clique_exact   # searchCliques vs exact Bron-Kerbosch
    points_2d      # alignPoints, planted 2-D sets, worker counts
    match_points   # matchPoints (no fit) vs alignPoints disjoint
    anchor_mirror  # anchored search vs mirror-image matches
    distinct       # two copies -> two distinct alignments
    lines_3d       # alignLines3D, transform recovery, labels
    api            # Progress, cancel, Status, 1-D / 3-D points
)
foreach(name IN LISTS ALIGNCALC_TESTS)
    set(target aligncalc_test_${name})
    add_executable(${target} ${name}.cpp)
    target_link_libraries(${target} PRIVATE aligncalc::aligncalc)
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME ${name} COMMAND ${target})
    if(WIN32 AND ALIGNCALC_SHARED)
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:aligncalc>")
    endif()
endforeach()
