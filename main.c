#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <locale.h>
#include <dirent.h>
#include <sys/stat.h>

// Configuration du syst�me
#define PAGE_SIZE 4096
#define NUM_PAGES 1024
#define MEMORY_SIZE (PAGE_SIZE * NUM_PAGES)
#define MAX_FILENAME 256
#define MAX_FILES 100
#define BLOCK_SIZE 512
#define MAX_BLOCKS 1000
#define ERROR_FILE_EXISTS -1
#define ERROR_NO_SPACE -2

// Structures pour la gestion de la m�moire
typedef struct {
    int page_number;
    int frame_number;
    bool valid;
    bool dirty;
    bool referenced;
} PageTableEntry;

typedef struct {
    PageTableEntry* entries;
    int num_entries;
} PageTable;

// Structures pour le syst�me de fichiers
typedef struct {
    char filename[MAX_FILENAME];
    size_t size;
    int index_block;
    time_t created;
    time_t modified;
    bool is_used;
} FileMetadata;

typedef struct {
    FileMetadata files[MAX_FILES];
    int num_files;
} Directory;

// Variables globales
static void* memory_pool = NULL;
static int* frame_table = NULL;
static PageTable page_table = {NULL, 0};
static Directory root_directory = {{0}, 0};
static int* free_blocks = NULL;
static bool system_initialized = false;

// Prototypes de fonctions
void cleanup_system(void);
int initialize_system(void);

// Initialisation du syst�me
int initialize_system(void) {
    if (system_initialized) {
        return 0;  // D�j� initialis�
    }

    // Allocation de la m�moire principale
    memory_pool = malloc(MEMORY_SIZE);
    if (!memory_pool) {
        fprintf(stderr, "Erreur: Impossible d'allouer la memoire principale\n");
        return -1;
    }

    // Allocation de la table des frames
    frame_table = calloc(NUM_PAGES, sizeof(int));
    if (!frame_table) {
        free(memory_pool);
        fprintf(stderr, "Erreur: Impossible d'allouer la table des frames\n");
        return -1;
    }

    // Allocation de la table des pages
    page_table.entries = calloc(NUM_PAGES, sizeof(PageTableEntry));
    if (!page_table.entries) {
        free(memory_pool);
        free(frame_table);
        fprintf(stderr, "Erreur: Impossible d'allouer la table des pages\n");
        return -1;
    }
    page_table.num_entries = NUM_PAGES;

    // Allocation de la table des blocs libres
    free_blocks = calloc(MAX_BLOCKS, sizeof(int));
    if (!free_blocks) {
        free(memory_pool);
        free(frame_table);
        free(page_table.entries);
        fprintf(stderr, "Erreur: Impossible d'allouer la table des blocs\n");
        return -1;
    }

    // Initialisation du r�pertoire racine
    memset(&root_directory, 0, sizeof(Directory));

    system_initialized = true;
    return 0;
}


// Nettoyage du syst�me4
void cleanup_system(void) {
    if (memory_pool) {
        free(memory_pool);
        memory_pool = NULL;
    }
    if (frame_table) {
        free(frame_table);
        frame_table = NULL;
    }
    if (page_table.entries) {
        free(page_table.entries);
        page_table.entries = NULL;
    }
    if (free_blocks) {
        free(free_blocks);
        free_blocks = NULL;
    }
    system_initialized = false;
}

// Gestion des fichiers
int create_file(const char* filename) {
    if (!system_initialized || !filename) {
        return -1;
    }

    // V�rification de l'unicit� du nom
    for (int i = 0; i < MAX_FILES; i++) {
        if (root_directory.files[i].is_used &&
            strcmp(root_directory.files[i].filename, filename) == 0) {
            return ERROR_FILE_EXISTS;
        }
    }

    // Recherche d'un emplacement libre dans le r�pertoire
    int file_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!root_directory.files[i].is_used) {
            file_index = i;
            break;
        }
    }

    if (file_index == -1) {
        return ERROR_NO_SPACE;
    }

    // Recherche d'un bloc d'index libre
    int index_block = -1;
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (free_blocks[i] == 0) {
            index_block = i;
            free_blocks[i] = 1;
            break;
        }
    }

    if (index_block == -1) {
        return ERROR_NO_SPACE;
    }

    // Initialisation du fichier
    FileMetadata* file = &root_directory.files[file_index];
    strncpy(file->filename, filename, MAX_FILENAME - 1);
    file->filename[MAX_FILENAME - 1] = '\0';
    file->size = 0;
    file->index_block = index_block;
    file->created = time(NULL);
    file->modified = file->created;
    file->is_used = true;

    root_directory.num_files++;
    return 0;
}


int delete_file(const char* filename) {
    if (!system_initialized || !filename) {
        return -1;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (root_directory.files[i].is_used && strcmp(root_directory.files[i].filename, filename) == 0) {
            // Free the index block
            free_blocks[root_directory.files[i].index_block] = 0;

            // Clear the file metadata
            memset(&root_directory.files[i], 0, sizeof(FileMetadata));
            root_directory.num_files--;

            return 0;
        }
    }

    return -1; // File not found
}

int create_folder(const char* folder_name) {
    if (!system_initialized || !folder_name) {
        return -1;
    }

    if (mkdir(folder_name) == 0) {
        return 0;
    } else {
        perror("Error creating folder");
        return 1;
    }
}

int delete_folder(const char* folder_name) {
    if (!system_initialized || !folder_name) {
        return -1;
    }

    if (rmdir(folder_name) == 0) {
        return 0;
    } else {
        perror("Error deleting folder");
        return 1;
    }
}

// Interface utilisateur
void print_help(void) {
    setlocale(LC_ALL, "fr_FR.UTF-8");
    printf("\nCommandes disponibles:\n");
    printf("  create <filename>  - Cree un nouveau fichier\n");
    printf("  delete <filename>  - Supprime un fichier\n");
    printf("  create_folder <foldername> - Cree un nouveau dossier\n");
    printf("  delete_folder <foldername> - Supprime un dossier\n");
    printf("  list              - Liste tous les fichiers\n");
    printf("  help              - Affiche cette aide\n");
    printf("  exit              - Quitte le systeme\n");
}

void shell(void) {
    char command[256];
    char arg[MAX_FILENAME];

    printf("\nMini OS - Interface de commandes\n");
    printf("Tapez 'help' pour la liste des commandes\n\n");

    while (1) {
        printf("mini-os> ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }

        // Suppression du newline
        command[strcspn(command, "\n")] = 0;

        if (strncmp(command, "create ", 7) == 0) {
            sscanf(command, "create %s", arg);
            int result = create_file(arg);
            switch (result) {
                case 0:
                    printf("Fichier '%s' cree avec succ�s\n", arg);
                    break;
                case ERROR_FILE_EXISTS:
                    printf("Erreur: Le fichier '%s' existe deja\n", arg);
                    break;
                case ERROR_NO_SPACE:
                    printf("Erreur: Plus d'espace disponible\n");
                    break;
                default:
                    printf("Erreur lors de la creation du fichier\n");
            }
        }
        else if (strncmp(command, "delete ", 7) == 0) {
            sscanf(command, "delete %s", arg);
            int result = delete_file(arg);
            if (result == 0) {
                printf("File '%s' deleted successfully.\n", arg);
            } else {
                printf("Error deleting file '%s'.\n", arg);
            }
        }
        else if (strncmp(command, "create_folder ", 13) == 0) {
            sscanf(command, "create_folder %s", arg);
            int result = create_folder(arg);
            if (result == 0) {
                printf("Folder '%s' created successfully.\n", arg);
            } else {
                printf("Error creating folder '%s'.\n", arg);
            }
        } else if (strncmp(command, "delete_folder ", 14) == 0) {
            sscanf(command, "delete_folder %s", arg);
            int result = delete_folder(arg);
            if (result == 0) {
                printf("Folder '%s' deleted successfully.\n", arg);
            } else {
                printf("Error deleting folder '%s'.\n", arg);
            }
        }
        else if (strcmp(command, "list") == 0) {
            printf("\nListe des fichiers et dossiers:\n");
            bool found = false;

            // List files
            for (int i = 0; i < MAX_FILES; i++) {
                if (root_directory.files[i].is_used) {
                    printf("%-20s %6zu octets  (cree le: %s)\n",
                        root_directory.files[i].filename,
                        root_directory.files[i].size,
                        ctime(&root_directory.files[i].created));
                    found = true;
                }
            }

            // List folders
            DIR *dir;
            struct dirent *entry;

            dir = opendir(".");
            if (dir) {
                while ((entry = readdir(dir)) != NULL) {
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                        struct stat file_stat;
                        char full_path[256];
                        snprintf(full_path, sizeof(full_path), "%s", entry->d_name);

                        if (stat(full_path, &file_stat) == 0) {
                            if (S_ISDIR(file_stat.st_mode)) {
                                printf("%-20s (dossier) (cree le: %s)\n", entry->d_name, ctime(&file_stat.st_ctime));
                            } else {
                                printf("%-20s %10lld octets (cree le: %s)\n", entry->d_name, (long long)file_stat.st_size, ctime(&file_stat.st_ctime));
                            }
                            found = true;
                        }
                    }
                }
                closedir(dir);
            }

            if (!found) {
                printf("Aucun fichier ou dossier trouvé\n");
            }
        }
        else if (strcmp(command, "help") == 0) {
            print_help();
        }
        else if (strcmp(command, "exit") == 0) {
            printf("Arret du systeme...\n");
            break;
        }
        else if (strlen(command) > 0) {
            printf("Commande inconnue. Tapez 'help' pour la liste des commandes\n");
        }
    }
}

int main(void) {
    setlocale(LC_ALL, "fr_FR.UTF-8");
    if (initialize_system() != 0) {
        fprintf(stderr, "Erreur fatale: Impossible d'initialiser le systeme\n");
        return 1;
    }

    printf("Mini OS demarre avec succes\n");
    shell();

    cleanup_system();
    return 0;
}
