## Brief overview
Guidelines for handling test-related development tasks in C++ projects, focusing on integration testing and test organization best practices.

## Testing approach
- Write integration tests that verify end-to-end functionality across multiple components
- Use descriptive test names that clearly indicate what behavior is being tested
- Group related tests within the same test file to maintain organization
- Follow the Arrange-Act-Assert pattern for clear test structure

## Test file organization
- Place test files in the test/ directory with src/ subdirectory for implementation
- Use consistent naming conventions: test_<component>_service.cpp for service tests
- Include necessary setup and teardown for each test to ensure isolation
- Group related test cases within the same test file when they share setup logic

## Test implementation
- Use Google Test framework conventions and assertions
- Mock external dependencies when testing service implementations
- Verify both success and error scenarios in test coverage
- Include tests for edge cases and boundary conditions

## Code quality in tests
- Maintain the same code quality standards in test files as in production code
- Use meaningful variable names and avoid magic numbers in tests
- Include comments for complex test setup or non-obvious assertions
- Keep test functions focused and avoid testing multiple scenarios in one test
