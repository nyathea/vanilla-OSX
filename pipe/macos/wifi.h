#ifndef VANILLA_PIPE_MACOS_WIFI_H
#define VANILLA_PIPE_MACOS_WIFI_H

#include <stddef.h>
#include <stdint.h>

// Wi-Fi interface management
int wifi_init(const char *interface_name);
void wifi_cleanup(void);

// Scanning
int wifi_scan_for_wiiu(char *ssid_out, size_t ssid_out_len);

// Association
int wifi_associate(const char *ssid, const uint8_t *bssid, const uint8_t *psk);
int wifi_disassociate(void);

// Status
int wifi_is_connected(void);
int wifi_get_ip_address(char *ip_out, size_t ip_out_len);

// DHCP / Network configuration
int wifi_configure_network(const char *interface_name);

#endif // VANILLA_PIPE_MACOS_WIFI_H
