#ifndef EXCEPTIONS_GAYANES_HPP
#define EXCEPTIONS_GAYANES_HPP
#include <stdexcept>
#include <string>

/* NotImplemented can be used when part of the emulator is not implemented yet */
class NotImplemented : public std::runtime_error
{
public:
    NotImplemented(const char* what) : runtime_error(what) {};
};

/* AddressFault can be used in case of incorrect address within the emulated cpu */
class AddressFault : public std::runtime_error
{
public:
    AddressFault(const char* what) : runtime_error(what) {};
};

class MemNotFound : public std::runtime_error
{
public:
    MemNotFound(const char* what) : runtime_error(what) {};
};

class CPUHalted : public std::runtime_error
{
public:
    CPUHalted(unsigned int pc): runtime_error(std::to_string(pc)) {};
};

/* When there is an error reading a file */
class FileReadingError : public std::runtime_error
{
public:
    FileReadingError(const char* what): runtime_error(what) {};
};

/* When a file has an incorrect format */
class IncorrectFileFormat : public std::runtime_error
{
public:
    IncorrectFileFormat(const char* what) : runtime_error(what) {};
};

/* When a memory allocation fails */
class MemAllocFailed : public std::runtime_error
{
public:
    MemAllocFailed(const char *what): runtime_error(what) {};
};

class PpuSync
{
public:
    PpuSync(){};
    ~PpuSync(){};
};

class ExitedGame
{
public:
    ExitedGame(){};
    ~ExitedGame(){};
};

#endif