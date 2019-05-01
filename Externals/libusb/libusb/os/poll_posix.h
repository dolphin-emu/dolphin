#ifndef LIBUSB_POLL_POSIX_H
#define LIBUSB_POLL_POSIX_H

#define usbi_write write
#define usbi_read read
#define usbi_close close
#define usbi_poll poll

int usbi_pipe(int pipefd[2]);

#define usbi_inc_fds_ref(x, y)
#define usbi_dec_fds_ref(x, y)

#endif /* LIBUSB_POLL_POSIX_H */
