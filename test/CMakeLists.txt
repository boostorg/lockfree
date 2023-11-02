if(NOT TARGET boost_lockfree_all_tests)
    add_custom_target(boost_lockfree_all_tests)
endif()


set(Tests
    destructor_test
    freelist_test
    queue_bounded_stress_test
    queue_fixedsize_stress_test
    queue_interprocess_test
    queue_test
    queue_unbounded_stress_test
    spsc_queue_stress_test
    spsc_queue_test
    stack_bounded_stress_test
    stack_fixedsize_stress_test
    stack_interprocess_test
    stack_test
    stack_unbounded_stress_test
    tagged_ptr_test
)

foreach(Test ${Tests})
    # Test target
    add_executable(boost_lockfree_${Test} ${Test}.cpp )
    target_include_directories(boost_lockfree_${Test} PRIVATE . )
    target_link_libraries(boost_lockfree_${Test} PRIVATE
        Boost::lockfree
        Boost::unit_test_framework
        Boost::thread
        Boost::foreach
    )

    target_compile_definitions(boost_lockfree_${Test} PRIVATE BOOST_TEST_NO_OLD_TOOLS )

    # CTest Target
    add_test(NAME boost_lockfree_${Test} COMMAND boost_lockfree_${Test})
    add_dependencies(boost_lockfree_all_tests boost_lockfree_${Test} )
endforeach()

target_link_libraries(boost_lockfree_stack_interprocess_test PRIVATE Boost::interprocess)
target_link_libraries(boost_lockfree_queue_interprocess_test PRIVATE Boost::interprocess)
