begin_task()

task_link_libraries(wheels await timber)

# Task libraries
add_task_library(rpc)
add_task_library(runtime)

# Tests
add_task_test_dir(tests tests)

end_task()
