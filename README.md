# 🌱 SeedTopia  
SeedTopia is a **Growtopia-inspired MMO game** built entirely from scratch in **modern C++23**.  
It combines a custom networking layer, secure database management, and an optimized UI system to create a scalable multiplayer experience.  

---

## 🛠 Features  
- **Main & Sub Servers**  
  - 1 Main UDP Server  
  - 4 Sub UDP Servers  

- **Security First**  
  - High-security packet manager  
  - Strong and safe database integration  

- **Performance Oriented**  
  - Optimized C++ functions and variables  
  - Designed for scalability and stability  

---

## 🚀 Tech Stack  

- **Language:** C++23 (Visual Studio 2022 Community)  
- **UI Library:** [SFML](https://www.sfml-dev.org/)  
- **Networking:** [ENet](http://enet.bespin.org/) (UDP)  
- **Database:** SQLite (via SQLite Modern C++ wrapper)  
- **Compiler Standards:**  
  - C++23 (`/std:c++23preview`)  
  - C17 (`/std:c17`)  
  - Target: `x64/Release`

---

## 🎮 Roadmap  

- [x] Configurations between main and subservers.
- [x] Login and Register system.
- [ ] Mail 2FA Confirmation system.
- [ ] Forgot Password system.

--- 

## Getting Started

### 1. Install Git
Download and install Git:  
https://github.com/git-for-windows/git/releases/download/v2.50.1.windows.1/Git-2.50.1-64-bit.exe  

### 2. Setup VCPKG & Dependencies
Open **CMD** and run:
```
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.bootstrap-vcpkg.bat
.vcpkg integrate install
.vcpkg install enet:x64-windows
.vcpkg install fmt:x64-windows
.vcpkg install sfml:x64-windows
.vcpkg install sqlite-modern-cpp:x64-windows
```

### 3. Configure in Visual Studio
- Go to Project → Properties  
- Select Configuration Properties → vcpkg  
- Set Use Vcpkg to "Yes"  

## Building the Game
1. Open the project in **Visual Studio Community**  
2. Build the solution (`Ctrl + Shift + B`)  
3. Run the executables from (`x64/Release`)
4. `Server.exe` and then `Client.exe(s)`

## License
This project is licensed under the **MIT License** – see the LICENSE file for details.  

## Contributing
Pull requests are welcome! Feel free to open issues for bugs, feature requests, or ideas.  

## Credits
- C++ – Core language  
- MySQL – High-optimised database library
- ENet – Low-level networking library  
- SFML - User Interface Library
