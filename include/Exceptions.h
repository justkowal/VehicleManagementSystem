#pragma once

#include <stdexcept>
#include <string>
#include <cstdint>

class RentalSystemException : public std::runtime_error {
public:
    explicit RentalSystemException(const std::string& msg) : std::runtime_error(msg) {}
};

class StorageException : public RentalSystemException {
public:
    explicit StorageException(const std::string& msg) : RentalSystemException("Storage Error: " + msg) {}
};

class PrinterException : public RentalSystemException {
public:
    explicit PrinterException(const std::string& msg) : RentalSystemException("Printer Error: " + msg) {}
};

class VehicleNotFoundException : public RentalSystemException {
public:
    explicit VehicleNotFoundException(uint32_t vehicle_id) 
        : RentalSystemException("Vehicle with ID " + std::to_string(vehicle_id) + " not found") {}
};

class VehicleAlreadyExistsException : public RentalSystemException {
public:
    explicit VehicleAlreadyExistsException(uint32_t vehicle_id) 
        : RentalSystemException("Vehicle with ID " + std::to_string(vehicle_id) + " already exists") {}
};

class VehicleStatusException : public RentalSystemException {
public:
    explicit VehicleStatusException(const std::string& msg) : RentalSystemException(msg) {}
};

class ValidationException : public RentalSystemException {
public:
    explicit ValidationException(const std::string& msg) : RentalSystemException("Validation Error: " + msg) {}
};

