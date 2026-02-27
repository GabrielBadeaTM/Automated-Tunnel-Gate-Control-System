# 🚦 Tunnel Traffic Monitoring & Management System

A real-time embedded system that simulates dynamic traffic control and hazard management inside a two-way road tunnel.
Built with simulated environmental sensors, automated barriers, and a priority-based safety logic, all managed by an Arduino Mega running FreeRTOS.
Developed as part of a university project in a 3-person team — combining hardware integration, real-time operating system concepts (tasks, mutexes, semaphores), and teamwork.

## 🛠️ Technologies Used

- Arduino MEGA 2560
- FreeRTOS (Real-Time Operating System)
- Potentiometers (Simulating natural gas and smoke sensors)
- Push Buttons (Simulating vehicle entry/exit sensors and a priority Panic Button)
- LEDs (Simulating tunnel barriers and visual feedback)
- C++ / Arduino IDE

## ⚙️ How It Works

1. **Traffic Tracking:** The system counts the number of cars entering and exiting on both directions, with a strict limit of 10 vehicles per lane.
2. **Environmental Monitoring:** Simulated sensors continuously read gas and smoke levels.
3. **Automated Safety:** If the tunnel reaches maximum capacity, or if dangerous levels of gas or smoke are detected, the system automatically lowers the entry barriers (turns on the red LEDs) to block new vehicles while allowing internal cars to exit.
4. **Emergency Lockdown:** A physical "Panic Button" triggers a high-priority hardware interrupt, instantly locking down all entries and exits regardless of traffic or automated logic.
5. **Manual Override:** A human operator can take full control of the system via a serial terminal interface, bypassing all automated rules to manage the barriers manually during special interventions.

The project focuses on deterministic real-time execution, avoiding race conditions using Mutexes, and efficiently handling asynchronous events via Binary Semaphores and Interrupt Service Routines (ISRs).

## 🤝 Team

Developed by **Team Awsome**:

- [@GabrielBadeaTM](https://github.com/GabrielBadeaTM)
- Buterez Daniela-Georgiana
- Ionașcu Vlad-Mihai

Developed during the Real-Time Application Programming (PATR) course at the Faculty of Automatic Control and Computers (ACS), University Politehnica of Bucharest (UPB) -- Semester I, 2025-2026.

---

📄 Many more technical details, RTOS task organigrams, and wiring descriptions are available in the project documentation.
