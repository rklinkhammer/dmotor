# Coding conventions

C++ uses C++14, four-space indentation, attached braces, fixed-width integer state,
explicit units, bounded arithmetic, and the checked-in clang-format configuration.
Types use PascalCase; functions/variables use snake_case; private members end in `_`.
Generated files are owned by the generator. HLS paths have no allocation, exceptions,
recursion, host I/O, or vendor headers. Vendor directives belong in platform adapters.
Widen before multiplying/subtracting; document bounds, rounding, and saturation.

RTL uses one primary clock and clock enables. Use nonblocking assignments for state,
explicit reset values, and separate CDC synchronizers at asynchronous boundaries.
Document input qualification and stable-configuration requirements. New events win
same-cycle clears; protection bypasses ordinary command processing. Reset release
must be synchronized by the wrapper. CDC attributes are hints, not CDC verification.

Tests fail independently of C++ NDEBUG and include adverse boundary conditions.
Run native/RTL tests after behavioral changes; HLS changes also require vendor
simulation/synthesis. Record unavailable tools and untested integration explicitly.
