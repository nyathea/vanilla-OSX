// main.c - macOS vanilla-pipe entry point
// Handles command-line arguments and main event loop

#include <arpa/inet.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "../def.h"
#include "../ports.h"
#include "wifi.h"

static volatile int running = 1;

static void nlprint(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
}

static void signal_handler(int sig) {
  nlprint("Received signal %d, shutting down...", sig);
  running = 0;
}

static void print_usage(const char *program_name) {
  nlprint("vanilla-pipe (macOS) - brokers a connection between Vanilla and the "
          "Wii U");
  nlprint("--------------------------------------------------------------------"
          "------------");
  nlprint("");
  nlprint("Usage: %s <-local | -udp> [wireless-interface]", program_name);
  nlprint("");
  nlprint("Options:");
  nlprint(
      "  -local    Use local Unix socket for IPC (recommended for local use)");
  nlprint("  -udp      Use UDP socket for IPC (for remote frontends)");
  nlprint("");
  nlprint("If no wireless interface is specified, the default Wi-Fi interface "
          "is used.");
  nlprint("");
}

int main(int argc, const char **argv) {
  // Check for root (may be required for Wi-Fi control)
  if (geteuid() != 0) {
    nlprint(
        "WARNING: vanilla-pipe may require root privileges for Wi-Fi control");
    // Don't exit - CoreWLAN might work without root for some operations
  }

  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  int udp_mode = 0;
  int local_mode = 0;
  const char *wireless_interface = NULL;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-udp")) {
      udp_mode = 1;
    } else if (!strcmp(argv[i], "-local")) {
      local_mode = 1;
    } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      print_usage(argv[0]);
      return 0;
    } else {
      wireless_interface = argv[i];
    }
  }

  if (udp_mode == local_mode) {
    nlprint("Error: Must specify either '-local' OR '-udp'");
    return 1;
  }

  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Initialize Wi-Fi
  nlprint("Initializing Wi-Fi interface...");
  if (wifi_init(wireless_interface) != 0) {
    nlprint("Failed to initialize Wi-Fi");
    return 1;
  }

  // Signal ready to parent process
  fprintf(stderr, "READY\n");
  fflush(stderr);

  nlprint("vanilla-pipe (macOS) ready and listening...");

  // Create socket for IPC
  int server_socket;
  if (local_mode) {
    server_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path) - 1,
             VANILLA_PIPE_LOCAL_SOCKET, VANILLA_PIPE_CMD_SERVER_PORT);
    unlink(addr.sun_path);

    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      nlprint("Failed to bind local socket: %s", addr.sun_path);
      wifi_cleanup();
      return 1;
    }
  } else {
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(VANILLA_PIPE_CMD_SERVER_PORT);

    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      nlprint("Failed to bind UDP socket");
      wifi_cleanup();
      return 1;
    }
  }

  // Set socket timeout
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  // Main event loop
  while (running) {
    vanilla_pipe_command_t cmd;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    ssize_t recv_len =
        recvfrom(server_socket, &cmd, sizeof(cmd), 0,
                 (struct sockaddr *)&client_addr, &client_addr_len);

    if (recv_len <= 0) {
      // Timeout or error, check if we should continue
      continue;
    }

    nlprint("Received command: 0x%02x", cmd.control_code);

    switch (cmd.control_code) {
    case VANILLA_PIPE_CC_SYNC: {
      // Sync with Wii U - scan for WiiU network
      char ssid[64];
      int result = wifi_scan_for_wiiu(ssid, sizeof(ssid));

      vanilla_pipe_command_t response;
      if (result == 0) {
        response.control_code = VANILLA_PIPE_CC_STATUS;
        response.status.status = htonl(VANILLA_SUCCESS);
      } else {
        response.control_code = VANILLA_PIPE_CC_STATUS;
        response.status.status = htonl(VANILLA_ERR_GENERIC);
      }

      sendto(server_socket, &response, sizeof(response), 0,
             (struct sockaddr *)&client_addr, client_addr_len);
      break;
    }

    case VANILLA_PIPE_CC_CONNECT: {
      // Connect to Wii U using provided credentials
      // TODO: Implement full connection flow
      nlprint("Connect request received");

      // Send bind acknowledgment first
      uint8_t ack = VANILLA_PIPE_CC_BIND_ACK;
      sendto(server_socket, &ack, sizeof(ack), 0,
             (struct sockaddr *)&client_addr, client_addr_len);
      break;
    }

    case VANILLA_PIPE_CC_UNBIND: {
      nlprint("Unbind request received");
      wifi_disassociate();
      break;
    }

    case VANILLA_PIPE_CC_QUIT: {
      nlprint("Quit request received");
      running = 0;
      break;
    }

    default:
      nlprint("Unknown command: 0x%02x", cmd.control_code);
      break;
    }
  }

  // Cleanup
  close(server_socket);
  wifi_cleanup();

  nlprint("vanilla-pipe shutdown complete");
  return 0;
}
