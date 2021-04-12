# inih (INI Not Invented Here)

[![TravisCI Build](https://travis-ci.org/benhoyt/inih.svg)](https://travis-ci.org/benhoyt/inih)

**inih (INI Not Invented Here)** is a simple [.INI file](http://en.wikipedia.org/wiki/INI_file) parser written in C. It's only a couple of pages of code, and it was designed to be _small and simple_, so it's good for embedded systems. It's also more or less compatible with Python's [ConfigParser](http://docs.python.org/library/configparser.html) style of .INI files, including RFC 822-style multi-line syntax and `name: value` entries.

To use it, just give `ini_parse()` an INI file, and it will call a callback for every `name=value` pair parsed, giving you strings for the section, name, and value. It's done this way ("SAX style") because it works well on low-memory embedded systems, but also because it makes for a KISS implementation.

You can also call `ini_parse_file()` to parse directly from a `FILE*` object, `ini_parse_string()` to parse data from a string, or `ini_parse_stream()` to parse using a custom fgets-style reader function for custom I/O.

Download a release, browse the source, or read about [how to use inih in a DRY style](http://blog.brush.co.nz/2009/08/xmacros/) with X-Macros.


## Compile-time options ##

You can control various aspects of inih using preprocessor defines:

### Syntax options ###

  * **Multi-line entries:** By default, inih supports multi-line entries in the style of Python's ConfigParser. To disable, add `-DINI_ALLOW_MULTILINE=0`.
  * **UTF-8 BOM:** By default, inih allows a UTF-8 BOM sequence (0xEF 0xBB 0xBF) at the start of INI files. To disable, add `-DINI_ALLOW_BOM=0`.
  * **Inline comments:** By default, inih allows inline comments with the `;` character. To disable, add `-DINI_ALLOW_INLINE_COMMENTS=0`. You can also specify which character(s) start an inline comment using `INI_INLINE_COMMENT_PREFIXES`.
  * **Start-of-line comments:** By default, inih allows both `;` and `#` to start a comment at the beginning of a line. You can override this by changing `INI_START_COMMENT_PREFIXES`.
  * **Allow no value:** By default, inih treats a name with no value (no `=` or `:` on the line) as an error. To allow names with no values, add `-DINI_ALLOW_NO_VALUE=1`, and inih will call your handler function with value set to NULL.

### Parsing options ###

  * **Stop on first error:** By default, inih keeps parsing the rest of the file after an error. To stop parsing on the first error, add `-DINI_STOP_ON_FIRST_ERROR=1`.
  * **Report line numbers:** By default, the `ini_handler` callback doesn't receive the line number as a parameter. If you need that, add `-DINI_HANDLER_LINENO=1`.
  * **Call handler on new section:** By default, inih only calls the handler on each `name=value` pair. To detect new sections (e.g., the INI file has multiple sections with the same name), add `-DINI_CALL_HANDLER_ON_NEW_SECTION=1`. Your handler function will then be called each time a new section is encountered, with `section` set to the new section name but `name` and `value` set to NULL.

### Memory options ###

  * **Stack vs heap:** By default, inih creates a fixed-sized line buffer on the stack. To allocate on the heap using `malloc` instead, specify `-DINI_USE_STACK=0`.
  * **Maximum line length:** The default maximum line length (for stack or heap) is 200 bytes. To override this, add something like `-DINI_MAX_LINE=1000`. Note that `INI_MAX_LINE` must be 3 more than the longest line (due to `\r`, `\n`, and the NUL).
  * **Initial malloc size:** `INI_INITIAL_ALLOC` specifies the initial malloc size when using the heap. It defaults to 200 bytes.
  * **Allow realloc:** By default when using the heap (`-DINI_USE_STACK=0`), inih allocates a fixed-sized buffer of `INI_INITIAL_ALLOC` bytes. To allow this to grow to `INI_MAX_LINE` bytes, doubling if needed, set `-DINI_ALLOW_REALLOC=1`.

## Simple example in C ##

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ini.h"

typedef struct
{
    int version;
    const char* name;
    const char* email;
} configuration;

static int handler(void* user, const char* section, const char* name,
                   const char* value)
{
    configuration* pconfig = (configuration*)user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("protocol", "version")) {
        pconfig->version = atoi(value);
    } else if (MATCH("user", "name")) {
        pconfig->name = strdup(value);
    } else if (MATCH("user", "email")) {
        pconfig->email = strdup(value);
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

int main(int argc, char* argv[])
{
    configuration config;

    if (ini_parse("test.ini", handler, &config) < 0) {
        printf("Can't load 'test.ini'\n");
        return 1;
    }
    printf("Config loaded from 'test.ini': version=%d, name=%s, email=%s\n",
        config.version, config.name, config.email);
    return 0;
}
```


## C++ example ##

If you're into C++ and the STL, there is also an easy-to-use [INIReader class](https://github.com/benhoyt/inih/blob/master/cpp/INIReader.h) that stores values in a `map` and lets you `Get()` them:

```cpp
#include <iostream>
#include "INIReader.h"

int main()
{
    INIReader reader("../examples/test.ini");

    if (reader.ParseError() < 0) {
        std::cout << "Can't load 'test.ini'\n";
        return 1;
    }
    std::cout << "Config loaded from 'test.ini': version="
              << reader.GetInteger("protocol", "version", -1) << ", name="
              << reader.Get("user", "name", "UNKNOWN") << ", email="
              << reader.Get("user", "email", "UNKNOWN") << ", pi="
              << reader.GetReal("user", "pi", -1) << ", active="
              << reader.GetBoolean("user", "active", true) << "\n";
    return 0;
}
```

This simple C++ API works fine, but it's not very fully-fledged. I'm not planning to work more on the C++ API at the moment, so if you want a bit more power (for example `GetSections()` and `GetFields()` functions), see these forks:

  * https://github.com/Blandinium/inih
  * https://github.com/OSSystems/inih


## Differences from ConfigParser ##

Some differences between inih and Python's [ConfigParser](http://docs.python.org/library/configparser.html) standard library module:

* INI name=value pairs given above any section headers are treated as valid items with no section (section name is an empty string). In ConfigParser having no section is an error.
* Line continuations are handled with leading whitespace on continued lines (like ConfigParser). However, instead of concatenating continued lines together, they are treated as separate values for the same key (unlike ConfigParser).


## Platform-specific notes ##

* Windows/Win32 uses UTF-16 filenames natively, so to handle Unicode paths you need to call `_wfopen()` to open a file and then `ini_parse_file()` to parse it; inih does not include `wchar_t` or Unicode handling.

## Meson notes ##

* The `meson.build` file is not required to use or compile inih, its main purpose is for distributions.
* By default Meson only creates a static library for inih, but Meson can be used to configure this behavior:
* with `-Ddefault_library=shared` a shared library is build.
* with `-Ddistro_install=true` the library will be installed with the header and a pkg-config entry, you may want to set `-Ddefault_library=shared` when using this.
* with `-Dwith_INIReader` you can build (and install if selected) the C++ library.
* all compile-time options are implemented in Meson as well, you can take a look at [meson_options.txt](https://github.com/benhoyt/inih/blob/master/meson_options.txt) for their definition. These won't work if `distro_install` is set to `true`.
* If you want to use inih for programs which may be shipped in a distro, consider linking against the shared libraries. The pkg-config entries are `inih` and `INIReader`.
* In case you use inih as a subproject, you can use the `inih_dep` and `INIReader_dep` dependency variables.

## Building from vcpkg ##

You can build and install inih using [vcpkg](https://github.com/microsoft/vcpkg/) dependency manager:

    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    ./vcpkg integrate install
    ./vcpkg install inih

The inih port in vcpkg is kept up to date by microsoft team members and community contributors.
If the version is out of date, please [create an issue or pull request](https://github.com/Microsoft/vcpkg) on the vcpkg repository.

## Related links ##

* [Conan package for inih](https://github.com/mohamedghita/conan-inih) (Conan is a C/C++ package manager)
