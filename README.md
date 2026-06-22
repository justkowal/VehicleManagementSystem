<p align="center">
  <a href="https://github.com/justkowal/VehicleManagementSystem/actions/workflows/ci.yml">
    <img src="https://github.com/justkowal/VehicleManagementSystem/actions/workflows/ci.yml/badge.svg" alt="CI Status"/>
  </a>
  <a href="https://github.com/justkowal/VehicleManagementSystem/actions/workflows/stress.yml">
    <img src="https://github.com/justkowal/VehicleManagementSystem/actions/workflows/stress.yml/badge.svg" alt="Stress Tests"/>
  </a>
  <a href="https://github.com/justkowal/VehicleManagementSystem/actions/workflows/release.yml">
    <img src="https://github.com/justkowal/VehicleManagementSystem/actions/workflows/release.yml/badge.svg" alt="Release CD"/>
  </a>
  <img src="https://img.shields.io/github/v/release/justkowal/VehicleManagementSystem?color=9400D3&style=flat-square" alt="Latest Release"/>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C%2B%2B20-00599C?style=flat-square&logo=cplusplus&logoColor=white" alt="C++20"/>
  <img src="https://img.shields.io/badge/Build%20System-CMake%20Presets-064F8C?style=flat-square&logo=cmake&logoColor=white" alt="CMake Presets"/>
  <img src="https://img.shields.io/badge/Static%20Analysis-Clang--Tidy-8F05F5?style=flat-square&logo=llvm" alt="Clang-Tidy"/>
  <img src="https://img.shields.io/badge/Code%20Style-Clang--Format-8F05F5?style=flat-square&logo=llvm" alt="Clang-Format"/>
  <img src="https://img.shields.io/badge/Test%20Harness-Catch2-orange?style=flat-square" alt="Catch2"/>
  <img src="https://img.shields.io/badge/Packages-DEB%20%7C%20RPM%20%7C%20TGZ-brightgreen?style=flat-square&logo=cmake" alt="CPack"/>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Lines%20of%20Code-9322-blueviolet?style=flat-square" alt="Lines of Code"/>
  <img src="https://img.shields.io/github/repo-size/justkowal/VehicleManagementSystem?style=flat-square&color=orange" alt="Repo Size"/>
  <img src="https://img.shields.io/github/last-commit/justkowal/VehicleManagementSystem?style=flat-square&color=brightgreen" alt="Last Commit"/>
  <img src="https://img.shields.io/badge/AGH%20University-PL--II%20Project-7D2232?style=flat-square" alt="AGH University Project"/>
</p>

---

# Vehicle Management System

## Overview

The Vehicle Management System is a C++20 application designed with a focus on separation of concerns, thread safety, and modularity. Instead of standard console interfaces that bundle business logic with text output, this system uses `notui` - a custom terminal layout engine built on top of the `notcurses` library.

This design decouples core fleet management logic from terminal I/O, improving testability and facilitating future interface extensions.

## System Architecture

```mermaid
graph LR
    classDef default fill:#1e1e24,stroke:#333,stroke-width:1px,color:#fff;
    classDef primary fill:#2a4494,stroke:#4a6cd4,stroke-width:2px,color:#fff;
    classDef interface fill:#b56a1b,stroke:#e69c24,stroke-width:1.5px,color:#fff;
    classDef plugin fill:#4a2c5c,stroke:#8f3bb8,stroke-width:1.5px,color:#fff;
    classDef ext fill:#1b4d3e,stroke:#2d8a68,stroke-width:1px,color:#fff;

    subgraph UserInterface ["Presentation Layer (UI/Render Thread)"]
        UI[UI Module / Main Loop]:::primary
        notui[notui Layout Engine]:::primary
        notcurses[notcurses Lib]:::ext
    end

    subgraph CoreEngine ["Core Business Logic (Worker/Logic Thread)"]
        FM[FleetManager]:::default
        VEH[Vehicle Variant Models]:::default
    end

    subgraph Abstractions ["Abstract Interface Registry Layer"]
        IS[IStorage Interface]:::interface
        IP[IPrinter Interface]:::interface
        SR[StorageRegistry]:::default
        PR[PrinterRegistry]:::default
    end

    subgraph Adapters ["Adapters & Implementations"]
        subgraph Static ["Static (Compiled-In)"]
            FS[FileStorage]:::default
            EP[EscPosPrinter]:::default
        end
        subgraph Dynamic ["Dynamic (Runtime Plugins)"]
            DSP[Storage Plugins .so]:::plugin
            DPP[Printer Plugins .so]:::plugin
        end
    end

    UI -->|Manages layout via| notui
    notui -->|Draws on| notcurses
    
    UI <-->|"Interacts Across Threads (State & Events)"| FM
    FM -->|Manages| VEH
    
    FM -->|State Persistence via| IS
    FM -->|Receipt Printing via| IP
    
    IS -.->|Resolved by| SR
    IP -.->|Resolved by| PR
    
    IS <---|Implements| FS
    IP <---|Implements| EP
    
    SR <-.-|Registers dynamically to| DSP
    PR <-.-|Registers dynamically to| DPP

    class UI,notui primary;
    class IS,IP interface;
    class DSP,DPP plugin;
    class notcurses ext;
```

## Key Architectural Features

### Declarative UI (notui)
Uses a tree of UI components layout-managed via `VBox`, `HBox`, and `SplitBox` to automatically handle terminal resizing. User inputs (keyboard and mouse) are handled by a `FocusManager` that dispatches events down the active widget tree.

### Exception Safety & Transactional State
Ensures data integrity via strong exception guarantees. Mutating operations are performed on local memory copies and successfully persisted to disk before committing to active memory, rolling back cleanly upon failure.

### Concurrency Model
Maintains a responsive UI by using a readers-writer lock (`std::shared_mutex`). UI rendering threads read fleet data concurrently, while mutations are blocked and queued to ensure thread safety without unnecessary bottlenecks.

### Dynamic Plugin System
Supports runtime extensibility using a Service Locator pattern. The system scans designated folders to dynamically load and register dynamic storage or printing implementations (`.so` targets) without needing to rebuild the core application.

## Build & CI/CD

- **CMake Presets** - Native CMake Workflow Presets provide standard configuration, build, and installation tasks.
- **Reproducible Environments** - Includes a `flake.nix` (Nix Flake) pinning the required C++ toolchain, libraries (e.g., `notcurses`) and development utilities (e.g. `escpresso`, `kitty`).
- **GitHub Actions** - CI workflows run builds, static analysis (Clang-Tidy), formatting checks (Clang-Format), thread stress tests, and automate CD release packaging.
- **Distribution Packages** - Integrated CPack configuration packages the compiled application into DEB, RPM, or TGZ formats.

## Getting Started

### 0. Nix Development Environment (Recommended)
To run within the pinned dev shell containing the complete C++20 toolchain (GCC, CMake, `notcurses`, `clang-tools`, and GPU-dependent utilities like `escpresso` or `kitty`):
```bash
nix develop --impure
```
*Note: The `--impure` flag is required because of GPU bridging/nixGL runtime detection. The included `escpresso` utility can be used to view and verify ESC/POS printer output.*

### 1. Build and Install the App
To configure, build, and install the application using standard CMake presets:
```bash
cmake --workflow --preset install-app
```

### 2. Build and Install Plugins (Optional)
To build and install the dynamic library plugins:
```bash
cmake --workflow --preset install-plugin
```

### 3. Run Tests
To run unit tests (including concurrency checks):
```bash
./build/bin/UnitTests
```
