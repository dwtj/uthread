# uthread #
## A Basic Many-To-Many User-to-Kernel Thread Library ##

The `uthread` library is a cooperative user thread library with dynamic many-to-many mapping between user threads and kernel threads. The user code can choose the maximum number of kernel threads that will be run, then an arbitrary number of user threads can be created to run on this number of threads. See the header file (`uthread.h`) and the source code file (`uthread.c`) for details of the public interface.

Invoke `make` to build the uthread library (`uthread.o`) and the test program (`test_uthread`).

For exact demonstration of how to use the library, see the `test_uthread.c` and the `makefile`. Notice that you will need to

- Include `uthread.h` in your application.
- Compile `uthread.c` to object code.
- Link your application with `uthread.o`.
