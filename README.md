# Classroom and Laboratory Resource Booking System

Second-year DSA PBL project. The main implementation is in **C (C99)**; a small web demonstration calls the C executable through Node.js.

## Team (exactly three members)
- **Nikunj Joshi (25/SET/BCSE/137)** — Resource management: add/view/search/sort; arrays, structures, linear search, bubble sort.
- **Mansi (25/SET/BCSE/131)** — Booking management and reports: booking, availability, history and statistics.
- **Khushi (25/SET/BCSE/148)** — Cancellation and waiting list: linked-list FIFO queue.

## Implementation
- **C:** `backend/main.c` is the primary code; `--api` mode accepts a role and an action for the web server.
- **Web:** `frontend/` contains HTML/CSS/JavaScript; `backend/server.js` invokes the C executable.
- **Storage:** C uses text files `resources.txt`, `bookings.txt`, and `waiting.txt`; **no SQLite database has been added yet**.
- **Console roles:** Administrator (demo password `1234`) has full access. Teacher (`123`) cannot add or sort resources. Student (no password) has read-only access.
- **Web demo:** Run locally. The demo passwords are for local testing. The API trusts the role supplied with each request, so review authentication and role enforcement before public deployment.

## Run on Windows (PowerShell)
Install GCC and Node.js. From the project root:

```powershell
cd backend
gcc -std=c99 -Wall -Wextra main.c -o booking.exe
./booking.exe
```

To start the web demo, exit the console and run `node server.js` from `backend/`. Open `http://localhost:3000`. The server uses Node.js built-in modules and stores its text files in `backend/`.

## Development notes
- Root `main.c` is the earlier faculty-console baseline used by `run_tests.sh`. Build `backend/main.c` for current console and web use.
- Preserve the C implementation and its syllabus-level algorithms when synchronizing the project files.
- `docs/` contains the supplied C-only presentation and report. The report still has placeholders for console screenshots.
- Runtime data files and executables are ignored by Git.
