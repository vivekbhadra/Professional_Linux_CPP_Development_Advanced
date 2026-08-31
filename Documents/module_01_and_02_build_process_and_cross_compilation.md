# Module 1: The C++ build process

## Why we start here

This module is the foundation of every other module in this course. You only get to cross-compiling when you know which tool produces which artefact. CMake is a program that generates and runs the same commands you would otherwise type by hand. The MQTT and HTTP modules link your code to libraries written by other people. If that linking step fails, an error message will be about symbols, archives and search paths. So, reading the output of nm and readelf is a routine part of the job, not a special skill.

The plan is easy. We take a small application, build it manually stage by stage, see what each stage has produced, and then package part of it as a static library and then as a shared library. We intentionally break the build twice along the way, because the two failures you will see are the two failures you are most likely to encounter in real work.

## The application we will build

The program calculates a couple of summary values on a set of sensor readings. It is deliberately small, but it is split into three files, which is the whole point: a single file program never teaches you anything about linking. The same code is propagated into CMake modules later in the course.

**Example 1.1: The interface of the sensor statistics library (`sensor_statistics.h`)**

```cpp
#ifndef SENSOR_STATISTICS_H
#define SENSOR_STATISTICS_H

#include <vector>

namespace telemetry {

double mean(const std::vector<double>& samples);
double peak(const std::vector<double>& samples);

}  // namespace telemetry

#endif  // SENSOR_STATISTICS_H
```

The header declares the two functions without defining them. The include guard stops the declarations being seen twice in one translation unit. GCC, Clang and MSVC all accept `#pragma once` as an alternative, and it is shorter, but it is not part of the standard, so a portable code base usually keeps the guard.

**Example 1.2: The implementation of the sensor statistics library (`sensor_statistics.cpp`)**

```cpp
#include "sensor_statistics.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace telemetry {

double mean(const std::vector<double>& samples)
{
    if (samples.empty()) {
        throw std::invalid_argument("mean() requires at least one sample");
    }

    const double total = std::accumulate(samples.begin(), samples.end(), 0.0);
    return total / static_cast<double>(samples.size());
}

double peak(const std::vector<double>& samples)
{
    if (samples.empty()) {
        throw std::invalid_argument("peak() requires at least one sample");
    }

    return *std::max_element(samples.begin(), samples.end());
}

}  // namespace telemetry
```

Note the `0.0` passed to `std::accumulate`. If you write `0` there, the accumulator is an `int`, every partial sum is truncated, and the function returns a wrong answer with no warning and no crash. It is a classic defect and worth carrying with you.

**Example 1.3: The application entry point (`telemetry_report_main.cpp`)**

```cpp
#include "sensor_statistics.h"

#include <iostream>
#include <vector>

int main()
{
    const std::vector<double> readings{21.4, 22.9, 23.7, 22.1, 24.8};

    std::cout << "samples : " << readings.size() << '\n';
    std::cout << "mean    : " << telemetry::mean(readings) << '\n';
    std::cout << "peak    : " << telemetry::peak(readings) << '\n';

    return 0;
}
```

Two habits in this file are worth adopting now. We never write `using namespace std;`, because it drags several thousand names into the global scope and creates ambiguities that only show up later, in a different file, after someone adds an include. And we end lines with `'\n'` rather than `std::endl`, because `std::endl` also flushes the stream, and a flush on every line is an unnecessary system call in a program that logs telemetry continuously. If your compiler is GCC 14 or newer you also have `std::print` from C++23 in `<print>`, which formats and writes in one call; C++20's `std::format` in `<format>` is available a little earlier, from GCC 13.

## The four stages

Invoking `g++ file.cpp -o app` runs four distinct programs in sequence. The driver hides them, so we will ask it to stop after each one.

### Preprocessing

The first stage is textual. We ask the driver to stop after it and write the result to a file, then measure the result.

```
$ g++ -std=c++17 -E sensor_statistics.cpp -o sensor_statistics.ii
$ wc -l sensor_statistics.ii
38129 sensor_statistics.ii
```

The 33 lines of Example 1.2 became 38,129 lines. The preprocessor replaced each `#include` with the full text of the named file, recursively, expanded macros, and evaluated the conditional directives. GCC uses the extension `.ii` for preprocessed C++ and `.i` for preprocessed C. What comes out is the translation unit, and the translation unit is what the compiler actually reads. This is why a missing `#include` in a header can break a source file that does not mention that header at all, and why compile times grow with the size of your header set rather than the size of your source.

C++20 modules change this model: an imported module is consumed as a compiled artifact rather than re-parsed as text, which removes the repeated preprocessing cost and the include-order fragility that comes with it. GCC 13 implements modules only partially and requires `-fmodules-ts`, so production builds on Linux are still overwhelmingly header based. We will keep to headers throughout this course.

### Compilation to assembly

The second stage is where the real work happens. Stopping after it gives us readable assembly for the host machine.

```
$ g++ -std=c++17 -S sensor_statistics.cpp -o sensor_statistics.s
$ wc -l sensor_statistics.s
774 sensor_statistics.s
```

The compiler parsed the translation unit, checked it against the language rules, instantiated the templates that were used, applied whatever optimizations the flags asked for, and emitted assembly text for the target architecture. Every diagnostic you have ever argued with came out of this stage. Nothing here knows anything about your other source files; `sensor_statistics.cpp` was compiled in complete ignorance of `telemetry_report_main.cpp`.

### Assembly to an object file

The third stage converts the assembly text into a binary object file, which we then ask the `file` command to identify.

```
$ g++ -std=c++17 -c sensor_statistics.cpp -o sensor_statistics.o
$ file sensor_statistics.o
sensor_statistics.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

The word to notice is *relocatable*. The object file holds machine code, read-only data, debug information and a symbol table, but the addresses inside it are not final. It records, in its relocation entries, every place where an address will have to be patched once somebody decides where this code is going to live. An object file cannot be executed.

### Linking

The fourth stage combines the object files into an executable, resolving the cross-references between them.

```
$ g++ -std=c++17 -c telemetry_report_main.cpp -o telemetry_report_main.o
$ g++ sensor_statistics.o telemetry_report_main.o -o telemetry_report
$ ./telemetry_report
samples : 5
mean    : 22.98
peak    : 24.8
```

The linker laid out the sections from both objects, assigned final addresses, applied the relocations, pulled in the C++ runtime startup code, and recorded which shared libraries the program will need at run time. Only now do the two source files know about each other.

## Symbols, and the error you will see most often

Everything the linker does is driven by symbols. Suppose we forget one of our object files.

```
$ g++ telemetry_report_main.o -o broken
/usr/bin/ld: telemetry_report_main.o: in function `main':
telemetry_report_main.cpp:(.text+0xfa): undefined reference to `telemetry::mean(std::vector<double, std::allocator<double> > const&)'
/usr/bin/ld: telemetry_report_main.cpp:(.text+0x141): undefined reference to `telemetry::peak(std::vector<double, std::allocator<double> > const&)'
collect2: error: ld returned 1 exit status
```

* Undefined Symbol Error: The linker failed because main calls a function that lacks a corresponding definition in any provided input file.
* Header vs. Linker Contract: The source code compiled successfully because the header promised the function existed, but that promise was broken at the link stage.
* Linker Diagnosis Strategy: Interpret the error message as a missing file or library in your link command rather than a defect in the source code syntax.
* Name Mangling Mechanism: To support function overloading, the compiler combines the namespace, function name, and parameter types into a single encoded symbol string.

```
$ nm sensor_statistics.o | grep telemetry
0000000000000000 T _ZN9telemetry4meanERKSt6vectorIdSaIdEE
0000000000000103 T _ZN9telemetry4peakERKSt6vectorIdSaIdEE

$ nm -C sensor_statistics.o | grep telemetry
0000000000000000 T telemetry::mean(std::vector<double, std::allocator<double> > const&)
0000000000000103 T telemetry::peak(std::vector<double, std::allocator<double> > const&)
```
The nm utility is a command-line tool in Linux used to dump and inspect symbol tables from binary files, including object files (.o), static library archives (.a), and shared objects or executables (.so, ELF binaries).

The encoded form is the mangled name. Reading `_ZN9telemetry4meanERKSt6vectorIdSaIdEE` from left to right: `_Z` marks a mangled C++ name, `N ... E` brackets a nested name whose parts are given as length-prefixed strings (`9telemetry`, then `4mean`), and the rest encodes the single parameter, a reference to a const `std::vector<double, std::allocator<double>>`. The `-C` option asks `nm` to demangle, and the standalone `c++filt` tool will demangle any name you paste into it. Because the parameter types are part of the symbol, changing a parameter from `int` to `long` in a header and rebuilding only some of the files gives you an undefined reference rather than silent corruption, which is a feature.

The letter in the middle column is the symbol type, and four of them matter day to day. `T` means the symbol is defined in this object, in the text section. `U` means it is used but not defined here, so somebody else must supply it. `W` marks a weak definition, which is what inline functions and template instantiations produce; several objects may each contain a copy and the linker keeps one. A lowercase letter means the symbol is local to this object and is not offered to the linker at all, which is exactly what an anonymous namespace or a `static` function gives you.

Looking at the other object from the same angle shows the other side of the contract.

```
$ nm -C telemetry_report_main.o | grep " U "
                 U telemetry::mean(std::vector<double, std::allocator<double> > const&)
                 U telemetry::peak(std::vector<double, std::allocator<double> > const&)
                 U std::ostream::operator<<(double)
                 U std::cout
```

`main` needs those four symbols, among others. Two came from our own object file and the rest came from the standard library. Linking is nothing more than matching every `U` in the inputs against a `T` or a `W` somewhere.

## Static libraries

Once a set of object files is stable, you stop passing them individually and collect them into an archive. The `ar` tool does this, and the archive can then be listed.

```
$ ar rcs libsensorstats.a sensor_statistics.o
$ ar t libsensorstats.a
sensor_statistics.o
$ g++ telemetry_report_main.o -L. -lsensorstats -o telemetry_report_static
$ ./telemetry_report_static
samples : 5
mean    : 22.98
peak    : 24.8
```

The `r` flag inserts members, `c` creates the archive without a warning if it does not exist, and `s` writes the index that lets the linker find a symbol without scanning every member. `-L.` adds the current directory to the library search path and `-lsensorstats` asks for `libsensorstats.a` or `libsensorstats.so`. A static library is a container of object files with an index, and nothing more. The members it contributes are copied into the executable, so the resulting binary has no run-time dependency on the archive.

Now the second deliberate failure. We change nothing except the order of the arguments.

```
$ g++ -L. -lsensorstats telemetry_report_main.o -o wrongorder
/usr/bin/ld: telemetry_report_main.o: in function `main':
telemetry_report_main.cpp:(.text+0xfa): undefined reference to `telemetry::mean(std::vector<double, std::allocator<double> > const&)'
collect2: error: ld returned 1 exit status
```

The GNU linker processes its inputs strictly left to right and keeps a running set of symbols that are still undefined. When it reaches an archive it extracts only those members that satisfy something currently undefined, and then it moves on and never looks back. Here the archive was examined first, at which point nothing was undefined, so no member was extracted; `telemetry_report_main.o` came afterwards and its references had nothing left to bind to. The rule that follows is worth memorizing: **libraries go after the objects and libraries that use them, and if two archives depend on each other you either repeat one of them on the command line or wrap the pair in `-Wl,--start-group` and `-Wl,--end-group`.** Object files are not affected, because every object given on the command line is included unconditionally.

## Shared libraries

A shared library is loaded at run time and shared between processes, which keeps executables small and lets you ship a fix without relinking every program. Building one takes two extra decisions.

```
$ g++ -std=c++17 -fPIC -c sensor_statistics.cpp -o sensor_statistics_pic.o
$ g++ -shared -Wl,-soname,libsensorstats.so.1 -o libsensorstats.so.1.0.0 sensor_statistics_pic.o
$ ln -sf libsensorstats.so.1.0.0 libsensorstats.so.1
$ ln -sf libsensorstats.so.1 libsensorstats.so
$ file libsensorstats.so.1.0.0
libsensorstats.so.1.0.0: ELF 64-bit LSB shared object, x86-64, version 1 (SYSV), dynamically linked, not stripped
```

`-fPIC` asks the compiler to generate position independent code, which reaches its globals through an indirection table instead of through absolute addresses, so the library works wherever the loader decides to map it. On x86-64 you cannot build a shared library without it. The `-soname` recorded in the library is the name the loader will ask for later, and it carries only the major version, which is the whole versioning scheme in one line: the three names are the real file, the interface version that programs bind to, and the development symlink that `-lsensorstats` resolves.

* g++ -std=c++17 -fPIC -c sensor_statistics.cpp -o sensor_statistics_pic.o: Compiles the source file into a relocatable object file using C++17 standards and position-independent code (-fPIC) required for shared libraries.
* g++ -shared -Wl,-soname,libsensorstats.so.1 -o libsensorstats.so.1.0.0 sensor_statistics_pic.o: Links the position-independent object file into a shared library named libsensorstats.so.1.0.0 while embedding libsensorstats.so.1 as its internal ABI runtime name.
* ln -sf libsensorstats.so.1.0.0 libsensorstats.so.1: Creates a symbolic link pointing the ABI major version name (libsensorstats.so.1) to the actual shared library release file.
* ln -sf libsensorstats.so.1 libsensorstats.so: Creates a development symbolic link (libsensorstats.so) pointing to the ABI version link so the compiler can locate the library via -lsensorstats.
* file libsensorstats.so.1.0.0: Inspects the dynamic binary artifact to confirm its ELF format, architecture, dynamically linked status, and symbol availability.

Linking against it succeeds, and then running it does not.

```
$ g++ telemetry_report_main.o -L. -lsensorstats -o telemetry_report_shared
$ ./telemetry_report_shared
./telemetry_report_shared: error while loading shared libraries: libsensorstats.so.1: cannot open shared object file: No such file or directory
$ ldd telemetry_report_shared
	linux-vdso.so.1 (0x00007f4baecf3000)
	libsensorstats.so.1 => not found
	libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007f4baea00000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f4baecab000)
```

`-L.` told the linker where to find the library at build time and said nothing about run time. The dynamic loader has its own search path, which does not include the current directory, so it looked in the system directories, failed, and refused to start the process. The `=> not found` line from `ldd` is the same failure reported one step earlier.

Setting `LD_LIBRARY_PATH` makes the symptom disappear and is the standard way to debug this, but shipping a wrapper script that sets it is a poor deployment strategy, because it leaks into every child process. Recording the search path inside the binary is cleaner.

```
$ g++ telemetry_report_main.o -L. -lsensorstats -Wl,-rpath,'$ORIGIN' -o telemetry_report_rpath
$ ./telemetry_report_rpath
samples : 5
mean    : 22.98
peak    : 24.8
$ readelf -d telemetry_report_rpath | grep -E 'NEEDED|PATH'
 0x0000000000000001 (NEEDED)             Shared library: [libsensorstats.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libstdc++.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [libgcc_s.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x000000000000001d (RUNPATH)            Library runpath: [$ORIGIN]
```

`$ORIGIN` is expanded by the loader to the directory holding the executable, so the program finds its private libraries no matter where the install tree is copied. Quote it in the shell, or it becomes an empty variable. For libraries that are genuinely installed system wide the correct answer is to place them under a directory listed in `/etc/ld.so.conf` and run `ldconfig`, which also creates the `.so.1` symlink from the recorded soname.

## Direct and transitive dependencies

Compare that `readelf -d` output with `ldd` run on the same class of binary.

```
$ ldd telemetry_report
	linux-vdso.so.1 (0x00007fca4a894000)
	libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007fca4a400000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007fca4a84b000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fca4a000000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007fca4a762000)
	/lib64/ld-linux-x86-64.so.2 (0x00007fca4a896000)
```

`libm.so.6` appears here but is not in the `NEEDED` list, because our binary does not depend on it directly; `libstdc++` does. `readelf -d` reports what this file records, while `ldd` reports the whole closure the loader would actually resolve. Two practical consequences follow. First, when you are auditing what you ship, use `readelf -d`. Second, prefer `readelf -d` or `objdump -p` on any binary you do not trust, because `ldd` works by asking the loader to do a trial resolution and can therefore run code from the file you are inspecting.

`file` also deserves a note, because its answer for a modern executable surprises people.

```
$ file telemetry_report
telemetry_report: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked,
interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=033f9811..., for GNU/Linux 3.2.0, not stripped
```

Most distributions build position independent executables by default, so the ELF type field says `DYN` rather than `EXEC`, and older versions of `file` describe such a binary as a shared object. It is an executable. `readelf -h` states it plainly as `DYN (Position-Independent Executable file)`. The interpreter line names the dynamic loader that the kernel will start first, and `not stripped` tells you the symbol table is still present.

## Build configurations

A configuration is a set of flags applied consistently across the whole project. Two are enough to start with.

A debug build uses `-O0 -g`. No optimization means the generated code follows the source statement by statement, so the debugger can show every variable and single-stepping goes where you expect. A release build uses `-O2 -DNDEBUG`, which enables the optimizations that are safe for general use and removes `assert` checks. `-O3` is not automatically better; it inlines and unrolls more aggressively, which can enlarge the binary enough to hurt cache behavior, so it is a measurement, not a default. Keeping `-g` in release builds costs nothing at run time, because the debug sections are not loaded into memory, and it is what makes a production core dump readable. When binary size matters, split the debug information into a separate file with `objcopy --only-keep-debug` and archive it alongside the release.

Warnings belong in every configuration. `-Wall -Wextra` is the working minimum, `-Wpedantic` adds portability complaints, and `-Werror` in continuous integration keeps the count at zero rather than letting it drift into the hundreds. Profiling and stack traces get much more useful with `-fno-omit-frame-pointer`, at a cost that is normally in the low single digits.

One rule protects you from an entire family of unpleasant bugs: the flags that affect the layout or the interface of types must be identical across everything you link together. Mixing a debug build of one library with a release build of another can violate the one definition rule, because the two translation units may see different definitions of the same class. The linker will not complain, and the program will misbehave far from the cause. This is one of the practical reasons we move to CMake in Module 3 rather than maintaining shell scripts.

Finally, note the flag pair `-MMD -MP`. It makes the compiler emit a makefile fragment listing every header the translation unit read, so an edit to a header triggers a rebuild of the files that include it. Header dependency tracking is the single hardest part of a hand written Makefile, and it is one of the things you stop thinking about once a real build system is doing it for you.

## Practical lab 1

Work in a clean directory containing the three source files from Examples 1.1 to 1.3.

1. Run the four stages separately with `-E`, `-S`, `-c` and a final link. Open `sensor_statistics.ii` and find where your own code starts. Open `sensor_statistics.s` and find the mangled name of `telemetry::mean`.
2. Identify every artifact with `file`. Explain in one sentence each why the object file is *relocatable* and the executable is *pie executable*.
3. List the symbols of both object files with `nm` and again with `nm -C`. Locate one `T`, one `U` and one `W` symbol and account for each.
4. Link without `sensor_statistics.o` and read the error. Now mark `telemetry::peak` as `static` in the `.cpp` file, rebuild the object, and observe how the symbol type and the error change.
5. Build `libsensorstats.a` and link against it. Then swap the argument order and reproduce the archive ordering failure.
6. Build the shared library with the soname and the symlink pair. Reproduce the loader failure, fix it once with `LD_LIBRARY_PATH` and once with `-Wl,-rpath,'$ORIGIN'`, and confirm the difference with `readelf -d`.
7. Compare `readelf -d` with `ldd` on the final binary and explain the extra entries.
8. Build the same source at `-O0` and at `-O2`, disassemble `telemetry::mean` in both with `objdump -d -C`, and compare the instruction counts.
9. Run `strip` on a copy of the release binary and measure the size difference. Confirm with `nm` that the symbols are gone and note which command still works.

---

# Module 2: Cross-compilation

## The problem being solved

The machine that compiles your code and the machine that runs it do not have to be the same, and in embedded and edge work they usually are not. A gateway board with an AArch64 processor, half a gigabyte of RAM and a read-only root filesystem is a poor place to run a compiler, and in many cases has no toolchain installed at all. Building on a fast x86-64 server and shipping the result also gives you one reproducible build environment rather than one per device.

Cross-compiling adds one requirement to everything from Module 1: every tool in the chain, and every library you link against, must belong to the target, not to the machine you are sitting at.

## Build, host and target

Three words are used precisely in toolchain documentation and loosely everywhere else. The *build* machine is where the compiler runs. The *host* machine is where the program being produced will run. The *target* only enters the picture when the program you are building is itself a compiler, and it means the machine that program will generate code for. For ordinary application work you can ignore *target* and think in terms of build machine and execution target. Our build machine is x86-64 Linux and our execution target is AArch64 Linux.

## Target triples and compiler prefixes

A toolchain identifies its output platform with a triple, which you can always ask the compiler to print.

```
$ aarch64-linux-gnu-g++ -dumpmachine
aarch64-linux-gnu
```

The fields are architecture, operating system and ABI, with an optional vendor field between the first two that is normally elided on Linux. `aarch64-linux-gnu` means 64-bit ARM, Linux, GNU C library. The 32-bit ARM triples show why the last field matters: `arm-linux-gnueabihf` uses the hard-float ABI and passes floating point arguments in the VFP registers, while `arm-linux-gnueabi` uses the soft-float ABI and passes them in the core registers. Both run on the same silicon and their object files cannot be linked together.

The triple is also the prefix on every tool in the cross toolchain, which is how you keep them apart from the native ones. Alongside `aarch64-linux-gnu-g++` you have `aarch64-linux-gnu-ld`, `-ar`, `-nm`, `-objdump`, `-readelf` and `-strip`. The convention is not cosmetic; typing `ar` instead of `aarch64-linux-gnu-ar` produces an archive whose index the cross linker may reject.

## What ABI compatibility actually covers

The application binary interface is the set of conventions two separately compiled pieces of code must agree on. It includes which registers carry arguments and return values, which registers a function must preserve, the sizes and alignments of the fundamental types, the layout of structures and virtual tables, the name mangling scheme, and the mechanism by which an exception propagates through a stack frame. Disagreement on any of these produces a program that links and then behaves incorrectly, if it links at all.

The linker catches the coarse case immediately. Here we hand a host library to the cross linker.

```
$ aarch64-linux-gnu-g++ telemetry_report_main_arm.o /lib/x86_64-linux-gnu/libm.so.6 -o x
/usr/lib/gcc-cross/aarch64-linux-gnu/13/../../../../aarch64-linux-gnu/bin/ld: /lib/x86_64-linux-gnu/libm.so.6: error adding symbols: file in wrong format
collect2: error: ld returned 1 exit status
```

`file in wrong format` means the machine field in the ELF header does not match what this linker emits. Whenever you see it, some part of your search path is pointing at the build machine's libraries. It is the single most common cross-compilation error, and it is almost always caused by a stray `-L/usr/lib` or by a `pkg-config` invocation that consulted the host database.

Two subtler ABI issues are worth knowing about before you meet them. The first is glibc symbol versioning: a binary linked against glibc 2.35 records versioned references that an older glibc on the device cannot satisfy, so the compatibility runs one way only, and the toolchain's glibc must be no newer than the target's. The second is the dual C++ ABI that GCC introduced with version 5, in which `std::string` and `std::list` have two incompatible layouts selected by the `_GLIBCXX_USE_CXX11_ABI` macro. If you must link against an old vendor library built before that change, you compile your own code with `-D_GLIBCXX_USE_CXX11_ABI=0`. The symptom of getting it wrong is an undefined reference to a function whose demangled name contains `std::__cxx11`.

## Sysroots

The compiler needs the target's headers and libraries, and it must not be able to see the build machine's. The directory tree holding them is the sysroot, and it mirrors the layout of the device's root filesystem.

Debian and Ubuntu cross packages take the multiarch approach and place the target files in an architecture-specific directory that the toolchain already knows about.

```
$ ls /usr/aarch64-linux-gnu/lib/
Scrt1.o  crt1.o  crti.o  crtn.o  ld-linux-aarch64.so.1  libc.so.6  libstdc++.so.6 ...
```

That is the AArch64 C library, the AArch64 C++ runtime, the AArch64 dynamic loader and the AArch64 startup object files, all sitting beside their x86-64 equivalents without conflicting. Nothing extra needs configuring for the simple case, which is why the lab below works straight after installing the package.

Vendor toolchains behave differently. A Yocto SDK, a Buildroot output or a board vendor's release ships a self-contained sysroot and expects you to point at it explicitly with `--sysroot=/path/to/sysroot`, usually by sourcing an environment script that sets `CC`, `CXX`, `SYSROOT` and the flags together. Once a sysroot is in play, `#include <openssl/ssl.h>` must resolve inside it, and `pkg-config` must be told to look inside it as well through `PKG_CONFIG_SYSROOT_DIR` and `PKG_CONFIG_LIBDIR`. This matters directly in Modules 7 and 9, where you link against an MQTT client library and `libcurl`.

## Building the application for AArch64

Nothing about the source changes. Only the driver does.

```
$ aarch64-linux-gnu-g++ -std=c++17 -O2 -c sensor_statistics.cpp -o sensor_statistics_arm.o
$ aarch64-linux-gnu-g++ -std=c++17 -O2 -c telemetry_report_main.cpp -o telemetry_report_main_arm.o
$ aarch64-linux-gnu-g++ sensor_statistics_arm.o telemetry_report_main_arm.o -o telemetry_report_arm
$ file telemetry_report_arm
telemetry_report_arm: ELF 64-bit LSB pie executable, ARM aarch64, version 1 (SYSV), dynamically linked,
interpreter /lib/ld-linux-aarch64.so.1, BuildID[sha1]=b2c66878..., for GNU/Linux 3.7.0, not stripped
```

Compare this line with the one in Module 1. The class and the general shape are the same, but the machine is now ARM aarch64 and the interpreter is `/lib/ld-linux-aarch64.so.1` rather than `/lib64/ld-linux-x86-64.so.2`. The interpreter path is recorded as an absolute path and must exist on the device, which is one of the things that makes a static build attractive for a quick test.

Confirming the header directly is a good habit, since `file` is guessing from patterns and `readelf` is reading the fields.

```
$ readelf -h telemetry_report_arm | grep -E 'Class|Type|Machine'
  Class:                             ELF64
  Type:                              DYN (Position-Independent Executable file)
  Machine:                           AArch64
```

Attempting to run it on the build machine produces the expected refusal.

```
$ ./telemetry_report_arm
bash: ./telemetry_report_arm: cannot execute binary file: Exec format error
```

The kernel read the ELF header, found a machine type it has no support for, and returned `ENOEXEC`. Seeing this error confirms that the cross build did what it was asked. If the binary runs on the build machine, you compiled it with the native compiler by mistake.

## Installing the emulation tools

Cross-compiling produces a binary you cannot run, so the next thing to set up is somewhere to run it. QEMU offers two quite different modes and we will use both, so install everything in one go.

```
$ sudo apt update
$ sudo apt install qemu-user qemu-user-static qemu-system-arm qemu-utils \
                   qemu-efi-aarch64 ipxe-qemu cloud-image-utils openssh-client
$ qemu-aarch64 -version | head -1
qemu-aarch64 version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.16)
$ qemu-system-aarch64 --version | head -1
QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.18)
```

Each package earns its place. `qemu-user` provides `qemu-aarch64`, which runs one foreign executable at a time. `qemu-user-static` provides the same emulator built as a static binary, which is what makes `chroot` into a target root filesystem possible. `qemu-system-arm` is the package that contains `qemu-system-aarch64`, the full machine emulator, which is easy to miss because the package name says `arm` and the binary says `aarch64`. `qemu-utils` gives you `qemu-img` for creating and inspecting disk images. `qemu-efi-aarch64` installs the UEFI firmware that an AArch64 virtual machine boots from, since there is no BIOS on this architecture. `cloud-image-utils` provides `cloud-localds`, which builds the small configuration image that sets up your login on first boot. `ipxe-qemu` supplies the option ROM for the emulated network card.

That last one is worth knowing about, because leaving it out gives an error that reads as though QEMU itself is broken.

```
qemu-system-aarch64: -device virtio-net-pci,netdev=net0: failed to find romfile "efi-virtio.rom"
```

The virtio network device wants its PXE boot ROM at start-up and refuses to be created without it. Installing `ipxe-qemu` puts the file at `/usr/lib/ipxe/qemu/efi-virtio.rom` and the machine starts. Adding `,romfile=` to the device suppresses the requirement instead, at the cost of network booting.

We will run the cross-compiled binary in three increasingly realistic ways: directly under user-mode emulation, then inside a target root filesystem, then on a full emulated machine that you log in to over SSH and copy files onto.

## Running the result under QEMU user-mode emulation

User-mode emulation runs a single foreign binary on your Linux machine by translating its instructions and forwarding its system calls to your own kernel. There is no kernel boot, no filesystem to prepare and no login, so the turnaround is as fast as running a native program.

```
$ qemu-aarch64 -L /usr/aarch64-linux-gnu ./telemetry_report_arm
samples : 5
mean    : 22.98
peak    : 24.8
```

The `-L` option gives QEMU the sysroot to use when the emulated program asks for its loader and its shared libraries. Leaving it out fails in a way that is worth seeing once, because it is the same class of failure as the one in Module 1.

```
$ qemu-aarch64 ./telemetry_report_arm
qemu-aarch64: Could not open '/lib/ld-linux-aarch64.so.1': No such file or directory
```

The path recorded in the binary is correct for the device and meaningless on this filesystem, so QEMU has to be told where the target root begins. User-mode emulation gives you a genuine check of the instruction set, the ABI and your program's logic, so an alignment fault or an endianness assumption will show up here. It runs on your own kernel, though, with your filesystem and your timing, so device drivers, `/dev` entries, memory pressure and real-time behavior are all outside its reach. Use it for functional testing and keep acceptance testing on the hardware.

## Assembling a target root filesystem

Running the binary out of your build directory tests the code but not the deployment. On the device the program will sit in an install directory and load its libraries from the device's own filesystem. You can rehearse that arrangement on the build machine by putting together a small root filesystem and pointing QEMU at it with `-L`.

```
$ mkdir -p target_root/lib target_root/opt/telemetry
$ S=/usr/aarch64-linux-gnu/lib
$ cp $S/ld-linux-aarch64.so.1 $S/libc.so.6 $S/libm.so.6 $S/libgcc_s.so.1 target_root/lib/
$ cp $S/libstdc++.so.6.0.33 target_root/lib/libstdc++.so.6
$ cp telemetry_report_arm target_root/opt/telemetry/
$ find target_root -type f | sort
target_root/lib/ld-linux-aarch64.so.1
target_root/lib/libc.so.6
target_root/lib/libgcc_s.so.1
target_root/lib/libm.so.6
target_root/lib/libstdc++.so.6
target_root/opt/telemetry/telemetry_report_arm
$ du -sh target_root
5.8M	target_root
$ qemu-aarch64 -L target_root ./target_root/opt/telemetry/telemetry_report_arm
samples : 5
mean    : 22.98
peak    : 24.8
```

Every path the emulated program asks for is resolved beneath `target_root`, so the interpreter recorded in the binary as `/lib/ld-linux-aarch64.so.1` is found at `target_root/lib/ld-linux-aarch64.so.1`. Notice that the real library file is named `libstdc++.so.6.0.33` and has to be installed under its soname, `libstdc++.so.6`, because that is the name the loader will search for. Getting the soname wrong here produces exactly the `cannot open shared object file` failure from Module 1, and it is the same mistake that breaks real deployments.

Those five files, at 5.8 MB in total, are the complete run-time dependency set for this program. That number is the honest answer to the question of what you actually have to ship alongside a dynamically linked C++ binary.

## Transparent execution with binfmt_misc

Typing `qemu-aarch64 -L ...` in front of every command becomes tedious once you are running test suites. The Linux kernel can be told to recognize AArch64 executables and start the emulator for you. The `qemu-user-static` package ships the registration already written.

```
$ cat /usr/lib/binfmt.d/qemu-aarch64.conf
:qemu-aarch64:M::\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\xb7\x00:
\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:
/usr/libexec/qemu-binfmt/aarch64-binfmt-P:OPF
```

The line is one long colon-separated record, shown wrapped here. The magic bytes are the start of an ELF header: `\x7fELF`, then class 2 for 64-bit, then little endian, and at the end `\xb7\x00`, which is machine type 183, the value the ELF specification assigns to AArch64. The mask beside it tells the kernel which of those bytes must match. The last field lists the flags, and `F` is the interesting one: it makes the kernel open the emulator at registration time and keep the file descriptor, so the emulator is still reachable even from inside a `chroot` where `/usr/libexec` no longer exists.

```
$ sudo systemctl restart systemd-binfmt
$ ./telemetry_report_arm
```

After registration the binary runs from the shell with no prefix and prints the same three lines as before. If it still reports `Exec format error`, check that `/proc/sys/fs/binfmt_misc` is mounted; some container and WSL environments do not mount it by default, and the registration silently has nowhere to go.

With that in place you can enter the target root filesystem directly, which gives you a shell where every command runs as AArch64 code.

```
$ sudo chroot target_root /opt/telemetry/telemetry_report_arm
```

This is how vendor build systems run target test suites, and how you can run the target's own `apt` or package tooling on a build server. It stops short of being a device, because the kernel underneath is still yours.

## Static linking and deployment

Dynamic linking is the right default for a device whose filesystem you control, because the runtime libraries are already there and shared. For a diagnostic tool dropped onto an unfamiliar system, a static binary removes every question about what is installed.

```
$ aarch64-linux-gnu-g++ -std=c++17 -O2 -static sensor_statistics.cpp telemetry_report_main.cpp -o telemetry_report_arm_static
$ file telemetry_report_arm_static
telemetry_report_arm_static: ELF 64-bit LSB executable, ARM aarch64, version 1 (GNU/Linux),
statically linked, BuildID[sha1]=af61088c..., for GNU/Linux 3.7.0, not stripped
$ qemu-aarch64 ./telemetry_report_arm_static
samples : 5
mean    : 22.98
peak    : 24.8
```

The type is now `EXEC` rather than `DYN`, there is no interpreter, and QEMU needed no `-L` because there was nothing left to resolve. The cost is size and the loss of shared library updates. A fully static glibc binary also has a genuine limitation: functions such as `getaddrinfo` and `getpwnam` load NSS modules dynamically at run time, so a statically linked program that resolves hostnames may still fail on a machine whose glibc differs. This is relevant to us, because Modules 7 and 9 resolve broker and server names.

The usual middle position is to link the C++ runtime statically and leave the C library dynamic, with `-static-libstdc++ -static-libgcc`. That solves the common case where the device's `libstdc++` is older than the one your toolchain compiled against, without any of the NSS problems.

Before copying anything to a device, run `aarch64-linux-gnu-strip` on a copy of the release binary and keep the unstripped original for debugging. The stripped file is what you transfer.

## Booting a full AArch64 target under QEMU system emulation

User-mode emulation cannot help you with anything that involves the operating system: a systemd service unit, a firewall rule, an SSH login, a package installation, or a network daemon such as the MQTT broker we run in Module 6. Full system emulation boots a real AArch64 Linux kernel on an emulated machine, with its own disk, its own network stack and its own users.

Start with a disk image. Ubuntu publishes ready-made arm64 server cloud images, which boot unattended and are the least troublesome starting point.

```
$ wget https://cloud-images.ubuntu.com/releases/24.04/release/ubuntu-24.04-server-cloudimg-arm64.img
$ qemu-img create -f qcow2 -F qcow2 -b ubuntu-24.04-server-cloudimg-arm64.img disk.qcow2 16G
$ qemu-img info disk.qcow2 | head -5
image: disk.qcow2
file format: qcow2
virtual size: 16 GiB (17179869184 bytes)
disk size: 196 KiB
cluster_size: 65536
```

Check the published directory for the exact file name, because the point release is part of it. Rather than growing and writing to the downloaded image, we create a copy-on-write overlay on top of it with `-b`. The overlay starts at a couple of hundred kilobytes, holds only the blocks you change, and can be deleted and recreated in seconds when you want a clean target. Keep the base image read-only.

A cloud image has no password and no user until cloud-init configures it, and it looks for that configuration on a small attached disk labeled `cidata`.

```
$ cat > user-data << 'EOF'
#cloud-config
users:
  - name: dev
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash
    ssh_authorized_keys:
      - ssh-ed25519 AAAA... your public key here
package_update: false
EOF
$ echo "instance-id: aarch64-lab" > meta-data
$ cloud-localds seed.img user-data meta-data
$ file seed.img
seed.img: ISO 9660 CD-ROM filesystem data 'cidata'
```

Paste the contents of your own `~/.ssh/id_ed25519.pub` into the `ssh_authorized_keys` entry. If you would rather use a password on a throwaway lab machine, replace that entry with `lock_passwd: false` and `plain_text_passwd: devpass`, and add `ssh_pwauth: true` at the top level. Do not carry that shortcut into anything that touches a real network.

AArch64 machines boot through UEFI rather than a BIOS. The firmware comes in two files: the code, which is read-only and shared, and the variable store, which each virtual machine needs its own writable copy of.

```
$ cp /usr/share/AAVMF/AAVMF_VARS.fd varstore.fd
```

Now start the machine.

```
$ qemu-system-aarch64 \
    -machine virt -cpu cortex-a57 -smp 2 -m 2048 \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/AAVMF/AAVMF_CODE.fd \
    -drive if=pflash,format=raw,file=varstore.fd \
    -drive if=virtio,format=qcow2,file=disk.qcow2 \
    -drive if=virtio,format=raw,file=seed.img \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-pci,netdev=net0 \
    -nographic
UEFI firmware (version 2024.02-2ubuntu0.9 built at 12:48:22 on Jun  2 2026)
```

The firmware banner is the first thing on the console, and the kernel messages and a login prompt follow, because `-nographic` sends the emulated serial port to your terminal. Taking the options in turn: `virt` is the generic QEMU machine model, which is what you use when you are not emulating one specific board; `cortex-a57` is a widely supported AArch64 core, and `-cpu max` enables every feature this QEMU build knows about if you want the newer instruction sets. The two `pflash` drives are the firmware pair. The two `virtio` drives are the root disk and the cloud-init seed. The `user` network back end needs no root privileges and no bridge configuration, and `hostfwd=tcp::2222-:22` is the part that matters for us: it forwards port 2222 on the build machine to port 22 inside the guest. To leave the console, press Ctrl-a and then x.

The first boot takes noticeably longer than the ones after it, because cloud-init is generating host keys and applying your configuration. Emulation with no hardware assistance translates every instruction, so the machine feels like an old low-power board. KVM acceleration is only available when the host is itself AArch64.

One failure is worth recognizing, because the message does not say what is wrong.

```
BdsDxe: failed to load Boot0001 "UEFI Misc Device" from PciRoot(0x0)/Pci(0x2,0x0): Not Found
BdsDxe: failed to load Boot0002 "UEFI Misc Device 2" from PciRoot(0x0)/Pci(0x3,0x0): Not Found

>>Start PXE over IPv4.
```

The firmware found no bootable partition on any attached drive and fell back to network booting. In practice this means the disk path is wrong, the image was never downloaded properly, or the `format=` given to `-drive` does not match the actual file. Check with `qemu-img info` before looking anywhere else.

## Transferring the cross-compiled binary to the emulated target

With port forwarding in place, the guest is reachable on `localhost:2222` and the ordinary tools work. Note the case difference between the two commands: `ssh` takes a lowercase `-p` and `scp` takes an uppercase `-P`.

```
$ scp -P 2222 telemetry_report_arm dev@localhost:/home/dev/
$ ssh -p 2222 dev@localhost
dev@ubuntu:~$ uname -m
aarch64
dev@ubuntu:~$ file telemetry_report_arm
dev@ubuntu:~$ ldd telemetry_report_arm
dev@ubuntu:~$ ./telemetry_report_arm
samples : 5
mean    : 22.98
peak    : 24.8
```

This is the first run of the program on a genuine AArch64 Linux system, with a real kernel, a real dynamic loader and the distribution's own `libstdc++`. Run `file` and `ldd` on the target as well as on the build machine, because the comparison is the point: the same binary, described by the target's own tools, with every dependency resolved to a real path rather than to your sysroot. If `ldd` reports a missing library here, you have found a genuine deployment gap that user-mode emulation was hiding from you, and the fix is either to install the package on the target or to link that library statically.

Recreating the VM changes its host key, and SSH will refuse to connect until you remove the old entry.

```
$ ssh-keygen -R '[localhost]:2222'
```

Copying files one at a time gets tiresome during development, so there are two alternatives worth knowing. A directory on the build machine can be shared into the guest, so a rebuild is visible immediately with no copying at all.

```
    -virtfs local,path=/home/dev/build,mount_tag=hostshare,security_model=mapped-xattr,id=hostshare
```

```
dev@ubuntu:~$ sudo mount -t 9p -o trans=virtio,version=9p2000.L hostshare /mnt
```

The `virtiofs` back end is faster than 9p but needs a `virtiofsd` daemon running beside QEMU, which is more setup than a lab needs. When the machine is shut down, `virt-copy-in` from the `libguestfs-tools` package writes files straight into the disk image, which is the right tool for baking a binary into an image you are about to ship.

Two habits make the emulated target pleasant to work with. Take a snapshot once the machine is configured, so you can throw away a broken experiment in a second.

```
$ qemu-img snapshot -c clean disk.qcow2
$ qemu-img snapshot -a clean disk.qcow2
```

And shut the guest down with `sudo poweroff` rather than killing QEMU, so the filesystem is flushed. From Module 6 onwards this same virtual machine is a useful place to put the Mosquitto broker and the HTTP test service, which lets you exercise the telemetry agent across a real network interface instead of over the loopback device.

## Inspecting foreign binaries

GNU binutils on Debian and Ubuntu are built with support for many object formats, so some native tools handle foreign files without complaint.

```
$ nm -C sensor_statistics_arm.o | grep telemetry
0000000000000000 T telemetry::mean(std::vector<double, std::allocator<double> > const&)
0000000000000094 T telemetry::peak(std::vector<double, std::allocator<double> > const&)
```

Symbol tables and ELF headers are architecture-neutral structures, so `nm`, `readelf` and `file` read an AArch64 object perfectly well on an x86-64 machine. Disassembly is a different matter, because it requires a decoder for the specific instruction set.

```
$ objdump -d telemetry_report_arm
telemetry_report_arm:     file format elf64-little
objdump: can't disassemble for architecture UNKNOWN!
```

The prefixed tool has the decoder compiled in and produces what you wanted.

```
$ aarch64-linux-gnu-objdump -d --no-show-raw-insn telemetry_report_arm
0000000000001134 <_ZN9telemetry4peakERKSt6vectorIdSaIdEE>:
    1134:	mov	x1, x0
    1138:	ldr	x0, [x0]
    113c:	ldr	x1, [x1, #8]
    1140:	cmp	x1, x0
    1144:	b.eq	1180 <_ZN9telemetry4peakERKSt6vectorIdSaIdEE+0x4c>
    1148:	ldr	d0, [x0], #8
```

Those are AArch64 instructions operating on the 64-bit `x` registers and the floating point `d0`, and the `ldr d0, [x0], #8` is a post-indexed load stepping through the vector. The safe rule is to reach for the prefixed tool always, and treat the native one working as a convenience rather than something to rely on.

## Where this goes next

Everything in these two modules was typed by hand, and that does not scale past a handful of files. Module 3 expresses the same build as a `CMakeLists.txt`, and cross-compiling then reduces to supplying a toolchain file that names the compiler prefix and the sysroot, so that one project description serves both the x86-64 and the AArch64 build.

## Practical lab 2

Install the toolchain and the emulation tools first.

```
$ sudo apt update
$ sudo apt install g++-aarch64-linux-gnu qemu-user qemu-user-static \
                   qemu-system-arm qemu-utils qemu-efi-aarch64 ipxe-qemu \
                   cloud-image-utils openssh-client
```

**Part A: cross-compiling**

1. Print the triple with `aarch64-linux-gnu-g++ -dumpmachine` and list the prefixed tools available under `/usr/bin`.
2. Cross-compile the Module 1 application for AArch64 without changing a single line of source.
3. Identify the result with `file` and confirm the machine and type fields with `readelf -h`. Attempt to run it natively and record the error.
4. List the direct dependencies with `readelf -d` and note the interpreter path.
5. Try to link the cross object against a host library and reproduce the `file in wrong format` diagnostic.
6. Disassemble `telemetry::mean` with the native `objdump` and then with `aarch64-linux-gnu-objdump`, and explain why one of them fails.

**Part B: user-mode emulation**

7. Run the binary under `qemu-aarch64` with the correct `-L` sysroot, then again without `-L`, and account for the difference.
8. Build the same program with `-static`. Compare the sizes, the ELF type and the `readelf -d` output against the dynamic build, and confirm that it runs under QEMU with no `-L`.
9. Build with `-static-libstdc++ -static-libgcc` and use `readelf -d` to show that `libstdc++.so.6` is no longer required while `libc.so.6` still is.
10. Assemble the `target_root` directory described above, install the binary under `/opt/telemetry`, and run it with `qemu-aarch64 -L target_root`. Then delete `target_root/lib/libstdc++.so.6` and record the failure. Restore it under its versioned name only, `libstdc++.so.6.0.33`, and explain why the program still does not start.
11. Register the emulator with `sudo systemctl restart systemd-binfmt`, then run the AArch64 binary from the shell with no prefix. Confirm the registration is live by listing `/proc/sys/fs/binfmt_misc`.
12. Strip a copy of the AArch64 binary with `aarch64-linux-gnu-strip`, record the size reduction, and confirm that it still runs.

**Part C: full system emulation and deployment**

13. Download the Ubuntu arm64 server cloud image and create a `qcow2` overlay on top of it. Record the size of the overlay before first boot.
14. Write a `user-data` file containing your own SSH public key, build the seed image with `cloud-localds`, and confirm with `file` that it is an ISO 9660 image labeled `cidata`.
15. Copy `AAVMF_VARS.fd` to a writable variable store and boot the machine with the command given above. Time the first boot and the second.
16. Deliberately point the root `-drive` at a file that does not exist or give it the wrong `format=`, and reproduce the `BdsDxe: failed to load Boot0001` and PXE fallback output.
17. Log in over the forwarded port with `ssh -p 2222 dev@localhost` and confirm with `uname -m` that you are on an AArch64 machine.
18. Transfer the stripped binary with `scp -P 2222` and run it on the target. Compare the output of `file` and `ldd` on the target with the output of the same commands on the build machine.
19. Remove the C++ runtime on the target with `sudo apt remove libstdc++6`, or rename the library, and observe the loader error. Then transfer the `-static-libstdc++` build and show that it still runs.
20. Take a snapshot with `qemu-img snapshot -c clean`, break something inside the guest, and restore it with `-a clean`.
21. Optional: share your build directory into the guest with `-virtfs` and mount it over 9p, then rebuild on the host and rerun on the target without copying anything.
