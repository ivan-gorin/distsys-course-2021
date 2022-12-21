begin_task()

task_link_libraries(whirl-frontend)

# Task libraries
add_task_library(kv/node atomic-kv-node)
add_task_library(kv/client atomic-kv-client)

# Tests
task_link_libraries(whirl-matrix)
add_task_test_dir(tests/tests-1 tests-1)

end_task()
