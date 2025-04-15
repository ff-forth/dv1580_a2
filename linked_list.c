#include "linked_list.h"
#include <stdio.h>  
#include <stdlib.h>
#include <stdlib.h>
#include <stdbool.h>
#include "memory_manager.h"
#include <string.h>
#include <pthread.h>

// Global mutex for list operations
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

void list_init(Node** head, size_t size)
{
    // check if size is greater than 0
    if (size <= 0)
    {
        fprintf(stderr, "list_init failed: Too small, list size is %zu\n", size);
        return;
    }
    
    // initialze list
    mem_init(size);
    *head = NULL;
};

void list_insert(Node** head, uint16_t data)
{
    Node* new_node = mem_alloc(sizeof(Node));
    if (!new_node) {
        fprintf(stderr, "list_insert failed: Memory allocation failed\n");
        return;
    }

    pthread_mutex_init(&new_node->lock, NULL);
    new_node->data = data;
    new_node->next = NULL;

    pthread_mutex_lock(&list_mutex);
    
    if (*head == NULL) {
        *head = new_node;
    } else {
        Node* current = *head;
        pthread_mutex_lock(&current->lock);
        
        while (current->next != NULL) {
            Node* next = current->next;
            pthread_mutex_lock(&next->lock);
            pthread_mutex_unlock(&current->lock);
            current = next;
        }
        
        current->next = new_node;
        pthread_mutex_unlock(&current->lock);
    }
    
    pthread_mutex_unlock(&list_mutex);
};

void list_insert_after(Node* prev_node, uint16_t data)
{
    if (!prev_node) {
        fprintf(stderr, "list_insert_after failed: Previous node is NULL\n");
        return;
    }

    Node* new_node = mem_alloc(sizeof(Node));
    if (!new_node) {
        fprintf(stderr, "list_insert_after failed: Memory allocation failed\n");
        return;
    }

    pthread_mutex_init(&new_node->lock, NULL);
    new_node->data = data;

    pthread_mutex_lock(&prev_node->lock);
    new_node->next = prev_node->next;
    prev_node->next = new_node;
    pthread_mutex_unlock(&prev_node->lock);
};

void list_insert_before(Node** head, Node* next_node, uint16_t data)
{
    if (!next_node) {
        fprintf(stderr, "list_insert_before failed: Next node is NULL\n");
        return;
    }

    Node* new_node = mem_alloc(sizeof(Node));
    if (!new_node) {
        fprintf(stderr, "list_insert_before failed: Memory allocation failed\n");
        return;
    }

    pthread_mutex_init(&new_node->lock, NULL);
    new_node->data = data;
    new_node->next = next_node;

    pthread_mutex_lock(&list_mutex);
    
    if (*head == next_node) {
        *head = new_node;
        pthread_mutex_unlock(&list_mutex);
        return;
    }

    Node* current = *head;
    pthread_mutex_lock(&current->lock);
    
    while (current->next != next_node) {
        Node* next = current->next;
        pthread_mutex_lock(&next->lock);
        pthread_mutex_unlock(&current->lock);
        current = next;
    }
    
    current->next = new_node;
    pthread_mutex_unlock(&current->lock);
    pthread_mutex_unlock(&list_mutex);
};

void list_delete(Node** head, uint16_t data)
{
    pthread_mutex_lock(&list_mutex);
    
    if (*head == NULL) {
        pthread_mutex_unlock(&list_mutex);
        fprintf(stderr, "list_delete failed: List is empty\n");
        return;
    }

    Node* current = *head;
    Node* prev = NULL;
    
    pthread_mutex_lock(&current->lock);
    
    while (current != NULL && current->data != data) {
        if (prev) {
            pthread_mutex_unlock(&prev->lock);
        }
        prev = current;
        current = current->next;
        if (current) {
            pthread_mutex_lock(&current->lock);
        }
    }

    if (current == NULL) {
        if (prev) {
            pthread_mutex_unlock(&prev->lock);
        }
        pthread_mutex_unlock(&list_mutex);
        fprintf(stderr, "list_delete failed: Node with data %hu not found\n", data);
        return;
    }

    if (prev == NULL) {
        *head = current->next;
    } else {
        prev->next = current->next;
        pthread_mutex_unlock(&prev->lock);
    }

    pthread_mutex_unlock(&current->lock);
    pthread_mutex_unlock(&list_mutex);
    
    pthread_mutex_destroy(&current->lock);
    mem_free(current);
};

Node* list_search(Node** head, uint16_t data)
{
    pthread_mutex_lock(&list_mutex);
    
    if (*head == NULL) {
        pthread_mutex_unlock(&list_mutex);
        fprintf(stderr, "list_search failed: List is empty\n");
        return NULL;
    }
    
    Node* current = *head;
    pthread_mutex_lock(&current->lock);
    pthread_mutex_unlock(&list_mutex);
    
    while (current != NULL) {
        if (current->data == data) {
            pthread_mutex_unlock(&current->lock);
            return current;
        }
        
        Node* next = current->next;
        if (next) {
            pthread_mutex_lock(&next->lock);
        }
        pthread_mutex_unlock(&current->lock);
        current = next;
    }
    
    return NULL;
};

void list_display(Node** head)
{
    list_display_range(head,NULL,NULL);
};

void list_display_range(Node** head, Node* start_node, Node* end_node)
{
    pthread_mutex_lock(&list_mutex);
    
    if (start_node == NULL) {
        start_node = *head;
    }
    
    if (!start_node) {
        pthread_mutex_unlock(&list_mutex);
        printf("[]\n");
        return;
    }

    Node* current = start_node;
    pthread_mutex_lock(&current->lock);
    pthread_mutex_unlock(&list_mutex);
    
    printf("[");
    while (current != NULL) {
        printf("%d", current->data);
        
        if (current == end_node) {
            break;
        }
        
        Node* next = current->next;
        if (next) {
            pthread_mutex_lock(&next->lock);
        }
        pthread_mutex_unlock(&current->lock);
        current = next;
        
        if (current) {
            printf(", ");
        }
    }
    printf("]\n");
    
    if (current) {
        pthread_mutex_unlock(&current->lock);
    }
};

int list_count_nodes(Node** head)
{
    pthread_mutex_lock(&list_mutex);
    
    if (*head == NULL) {
        pthread_mutex_unlock(&list_mutex);
        return 0;
    }
    
    int count = 0;
    Node* current = *head;
    pthread_mutex_lock(&current->lock);
    pthread_mutex_unlock(&list_mutex);
    
    while (current != NULL) {
        count++;
        Node* next = current->next;
        if (next) {
            pthread_mutex_lock(&next->lock);
        }
        pthread_mutex_unlock(&current->lock);
        current = next;
    }
    
    return count;
};

void list_cleanup(Node** head)
{
    pthread_mutex_lock(&list_mutex);
    
    Node* current = *head;
    while (current != NULL) {
        Node* next = current->next;
        pthread_mutex_destroy(&current->lock);
        mem_free(current);
        current = next;
    }
    
    *head = NULL;
    pthread_mutex_unlock(&list_mutex);
    mem_deinit();
};
