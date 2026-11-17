## Role-based project synchronization

- Imported the supplied C99 console and API implementation, Node.js bridge, static frontend, and C-only presentation and report.
- Kept the earlier faculty-console source for the existing regression suite.

## Faculty-oriented three-member review

- Team: Nikunj Joshi (137), Resource Management; Mansi (131), Booking Management and Reports; Khushi (148), Cancellation and Waiting List.
- Corrected module credits and faculty prompts/table headings; shortened redundant comments.
- Kept the original startup waiting-list algorithm; reverted the earlier traversal simplification to keep this update minimal.
- Preserved validation, booking history, reports, storage limits and safe file handling. No frontend or database added.

# CHANGELOG.md

## Final corrections (this release)

Only two small corrections; nothing else in `main.c` was changed.

| # | Problem | Fix | Module |
|---|---|---|---|
| 1 | In `commitFile()`, if renaming the temporary file failed, the old data file was deleted first and a second rename was tried. If that failed too, the old file was lost (the previous version left `resources.txt` missing in the new test T39). | The old file is no longer deleted before the new one is in place. Steps: (1) normal `rename`; (2) if that fails, move the old file to `<file>.bak`; if even that fails, change nothing and warn; (3) rename the new file into place; if that fails, restore the old file from `.bak` and warn; (4) on success remove `.bak`. A temporary file with the new data is kept whenever saving fails. | Shared (file handling) |
| 2 | `-Wconversion` warning in `openTemp()`: `int size` was passed to `snprintf()`, which expects `size_t`. | The parameter is now `size_t size` (callers already pass `sizeof(tmp)`). | Shared (file handling) |

Testing: 3 new tests (T38–T40) simulate `rename()` failures (including the Windows behaviour of refusing to overwrite). T01 now compiles with `-Wconversion`. Result: 41 of 41 tests pass on the sanitizer build and on the normal build. Against the previous version the new suite fails T01, T39 and T40. See `TEST_RESULTS.md`.

Documentation updated: `README.md` (compile command, safe-saving description, test numbers), `TEST_RESULTS.md`, `run_tests.sh`.

---

## Previous release (final verification)

The structure, menu, module split and all existing features are unchanged. Only genuine problems found during the final verification were fixed. Items 1, 3, 4, 5, 6 and 7 were first reproduced on the previous version (see `TEST_RESULTS.md`, section 3). Items 2 and 8 were found by reading the code; they are protective changes and are covered by the tests listed in `TEST_RESULTS.md` (T36 for item 2).

### Fixes in `main.c`

| # | Problem found | Fix | Module |
|---|---|---|---|
| 1 | A failed save (e.g. disk full) could cut a data file short and destroy old records | Data is written to `<file>.tmp` first (`openTemp`, `commitFile`); the old file is replaced only if the new one was written completely. A leftover temp file is removed on failure | Shared (file handling) |
| 2 | If a data file could not be read, the next save could overwrite it with incomplete data | `loadProblem` flag: when a file cannot be read, saving is switched off and a warning is shown (`openForRead`, `closeRead`) | Shared (file handling) |
| 3 | Data-file records with extra fields (e.g. `101 Room_101 1 60 JUNK`) were accepted, and names longer than 29 characters were silently cut | Each `sscanf` now has a trailing `%c` and must match exactly the expected number of fields; names and dates are read into larger temporary buffers and rejected if too long | Shared (file handling) |
| 4 | A waiting request for a slot that was already free stayed in the list, so someone else could book the slot first | New `processAllWaiting()` runs after loading and gives every free slot to its earliest waiting request. `processWaiting()` now returns 1 or 0 | Module 3 (Khushi) |
| 5 | A person could be loaded as waiting for a slot they already hold | `loadAll()` rejects such a record | Shared (file handling) |
| 6 | A data file could contain a very large booking ID that the Cancel prompt (maximum 999999) could not accept | Booking IDs in data files must be 1 to 99999 (`MAX_BOOKING_ID`), so new IDs always stay cancellable | Shared / Module 2 |
| 7 | Whitespace-only lines and over-long lines in data files were not handled cleanly | New `isBlankLine()` and `getFileLine()` skip blank lines and reject over-long lines | Shared (file handling) |
| 8 | The "too many records" warning could appear when the extra line was not even a valid record | The limit is checked only for valid records | Shared (file handling) |

### Fixes in `README.md`

- Time complexity table corrected: `processWaiting` is O(Q) (it was written as O(Q + B)), loading is O(R² + B·(R+B) + Q·(R+B+Q) + Q²·B), and `processAllWaiting` was added.
- Test cases replaced by the 38 automated tests listed in `TEST_RESULTS.md`; no result is claimed that was not run.
- Added: start-up handling of waiting requests, safe-saving method, data-file checking rules, limitations that could not be verified.

### New files

- `TEST_RESULTS.md` – test report (38 tests, all passed on the sanitizer build and the normal build).
- `run_tests.sh` – the automatic test script (Linux/macOS with bash and gcc).
- `CHANGELOG.md` – this file.

## Earlier version (second release)

Date validation with leap years, duplicate waiting-request check, `enqueueWaiting()` return value, file error messages, validation of loaded records, safe long-input handling, 200-booking limit handling, `freeWaitingList()`, unique booking IDs after restart.
