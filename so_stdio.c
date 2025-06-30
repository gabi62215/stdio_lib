#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "so_stdio.h"
#include "file.h"

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *file;
	int fd;

	/* open file according to mode */
	if (strcmp(mode, "r") == 0)
		fd = open(pathname, O_RDONLY);
	else if (strcmp(mode, "r+") == 0)
		fd = open(pathname, O_RDWR);
	else if (strcmp(mode, "w") == 0)
		fd = open(pathname, O_WRONLY | O_TRUNC | O_CREAT, PERM);
	else if (strcmp(mode, "w+") == 0)
		fd = open(pathname, O_RDWR | O_TRUNC | O_CREAT, PERM);
	else if (strcmp(mode, "a") == 0)
		fd = open(pathname, O_WRONLY | O_APPEND | O_CREAT, PERM);
	else if (strcmp(mode, "a+") == 0)
		fd = open(pathname, O_RDWR | O_APPEND | O_CREAT, PERM);
	else
		return NULL;

	if (fd < 0)
		return NULL;

	/* allocate file structure */
	file = (SO_FILE *)calloc(1, sizeof(SO_FILE));
	if (!file)
		return NULL;

	file->fd = fd;

	return file;
}

int so_fclose(SO_FILE *stream)
{
	if (!stream)
		return SO_EOF;

	/* flush buffer before closing file */
	int ret = so_fflush(stream);

	if (ret < 0) {
		ret = close(stream->fd);
		free(stream);
		return SO_EOF;
	}

	/* close file and free strucutre */
	ret = close(stream->fd);
	if (ret < 0) {
		free(stream);
		return SO_EOF;
	}
	free(stream);

	return 0;
}

int so_feof(SO_FILE *stream)
{
	if (!stream)
		return SO_EOF;
	return stream->end_of_file;
}

int so_ferror(SO_FILE *stream)
{
	if (!stream)
		return SO_EOF;
	return stream->error;
}

int so_fileno(SO_FILE *stream)
{
	if (!stream)
		return SO_EOF;
	return stream->fd;
}

long so_ftell(SO_FILE *stream)
{
	if (!stream)
		return SO_EOF;
	return stream->cursor;
}

/* function taken from the lab */
ssize_t xwrite(int fd, const void *buf, size_t count)
{
	size_t bytes_written = 0;

	while (bytes_written < count) {
		ssize_t bytes_written_now = write(fd, buf + bytes_written,
										  count - bytes_written);

		if (bytes_written_now <= 0) /* I/O error */
			return -1;

		bytes_written += bytes_written_now;
	}

	return bytes_written;
}

int so_fflush(SO_FILE *stream)
{
	if (!stream)
		return SO_EOF;

	int ret;

	/* writes data from buffer to file */
	ret = xwrite(stream->fd, stream->buffer_w, stream->buff_cursor_w);

	if (ret < 0)
		return SO_EOF;

	/* resets buffer */
	stream->buff_cursor_w = 0;
	memset(stream->buffer_w, 0, BUFSIZE);

	return 0;
}

int so_fgetc(SO_FILE *stream)
{
	if (!stream)
		return SO_EOF;

	int ret;

	/* if buffer is full or empty read from file */
	if (stream->buff_size_r == 0 ||
		stream->buff_cursor_r == stream->buff_size_r) {
		memset(stream->buffer_r, 0, BUFSIZE);
		ret = read(stream->fd, stream->buffer_r, BUFSIZE);

		if (ret < 0) {
			stream->error = 1;
			return SO_EOF;
		}
		if (ret == 0) {
			stream->end_of_file = 1;
			return SO_EOF;
		}

		/* reset buffer cursor and update size */
		stream->buff_size_r = ret;
		stream->buff_cursor_r = 0;
	}

	/* update file and buffer cursors and set last operation */
	stream->cursor++;
	stream->buff_cursor_r++;
	stream->last_operation = READ;

	return stream->buffer_r[stream->buff_cursor_r - 1];
}

int so_fputc(int c, SO_FILE *stream)
{
	if (!stream)
		return SO_EOF;

	int ret;

	/* if the buffer is full flush it */
	if (stream->buff_cursor_w == BUFSIZE) {
		ret = so_fflush(stream);
		if (ret != 0)
			return SO_EOF;
	}

	/* upate cursors, place character in buffer and update last operation */
	stream->cursor++;
	stream->buffer_w[stream->buff_cursor_w] = c;
	stream->buff_cursor_w++;
	stream->last_operation = WRITE;

	return c;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	if (!stream)
		return 0;

	int bytes_read;
	int bytes_to_read = size * nmemb;
	int cursor = 0;
	int copied_bytes = 0;

	while (bytes_to_read > 0) {

		/* if buffer is full or empty do a read */
		if (stream->buff_size_r == 0 ||
			stream->buff_cursor_r == stream->buff_size_r) {
			memset(stream->buffer_r, 0, BUFSIZE);
			bytes_read = read(stream->fd, stream->buffer_r, BUFSIZE);

			if (bytes_read == 0) {
				stream->end_of_file = 1;
				return cursor / size;
			}
			if (bytes_read < 0) {
				stream->error = 1;
				return 0;
			}

			stream->buff_size_r = bytes_read;
			stream->buff_cursor_r = 0;
		}

		/* copy reamining bytes from the buffer to ptr */
		if (bytes_to_read > (stream->buff_size_r - stream->buff_cursor_r))
			copied_bytes = (stream->buff_size_r - stream->buff_cursor_r);
		else
			copied_bytes = bytes_to_read;

		memcpy(ptr + cursor, stream->buffer_r + stream->buff_cursor_r,
			copied_bytes);
		bytes_to_read -= copied_bytes;
		stream->buff_cursor_r += copied_bytes;
		cursor += copied_bytes;
		stream->cursor += copied_bytes;
	}

	stream->last_operation = READ;

	return cursor / size;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	if (!stream)
		return 0;

	int ret = 0;
	int bytes_to_write = size * nmemb;
	int cursor = 0;
	int copied_bytes = 0;

	while (bytes_to_write > 0) {

		/* if buffer is full flush it */
		if (stream->buff_cursor_w == BUFSIZE) {
			ret = so_fflush(stream);
			if (ret < 0) {
				stream->error = 1;
				return 0;
			}
		}

		/* copy bytes acording to remaining size of buffer */
		bytes_to_write -= copied_bytes;
		if (bytes_to_write > BUFSIZE - stream->buff_cursor_w)
			copied_bytes = BUFSIZE - stream->buff_cursor_w;
		else
			copied_bytes = bytes_to_write;

		memcpy(stream->buffer_w + stream->buff_cursor_w, ptr + cursor,
			copied_bytes);
		stream->buff_cursor_w += copied_bytes;
		cursor += copied_bytes;
		stream->cursor += copied_bytes;
	}

	/* update last operation */
	stream->last_operation = READ;

	return cursor / size;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	if (!stream)
		return SO_EOF;

	int ret;

	/* check last operation */
	if (stream->last_operation == WRITE || stream->last_operation == READ) {
		so_fflush(stream);
		memset(stream->buffer_r, 0, BUFSIZE);
		stream->buff_cursor_r = 0;
		stream->buff_size_r = 0;
	}

	/* call lseek if whence is valid */
	if (whence == SEEK_END || whence == SEEK_CUR || whence == SEEK_SET)
		ret = lseek(stream->fd, offset, whence);

	if (ret == -1) {
		stream->error = 1;
		return SO_EOF;
	}

	/* update cursor and last operation */
	stream->cursor = ret;
	stream->last_operation = SEEK;

	return 0;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *file;
	int fds[2];
	int ret;
	int fd_child = 0;
	int fd_parent = 0;
	pid_t pid;

	/* check for read or write */
	if (strcmp(type, "r") == 0) {
		fd_parent = PIPE_READ;
		fd_child = PIPE_WRITE;
	} else if (strcmp(type, "w") == 0) {
		fd_parent = PIPE_WRITE;
		fd_child = PIPE_READ;
	} else
		return NULL;

	/* create pipe */
	ret = pipe(fds);

	if (ret < 0)
		return NULL;

	pid = fork();

	switch (pid) {
	case -1:
		/* fork failed */
		close(fds[fd_child]);
		close(fds[fd_parent]);

		return NULL;
	case 0:
		/* child process */
		close(fds[fd_parent]);

		/* redirect stdin or stdout */
		ret = dup2(fds[fd_child], fd_child);
		if (ret < 0)
			return NULL;

		/* exec command */
		ret = execlp("sh", "sh", "-c", command, (char  *) NULL);
		if (ret < 0)
			return NULL;

		break;
	default:
		/* parent process */
		close(fds[fd_child]);

		/* create SO_FILE structure and return it */
		file = (SO_FILE *)calloc(1, sizeof(SO_FILE));

		if (!file) {
			close(fds[fd_parent]);
			return NULL;
		}

		file->fd = fds[fd_parent];
		file->pid = pid;

		return file;
	}

	return NULL;
}

int so_pclose(SO_FILE *stream)
{
	if (!stream)
		return SO_EOF;

	int ret;

	/* flush buffer before freeing */
	ret = so_fflush(stream);
	if (ret < 0) {
		close(stream->fd);
		free(stream);
		return SO_EOF;
	}

	/* close fd */
	ret = close(stream->fd);
	if (ret < 0) {
		free(stream);
		return SO_EOF;
	}

	/* wait for process */
	ret = waitpid(stream->pid, NULL, 0);
	if (ret < 0) {
		free(stream);
		return SO_EOF;
	}

	/* free structure */
	free(stream);

	return 0;
}
