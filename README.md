# 🤖 Intelligent Robot Path Planning and Dynamic Obstacle Avoidance System

A C++17 and SFML 3.x robotics simulation that demonstrates intelligent robot navigation using **A\*** and **Dijkstra's algorithm**, with static obstacle handling, dynamic obstacle avoidance, automatic path replanning, and real-time visualization.

## 📌 Project Overview

This project was developed as part of my **C++ programming coursework** to demonstrate the practical application of graph algorithms, object-oriented programming, path planning, and simulation.

The system simulates a robot moving through a grid-based environment while avoiding obstacles and recalculating its route when the planned path becomes blocked.

## ✨ Features

- 🧭 A* pathfinding algorithm
- 📊 Dijkstra algorithm comparison
- 🧱 Static obstacle placement
- 🔄 Dynamic obstacle movement
- 🤖 Autonomous robot navigation
- ♻️ Automatic path replanning
- 📈 Path and performance statistics
- 🧠 Step-by-step A* algorithm explanation
- 🎯 Multiple predefined simulation scenarios
- 🧪 C++ unit and integration tests
- 🖥️ Interactive SFML desktop interface

## 🛠️ Technologies Used

- **C++17**
- **SFML 3.x**
- **CMake**
- **MinGW / GCC**
- **Git & GitHub**

## 🧠 Algorithms

### A* Algorithm

The project uses the A* search algorithm:

`f(n) = g(n) + h(n)`

where:

- `g(n)` = cost from the starting node to the current node
- `h(n)` = estimated cost from the current node to the goal
- `f(n)` = total estimated cost

### Dijkstra's Algorithm

Dijkstra's algorithm is implemented separately to provide a comparison with A* in terms of pathfinding behaviour and efficiency.

## 🚧 Dynamic Obstacle Avoidance

When a dynamic or newly placed obstacle blocks the current route:

1. The robot detects the blocked path.
2. The current route is invalidated.
3. A* recalculates a new path.
4. The robot continues navigation using the updated route.

## 📁 Project Structure

```text
RobotPathPlanning/
├── assets/                 # Fonts and application assets
├── include/                # C++ header files
├── src/                    # C++ source files
├── scenarios/              # Predefined simulation scenarios
├── tests/                  # Unit and integration tests
├── release/                # Ready-to-run Windows version
│   └── RobotPathPlanning/
│       ├── RobotPathPlanning.exe
│       ├── Run.bat
│       ├── assets/
│       ├── scenarios/
│       └── required DLLs
├── CMakeLists.txt          # CMake build configuration
├── build_windows.bat       # Windows build helper
├── package_release.ps1     # Release packaging script
└── README.md
```

## ▶️ How to Run

### Option 1: Ready-to-Run Windows Version

No Visual Studio or VS Code is required for the packaged version.

1. Open the **Releases** section of this GitHub repository.
2. Download the latest `RobotPathPlanning` release ZIP.
3. Extract the ZIP.
4. Open the `RobotPathPlanning` folder.
5. Double-click **`Run.bat`**.
6. The application will launch.

### Option 2: Build from Source

Requirements:

- Windows 10/11
- CMake 3.22+
- MinGW-w64 / GCC 13+ or Visual Studio 2022
- Git
- Internet connection for CMake to fetch SFML 3.0.0

Clone the repository:

```bash
git clone https://github.com/YOUR-USERNAME/RobotPathPlanning.git
cd RobotPathPlanning
```

Configure and build with CMake:

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## 🎮 Keyboard Controls

| Key | Function |
|---|---|
| `Space` / `C` | Calculate A* path |
| `R` | Run simulation |
| `P` | Pause / Resume |
| `S` / `G` | Set Start / Goal |
| `O` | Add Static Obstacle |
| `D` | Add Dynamic Obstacle |
| `X` | Remove Obstacle |
| `Left / Right` | Step through A* explanation |
| `Esc` | Close modal |

## 🧪 Scenarios

The project includes predefined scenarios covering:

- Open environments
- Static obstacle navigation
- Maze navigation
- Dynamic obstacle crossing
- Multiple dynamic obstacles
- Blocked optimal routes
- Narrow corridors
- No-path situations
- Random environments
- Complex dynamic environments

## 📊 Demonstration

A recommended demonstration flow is:

1. Calculate the initial A* route.
2. Start the robot simulation.
3. Add an obstacle on the planned route.
4. Observe automatic path replanning.
5. Try a dynamic obstacle scenario.
6. Compare A* and Dijkstra performance.
7. Use the A* explanation mode to inspect the algorithm's decisions.

## 📚 Learning Outcomes

This project demonstrates practical understanding of:

- Object-oriented programming in C++
- Graph traversal algorithms
- Heuristic search
- Path planning
- Dynamic replanning
- Priority queues and related data structures
- Modular C++ project architecture
- CMake-based project configuration
- Unit testing
- Real-time graphical simulation

## 👩‍💻 Author

**Mokshitha Kilari**  
B.Tech Artificial Intelligence and Machine Learning  
SIMATS Engineering

---

⭐ Developed as a C++ class project and extended into a complete interactive robotics simulation.
