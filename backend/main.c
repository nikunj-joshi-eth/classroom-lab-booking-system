/* Classroom and Laboratory Resource Booking System
   Nikunj Joshi (137) - Resource Management
   Mansi (131) - Booking Management and Reports
   Khushi (148) - Cancellation and Waiting List
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RES        50
#define MAX_BOOK       200
#define NUM_SLOTS      4
#define NAME_LEN       30
#define MAX_BOOKING_ID 99999
#define RES_FILE       "resources.txt"
#define BOOK_FILE      "bookings.txt"
#define WAIT_FILE      "waiting.txt"

typedef struct {
    int  id;
    char name[NAME_LEN];
    int  type;              
    int  capacity;
} Resource;

typedef struct {
    int  bookingId;
    int  resId;
    char date[11];          
    int  slot;              
    char bookedBy[NAME_LEN];
    int  status;            
} Booking;


typedef struct Node {
    int  resId;
    char date[11];
    int  slot;
    char bookedBy[NAME_LEN];
    struct Node *next;
} Node;

Resource resources[MAX_RES];
int resCount = 0;

Booking bookings[MAX_BOOK];
int bookCount = 0;
int nextBookingId = 1;

Node *front = NULL;         
Node *rear = NULL;          
int waitCount = 0;

int apiMode = 0;            

const char *slotNames[NUM_SLOTS + 1] = {
    "", "9-10 AM", "10-11 AM", "11-12 PM", "2-3 PM"
};


void freeWaitingList(void);
int  enqueueWaiting(int resId, const char *date, int slot, const char *name);
/* Khushi (148) - Cancellation and Waiting List: linked-list queue */
int  isInWaitingList(int resId, const char *date, int slot, const char *name);
int  saveAll(void);


/* Shared input and validation - Nikunj Joshi, Mansi, Khushi */
int isSpaceChar(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}


int readLine(char *buf, int size) {
    int len, ch;
    if (fgets(buf, size, stdin) == NULL) {
        printf("\nInput ended. Exiting.\n");
        freeWaitingList();
        exit(0);
    }
    len = (int)strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        return 1;
    }
    ch = getchar();
    if (ch == EOF || ch == '\n')
        return 1;
    while (ch != '\n' && ch != EOF)     
        ch = getchar();
    return 0;
}


int readInt(const char *prompt, int min, int max) {
    char buf[50], *end;
    long val;
    while (1) {
        if (!apiMode) printf("%s", prompt);
        if (readLine(buf, sizeof(buf))) {
            val = strtol(buf, &end, 10);
            while (isSpaceChar(*end))
                end++;
            if (end != buf && *end == '\0' && val >= min && val <= max)
                return (int)val;
        }
        printf("Invalid input! Enter a number from %d to %d.\n", min, max);
    }
}


void readString(const char *prompt, char *dest, int size) {
    char buf[100], *p;
    int i, len;
    while (1) {
        if (!apiMode) printf("%s", prompt);
        if (!readLine(buf, sizeof(buf))) {
            printf("Input too long! Enter 1 to %d characters.\n", size - 1);
            continue;
        }
        p = buf;
        while (isSpaceChar(*p))
            p++;
        len = (int)strlen(p);
        while (len > 0 && isSpaceChar(p[len - 1]))
            p[--len] = '\0';
        if (len == 0 || len >= size) {
            printf("Invalid input! Enter 1 to %d characters.\n", size - 1);
            continue;
        }
        for (i = 0; i < len; i++)
            if (isSpaceChar(p[i]))
                p[i] = '_';
        strcpy(dest, p);
        return;
    }
}

int isLeap(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int month, int year) {
    int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeap(year))
        return 29;
    return days[month - 1];
}


int validDate(const char *d) {
    int i, day, month, year;
    if (strlen(d) != 10)
        return 0;
    for (i = 0; i < 10; i++) {
        if (i == 2 || i == 5) {
            if (d[i] != '-')
                return 0;
        } else if (d[i] < '0' || d[i] > '9')
            return 0;
    }
    day = (d[0] - '0') * 10 + (d[1] - '0');
    month = (d[3] - '0') * 10 + (d[4] - '0');
    year = atoi(d + 6);
    if (year < 2024 || year > 2100)
        return 0;
    if (month < 1 || month > 12)
        return 0;
    return day >= 1 && day <= daysInMonth(month, year);
}

void readDate(char *dest) {
    char buf[50];
    while (1) {
        readString("Enter date (DD-MM-YYYY): ", buf, 50);
        if (validDate(buf)) {
            strcpy(dest, buf);
            return;
        }
        printf("Invalid date! Use a real date from 2024 to 2100, e.g. 15-08-2026.\n");
    }
}

int readSlot(void) {
    int i;
    if (!apiMode) {
        printf("Time slots:\n");
        for (i = 1; i <= NUM_SLOTS; i++)
            printf("  %d. %s\n", i, slotNames[i]);
    }
    return readInt("Choose slot: ", 1, NUM_SLOTS);
}


/* Nikunj Joshi (137) - Resource Management: search, add, view, sort */
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
    readString("Enter name (e.g. Room101): ", r.name, NAME_LEN);
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
    if (resCount == 0) {
        printf("No resources available.\n");
        return;
    }
    id = readInt("Enter Resource ID to search: ", 1, 9999);
    idx = findResourceIndex(id);
    if (idx == -1)
        printf("Resource with ID %d not found.\n", id);
    else
        printf("Found: ID=%d, Name=%s, Type=%s, Capacity=%d\n", resources[idx].id,
               resources[idx].name, typeName(resources[idx].type),
               resources[idx].capacity);
}


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


/* Mansi (131) - Booking Management: availability, booking, reports */
int findActiveBooking(int resId, const char *date, int slot) {
    int i;
    for (i = 0; i < bookCount; i++)
        if (bookings[i].status == 1 && bookings[i].resId == resId &&
            bookings[i].slot == slot && strcmp(bookings[i].date, date) == 0)
            return i;
    return -1;
}


int isAvailable(int resId, const char *date, int slot) {
    return findActiveBooking(resId, date, slot) == -1;
}

int bookingIdExists(int id) {
    int i;
    for (i = 0; i < bookCount; i++)
        if (bookings[i].bookingId == id)
            return 1;
    return 0;
}


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
    char date[11], name[NAME_LEN];

    if (resCount == 0) {
        printf("No resources available. Add one first.\n");
        return;
    }
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
    readString("Enter your name: ", name, NAME_LEN);

    idx = findActiveBooking(resId, date, slot);
    if (idx == -1) {                                    
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
    printf("\n%-8s %-18s %-12s %-10s %-15s\n", "BookID", "Room", "Date", "Slot", "Booked By");
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
    if (!found)
        printf("No active bookings.\n");
}


void bookingHistory(void) {
    int i, idx;
    if (bookCount == 0) {
        printf("No booking history.\n");
        return;
    }
    printf("\n%-8s %-18s %-12s %-10s %-15s %-10s\n", "BookID", "Room", "Date", "Slot",
           "Booked By", "Status");
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
    int count[MAX_RES] = {0};       

    for (i = 0; i < bookCount; i++) {
        if (bookings[i].status == 1)
            active++;
        else
            cancelled++;
        idx = findResourceIndex(bookings[i].resId);
        if (idx != -1)
            count[idx]++;
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


int isInWaitingList(int resId, const char *date, int slot, const char *name) {
    Node *cur;
    for (cur = front; cur != NULL; cur = cur->next)
        if (cur->resId == resId && cur->slot == slot &&
            strcmp(cur->date, date) == 0 && strcmp(cur->bookedBy, name) == 0)
            return 1;
    return 0;
}


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
        front = rear = n;           
    else {
        rear->next = n;
        rear = n;
    }
    waitCount++;
    return 1;
}


int processWaiting(int resId, const char *date, int slot) {
    Node *prev = NULL, *cur = front;
    int id;
    while (cur != NULL) {
        if (cur->resId == resId && cur->slot == slot && strcmp(cur->date, date) == 0) {
            id = createBooking(resId, date, slot, cur->bookedBy);
            if (id == 0) {          
                printf("Booking storage is full. Waiting request of %s could not be confirmed.\n",
                       cur->bookedBy);
                return 0;
            }
            printf("Waiting request of %s moved to a confirmed booking (Booking ID = %d).\n",
                   cur->bookedBy, id);
            if (prev == NULL)       
                front = cur->next;
            else
                prev->next = cur->next;
            if (cur == rear)
                rear = prev;
            free(cur);
            waitCount--;
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}


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
    if (bookCount == 0) {
        printf("No bookings to cancel.\n");
        return;
    }
    id = readInt("Enter Booking ID to cancel: ", 1, 999999);
    for (i = 0; i < bookCount; i++)         
        if (bookings[i].bookingId == id) {
            found = i;
            break;
        }
    if (found == -1) {
        printf("Booking ID %d not found.\n", id);
        return;
    }
    if (bookings[found].status == 0) {
        printf("Booking %d is already cancelled.\n", id);
        return;
    }

    bookings[found].status = 0;             
    printf("Booking %d cancelled.\n", id);
    processWaiting(bookings[found].resId, bookings[found].date, bookings[found].slot);
    saveAll();
}

void viewWaitingList(void) {
    Node *cur = front;
    int pos = 1, idx;
    if (cur == NULL) {
        printf("Waiting list is empty.\n");
        return;
    }
    printf("\n%-4s %-18s %-12s %-10s %-15s\n", "No.", "Room", "Date", "Slot", "Name");
    printf("-----------------------------------------------------------\n");
    while (cur != NULL) {                   
        idx = findResourceIndex(cur->resId);
        printf("%-4d %-18s %-12s %-10s %-15s\n", pos++,
               idx != -1 ? resources[idx].name : "Unknown", cur->date,
               slotNames[cur->slot], cur->bookedBy);
        cur = cur->next;
    }
}


/* Shared file handling - Nikunj Joshi, Mansi, Khushi */
void slotText(int slot, char *out) {
    int i;
    strcpy(out, slotNames[slot]);
    for (i = 0; out[i] != '\0'; i++)
        if (out[i] == ' ')
            out[i] = '_';
}

int slotFromText(const char *text) {
    char tmp[16];
    int i;
    for (i = 1; i <= NUM_SLOTS; i++) {
        slotText(i, tmp);
        if (strcmp(tmp, text) == 0)
            return i;
    }
    return 0;
}

int typeFromText(const char *text) {
    if (strcmp(text, "Classroom") == 0)
        return 1;
    if (strcmp(text, "Laboratory") == 0)
        return 2;
    return 0;
}

int statusFromText(const char *text) {
    if (strcmp(text, "Active") == 0)
        return 1;
    if (strcmp(text, "Cancelled") == 0)
        return 0;
    return -1;
}


int saveAll(void) {
    FILE *fp;
    Node *cur;
    int i, idx, ok = 1;
    char stext[16];

    fp = fopen(RES_FILE, "w");
    if (fp == NULL) {
        printf("Warning: cannot write %s.\n", RES_FILE);
        ok = 0;
    } else {
        fprintf(fp, "%-6s %-20s %-12s %-8s\n", "ID", "Name", "Type", "Capacity");
        for (i = 0; i < resCount; i++)
            fprintf(fp, "%-6d %-20s %-12s %-8d\n", resources[i].id, resources[i].name,
                    typeName(resources[i].type), resources[i].capacity);
        fclose(fp);
    }

    fp = fopen(BOOK_FILE, "w");
    if (fp == NULL) {
        printf("Warning: cannot write %s.\n", BOOK_FILE);
        ok = 0;
    } else {
        fprintf(fp, "%-10s %-11s %-20s %-12s %-10s %-20s %-10s\n", "BookingID",
                "ResourceID", "Room", "Date", "Slot", "BookedBy", "Status");
        for (i = 0; i < bookCount; i++) {
            idx = findResourceIndex(bookings[i].resId);
            slotText(bookings[i].slot, stext);
            fprintf(fp, "%-10d %-11d %-20s %-12s %-10s %-20s %-10s\n", bookings[i].bookingId,
                    bookings[i].resId, idx != -1 ? resources[idx].name : "Unknown",
                    bookings[i].date, stext, bookings[i].bookedBy,
                    bookings[i].status ? "Active" : "Cancelled");
        }
        fclose(fp);
    }

    fp = fopen(WAIT_FILE, "w");
    if (fp == NULL) {
        printf("Warning: cannot write %s.\n", WAIT_FILE);
        ok = 0;
    } else {
        fprintf(fp, "%-4s %-11s %-20s %-12s %-10s %-20s\n", "No.", "ResourceID", "Room",
                "Date", "Slot", "Name");
        for (cur = front, i = 1; cur != NULL; cur = cur->next, i++) {
            idx = findResourceIndex(cur->resId);
            slotText(cur->slot, stext);
            fprintf(fp, "%-4d %-11d %-20s %-12s %-10s %-20s\n", i, cur->resId,
                    idx != -1 ? resources[idx].name : "Unknown", cur->date, stext,
                    cur->bookedBy);
        }
        fclose(fp);
    }
    return ok;
}


void loadAll(void) {
    FILE *fp;
    char line[200], dtmp[16], ntmp[40], ttmp[16], stmp[16];
    Resource r;
    Booking b;
    int resId, slot, pos, skipped = 0;

    fp = fopen(RES_FILE, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (line[0] < '0' || line[0] > '9')
                continue;
            if (sscanf(line, "%d %39s %15s %d", &r.id, ntmp, ttmp, &r.capacity) == 4 &&
                strlen(ntmp) < NAME_LEN && r.id >= 1 && r.id <= 9999 &&
                (r.type = typeFromText(ttmp)) != 0 && r.capacity >= 1 && r.capacity <= 500 &&
                findResourceIndex(r.id) == -1 && resCount < MAX_RES) {
                strcpy(r.name, ntmp);
                resources[resCount++] = r;
            } else
                skipped++;
        }
        fclose(fp);
    }

    fp = fopen(BOOK_FILE, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (line[0] < '0' || line[0] > '9')
                continue;
            if (sscanf(line, "%d %d %*s %15s %15s %39s %15s", &b.bookingId, &b.resId, dtmp,
                       stmp, ntmp, ttmp) == 6 &&
                (b.slot = slotFromText(stmp)) != 0 &&
                (b.status = statusFromText(ttmp)) != -1 &&
                b.bookingId >= 1 && b.bookingId <= MAX_BOOKING_ID &&
                findResourceIndex(b.resId) != -1 && validDate(dtmp) &&
                strlen(ntmp) < NAME_LEN && !bookingIdExists(b.bookingId) &&
                (b.status == 0 || isAvailable(b.resId, dtmp, b.slot)) &&
                bookCount < MAX_BOOK) {
                strcpy(b.date, dtmp);
                strcpy(b.bookedBy, ntmp);
                bookings[bookCount++] = b;
                if (b.bookingId >= nextBookingId)
                    nextBookingId = b.bookingId + 1;
            } else
                skipped++;
        }
        fclose(fp);
    }

    fp = fopen(WAIT_FILE, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (line[0] < '0' || line[0] > '9')
                continue;
            if (sscanf(line, "%d %d %*s %15s %15s %39s", &pos, &resId, dtmp, stmp, ntmp) == 5 &&
                findResourceIndex(resId) != -1 && validDate(dtmp) &&
                (slot = slotFromText(stmp)) != 0 && strlen(ntmp) < NAME_LEN &&
                enqueueWaiting(resId, dtmp, slot, ntmp))
                continue;
            skipped++;
        }
        fclose(fp);
    }

    if (skipped > 0)
        printf("Warning: %d invalid or duplicate record(s) in the data files were ignored.\n",
               skipped);
}

#define ADMIN_PASS   "1234"
#define TEACHER_PASS "123"

/* Shared access and menus - Nikunj Joshi, Mansi, Khushi */
const char *roleName(int role) {
    if (role == 1)
        return "Administrator";
    if (role == 2)
        return "Teacher";
    return "Student";
}


int login(void) {
    int choice, retry;
    char pass[NAME_LEN];

    while (1) {
        printf("\n===== RESOURCE BOOKING SYSTEM =====\n");
        printf("\nSelect Login Type:\n\n");
        printf("1. Administrator\n");
        printf("2. Teacher\n");
        printf("3. Student\n");
        printf("0. Exit\n");
        choice = readInt("\nEnter your choice: ", 0, 3);

        if (choice == 0 || choice == 3)
            return choice;

        retry = 1;
        while (retry) {                     
            readString("Enter password: ", pass, NAME_LEN);
            if ((choice == 1 && strcmp(pass, ADMIN_PASS) == 0) ||
                (choice == 2 && strcmp(pass, TEACHER_PASS) == 0))
                return choice;
            printf("Incorrect password!\n");
            retry = readInt("Try again? (1 = Yes, 0 = Back to login menu): ", 0, 1);
        }
    }
}


void runApi(int role, const char *action) {
    char pass[NAME_LEN];
    int adminOnly   = (strcmp(action, "addResource") == 0 || strcmp(action, "sortResources") == 0);
    int notStudent  = (strcmp(action, "bookResource") == 0 || strcmp(action, "cancelBooking") == 0);

    if ((adminOnly && role != 1) || (notStudent && role == 3)) {
        printf("Access denied!\n");
        return;
    }
    if (strcmp(action, "login") == 0) {
        if (role == 3) { printf("LOGIN_OK\n"); return; }
        readString("", pass, NAME_LEN);
        if ((role == 1 && strcmp(pass, ADMIN_PASS) == 0) ||
            (role == 2 && strcmp(pass, TEACHER_PASS) == 0))
            printf("LOGIN_OK\n");
        else
            printf("LOGIN_FAIL\n");
    }
    else if (strcmp(action, "addResource") == 0)        { addResource(); saveAll(); }
    else if (strcmp(action, "viewResources") == 0)      viewResources();
    else if (strcmp(action, "searchResource") == 0)     searchResource();
    else if (strcmp(action, "sortResources") == 0)      { sortResources(); saveAll(); }
    else if (strcmp(action, "bookResource") == 0)       bookResource();
    else if (strcmp(action, "viewBookings") == 0)       viewBookings();
    else if (strcmp(action, "cancelBooking") == 0)      cancelBooking();
    else if (strcmp(action, "viewWaitingList") == 0)    viewWaitingList();
    else if (strcmp(action, "bookingHistory") == 0)     bookingHistory();
    else if (strcmp(action, "bookingStatistics") == 0)  bookingStatistics();
    else printf("Unknown action.\n");
}

int main(int argc, char *argv[]) {
    int choice, role, running = 1;


    if (argc == 4 && strcmp(argv[1], "--api") == 0) {
        apiMode = 1;
        loadAll();
        runApi(atoi(argv[2]), argv[3]);
        freeWaitingList();
        return 0;
    }

    loadAll();                                  
    printf("\n(Data loaded: %d resources, %d bookings, %d waiting requests)\n",
           resCount, bookCount, waitCount);

    while (running) {
        role = login();
        if (role == 0) {
            printf("Goodbye!\n");
            break;
        }
        printf("\nLogged in as %s.\n", roleName(role));

        choice = -1;
        while (choice != 0 && choice != 11) {
            printf("\n===== RESOURCE BOOKING SYSTEM (%s) =====\n", roleName(role));
            if (role == 1)
                printf("1. Add Classroom/Lab\n");
            printf("2. View Resources\n");
            printf("3. Search Resource\n");
            if (role == 1)
                printf("4. Sort Resources\n");
            if (role != 3)
                printf("5. Book Resource\n");
            printf("6. View Bookings\n");
            if (role != 3)
                printf("7. Cancel Booking\n");
            printf("8. View Waiting List\n");
            printf("9. Booking History\n");
            printf("10. Booking Statistics\n");
            printf("11. Logout\n");
            printf("0. Exit\n");
            choice = readInt("Enter your choice: ", 0, 11);

            switch (choice) {
                case 1:
                    if (role == 1) { addResource(); saveAll(); }
                    else printf("Access denied!\n");
                    break;
                case 2:  viewResources();            break;
                case 3:  searchResource();           break;
                case 4:
                    if (role == 1) { sortResources(); saveAll(); }
                    else printf("Access denied!\n");
                    break;
                case 5:
                    if (role != 3) bookResource();
                    else printf("Access denied!\n");
                    break;
                case 6:  viewBookings();             break;
                case 7:
                    if (role != 3) cancelBooking();
                    else printf("Access denied!\n");
                    break;
                case 8:  viewWaitingList();          break;
                case 9:  bookingHistory();           break;
                case 10: bookingStatistics();        break;
                case 11:
                    saveAll();
                    printf("Logged out.\n");
                    break;
                case 0:
                    if (saveAll())
                        printf("Data saved. Goodbye!\n");
                    else
                        printf("Goodbye! (Some data could not be saved.)\n");
                    running = 0;
                    break;
            }
        }
    }

    freeWaitingList();
    return 0;
}
