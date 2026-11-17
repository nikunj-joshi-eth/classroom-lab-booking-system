#!/bin/bash
# Automated tests for the Resource Booking System (Linux / bash + gcc).
# Usage:  bash run_tests.sh            (builds main.c with sanitizers if possible)
#         REPORT=TEST_RESULTS_RAW.md bash run_tests.sh   (also writes a table)
# Every test runs the program with scripted keyboard input in a fresh folder
# and checks the printed text and the saved .txt files.

cd "$(dirname "$0")" || exit 1
shopt -s lastpipe        # lets "... | run" set variables in this shell
WORK=$(mktemp -d)
PASSES=0; FAILS=0; SANITIZER_PROBLEM_ANYWHERE=0
REPORT=${REPORT:-}; RUN_ENV=""
case "$REPORT" in /*|"") ;; *) REPORT="$PWD/$REPORT" ;; esac   # make the path absolute
if [ -n "$REPORT" ]; then : > "$REPORT"; fi

# ---------- build ----------
gcc -std=c99 -Wall -Wextra -Wconversion -pedantic main.c -o "$WORK/booking_plain" 2> "$WORK/warnings.txt"
BUILD_OK=$?
if gcc -std=c99 -Wall -Wextra -Wconversion -pedantic -g -fsanitize=address,undefined main.c -o "$WORK/booking_san" 2>/dev/null; then
    BIN="$WORK/booking_san"; BUILD_NAME="AddressSanitizer + UndefinedBehaviorSanitizer build"
else
    BIN="$WORK/booking_plain"; BUILD_NAME="plain build (sanitizers not available)"
fi
[ "$USE_PLAIN" = "1" ] && { BIN="$WORK/booking_plain"; BUILD_NAME="plain build (no sanitizers)"; }
echo "Using: $BUILD_NAME"

# ---------- helpers ----------
fresh() { T=$(mktemp -d -p "$WORK"); cd "$T" || exit 1; }

# run: program reads stdin; sets OUT (menu text removed), RAW, CODE
run() {
    RAW=$(env $RUN_ENV "$BIN" 2>&1); CODE=$?
    OUT=$(printf '%s\n' "$RAW" | sed '/^===== RESOURCE BOOKING SYSTEM/,/^0. Exit/d' | sed '/^$/d' \
          | sed 's/Enter your choice: //g; /^  [1-4]\. /d; /^Time slots:/d')
    if printf '%s' "$RAW" | grep -qE "Sanitizer|runtime error"; then SANBAD=1; fi
}

add_res() { printf '1\n%s\n%s\n%s\n%s\n' "$1" "$2" "$3" "$4"; }   # id name type capacity
book()    { printf '5\n%s\n%s\n%s\n%s\n' "$1" "$2" "$3" "$4"; }   # resId date slot name
cancel()  { printf '7\n%s\n' "$1"; }

tbegin() { TID="$1"; TDESC="$2"; TEXP="$3"; BAD=0; ACT=""; NOTE=""; SANBAD=0; NCHK=0; }
note_ok()  { NCHK=$((NCHK+1))
             if [ -z "$ACT" ]; then   # keep only the program's answer (cut off the prompts in front of it)
                 ACT=$(printf '%s' "$1" | sed -E 's/^((Enter [^:]*|Choose slot|Type \([^)]*\)|Join the waiting list\? \([^)]*\)): )+//')
             fi; }
note_bad() { NCHK=$((NCHK+1)); BAD=1; NOTE="$NOTE MISSING/WRONG: $1;"; }

expect() {      # fixed text must appear in OUT
    local line; line=$(printf '%s\n' "$OUT" | grep -F -m1 -- "$1")
    if [ -n "$line" ]; then note_ok "$line"; else note_bad "[$1]"; fi
}
expect_re() {   # regular expression must match a line of OUT
    local line; line=$(printf '%s\n' "$OUT" | grep -E -m1 -- "$1")
    if [ -n "$line" ]; then note_ok "$line"; else note_bad "regex [$1]"; fi
}
absent() {      # fixed text must NOT appear in OUT
    if printf '%s\n' "$OUT" | grep -qF -- "$1"; then note_bad "unexpected [$1]"; else NCHK=$((NCHK+1)); fi
}
expect_count() { # fixed text must appear on exactly N lines
    local c; c=$(printf '%s\n' "$OUT" | grep -cF -- "$1")
    if [ "$c" = "$2" ]; then note_ok "$c x [$1]"; else note_bad "[$1] seen $c times, expected $2"; fi
}
expect_eq() {   # label, expected, actual
    if [ "$2" = "$3" ]; then note_ok "$1 = $3"; else note_bad "$1: expected [$2] got [$3]"; fi
}
expect_exit() { expect_eq "exit code" "$1" "$CODE"; }

tend() {
    [ "$SANBAD" = 1 ] && { BAD=1; NOTE="$NOTE SANITIZER REPORTED A PROBLEM;"; SANITIZER_PROBLEM_ANYWHERE=1; }
    if [ "$BAD" = 0 ]; then R="PASS"; PASSES=$((PASSES+1)); else R="FAIL"; FAILS=$((FAILS+1)); fi
    printf '[%s] %s  %s\n' "$R" "$TID" "$TDESC"
    [ "$BAD" = 1 ] && printf '       %s\n' "$NOTE"
    if [ -n "$REPORT" ]; then
        local a; a=$(printf '%s' "$ACT" | tr '|' '/' | sed 's/  */ /g' | cut -c1-90)
        [ "$BAD" = 1 ] && a="$a $NOTE"
        printf '| %s | %s | %s | %s (%s check(s)) | %s |\n' "$TID" "$TDESC" "$(printf '%s' "$TEXP" | tr '|' '/')" "$a" "$NCHK" "$R" >> "$REPORT"
    fi
}

# small helper library that makes rename() fail in chosen ways (used by T38-T40)
cat > "$WORK/fake_rename.c" <<'EOC'
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
static int ends(const char *s, const char *e) { size_t a = strlen(s), b = strlen(e); return a >= b && strcmp(s + a - b, e) == 0; }
int rename(const char *o, const char *n) {
    static int (*real)(const char *, const char *);
    const char *m = getenv("FAKE_RENAME");
    if (!real) real = (int (*)(const char *, const char *))dlsym(RTLD_NEXT, "rename");
    if (m && (strcmp(m, "WIN") == 0 || strcmp(m, "BAKFAIL") == 0) && access(n, F_OK) == 0) { errno = EEXIST; return -1; }
    if (m && strcmp(m, "BAKFAIL") == 0 && ends(n, ".bak")) { errno = EACCES; return -1; }
    if (m && strcmp(m, "TMPFAIL") == 0 && ends(o, ".tmp")) { errno = EACCES; return -1; }
    return real(o, n);
}
EOC
gcc -shared -fPIC -o "$WORK/fake_rename.so" "$WORK/fake_rename.c" -ldl 2>/dev/null; SHIM_OK=$?

LONG=$(printf 'x%.0s' $(seq 1 500))

# ---------- T01 compile ----------
tbegin T01 "Compile with -std=c99 -Wall -Wextra -Wconversion -pedantic" "Builds with no warnings or errors"
if [ "$BUILD_OK" = 0 ]; then note_ok "gcc exit code 0"; else note_bad "gcc failed"; fi
if [ ! -s "$WORK/warnings.txt" ]; then note_ok "no warnings printed"; else note_bad "warnings: $(head -c 200 "$WORK/warnings.txt")"; fi
tend

# ---------- Module 1 ----------
fresh; tbegin T02 "Add, view, sort and search resources" "Added messages; sorted order 50,101,102; ID 101 found; ID 999 not found"
{ add_res 102 "Lab A" 2 40; add_res 101 "Room 101" 1 60; add_res 50 "Room 50" 1 30; echo 2; echo 4; printf '3\n101\n3\n999\n0\n'; } | run
expect "Laboratory added successfully."; expect "Classroom added successfully."
expect "Resources sorted by ID."
expect_eq "sorted IDs" "50,101,102" "$(printf '%s\n' "$OUT" | awk '/sorted by ID/{f=1;next} f&&/^[0-9]+ /{print $1}' | paste -sd,)"
expect "Found: ID=101, Name=Room_101, Type=Classroom, Capacity=60"
expect "Resource with ID 999 not found."
expect_eq "first line of resources.txt" "50 Room_50 1 30" "$(head -1 resources.txt)"
tend

fresh; tbegin T03 "Duplicate resource ID is rejected" "Error message; only one record saved"
{ add_res 101 "Room 101" 1 60; printf '1\n101\n0\n'; } | run
expect "Error: Resource ID 101 already exists!"
expect_eq "lines in resources.txt" "1" "$(wc -l < resources.txt | tr -d ' ')"
tend

fresh; tbegin T04 "Invalid resource inputs (ID 0, 10000; type 3; capacity 0, 501)" "Each invalid value rejected, then valid ones accepted"
printf '1\n0\n10000\n7\nLab\n3\n2\n0\n501\n30\n0\n' | run
expect_count "Invalid input! Enter a number from 1 to 9999." 2
expect_count "Invalid input! Enter a number from 1 to 2." 1
expect_count "Invalid input! Enter a number from 1 to 500." 2
expect "Laboratory added successfully."
tend

fresh; tbegin T05 "Names with spaces are stored with underscores" "'  Room  101 ' saved as Room__101 (spaces trimmed at the ends)"
printf '1\n101\n  Room  101 \n1\n60\n0\n' | run
expect_eq "resources.txt" "101 Room__101 1 60" "$(cat resources.txt)"
tend

fresh; tbegin T06 "Resource limit of 50" "51st resource refused with a message"
{ for i in $(seq 1 50); do add_res $i "R$i" 1 10; done; echo 1; echo 0; } | run
expect "Resource list is full (maximum 50)!"
expect_eq "lines in resources.txt" "50" "$(wc -l < resources.txt | tr -d ' ')"
tend

# ---------- Module 2 ----------
fresh; tbegin T07 "Valid booking is created and saved" "Booking ID 1; record in bookings.txt; listed in View Bookings"
{ add_res 101 "Room 101" 1 60; book 101 15-08-2026 1 Asha; echo 6; echo 0; } | run
expect "Booking confirmed! Booking ID = 1"
expect_re "^1 +Room_101 +15-08-2026 +9-10 AM +Asha"
expect_eq "bookings.txt" "1 101 15-08-2026 1 Asha 1" "$(cat bookings.txt)"
tend

fresh; tbegin T08 "Double booking is prevented; other slot/room is allowed" "Same room+date+slot refused; slot 2 and Lab A slot 1 accepted"
{ add_res 101 "Room 101" 1 60; add_res 102 "Lab A" 2 40; book 101 15-08-2026 1 Asha; book 101 15-08-2026 1 Ravi; echo 0
  book 101 15-08-2026 2 Ravi; book 102 15-08-2026 1 Tina; echo 0; } | run
expect "Sorry, this room is already booked for that date and slot."
expect_count "Booking confirmed!" 3
expect_eq "lines in bookings.txt" "3" "$(wc -l < bookings.txt | tr -d ' ')"
tend

fresh; tbegin T09 "Same person booking the same slot again" "Told that they already hold the booking; no new record"
{ add_res 101 "Room 101" 1 60; book 101 15-08-2026 1 Asha; book 101 15-08-2026 1 Asha; echo 0; } | run
expect "You already hold this booking (Booking ID = 1)."
expect_eq "lines in bookings.txt" "1" "$(wc -l < bookings.txt | tr -d ' ')"
tend

fresh; tbegin T10 "Invalid dates are rejected" "11 impossible/badly formatted dates rejected, 15-08-2026 accepted"
{ add_res 101 "Room 101" 1 60; printf '5\n101\n'
  for d in 31-02-2026 29-02-2026 29-02-2100 31-04-2026 00-01-2026 15-13-2026 1-1-2026 15-08-2023 15-08-2101 15/08/2026 abcdefghij 15-08-2026; do echo "$d"; done
  printf '1\nAsha\n0\n'; } | run
expect_count "Invalid date!" 11
expect "Booking confirmed! Booking ID = 1"
tend

fresh; tbegin T11 "Leap years and month ends are accepted" "29-02-2028, 29-02-2024, 30-04-2026, 31-12-2100, 28-02-2026 all accepted"
{ add_res 101 "Room 101" 1 60; for d in 29-02-2028 29-02-2024 30-04-2026 31-12-2100 28-02-2026; do book 101 $d 1 Asha; done; echo 0; } | run
expect_count "Booking confirmed!" 5
absent "Invalid date!"
tend

fresh; tbegin T12 "Booking an unknown resource ID" "Not-found message, nothing saved"
{ add_res 101 "Room 101" 1 60; printf '5\n999\n0\n'; } | run
expect "Resource with ID 999 not found."
expect_eq "bookings.txt size" "0" "$(wc -c < bookings.txt 2>/dev/null | tr -d ' ')"
tend

# ---------- Module 3 ----------
fresh; tbegin T13 "Cancel a booking; history keeps it" "Cancelled in history, gone from active list, file shows status 0"
{ add_res 101 "Room 101" 1 60; book 101 15-08-2026 1 Asha; book 101 15-08-2026 2 Ravi; cancel 1; echo 9; echo 0; } | run
expect "Booking 1 cancelled."
expect_re "Asha .*Cancelled"
expect_re "Ravi .*Active"
expect_eq "first line of bookings.txt" "1 101 15-08-2026 1 Asha 0" "$(head -1 bookings.txt)"
printf '6\n0\n' | run
expect "Ravi"; absent "Asha"
tend

fresh; tbegin T14 "Cancel errors" "Already-cancelled, unknown ID, and nothing-to-cancel handled"
{ echo 7; echo 0; } | run; expect "No bookings to cancel."
{ add_res 101 "Room 101" 1 60; book 101 15-08-2026 1 Asha; cancel 1; cancel 1; cancel 99; echo 0; } | run
expect "Booking 1 is already cancelled."; expect "Booking ID 99 not found."
tend

fresh; tbegin T15 "Waiting list: join, duplicate request rejected" "Ravi and Neha queued; Ravi's repeat refused; holder Asha cannot queue"
{ add_res 101 "Room 101" 1 60; book 101 15-08-2026 1 Asha
  book 101 15-08-2026 1 Ravi; echo 1
  book 101 15-08-2026 1 Ravi
  book 101 15-08-2026 1 Neha; echo 1
  book 101 15-08-2026 1 Asha; echo 8; echo 0; } | run
expect_count "Added to waiting list." 2
expect "You are already on the waiting list for this room, date and slot."
expect "You already hold this booking (Booking ID = 1)."
expect_eq "waiting.txt lines" "2" "$(wc -l < waiting.txt | tr -d ' ')"
expect_eq "queue order in file" "Ravi,Neha" "$(awk '{print $4}' waiting.txt | paste -sd,)"
tend

fresh; tbegin T16 "Cancellation serves the earliest ELIGIBLE waiting request (FIFO)" "Dev(slot 2) skipped on slot-1 cancel; Ravi, then Neha, then Dev served in turn; Ravi(slot 2) left"
{ add_res 101 "Room 101" 1 60
  book 101 15-08-2026 1 Asha; book 101 15-08-2026 2 Zoe
  book 101 15-08-2026 2 Dev; echo 1
  book 101 15-08-2026 1 Ravi; echo 1
  book 101 15-08-2026 1 Neha; echo 1
  book 101 15-08-2026 2 Ravi; echo 1
  cancel 1; cancel 3; cancel 2; echo 8; echo 9; echo 0; } | run
expect "Waiting request of Ravi moved to a confirmed booking (Booking ID = 3)."
expect "Waiting request of Neha moved to a confirmed booking (Booking ID = 4)."
expect "Waiting request of Dev moved to a confirmed booking (Booking ID = 5)."
expect_eq "waiting.txt after" "101 15-08-2026 2 Ravi" "$(cat waiting.txt)"
tend

fresh; tbegin T17 "Statistics are accurate" "Total 3, Active 2, Cancelled 1, Waiting 1, most booked Room_101 with 2, per-room table 2 and 1"
{ add_res 101 "Room 101" 1 60; add_res 102 "Lab A" 2 40
  book 101 15-08-2026 1 Asha; book 101 15-08-2026 2 Zoe; book 102 15-08-2026 1 Tina
  book 101 15-08-2026 2 Ravi; echo 1
  cancel 1; echo 10; echo 0; } | run
expect "Total bookings     : 3 (limit 200)"; expect "Active bookings    : 2"
expect "Cancelled bookings : 1"; expect "Waiting requests   : 1"
expect "Most booked room   : Room_101 (ID 101) with 2 booking(s)"
expect_re "^101 +Room_101 +2 "; expect_re "^102 +Lab_A +1 "
tend

tbegin T18 "Restart: data reloaded, booking IDs continue" "Counts reloaded; history shows cancelled booking; new booking gets ID 4"
{ echo 9; book 102 16-08-2026 1 Zed; echo 8; echo 0; } | run
expect "(Data loaded: 2 resources, 3 bookings, 1 waiting requests)"
expect_re "Asha .*Cancelled"
expect "Booking confirmed! Booking ID = 4"
expect_re "Ravi"
tend

# ---------- file handling ----------
fresh; tbegin T19 "Empty (0-byte) data files" "Loads without warnings"
: > resources.txt; : > bookings.txt; : > waiting.txt
echo 0 | run
expect "(Data loaded: 0 resources, 0 bookings, 0 waiting requests)"; absent "Warning"
tend

fresh; tbegin T20 "Data files with only blank / space-only lines" "Loads without warnings"
printf '\n   \n\t\n' > resources.txt; printf '\n\n' > bookings.txt; printf '  \n' > waiting.txt
echo 0 | run
expect "(Data loaded: 0 resources, 0 bookings, 0 waiting requests)"; absent "Warning"
tend

fresh; tbegin T21 "Data files with Windows (CRLF) line endings" "Records loaded without warnings"
printf '101 Room_101 1 60\r\n' > resources.txt
printf '1 101 15-08-2026 1 Asha 1\r\n' > bookings.txt
printf '101 15-08-2026 1 Raj\r\n' > waiting.txt
echo 0 | run
expect "(Data loaded: 1 resources, 1 bookings, 1 waiting requests)"; absent "Warning"
tend

fresh; tbegin T22 "Records with EXTRA fields or over-long names are rejected" "6 bad records skipped; 1 valid record of each kind kept"
cat > resources.txt <<'EOF'
101 Room_101 1 60 JUNK
102 Lab_A 2 40
103 Averyveryveryveryverylongnamehere_x 1 50
EOF
cat > bookings.txt <<'EOF'
1 102 15-08-2026 1 Asha 1 99
2 102 15-08-2026 1 Asha 1
3 102 15-08-2026 2 Averyveryveryveryverylongnamehere_x 1
EOF
cat > waiting.txt <<'EOF'
102 15-08-2026 1 Raj extra
102 15-08-2026 1 Rajveryveryveryveryverylongnamehere_xyz
102 15-08-2026 1 Raj
EOF
echo 0 | run
expect "6 invalid or duplicate record(s)"
expect "(Data loaded: 1 resources, 1 bookings, 1 waiting requests)"
tend

fresh; tbegin T23 "Invalid file contents (bad IDs, types, capacities, dates, slots, statuses, duplicates)" "17 bad records skipped; valid ones loaded; next booking ID is 21"
cat > resources.txt <<'EOF'
101 Room_101 1 60
101 Dup 1 30
0 Zero 1 30
102 Lab_A 3 40
103 Lab_B 2 0
104 Lab_C 2 501
105 Hall 1 80
abc
10000 Big 1 20
EOF
cat > bookings.txt <<'EOF'
1 101 15-08-2026 1 Asha 1
2 101 15-08-2026 1 Bob 1
1 101 16-08-2026 2 Cat 1
3 999 15-08-2026 1 Dan 1
4 101 31-02-2026 1 Eve 1
5 101 15-08-2026 9 Fay 1
6 101 15-08-2026 2 Gus 5
7 105 15-08-2026 2 Hal 0
20 101 17-08-2026 3 Ivy 1
EOF
cat > waiting.txt <<'EOF'
101 15-08-2026 1 Raj
101 15-08-2026 1 Raj
999 15-08-2026 1 Sam
101 30-02-2026 1 Tim
101 15-08-2026 7 Uma
EOF
{ book 101 18-08-2026 1 Zed; echo 0; } | run
expect "17 invalid or duplicate record(s)"
expect "(Data loaded: 2 resources, 3 bookings, 1 waiting requests)"
expect "Booking confirmed! Booking ID = 21"
tend

fresh; tbegin T24 "Waiting request for a FREE slot in the file" "Promoted at start-up, so nobody can jump the queue"
printf '101 Room_101 1 60\n' > resources.txt; : > bookings.txt; printf '101 15-08-2026 1 Raj\n' > waiting.txt
{ book 101 15-08-2026 1 Sue; echo 0; echo 8; echo 6; echo 0; } | run
expect "Waiting request of Raj moved to a confirmed booking (Booking ID = 1)."
expect "Sorry, this room is already booked for that date and slot."
expect "Waiting list is empty."
tend

fresh; tbegin T25 "Person waiting for a slot they already hold (file)" "Request rejected as invalid"
printf '101 Room_101 1 60\n' > resources.txt
printf '1 101 15-08-2026 1 Asha 1\n' > bookings.txt; printf '101 15-08-2026 1 Asha\n' > waiting.txt
echo 0 | run
expect "1 invalid or duplicate record(s)"; expect "(Data loaded: 1 resources, 1 bookings, 0 waiting requests)"
tend

fresh; tbegin T26 "Booking IDs after restart (cancelled record has highest ID; huge ID in file)" "ID 999999 rejected; new booking gets ID 6 (highest loaded ID 5 + 1)"
printf '101 Room_101 1 60\n' > resources.txt
printf '1 101 15-08-2026 1 A 1\n5 101 15-08-2026 2 B 0\n999999 101 15-08-2026 3 C 1\n' > bookings.txt
{ book 101 20-08-2026 1 Zed; cancel 6; echo 0; } | run
expect "1 invalid or duplicate record(s)"
expect "Booking confirmed! Booking ID = 6"
expect "Booking 6 cancelled."
tend

fresh; tbegin T27 "Storage limit: 200 bookings, cancel while full, restart" "200th ok; 201st refused; cancel keeps request waiting; restart keeps it"
{ add_res 101 Room101 1 60
  n=0; for m in 1 2 3 4 5 6 7; do for d in $(seq 1 28); do for s in 1 2 3 4; do
      [ $n -lt 199 ] && { book 101 $(printf '%02d-%02d-2027' $d $m) $s "Stu$n"; n=$((n+1)); }; done; done; done
  book 101 01-01-2027 1 Wanda; echo 1
  book 101 01-12-2027 1 Last
  echo 5
  cancel 1; echo 8; echo 0; } | run
expect "Booking confirmed! Booking ID = 200"
expect "Booking storage is full (200 records). No new bookings can be made."
expect "Booking storage is full. Waiting request of Wanda could not be confirmed."
expect_eq "lines in bookings.txt" "200" "$(wc -l < bookings.txt | tr -d ' ')"
expect_eq "waiting.txt" "101 01-01-2027 1 Wanda" "$(cat waiting.txt)"
echo 0 | run
expect "(Data loaded: 1 resources, 200 bookings, 1 waiting requests)"
expect "Booking storage is full. Waiting request of Wanda could not be confirmed."
tend

# ---------- input handling ----------
fresh; tbegin T28 "Very long (500 character) inputs" "Rejected at every prompt; next prompt unaffected; valid data stored"
printf '%s\n1\n%s\n101\n%s\nRoom 101\n%s\n1\n%s\n60\n2\n0\n' "$LONG" "$LONG" "$LONG" "$LONG" "$LONG" | run
expect "Input too long! Enter 1 to 29 characters."
expect_count "Invalid input! Enter a number" 4
expect "Classroom added successfully."
expect_eq "resources.txt" "101 Room_101 1 60" "$(cat resources.txt)"
tend

fresh; tbegin T29 "Invalid numbers at the menu" "12abc, empty, -1, huge number, 3.5, '1 2' all rejected"
printf '12abc\n\n-1\n99999999999999999999\n3.5\n1 2\nabc\n11\n0\n' | run
expect_count "Invalid input! Enter a number from 0 to 10." 8
tend

fresh; tbegin T30 "Empty program state: every menu option" "Friendly messages, no crash"
{ for c in 2 3 4 5 6 7 8 9 10; do echo $c; done; echo 0; } | run
expect "No resources available."; expect "Not enough resources to sort."
expect "No resources available. Add one first."; expect "No active bookings."
expect "No bookings to cancel."; expect "Waiting list is empty."; expect "No booking history."
expect "Most booked room   : None (no bookings yet)"
tend

fresh; tbegin T31 "Input ends in the middle of a question" "Clean exit (code 0), earlier data intact, no sanitizer errors"
{ add_res 101 "Room 101" 1 60; printf '5\n101\n15-08-2026\n'; } | run
expect "Input ended. Exiting."; expect_exit 0
expect_eq "resources.txt" "101 Room_101 1 60" "$(cat resources.txt)"
{ book 101 15-08-2026 1 Asha; printf '5\n101\n15-08-2026\n1\nRavi\n'; } | run
expect "Input ended. Exiting."; expect_exit 0
echo -n "" | run; expect "Input ended. Exiting."
tend

fresh; tbegin T32 "Last input line without a newline" "Menu choice 0 accepted, program exits normally"
printf '2\n0' | run
expect "Data saved. Goodbye!"; expect_exit 0
tend

fresh; tbegin T33 "Odd bytes in input (control character, NUL byte)" "No crash, no sanitizer errors"
printf '1\n101\nRoom\001X\n1\n60\n0\n' | run; expect "Classroom added successfully."
printf '1\n102\n\000\nRoom\n1\n1\n0\n' | run; expect_exit 0
tend

# ---------- saving safety ----------
fresh; tbegin T34 "Write failure does not damage existing records (file size limit)" "bookings.txt unchanged (same checksum); warning shown; no leftover .tmp file"
printf '101 Room_101 1 60\n' > resources.txt
n=0; : > bookings.txt; for m in 1 2 3 4 5 6 7; do for d in $(seq 1 28); do [ $n -lt 150 ] && { printf '%d 101 %02d-%02d-2027 1 Student_%d 1\n' $((n+1)) $d $m $n >> bookings.txt; n=$((n+1)); }; done; done
SUM1=$(md5sum bookings.txt)
RAW=$( ( trap '' XFSZ; ulimit -f 1; printf '5\n101\n01-12-2027\n2\nSue\n0\n' | "$BIN" 2>&1 ) ); CODE=$?
OUT=$(printf '%s\n' "$RAW" | sed '/^===== RESOURCE BOOKING SYSTEM/,/^0. Exit/d' | sed '/^$/d')
printf '%s' "$RAW" | grep -qE "Sanitizer|runtime error" && SANBAD=1
expect "bookings.txt was not changed."
expect_eq "bookings.txt checksum" "$SUM1" "$(md5sum bookings.txt)"
expect_eq "leftover .tmp files" "0" "$(ls *.tmp 2>/dev/null | wc -l | tr -d ' ')"
tend

fresh; tbegin T35 "Temporary file cannot be created" "Warning shown; old resources.txt unchanged"
printf '101 Room_101 1 60\n' > resources.txt; mkdir resources.txt.tmp
SUM1=$(md5sum resources.txt)
{ add_res 102 "Lab A" 2 40; echo 0; } | run
expect "cannot create resources.txt.tmp. resources.txt was not changed."
expect "Goodbye! (Some data could not be saved.)"
expect_eq "resources.txt checksum" "$SUM1" "$(md5sum resources.txt)"
tend

fresh; tbegin T36 "A data file cannot be read (bookings.txt is a folder)" "Saving switched off; other files not overwritten"
printf '101 Room_101 1 60\n' > resources.txt; mkdir bookings.txt
SUM1=$(md5sum resources.txt)
{ add_res 102 "Lab A" 2 40; echo 0; } | run
expect "error while reading bookings.txt"
expect "saving is switched off"
expect "Goodbye! (Some data could not be saved.)"
expect_eq "resources.txt checksum" "$SUM1" "$(md5sum resources.txt)"
tend

fresh; tbegin T37 "Normal exit saves all three data files" "Message shown; resources.txt, bookings.txt, waiting.txt exist; no .tmp left"
{ add_res 101 "Room 101" 1 60; echo 0; } | run
expect "Data saved. Goodbye!"
expect_eq "files present" "3" "$(ls resources.txt bookings.txt waiting.txt 2>/dev/null | wc -l | tr -d ' ')"
expect_eq "leftover .tmp files" "0" "$(ls *.tmp 2>/dev/null | wc -l | tr -d ' ')"
tend

fresh; tbegin T38 "Rename cannot overwrite an existing file (Windows-like)" "Save still works through the backup step; both records saved; no .bak/.tmp left"
printf '101 Room_101 1 60\n' > resources.txt
OLDBIN=$BIN; BIN="$WORK/booking_plain"; RUN_ENV="LD_PRELOAD=$WORK/fake_rename.so FAKE_RENAME=WIN"
{ add_res 102 "Lab A" 2 40; echo 0; } | run
BIN=$OLDBIN; RUN_ENV=""
if [ "$SHIM_OK" = 0 ]; then note_ok "test helper library built"; else note_bad "helper library could not be built"; fi
expect "Data saved. Goodbye!"
expect_eq "lines in resources.txt" "2" "$(wc -l < resources.txt | tr -d ' ')"
expect_eq "leftover .bak/.tmp files" "0" "$(ls *.bak *.tmp 2>/dev/null | wc -l | tr -d ' ')"
tend

fresh; tbegin T39 "Renaming the new file always fails" "Old resources.txt restored/unchanged (same checksum); no .bak left; new data kept in .tmp"
printf '101 Room_101 1 60\n' > resources.txt; SUM1=$(md5sum resources.txt)
OLDBIN=$BIN; BIN="$WORK/booking_plain"; RUN_ENV="LD_PRELOAD=$WORK/fake_rename.so FAKE_RENAME=TMPFAIL"
{ add_res 102 "Lab A" 2 40; echo 0; } | run
BIN=$OLDBIN; RUN_ENV=""
expect "Warning: could not replace resources.txt. It was restored; new data is in resources.txt.tmp."
expect "Goodbye! (Some data could not be saved.)"
expect_eq "resources.txt checksum" "$SUM1" "$(md5sum resources.txt)"
expect_eq "leftover .bak files" "0" "$(ls *.bak 2>/dev/null | wc -l | tr -d ' ')"
expect_eq "lines in resources.txt.tmp" "2" "$(wc -l < resources.txt.tmp | tr -d ' ')"
tend

fresh; tbegin T40 "The old file cannot be moved aside" "Old resources.txt unchanged (same checksum); no .bak created; new data kept in .tmp"
printf '101 Room_101 1 60\n' > resources.txt; SUM1=$(md5sum resources.txt)
OLDBIN=$BIN; BIN="$WORK/booking_plain"; RUN_ENV="LD_PRELOAD=$WORK/fake_rename.so FAKE_RENAME=BAKFAIL"
{ add_res 102 "Lab A" 2 40; echo 0; } | run
BIN=$OLDBIN; RUN_ENV=""
expect "Warning: could not replace resources.txt. It was not changed; new data is in resources.txt.tmp."
expect_eq "resources.txt checksum" "$SUM1" "$(md5sum resources.txt)"
expect_eq "leftover .bak files" "0" "$(ls *.bak 2>/dev/null | wc -l | tr -d ' ')"
expect_eq "lines in resources.txt.tmp" "2" "$(wc -l < resources.txt.tmp | tr -d ' ')"
tend

fresh; tbegin T41 "Memory safety over all tests" "No AddressSanitizer / LeakSanitizer / UndefinedBehaviorSanitizer report in any test"
if [ "$USE_PLAIN" = "1" ]; then note_ok "not applicable for the plain build"
elif [ "$SANITIZER_PROBLEM_ANYWHERE" = 0 ]; then note_ok "no sanitizer report in tests T02-T37 (T38-T40 use the normal build)"
else note_bad "a sanitizer reported a problem"; fi
tend

echo "-----------------------------------------"
echo "Passed: $PASSES   Failed: $FAILS   ($BUILD_NAME)"
[ "$FAILS" = 0 ]
