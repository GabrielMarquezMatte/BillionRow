# Billion Row CSV Challenge

## Overview

This repository is a personal challenge to read and process 1 billion rows from a CSV file as quickly as possible using C++. The project leverages modern C++ features and libraries to achieve high performance.

## Features

- **High-Performance Parsing**: Uses memory-mapped file IO for efficient file reading.
- **Multi-threading**: Utilizes multi-threading to process CSV rows in parallel.
- **Custom Hashing**: Implements a custom hash function for optimized unordered map usage.

## Requirements

- **C++ Compiler**: Requires a C++23 compatible compiler.
- **CMake**: Version 3.28.1 or higher.
- **vcpkg**: For managing dependencies.

## Dependencies

This project uses the following libraries managed via `vcpkg`:

- [mio](https://github.com/mandreyel/mio)
- [unordered_dense](https://github.com/martinus/unordered_dense)
- [indicators](https://github.com/p-ranav/indicators)

## Building the Project

### Build Instructions

1. Clone the repository with submodules:

    ```bash
    git clone --recurse-submodules https://github.com/yourusername/BillionRowChallenge.git
    cd BillionRowChallenge
    ```

2. Create a build directory:

    ```bash
    mkdir build
    cd build
    ```

3. Configure the project with CMake:

    ```bash
    cmake ..
    ```

4. Build the project:

    ```bash
    cmake --build .
    ```

There is no need to bootstrap vcpkg, once the cmake already detects the vcpkg submodule and automatically executes the bootstrap process.

## Running the Application

### Generating the Dataset

To generate a large CSV dataset, use the provided Python script. This script will create a file named `data.csv` in the `data` directory within the project root.

1. Ensure you have Python installed on your system.
2. Run the following command to generate the dataset:

    ```bash
    python src/create_dataset.py 1000000000
    ```

   Replace `1000000000` with the desired number of records.

### Running the C++ Application

After generating the dataset, you can run the C++ application:

1. Ensure that the generated `data.csv` file is in the `data` directory.
2. Run the application:

    ```bash
    ./AggregateData
    ```

The application will read the CSV, process it, and output the results to `output.csv` in the `data` directory.

## Performance Considerations

The application is designed to maximize performance through:

- **Memory-mapped File IO**: Efficiently handles large files by mapping them into memory.
- **Parallel Processing**: Splits the workload across multiple threads, leveraging all available CPU cores.
- **Optimized Data Structures**: Uses `ankerl::unordered_dense::map` with a custom hash function for fast lookups and inserts.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contact

For any questions or suggestions, feel free to open an issue or contact me at <gabrielandremarquez.matte@gmail.com>.
