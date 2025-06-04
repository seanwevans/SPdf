# SPdf

SPdf is a lightweight, custom implementation for managing structured streams of data in a document-like format. It enables efficient serialization, deserialization, and management of multiple data streams within a single SPDF file, mimicking features of PDF structures but with simplified and flexible handling.

## Features
- **Stream-based Document Management**: Supports multiple data streams with unique IDs, timestamps, and metadata.
- **Serialization and Deserialization**: Save and load SPDF documents with automatic stream handling.
- **Cross-Platform**: Written in C and C++ for performance and compatibility.
- **Thread-Safe Operations**: Utilizes mutex locks for thread safety during stream manipulation.
- **Unique Stream IDs**: Automatic generation of UUIDs for data stream identification.
- **Buffer Duplication**: `create_stream` copies the caller's data into an internal
  buffer, leaving ownership with the caller.

## Project Structure
```
spdf.h      // C API definitions
spdf.hpp    // C++ API definitions
spdf.c      // C implementation
spdf.cpp    // C++ implementation
main.c      // C demo application
main.cpp    // C++ demo application
```

## Installation
To compile the project, ensure you have `gcc` and `g++` installed.

### Compile C Version
```bash
gcc main.c spdf.c -o spdf_c -lpthread
```

### Compile C++ Version
```bash
g++ main.cpp spdf.cpp -o spdf_cpp
```

## Usage

### Running C Version
```bash
./spdf_c
```

### Running C++ Version
```bash
./spdf_cpp
```

### Example
The C++ version allows easy addition and management of data streams:
```cpp
SPDF document;
document.addStream(
    "UTF-8", "text/plain", "None", {0.0, 0.0},
    {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'});

document.print();
```

