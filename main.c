/*
 * Classroom and Laboratory Resource Booking System
 * Language : C (standard C99, no external libraries)
 * Team     : Nikunj Joshi (137), Mansi (131), Khushi (148)
 *
 * Data structures used:
 *   - Array of structures : resources and bookings
 *   - Linked list queue   : waiting list (FIFO)
 *   - Linear search, Bubble sort
 *   - Text files          : data is saved and loaded between runs
 *
 * Storage limits: 50 resources, 200 booking records (cancelled bookings
 * stay in history, so they also use space), waiting list has no fixed limit.
 *
 * Saving writes to "<file>.tmp" first and replaces the old file only when the
 * new one was written completely. Loading checks every record.
 *
 * Compile: gcc -std=c99 -Wall -Wextra -pedantic main.c -o booking
 * Run    : ./booking
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define MAX_RES    50     /* maximum number of resources */
#define MAX_BOOK   200    /* maximum number of booking records (history included) */
#define NUM_SLOTS  4      /* fixed time slots */
#define MAX_BOOKING_ID 99999   /* largest booking ID accepted from a data file */
#define RES_FILE   "resources.txt"
#define BOOK_FILE  "bookings.txt"
#define WAIT_FILE  "waiting.txt"

/* ---------- Data structures ---------- */

typedef struct {
    int  id;
    char name[30];
    int  type;            /* 1 = Classroom, 2 = Laboratory */
    int  capacity;
} Resource;

typedef struct {
    int  bookingId;
    int  resId;
    char date[11];        /* DD-MM-YYYY */
    int  slot;            /* 1..NUM_SLOTS */
    char bookedBy[30];
    int  status;          /* 1 = Active, 0 = Cancelled */
} Booking;

/* Node of the waiting list (linked-list based queue) */
typedef struct Node {
    int  resId;
    char date[11];
    int  slot;
    char bookedBy[30];
    struct Node *next;
} Node;

/* ---------- Global data ---------- */

Resource resources[MAX_RES];
int resCount = 0;

Booking bookings[MAX_BOOK];
int bookCount = 0;
int nextBookingId = 1;

Node *front = NULL;       /* queue front (first to be served) */
Node *rear = NULL;        /* queue rear (new requests join here) */
int waitCount = 0;        /* number of requests in the waiting list */
int loadProblem = 0;      /* 1 if a data file could not be read: saving is then switched off */

const char *slotNames[NUM_SLOTS + 1] = {
    "", "9-10 AM", "10-11 AM", "11-12 PM", "2-3 PM"
};

/* Functions that are used before they are defined */
void freeWaitingList(void);
int  enqueueWaiting(int resId, const char *date, int slot, const char *name);
int  isInWaitingList(int resId, const char *date, int slot, const char *name);
int  saveAll(void);

/* ---------- Common helper functions ---------- */

/* Called when the input ends (Ctrl+D / end of file). Data is already saved. */
void endOfInput(void) {
    printf("\nInput ended. Exiting.\n");
    freeWaitingList();
    exit(0);
}

/* Read one line safely into buf.
   Returns 0 at end of input, 1 if the line fitted, -1 if it was too long
   (the extra characters are thrown away so they do not spoil the next input). */
int readLine(char *buf, int size) {
    int len, ch;
    if (fgets(buf, size, stdin) == NULL)
        return 0;
    len = (int)strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        return 1;
    }
    ch = getchar();                        /* line had no '\n' in the buffer */
    if (ch == EOF || ch == '\n')
        return 1;
    while (ch != '\n' && ch != EOF)        /* discard the rest of the long line */
        ch = getchar();
    return -1;
}

/* Read an integer between min and max; repeat until input is valid */
int readInt(const char *prompt, int min, int max) {
    char buf[50], *end;
    long val;
    int status;
    while (1) {
        printf("%s", prompt);
        status = readLine(buf, sizeof(buf));
        if (status == 0)
            endOfInput();
        if (status == 1) {
            val = strtol(buf, &end, 10);
            while (*end == ' ' || *end == '\t' || *end == '\r')
                end++;                     /* allow spaces after the number */
            if (end != buf && *end == '\0' && val >= min && val <= max)
                return (int)val;
        }
        printf("Invalid input! Enter a number from %d to %d.\n", min, max);
    }
}

/* Read a non-empty word/name of at most size-1 characters.
   Spaces are replaced by '_' (keeps the data files simple). */
void readString(const char *prompt, char *dest, int size) {
    char buf[100], *p;
    int i, len, status;
    while (1) {
        printf("%s", prompt);
        status = readLine(buf, sizeof(buf));
        if (status == 0)
            endOfInput();
        if (status == -1) {
            printf("Input too long! Enter 1 to %d characters.\n", size - 1);
            continue;
        }
        p = buf;
        while (*p == ' ' || *p == '\t')    /* remove leading spaces */
            p++;
        len = (int)strlen(p);
        while (len > 0 && isspace((unsigned char)p[len - 1]))
            p[--len] = '\0';               /* remove trailing spaces */
        if (len == 0 || len >= size) {
            printf("Invalid input! Enter 1 to %d characters.\n", size - 1);
            continue;
        }
        for (i = 0; i < len; i++)
            if (isspace((unsigned char)p[i])) p[i] = '_';
        strcpy(dest, p);
        return;
    }
}

/* Leap year: divisible by 4, but century years must be divisible by 400 */
int isLeap(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int month, int year) {
    int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeap(year))
        return 29;
    return days[month - 1];
}

/* Check date format DD-MM-YYYY and that the date really exists */
int validDate(const char *d) {
    int i, day, month, year;
    if (strlen(d) != 10) return 0;
    for (i = 0; i < 10; i++) {
        if (i == 2 || i == 5) { if (d[i] != '-') return 0; }
        else if (!isdigit((unsigned char)d[i])) return 0;
    }
    day = (d[0] - '0') * 10 + (d[1] - '0');
    month = (d[3] - '0') * 10 + (d[4] - '0');
    year = atoi(d + 6);
    if (year < 2024 || year > 2100) return 0;
    if (month < 1 || month > 12) return 0;
    return day >= 1 && day <= daysInMonth(month, year);
}

void readDate(char *dest) {
    char buf[50];
    while (1) {
        readString("Enter date (DD-MM-YYYY): ", buf, 50);
        if (validDate(buf)) { strcpy(dest, buf); return; }
        printf("Invalid date! Use a real date from 2024 to 2100, e.g. 15-08-2026.\n");
    }
}

int readSlot(void) {
    int i;
    printf("Time slots:\n");
    for (i = 1; i <= NUM_SLOTS; i++)
        printf("  %d. %s\n", i, slotNames[i]);
    return readInt("Choose slot: ", 1, NUM_SLOTS);
}

/* =====================================================================
 * MODULE 1 : RESOURCE MANAGEMENT   (Nikunj Joshi - 25/SET/BCSE/137)
 * DSA: Arrays, Structures, Linear Search, Bubble Sort
 * ===================================================================== */

/* Linear search: returns array index of resource id, or -1 */
int findResourceIndex(int id) {
    int i;
    for (i = 0; i < resCount; i++)
        if (resources[i].id == id)
            return i;
    return -1;
}

const char *typeName(int type) {
    return (type == 1) ? "Classroom" : "Laboratory";
}

void addResource(void) {
    Resource r;
    if (resCount >= MAX_RES) {
        printf("Resource list is full (maximum %d)!\n", MAX_RES);
        return;
    }
    r.id = readInt("Enter Resource ID (1-9999): ", 1, 9999);
    if (findResourceIndex(r.id) != -1) {
        printf("Error: Resource ID %d already exists!\n", r.id);
        return;
    }
    readString("Enter name (e.g. Room101): ", r.name, 30);
    r.type = readInt("Type (1 = Classroom, 2 = Laboratory): ", 1, 2);
    r.capacity = readInt("Enter capacity (1-500): ", 1, 500);
    resources[resCount++] = r;
    printf("%s added successfully.\n", typeName(r.type));
}

void viewResources(void) {
    int i;
    if (resCount == 0) {
        printf("No resources available.\n");
        return;
    }
    printf("\n%-6s %-20s %-12s %-8s\n", "ID", "Name", "Type", "Capacity");
    printf("------------------------------------------------\n");
    for (i = 0; i < resCount; i++)
        printf("%-6d %-20s %-12s %-8d\n", resources[i].id, resources[i].name,
               typeName(resources[i].type), resources[i].capacity);
}

void searchResource(void) {
    int id, idx;
    if (resCount == 0) { printf("No resources available.\n"); return; }
    id = readInt("Enter Resource ID to search: ", 1, 9999);
    idx = findResourceIndex(id);
    if (idx == -1)
        printf("Resource with ID %d not found.\n", id);
    else
        printf("Found: ID=%d, Name=%s, Type=%s, Capacity=%d\n", resources[idx].id,
               resources[idx].name, typeName(resources[idx].type),
               resources[idx].capacity);
}

/* Bubble sort resources by ID (ascending) */
void sortResources(void) {
    int i, j;
    Resource temp;
    if (resCount < 2) {
        printf("Not enough resources to sort.\n");
        return;
    }
    for (i = 0; i < resCount - 1; i++)
        for (j = 0; j < resCount - i - 1; j++)
            if (resources[j].id > resources[j + 1].id) {
                temp = resources[j];
                resources[j] = resources[j + 1];
                resources[j + 1] = temp;
            }
    printf("Resources sorted by ID.\n");
    viewResources();
}

/* =====================================================================
 * MODULE 2 : BOOKING MANAGEMENT AND REPORTS    (Mansi - 25/SET/BCSE/131)
 * DSA: Arrays, Structures, Searching
 * ===================================================================== */

/* Linear search: index of the ACTIVE booking for this room, date and slot,
   or -1 if there is none (cancelled bookings are ignored). */
int findActiveBooking(int resId, const char *date, int slot) {
    int i;
    for (i = 0; i < bookCount; i++)
        if (bookings[i].status == 1 && bookings[i].resId == resId &&
            bookings[i].slot == slot && strcmp(bookings[i].date, date) == 0)
            return i;
    return -1;
}

/* Is this room free on this date and slot? */
int isAvailable(int resId, const char *date, int slot) {
    return findActiveBooking(resId, date, slot) == -1;
}

/* Linear search: does a booking with this booking ID already exist? */
int bookingIdExists(int id) {
    int i;
    for (i = 0; i < bookCount; i++)
        if (bookings[i].bookingId == id)
            return 1;
    return 0;
}

/* Store a new active booking. Returns booking id, or 0 if the array is full */
int createBooking(int resId, const char *date, int slot, const char *name) {
    Booking *b;
    if (bookCount >= MAX_BOOK)
        return 0;
    b = &bookings[bookCount++];
    b->bookingId = nextBookingId++;
    b->resId = resId;
    strcpy(b->date, date);
    b->slot = slot;
    strcpy(b->bookedBy, name);
    b->status = 1;
    return b->bookingId;
}

void bookResource(void) {
    int resId, slot, id, idx;
    char date[11], name[30];

    if (resCount == 0) { printf("No resources available. Add one first.\n"); return; }
    if (bookCount >= MAX_BOOK) {
        printf("Booking storage is full (%d records). No new bookings can be made.\n", MAX_BOOK);
        return;
    }
    resId = readInt("Enter Resource ID to book: ", 1, 9999);
    if (findResourceIndex(resId) == -1) {
        printf("Resource with ID %d not found.\n", resId);
        return;
    }
    readDate(date);
    slot = readSlot();
    readString("Enter faculty name: ", name, 30);

    idx = findActiveBooking(resId, date, slot);
    if (idx == -1) {                                   /* room is free */
        id = createBooking(resId, date, slot, name);
        if (id != 0)
            printf("Booking confirmed! Booking ID = %d\n", id);
    } else if (strcmp(bookings[idx].bookedBy, name) == 0) {
        printf("You already hold this booking (Booking ID = %d).\n", bookings[idx].bookingId);
    } else {
        printf("Sorry, this room is already booked for that date and slot.\n");
        if (isInWaitingList(resId, date, slot, name))
            printf("You are already on the waiting list for this room, date and slot.\n");
        else if (readInt("Join the waiting list? (1 = Yes, 0 = No): ", 0, 1) == 1) {
            if (enqueueWaiting(resId, date, slot, name))
                printf("Added to waiting list.\n");
            else
                printf("Could not add to the waiting list (out of memory).\n");
        }
    }
    saveAll();
}

void viewBookings(void) {
    int i, idx, found = 0;
    printf("\n%-8s %-18s %-12s %-10s %-15s\n", "BookID", "Room", "Date", "Slot", "Faculty");
    printf("-----------------------------------------------------------------\n");
    for (i = 0; i < bookCount; i++) {
        if (bookings[i].status == 1) {
            idx = findResourceIndex(bookings[i].resId);
            printf("%-8d %-18s %-12s %-10s %-15s\n", bookings[i].bookingId,
                   idx != -1 ? resources[idx].name : "Unknown", bookings[i].date,
                   slotNames[bookings[i].slot], bookings[i].bookedBy);
            found = 1;
        }
    }
    if (!found) printf("No active bookings.\n");
}

/* =====================================================================
 * MODULE 3 : CANCELLATION AND WAITING LIST   (Khushi - 25/SET/BCSE/148)
 * DSA: Linked List, Queue (FIFO)
 * ===================================================================== */

/* Traverse the waiting list: is this exact request already waiting? */
int isInWaitingList(int resId, const char *date, int slot, const char *name) {
    Node *cur;
    for (cur = front; cur != NULL; cur = cur->next)
        if (cur->resId == resId && cur->slot == slot &&
            strcmp(cur->date, date) == 0 && strcmp(cur->bookedBy, name) == 0)
            return 1;
    return 0;
}

/* Add a request at the REAR of the queue.
   Returns 1 on success, 0 on failure (duplicate request or no memory). */
int enqueueWaiting(int resId, const char *date, int slot, const char *name) {
    Node *n;
    if (isInWaitingList(resId, date, slot, name))
        return 0;
    n = (Node *)malloc(sizeof(Node));
    if (n == NULL)
        return 0;
    n->resId = resId;
    strcpy(n->date, date);
    n->slot = slot;
    strcpy(n->bookedBy, name);
    n->next = NULL;
    if (rear == NULL)
        front = rear = n;          /* queue was empty */
    else {
        rear->next = n;
        rear = n;
    }
    waitCount++;
    return 1;
}

/* After a cancellation: serve the FIRST request (in front-to-rear order) that
   wants the same room, date and slot. Requests for other slots are skipped
   and keep their place. So the rule is FIFO among the eligible requests.
   Returns 1 if a request was confirmed, otherwise 0. */
int processWaiting(int resId, const char *date, int slot) {
    Node *prev = NULL, *cur = front;
    int id;
    while (cur != NULL) {
        if (cur->resId == resId && cur->slot == slot && strcmp(cur->date, date) == 0) {
            id = createBooking(resId, date, slot, cur->bookedBy);
            if (id == 0) {                     /* no space: request stays in the list */
                printf("Booking storage is full. Waiting request of %s could not be confirmed.\n",
                       cur->bookedBy);
                return 0;
            }
            printf("Waiting request of %s moved to a confirmed booking (Booking ID = %d).\n",
                   cur->bookedBy, id);
            if (prev == NULL) front = cur->next;   /* remove node from the list */
            else prev->next = cur->next;
            if (cur == rear) rear = prev;
            free(cur);
            waitCount--;
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

/* Used after loading the files: give every FREE slot to its earliest waiting
   request, so nobody can jump ahead of a request that was already waiting. */
void processAllWaiting(void) {
    Node *cur;
    char date[11];
    int resId, slot, changed = 1;
    while (changed) {
        changed = 0;
        for (cur = front; cur != NULL; cur = cur->next) {
            if (isAvailable(cur->resId, cur->date, cur->slot)) {
                resId = cur->resId;            /* copy, because the node may be freed */
                slot = cur->slot;
                strcpy(date, cur->date);
                changed = processWaiting(resId, date, slot);
                break;                         /* list changed (or storage full) */
            }
        }
    }
}

/* Free every remaining node (called before the program exits) */
void freeWaitingList(void) {
    Node *temp;
    while (front != NULL) {
        temp = front;
        front = front->next;
        free(temp);
    }
    rear = NULL;
    waitCount = 0;
}

void cancelBooking(void) {
    int id, i, found = -1;
    if (bookCount == 0) { printf("No bookings to cancel.\n"); return; }
    id = readInt("Enter Booking ID to cancel: ", 1, 999999);
    for (i = 0; i < bookCount; i++)
        if (bookings[i].bookingId == id) { found = i; break; }
    if (found == -1) { printf("Booking ID %d not found.\n", id); return; }
    if (bookings[found].status == 0) { printf("Booking %d is already cancelled.\n", id); return; }

    bookings[found].status = 0;                /* kept in history */
    printf("Booking %d cancelled.\n", id);
    processWaiting(bookings[found].resId, bookings[found].date, bookings[found].slot);
    saveAll();
}

void viewWaitingList(void) {
    Node *cur = front;
    int pos = 1, idx;
    if (cur == NULL) { printf("Waiting list is empty.\n"); return; }
    printf("\n%-4s %-18s %-12s %-10s %-15s\n", "No.", "Room", "Date", "Slot", "Faculty");
    printf("-----------------------------------------------------------\n");
    while (cur != NULL) {
        idx = findResourceIndex(cur->resId);
        printf("%-4d %-18s %-12s %-10s %-15s\n", pos++,
               idx != -1 ? resources[idx].name : "Unknown", cur->date,
               slotNames[cur->slot], cur->bookedBy);
        cur = cur->next;
    }
}

/* =====================================================================
 * MODULE 2 (continued) : REPORTS AND STATISTICS   (Mansi - 25/SET/BCSE/131)
 * DSA: Arrays, Traversal, Searching
 * ===================================================================== */

/* Show all bookings, active and cancelled */
void bookingHistory(void) {
    int i, idx;
    if (bookCount == 0) { printf("No booking history.\n"); return; }
    printf("\n%-8s %-18s %-12s %-10s %-15s %-10s\n", "BookID", "Room", "Date", "Slot",
           "Faculty", "Status");
    printf("---------------------------------------------------------------------------\n");
    for (i = 0; i < bookCount; i++) {
        idx = findResourceIndex(bookings[i].resId);
        printf("%-8d %-18s %-12s %-10s %-15s %-10s\n", bookings[i].bookingId,
               idx != -1 ? resources[idx].name : "Unknown", bookings[i].date,
               slotNames[bookings[i].slot], bookings[i].bookedBy,
               bookings[i].status ? "Active" : "Cancelled");
    }
}

void bookingStatistics(void) {
    int i, idx, active = 0, cancelled = 0, best = -1;
    int count[MAX_RES] = {0};      /* bookings made per resource (history included) */

    for (i = 0; i < bookCount; i++) {
        if (bookings[i].status == 1) active++;
        else cancelled++;
        idx = findResourceIndex(bookings[i].resId);
        if (idx != -1) count[idx]++;
    }

    printf("\n===== BOOKING REPORT =====\n");
    printf("Total bookings     : %d (limit %d)\n", bookCount, MAX_BOOK);
    printf("Active bookings    : %d\n", active);
    printf("Cancelled bookings : %d\n", cancelled);
    printf("Waiting requests   : %d\n", waitCount);

    for (i = 0; i < resCount; i++)
        if (count[i] > 0 && (best == -1 || count[i] > count[best]))
            best = i;
    if (best == -1)
        printf("Most booked room   : None (no bookings yet)\n");
    else
        printf("Most booked room   : %s (ID %d) with %d booking(s)\n",
               resources[best].name, resources[best].id, count[best]);

    if (resCount > 0) {
        printf("\n%-6s %-20s %-10s\n", "ID", "Room", "Bookings");
        for (i = 0; i < resCount; i++)
            printf("%-6d %-20s %-10d\n", resources[i].id, resources[i].name, count[i]);
    }
}

/* =====================================================================
 * FILE HANDLING (shared by all modules)
 * ===================================================================== */

/* Data is first written to "<name>.tmp". The old file is replaced only when
   the new one was written completely, so a failed save cannot destroy old data. */
FILE *openTemp(const char *name, char *tmp, size_t size) {
    FILE *fp;
    snprintf(tmp, size, "%s.tmp", name);
    fp = fopen(tmp, "w");
    if (fp == NULL)
        printf("Warning: cannot create %s. %s was not changed.\n", tmp, name);
    return fp;
}

/* Close the temporary file and, if all is fine, make it the real file.
   The old file is never deleted before the new one is in place.
   Returns 1 on success, 0 on failure. */
int commitFile(FILE *fp, const char *name, const char *tmp) {
    char bak[64];
    int bad = ferror(fp);
    if (fclose(fp) != 0)
        bad = 1;
    if (bad) {
        printf("Warning: error while writing %s. %s was not changed.\n", tmp, name);
        remove(tmp);
        return 0;
    }
    if (rename(tmp, name) == 0)         /* normal case: new file replaces old file */
        return 1;

    /* Some systems (e.g. Windows) cannot rename over an existing file.
       So: move the old file aside, put the new file in place, then drop the old one. */
    snprintf(bak, sizeof(bak), "%s.bak", name);
    remove(bak);                        /* leftover backup of an earlier run, if any */
    if (rename(name, bak) != 0) {       /* cannot move the old file: leave everything as it is */
        printf("Warning: could not replace %s. It was not changed; new data is in %s.\n", name, tmp);
        return 0;
    }
    if (rename(tmp, name) != 0) {       /* failed again: put the old file back */
        rename(bak, name);
        printf("Warning: could not replace %s. It was restored; new data is in %s.\n", name, tmp);
        return 0;
    }
    remove(bak);                        /* success: the backup is no longer needed */
    return 1;
}

/* Save everything. Returns 1 if all three files were saved, otherwise 0. */
int saveAll(void) {
    static int warned = 0;
    FILE *fp;
    char tmp[64];
    int i, ok = 1;
    Node *cur;

    if (loadProblem) {                  /* old files were not read properly: do not overwrite */
        if (!warned)
            printf("Warning: saving is switched off because a data file could not be read.\n");
        warned = 1;
        return 0;
    }

    fp = openTemp(RES_FILE, tmp, sizeof(tmp));
    if (fp == NULL) ok = 0;
    else {
        for (i = 0; i < resCount; i++)
            fprintf(fp, "%d %s %d %d\n", resources[i].id, resources[i].name,
                    resources[i].type, resources[i].capacity);
        if (!commitFile(fp, RES_FILE, tmp)) ok = 0;
    }

    fp = openTemp(BOOK_FILE, tmp, sizeof(tmp));
    if (fp == NULL) ok = 0;
    else {
        for (i = 0; i < bookCount; i++)
            fprintf(fp, "%d %d %s %d %s %d\n", bookings[i].bookingId, bookings[i].resId,
                    bookings[i].date, bookings[i].slot, bookings[i].bookedBy,
                    bookings[i].status);
        if (!commitFile(fp, BOOK_FILE, tmp)) ok = 0;
    }

    fp = openTemp(WAIT_FILE, tmp, sizeof(tmp));
    if (fp == NULL) ok = 0;
    else {
        for (cur = front; cur != NULL; cur = cur->next)
            fprintf(fp, "%d %s %d %s\n", cur->resId, cur->date, cur->slot, cur->bookedBy);
        if (!commitFile(fp, WAIT_FILE, tmp)) ok = 0;
    }
    return ok;
}

/* Open a data file for reading. A missing file is normal on the first run. */
FILE *openForRead(const char *name) {
    FILE *fp = fopen(name, "r");
    if (fp == NULL && errno != ENOENT) {
        printf("Warning: cannot read %s. Saving is switched off to protect your data.\n", name);
        loadProblem = 1;
    }
    return fp;
}

void closeRead(FILE *fp, const char *name) {
    if (ferror(fp)) {
        printf("Warning: error while reading %s. Saving is switched off to protect your data.\n",
               name);
        loadProblem = 1;
    }
    fclose(fp);
}

/* Read one line of a data file. Returns 0 at end of file, 1 if the line fitted,
   -1 if it was too long (the rest of that line is thrown away). */
int getFileLine(FILE *fp, char *line, int size) {
    int ch;
    if (fgets(line, size, fp) == NULL)
        return 0;
    if (strchr(line, '\n') != NULL || feof(fp))
        return 1;
    while ((ch = fgetc(fp)) != '\n' && ch != EOF)
        ;                                  /* skip the rest of the long line */
    return -1;
}

int isBlankLine(const char *s) {
    while (*s != '\0') {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* Load the three files. Every record is checked; bad records are skipped.
   A record with a missing, wrong or EXTRA field is bad (the trailing %c in each
   sscanf succeeds only when there is extra text, so we need exactly N fields). */
void loadAll(void) {
    FILE *fp;
    char line[200], extra, dtmp[16], ntmp[40];
    Resource r;
    Booking b;
    int resId, slot, holder, status, skipped = 0;

    fp = openForRead(RES_FILE);              /* resources first */
    if (fp != NULL) {
        while ((status = getFileLine(fp, line, sizeof(line))) != 0) {
            if (status == 1 && isBlankLine(line)) continue;
            if (status == 1 &&
                sscanf(line, "%d %39s %d %d %c", &r.id, ntmp, &r.type, &r.capacity, &extra) == 4 &&
                strlen(ntmp) < sizeof(r.name) &&
                r.id >= 1 && r.id <= 9999 && (r.type == 1 || r.type == 2) &&
                r.capacity >= 1 && r.capacity <= 500 && findResourceIndex(r.id) == -1) {
                if (resCount >= MAX_RES) {
                    printf("Warning: more than %d resources in file. Extra ones ignored.\n", MAX_RES);
                    break;
                }
                strcpy(r.name, ntmp);
                resources[resCount++] = r;
            } else
                skipped++;
        }
        closeRead(fp, RES_FILE);
    }

    fp = openForRead(BOOK_FILE);             /* then bookings */
    if (fp != NULL) {
        while ((status = getFileLine(fp, line, sizeof(line))) != 0) {
            if (status == 1 && isBlankLine(line)) continue;
            if (status == 1 &&
                sscanf(line, "%d %d %15s %d %39s %d %c", &b.bookingId, &b.resId, dtmp,
                       &b.slot, ntmp, &b.status, &extra) == 6 &&
                b.bookingId >= 1 && b.bookingId <= MAX_BOOKING_ID &&
                findResourceIndex(b.resId) != -1 && validDate(dtmp) &&
                b.slot >= 1 && b.slot <= NUM_SLOTS && strlen(ntmp) < sizeof(b.bookedBy) &&
                (b.status == 0 || b.status == 1) && !bookingIdExists(b.bookingId) &&
                (b.status == 0 || isAvailable(b.resId, dtmp, b.slot))) {
                if (bookCount >= MAX_BOOK) {
                    printf("Warning: more than %d bookings in file. Extra ones ignored.\n", MAX_BOOK);
                    break;
                }
                strcpy(b.date, dtmp);
                strcpy(b.bookedBy, ntmp);
                bookings[bookCount++] = b;
                if (b.bookingId >= nextBookingId)
                    nextBookingId = b.bookingId + 1;   /* new IDs never repeat old ones */
            } else
                skipped++;
        }
        closeRead(fp, BOOK_FILE);
    }

    fp = openForRead(WAIT_FILE);             /* finally the waiting list */
    if (fp != NULL) {
        while ((status = getFileLine(fp, line, sizeof(line))) != 0) {
            if (status == 1 && isBlankLine(line)) continue;
            if (status == 1 &&
                sscanf(line, "%d %15s %d %39s %c", &resId, dtmp, &slot, ntmp, &extra) == 4 &&
                findResourceIndex(resId) != -1 && validDate(dtmp) &&
                slot >= 1 && slot <= NUM_SLOTS && strlen(ntmp) < sizeof(b.bookedBy)) {
                holder = findActiveBooking(resId, dtmp, slot);
                /* a person cannot wait for a slot that he or she already holds */
                if (!(holder != -1 && strcmp(bookings[holder].bookedBy, ntmp) == 0) &&
                    enqueueWaiting(resId, dtmp, slot, ntmp))
                    continue;
            }
            skipped++;
        }
        closeRead(fp, WAIT_FILE);
    }

    if (skipped > 0)
        printf("Warning: %d invalid or duplicate record(s) in the data files were ignored.\n",
               skipped);
    processAllWaiting();                     /* free slots go to waiting requests */
}

/* =====================================================================
 * MAIN MENU
 * ===================================================================== */

int main(void) {
    int choice;

    loadAll();
    printf("\n(Data loaded: %d resources, %d bookings, %d waiting requests)\n",
           resCount, bookCount, waitCount);

    do {
        printf("\n===== FACULTY RESOURCE BOOKING SYSTEM =====\n");
        printf("1. Add Classroom/Lab\n");
        printf("2. View Resources\n");
        printf("3. Search Resource\n");
        printf("4. Sort Resources\n");
        printf("5. Book Resource\n");
        printf("6. View Bookings\n");
        printf("7. Cancel Booking\n");
        printf("8. View Waiting List\n");
        printf("9. Booking History\n");
        printf("10. Booking Statistics\n");
        printf("0. Exit\n");
        choice = readInt("Enter your choice: ", 0, 10);

        switch (choice) {
            case 1:  addResource(); saveAll();   break;
            case 2:  viewResources();            break;
            case 3:  searchResource();           break;
            case 4:  sortResources(); saveAll(); break;
            case 5:  bookResource();             break;
            case 6:  viewBookings();             break;
            case 7:  cancelBooking();            break;
            case 8:  viewWaitingList();          break;
            case 9:  bookingHistory();           break;
            case 10: bookingStatistics();        break;
            case 0:
                if (saveAll()) printf("Data saved. Goodbye!\n");
                else           printf("Goodbye! (Some data could not be saved.)\n");
                break;
        }
    } while (choice != 0);

    freeWaitingList();
    return 0;
}
