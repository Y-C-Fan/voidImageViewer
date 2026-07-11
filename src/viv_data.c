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
// Image metadata management

#include "viv.h"
#include "viv_data.h"

static _viv_image_data_t *_viv_image_data_start = 0;
static _viv_image_data_t *_viv_image_data_last = 0;
static int _viv_image_data_count = 0;

#define VIV_DATA_FILENAME "voidImageViewer_data.txt"

// Convert wide string to UTF-8
static utf8_t *_viv_wide_to_utf8(const wchar_t *wide_str)
{
	int len;
	utf8_t *buf;

	len = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, NULL, 0, NULL, NULL);
	if (len <= 0) return 0;

	buf = (utf8_t *)mem_alloc(len);
	if (!buf) return 0;

	WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, (char *)buf, len, NULL, NULL);
	return buf;
}

// Convert UTF-8 to wide string
static wchar_t *_viv_utf8_to_wide(const utf8_t *utf8_str)
{
	int len;
	wchar_t *buf;

	len = MultiByteToWideChar(CP_UTF8, 0, (const char *)utf8_str, -1, NULL, 0);
	if (len <= 0) return 0;

	buf = (wchar_t *)mem_alloc(len * sizeof(wchar_t));
	if (!buf) return 0;

	MultiByteToWideChar(CP_UTF8, 0, (const char *)utf8_str, -1, buf, len);
	return buf;
}

// Build the full path to the data file
static void _viv_data_get_path(wchar_t *out_path, const wchar_t *exe_path)
{
	string_path_combine_utf8(out_path, exe_path, (const utf8_t *)VIV_DATA_FILENAME);
}

// Parse a line: path|viewed(0/1)|rating(0-5)
// path is UTF-8 encoded in the file
static int _viv_data_parse_line(const utf8_t *line_utf8, wchar_t **out_path, int *out_viewed, int *out_rating)
{
	const utf8_t *p;
	const utf8_t *path_start;
	const utf8_t *path_end;
	utf8_t *path_utf8;

	*out_path = 0;
	*out_viewed = 0;
	*out_rating = 0;

	// Skip leading whitespace
	p = line_utf8;
	while (*p == ' ' || *p == '\t') p++;
	if (!*p) return 0;

	path_start = p;

	// Find first delimiter
	while (*p && *p != VIV_DATA_DELIMITER) p++;
	path_end = p;

	if (path_end == path_start) return 0;

	// Copy path as UTF-8
	{
		uintptr_t path_len = (uintptr_t)(path_end - path_start);
		path_utf8 = utf8_alloc_utf8_n((const char *)path_start, path_len);
	}

	// Skip delimiter
	if (*p == VIV_DATA_DELIMITER) p++;

	// Parse viewed
	{
		const utf8_t *viewed_start = p;
		while (*p && *p != VIV_DATA_DELIMITER && *p != '\r' && *p != '\n') p++;
		*out_viewed = utf8_to_int(viewed_start);
		if (*out_viewed < 0) *out_viewed = 0;
		if (*out_viewed > 1) *out_viewed = 1;
	}

	// Skip delimiter
	if (*p == VIV_DATA_DELIMITER) p++;

	// Parse rating
	{
		const utf8_t *rating_start = p;
		while (*p && *p != '\r' && *p != '\n') p++;
		*out_rating = utf8_to_int(rating_start);
		if (*out_rating < 0) *out_rating = 0;
		if (*out_rating > 5) *out_rating = 5;
	}

	// Convert path to wide string
	if (path_utf8)
	{
		*out_path = _viv_utf8_to_wide(path_utf8);
		mem_free(path_utf8);
	}

	return 1;
}

void viv_data_load(const wchar_t *exe_path)
{
	wchar_t filename[STRING_SIZE];
	HANDLE h;

	_viv_data_clear();

	_viv_data_get_path(filename, exe_path);

	h = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
	if (h == INVALID_HANDLE_VALUE)
	{
		// No data file yet - that's fine
		return;
	}

	{
		DWORD size;
		DWORD numread;
		char *buf;

		size = GetFileSize(h, 0);
		if (size == 0)
		{
			CloseHandle(h);
			return;
		}

		buf = (char *)mem_alloc(size + 1);
		if (!buf)
		{
			CloseHandle(h);
			return;
		}

		if (ReadFile(h, buf, size, &numread, NULL) && numread == size)
		{
			const utf8_t *p;
			const utf8_t *line_start;
			const utf8_t *line_end;

			buf[size] = 0;
			p = (const utf8_t *)buf;

			// Skip UTF-8 BOM if present
			if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
			{
				p += 3;
			}

			line_start = p;

			while (*p)
			{
				line_end = p;
				while (*p && *p != '\n' && *p != '\r') p++;

				if (line_end != line_start)
				{
					wchar_t *path;
					int viewed;
					int rating;
					uintptr_t line_len = (uintptr_t)(line_end - line_start);
					char *line_buf = (char *)mem_alloc(line_len + 1);

					if (line_buf)
					{
						memcpy(line_buf, line_start, line_len);
						line_buf[line_len] = 0;

						if (_viv_data_parse_line((const utf8_t *)line_buf, &path, &viewed, &rating))
						{
							if (path && path[0])
							{
								_viv_data_add(path, viewed, rating);
							}
							if (path) mem_free(path);
						}
						mem_free(line_buf);
					}
				}

				// Skip line ending
				if (*p == '\r') p++;
				if (*p == '\n') p++;

				line_start = p;
			}
		}

		mem_free(buf);
		CloseHandle(h);
	}
}

void viv_data_save(const wchar_t *exe_path)
{
	wchar_t filename[STRING_SIZE];
	wchar_t tempname[STRING_SIZE];
	HANDLE h;
	_viv_image_data_t *d;

	if (!_viv_image_data_count) return;

	_viv_data_get_path(filename, exe_path);
	string_copy(tempname, filename);
	string_cat_utf8(tempname, (const utf8_t *)".tmp");

	h = CreateFile(tempname, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (h == INVALID_HANDLE_VALUE) return;

	{
		DWORD numwritten;
		utf8_t *path_utf8;
		wchar_t line_buf[STRING_SIZE];

		d = _viv_image_data_start;
		while (d)
		{
			// Convert path to UTF-8 for writing
			path_utf8 = _viv_wide_to_utf8(d->path);
			if (path_utf8)
			{
				int ret;

				// Format: path|viewed|rating\n
				ret = swprintf(line_buf, STRING_SIZE, L"%hs|%d|%d\n", path_utf8, d->viewed, d->rating);
				if (ret > 0)
				{
					utf8_t *line_utf8 = _viv_wide_to_utf8(line_buf);
					if (line_utf8)
					{
						DWORD len = (DWORD)utf8_length(line_utf8);
						WriteFile(h, line_utf8, len, &numwritten, NULL);
						mem_free(line_utf8);
					}
				}
				mem_free(path_utf8);
			}

			d = d->next;
		}
	}

	CloseHandle(h);

	// Atomic replace
	MoveFileExW(tempname, filename, MOVEFILE_REPLACE_EXISTING);
}

void viv_data_free(void)
{
	_viv_data_clear();
}

static void _viv_data_clear(void)
{
	_viv_image_data_t *d;
	_viv_image_data_t *next;

	d = _viv_image_data_start;
	while (d)
	{
		next = d->next;
		if (d->path) mem_free(d->path);
		mem_free(d);
		d = next;
	}

	_viv_image_data_start = 0;
	_viv_image_data_last = 0;
	_viv_image_data_count = 0;
}

static _viv_image_data_t *_viv_data_add(const wchar_t *path, int viewed, int rating)
{
	_viv_image_data_t *d;

	// Check if already exists
	d = _viv_data_find_internal(path);
	if (d)
	{
		d->viewed = (BYTE)viewed;
		d->rating = (BYTE)rating;
		return d;
	}

	// Create new entry
	d = (_viv_image_data_t *)mem_alloc(sizeof(_viv_image_data_t));
	if (!d) return 0;

	d->path = (wchar_t *)mem_alloc((string_get_length(path) + 1) * sizeof(wchar_t));
	if (!d->path)
	{
		mem_free(d);
		return 0;
	}

	string_copy(d->path, path);
	d->viewed = (BYTE)viewed;
	d->rating = (BYTE)rating;
	d->next = 0;
	d->prev = _viv_image_data_last;

	if (_viv_image_data_last)
	{
		_viv_image_data_last->next = d;
	}
	else
	{
		_viv_image_data_start = d;
	}

	_viv_image_data_last = d;
	_viv_image_data_count++;

	return d;
}

static _viv_image_data_t *_viv_data_find_internal(const wchar_t *full_path)
{
	_viv_image_data_t *d;

	d = _viv_image_data_start;
	while (d)
	{
		if (string_compare(d->path, full_path) == 0)
		{
			return d;
		}
		d = d->next;
	}

	return 0;
}

_viv_image_data_t *viv_data_find(const wchar_t *full_path)
{
	return _viv_data_find_internal(full_path);
}

void viv_data_set_viewed(const wchar_t *full_path, int viewed)
{
	_viv_image_data_t *d;

	d = _viv_data_find_internal(full_path);
	if (!d)
	{
		d = _viv_data_add(full_path, viewed, 0);
		return;
	}

	if (d->viewed != (BYTE)viewed)
	{
		d->viewed = (BYTE)viewed;
	}
}

void viv_data_set_rating(const wchar_t *full_path, int rating)
{
	_viv_image_data_t *d;

	d = _viv_data_find_internal(full_path);
	if (!d)
	{
		d = _viv_data_add(full_path, 0, rating);
		return;
	}

	if (d->rating != (BYTE)rating)
	{
		d->rating = (BYTE)rating;
	}
}

int viv_data_toggle_viewed(const wchar_t *full_path)
{
	_viv_image_data_t *d;

	d = _viv_data_find_internal(full_path);
	if (!d)
	{
		d = _viv_data_add(full_path, 1, 0);
		return 1;
	}

	d->viewed = !d->viewed;
	return d->viewed;
}

int viv_data_get_unviewed_count(void)
{
	_viv_image_data_t *d;
	int count;

	count = 0;
	d = _viv_image_data_start;
	while (d)
	{
		if (!d->viewed)
		{
			count++;
		}
		d = d->next;
	}

	return count;
}

// Find the next unviewed image in the playlist, starting from the current image.
int viv_data_get_next_unviewed(WIN32_FIND_DATA *best_fd)
{
	_viv_playlist_t *current_d;
	_viv_playlist_t *d;
	int got_best;

	got_best = 0;
	os_zero_memory(best_fd, sizeof(WIN32_FIND_DATA));

	if (!_viv_playlist_start) return 0;

	current_d = _viv_playlist_from_fd(_viv_current_fd);

	d = _viv_playlist_start;
	while (d)
	{
		_viv_image_data_t *data;
		int compare_ret;

		if (d == current_d)
		{
			d = d->next;
			continue;
		}

		compare_ret = _viv_fd_compare(&d->fd, _viv_current_fd);
		if (compare_ret <= 0)
		{
			d = d->next;
			continue;
		}

		// Check if this image is unviewed
		data = _viv_data_find_internal(d->fd.cFileName);
		if (data && data->viewed)
		{
			d = d->next;
			continue;
		}

		// Found an unviewed image after current
		if (!got_best || _viv_fd_compare(&d->fd, best_fd) < 0)
		{
			os_copy_memory(best_fd, &d->fd, sizeof(WIN32_FIND_DATA));
			got_best = 1;
		}

		d = d->next;
	}

	// Wrap around if not found
	if (!got_best)
	{
		d = _viv_playlist_start;
		while (d)
		{
			_viv_image_data_t *data;

			if (d == current_d)
			{
				d = d->next;
				continue;
			}

			data = _viv_data_find_internal(d->fd.cFileName);
			if (data && data->viewed)
			{
				d = d->next;
				continue;
			}

			if (!got_best || _viv_fd_compare(&d->fd, best_fd) < 0)
			{
				os_copy_memory(best_fd, &d->fd, sizeof(WIN32_FIND_DATA));
				got_best = 1;
			}

			d = d->next;
		}
	}

	return got_best;
}

// Find the previous unviewed image in the playlist, before the current image.
int viv_data_get_prev_unviewed(WIN32_FIND_DATA *best_fd)
{
	_viv_playlist_t *current_d;
	_viv_playlist_t *d;
	_viv_playlist_t *last_unviewed;
	int got_best;

	got_best = 0;
	os_zero_memory(best_fd, sizeof(WIN32_FIND_DATA));

	if (!_viv_playlist_start) return 0;

	current_d = _viv_playlist_from_fd(_viv_current_fd);
	last_unviewed = 0;

	// Find the last unviewed image before current in sort order
	d = _viv_playlist_start;
	while (d)
	{
		_viv_image_data_t *data;
		int compare_ret;

		if (d == current_d)
		{
			d = d->next;
			continue;
		}

		compare_ret = _viv_fd_compare(&d->fd, _viv_current_fd);
		if (compare_ret >= 0)
		{
			d = d->next;
			continue;
		}

		data = _viv_data_find_internal(d->fd.cFileName);
		if (data && data->viewed)
		{
			d = d->next;
			continue;
		}

		if (!last_unviewed || _viv_fd_compare(&d->fd, &last_unviewed->fd) > 0)
		{
			last_unviewed = d;
		}

		d = d->next;
	}

	if (last_unviewed)
	{
		os_copy_memory(best_fd, &last_unviewed->fd, sizeof(WIN32_FIND_DATA));
		return 1;
	}

	// Wrap around: check from end of list
	d = _viv_playlist_start;
	while (d)
	{
		_viv_image_data_t *data;
		int compare_ret;

		if (d == current_d)
		{
			d = d->next;
			continue;
		}

		compare_ret = _viv_fd_compare(&d->fd, _viv_current_fd);
		if (compare_ret <= 0)
		{
			d = d->next;
			continue;
		}

		data = _viv_data_find_internal(d->fd.cFileName);
		if (data && data->viewed)
		{
			d = d->next;
			continue;
		}

		if (!got_best || _viv_fd_compare(&d->fd, best_fd) > 0)
		{
			os_copy_memory(best_fd, &d->fd, sizeof(WIN32_FIND_DATA));
			got_best = 1;
		}

		d = d->next;
	}

	return got_best;
}
