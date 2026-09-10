# Sociable Weaver

**A personal project launcher built from scratch in C.**

Run any of your development projects from a browser dashboard — no IDE, no terminal commands, one click. Live output streams directly into the browser. Built as a bridge project toward a J2ME emulator, every line of C here maps directly to skills needed for systems-level programming.

---

## What It Does

- **Launch any project** with one click — Python, Node, Java, anything. You define the run command once.
- **Live terminal output** streams into the browser panel as the project runs.
- **Stop running projects** cleanly from the dashboard.
- **Add projects** via a browser form — name, path, command, optional database port.
- **Database port check** — before launching, Weaver checks if your required database is running. If not, it warns you instead of letting the project crash silently.
- **Persists between sessions** — your project list is saved in `data/projects.json`.
- **Opens automatically** — double-click the exe, browser opens at `localhost:8080`.
- **Fully offline** — no internet, no cloud, no accounts.

---

## Screenshots

> Add screenshots here after taking them — see the Screenshots section below.

---

## How to Build

### Requirements

- Windows (x64)
- GCC via MSYS2 — install from [msys2.org](https://www.msys2.org), then run:
  ```
  pacman -S mingw-w64-x86_64-gcc
  ```
- Add `C:\msys64\mingw64\bin` to your Windows PATH

### Build

```
gcc src/main.c src/server.c src/router.c src/projects.c src/runner.c -o sociable-weaver.exe -lws2_32
```

Or run the included batch file:

```
build.bat
```

### Run

```
.\sociable-weaver.exe
```

The browser opens automatically at `http://localhost:8080`.

---

## How to Use

### Adding a Project

Fill in the **Add Project** form on the right panel:

| Field | What to enter | Example |
|---|---|---|
| Project name | Any display name | `My Flask API` |
| Folder path | Full path to the project folder | `C:/Users/you/Projects/flask-app` |
| Run command | The command to start the project | `python app.py` or `node index.js` |
| Database port | Port your DB runs on, 0 if none | `3306` for MySQL, `27017` for MongoDB |

Click **Add Project** — the card appears immediately in the list.

### Running a Project

Click **Run** on any project card. The output panel opens below the card and live output streams in as the project runs.

### Stopping a Project

Click **Stop** on any running project card. The process is killed immediately.

### Database Port Check

If you set a database port when registering a project, Weaver checks that port before running. If nothing is listening on that port, you see a warning:

```
Database not detected on port 3306. Start it first.
```

Start your database, then click Run again.

---

## Project Structure

```
sociable-weaver/
├── src/
│   ├── main.c          ← Entry point — starts server, opens browser
│   ├── server.c        ← Socket engine — accepts browser connections
│   ├── server.h
│   ├── router.c        ← HTTP request router — handles all endpoints
│   ├── router.h
│   ├── projects.c      ← Reads and writes data/projects.json
│   ├── projects.h
│   ├── runner.c        ← Spawns projects with CreateProcess, pipes output
│   └── runner.h
├── web/
│   ├── index.html      ← Dashboard page
│   ├── style.css       ← Light mode UI with gradient background
│   └── app.js          ← Fetches data from C server, handles Run/Stop/Add
├── data/
│   └── projects.json   ← Your saved project list
├── build.bat           ← One command build script
└── README.md
```

---

## API Endpoints

Sociable Weaver is a real HTTP server. These are the endpoints it serves:

| Method | Path | What it does |
|---|---|---|
| GET | `/` | Serves the dashboard HTML |
| GET | `/style.css` | Serves the stylesheet |
| GET | `/app.js` | Serves the browser-side JavaScript |
| GET | `/api/projects` | Returns project list as JSON |
| POST | `/api/add` | Adds a new project to projects.json |
| POST | `/api/run` | Spawns a project process |
| GET | `/api/output?name=X` | Returns latest output from a running project |
| POST | `/api/stop` | Kills a running project process |

---

## How It Works

The C server listens on port 8080. When a browser connects, `server.c` accepts the connection and passes it to `router.c`. The router reads the raw HTTP request, extracts the URL path, and calls the right handler:

- Static files (`index.html`, `style.css`, `app.js`) are read from disk and sent with correct HTTP headers.
- API endpoints call functions in `projects.c` or `runner.c` and return JSON.

When you click Run, `runner.c` uses Windows `CreateProcess()` to spawn the project as a child process, with stdout redirected through a pipe. The browser polls `/api/output` every 800ms — the server reads available bytes from the pipe and sends them back as JSON. The browser appends them to the output panel.

---

## Why C

This project is a deliberate bridge toward building a J2ME emulator from scratch. Every concept here maps directly to emulator development:

| Sociable Weaver | J2ME Emulator |
|---|---|
| Raw socket server | Reading `.class` file bytes |
| HTTP request parsing | Bytecode and constant pool parsing |
| Request dispatch table | Opcode dispatch table |
| `malloc` / `free` | Heap allocator for Java objects |
| `CreateProcess` + pipes | Threading model — game, render, audio threads |

---

## Known Limitations

- **GUI applications** (PyQt6, Tkinter, Electron) will start and exit immediately — they need a display environment that the piped process does not have. Use Weaver for terminal/server projects.
- **Windows only** — uses Winsock2 and Windows `CreateProcess`. Linux/Mac support would require `fork()`/`exec()` and POSIX sockets.
- **No authentication** — Weaver runs on localhost only and is not designed to be exposed to a network.

---

## Taking Screenshots

To complete this README, take these five screenshots and add them above:

1. **Dashboard with projects** — add 2-3 project cards visible
2. **Project running** — click Run, wait for output panel to appear with text
3. **Add project form** — fill in all four fields before clicking Add
4. **Database warning** — register a project with db_port 3306 while MySQL is off, click Run
5. **Multiple projects** — two or more projects visible with different statuses

To add a screenshot: save it to the repo folder, then add this to the README where you want it:
```markdown
![Dashboard](screenshots/dashboard.png)
```

---

## License

MIT — do whatever you want with it.
