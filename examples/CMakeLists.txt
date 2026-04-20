add_executable(guerrillamail-cpp-example-basic
    basic_flow.cpp
)

target_link_libraries(guerrillamail-cpp-example-basic
    PRIVATE
        guerrillamail::guerrillamail-cpp
)
