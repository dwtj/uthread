# uthread #

The `uthread` library is a cooperative user thread library. It performs dynamic many-to-many mappings between user threads and kernel threads.

The user code first chooses the maximum number of kernel threads that will be run. Then, an arbitrary number of user threads can be created to run on this number of threads. See the header file, `uthread.h`, for details about the public interface.

Invoke `make` to build the uthread library, `uthread.o`, and the test program, `test_uthread`.

For an exact demonstration of how to use the library, see the `test_uthread.c` and the `makefile`. Notice that you will need to

- Include `uthread.h` in your application.
- Compile the one dependency of `uthread.c`: `lib/heap.c`.
- Compile `uthread.c` to object code.
- Link your application with `uthread.o` and `heap.o`.

(See `lib/README.md` for an attribution to the heap implementation's authors.)
