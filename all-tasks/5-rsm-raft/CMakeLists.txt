begin_task()

task_link_libraries(whirl-frontend)

# Task libraries
add_task_library(rsm rsm-raft)
add_task_library(kv kv-store)

# Tests

task_link_libraries(whirl-matrix)
add_task_library(tests/time_models tests-time-models)

add_task_test_dir(tests/tests-1 tests-1)
add_task_test_dir(tests/tests-2 tests-2)
add_task_test_dir(tests/tests-3 tests-3)

end_task()
