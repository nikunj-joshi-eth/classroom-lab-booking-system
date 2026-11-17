# Classroom and Laboratory Resource Booking System

A faculty-oriented console application in C for booking classrooms and laboratories. The current project uses text files; no frontend or database is implemented.

## Team (exactly 3 members)

| Member | Roll number | Responsibility |
|---|---|---|
| Nikunj Joshi | 137 | Resource Management — arrays, searching, sorting |
| Khushi | 148 | Booking Management and Reports — bookings, availability, statistics |
| Mansi | 131 | Cancellation and Waiting List — linked lists, FIFO queue |

Input validation and file handling are shared across the three modules.

## Features and DSA concepts

- Add, view and search resources using an array of structures and linear search; sort by ID using bubble sort.
- Book a resource for a date and one of four time slots using a booking array. Faculty names identify booking holders; this is not an authentication system.
- Prevent double bookings and duplicate waiting requests. Validate IDs, capacities, dates (including leap years), slots and input lengths.
- Cancel bookings while retaining history. A linked-list queue assigns a released slot to the earliest eligible waiting request (same resource, date and slot).
- Show active bookings, waiting requests, history and statistics, including the most booked resource. Statistics include cancelled bookings in historical totals.
- Load and save resources.txt, bookings.txt and waiting.txt in the current working directory using basic C file handling. Invalid loaded records are skipped with a warning.

The implementation uses arrays, structures, pointers, linked lists, queues, linear search, bubble sort and standard C input/output. It uses no advanced data structures or external libraries.

## Build and run

```sh
gcc -std=c99 -Wall -Wextra -Wconversion -pedantic main.c -o booking
./booking
```

On Windows, run `booking.exe`. Choose menu options 1–10 to manage resources and faculty bookings, or 0 to save and exit.

## Limits and data protection

There are at most 50 resources and 200 booking records, including cancelled records. Names are limited to 29 characters; spaces are stored as underscores. Dates must be real dates from 2024 to 2100. The waiting list is limited by available memory.

Each data file is written to a temporary file before replacement. A backup supports replacement on Windows and recovery if replacement fails. Saving is disabled if a data file cannot be read, to protect existing data. These safeguards are retained despite their extra code. Saves across the three files are not a single transaction; concurrent program instances and power-loss recovery are not supported.

## Tests

`run_tests.sh` contains 41 scripted tests covering resources, bookings, cancellation, FIFO waiting requests, reports, persistence, validation and file failures.

```sh
bash run_tests.sh
USE_PLAIN=1 bash run_tests.sh
```

The complete suite requires Linux, Bash and GCC: some tests use Linux file limits and an LD_PRELOAD helper to simulate rename failures. Git Bash on Windows can run many checks but cannot reproduce all of these Linux behaviours. See TEST_RESULTS.md for historical results and the current review results.
