# Parallel-Banking-System



## Overview  
This project implements a multi-threaded banking system in **C++**, strictly using **POSIX Threads (pthreads)** for concurrency. It supports multiple **ATMs**, each processing banking transactions in parallel while ensuring synchronization and consistency in account operations.

## Features  
- Concurrent execution of ATM transactions  
- Secure account operations: deposit, withdrawal, transfer, balance inquiry  
- Periodic commission deduction for bank revenue  
- VIP transaction processing with priority handling  
- Bank rollback mechanism for recovery  

## Installation  
Ensure you have a **C++ compiler** with **pthreads** support. Compile using:  
```sh
g++ -std=c++11 -Wall -Werror -pedantic-errors -DNDEBUG -pthread *.cpp -o bank
```
Alternatively, use the provided `Makefile`:
```sh
make
```

## Usage  
Run the program with ATM input files:  
```sh
./bank <number of VIP threads> <ATM input file 1> <ATM input file 2> ...
```
Example:  
```sh
./bank 2 transactions1.txt transactions2.txt
```

## Logs  
The system maintains a **log file (log.txt)** capturing transaction events, errors, and commission updates.

