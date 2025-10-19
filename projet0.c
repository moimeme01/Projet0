#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define HEAP_SIZE 64000

// ============================================================================
//  HEAP SIMULÉE
// ----------------------------------------------------------------------------
// Cette variable globale représente la heap simulée.
// Tous les malloc/free travaillent *uniquement* dans ce tableau.
// ============================================================================
uint8_t MY_HEAP[HEAP_SIZE];

/* ============================================================================
    STRUCTURE D’UN BLOC MÉMOIRE
   ----------------------------------------------------------------------------
   Chaque allocation commence par une en-tête `Block` qui contient :
   - la taille de la zone utilisateur (size)
   - un indicateur libre/occupé (free)
   - deux pointeurs logiques (next/prev) pour relier plusieurs fragments
   - le champ `data[]` qui représente la mémoire retournée à l’utilisateur

   Structure visuelle :
   ┌────────────────────────────┬────────────────────────────┐
   │ En-tête (struct Block)     │ Zone utilisateur (data[])  │
   ├────────────────────────────┼────────────────────────────┤
   │ size | free | next | prev  │ (octets accessibles)       │
   └────────────────────────────┴────────────────────────────┘
   ============================================================================
*/
typedef struct Block {
    size_t size;           // Taille utile en octets (zone data[])
    int free;              // 1 = libre, 0 = occupé
    struct Block *next;    // Pointeur logique vers le fragment suivant (même malloc)
    struct Block *prev;    // Pointeur logique vers le fragment précédent
    unsigned char data[];  // Début de la zone utilisateur
} Block;

/* ============================================================================
   init_heap()
   ----------------------------------------------------------------------------
   Initialise la heap virtuelle avec un seul grand bloc libre.
   Ce bloc occupe tout MY_HEAP (moins la taille de l'en-tête).
   ============================================================================
*/
static void init_heap() {
    Block *first = (Block *)MY_HEAP;
    first->size = HEAP_SIZE - sizeof(Block);
    first->free = 1;
    first->next = NULL;
    first->prev = NULL;
}

/* ============================================================================
    my_malloc()
   ----------------------------------------------------------------------------
   Fonction principale d’allocation mémoire.
   Objectif :
     - Trouver un bloc libre assez grand.
     - Si le bloc est plus grand que nécessaire → le découper.
     - Si la mémoire est fragmentée → lier plusieurs fragments logiquement.

   Logique de fragmentation :
     Si on veut allouer 1024 octets mais qu’on a deux blocs de 512 libres,
     my_malloc() relie les deux :
         [Bloc B] <-> [Bloc D’]
     Ces blocs forment un seul malloc logique.

   Retourne :
     → un pointeur vers le champ `data[]` du premier bloc utilisé.
   ============================================================================
*/
void *my_malloc(size_t size) {
    if (size == 0) return NULL;

    static int initialized = 0;
    if (!initialized) {  // On initialise la heap une seule fois
        init_heap();
        initialized = 1;
    }

    size_t remaining = size;   // Octets encore à allouer
    Block *first = NULL;       // Premier fragment logique
    Block *last  = NULL;       // Dernier fragment logique
    unsigned char *cursor = MY_HEAP; // Curseur pour parcourir la heap

    // === PARCOURS PHYSIQUE DE LA HEAP ===
    while (cursor < MY_HEAP + HEAP_SIZE && remaining > 0) {
        Block *cur = (Block*)cursor;

        if (cur->free && cur->size > 0) {

            // ---  Découpage de bloc ---
            // Si le bloc est plus grand que nécessaire, on le scinde :
            // [Bloc occupé][Bloc libre]
            if (cur->size > remaining + sizeof(Block)) {
                Block *tail = (Block*)(cur->data + remaining);
                tail->size = cur->size - remaining - sizeof(Block);
                tail->free = 1;
                tail->next = NULL;
                tail->prev = NULL;
                cur->size = remaining;
            }

            // --- Marquer comme occupé ---
            cur->free = 0;

            // --- Chaînage logique ---
            // Si c’est le premier fragment utilisé, on le mémorise
            if (!first) first = cur;

            // Sinon, on relie logiquement le bloc précédent et le courant
            if (last) {
                last->next = cur;
                cur->prev = last;
            }

            last = cur;

            // --- Soustraction de la taille utilisée ---
            remaining -= (cur->size >= remaining) ? remaining : cur->size;
        }

        // --- Passage au bloc suivant dans la mémoire ---
        cursor += sizeof(Block) + cur->size;
    }

    // ===  Si la mémoire est insuffisante ===
    if (remaining > 0) {
        for (Block *b = first; b; b = b->next) b->free = 1;
        for (Block *b = first; b;) {
            Block *n = b->next;
            b->next = b->prev = NULL;
            b = n;
        }
        printf("Mémoire insuffisante : il manque %zu octets.\n", remaining);
        return NULL;
    }

    // === Retour du pointeur utilisateur ===
    // On retourne l’adresse de `data[]`, c’est-à-dire la partie accessible.
    return first ? first->data : NULL;
}

/* ============================================================================
   my_free()
   ----------------------------------------------------------------------------
   Libère un bloc alloué par my_malloc().

   Étapes :
    - Retrouver le début du bloc à partir du pointeur utilisateur.
    - Remonter au premier fragment logique (si la malloc a été fragmentée).
    - Libérer chaque fragment logique.
    - Fusionner physiquement les blocs libres adjacents dans la heap.

   Explication de la ligne clé :
     Block *first = (Block *)((unsigned char*)pointer - offsetof(Block, data));

   → `pointer` pointe sur data[], pas sur le début du bloc.
   → `offsetof(Block, data)` donne le décalage entre le début de Block et data.
   → En soustrayant cet offset, on “remonte” jusqu’au début du bloc.

   Exemple concret :
     Si un bloc commence à 0x1000 et data[] à 0x101C (soit +28 octets),
     alors :
         pointer = 0x101C
         offsetof(Block, data) = 28
         (unsigned char*)pointer - 28 = 0x1000
     On retombe exactement sur le début du bloc.

   On cast ensuite en (Block *) pour accéder à ses champs (size, free, etc.)
   ============================================================================
*/
void my_free(void *pointer) {
    if (!pointer) return;

    // Retrouver le début du bloc en remontant depuis data[]
    Block *first = (Block *)((unsigned char*)pointer - offsetof(Block, data));

    // Remonter au tout premier fragment logique si plusieurs blocs ont été liés
    while (first->prev) first = first->prev;

    // Libérer tous les fragments logiquement liés
    for (Block *b = first; b; b = b->next) b->free = 1;

    // Supprimer les liens logiques
    for (Block *b = first; b;) {
        Block *n = b->next;
        b->next = b->prev = NULL;
        b = n;
    }

    // Fusionner les blocs libres adjacents physiquement (coalescence)
    unsigned char *cursor = MY_HEAP;
    while (cursor < MY_HEAP + HEAP_SIZE) {
        Block *cur = (Block*)cursor;
        unsigned char *next_addr = cursor + sizeof(Block) + cur->size;
        if (next_addr >= MY_HEAP + HEAP_SIZE) break;

        Block *n = (Block*)next_addr;

        // Si deux blocs libres se suivent physiquement, on les fusionne
        if (cur->free && n->free) {
            cur->size += sizeof(Block) + n->size;
            continue; // on reste sur le même bloc pour vérifier s’il y a encore un libre derrière
        }

        cursor = next_addr;
    }
}