#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct BookNode {
    int bookID;
    char title[100];
    char author[100];
    char category[50];
    int totalCopies;
    int availableCopies;
    struct BookNode *next;
} BookNode;

// ---------- Core Linked List Functions (same as before) ----------

BookNode* createBookNode(
    int bookID,
    const char *title,
    const char *author,
    const char *category,
    int totalCopies
) {
    BookNode *newNode = (BookNode*)malloc(sizeof(BookNode));
    if (!newNode) {
        fprintf(stderr, "Memory allocation failed!\n");
        return NULL;
    }

    newNode->bookID = bookID;
    strncpy(newNode->title, title, sizeof(newNode->title) - 1);
    newNode->title[sizeof(newNode->title) - 1] = '\0';

    strncpy(newNode->author, author, sizeof(newNode->author) - 1);
    newNode->author[sizeof(newNode->author) - 1] = '\0';

    strncpy(newNode->category, category, sizeof(newNode->category) - 1);
    newNode->category[sizeof(newNode->category) - 1] = '\0';

    newNode->totalCopies = totalCopies;
    newNode->availableCopies = totalCopies;
    newNode->next = NULL;

    return newNode;
}

void addBookSorted(
    BookNode **headRef,
    int bookID,
    const char *title,
    const char *author,
    const char *category,
    int totalCopies
) {
    BookNode *newNode = createBookNode(bookID, title, author, category, totalCopies);
    if (!newNode) return;

    if (*headRef == NULL || bookID < (*headRef)->bookID) {
        newNode->next = *headRef;
        *headRef = newNode;
        return;
    }

    BookNode *current = *headRef;
    while (current->next != NULL && current->next->bookID < bookID) {
        current = current->next;
    }

    if (current->next != NULL && current->next->bookID == bookID) {
        // duplicate ID, do not add
        free(newNode);
        return;
    }

    newNode->next = current->next;
    current->next = newNode;
}

BookNode* searchBook(BookNode *head, int bookID) {
    BookNode *current = head;
    while (current != NULL) {
        if (current->bookID == bookID) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

int deleteBook(BookNode **headRef, int bookID) {
    if (*headRef == NULL) return 0;

    BookNode *current = *headRef;
    BookNode *prev = NULL;

    if (current->bookID == bookID) {
        *headRef = current->next;
        free(current);
        return 1;
    }

    while (current != NULL && current->bookID != bookID) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) return 0;

    prev->next = current->next;
    free(current);
    return 1;
}

int issueBook(BookNode *head, int bookID) {
    BookNode *book = searchBook(head, bookID);
    if (!book) return 0;
    if (book->availableCopies > 0) {
        book->availableCopies -= 1;
        return 1;
    }
    return -1;
}

int returnBook(BookNode *head, int bookID) {
    BookNode *book = searchBook(head, bookID);
    if (!book) return 0;
    if (book->availableCopies < book->totalCopies) {
        book->availableCopies += 1;
        return 1;
    }
    return -1;
}

void freeAllBooks(BookNode **headRef) {
    BookNode *current = *headRef;
    while (current != NULL) {
        BookNode *tmp = current;
        current = current->next;
        free(tmp);
    }
    *headRef = NULL;
}

// ---------- File persistence: load/save books.txt ----------

BookNode* loadBooksFromFile(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        // file may not exist yet, that's ok
        return NULL;
    }

    BookNode *head = NULL;
    char line[512];

    while (fgets(line, sizeof(line), fp) != NULL) {
        // each line: id,title,author,category,available,total
        char *p = strchr(line, '\n');
        if (p) *p = '\0';

        if (line[0] == '\0') continue;

        char *token;
        int id, available, total;
        char title[100], author[100], category[50];

        token = strtok(line, ",");
        if (!token) continue;
        id = atoi(token);

        token = strtok(NULL, ",");
        if (!token) continue;
        strncpy(title, token, sizeof(title) - 1);
        title[sizeof(title) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        strncpy(author, token, sizeof(author) - 1);
        author[sizeof(author) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        strncpy(category, token, sizeof(category) - 1);
        category[sizeof(category) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        available = atoi(token);

        token = strtok(NULL, ",");
        if (!token) continue;
        total = atoi(token);

        BookNode *node = createBookNode(id, title, author, category, total);
        if (!node) continue;
        node->availableCopies = available; // override
        addBookSorted(&head, node->bookID, node->title, node->author, node->category, node->totalCopies);
        // but addBookSorted creates new node; to keep simple, we can insert manually instead.
        // simpler: directly insert node at correct place:

        // We'll do a simpler insert to keep availability:
        // (overwrite previous addBookSorted effect)
    }

    fclose(fp);

    // The above logic using createBookNode + addBookSorted could be simplified,
    // but for now assume file is already sorted = we can just append.
    // To avoid confusion, let's actually re-write this function properly:
    // (See note below)
    return head;
}

// Actually better: let's rewrite loadBooksFromFile cleanly:

BookNode* loadBooksFromFile_fixed(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return NULL;

    BookNode *head = NULL;
    BookNode *tail = NULL;
    char line[512];

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = strchr(line, '\n');
        if (p) *p = '\0';
        if (line[0] == '\0') continue;

        int id, available, total;
        char title[100], author[100], category[50];

        char *token = strtok(line, ",");
        if (!token) continue;
        id = atoi(token);

        token = strtok(NULL, ",");
        if (!token) continue;
        strncpy(title, token, sizeof(title) - 1);
        title[sizeof(title) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        strncpy(author, token, sizeof(author) - 1);
        author[sizeof(author) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        strncpy(category, token, sizeof(category) - 1);
        category[sizeof(category) - 1] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        available = atoi(token);

        token = strtok(NULL, ",");
        if (!token) continue;
        total = atoi(token);

        BookNode *node = createBookNode(id, title, author, category, total);
        if (!node) continue;
        node->availableCopies = available;
        node->next = NULL;

        if (head == NULL) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
    }

    fclose(fp);
    return head;
}

void saveBooksToFile(BookNode *head, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error opening %s for writing\n", filename);
        return;
    }

    BookNode *current = head;
    while (current != NULL) {
        fprintf(fp, "%d,%s,%s,%s,%d,%d\n",
                current->bookID,
                current->title,
                current->author,
                current->category,
                current->availableCopies,
                current->totalCopies);
        current = current->next;
    }

    fclose(fp);
}

// ---------- Output as JSON for browser/server ----------

void printBooksAsJson(BookNode *head) {
    printf("[");
    BookNode *current = head;
    int first = 1;
    while (current != NULL) {
        if (!first) {
            printf(",");
        }
        first = 0;
        // NOTE: no special escaping for quotes in strings
        printf("{\"id\":%d,\"title\":\"%s\",\"author\":\"%s\","
               "\"category\":\"%s\",\"available\":%d,\"total\":%d}",
               current->bookID,
               current->title,
               current->author,
               current->category,
               current->availableCopies,
               current->totalCopies);
        current = current->next;
    }
    printf("]");
}

// ---------- Main: handle commands ----------

int main(int argc, char *argv[]) {
    const char *DATA_FILE = "books.txt";

    if (argc < 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s list\n", argv[0]);
        fprintf(stderr, "  %s add <id> <title> <author> <category> <totalCopies>\n", argv[0]);
        fprintf(stderr, "  %s delete <id>\n", argv[0]);
        fprintf(stderr, "  %s issue <id>\n", argv[0]);
        fprintf(stderr, "  %s return <id>\n", argv[0]);
        return 1;
    }

    const char *command = argv[1];

    // load current books from file
    BookNode *head = loadBooksFromFile_fixed(DATA_FILE);

    if (strcmp(command, "list") == 0) {
        // just print JSON of current list
        printBooksAsJson(head);
        freeAllBooks(&head);
        return 0;
    }

    // Commands that modify data
    if (strcmp(command, "add") == 0) {
        if (argc < 7) {
            fprintf(stderr, "Usage: %s add <id> <title> <author> <category> <totalCopies>\n", argv[0]);
            freeAllBooks(&head);
            return 1;
        }
        int id = atoi(argv[2]);
        const char *title = argv[3];
        const char *author = argv[4];
        const char *category = argv[5];
        int total = atoi(argv[6]);

        addBookSorted(&head, id, title, author, category, total);
        saveBooksToFile(head, DATA_FILE);
        printBooksAsJson(head); // optional: still print list after add

    } else if (strcmp(command, "delete") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s delete <id>\n", argv[0]);
            freeAllBooks(&head);
            return 1;
        }
        int id = atoi(argv[2]);
        deleteBook(&head, id);
        saveBooksToFile(head, DATA_FILE);
        printBooksAsJson(head);

    } else if (strcmp(command, "issue") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s issue <id>\n", argv[0]);
            freeAllBooks(&head);
            return 1;
        }
        int id = atoi(argv[2]);
        issueBook(head, id);
        saveBooksToFile(head, DATA_FILE);
        printBooksAsJson(head);

    } else if (strcmp(command, "return") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s return <id>\n", argv[0]);
            freeAllBooks(&head);
            return 1;
        }
        int id = atoi(argv[2]);
        returnBook(head, id);
        saveBooksToFile(head, DATA_FILE);
        printBooksAsJson(head);

    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        freeAllBooks(&head);
        return 1;
    }

    freeAllBooks(&head);
    return 0;
}