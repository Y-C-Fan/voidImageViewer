//
// Copyright 2026 hesphoros
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Image metadata management (viewed status + rating)
// Data file: voidImageViewer_data.txt (UTF-8, one entry per line)
// Format: full_path|viewed(0/1)|rating(0-5)

#ifndef VIV_DATA_H
#define VIV_DATA_H

#include "viv.h"

#ifdef __cplusplus
extern "C" {
#endif

// Image metadata entry
typedef struct _viv_image_data_s
{
	struct _viv_image_data_s *next;
	struct _viv_image_data_s *prev;
	wchar_t *path;		// full path to image (wide string)
	BYTE viewed;		// 0 = unviewed, 1 = viewed
	BYTE rating;		// 0-5 stars

} _viv_image_data_t;

// Load metadata from data file (called at startup)
void viv_data_load(const wchar_t *exe_path);

// Save metadata to data file (called at exit)
void viv_data_save(void);

// Free all metadata
void viv_data_free(void);

// Find metadata by full path (wide string). Returns NULL if not found.
_viv_image_data_t *viv_data_find(const wchar_t *full_path);

// Set viewed status. Creates entry if not exists.
void viv_data_set_viewed(const wchar_t *full_path, int viewed);

// Set rating (0-5). Creates entry if not exists.
void viv_data_set_rating(const wchar_t *full_path, int rating);

// Toggle viewed status. Returns new status.
int viv_data_toggle_viewed(const wchar_t *full_path);

// Get count of unviewed images in a playlist
int viv_data_get_unviewed_count(void);

// Get the next unviewed image fd relative to current. Returns 1 if found, 0 if not.
// Sets best_fd to the found image's WIN32_FIND_DATA.
int viv_data_get_next_unviewed(WIN32_FIND_DATA *best_fd);

// Get the previous unviewed image fd relative to current. Returns 1 if found, 0 if not.
int viv_data_get_prev_unviewed(WIN32_FIND_DATA *best_fd);

#ifdef __cplusplus
}
#endif

#endif // VIV_DATA_H
