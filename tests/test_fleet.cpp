#include <catch2/catch_test_macros.hpp>
#include "FleetMgmt.h"
#include "MockSystem.h"
#include "Exceptions.h"
#include "EscPosPrinter.h"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

TEST_CASE("FleetManager Domain Logic", "[fleet]") {
    auto storage = std::make_unique<MockStorage>();
    auto printer = std::make_unique<MockPrinter>();
    
    auto* raw_storage = storage.get();
    auto* raw_printer = printer.get();
    
    FleetManager manager(std::move(storage), std::move(printer));

    Car test_car{101, "Toyota", "Yaris", 5, 1500, VehicleStatus::Available};

    SECTION("Adding a vehicle saves it to storage immediately") {
        manager.addVehicle(Vehicle(test_car));
        
        REQUIRE(manager.getFleet().size() == 1);
        REQUIRE(raw_storage->fake_db.size() == 1);
        REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Available);
    }

    SECTION("Renting a vehicle updates state and prints a receipt") {
        manager.addVehicle(Vehicle(test_car));
        
        auto rental_code = manager.rentVehicle(101, "John", "Doe", "AB123456");
        
        REQUIRE(rental_code.has_value() == true);
        REQUIRE(rental_code->substr(0, 5) == "RENT-");
        
        REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Rented);
        REQUIRE(raw_storage->fake_db[0].getStatus() == VehicleStatus::Rented);
        
        REQUIRE(raw_printer->checkout_prints == 1);
        REQUIRE(raw_storage->fake_records.size() == 1);
    }

    SECTION("Cannot rent an already rented vehicle") {
        manager.addVehicle(Vehicle(test_car));
        manager.rentVehicle(101, "John", "Doe", "AB123456");
        
        REQUIRE_THROWS_AS(manager.rentVehicle(101, "John", "Doe", "AB123456"), VehicleStatusException);
        REQUIRE(raw_printer->checkout_prints == 1);
    }

    SECTION("Returning a vehicle calculates costs and updates state") {
        manager.addVehicle(Vehicle(test_car));
        auto code = manager.rentVehicle(101, "John", "Doe", "AB123456");
        
        bool return_success = manager.returnVehicle(code.value());
        
        REQUIRE(return_success == true);
        REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Available);
        REQUIRE(raw_printer->return_prints == 1);
    }

    SECTION("Returning with an invalid code fails gracefully") {
        manager.addVehicle(Vehicle(test_car));
        manager.rentVehicle(101, "John", "Doe", "AB123456");
        
        REQUIRE_THROWS_AS(manager.returnVehicle("RENT-NONEXS"), VehicleStatusException);
        REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Rented);
        REQUIRE(raw_printer->return_prints == 0);
    }

    SECTION("Decommissioning an available vehicle removes it and deletes records") {
        manager.addVehicle(Vehicle(test_car));
        Record rec{101, std::chrono::system_clock::now(), RecordType::Checkout, "Some details"};
        raw_storage->appendRecord(rec);

        REQUIRE(raw_storage->fake_records.size() == 1);

        bool success = manager.removeVehicle(101);
        
        REQUIRE(success == true);
        REQUIRE(manager.getFleet().empty() == true);
        REQUIRE(manager.getVehicle(101).has_value() == false);
        REQUIRE(raw_storage->fake_records.empty() == true);
    }

    SECTION("Decommissioning a rented vehicle is blocked") {
        manager.addVehicle(Vehicle(test_car));
        auto rental_code = manager.rentVehicle(101, "John", "Doe", "AB123456");
        REQUIRE(rental_code.has_value() == true);

        REQUIRE_THROWS_AS(manager.removeVehicle(101), VehicleStatusException);
        REQUIRE(manager.getFleet().size() == 1);
        REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Rented);
    }
}

TEST_CASE("EscPosPrinter barcode generation and receipt content", "[printer]") {
    std::string test_file = "test_printer_output.bin";
    std::remove(test_file.c_str());

    {
        EscPosPrinter printer(test_file);
        std::tm tm_struct{};
        tm_struct.tm_year = 126; 
        tm_struct.tm_mon = 5;    
        tm_struct.tm_mday = 11;
        tm_struct.tm_hour = 22;
        tm_struct.tm_min = 30;
        tm_struct.tm_sec = 15;
        tm_struct.tm_isdst = -1;
        std::time_t raw_time = std::mktime(&tm_struct);
        auto time_point = std::chrono::system_clock::from_time_t(raw_time);

        printer.printCheckout("Tesla Model S", "RENT-ABCD12", "Jan", "Kowalski", "XYZ123456", time_point);
    }

    std::ifstream file(test_file, std::ios::binary);
    REQUIRE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    std::remove(test_file.c_str());

    REQUIRE(content.find("Najemca: Jan Kowalski\n") != std::string::npos);
    REQUIRE(content.find("ID: XYZ123456\n") != std::string::npos);
    REQUIRE(content.find("Data: 2026-06-11 22:30:15\n") != std::string::npos);

    std::string expected_disable_hri = std::string() + char(0x1D) + char(0x48) + char(0x00);
    REQUIRE(content.find(expected_disable_hri) != std::string::npos);

    std::string expected_barcode = std::string() + char(0x1D) + char(0x6B) + char(69) + char(11) + "RENT-ABCD12";
    REQUIRE(content.find(expected_barcode) != std::string::npos);

    size_t text_pos = content.find("RENT-ABCD12");
    REQUIRE(text_pos != std::string::npos);
    size_t barcode_pos = content.find(expected_barcode);
    REQUIRE(barcode_pos != std::string::npos);
    REQUIRE(barcode_pos > text_pos);
}

static void run_mock_printer_server(uint16_t port, std::string& out_received) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        close(server_fd);
        return;
    }

    if (listen(server_fd, 1) < 0) {
        close(server_fd);
        return;
    }

    socklen_t addrlen = sizeof(address);
    int new_socket = accept(server_fd, reinterpret_cast<struct sockaddr*>(&address), &addrlen);
    if (new_socket >= 0) {
        char buffer[4096] = {0};
        ssize_t valread = read(new_socket, buffer, sizeof(buffer) - 1);
        if (valread > 0) {
            out_received = std::string(buffer, static_cast<size_t>(valread));
        }
        close(new_socket);
    }
    close(server_fd);
}

TEST_CASE("EscPosPrinter network transmission and socket mode", "[printer][network]") {
    std::string received;
    uint16_t port = 59123;
    
    std::thread server_thread(run_mock_printer_server, port, std::ref(received));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        EscPosPrinter printer("127.0.0.1:59123");
        printer.printCheckout("Ducati Monster", "RENT-MOCK99");
    }

    server_thread.join();

    REQUIRE(received.empty() == false);
    REQUIRE(received.find("Ducati Monster") != std::string::npos);
    REQUIRE(received.find("RENT-MOCK99") != std::string::npos);

    std::string return_received;
    std::thread return_server_thread(run_mock_printer_server, port, std::ref(return_received));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        EscPosPrinter printer("127.0.0.1:59123");
        printer.printReturn("Ducati Monster", 2500, 3.0, 7500);
    }

    return_server_thread.join();

    REQUIRE(return_received.empty() == false);
    REQUIRE(return_received.find("Ducati Monster") != std::string::npos);
    REQUIRE(return_received.find("DO ZAPLATY") != std::string::npos);

    SECTION("Connecting to offline printer throws PrinterException") {
        EscPosPrinter printer("127.0.0.1:59999");
        REQUIRE_THROWS_AS(printer.printCheckout("Test", "RENT-123"), PrinterException);
    }
}

TEST_CASE("FleetManager searchFleet across all fields", "[fleet][search]") {
    auto storage = std::make_unique<MockStorage>();
    auto printer = std::make_unique<MockPrinter>();
    FleetManager manager(std::move(storage), std::move(printer));

    Car car{101, "Toyota", "Yaris", 5, 1500, VehicleStatus::Available};
    Bike bike{102, "Giant", "Road Bike", 850, VehicleStatus::Rented};
    Truck truck{103, "Volvo", "FH16", 20000, 4500, VehicleStatus::Maintenance};

    manager.addVehicle(Vehicle(car));
    manager.addVehicle(Vehicle(bike));
    manager.addVehicle(Vehicle(truck));

    auto search_func = [&](const std::string& query) {
        std::string search_query = query;
        std::transform(search_query.begin(), search_query.end(), search_query.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        return manager.searchFleet([&](const Vehicle& veh) {
            auto matches = [&](const std::string& field_val) {
                std::string val_lower = field_val;
                std::transform(val_lower.begin(), val_lower.end(), val_lower.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return val_lower.find(search_query) != std::string::npos;
            };

            if (matches(std::to_string(veh.getId()))) return true;
            if (matches(veh.getBrand())) return true;
            if (matches(veh.getModelOrType())) return true;
            if (matches(veh.getTypeString())) return true;
            if (matches(veh.getStatusString())) return true;

            std::ostringstream rate_ss;
            int rate = veh.getPricePerHour();
            rate_ss << (rate / 100) << "." << std::setw(2) << std::setfill('0') << std::abs(rate % 100);
            if (matches(rate_ss.str())) return true;

            std::string specs_text;
            std::visit(
                [&](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, Car>) {
                        specs_text = "Capacity: " + std::to_string(item.seats) + " seats";
                    } else if constexpr (std::is_same_v<T, Truck>) {
                        specs_text =
                            "Payload capacity: " + std::to_string(item.payload_capacity_kg) + " kg";
                    } else {
                        specs_text = "Standard road category model";
                    }
                },
                veh.getVariant());
            if (matches(specs_text)) return true;

            return false;
        });
    };

    SECTION("Search by ID") {
        auto res = search_func("102");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 102);
    }

    SECTION("Search by Brand (case-insensitive)") {
        auto res = search_func("toYoTa");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 101);
    }

    SECTION("Search by Model/Type") {
        auto res = search_func("road");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 102);
    }

    SECTION("Search by TypeString") {
        auto res = search_func("truck");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 103);
    }

    SECTION("Search by StatusString") {
        auto res = search_func("rented");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 102);
    }

    SECTION("Search by Price Per Hour") {
        auto res = search_func("15.00");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 101);
    }

    SECTION("Search by Specific Spec (Seats)") {
        auto res = search_func("seats");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 101);
    }

    SECTION("Search by Specific Spec (Payload)") {
        auto res = search_func("20000");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 103);
    }
}

TEST_CASE("FleetManager searchFleet with AdvancedFilter", "[fleet][search][advanced]") {
    auto storage = std::make_unique<MockStorage>();
    auto printer = std::make_unique<MockPrinter>();
    FleetManager manager(std::move(storage), std::move(printer));

    Car car1{101, "Toyota", "Yaris", 5, 1500, VehicleStatus::Available}; 
    Car car2{102, "Honda", "Civic", 4, 2500, VehicleStatus::Rented};    
    Truck truck{103, "Volvo", "FH16", 20000, 4500, VehicleStatus::Available}; 
    Bike bike{104, "Giant", "Road Bike", 800, VehicleStatus::Available};   

    manager.addVehicle(Vehicle(car1));
    manager.addVehicle(Vehicle(car2));
    manager.addVehicle(Vehicle(truck));
    manager.addVehicle(Vehicle(bike));

    SECTION("Filter ID: Min and Max range") {
        AdvancedFilter filter;
        filter.id.min_val = "102";
        filter.id.max_val = "103";
        auto res = manager.searchFleet([](const Vehicle&) { return true; }, filter);
        REQUIRE(res.size() == 2);
        REQUIRE(res[0].getId() == 102);
        REQUIRE(res[1].getId() == 103);
    }

    SECTION("Filter Price: Min and Max range") {
        AdvancedFilter filter;
        filter.price.min_val = "10.00";
        filter.price.max_val = "30.00";
        auto res = manager.searchFleet([](const Vehicle&) { return true; }, filter);
        REQUIRE(res.size() == 2);
        REQUIRE(res[0].getId() == 101);
        REQUIRE(res[1].getId() == 102);
    }

    SECTION("Filter Brand: Equals requirement") {
        AdvancedFilter filter;
        filter.brand.equals_val = "Volvo";
        auto res = manager.searchFleet([](const Vehicle&) { return true; }, filter);
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 103);
    }

    SECTION("Filter Seats: min and max range for Cars") {
        AdvancedFilter filter;
        filter.seats.min_val = "5";
        auto res = manager.searchFleet([](const Vehicle&) { return true; }, filter);
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 101);
    }

    SECTION("Filter Payload: min range for Trucks") {
        AdvancedFilter filter;
        filter.payload.min_val = "15000";
        auto res = manager.searchFleet([](const Vehicle&) { return true; }, filter);
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].getId() == 103);
    }

    SECTION("Inactive filters return everything") {
        AdvancedFilter filter;
        auto res = manager.searchFleet([](const Vehicle&) { return true; }, filter);
        REQUIRE(res.size() == 4);
    }
}