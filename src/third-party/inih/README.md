**inih (INI Not Invented Here)** is a simple [.INI file](http://en.wikipedia.org/wiki/INI_file) parser written in C. It's only a couple of pages of code, and it was designed to be _small and simple_, so it's good for embedded systems. It's also more or less compatible with Python's [ConfigParser](http://docs.python.org/library/configparser.html) style of .INI files, including RFC 822-style multi-line syntax and `name: value` entries.

To use it, just give `ini_parse()` an INI file, and it will call a callback for every `name=value` pair parsed, giving you strings for the section, name, and value. It's done this way ("SAX style") because it works well on low-memory embedded systems, but also because it makes for a KISS implementation.

You can also call `ini_parse_file()` to parse directly from a `FILE*` object, or `ini_parse_stream()` to parse using a custom reader to implement string-based or other custom I/O ([see example code](https://github.com/benhoyt/inih/blob/master/examples/ini_buffer.c)).

Download a release, browse the source, or read about [how to use inih in a DRY style](http://blog.brush.co.nz/2009/08/xmacros/) with X-Macros.


## Compile-time options ##

  * **Multi-line entries:** By default, inih supports multi-line entries in the style of Python's ConfigParser. To disable, add `-DINI_ALLOW_MULTILINE=0`.
  * **UTF-8 BOM:** By default, inih allows a UTF-8 BOM sequence (0xEF 0xBB 0xBF) at the start of INI files. To disable, add `-DINI_ALLOW_BOM=0`.
  * **Stack vs heap:** By default, inih allocates its line buffer on the stack. To allocate on the heap using `malloc` instead, specify `-DINI_USE_STACK=0`.
  * **Stop on first error:** By default, inih keeps parsing the rest of the file after an error. To stop parsing on the first error, add `-DINI_STOP_ON_FIRST_ERROR=1`.
  * **Maximum line length:** The default maximum line length is 200 bytes. To override this, add something like `-DINI_MAX_LINE=1000`.


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
