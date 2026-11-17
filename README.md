# Classroom and Laboratory Resource Booking System

Second-year DSA PBL project. The main implementation is in **C (C99)**; a small web demonstration calls the C executable through Node.js.

## Team (exactly three members)
- **Nikunj Joshi (25/SET/BCSE/137)** — Resource management: add/view/search/sort; arrays, structures, linear search, bubble sort.
- **Mansi (25/SET/BCSE/131)** — Booking management and reports: booking, availability, history and statistics.
- **Khushi (25/SET/BCSE/148)** — Cancellation and waiting list: linked-list FIFO queue.

## Implementation
- **C:** `backend/main.c` is the primary code; `--api` mode accepts a role and an action for the web server.
- **Web:** `frontend/` contains HTML/CSS/JavaScript; `backend/server.js` invokes the C executable.
- **Storage:** C owns `resources.txt`, `bookings.txt`, and `waiting.txt`. Node.js mirrors them to `backend/booking.db` for SQLite queries.
- **Console roles:** Administrator (demo password `1234`) has full access. Teacher (`123`) cannot add or sort resources. Student (no password) has read-only access.
- **Web demo:** Run locally. The demo passwords are for local testing. The API trusts the role supplied with each request, so review authentication and role enforcement before public deployment.

## Run on Windows (PowerShell)
Install GCC (MinGW-w64) and Node.js 24. Make sure `gcc` and `node` are on your PATH. From the project root:

```powershell
cd backend
gcc -std=c99 -Wall -Wextra main.c -o booking.exe
./booking.exe
```

To start the web demo, exit the console and run `node server.js` from `backend/`. Open `http://localhost:3000`. Node.js includes the SQLite module, so no `npm install` or separate SQLite installation is needed. Run the console and web demo separately when they share the same files.

### SQLite synchronization

`backend/schema.sql` defines `resources`, `bookings`, and `waiting_requests`. Node.js refreshes all three tables in a transaction at startup and after C write operations. Restart the server to import changes made in the standalone console. Booking IDs and the `Active` and `Cancelled` labels are preserved. Query `waiting_requests` with `ORDER BY position` for FIFO order.

Missing source files are treated as empty. Invalid and duplicate rows are skipped and counted in the server log. If synchronization fails, the server logs the error, any incomplete database transaction rolls back, and the C-backed API continues using the text files. The next startup or completed write operation retries synchronization.

## Development notes
- Root `main.c` is the earlier faculty-console baseline used by `run_tests.sh`. Build `backend/main.c` for current console and web use.
- Preserve the C implementation and its syllabus-level algorithms when synchronizing the project files.
- `docs/` contains the supplied C-only presentation and report. The report still has placeholders for console screenshots.
- Runtime text files, SQLite databases and sidecars, and executables are ignored by Git.
