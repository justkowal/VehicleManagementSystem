# Vehicle Management System

## Overview

The Vehicle Management System is a C++ app built with a big focus on decoupling, thread safety and modularity. Instead of using standard terminal interfaces that mix domain logic with console output, this project brings in `notui` - a custom abstract layout rendering framework built on top of the `notcurses` library.

This separation strictly isolates the core business logic from the mess of terminal I/O. By treating the UI as its own separate rendering target, the app is much easier to test and maintain, and its open to future extensions without breaking the underlying state management.

## Key Architectural Highlights

### Custom UI Engine (notui)
Instead of hardcoding UI coordinates and doing manual redraws, the project uses a component-driven terminal rendering tree. It uses declarative layout managers like `VBox`, `HBox`, and `SplitBox` to automatically handle terminal resizing and widget placement. Also, an interactive state machine (`FocusManager`) safely passes keyboard and mouse events down the active widget tree, preventing messy coupling between the input loop and the domain models.

### Transactional State & Exception Safety
Since data integrity is super important in management software, all mutating operations in the system follow the Strong Exception Guarantee. When a user or system process modifies a record, the operations are done on local memory copies first. The system writes these changes out to disk before committing them to active memory. If any part of the operation fails, the system rolls back cleanly without corrupting the live app state.

### Concurrent Optimization
To make sure the UI stays responsive during background tasks or large data queries, the system uses a C++17 multiple-readers, single-writer concurrency pattern with `std::shared_mutex`. This lets non-blocking, parallel UI render sweeps read fleet data concurrently. At the same time it safely blocks and queues mutations, ensuring thread safe reads without the usual performance bottlenecks of exclusive locking.

### Runtime Dynamic Plugins
The architecture avoids a huge monolithic binary by using a decoupled Abstract Factory and Service Locator pattern. The system actively scans its directories and can hot-swap dynamic libraries (`.so` MODULE targets) at runtime. This means new vehicle types, logging sinks, or reporting modules can be injected and used by the app without requiring a full re-compilation or re-linking of the main core.

## Dev-Ops & Build Infrastructure

The project is wrapped in a solid DevOps ecosystem to ensure reliable builds and deployments.

- Modern Build Orchestration - Uses native CMake Workflow Presets (Version 6) to provide a unified, reproducible build pipeline.
- Continuous Integration - Backed by GitHub Actions workflows that handle continuous integration, rigorous thread stress-testing, and automated Release packaging when you tag commits.
- Automated Distribution - Uses CPack configuration, enabling effortless compilation and packaging into standalone installers (like DEB or RPM) for end users. There is no manual steps needed.

## Getting Started

Building and testing the app is heavily standardized through the provided CMake presets.

To configure, build, and locally install the main app run
```bash
cmake --workflow --preset install-app
```

If your developing dynamic plugins and want to hot-compile them while the main app is running
```bash
cmake --workflow --preset install-plugin
```

To run the unit tests and verify the concurrency guarantees
```bash
./build/bin/UnitTests
```
