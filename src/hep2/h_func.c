// HAYA'S EXPANSION PACK 2
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by haya3218.

// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hep2/h_func.c
/// \brief Helper functions

#include "h_general.h"
#include "../z_zone.h"

// Function to add an element to an array dynamically
void add_element(void **array, UINT8 *capacity, UINT8 typeSize, void *new_elem) {
    // If the array is not initialized
    if (*array == NULL) {
        *array = Z_Malloc(typeSize, PU_STATIC, NULL);
        if (*array == NULL) {
            // Handle memory allocation failure
            return; // Or any appropriate error handling
        }
        *capacity = 1;
        memcpy(*array, new_elem, typeSize); // Copy the new element
        return;
    }

    // Increase the size of the array
    *capacity++;

    // Reallocate memory for the array
    void *temp = Z_Realloc(*array, (*capacity) * typeSize, PU_STATIC, NULL);
    if (temp == NULL) {
        // Handle reallocation failure
        // Free the old memory block to avoid memory leaks
        Z_Free(*array);
        return; // Or any appropriate error handling
    }
    
    *array = temp; // Update array to point to the newly reallocated memory

    // Copy the new element to the end of the array
    void *dest = (char *)(*array) + ((*capacity - 1) * typeSize);
    memcpy(dest, new_elem, typeSize);
}