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

// ---------- Helper: read a line safely (for strings with spaces) ----------

void readLine(char *buffer, int size) {
    if (fgets(buffer, size, stdin) != NULL) {
        // remove trailing newline if present
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
    }
}

// ---------- Create a new node ----------

BookNode* createBookNode(
    int bookID,
    const char *title,
    const char *author,
    const char *category,
    int totalCopies
) {
    BookNode *newNode = (BookNode*)malloc(sizeof(BookNode));
    if (!newNode) {
        printf("Memory allocation failed!\n");
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

// ---------- Insert book in sorted order by bookID ----------

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

    // If list is empty or new node should be first
    if (*headRef == NULL || bookID < (*headRef)->bookID) {
        newNode->next = *headRef;
        *headRef = newNode;
        printf("Book added successfully (at head).\n");
        return;
    }

    // Traverse to find correct position
    BookNode *current = *headRef;
    while (current->next != NULL && current->next->bookID < bookID) {
        current = current->next;
    }

    // Optional: handle duplicate ID (if you don't want duplicates)
    if (current->next != NULL && current->next->bookID == bookID) {
        printf("Book ID %d already exists! Not adding duplicate.\n", bookID);
        free(newNode);
        return;
    }

    newNode->next = current->next;
    current->next = newNode;
    printf("Book added successfully.\n");
}

// ---------- Search book by ID ----------

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

// ---------- Delete book by ID ----------

int deleteBook(BookNode **headRef, int bookID) {
    if (*headRef == NULL) {
        return 0; // list empty
    }

    BookNode *current = *headRef;
    BookNode *prev = NULL;

    // If head itself holds the book
    if (current != NULL && current->bookID == bookID) {
        *headRef = current->next;
        free(current);
        return 1;
    }

    // Search for the book to be deleted
    while (current != NULL && current->bookID != bookID) {
        prev = current;
        current = current->next;
    }

    // If book not found
    if (current == NULL) {
        return 0;
    }

    // Unlink the node
    prev->next = current->next;
    free(current);
    return 1;
}

// ---------- Issue a book ----------

int issueBook(BookNode *head, int bookID) {
    BookNode *book = searchBook(head, bookID);
    if (book == NULL) {
        return 0; // not found
    }
    if (book->availableCopies > 0) {
        book->availableCopies -= 1;
        return 1; // issued
    } else {
        return -1; // no copies available
    }
}

// ---------- Return a book ----------

int returnBook(BookNode *head, int bookID) {
    BookNode *book = searchBook(head, bookID);
    if (book == NULL) {
        return 0; // not found
    }
    if (book->availableCopies < book->totalCopies) {
        book->availableCopies += 1;
        return 1; // returned
    } else {
        return -1; // all copies already in library
    }
}

// ---------- Display all books ----------

void displayBooks(BookNode *head) {
    if (head == NULL) {
        printf("\nNo books in the library.\n");
        return;
    }

    printf("\n%-6s %-25s %-20s %-15s %-10s\n",
           "ID", "Title", "Author", "Category", "Avail/Total");
    printf("-------------------------------------------------------------------------------\n");

    BookNode *current = head;
    while (current != NULL) {
        printf("%-6d %-25s %-20s %-15s %d/%d\n",
               current->bookID,
               current->title,
               current->author,
               current->category,
               current->availableCopies,
               current->totalCopies);
        current = current->next;
    }
}

// ---------- Export books to file (for JS visualization later) ----------

void exportBooksToFile(BookNode *head, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error opening file %s for writing.\n", filename);
        return;
    }

    // Format: ID,Title,Author,Category,Available,Total
    BookNode *current = head;
    while (current != NULL) {
        // Note: avoid commas in strings, or handle them later in JS
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
    printf("Books exported to %s successfully.\n", filename);
}

// ---------- Free all nodes (cleanup) ----------

void freeAllBooks(BookNode **headRef) {
    BookNode *current = *headRef;
    while (current != NULL) {
        BookNode *temp = current;
        current = current->next;
        free(temp);
    }
    *headRef = NULL;
}

// ---------- Main menu ----------

int main() {
    BookNode *head = NULL;
    int choice;
    int bookID, totalCopies;
    char title[100], author[100], category[50];

    do {
        printf("\n===== Library Book Tracking System (Linked List in C) =====\n");
        printf("1. Add Book\n");
        printf("2. Delete Book\n");
        printf("3. Search Book\n");
        printf("4. Issue Book\n");
        printf("5. Return Book\n");
        printf("6. Display All Books\n");
        printf("7. Export Books to File (books.txt)\n");
        printf("0. Exit\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) {
            // invalid input, clear and continue
            printf("Invalid input. Please enter a number.\n");
            // clear input buffer
            int c;
            while ((c = getchar()) != '\n' && c != EOF) {}
            continue;
        }

        // clear leftover newline before reading strings
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}

        switch (choice) {
            case 1:
                printf("Enter Book ID (integer): ");
                scanf("%d", &bookID);
                while ((c = getchar()) != '\n' && c != EOF) {}

                printf("Enter Title: ");
                readLine(title, sizeof(title));

                printf("Enter Author: ");
                readLine(author, sizeof(author));

                printf("Enter Category: ");
                readLine(category, sizeof(category));

                printf("Enter Total Copies: ");
                scanf("%d", &totalCopies);
                while ((c = getchar()) != '\n' && c != EOF) {}

                addBookSorted(&head, bookID, title, author, category, totalCopies);
                break;

            case 2:
                printf("Enter Book ID to delete: ");
                scanf("%d", &bookID);
                while ((c = getchar()) != '\n' && c != EOF) {}

                if (deleteBook(&head, bookID)) {
                    printf("Book deleted successfully.\n");
                } else {
                    printf("Book with ID %d not found.\n", bookID);
                }
                break;

            case 3: {
                printf("Enter Book ID to search: ");
                scanf("%d", &bookID);
                while ((c = getchar()) != '\n' && c != EOF) {}

                BookNode *found = searchBook(head, bookID);
                if (found) {
                    printf("Book found!\n");
                    printf("ID: %d\nTitle: %s\nAuthor: %s\nCategory: %s\nAvailable: %d\nTotal: %d\n",
                           found->bookID, found->title, found->author,
                           found->category, found->availableCopies, found->totalCopies);
                } else {
                    printf("Book with ID %d not found.\n", bookID);
                }
                break;
            }

            case 4: {
                printf("Enter Book ID to issue: ");
                scanf("%d", &bookID);
                while ((c = getchar()) != '\n' && c != EOF) {}

                int result = issueBook(head, bookID);
                if (result == 1) {
                    printf("Book issued successfully.\n");
                } else if (result == 0) {
                    printf("Book with ID %d not found.\n", bookID);
                } else {
                    printf("No copies available to issue.\n");
                }
                break;
            }

            case 5: {
                printf("Enter Book ID to return: ");
                scanf("%d", &bookID);
                while ((c = getchar()) != '\n' && c != EOF) {}

                int result = returnBook(head, bookID);
                if (result == 1) {
                    printf("Book returned successfully.\n");
                } else if (result == 0) {
                    printf("Book with ID %d not found.\n", bookID);
                } else {
                    printf("All copies are already in library. Cannot return extra.\n");
                }
                break;
            }

            case 6:
                displayBooks(head);
                break;

            case 7:
                exportBooksToFile(head, "books.txt");
                break;

            case 0:
                printf("Exiting...\n");
                break;

            default:
                printf("Invalid choice. Please try again.\n");
        }

    } while (choice != 0);

    freeAllBooks(&head);
    return 0;
}
