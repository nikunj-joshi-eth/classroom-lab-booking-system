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
- **Web demo:** The server listens on the local machine. The login endpoint accepts a role selection and verifies it through C. Every other action uses the session role. Sessions expire after one hour or when the server restarts. Administrator and Teacher share the documented demo passwords within their respective roles.

## Run on Windows (PowerShell)
Install GCC (MinGW-w64) and Node.js 24. Make sure `gcc` and `node` are on your PATH. From the project root:

```powershell
cd backend
gcc -std=c99 -Wall -Wextra main.c -o booking.exe
./booking.exe
```

To start the web demo, exit the console and run `node server.js` from `backend/`. Open `http://127.0.0.1:3000`. Node.js includes the SQLite module, so no `npm install` or separate SQLite installation is needed. Run the console and web demo separately when they share the same files.

Choose a date on the dashboard and filter rooms by name or ID, type, minimum capacity, or availability. Administrators and teachers can select an available slot to open a prefilled booking form or select a booked slot to request the waiting list. Students can browse the timetable and reports. The timetable reads the primary text files and reflects active bookings only.

Existing `POST /api/<action>` endpoints retain their action names, input fields, and `output` response. API clients should retain the login session cookie and send it with later requests.

### SQLite synchronization

`backend/schema.sql` defines `resources`, `bookings`, and `waiting_requests`. Node.js refreshes all three tables in a transaction at startup and after C write operations. Restart the server to import changes made in the standalone console. Booking IDs and the `Active` and `Cancelled` labels are preserved. Query `waiting_requests` with `ORDER BY position` for FIFO order.

Missing source files are treated as empty. Invalid and duplicate rows are skipped and counted in the server log. If synchronization fails, the server logs the error and rolls back any incomplete database transaction. The API also displays a synchronization warning when a write triggers the failure. The next startup or completed write operation retries synchronization.

## Development notes
- Root `main.c` is the earlier faculty-console baseline used by `run_tests.sh`. Build `backend/main.c` for current console and web use.
- Preserve the C implementation and its syllabus-level algorithms when synchronizing the project files.
- `docs/` contains the supplied C-only presentation and report. The report still has placeholders for console screenshots.
- Runtime text files, SQLite databases and sidecars, and executables are ignored by Git.
