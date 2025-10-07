## Brief overview
Guidelines for maintaining project README.md to ensure documentation stays current with code changes, build system modifications, and architectural updates.

## Documentation maintenance triggers
- **Build system changes**: Any modifications to CMakeLists.txt files must be reflected in build instructions
- **API modifications**: Public interface changes require API reference updates
- **Dependency updates**: New or updated dependencies must be documented in prerequisites
- **Platform-specific fixes**: macOS, Windows, or Linux specific workarounds must be added
- **New features**: Feature additions require usage examples and architectural documentation
- **Configuration changes**: New CMake options or build flags need documentation
- **Architectural modifications**: Changes to system design, component responsibilities, or data flow require Architecture section updates
- **Component changes**: Addition, removal, or renaming of major system components (DataNode, Coordinator, Client) requires Project Structure updates
- **Interface changes**: Modifications to component interactions or communication protocols require API reference updates
- **Implementation transitions**: Tasks starting as analysis/recommendations but resulting in code/dependency changes require README updates
- **Scope evolution**: When task scope expands from analysis-only to include implementation, README maintenance becomes mandatory

## Required documentation sections
- **Quick Start**: Must work with current build system and dependencies
- **Build Instructions**: Keep platform-specific instructions current
- **API Reference**: Update examples when interfaces change
- **Project Structure**: Maintain accuracy as codebase evolves
- **Development Setup**: Update with new tools or requirements

## Quality standards
- **Accuracy**: All code examples must compile and run with current codebase
- **Completeness**: Document all public APIs and configuration options
- **Clarity**: Use clear, concise language with practical examples
- **Currency**: Remove outdated information and obsolete instructions
- **Consistency**: Follow established formatting and terminology

## Review checklist
Before completing any task that affects OR when a task transitions to include:
- [ ] Implementation work (not just analysis/recommendations)
- [ ] Build system (CMakeLists.txt changes)
- [ ] Public APIs (header file modifications)
- [ ] Dependencies (new libraries or tools)
- [ ] Platform support (OS-specific fixes)
- [ ] Project structure (new components or directories)
- [ ] Architecture (system design or component responsibilities)
- [ ] Component interfaces (interaction protocols or communication patterns)

**Always update README.md** to reflect these changes with accurate:
- [ ] Build instructions and commands
- [ ] API usage examples
- [ ] Dependency requirements
- [ ] Platform-specific guidance
- [ ] Project structure descriptions
- [ ] Architecture section updates
- [ ] Component interaction documentation

## Task lifecycle considerations
- **Analysis Tasks**: May not require README updates if no changes are made
- **Implementation Tasks**: Always check README requirements when making changes
- **Hybrid Tasks**: If analysis leads to implementation, README updates become mandatory
- **Scope Changes**: When task scope evolves from analysis-only to implementation, re-evaluate README requirements
- **Transition Points**: Document updates required when crossing from planning to execution

## Best practices
- **Test examples**: Verify all code snippets compile and execute
- **Version alignment**: Keep documented versions aligned with actual dependencies
- **Platform coverage**: Document solutions for common platform-specific issues
- **Build verification**: Test build instructions on target platforms
- **Example validation**: Ensure usage examples demonstrate current APIs

## Enforcement
This rule ensures project documentation maintains professional quality and provides accurate guidance for developers using or contributing to the codebase.
