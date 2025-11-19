// // Minimal mock implementation to support x86 simulator control and HAL API

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>

#include "mock_hal_control.h"

#include "MplaneAlarms.h"
// // #include "MplanePerformanceMgmt.h"
#include "MplaneUplaneConf.h"
#include "MplaneSync.h"

// Internal mock state
static int g_sync_locked = 0; // 0=UNLOCKED, 1=LOCKED

// Software mgmt outcomes (textual status tracking)
static char g_sw_download_result[32] = "OK";
static char g_sw_install_result[32] = "OK";
static char g_sw_activate_result[32] = "OK";
static char g_sw_reset_result[32] = "OK";
static char g_sw_slot[64] = "";
static char g_sw_file[128] = "";

// Alarm callback pointer (local to this process)
static halmplane_oran_alarm_cb_t g_alarm_cb = NULL;

// Unix socket for alarm IPC
#define ALARM_SOCKET_PATH "/tmp/mplane_alarm_socket"
static pthread_t g_alarm_listener_thread;
static volatile int g_alarm_listener_running = 0;

// Alarm message structure for IPC
struct alarm_ipc_msg {
  uint16_t fault_id;
  char fault_source[64];
  int severity;
  int is_cleared;
  char fault_text[256];
  uint64_t fault_time;
};

// Socket listener thread function
static void* alarm_socket_listener(void* arg) {
  (void)arg;
  int server_fd, client_fd;
  struct sockaddr_un addr;
  struct alarm_ipc_msg msg;

  // Create socket
  server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("alarm_socket_listener: socket");
    return NULL;
  }

  // Remove old socket file if it exists
  unlink(ALARM_SOCKET_PATH);

  // Bind socket
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, ALARM_SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("alarm_socket_listener: bind");
    close(server_fd);
    return NULL;
  }

  // Listen
  if (listen(server_fd, 5) < 0) {
    perror("alarm_socket_listener: listen");
    close(server_fd);
    unlink(ALARM_SOCKET_PATH);
    return NULL;
  }

  // Make socket accessible by all users
  chmod(ALARM_SOCKET_PATH, 0666);

  fprintf(stderr, "[alarm_socket_listener] Listening on %s\n", ALARM_SOCKET_PATH);

  g_alarm_listener_running = 1;

  while (g_alarm_listener_running) {
    // Accept connection with timeout
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int ret = select(server_fd + 1, &readfds, NULL, NULL, &tv);
    if (ret < 0) {
      if (errno == EINTR) continue;
      perror("alarm_socket_listener: select");
      break;
    }
    if (ret == 0) continue;  // Timeout, check if still running

    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
      perror("alarm_socket_listener: accept");
      continue;
    }

    // Read alarm message
    ssize_t n = recv(client_fd, &msg, sizeof(msg), 0);
    if (n == sizeof(msg)) {
      // Call the registered callback
      if (g_alarm_cb) {
        halmplane_oran_alarm_t alarm;
        memset(&alarm, 0, sizeof(alarm));
        alarm.fault_id = msg.fault_id;
        alarm.fault_source = msg.fault_source;
        alarm.fault_severity = (halmplane_oran_fault_severity_t)msg.severity;
        alarm.is_cleared = msg.is_cleared;
        alarm.fault_text = msg.fault_text;
        alarm.fault_time = msg.fault_time;
        alarm.event_time = msg.fault_time;

        g_alarm_cb(&alarm, NULL);

        // Send acknowledgment
        const char* ack = "OK";
        send(client_fd, ack, strlen(ack), 0);
      } else {
        // No callback registered
        const char* err = "ERR:no_callback";
        send(client_fd, err, strlen(err), 0);
      }
    }

    close(client_fd);
  }

  close(server_fd);
  unlink(ALARM_SOCKET_PATH);
  fprintf(stderr, "[alarm_socket_listener] Stopped\n");
  return NULL;
}

// Carrier state callbacks
static halmplane_carrier_state_cb_t g_rx_carrier_cb = NULL;
static halmplane_carrier_state_cb_t g_tx_carrier_cb = NULL;

// SW callback
static mock_sw_cb_t g_sw_cb = NULL;

// Simple carrier state table
typedef struct {
  char name[64];
  carrier_state_t state;
  int in_use;
} carrier_entry_t;

#define MAX_CARRIERS 16
static carrier_entry_t g_rx_carriers[MAX_CARRIERS];
static carrier_entry_t g_tx_carriers[MAX_CARRIERS];

static carrier_entry_t* find_entry(carrier_entry_t* tbl, const char* name) {
  for (int i = 0; i < MAX_CARRIERS; ++i) {
    if (tbl[i].in_use && strncmp(tbl[i].name, name, sizeof(tbl[i].name)) == 0)
      return &tbl[i];
  }
  for (int i = 0; i < MAX_CARRIERS; ++i) {
    if (!tbl[i].in_use) {
      strncpy(tbl[i].name, name, sizeof(tbl[i].name) - 1);
      tbl[i].name[sizeof(tbl[i].name) - 1] = '\0';
      tbl[i].state = DISABLED;
      tbl[i].in_use = 1;
      return &tbl[i];
    }
  }
  return NULL;
}

void mock_hal_reset(void) {
  g_sync_locked = 0;
  g_alarm_cb = NULL;
  g_rx_carrier_cb = NULL;
  g_tx_carrier_cb = NULL;
  memset(g_rx_carriers, 0, sizeof(g_rx_carriers));
  memset(g_tx_carriers, 0, sizeof(g_tx_carriers));
  strncpy(g_sw_download_result, "OK", sizeof(g_sw_download_result) - 1);
  strncpy(g_sw_install_result, "OK", sizeof(g_sw_install_result) - 1);
  strncpy(g_sw_activate_result, "OK", sizeof(g_sw_activate_result) - 1);
  strncpy(g_sw_reset_result, "OK", sizeof(g_sw_reset_result) - 1);
  g_sw_slot[0] = '\0';
  g_sw_file[0] = '\0';
}

void mock_hal_set_sync_locked(bool locked) { g_sync_locked = locked ? 1 : 0; }
bool mock_hal_get_sync_locked(void) { return g_sync_locked ? true : false; }

int mock_hal_inject_alarm(
    uint16_t fault_id,
    const char* fault_source,
    int severity,
    bool is_cleared,
    const char* fault_text) {

  // Send alarm via Unix socket to the server process
  int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    perror("mock_hal_inject_alarm: socket");
    return 1;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, ALARM_SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("mock_hal_inject_alarm: connect");
    close(sock_fd);
    return 1;
  }

  // Prepare alarm message
  struct alarm_ipc_msg msg;
  memset(&msg, 0, sizeof(msg));
  msg.fault_id = fault_id;
  strncpy(msg.fault_source, fault_source ? fault_source : "SIM", sizeof(msg.fault_source) - 1);
  msg.severity = severity;
  msg.is_cleared = is_cleared;
  strncpy(msg.fault_text, fault_text ? fault_text : "sim alarm", sizeof(msg.fault_text) - 1);
  msg.fault_time = (uint64_t)time(NULL);

  // Send message
  if (send(sock_fd, &msg, sizeof(msg), 0) != sizeof(msg)) {
    perror("mock_hal_inject_alarm: send");
    close(sock_fd);
    return 1;
  }

  // Wait for acknowledgment
  char ack[32];
  ssize_t n = recv(sock_fd, ack, sizeof(ack) - 1, 0);
  if (n > 0) {
    ack[n] = '\0';
  }

  close(sock_fd);
  return 0;
}

int mock_hal_register_sw_cb(mock_sw_cb_t cb) { g_sw_cb = cb; return 0; }

static void emit_sw_event(const char* phase, const char* result) {
  if (!g_sw_cb) return;
  mock_sw_event_t ev = { phase, result, g_sw_slot[0] ? g_sw_slot : NULL,
                         g_sw_file[0] ? g_sw_file : NULL };
  g_sw_cb(&ev);
}

void mock_hal_set_sw_result(const char* phase, const char* result) {
  mock_hal_set_sw_result_ext(phase, result, NULL, NULL);
}

void mock_hal_set_sw_result_ext(
    const char* phase, const char* result, const char* slot, const char* file) {
  if (!phase || !result) return;
  if (slot) {
    strncpy(g_sw_slot, slot, sizeof(g_sw_slot) - 1);
    g_sw_slot[sizeof(g_sw_slot) - 1] = '\0';
  }
  if (file) {
    strncpy(g_sw_file, file, sizeof(g_sw_file) - 1);
    g_sw_file[sizeof(g_sw_file) - 1] = '\0';
  }
  if (strcmp(phase, "download") == 0) {
    strncpy(g_sw_download_result, result, sizeof(g_sw_download_result) - 1);
  } else if (strcmp(phase, "install") == 0) {
    strncpy(g_sw_install_result, result, sizeof(g_sw_install_result) - 1);
  } else if (strcmp(phase, "activate") == 0) {
    strncpy(g_sw_activate_result, result, sizeof(g_sw_activate_result) - 1);
  } else if (strcmp(phase, "reset") == 0) {
    strncpy(g_sw_reset_result, result, sizeof(g_sw_reset_result) - 1);
  }
  emit_sw_event(phase, result);
}

// int mock_hal_trigger_pm(
//     const char* object_name, const char* out_dir, const char* remote_dir) {
//   // Minimal stub: create a CSV file with a timestamped name
//   if (!object_name || !out_dir) return 1;
//   char path[512];
//   time_t now = time(NULL);
//   snprintf(path, sizeof(path), "%s/C%ld_%ld_%s.csv", out_dir, (long)now,
//            (long)now + 15, object_name);
//   FILE* f = fopen(path, "w");
//   if (!f) return 2;
//   fprintf(f, "timestamp,value\n");
//   for (int i = 0; i < 5; ++i) {
//     fprintf(f, "%ld,%d\n", (long)now + i, 42 + i);
//   }
//   fclose(f);

//   if (remote_dir && remote_dir[0]) {
//     const char* fname = strrchr(path, '/');
//     fname = fname ? fname + 1 : path;
//     char rpath[512];
//     snprintf(rpath, sizeof(rpath), "%s/%s", remote_dir, fname);
//     FILE* src = fopen(path, "rb");
//     if (src) {
//       FILE* dst = fopen(rpath, "wb");
//       if (dst) {
//         char buf[4096];
//         size_t n;
//         while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
//           fwrite(buf, 1, n, dst);
//         }
//         fclose(dst);
//       }
//       fclose(src);
//     }
//   }
//   return 0;
// }

int mock_hal_status(char* buf, size_t buflen) {
  if (!buf || buflen == 0) return 1;
  int n = snprintf(buf, buflen,
                   "{\"sync_locked\":%s,\"sw\":{\"download\":\"%s\",\"install\":\"%s\",\"activate\":\"%s\",\"reset\":\"%s\"}}",
                   g_sync_locked ? "true" : "false", g_sw_download_result,
                   g_sw_install_result, g_sw_activate_result,
                   g_sw_reset_result);
  return (n < 0 || (size_t)n >= buflen) ? 2 : 0;
}

// // ===================== HAL API implementations =====================

// Alarms
int halmplane_registerOranAlarmCallback(halmplane_oran_alarm_cb_t cb) {
  g_alarm_cb = cb;

  // Start the socket listener thread if not already running
  if (!g_alarm_listener_running) {
    if (pthread_create(&g_alarm_listener_thread, NULL, alarm_socket_listener, NULL) != 0) {
      perror("halmplane_registerOranAlarmCallback: pthread_create");
      return 1;
    }
    // Detach thread so it cleans up automatically
    pthread_detach(g_alarm_listener_thread);
  }

  return 0;
}

// Carrier callbacks
int halmplane_register_rx_carrier_state_cb(halmplane_carrier_state_cb_t cb) {
  g_rx_carrier_cb = cb;
  return 0;
}

int halmplane_register_tx_carrier_state_cb(halmplane_carrier_state_cb_t cb) {
  g_tx_carrier_cb = cb;
  return 0;
}

// Apply/validate carrier state changes
static int parse_new_state(const char* new_state, carrier_state_t* out) {
  if (!new_state || !out) return 1;
  if (strcmp(new_state, "ACTIVE") == 0) {
    *out = READY;
    return 0;
  }
  if (strcmp(new_state, "INACTIVE") == 0) {
    *out = DISABLED;
    return 0;
  }
  if (strcmp(new_state, "SLEEP") == 0) {
    *out = BUSY;
    return 0;
  }
  return 1;
}

int halmplane_tx_carrier_state_change(
    const char* name,
    uint64_t chbw,
    uint64_t center,
    double gain,
    const char* new_state,
    int do_apply) {
  (void)chbw; (void)center; (void)gain;
  carrier_state_t target;
  if (parse_new_state(new_state, &target)) return 1;
  if (target == READY && !g_sync_locked) {
    // Enforce sync precondition
    return 1;
  }
  fprintf(stderr,
          "tx_carrier_state_change name=%s state=%s do_apply=%d\n",
          name,
          new_state,
          do_apply);
  fflush(stderr);
  if (!do_apply) return 0;

  carrier_entry_t* e = find_entry(g_tx_carriers, name);
  if (!e) return 2;
  e->state = target;
  if (g_tx_carrier_cb) {
    g_tx_carrier_cb(name, target);
  }
  return 0;
}

int halmplane_rx_carrier_state_change(
    const char* name,
    uint64_t chbw,
    uint64_t center,
    double gain_correction,
    const char* new_state,
    int do_apply) {
  (void)chbw; (void)center; (void)gain_correction;
  carrier_state_t target;
  if (parse_new_state(new_state, &target)) return 1;
  if (target == READY && !g_sync_locked) {
    return 1;
  }
  if (!do_apply) return 0;

  carrier_entry_t* e = find_entry(g_rx_carriers, name);
  if (!e) return 2;
  e->state = target;
  if (g_rx_carrier_cb) {
    g_rx_carrier_cb(name, target);
  }
  return 0;
}

// EAXC and compression updates (accept all)
int halmplane_update_rx_eaxc(const char* endpoint_name, e_axcid_t* eaxc) {
  (void)endpoint_name; (void)eaxc; return 0;
}
int halmplane_update_tx_eaxc(const char* endpoint_name, e_axcid_t* eaxc) {
  (void)endpoint_name; (void)eaxc; return 0;
}
int halmplane_update_rx_endpoint_compression(
    const char* endpoint_name, compression_t* compression) {
  (void)endpoint_name; (void)compression; return 0;
}
int halmplane_update_tx_endpoint_compression(
    const char* endpoint_name, compression_t* compression) {
  (void)endpoint_name; (void)compression; return 0;
}
int halmplane_update_rx_endpoint_compression_dyn_config(
    const char* endpoint_name, dynamic_compression_configuration_t* config) {
  (void)endpoint_name; (void)config; return 0;
}
int halmplane_update_tx_endpoint_compression_dyn_config(
    const char* endpoint_name, dynamic_compression_configuration_t* config) {
  (void)endpoint_name; (void)config; return 0;
}

int halmplane_setUPlaneConfiguration(user_plane_configuration_t* cfg) {
  (void)cfg; return 0;
}

// // Performance Mgmt minimal APIs
// static halmplane_oran_perf_meas_cb_t g_perf_cb = NULL;
// int halmplane_registerOranPerfMeasCallback(
//     halmplane_oran_perf_meas_cb_t callback) {
//   g_perf_cb = callback;
//   return 0;
// }
// const halmplane_oran_perf_meas_cb_t get_perf_meas_cb_ptr(void) { return g_perf_cb; }

// int halmplane_getRssi(uint8_t interface, double* rssiValue) {
//   if (!rssiValue) return 1;
//   *rssiValue = -50.0 - (double)(interface % 4);
//   return 0;
// }
