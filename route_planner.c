/* Bangladesh Route Planner
   Team ERSiSa_RouteMinds

   Build : gcc -std=c99 -Wall -Wextra -o route_planner route_planner.c
   Files : users.txt, admin.txt, destinations.txt, routes.txt are created in the
           current folder on first run.
   Note  : distances and costs are approximate sample values, not official data. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

#define MAX_DEST          30
#define NAME_LEN          50
#define MAX_DISTANCE      5000
#define MAX_COST          1000000
#define MAX_USERS         200
#define FIELD_LEN         64
#define PHONE_LEN         20
#define MAX_STORED_PATHS  2000
#define INF_VALUE         1000000000

#define USERS_FILE         "users.txt"
#define ADMIN_FILE         "admin.txt"
#define DESTINATIONS_FILE  "destinations.txt"
#define ROUTES_FILE        "routes.txt"

#define DEFAULT_ADMIN_ID   "ERSiSa_RouteMinds"
#define DEFAULT_ADMIN_PASS "diualgorithmics"

#define INPUT_BACK 0   /* user typed 0 */
#define INPUT_OK   1

typedef struct {
    char          fullName[FIELD_LEN];
    char          username[FIELD_LEN];
    unsigned long passwordHash;
    char          email[FIELD_LEN];
    char          phone[PHONE_LEN];
} User;

typedef struct {
    int nodes[MAX_DEST];
    int length;
    int totalDistance;
    int totalCost;
} Path;

User users[MAX_USERS];
int  userCount = 0;

char destNames[MAX_DEST][NAME_LEN];
int  destCount = 0;
int  distMatrix[MAX_DEST][MAX_DEST];   /* 0 = no direct road */
int  costMatrix[MAX_DEST][MAX_DEST];

static char          adminId[FIELD_LEN] = DEFAULT_ADMIN_ID;
static unsigned long adminHash = 0;


/* ---------------------------------------------------------------------- */
/*  Input and display helpers                                              */
/* ---------------------------------------------------------------------- */

void trimString(char *s)
{
    size_t len = strlen(s), start = 0;

    while (s[start] != '\0' && isspace((unsigned char)s[start]))
        start++;
    if (start > 0)
        memmove(s, s + start, len - start + 1);
    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

void readLine(const char *prompt, char *buf, size_t size)
{
    size_t len;

    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, (int)size, stdin) == NULL) {
        printf("\n\n  Input closed. Exiting.\n");
        exit(0);
    }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
    else {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
    }
    trimString(buf);
}

/* Text input. Typing 0 means "go back". */
int askText(const char *prompt, char *buf, size_t size, int allowBlank)
{
    for (;;) {
        readLine(prompt, buf, size);
        if (strcmp(buf, "0") == 0)
            return INPUT_BACK;
        if (buf[0] == '\0') {
            if (allowBlank)
                return INPUT_OK;
            printf("  [!] This field cannot be empty.\n");
            continue;
        }
        if (strchr(buf, '|') != NULL) {
            printf("  [!] The '|' character is not allowed.\n");
            continue;
        }
        return INPUT_OK;
    }
}

int askInt(const char *prompt, int *out, int lo, int hi)
{
    char buf[32], *end;
    long v;

    for (;;) {
        readLine(prompt, buf, sizeof buf);
        if (strcmp(buf, "0") == 0)
            return INPUT_BACK;
        if (buf[0] == '\0') {
            printf("  [!] Please enter a number.\n");
            continue;
        }
        errno = 0;
        v = strtol(buf, &end, 10);
        if (*end != '\0' || errno == ERANGE) {
            printf("  [!] Please enter a valid whole number.\n");
            continue;
        }
        if (v < lo || v > hi) {
            printf("  [!] The value must be between %d and %d.\n", lo, hi);
            continue;
        }
        *out = (int)v;
        return INPUT_OK;
    }
}

int askYesNo(const char *prompt, int *answer)
{
    char buf[16];

    for (;;) {
        readLine(prompt, buf, sizeof buf);
        if (strcmp(buf, "0") == 0)
            return INPUT_BACK;
        if (strcmp(buf, "y") == 0 || strcmp(buf, "Y") == 0 || strcmp(buf, "yes") == 0) {
            *answer = 1;
            return INPUT_OK;
        }
        if (strcmp(buf, "n") == 0 || strcmp(buf, "N") == 0 || strcmp(buf, "no") == 0) {
            *answer = 0;
            return INPUT_OK;
        }
        printf("  [!] Please type y or n.\n");
    }
}

/* Menu choice between 1 and max. */
int readChoice(const char *prompt, int max)
{
    char buf[32], *end;
    long v;

    for (;;) {
        readLine(prompt, buf, sizeof buf);
        errno = 0;
        v = strtol(buf, &end, 10);
        if (buf[0] == '\0' || *end != '\0' || errno == ERANGE || v < 1 || v > max) {
            printf("  [!] Invalid choice. Please enter a number from 1 to %d.\n", max);
            continue;
        }
        return (int)v;
    }
}

int equalsIgnoreCase(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

int containsIgnoreCase(const char *text, const char *pattern)
{
    size_t tl = strlen(text), pl = strlen(pattern), i, j;

    if (pl == 0)
        return 1;
    for (i = 0; i + pl <= tl; i++) {
        for (j = 0; j < pl; j++)
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)pattern[j]))
                break;
        if (j == pl)
            return 1;
    }
    return 0;
}

void printLine(char ch, int count)
{
    while (count-- > 0)
        putchar(ch);
    putchar('\n');
}

void printTitle(const char *title)
{
    printf("\n");
    printLine('=', 60);
    printf("  %s\n", title);
    printLine('=', 60);
}

void printBackHint(void)
{
    printf("  (Type 0 at any step to go back)\n\n");
}

void pauseForEnter(void)
{
    char tmp[8];
    readLine("\n  Press Enter to continue...", tmp, sizeof tmp);
}


/* ---------------------------------------------------------------------- */
/*  Route network                                                          */
/* ---------------------------------------------------------------------- */

static const char *defaultDestinations[12] = {
    "Dhaka", "Chattogram", "Cox's Bazar", "Sylhet", "Rajshahi", "Jashore",
    "Khulna", "Barishal", "Cumilla", "Mymensingh", "Bogura", "Rangpur"
};

typedef struct { int a, b, distance, cost; } SampleRoute;

static const SampleRoute defaultRoutes[] = {
    { 0,  8,  97, 250}, { 0,  1, 244, 750}, { 8,  1, 150, 320}, { 1,  2, 152, 420},
    { 0,  3, 240, 650}, { 8,  3, 215, 480}, { 0,  9, 120, 280}, { 9,  3, 200, 450},
    { 0, 10, 200, 520}, { 9, 10, 175, 400}, {10,  4, 100, 230}, {10, 11, 100, 240},
    { 0,  4, 255, 640}, { 0,  5, 190, 480}, { 5,  6,  65, 140}, { 5,  4, 235, 520},
    { 6,  7, 120, 300}, { 0,  7, 170, 450}, {11,  4, 185, 420}
};

int findDestination(const char *name)
{
    int i;
    for (i = 0; i < destCount; i++)
        if (equalsIgnoreCase(destNames[i], name))
            return i;
    return -1;
}

/* returns 0 = added, 1 = duplicate, 2 = list full */
int addDestination(const char *name)
{
    if (destCount >= MAX_DEST)
        return 2;
    if (findDestination(name) != -1)
        return 1;
    strncpy(destNames[destCount], name, NAME_LEN - 1);
    destNames[destCount][NAME_LEN - 1] = '\0';
    destCount++;
    return 0;
}

int renameDestination(int index, const char *newName)
{
    int existing = findDestination(newName);
    if (existing != -1 && existing != index)
        return 1;
    strncpy(destNames[index], newName, NAME_LEN - 1);
    destNames[index][NAME_LEN - 1] = '\0';
    return 0;
}

/* Remove a destination and close the gap in both matrices. */
void deleteDestination(int index)
{
    int n = destCount, i, j;

    for (i = index; i < n - 1; i++)
        strcpy(destNames[i], destNames[i + 1]);
    for (i = index; i < n - 1; i++)
        for (j = 0; j < n; j++) {
            distMatrix[i][j] = distMatrix[i + 1][j];
            costMatrix[i][j] = costMatrix[i + 1][j];
        }
    for (i = 0; i < n; i++)
        for (j = index; j < n - 1; j++) {
            distMatrix[i][j] = distMatrix[i][j + 1];
            costMatrix[i][j] = costMatrix[i][j + 1];
        }
    for (i = 0; i < n; i++) {
        distMatrix[n - 1][i] = distMatrix[i][n - 1] = 0;
        costMatrix[n - 1][i] = costMatrix[i][n - 1] = 0;
    }
    destNames[n - 1][0] = '\0';
    destCount--;
}

int routeExists(int a, int b)
{
    return distMatrix[a][b] > 0;
}

/* returns 0 = ok, 1 = same place, 2 = already exists, 3 = invalid values */
int addRoute(int a, int b, int distance, int cost)
{
    if (a < 0 || b < 0 || a >= destCount || b >= destCount)
        return 3;
    if (a == b)
        return 1;
    if (distance <= 0 || distance > MAX_DISTANCE || cost <= 0 || cost > MAX_COST)
        return 3;
    if (routeExists(a, b))
        return 2;
    distMatrix[a][b] = distMatrix[b][a] = distance;
    costMatrix[a][b] = costMatrix[b][a] = cost;
    return 0;
}

int updateRoute(int a, int b, int distance, int cost)
{
    if (a < 0 || b < 0 || a >= destCount || b >= destCount || a == b || !routeExists(a, b))
        return 1;
    if (distance <= 0 || distance > MAX_DISTANCE || cost <= 0 || cost > MAX_COST)
        return 2;
    distMatrix[a][b] = distMatrix[b][a] = distance;
    costMatrix[a][b] = costMatrix[b][a] = cost;
    return 0;
}

int deleteRoute(int a, int b)
{
    if (a < 0 || b < 0 || a >= destCount || b >= destCount || a == b || !routeExists(a, b))
        return 1;
    distMatrix[a][b] = distMatrix[b][a] = 0;
    costMatrix[a][b] = costMatrix[b][a] = 0;
    return 0;
}

int countRoutes(void)
{
    int i, j, count = 0;
    for (i = 0; i < destCount; i++)
        for (j = i + 1; j < destCount; j++)
            if (distMatrix[i][j] > 0)
                count++;
    return count;
}

int countNeighbours(int v)
{
    int j, count = 0;
    for (j = 0; j < destCount; j++)
        if (distMatrix[v][j] > 0)
            count++;
    return count;
}

void printDestinationList(void)
{
    int i;
    printf("\n  %-5s %s\n", "No.", "Destination");
    printf("  ");
    printLine('-', 30);
    if (destCount == 0)
        printf("  (no destinations available)\n");
    for (i = 0; i < destCount; i++)
        printf("  %-5d %s\n", i + 1, destNames[i]);
}

int printAllRoutes(void)
{
    int i, j, count = 0;

    printf("\n  %-5s %-18s %-18s %10s %13s\n", "No.", "From", "To", "Distance", "Cost");
    printf("  ");
    printLine('-', 68);
    for (i = 0; i < destCount; i++)
        for (j = i + 1; j < destCount; j++)
            if (distMatrix[i][j] > 0) {
                count++;
                printf("  %-5d %-18.18s %-18.18s %7d KM %9d BDT\n", count, destNames[i],
                       destNames[j], distMatrix[i][j], costMatrix[i][j]);
            }
    if (count == 0)
        printf("  (no routes available)\n");
    // printf("\n  Note: distances and costs are approximate sample values.\n");
    return count;
}

/* Destination can be typed as a list number or as a name. */
int askDestination(const char *prompt, int *index)
{
    char buf[NAME_LEN];
    size_t i, len;
    int allDigits, idx;

    for (;;) {
        if (askText(prompt, buf, sizeof buf, 0) == INPUT_BACK)
            return INPUT_BACK;
        len = strlen(buf);
        allDigits = 1;
        for (i = 0; i < len; i++)
            if (!isdigit((unsigned char)buf[i]))
                allDigits = 0;

        if (allDigits) {
            if (len <= 4 && atoi(buf) >= 1 && atoi(buf) <= destCount) {
                *index = atoi(buf) - 1;
                return INPUT_OK;
            }
            printf("  [!] There is no destination number %s.\n", buf);
            continue;
        }
        idx = findDestination(buf);
        if (idx < 0) {
            printf("  [!] '%s' is not in the destination list.\n", buf);
            continue;
        }
        *index = idx;
        return INPUT_OK;
    }
}

/* ---- saving and loading ---- */

static void seedDefaultData(void)
{
    size_t i;

    memset(distMatrix, 0, sizeof distMatrix);
    memset(costMatrix, 0, sizeof costMatrix);
    destCount = 0;
    for (i = 0; i < 12; i++)
        addDestination(defaultDestinations[i]);
    for (i = 0; i < sizeof defaultRoutes / sizeof defaultRoutes[0]; i++)
        addRoute(defaultRoutes[i].a, defaultRoutes[i].b,
                 defaultRoutes[i].distance, defaultRoutes[i].cost);
}

static int saveDestinations(void)
{
    FILE *f = fopen(DESTINATIONS_FILE, "w");
    int i;

    if (f == NULL) {
        printf("  [!] Could not write to %s\n", DESTINATIONS_FILE);
        return 0;
    }
    for (i = 0; i < destCount; i++)
        fprintf(f, "%s\n", destNames[i]);
    if (fclose(f) != 0) {
        printf("  [!] Error while closing %s\n", DESTINATIONS_FILE);
        return 0;
    }
    return 1;
}

static int saveRoutes(void)
{
    FILE *f = fopen(ROUTES_FILE, "w");
    int i, j;

    if (f == NULL) {
        printf("  [!] Could not write to %s\n", ROUTES_FILE);
        return 0;
    }
    for (i = 0; i < destCount; i++)
        for (j = i + 1; j < destCount; j++)
            if (distMatrix[i][j] > 0)
                fprintf(f, "%s|%s|%d|%d\n", destNames[i], destNames[j],
                        distMatrix[i][j], costMatrix[i][j]);
    if (fclose(f) != 0) {
        printf("  [!] Error while closing %s\n", ROUTES_FILE);
        return 0;
    }
    return 1;
}

int saveGraphData(void)
{
    int a = saveDestinations();
    int b = saveRoutes();
    return a && b;
}

void loadGraphData(void)
{
    FILE *f;
    char line[256];

    memset(distMatrix, 0, sizeof distMatrix);
    memset(costMatrix, 0, sizeof costMatrix);
    destCount = 0;

    f = fopen(DESTINATIONS_FILE, "r");
    if (f == NULL) {                       /* first run */
        seedDefaultData();
        if (!saveGraphData())
            printf("  [!] Warning: could not save the starting data.\n");
        return;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        trimString(line);
        if (line[0] == '\0')
            continue;
        if (strlen(line) >= NAME_LEN || addDestination(line) != 0)
            printf("  [!] Skipped invalid destination '%.40s' in %s\n", line, DESTINATIONS_FILE);
    }
    fclose(f);

    f = fopen(ROUTES_FILE, "r");
    if (f == NULL)
        return;
    while (fgets(line, sizeof line, f) != NULL) {
        char a[NAME_LEN], b[NAME_LEN];
        int d, c, u, v;

        trimString(line);
        if (line[0] == '\0')
            continue;
        if (sscanf(line, "%49[^|]|%49[^|]|%d|%d", a, b, &d, &c) != 4) {
            printf("  [!] Skipped a damaged line in %s\n", ROUTES_FILE);
            continue;
        }
        trimString(a);
        trimString(b);
        u = findDestination(a);
        v = findDestination(b);
        if (u < 0 || v < 0 || addRoute(u, v, d, c) != 0)
            printf("  [!] Skipped invalid route '%.30s' - '%.30s' in %s\n", a, b, ROUTES_FILE);
    }
    fclose(f);
}


/* ---------------------------------------------------------------------- */
/*  Route finding (DFS with backtracking, Dijkstra, BFS)                   */
/* ---------------------------------------------------------------------- */

static void computeTotals(Path *p)
{
    int i;
    p->totalDistance = 0;
    p->totalCost = 0;
    for (i = 0; i + 1 < p->length; i++) {
        p->totalDistance += distMatrix[p->nodes[i]][p->nodes[i + 1]];
        p->totalCost     += costMatrix[p->nodes[i]][p->nodes[i + 1]];
    }
}

/* Build the path from the "came from" array. */
static int buildPathFromPrev(const int prev[], int src, int dst, Path *out)
{
    int rev[MAX_DEST], n = 0, cur = dst, i;

    while (cur != -1 && n < MAX_DEST) {
        rev[n++] = cur;
        if (cur == src)
            break;
        cur = prev[cur];
    }
    if (n == 0 || rev[n - 1] != src)
        return 0;
    out->length = n;
    for (i = 0; i < n; i++)
        out->nodes[i] = rev[n - 1 - i];
    computeTotals(out);
    return 1;
}

static int   g_target, g_count, g_max, g_truncated;
static Path *g_out;
static int   g_visited[MAX_DEST];
static int   g_stack[MAX_DEST];

static void dfsRecurse(int u, int depth, int dist, int cost)
{
    int v, i;

    g_stack[depth] = u;
    if (u == g_target) {
        if (g_count >= g_max) {
            g_truncated = 1;
            return;
        }
        g_out[g_count].length = depth + 1;
        for (i = 0; i <= depth; i++)
            g_out[g_count].nodes[i] = g_stack[i];
        g_out[g_count].totalDistance = dist;
        g_out[g_count].totalCost = cost;
        g_count++;
        return;
    }
    g_visited[u] = 1;
    for (v = 0; v < destCount; v++) {
        if (distMatrix[u][v] > 0 && !g_visited[v]) {
            dfsRecurse(v, depth + 1, dist + distMatrix[u][v], cost + costMatrix[u][v]);
            if (g_truncated)
                break;
        }
    }
    g_visited[u] = 0;   /* backtrack */
}

/* Finds every simple path from src to dst. Returns how many were stored. */
int findAllPaths(int src, int dst, Path *results, int maxResults, int *truncated)
{
    memset(g_visited, 0, sizeof g_visited);
    g_target = dst;
    g_count = 0;
    g_max = maxResults;
    g_truncated = 0;
    g_out = results;
    dfsRecurse(src, 0, 0, 0);
    if (truncated != NULL)
        *truncated = g_truncated;
    return g_count;
}

/* Dijkstra with an array. byCost = 0 uses distance, 1 uses cost. */
int bestPath(int src, int dst, int byCost, Path *out)
{
    int best[MAX_DEST], prev[MAX_DEST], done[MAX_DEST];
    int i, iter, u, v, w;

    for (i = 0; i < destCount; i++) {
        best[i] = INF_VALUE;
        prev[i] = -1;
        done[i] = 0;
    }
    best[src] = 0;

    for (iter = 0; iter < destCount; iter++) {
        u = -1;
        for (i = 0; i < destCount; i++)
            if (!done[i] && best[i] < INF_VALUE && (u == -1 || best[i] < best[u]))
                u = i;
        if (u == -1 || u == dst)
            break;
        done[u] = 1;
        for (v = 0; v < destCount; v++) {
            w = byCost ? costMatrix[u][v] : distMatrix[u][v];
            if (w > 0 && !done[v] && best[u] + w < best[v]) {
                best[v] = best[u] + w;
                prev[v] = u;
            }
        }
    }
    if (best[dst] >= INF_VALUE)
        return 0;
    return buildPathFromPrev(prev, src, dst, out);
}

/* BFS: fewest road segments. A plain array holds the waiting nodes. */
int fewestStopsPath(int src, int dst, Path *out)
{
    int waiting[MAX_DEST], head = 0, tail = 0;
    int visited[MAX_DEST], prev[MAX_DEST];
    int i, u, v;

    for (i = 0; i < destCount; i++) {
        visited[i] = 0;
        prev[i] = -1;
    }
    visited[src] = 1;
    waiting[tail++] = src;
    while (head < tail) {
        u = waiting[head++];
        if (u == dst)
            break;
        for (v = 0; v < destCount; v++)
            if (distMatrix[u][v] > 0 && !visited[v]) {
                visited[v] = 1;
                prev[v] = u;
                waiting[tail++] = v;
            }
    }
    if (!visited[dst])
        return 0;
    return buildPathFromPrev(prev, src, dst, out);
}

static int comparePaths(const void *x, const void *y)
{
    const Path *a = (const Path *)x, *b = (const Path *)y;
    if (a->totalDistance != b->totalDistance)
        return a->totalDistance - b->totalDistance;
    return a->totalCost - b->totalCost;
}

void sortPathsByDistance(Path *paths, int count)
{
    qsort(paths, (size_t)count, sizeof(Path), comparePaths);
}

int pathsEqual(const Path *a, const Path *b)
{
    int i;
    if (a->length != b->length)
        return 0;
    for (i = 0; i < a->length; i++)
        if (a->nodes[i] != b->nodes[i])
            return 0;
    return 1;
}

void printPathDetails(const Path *p)
{
    int i;

    printf("  ");
    for (i = 0; i < p->length; i++) {
        printf("%s", destNames[p->nodes[i]]);
        if (i + 1 < p->length)
            printf(" -> ");
    }
    printf("\n");
    printf("  Total Distance : %d KM\n", p->totalDistance);
    printf("  Estimated Cost : BDT %d\n", p->totalCost);
    printf("  Stops          : %d\n", p->length - 2);
}


/* ---------------------------------------------------------------------- */
/*  Accounts                                                               */
/* ---------------------------------------------------------------------- */

unsigned long hashPassword(const char *password)
{
    uint32_t h = 5381;
    const char *salt = "BRP#";
    const char *p;

    for (p = salt; *p; p++)
        h = ((h << 5) + h) + (unsigned char)*p;
    for (p = password; *p; p++)
        h = ((h << 5) + h) + (unsigned char)*p;
    return (unsigned long)h;
}

int saveUsers(void)
{
    FILE *f = fopen(USERS_FILE, "w");
    int i;

    if (f == NULL) {
        printf("  [!] Could not write to %s\n", USERS_FILE);
        return 0;
    }
    for (i = 0; i < userCount; i++)
        fprintf(f, "%s|%s|%lu|%s|%s\n", users[i].fullName, users[i].username,
                users[i].passwordHash, users[i].email, users[i].phone);
    if (fclose(f) != 0) {
        printf("  [!] Error while closing %s\n", USERS_FILE);
        return 0;
    }
    return 1;
}

void loadUsers(void)
{
    FILE *f = fopen(USERS_FILE, "r");
    char line[512];

    userCount = 0;
    if (f == NULL) {
        saveUsers();
        return;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        User u;
        trimString(line);
        if (line[0] == '\0')
            continue;
        if (userCount >= MAX_USERS) {
            printf("  [!] User limit (%d) reached while loading %s\n", MAX_USERS, USERS_FILE);
            break;
        }
        if (sscanf(line, "%63[^|]|%63[^|]|%lu|%63[^|]|%19[^\n\r]",
                   u.fullName, u.username, &u.passwordHash, u.email, u.phone) != 5) {
            printf("  [!] Skipped a damaged line in %s\n", USERS_FILE);
            continue;
        }
        users[userCount++] = u;
    }
    fclose(f);
}

static int saveAdmin(void)
{
    FILE *f = fopen(ADMIN_FILE, "w");

    if (f == NULL) {
        printf("  [!] Could not write to %s\n", ADMIN_FILE);
        return 0;
    }
    fprintf(f, "%s|%lu\n", adminId, adminHash);
    if (fclose(f) != 0) {
        printf("  [!] Error while closing %s\n", ADMIN_FILE);
        return 0;
    }
    return 1;
}

void loadAdminCredentials(void)
{
    FILE *f = fopen(ADMIN_FILE, "r");
    char line[256], id[FIELD_LEN];
    unsigned long h;

    if (f != NULL) {
        if (fgets(line, sizeof line, f) != NULL && sscanf(line, "%63[^|]|%lu", id, &h) == 2) {
            strcpy(adminId, id);
            adminHash = h;
            fclose(f);
            return;
        }
        fclose(f);
    }
    strcpy(adminId, DEFAULT_ADMIN_ID);
    adminHash = hashPassword(DEFAULT_ADMIN_PASS);
    saveAdmin();
}

int findUserIndex(const char *username)
{
    int i;
    for (i = 0; i < userCount; i++)
        if (equalsIgnoreCase(users[i].username, username))
            return i;
    return -1;
}

static int isValidUsername(const char *s)
{
    size_t i, len = strlen(s);

    if (len < 3 || len > 20)
        return 0;
    for (i = 0; i < len; i++)
        if (!isalnum((unsigned char)s[i]) && s[i] != '_')
            return 0;
    return 1;
}

static int isValidEmail(const char *e)
{
    const char *at = strchr(e, '@'), *dot, *p;

    if (at == NULL || at == e || strchr(at + 1, '@') != NULL)
        return 0;
    dot = strrchr(at + 1, '.');
    if (dot == NULL || dot == at + 1 || dot[1] == '\0')
        return 0;
    for (p = e; *p; p++)
        if (isspace((unsigned char)*p))
            return 0;
    return 1;
}

static int isValidPhone(const char *p)
{
    size_t i = 0, digits = 0;

    if (p[0] == '+')
        i = 1;
    for (; p[i]; i++) {
        if (!isdigit((unsigned char)p[i]))
            return 0;
        digits++;
    }
    return digits >= 7 && digits <= 15;
}

/* allowBlank = 1: an empty answer means "keep the current value". */
int askUsername(char *buf, size_t size, int allowBlank, int ignoreIndex)
{
    int idx;

    for (;;) {
        if (askText(allowBlank ? "  New username (Enter to keep): "
                               : "  Username (3-20 letters, digits or _): ",
                    buf, size, allowBlank) == INPUT_BACK)
            return INPUT_BACK;
        if (buf[0] == '\0')
            return INPUT_OK;
        if (!isValidUsername(buf)) {
            printf("  [!] Username must be 3-20 characters: letters, digits or underscore.\n");
            continue;
        }
        idx = findUserIndex(buf);
        if (idx != -1 && idx != ignoreIndex) {
            printf("  [!] The username '%s' is already taken.\n", buf);
            continue;
        }
        return INPUT_OK;
    }
}

int askEmail(char *buf, size_t size, int allowBlank)
{
    for (;;) {
        if (askText(allowBlank ? "  New email (Enter to keep): " : "  Email address: ",
                    buf, size, allowBlank) == INPUT_BACK)
            return INPUT_BACK;
        if (buf[0] == '\0' || isValidEmail(buf))
            return INPUT_OK;
        printf("  [!] That does not look like an email (example: name@example.com).\n");
    }
}

int askPhone(char *buf, size_t size, int allowBlank)
{
    for (;;) {
        if (askText(allowBlank ? "  New phone (Enter to keep): " : "  Phone number: ",
                    buf, size, allowBlank) == INPUT_BACK)
            return INPUT_BACK;
        if (buf[0] == '\0' || isValidPhone(buf))
            return INPUT_OK;
        printf("  [!] Phone number must have 7-15 digits (a leading + is allowed).\n");
    }
}

/* *changed = 1 when a new password was entered. */
int askNewPassword(unsigned long *hashOut, int allowBlank, int *changed)
{
    char pw[FIELD_LEN], confirm[FIELD_LEN];

    *changed = 0;
    for (;;) {
        if (askText(allowBlank ? "  New password (Enter to keep): " : "  Password (min 4 characters): ",
                    pw, sizeof pw, allowBlank) == INPUT_BACK)
            return INPUT_BACK;
        if (pw[0] == '\0')
            return INPUT_OK;
        if (strlen(pw) < 4) {
            printf("  [!] Password must be at least 4 characters.\n");
            continue;
        }
        if (askText("  Confirm password: ", confirm, sizeof confirm, 0) == INPUT_BACK)
            continue;   /* back to the password prompt */
        if (strcmp(pw, confirm) != 0) {
            printf("  [!] The passwords do not match.\n");
            continue;
        }
        *hashOut = hashPassword(pw);
        *changed = 1;
        return INPUT_OK;
    }
}

void registerUser(void)
{
    User u;
    int step = 0, r = INPUT_OK, changed;

    memset(&u, 0, sizeof u);
    printTitle("USER REGISTRATION");
    if (userCount >= MAX_USERS) {
        printf("  [!] Registration is closed (user limit of %d reached).\n", MAX_USERS);
        return;
    }
    printBackHint();

    while (step < 5) {
        switch (step) {
        case 0:  r = askText("  Full name: ", u.fullName, sizeof u.fullName, 0); break;
        case 1:  r = askUsername(u.username, sizeof u.username, 0, -1);          break;
        case 2:  r = askNewPassword(&u.passwordHash, 0, &changed);               break;
        case 3:  r = askEmail(u.email, sizeof u.email, 0);                       break;
        default: r = askPhone(u.phone, PHONE_LEN, 0);                            break;
        }
        if (r == INPUT_BACK) {
            if (step == 0) {
                printf("\n  Registration cancelled.\n");
                return;
            }
            step--;
        } else
            step++;
    }

    users[userCount++] = u;
    if (saveUsers())
        printf("\n  Registration successful. You can now log in as '%s'.\n", u.username);
    else {
        userCount--;
        printf("\n  Registration failed because the data file could not be written.\n");
    }
}

/* Returns the user index, or -1 if login failed or the user went back. */
int userLogin(void)
{
    char username[FIELD_LEN], password[FIELD_LEN];
    int step = 0, r, idx;

    printTitle("USER LOGIN");
    printBackHint();
    while (step < 2) {
        if (step == 0)
            r = askText("  Username : ", username, sizeof username, 0);
        else
            r = askText("  Password : ", password, sizeof password, 0);
        if (r == INPUT_BACK) {
            if (step == 0)
                return -1;
            step--;
        } else
            step++;
    }
    idx = findUserIndex(username);
    if (idx == -1 || users[idx].passwordHash != hashPassword(password)) {
        printf("\n  [!] Incorrect username or password.\n");
        return -1;
    }
    printf("\n  Login successful. Welcome, %s!\n", users[idx].fullName);
    return idx;
}

int adminLogin(void)
{
    char id[FIELD_LEN], password[FIELD_LEN];
    int step = 0, r;

    printTitle("ADMIN LOGIN");
    printBackHint();
    while (step < 2) {
        if (step == 0)
            r = askText("  Admin ID : ", id, sizeof id, 0);
        else
            r = askText("  Password : ", password, sizeof password, 0);
        if (r == INPUT_BACK) {
            if (step == 0)
                return 0;
            step--;
        } else
            step++;
    }
    if (strcmp(id, adminId) == 0 && hashPassword(password) == adminHash) {
        printf("\n  Login successful.\n");
        return 1;
    }
    printf("\n  [!] Incorrect Admin ID or password.\n");
    return 0;
}

int changeAdminCredentials(void)
{
    char current[FIELD_LEN], newId[FIELD_LEN];
    unsigned long newHash = 0;
    int step = 0, r = INPUT_OK, changed;

    printTitle("CHANGE ADMIN CREDENTIALS");
    printBackHint();
    while (step < 3) {
        switch (step) {
        case 0:
            r = askText("  Current password: ", current, sizeof current, 0);
            if (r == INPUT_OK && hashPassword(current) != adminHash) {
                printf("  [!] Current password is incorrect.\n");
                continue;
            }
            break;
        case 1:  r = askText("  New Admin ID: ", newId, sizeof newId, 0); break;
        default: r = askNewPassword(&newHash, 0, &changed);               break;
        }
        if (r == INPUT_BACK) {
            if (step == 0)
                return 0;
            step--;
        } else
            step++;
    }
    strcpy(adminId, newId);
    adminHash = newHash;
    if (saveAdmin())
        printf("\n  Admin credentials updated.\n");
    else
        printf("\n  Credentials changed for this session, but saving failed.\n");
    return 1;
}


/* ---------------------------------------------------------------------- */
/*  Admin panel                                                            */
/* ---------------------------------------------------------------------- */

static void persistGraph(void)
{
    if (!saveGraphData())
        printf("  [!] The change is active but could not be saved to file.\n");
    else
        printf("  Saved.\n");
}

static void persistUsers(void)
{
    if (!saveUsers())
        printf("  [!] The change is active but could not be saved to file.\n");
    else
        printf("  Saved.\n");
}

static void printUserHeader(void)
{
    printf("\n  %-5s %-22s %-14s %-26s %-15s\n", "No.", "Full Name", "Username", "Email", "Phone");
    printf("  ");
    printLine('-', 85);
}

static void printUserRow(int i)
{
    printf("  %-5d %-22.22s %-14.14s %-26.26s %-15.15s\n", i + 1, users[i].fullName,
           users[i].username, users[i].email, users[i].phone);
}

static int viewAllUsers(void)
{
    int i;

    printTitle("REGISTERED USERS");
    if (userCount == 0) {
        printf("  No users have registered yet.\n");
        return 1;
    }
    printUserHeader();
    for (i = 0; i < userCount; i++)
        printUserRow(i);
    printf("\n  Total users: %d\n", userCount);
    return 1;
}

static int searchUser(void)
{
    char key[FIELD_LEN];
    int i, found = 0;

    printTitle("SEARCH USER");
    printBackHint();
    if (askText("  Username (or part of it): ", key, sizeof key, 0) == INPUT_BACK)
        return 0;
    for (i = 0; i < userCount; i++)
        if (containsIgnoreCase(users[i].username, key)) {
            if (!found)
                printUserHeader();
            printUserRow(i);
            found++;
        }
    if (!found)
        printf("\n  No user found matching '%s'.\n", key);
    else
        printf("\n  %d user(s) found.\n", found);
    return 1;
}

static int viewUserDetails(void)
{
    char username[FIELD_LEN];
    int idx;

    printTitle("USER DETAILS");
    printBackHint();
    for (;;) {
        if (askText("  Username: ", username, sizeof username, 0) == INPUT_BACK)
            return 0;
        idx = findUserIndex(username);
        if (idx != -1)
            break;
        printf("  [!] User '%s' does not exist.\n", username);
    }
    printf("\n  Full Name : %s\n  Username  : %s\n  Email     : %s\n  Phone     : %s\n",
           users[idx].fullName, users[idx].username, users[idx].email, users[idx].phone);
    return 1;
}

static int updateUser(void)
{
    char username[FIELD_LEN], newName[FIELD_LEN], newUser[FIELD_LEN];
    char newEmail[FIELD_LEN], newPhone[PHONE_LEN];
    unsigned long newHash = 0;
    int idx = -1, step = 0, r = INPUT_OK, changed = 0;

    printTitle("UPDATE USER");
    printBackHint();
    while (step < 6) {
        switch (step) {
        case 0:
            r = askText("  Username of the user to update: ", username, sizeof username, 0);
            if (r == INPUT_OK) {
                idx = findUserIndex(username);
                if (idx == -1) {
                    printf("  [!] User '%s' does not exist.\n", username);
                    continue;
                }
                printf("\n  Current details: %s | %s | %s | %s\n", users[idx].fullName,
                       users[idx].username, users[idx].email, users[idx].phone);
                printf("  Press Enter on any field to keep it as it is.\n\n");
            }
            break;
        case 1:  r = askText("  New full name (Enter to keep): ", newName, sizeof newName, 1); break;
        case 2:  r = askUsername(newUser, sizeof newUser, 1, idx);                              break;
        case 3:  r = askEmail(newEmail, sizeof newEmail, 1);                                    break;
        case 4:  r = askPhone(newPhone, PHONE_LEN, 1);                                          break;
        default: r = askNewPassword(&newHash, 1, &changed);                                     break;
        }
        if (r == INPUT_BACK) {
            if (step == 0)
                return 0;
            step--;
        } else
            step++;
    }

    if (newName[0])  strcpy(users[idx].fullName, newName);
    if (newUser[0])  strcpy(users[idx].username, newUser);
    if (newEmail[0]) strcpy(users[idx].email, newEmail);
    if (newPhone[0]) strcpy(users[idx].phone, newPhone);
    if (changed)     users[idx].passwordHash = newHash;
    printf("\n  User updated.\n");
    persistUsers();
    return 1;
}

static int deleteUser(void)
{
    char username[FIELD_LEN];
    int idx = -1, step = 0, r = INPUT_OK, answer = 0, i;

    printTitle("DELETE USER");
    printBackHint();
    while (step < 2) {
        if (step == 0) {
            r = askText("  Username to delete: ", username, sizeof username, 0);
            if (r == INPUT_OK) {
                idx = findUserIndex(username);
                if (idx == -1) {
                    printf("  [!] User '%s' does not exist.\n", username);
                    continue;
                }
            }
        } else
            r = askYesNo("  Delete this account permanently? (y/n): ", &answer);

        if (r == INPUT_BACK) {
            if (step == 0)
                return 0;
            step--;
        } else
            step++;
    }
    if (!answer) {
        printf("\n  Nothing was deleted.\n");
        return 1;
    }
    for (i = idx; i < userCount - 1; i++)
        users[i] = users[i + 1];
    userCount--;
    printf("\n  User deleted.\n");
    persistUsers();
    return 1;
}

static int adminViewDestinations(void)
{
    printTitle("ALL DESTINATIONS");
    printDestinationList();
    printf("\n  Total: %d of %d\n", destCount, MAX_DEST);
    return 1;
}

static int addNewDestination(void)
{
    char name[NAME_LEN];
    int r;

    printTitle("ADD DESTINATION");
    if (destCount >= MAX_DEST) {
        printf("  [!] The destination list is full (%d).\n", MAX_DEST);
        return 1;
    }
    printBackHint();
    for (;;) {
        if (askText("  Destination name: ", name, sizeof name, 0) == INPUT_BACK)
            return 0;
        r = addDestination(name);
        if (r == 1) {
            printf("  [!] '%s' already exists.\n", name);
            continue;
        }
        break;
    }
    printf("\n  '%s' added.\n", name);
    persistGraph();
    return 1;
}

static int updateDestination(void)
{
    char name[NAME_LEN];
    int idx = 0, step = 0, r;

    printTitle("UPDATE DESTINATION");
    if (destCount == 0) {
        printf("  [!] There are no destinations yet.\n");
        return 1;
    }
    printBackHint();
    printDestinationList();
    while (step < 2) {
        if (step == 0)
            r = askDestination("\n  Destination to rename (number or name): ", &idx);
        else {
            r = askText("  New name: ", name, sizeof name, 0);
            if (r == INPUT_OK && renameDestination(idx, name) != 0) {
                printf("  [!] Another destination is already called '%s'.\n", name);
                continue;
            }
        }
        if (r == INPUT_BACK) {
            if (step == 0)
                return 0;
            step--;
        } else
            step++;
    }
    printf("\n  Destination updated.\n");
    persistGraph();
    return 1;
}

static int deleteDestinationMenu(void)
{
    int idx = 0, step = 0, r, answer = 0;

    printTitle("DELETE DESTINATION");
    if (destCount == 0) {
        printf("  [!] There are no destinations yet.\n");
        return 1;
    }
    printBackHint();
    printDestinationList();
    while (step < 2) {
        if (step == 0)
            r = askDestination("\n  Destination to delete (number or name): ", &idx);
        else {
            printf("  '%s' has %d connected route(s). They will be deleted too.\n",
                   destNames[idx], countNeighbours(idx));
            r = askYesNo("  Continue? (y/n): ", &answer);
        }
        if (r == INPUT_BACK) {
            if (step == 0)
                return 0;
            step--;
        } else
            step++;
    }
    if (!answer) {
        printf("\n  Nothing was deleted.\n");
        return 1;
    }
    deleteDestination(idx);
    printf("\n  Destination deleted.\n");
    persistGraph();
    return 1;
}

static int viewAllRoutes(void)
{
    printTitle("ALL ROUTES");
    printAllRoutes();
    return 1;
}

/* mode 0 = add, 1 = update, 2 = delete */
static int routeForm(int mode)
{
    static const char *titles[3] = { "ADD ROUTE", "UPDATE ROUTE", "DELETE ROUTE" };
    int a = 0, b = 0, d = 0, c = 0, answer = 1;
    int step = 0, r = INPUT_OK, lastStep = (mode == 2) ? 3 : 4;

    printTitle(titles[mode]);
    if (destCount < 2) {
        printf("  [!] At least two destinations are needed.\n");
        return 1;
    }
    printBackHint();
    printDestinationList();

    while (step < lastStep) {
        switch (step) {
        case 0:
            r = askDestination("\n  First destination  (number or name): ", &a);
            break;
        case 1:
            r = askDestination("  Second destination (number or name): ", &b);
            if (r == INPUT_OK) {
                if (a == b) {
                    printf("  [!] A route needs two different destinations.\n");
                    continue;
                }
                if (mode == 0 && routeExists(a, b)) {
                    printf("  [!] %s - %s already exists. Use Update Route to change it.\n",
                           destNames[a], destNames[b]);
                    continue;
                }
                if (mode != 0 && !routeExists(a, b)) {
                    printf("  [!] There is no route between %s and %s.\n", destNames[a], destNames[b]);
                    continue;
                }
                if (mode == 1)
                    printf("  Current: %d KM, BDT %d\n", distMatrix[a][b], costMatrix[a][b]);
            }
            break;
        case 2:
            if (mode == 2)
                r = askYesNo("  Delete this route? (y/n): ", &answer);
            else
                r = askInt("  Distance in KM (1-5000): ", &d, 1, MAX_DISTANCE);
            break;
        default:
            r = askInt("  Estimated cost in BDT (1-1000000): ", &c, 1, MAX_COST);
            break;
        }
        if (r == INPUT_BACK) {
            if (step == 0)
                return 0;
            step--;
        } else
            step++;
    }

    if (mode == 0) {
        if (addRoute(a, b, d, c) != 0) {
            printf("\n  [!] The route could not be added.\n");
            return 1;
        }
        printf("\n  Route %s - %s added.\n", destNames[a], destNames[b]);
    } else if (mode == 1) {
        if (updateRoute(a, b, d, c) != 0) {
            printf("\n  [!] The route could not be updated.\n");
            return 1;
        }
        printf("\n  Route %s - %s updated.\n", destNames[a], destNames[b]);
    } else {
        if (!answer) {
            printf("\n  Nothing was deleted.\n");
            return 1;
        }
        deleteRoute(a, b);
        printf("\n  Route %s - %s deleted.\n", destNames[a], destNames[b]);
    }
    persistGraph();
    return 1;
}

static int addNewRoute(void)        { return routeForm(0); }
static int updateExistingRoute(void) { return routeForm(1); }
static int deleteExistingRoute(void) { return routeForm(2); }

static int viewRouteStatistics(void)
{
    int i, j, edges = 0, totalD = 0, totalC = 0;
    int minD = INT_MAX, maxD = 0, minC = INT_MAX, maxC = 0;
    int minDa = 0, minDb = 0, maxDa = 0, maxDb = 0, minCa = 0, minCb = 0, maxCa = 0, maxCb = 0;
    int best = -1, bestDeg = -1, deg, isolated = 0;

    printTitle("ROUTE STATISTICS");
    for (i = 0; i < destCount; i++)
        for (j = i + 1; j < destCount; j++)
            if (distMatrix[i][j] > 0) {
                int d = distMatrix[i][j], c = costMatrix[i][j];
                edges++;
                totalD += d;
                totalC += c;
                if (d < minD) { minD = d; minDa = i; minDb = j; }
                if (d > maxD) { maxD = d; maxDa = i; maxDb = j; }
                if (c < minC) { minC = c; minCa = i; minCb = j; }
                if (c > maxC) { maxC = c; maxCa = i; maxCb = j; }
            }

    printf("  Destinations         : %d\n", destCount);
    printf("  Direct routes        : %d\n", edges);
    if (destCount > 1)
        printf("  Network coverage     : %.1f%% of possible connections\n",
               200.0 * edges / ((double)destCount * (destCount - 1)));
    if (edges == 0) {
        printf("  There are no routes yet.\n");
        return 1;
    }
    printf("  Average distance     : %.1f KM\n", (double)totalD / edges);
    printf("  Average cost         : BDT %.1f\n", (double)totalC / edges);
    printf("  Shortest direct road : %d KM (%s - %s)\n", minD, destNames[minDa], destNames[minDb]);
    printf("  Longest direct road  : %d KM (%s - %s)\n", maxD, destNames[maxDa], destNames[maxDb]);
    printf("  Cheapest direct road : BDT %d (%s - %s)\n", minC, destNames[minCa], destNames[minCb]);
    printf("  Costliest direct road: BDT %d (%s - %s)\n", maxC, destNames[maxCa], destNames[maxCb]);

    for (i = 0; i < destCount; i++) {
        deg = countNeighbours(i);
        if (deg > bestDeg) { bestDeg = deg; best = i; }
        if (deg == 0) isolated++;
    }
    printf("  Busiest destination  : %s (%d direct routes)\n", destNames[best], bestDeg);
    if (isolated > 0) {
        printf("  Not connected to any route:");
        for (i = 0; i < destCount; i++)
            if (countNeighbours(i) == 0)
                printf(" %s;", destNames[i]);
        printf("\n");
    }
    return 1;
}

static void printAdminMenu(void)
{
    printTitle("ADMIN PANEL");
    printf("   1. View All Registered Users\n");
    printf("   2. Search User by Username\n");
    printf("   3. View Individual User Details\n");
    printf("   4. Update User Information\n");
    printf("   5. Delete User Account\n");
    printf("   6. View All Destinations\n");
    printf("   7. Add New Destination\n");
    printf("   8. Update Destination\n");
    printf("   9. Delete Destination\n");
    printf("  10. View All Available Routes\n");
    printf("  11. Add New Route\n");
    printf("  12. Update Existing Route\n");
    printf("  13. Delete Route\n");
    printf("  14. View Route Statistics\n");
    printf("  15. Logout\n");
    printf("  16. Change Admin Credentials\n");
    printLine('=', 60);
}

void adminPanel(void)
{
    int choice, done;

    for (;;) {
        printAdminMenu();
        choice = readChoice("  Enter your choice: ", 16);
        done = 1;
        switch (choice) {
        case 1:  done = viewAllUsers();           break;
        case 2:  done = searchUser();             break;
        case 3:  done = viewUserDetails();        break;
        case 4:  done = updateUser();             break;
        case 5:  done = deleteUser();             break;
        case 6:  done = adminViewDestinations();  break;
        case 7:  done = addNewDestination();      break;
        case 8:  done = updateDestination();      break;
        case 9:  done = deleteDestinationMenu();  break;
        case 10: done = viewAllRoutes();          break;
        case 11: done = addNewRoute();            break;
        case 12: done = updateExistingRoute();    break;
        case 13: done = deleteExistingRoute();    break;
        case 14: done = viewRouteStatistics();    break;
        case 16: done = changeAdminCredentials(); break;
        case 15:
            printf("\n  You have been logged out.\n");
            return;
        }
        if (done)
            pauseForEnter();
    }
}


/* ---------------------------------------------------------------------- */
/*  User panel                                                             */
/* ---------------------------------------------------------------------- */

static Path pathBuffer[MAX_STORED_PATHS];

/* Asks for source and destination. Returns 0 if the user goes back. */
static int askRoutePair(int *src, int *dst)
{
    int step = 0, r;

    if (destCount < 2) {
        printf("  [!] Not enough destinations are available. Please contact the admin.\n");
        return 0;
    }
    printBackHint();
    printDestinationList();
    printf("\n  Type a destination number or name.\n\n");
    while (step < 2) {
        if (step == 0)
            r = askDestination("  From : ", src);
        else {
            r = askDestination("  To   : ", dst);
            if (r == INPUT_OK && *src == *dst) {
                printf("  [!] Start and end are the same. Please choose a different destination.\n");
                continue;
            }
        }
        if (r == INPUT_BACK) {
            if (step == 0)
                return 0;
            step--;
        } else
            step++;
    }
    return 1;
}

static void printHeader(int src, int dst)
{
    printf("\n");
    printLine('=', 60);
    printf("  BANGLADESH ROUTE PLANNER\n");
    printf("  From : %s\n  To   : %s\n", destNames[src], destNames[dst]);
    printLine('=', 60);
}

static void printNoRoute(int src, int dst)
{
    printf("\n  [!] Sorry, no route is available from %s to %s.\n", destNames[src], destNames[dst]);
}

static int showAvailableRoutes(int src, int dst)
{
    int truncated = 0, n, i;

    n = findAllPaths(src, dst, pathBuffer, MAX_STORED_PATHS, &truncated);
    if (n == 0) {
        printNoRoute(src, dst);
        return 0;
    }
    sortPathsByDistance(pathBuffer, n);
    printf("\n  AVAILABLE ROUTES (%d found)\n", n);
    for (i = 0; i < n; i++) {
        printf("\n  Route %d:\n", i + 1);
        printPathDetails(&pathBuffer[i]);
    }
    if (truncated)
        printf("\n  Only the first %d routes are shown.\n", MAX_STORED_PATHS);
    return n;
}

/* mode 0 = shortest, 1 = cheapest, 2 = fewest stops */
static void showBest(int src, int dst, int mode)
{
    static const char *titles[3] = {
        "SHORTEST DISTANCE ROUTE", "CHEAPEST ROUTE", "FEWEST STOPS ROUTE"
    };
    Path p;
    int ok = (mode == 2) ? fewestStopsPath(src, dst, &p) : bestPath(src, dst, mode, &p);

    printf("\n  %s\n", titles[mode]);
    if (!ok) {
        printNoRoute(src, dst);
        return;
    }
    printPathDetails(&p);
}

static int userViewDestinations(void)
{
    printTitle("DESTINATIONS");
    printDestinationList();
    return 1;
}

static int viewRoutes(void)
{
    printTitle("AVAILABLE ROUTES");
    printAllRoutes();
    return 1;
}

static int searchRoute(void)
{
    int src, dst;

    printTitle("SEARCH ROUTE");
    if (!askRoutePair(&src, &dst))
        return 0;
    printHeader(src, dst);
    if (routeExists(src, dst))
        printf("\n  Direct road: %d KM, BDT %d\n", distMatrix[src][dst], costMatrix[src][dst]);
    else
        printf("\n  There is no direct road between these two destinations.\n");

    if (showAvailableRoutes(src, dst) == 0)
        return 1;
    printf("\n  RECOMMENDED OPTIONS\n");
    showBest(src, dst, 0);
    showBest(src, dst, 1);
    showBest(src, dst, 2);
    return 1;
}

static int findAllRoutes(void)
{
    int src, dst;

    printTitle("ALL POSSIBLE ROUTES");
    if (!askRoutePair(&src, &dst))
        return 0;
    printHeader(src, dst);
    showAvailableRoutes(src, dst);
    return 1;
}

static int findBestRoute(int mode)
{
    static const char *titles[3] = {
        "SHORTEST DISTANCE ROUTE", "CHEAPEST ROUTE", "FEWEST STOPS ROUTE"
    };
    int src, dst;

    printTitle(titles[mode]);
    if (!askRoutePair(&src, &dst))
        return 0;
    printHeader(src, dst);
    showBest(src, dst, mode);
    return 1;
}

static int findShortest(void)   { return findBestRoute(0); }
static int findCheapest(void)   { return findBestRoute(1); }
static int findFewestStops(void) { return findBestRoute(2); }

static int compareRoutes(void)
{
    int src, dst, truncated = 0, n;
    Path shortest, cheapest, fewest;

    printTitle("COMPARE ROUTES");
    if (!askRoutePair(&src, &dst))
        return 0;
    printHeader(src, dst);

    n = findAllPaths(src, dst, pathBuffer, MAX_STORED_PATHS, &truncated);
    if (n == 0) {
        printNoRoute(src, dst);
        return 1;
    }
    bestPath(src, dst, 0, &shortest);
    bestPath(src, dst, 1, &cheapest);
    fewestStopsPath(src, dst, &fewest);

    printf("\n  %d route(s) available%s.\n", n, truncated ? " (at least)" : "");
    printf("\n  %-20s %10s %12s %7s\n", "Option", "Distance", "Cost", "Stops");
    printf("  ");
    printLine('-', 53);
    printf("  %-20s %7d KM %8d BDT %7d\n", "Shortest distance", shortest.totalDistance,
           shortest.totalCost, shortest.length - 2);
    printf("  %-20s %7d KM %8d BDT %7d\n", "Cheapest", cheapest.totalDistance,
           cheapest.totalCost, cheapest.length - 2);
    printf("  %-20s %7d KM %8d BDT %7d\n", "Fewest stops", fewest.totalDistance,
           fewest.totalCost, fewest.length - 2);

    printf("\n  Shortest distance route:\n");
    printPathDetails(&shortest);
    printf("\n  Cheapest route:\n");
    printPathDetails(&cheapest);
    printf("\n  Fewest stops route:\n");
    printPathDetails(&fewest);

    printf("\n  THINGS TO NOTE\n");
    if (pathsEqual(&shortest, &cheapest) ||
        (shortest.totalCost == cheapest.totalCost && shortest.totalDistance == cheapest.totalDistance))
        printf("  - The shortest route is also the cheapest one for this trip.\n");
    else
        printf("  - The cheapest route costs BDT %d less than the shortest route, but it is %d KM longer.\n",
               shortest.totalCost - cheapest.totalCost,
               cheapest.totalDistance - shortest.totalDistance);
    if (fewest.length < cheapest.length)
        printf("  - The cheapest route needs %d more stop(s) than the route with the fewest stops.\n",
               cheapest.length - fewest.length);
    if (fewest.totalDistance > shortest.totalDistance)
        printf("  - The route with the fewest stops is %d KM longer than the shortest route.\n",
               fewest.totalDistance - shortest.totalDistance);
    if (fewest.totalCost > cheapest.totalCost)
        printf("  - The route with the fewest stops costs BDT %d more than the cheapest route.\n",
               fewest.totalCost - cheapest.totalCost);
    return 1;
}

static int viewProfile(int idx)
{
    printTitle("MY PROFILE");
    printf("  Full Name : %s\n  Username  : %s\n  Email     : %s\n  Phone     : %s\n",
           users[idx].fullName, users[idx].username, users[idx].email, users[idx].phone);
    return 1;
}

static int updateProfile(int idx)
{
    char newName[FIELD_LEN], newEmail[FIELD_LEN], newPhone[PHONE_LEN], cur[FIELD_LEN];
    unsigned long newHash = 0;
    int step = 0, r = INPUT_OK, wantPassword = 0, changed = 0;

    printTitle("UPDATE MY PROFILE");
    printBackHint();
    printf("  Press Enter on a field to keep it as it is.\n\n");
    newName[0] = newEmail[0] = newPhone[0] = '\0';

    while (step < 6) {
        switch (step) {
        case 0: r = askText("  New full name (Enter to keep): ", newName, sizeof newName, 1); break;
        case 1: r = askEmail(newEmail, sizeof newEmail, 1);                                    break;
        case 2: r = askPhone(newPhone, PHONE_LEN, 1);                                          break;
        case 3:
            r = askYesNo("  Change password? (y/n): ", &wantPassword);
            if (r == INPUT_OK && !wantPassword) {
                step = 6;
                continue;
            }
            break;
        case 4:
            r = askText("  Current password: ", cur, sizeof cur, 0);
            if (r == INPUT_OK && hashPassword(cur) != users[idx].passwordHash) {
                printf("  [!] That is not your current password.\n");
                continue;
            }
            break;
        default:
            r = askNewPassword(&newHash, 0, &changed);
            break;
        }
        if (r == INPUT_BACK) {
            if (step == 0)
                return 0;
            step--;
        } else
            step++;
    }

    if (newName[0])  strcpy(users[idx].fullName, newName);
    if (newEmail[0]) strcpy(users[idx].email, newEmail);
    if (newPhone[0]) strcpy(users[idx].phone, newPhone);
    if (changed)     users[idx].passwordHash = newHash;

    if (saveUsers())
        printf("\n  Profile updated.\n");
    else
        printf("\n  [!] Profile changed, but it could not be saved to file.\n");
    return 1;
}

static void printUserMenu(const char *name)
{
    printTitle("USER PANEL");
    printf("  Logged in as: %s\n\n", name);
    printf("   1. View All Available Destinations\n");
    printf("   2. View All Available Routes\n");
    printf("   3. Search Route Between Two Destinations\n");
    printf("   4. Find All Possible Routes\n");
    printf("   5. Find Shortest Distance Route\n");
    printf("   6. Find Cheapest Route\n");
    printf("   7. Find Route with Minimum Stops\n");
    printf("   8. Compare Available Routes\n");
    printf("   9. View My Profile\n");
    printf("  10. Update My Profile\n");
    printf("  11. Logout\n");
    printLine('=', 60);
}

void userPanel(int userIndex)
{
    int choice, done;

    for (;;) {
        printUserMenu(users[userIndex].fullName);
        choice = readChoice("  Enter your choice: ", 11);
        done = 1;
        switch (choice) {
        case 1:  done = userViewDestinations();    break;
        case 2:  done = viewRoutes();              break;
        case 3:  done = searchRoute();             break;
        case 4:  done = findAllRoutes();           break;
        case 5:  done = findShortest();            break;
        case 6:  done = findCheapest();            break;
        case 7:  done = findFewestStops();         break;
        case 8:  done = compareRoutes();           break;
        case 9:  done = viewProfile(userIndex);    break;
        case 10: done = updateProfile(userIndex);  break;
        case 11:
            printf("\n  You have been logged out. Goodbye, %s!\n", users[userIndex].fullName);
            return;
        }
        if (done)
            pauseForEnter();
    }
}


/* ---------------------------------------------------------------------- */
/*  Main                                                                   */
/* ---------------------------------------------------------------------- */

static void printMainMenu(void)
{
    printf("\n");
    printLine('=', 50);
    printf("        BANGLADESH ROUTE PLANNER\n");
    printLine('=', 50);
    printf("  1. Admin Login\n");
    printf("  2. User Registration\n");
    printf("  3. User Login\n");
    printf("  4. Exit\n");
    printLine('=', 50);
}

int main(void)
{
    int choice, idx;

    loadAdminCredentials();
    loadUsers();
    loadGraphData();

    for (;;) {
        printMainMenu();
        choice = readChoice("Enter your choice: ", 4);
        switch (choice) {
        case 1:
            if (adminLogin())
                adminPanel();
            break;
        case 2:
            registerUser();
            break;
        case 3:
            idx = userLogin();
            if (idx >= 0)
                userPanel(idx);
            break;
        case 4:
            printf("\nThank you from 'ERSiSa_Routeminds' for using Bangladesh Route Planner. Goodbye!\n");
            return 0;
        }
    }
}
