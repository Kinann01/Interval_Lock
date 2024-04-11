# Interval Lock Library

## Overview
This library provides a thread-safe mechanism for managing shared and exclusive locks on intervals. It uses an interval tree to efficiently handle overlapping intervals. The library offers two types of locks:
- `shared_lock`: Allows multiple locks on the same interval concurrently.
- `exclusive_lock`: Guarantees exclusive access to an interval.

## Features
- Thread safety using mutexes and condition variables.
- Efficient management of interval locks using an interval tree.
- Support for upgrading a shared lock to an exclusive lock and downgrading an exclusive lock to a shared lock.

## Prerequisites
- C++20 compliant compiler

## Tests
A simple test is provided in main.cpp. The test creates a few shared and exclusive locks on intervals and checks if the locks are acquired correctly. To run the test, compile the project and run the executable. There are other more complicated tests in the `tests` directory.

## Compilation
This project requires the use of C++20 features. Make sure your compiler supports the `-std=c++20` flag. For example, you can compile the project with the following command:

```sh
g++ -std=c++20 src/main.cpp
```
Then run the executable:

```sh
./a.out
```