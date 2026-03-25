## Rules

1. All code must be ANSI C (C89) compliant.
   - Variables must be defined at the beginning of methods
   - Only C-style comments, `/* */` may be used.
   - Functions that did not exist in ANSI C should be implemented within the shared utility library.
     For example, strncpy.
   - The utility library should also implement bool and the standard uint16, etc. types.
   - Code should be compiled in strict C89 mode.
   - Variable names should use snake case. (ie. `char *user_name`)
   - Method and type names should use camel case. (ie. `FindUserByUsername(db, user_name)`)
   - Types and methods should be well documented.
   - Headers should include a #ifndef / #define / #endif check to ensure they are not included more than once.
     This check should be done in the header and not in the code that includes the header.
2. No external libraries should be used beyond the standard C library and the libraries defined here.
   - Libraries specific to the operating system should also be avoided.
   - All operating system specific code should be wrapped in wrapper methods to allow for easy porting. 
     The wrapper methods should be in a separate `.c` file with their own `.h` file so that the proper `.c` file can
     be included at build time. For example, a library might define `os.h` which define OS specific wrapper methods
     and then the build script could include `unix.c`, `dos.c`, etc. at build time.
3. To maintain compatibility with DOS, all file names **must** adhere to the 8.3 naming scheme.
4. Libraries should exist in their own folders.
   - For example, a utility library might be found in `src/util/util.c`.
5. Each library should have a primary header in `include` and any additional headers should be in their own folders.
   - The primary header should include any additional headers.
   - For example, `include/util.h` might include `include/util/strings.h`.
6. All library functions must have comprehensive tests.
   - A very simple shared unit testing library should be created to help facilitate testing while increasing code reuse.
7. Tests should exist in their own folders. For example: `tests/util/strings.c`.
8. Libraries should try not to allocate or deallocate memory themselves.
   - For simple cases, memory should be allocated on the stack or in the data segment.
   - For more complicated cases, the calling code should pass in pointers to any memory that the library needs.
   - Helper methods should be provided for initalizing structs, etc. to make it easier for the calling code.
   - For libraries where it makes more sense for the library to maintain its own memory (btree for example), 
     wrap `malloc` and `free` in wrapper methods and place those methods in the OS dependent code.

