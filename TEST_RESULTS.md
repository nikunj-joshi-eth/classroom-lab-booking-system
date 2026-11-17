## v1.0.0 release verification

The final build used `backend/main.c`, Windows MinGW GCC 6.3.0, Node.js 24.15.0, and headless Chrome. Tests used temporary data directories.

- Strict C99 compilation passed with `-Wall -Wextra -Wconversion -pedantic -Werror`.
- Browser and HTTP checks passed for all three roles, resource creation, listing, searching and sorting, timetable filters, booking, conflict prevention, cancellation, FIFO promotion, history and statistics.
- SQLite checks passed for startup and post-write synchronization, persisted booking history after a Node restart, and a sync failure with the primary text files still available.
- Focused C and API checks passed for invalid input, the maximum booking ID, student read-only exit, unreadable data files, and preservation of primary data on temporary-file staging failure.
- Linux file-limit and rename fault injection and sanitizer coverage were outside this Windows release run. The historical legacy-suite results below apply to root `main.c`.

## Role-based reference package verification

- Windows MinGW built `backend/main.c` as C99 with `-Wall -Wextra -Wconversion -pedantic` and emitted no warnings.
- Console checks passed for administrator and teacher password login, student access without a password, and restricted menu actions.
- Node.js syntax checks passed for the server and frontend script. Local HTTP checks served all three frontend files and exercised the C API for login, role restrictions, resource creation and listing, booking, FIFO wait-list promotion after cancellation, reports, and creation of all three runtime files.
- The existing `run_tests.sh` targets the root faculty-console baseline. Its Windows rerun returned 36 passes and five failures in platform-dependent file I/O checks, with sanitizers disabled.

## Current review � 17 November 2026

Team: Nikunj Joshi (137), Resource Management; Mansi (131), Booking Management and Reports; Khushi (148), Cancellation and Waiting List.

- Environment: Windows, Git Bash, MinGW GCC 6.3.0.
- Build: `gcc -std=c99 -Wall -Wextra -Wconversion -pedantic main.c -o booking` passed with no warnings.
- Existing suite: `USE_PLAIN=1 bash run_tests.sh` completed with **36 PASS / 5 FAIL**. T41 reports PASS for the plain build but explicitly marks sanitizer checking as not applicable; no sanitizer validation was performed.
- T34 could not apply Linux `ulimit -f` (Invalid argument), so it did not simulate a write failure.
- T36 expected a read-error message, while Windows rejects opening a directory and prints the cannot-read warning instead.
- T38�T40 could not build/use the Linux `LD_PRELOAD` rename-failure helper. Their simulated failures were not exercised.
- Additional native check passed: four waiting requests across three free slots promoted the first faculty for each slot, leaving the second request for the first slot waiting. The original startup traversal was subsequently restored for the minimal team update; see the rerun below.
- Full Linux failure-injection and sanitizer verification remain outstanding. Historical results below describe an earlier version, not this review.

Rerun after restoring the original startup algorithm: warning-free compilation; **36 PASS / 5 FAIL**, with the same platform limitations above. No algorithm or storage-format changes are included in the three-member commit.

---

# TEST_RESULTS.md

Final verification of the Classroom and Laboratory Resource Booking System (`main.c`).

## 1. How the tests were done

| Item | Detail |
|---|---|
| Environment | Ubuntu 24.04 (Linux), gcc 13.3.0, bash |
| Build 1 (normal) | `gcc -std=c99 -Wall -Wextra -Wconversion -pedantic main.c -o booking` — **no warnings, no errors** |
| Build 2 (checking) | same flags plus `-g -fsanitize=address,undefined` (AddressSanitizer, LeakSanitizer and UndefinedBehaviorSanitizer) |
| Method | `run_tests.sh` runs the program with scripted keyboard input in a fresh folder for every test, then checks the printed text and the saved `.txt` files. Pass/fail comes from these automatic checks. |
| Who ran them | The AI assistant that prepared this version, in its own sandbox. The team has **not** yet run them on their own computers. |
| Repeat them | `bash run_tests.sh` (needs a Linux/macOS shell and gcc) |

**Result: 41 of 41 tests passed on the sanitizer build, and 41 of 41 passed on the normal build. No sanitizer report (memory error, memory leak, undefined behaviour) appeared in any test.** (Tests T38–T40 use the normal build, because they load a small test library that makes `rename()` fail.)

To check that the tests are able to fail, the new test suite was also run against the previous release of `main.c`: 3 tests failed there (T01, T39, T40), matching the two final corrections (section 3). In T39 the old data file was gone after the failed rename.

## 2. Test results (sanitizer build)

"Actual" is the first output line found by the test's checks (prompts cut off) and the number of automatic checks made.

| # | Test | Expected | Actual | Result |
|---|---|---|---|---|
| T01 | Compile with -std=c99 -Wall -Wextra -Wconversion -pedantic | Builds with no warnings or errors | gcc exit code 0 (2 check(s)) | PASS |
| T02 | Add, view, sort and search resources | Added messages; sorted order 50,101,102; ID 101 found; ID 999 not found | Laboratory added successfully. (7 check(s)) | PASS |
| T03 | Duplicate resource ID is rejected | Error message; only one record saved | Error: Resource ID 101 already exists! (2 check(s)) | PASS |
| T04 | Invalid resource inputs (ID 0, 10000; type 3; capacity 0, 501) | Each invalid value rejected, then valid ones accepted | 2 x [Invalid input! Enter a number from 1 to 9999.] (4 check(s)) | PASS |
| T05 | Names with spaces are stored with underscores | '  Room  101 ' saved as Room__101 (spaces trimmed at the ends) | resources.txt = 101 Room__101 1 60 (1 check(s)) | PASS |
| T06 | Resource limit of 50 | 51st resource refused with a message | Resource list is full (maximum 50)! (2 check(s)) | PASS |
| T07 | Valid booking is created and saved | Booking ID 1; record in bookings.txt; listed in View Bookings | Booking confirmed! Booking ID = 1 (3 check(s)) | PASS |
| T08 | Double booking is prevented; other slot/room is allowed | Same room+date+slot refused; slot 2 and Lab A slot 1 accepted | Sorry, this room is already booked for that date and slot. (3 check(s)) | PASS |
| T09 | Same person booking the same slot again | Told that they already hold the booking; no new record | You already hold this booking (Booking ID = 1). (2 check(s)) | PASS |
| T10 | Invalid dates are rejected | 11 impossible/badly formatted dates rejected, 15-08-2026 accepted | 11 x [Invalid date!] (2 check(s)) | PASS |
| T11 | Leap years and month ends are accepted | 29-02-2028, 29-02-2024, 30-04-2026, 31-12-2100, 28-02-2026 all accepted | 5 x [Booking confirmed!] (2 check(s)) | PASS |
| T12 | Booking an unknown resource ID | Not-found message, nothing saved | Resource with ID 999 not found. (2 check(s)) | PASS |
| T13 | Cancel a booking; history keeps it | Cancelled in history, gone from active list, file shows status 0 | Booking 1 cancelled. (6 check(s)) | PASS |
| T14 | Cancel errors | Already-cancelled, unknown ID, and nothing-to-cancel handled | No bookings to cancel. (3 check(s)) | PASS |
| T15 | Waiting list: join, duplicate request rejected | Ravi and Neha queued; Ravi's repeat refused; holder Asha cannot queue | 2 x [Added to waiting list.] (5 check(s)) | PASS |
| T16 | Cancellation serves the earliest ELIGIBLE waiting request (FIFO) | Dev(slot 2) skipped on slot-1 cancel; Ravi, then Neha, then Dev served in turn; Ravi(slot 2) left | Waiting request of Ravi moved to a confirmed booking (Booking ID = 3). (4 check(s)) | PASS |
| T17 | Statistics are accurate | Total 3, Active 2, Cancelled 1, Waiting 1, most booked Room_101 with 2, per-room table 2 and 1 | Total bookings : 3 (limit 200) (7 check(s)) | PASS |
| T18 | Restart: data reloaded, booking IDs continue | Counts reloaded; history shows cancelled booking; new booking gets ID 4 | (Data loaded: 2 resources, 3 bookings, 1 waiting requests) (4 check(s)) | PASS |
| T19 | Empty (0-byte) data files | Loads without warnings | (Data loaded: 0 resources, 0 bookings, 0 waiting requests) (2 check(s)) | PASS |
| T20 | Data files with only blank / space-only lines | Loads without warnings | (Data loaded: 0 resources, 0 bookings, 0 waiting requests) (2 check(s)) | PASS |
| T21 | Data files with Windows (CRLF) line endings | Records loaded without warnings | (Data loaded: 1 resources, 1 bookings, 1 waiting requests) (2 check(s)) | PASS |
| T22 | Records with EXTRA fields or over-long names are rejected | 6 bad records skipped; 1 valid record of each kind kept | Warning: 6 invalid or duplicate record(s) in the data files were ignored. (2 check(s)) | PASS |
| T23 | Invalid file contents (bad IDs, types, capacities, dates, slots, statuses, duplicates) | 17 bad records skipped; valid ones loaded; next booking ID is 21 | Warning: 17 invalid or duplicate record(s) in the data files were ignored. (3 check(s)) | PASS |
| T24 | Waiting request for a FREE slot in the file | Promoted at start-up, so nobody can jump the queue | Waiting request of Raj moved to a confirmed booking (Booking ID = 1). (3 check(s)) | PASS |
| T25 | Person waiting for a slot they already hold (file) | Request rejected as invalid | Warning: 1 invalid or duplicate record(s) in the data files were ignored. (2 check(s)) | PASS |
| T26 | Booking IDs after restart (cancelled record has highest ID; huge ID in file) | ID 999999 rejected; new booking gets ID 6 (highest loaded ID 5 + 1) | Warning: 1 invalid or duplicate record(s) in the data files were ignored. (3 check(s)) | PASS |
| T27 | Storage limit: 200 bookings, cancel while full, restart | 200th ok; 201st refused; cancel keeps request waiting; restart keeps it | Booking confirmed! Booking ID = 200 (7 check(s)) | PASS |
| T28 | Very long (500 character) inputs | Rejected at every prompt; next prompt unaffected; valid data stored | Input too long! Enter 1 to 29 characters. (4 check(s)) | PASS |
| T29 | Invalid numbers at the menu | 12abc, empty, -1, huge number, 3.5, '1 2' all rejected | 8 x [Invalid input! Enter a number from 0 to 10.] (1 check(s)) | PASS |
| T30 | Empty program state: every menu option | Friendly messages, no crash | No resources available. (8 check(s)) | PASS |
| T31 | Input ends in the middle of a question | Clean exit (code 0), earlier data intact, no sanitizer errors | Input ended. Exiting. (6 check(s)) | PASS |
| T32 | Last input line without a newline | Menu choice 0 accepted, program exits normally | Data saved. Goodbye! (2 check(s)) | PASS |
| T33 | Odd bytes in input (control character, NUL byte) | No crash, no sanitizer errors | Classroom added successfully. (2 check(s)) | PASS |
| T34 | Write failure does not damage existing records (file size limit) | bookings.txt unchanged (same checksum); warning shown; no leftover .tmp file | Warning: error while writing bookings.txt.tmp. bookings.txt was not changed. (3 check(s)) | PASS |
| T35 | Temporary file cannot be created | Warning shown; old resources.txt unchanged | Warning: cannot create resources.txt.tmp. resources.txt was not changed. (3 check(s)) | PASS |
| T36 | A data file cannot be read (bookings.txt is a folder) | Saving switched off; other files not overwritten | Warning: error while reading bookings.txt. Saving is switched off to protect your data. (4 check(s)) | PASS |
| T37 | Normal exit saves all three data files | Message shown; resources.txt, bookings.txt, waiting.txt exist; no .tmp left | Data saved. Goodbye! (3 check(s)) | PASS |
| T38 | Rename cannot overwrite an existing file (Windows-like) | Save still works through the backup step; both records saved; no .bak/.tmp left | test helper library built (4 check(s)) | PASS |
| T39 | Renaming the new file always fails | Old resources.txt restored/unchanged (same checksum); no .bak left; new data kept in .tmp | Warning: could not replace resources.txt. It was restored; new data is in resources.txt.tm (5 check(s)) | PASS |
| T40 | The old file cannot be moved aside | Old resources.txt unchanged (same checksum); no .bak created; new data kept in .tmp | Warning: could not replace resources.txt. It was not changed; new data is in resources.txt (4 check(s)) | PASS |
| T41 | Memory safety over all tests | No AddressSanitizer / LeakSanitizer / UndefinedBehaviorSanitizer report in any test | no sanitizer report in tests T02-T37 (T38-T40 use the normal build) (1 check(s)) | PASS |

## 3. Problems corrected in the final release

| Problem | What was observed on the previous release | Test now covering it |
|---|---|---|
| `commitFile()` could lose the old data file when renaming failed | With `rename()` made to fail, `resources.txt` was missing afterwards (checksum could not be computed) | T39, T40 (and T38 for the Windows-like case) |
| `-Wconversion` warning in `openTemp()` (`int` passed where `size_t` expected) | `gcc ... -Wconversion` printed a `-Wsign-conversion` warning | T01 |

Problems corrected in the earlier verification round (safe saving with temporary files, extra fields in data files, free-slot waiting requests, huge booking IDs, whitespace-only lines) remain covered by tests T20, T22, T24–T27 and T34–T36 and are listed in `CHANGELOG.md`.

## 4. What could not be verified

- **Windows and macOS:** not tested. The Windows-style behaviour (a rename that refuses to overwrite an existing file) was only **simulated** on Linux by a small test library (test T38); it was not run on a real Windows system.
- **A real full disk or power failure:** simulated only with a file-size limit (`ulimit -f`). A power cut in the middle of saving was not tested. If the program stops exactly between moving the old file to `.bak` and renaming the new file into place, the data would be in the `.bak` or `.tmp` file; this was not tested.
- **Unreadable file by permissions:** the tests ran as the `root` user, so permission errors could not be created. A data file that cannot be read was simulated by making `bookings.txt` a folder (T36).
- **valgrind:** not installed. AddressSanitizer/LeakSanitizer were used instead (their ability to report a leak was confirmed on a small leaking test program in the earlier round).
- **Several programs at once:** two copies of the program using the same data files at the same time is not supported and was not tested.
- **Manual testing by a person:** all tests used scripted input; nobody typed into the program interactively for this report.
