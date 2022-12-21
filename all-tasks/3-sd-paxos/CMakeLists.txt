begin_task()

task_link_libraries(whirl-frontend)

# Task libraries
add_task_library(paxos/node paxos-node)
add_task_library(paxos/client paxos-client)

# Checker library

task_link_libraries(whirl-matrix)
add_task_library(consensus)

# Tests

add_task_test_dir(tests/tests-1 tests-1)

end_task()
