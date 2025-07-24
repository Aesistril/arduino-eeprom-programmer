#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define CHUNK_SIZE 64
#define BAUD_RATE B57600

// Configure serial port at 9600 baud, 8N1, no flow control
int open_serial(const char *device) {
  int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0) {
    perror("open");
    return -1;
  }

  struct termios tty;
  if (tcgetattr(fd, &tty) != 0) {
    perror("tcgetattr");
    close(fd);
    return -1;
  }

  cfsetospeed(&tty, BAUD_RATE);
  cfsetispeed(&tty, BAUD_RATE);

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
  tty.c_iflag &= ~IGNBRK;                     // disable break processing
  cfmakeraw(&tty); // Fully raw mode — like picocom does
  tty.c_cc[VMIN] = 1;  // read blocks until at least 1 char arrives
  tty.c_cc[VTIME] = 0; // no timeout

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

  tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls, enable reading
  tty.c_cflag &= ~(PARENB | PARODD); // no parity
  tty.c_cflag &= ~CSTOPB;            // 1 stop bit
  tty.c_cflag &= ~CRTSCTS;           // no hardware flow control

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    perror("tcsetattr");
    close(fd);
    return -1;
  }

  tcflush(fd, TCIOFLUSH);

  sleep(2);
  return fd;
}

void activate_mode(int serial_fd, const char *mode) {
  tcflush(serial_fd, TCIFLUSH);       // clear any junk first
  sleep(2);                  // small pre-delay
  write(serial_fd, mode, 2);         // send mode
  tcdrain(serial_fd);                 // ensure it was sent
  sleep(2);
  tcflush(serial_fd, TCIFLUSH);       // clear any junk first
}


char *romReadData(int serial_fd, uint16_t size) {
  activate_mode(serial_fd, "4\n");

  char *data = malloc(size);
  if (!data) {
    return NULL;
  }

  char sizestr[10] = {0};
  sprintf(sizestr, "%d\n", size);

  // we are sending size data in ASCII instead of raw bin. because I want this
  // to be usable by humans through the serial terminal
  write(serial_fd, sizestr, strnlen(sizestr, 10));
  tcdrain(serial_fd); // wait for data to be sent

  ssize_t received = 0;
  while (received < size) {
    ssize_t r = read(serial_fd, data + received, size - received);
    if (r < 0) {
      perror("read");
      free(data);
      return NULL;
    }
    if (r == 0) {
      fprintf(stderr, "Serial EOF reached prematurely\n");
      break;
    }
    received += r;
  }

  return data;
}

int romWriteFile(char *serial_device, char *filename) {
  int serial_fd = open_serial(serial_device);
  if (serial_fd < 0) {
    return EXIT_FAILURE;
  }

  FILE *file = fopen(filename, "rb");
  if (!file) {
    perror("fopen");
    close(serial_fd);
    return EXIT_FAILURE;
  }

  struct stat st;
  if (stat(filename, &st) != 0) {
    perror("stat");
    fclose(file);
    close(serial_fd);
    return EXIT_FAILURE;
  }

  uint8_t buffer[CHUNK_SIZE];
  size_t bytes_read;
  size_t total_sent = 0;

  printf("Starting file transfer: %s (%ld bytes)\n", filename, st.st_size);

  activate_mode(serial_fd, "3\n");

  while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
    ssize_t written = write(serial_fd, buffer, bytes_read);
    if (written < 0) {
      perror("write");
      break;
    }

    tcdrain(serial_fd); // wait for data to be sent

    total_sent += bytes_read;
    printf("Writing %ld bytes (%ld total) - ", bytes_read, total_sent);

    // Wait for Arduino response 'O' or 'D'
    char resp = 0;
    ssize_t r = 0;
    while (r <= 0 || (resp != 'O' && resp != 'D')) {
      r = read(serial_fd, &resp, 1);
      if (r < 0) {
        perror("read");
        goto cleanup;
      }
    }

    if (resp == 'O') {
      printf("OK\n");
    } else if (resp == 'D') {
      printf("Arduino sent done. Stopping transfer.\n");
      break;
    }
  }

  printf("File transfer complete. Verifying...\n");
  // while (getchar() != '\n');  // Wait for Enter

  rewind(file);

  int current_byte = 0;
  char *romdata = romReadData(serial_fd, st.st_size);
  while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
    if (memcmp(romdata + current_byte, buffer, bytes_read) == 0) {
      // Data matches
      // printf("Bytes %d-%zu OK\n", current_byte, current_byte + bytes_read - 1);
    } else {
      // Mismatch
      printf("Bytes %d-%zu DOES NOT MATCH!!!\n", current_byte, current_byte + bytes_read - 1);
    }

    current_byte += bytes_read;
  }

cleanup:
  free(romdata);
  fclose(file);
  close(serial_fd);
  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  if (argc == 4 && strcmp(argv[1], "write") == 0) {
    return romWriteFile(argv[2], argv[3]);

  } else if (argc == 4 && strcmp(argv[1], "dump") == 0) {
    int size = atoi(argv[3]);

    int serial_fd = open_serial(argv[2]);
    if (serial_fd < 0) {
      return EXIT_FAILURE;
    }

    char *data = romReadData(serial_fd, size);

    if (data) {
      close(serial_fd);
      fwrite(data, size, 1, stdout);
      free(data);
      return EXIT_SUCCESS;
    }
    
    close(serial_fd);
    return EXIT_FAILURE;

  } else {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s write <serial_device> <file_to_send>\n", argv[0]);
    fprintf(stderr, "  %s dump <serial_device> <size>\n", argv[0]);
    fprintf(stderr, "\nNote: dump outputs to stdout\n");
    return EXIT_FAILURE;
  }
}
