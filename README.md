README

Yet another interpreter platform
====

This is actually just an interpreter platform -- that is, a modular virtual machine similar to LLVM (albeit much simpler).
It operates by reading one or more input files using several reader plugins which translate the source code into the byte-code, linking the resulting byte-code images and executing the resulting image in either a stack-based virtual machine or the host CPU itself (JIT-compiling the byte-code for the host CPU architecture). Except this, all components of the platform are meant to be completely portable and platform-independent.

The platform is divided into an unchangeable core and a number of modules. The core provides end-user API and interfaces for the modules (including all definitions the modules shall use to communicate with each other, e. g. data types).

The platform supports two data types: *signed integer* and *floating-point*. Pointers to data can be stored in the integer format.

Supported features
----

Currently supported core features:
* Reading source files into so-called *"contexts"* (byte-code images)
* Merging the contexts together (with linking support)
* Dumping contexts into files using source file plugins
* Translating contexts into the native CPU machine code
* Executing contexts in a stack-based virtual machine without compiling
* Intercepting the native OS exceptions (like *SIGSEGV*) and translating them into C++ ones

Currently supported input formats (source plugins):
* Own assembly language (*read only*)
* Byte-code format (*read and write*)

JIT
----

The JIT-translation is a way to execute code faster than in a virtual machine. Actually, this method of execution has some limitations and shortcomings compared to the virtual machine itself: for example, type checking is not performed at all.
(You can find a full list of JIT's limitations two sections below.)

The JIT translation is currently implemented for Intel IA-32e (Long mode) architecture. It uses System V x86_64 ABI.

Building the platform
====

The platform can be built using Clang C++ compiler. Building with GCC shall be possible, but the author, while relying on "bleeding-edge" C\+\+11 conformance, rarely tests compilation with this toolchain. Overall building process is managed using [CMake] [1].

Platform consists of two components: an AAL-like library called [uXray](intelfx/uXray) and the platform itself, which can be accessed here: intelfx/Homework_2011. Git-repository for the latter contains the support library as a submodule, so the following can be done to checkout the sources:

	# git checkout git://github.com/intelfx/Homework_2011.git
	# cd Homework_2011
	# git submodule update --init

CMake is configured such that it would place the output files in a `__bin` folder inside of the source tree, regardless of where you would put the building directory. Moreover, the helper library will put its executable (which is a DSO) into the primary source tree, so you will end up having both the helper library and the platform in a same directory automatically.

The platform has two targets, `interpreterplatform` and `homework_stack_nl`. The former is a DSO, while the latter is a historic statically linked executable which does nothing more than just open an assembly file and interpret it.
Also there is a dynamically linked test application called `interpreterplatformtest`, which is a best way to test the platform.

Consider the following commands for building (assuming that you are in the root source tree after checking it out as shown above):

	# mkdir -p build/{uXray,platform}
	# cd build/uXray
	# cmake ../../Libs/uXray -DCMAKE_BUILD_TYPE=Release
	# make
	# cd ../platform
	# cmake ../../ -DCMAKE_BUILD_TYPE=Release
	# make
	# cd ../../__bin

After doing that, you will end up in the directory with three binary files (or more, if you built the platform on Windows).

Usage/testing
====

Running the test executable
----

The test executable (the dynamically linked one, `interpreterplatformtest`),  will load any number of files whose names are provided on the command line, link them together and pass the resulting context to execution.

Switches accepted by the test executable:
* `--jit`:      enable JIT mode.
* `--quiet`:    disable almost all logging.
* `--debug`:    enable debug logging.
* `--bytecode`: consider any further given files as binary files.
* `--asm`:      consider any further given files as assembly text files.
* `--dump-to`:  write the internal context (after loading and merging) to the given byte-code file (name shall be given as the next argument).

Assembly syntax
====

Assembly files consist of declarations and commands, separated with newlines and possibly intermixed. Comments are supported from any place in a line to the end of line. The comment character is `#`.

A command looks like `<name>.<type> [<arg>]` -- that is, it can have zero or one argument. The `name` identifies a command; `type` selects the data type the command works with and can be omitted (both type and a dot shall then be omitted).

A declaration looks like `decl.<type> [<name>] [= <value>]` or `decl.<type> [<name>] : <reference>`.
First form is a variable; it may or may not have a `name` and may or may not have a `value`. Second form is an alias; it may or may not have a `name` but shall have a `reference` to another declaration or to a memory region.

The name of a declaration becomes a reference itself; it is called a "symbol". A reference is associated with each symbol; if first form is used, it points to the address of the value in memory, and if second one is used, then the reference is directly taken from the declaration.

A special declaration form is a label (of form `<name>:`), which resolves to the address of an instruction immediately following the label.

Types
----

There are two data types in the platform: *integer* and *floating-point*.
They are selected by characters `i` and `f`, respectively.

Again, a type may be omitted to select the default data type (now it is *floating-point*). Service instructions like `call` or `ret` which do not work with data _shall_ have their type specifier omitted.

Arguments
----

An argument to a command is either a `value` or a `reference`. A value has its type picked automatically from the instruction's type, and shall be in a form recognizable by `strtol()` or `strtod()` C library functions. A reference links a command to some memory location and has a special form described later.

Values and references are not interchangeable. Some commands work with values, and some -- with references.

References
----

A reference has a form of `[<section>:]<first component>[+<second component>]`. Each component is either a direct address/offset (like `10`), a symbol (like `var1`), a register (like `$ra`) or an indirection entry of form `(<section>:<address>)` or `(<symbol>)` or `(<register>)`.

When resolving such a reference, indirection entries are decoded first and the data they point to is read as an integer, which is then considered a direct memory offset.
Then symbols are decoded. If either of a reference's components is a symbol (a reference itself), the symbol's section replaces the global section.
Then two components (resolved to offsets) are added together and the final value is then dereferenced.

A special type of the reference is a string (of a form `"<any text>"`), which resolves to the section and address of the given string value in memory.

Sections
----

Here is a list of sections:
* `c` -- a reference to code
* `d` -- a reference to data
* `r` -- a reference to the register file (by register #)
* `b` -- a reference to the bytepool (string pool, a byte-granular memory region)
* `f` -- a reference to the stack relative to the current stack frame towards the local variable region (after the frame pointer)
* `p` -- a reference to the stack relative to the cyrrent stack frame towards the function parameter region (behind the frame pointer)

Registers
----

The execution platform has a fixed count of registers which can be accessed using a named reference (like `$ra`) or an indexed reference (like `r:0`).
The registers are:
* ra (`$ra`, `r:0`)
* rb (`$rb`, `r:1`)
* rc (`$rc`, `r:2`)
* rd (`$rd`, `r:3`)
* re (`$re`, `r:4`)
* rf (`$rf`, `r:5`)

The register `$rf` is used as an implicit destination for some operations. Also, registers are used to communicate with the runtime and the client API.

Stacks
----

The platform has a separate stack for each data type. A single stack (now it is the *integer* one) is selected as the frame stack; references to sections `f` and `p` reference the frame stack.

The frame pointer is updated with the frame stack top each time a new function is entered. Parameter area begins from address `p:1` to higher addresses;
local variable area - from `f:0` to higher addresses.

Flags
----

Conditional branching commands of the platform use flags to determine the comparison result.
Flags can be explicitly set by a comparison or analysis command, or they can be implicitly set by any command which references a well-defined single value on stack, like `pop`, `top` or any arithmetic command. (See below for command list.)

Commands
----

Here is a list of the platform's virtual commands, along with their possible types and references:
* push.{i,f} <value>  -- push a immediate value on the top of stack
* pop.{i,f}           -- remove a value from the top of stack
* top.{i,f}           -- debug-print a value on the top of stack
* cmp.{i,f}           -- compare two values on the top of stack (subtrahend is on top and it is popped)
* anal.{i,f}          -- analyze a value on the top of stack (i. e., compare it with zero)
* dup.{i,f}           -- duplicate a value on the top of stack
* swap.{i,f}          -- swap two values on the top of stack
* lea <ref>           -- load a reference's absolute (effective) address into `$rf`
* ld.{i,f}            -- push a referenced value on the top of stack
* st.{i,f}            -- save the top of stack into the reference and pop the value
* ldint.f             -- push a referenced integer value on the top of the floating-point stack
* stint.f             -- save the top of the floating-point stack into the reference as an integer
* settype.{i,f} <ref> -- force-set the type of a memory location into the instruction's type
* abs.{i,f}           -- take an absolute value of the top of stack
* add.{i,f}           -- add two values on the top of stack
* sub.{i,f}           -- subtract the next value from the top of stack (minuend on top)
* mul.{i,f}           -- multiply two values on the top of stack
* div.{i,f}           -- divide the top of stack by the next value (dividend on top)
* mod.{i,f}           -- take a remainder of the stack's top from division by the next value (dividend on top)
* inc.{i,f}           -- increase the value on top of stack
* dec.{i,f}           -- decrease the value on top of stack
* neg.{i,f}           -- change the sign of the value on top of stack
* jump <ref>          -- an unconditional jump
* call <ref>          -- an unconditional call
* ret                 -- a subroutine return
* jcc <ref>           -- a conditional jump (conditions mimic the ones in x86 assembly language)
* snfc                -- do not implicitly change the flags (only do that with `cmp` and `anal` commands)
* cnfc                -- inverse of `snfc`
* quit.{i,f,nothing}  -- exit the context; indicate the type of return value (or lack of such) with the instruction's type.
* sys <value>         -- perform a system call (to the interpreter runtime). The argument must be integer.

JIT-compilation
====

The JIT-compilation is an alternative to VM-based execution. It is faster, but it has several drawbacks compared to execution in the VM.
* There is no type checking
* `snfc` and `cnfc` commands are not handled; only `cmp` and `anal` commands may change the flags
* Integer stack is also used for return addresses
* User-registered instructions are supported only if they do not work with stacks

The platform is able to automatically fall back to interpreting if an error happens during compilation or execution.

[1]: http://www.cmake.org "CMake"
