// wifi.m - CoreWLAN-based Wi-Fi management for macOS vanilla-pipe
// Objective-C implementation using Apple's CoreWLAN framework

#import <CoreWLAN/CoreWLAN.h>
#import <Foundation/Foundation.h>
#import <SystemConfiguration/SystemConfiguration.h>

#include "wifi.h"
#include <stdio.h>
#include <string.h>

// Global state
static CWWiFiClient *wifiClient = nil;
static CWInterface *wifiInterface = nil;
static NSString *currentSSID = nil;

// Logging helper (matches vanilla's logging style)
static void wifi_log(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
}

int wifi_init(const char *interface_name) {
  @autoreleasepool {
    wifiClient = [CWWiFiClient sharedWiFiClient];
    if (!wifiClient) {
      wifi_log("Failed to get CWWiFiClient");
      return -1;
    }

    if (interface_name && strlen(interface_name) > 0) {
      // Use specified interface
      NSString *ifName = [NSString stringWithUTF8String:interface_name];
      wifiInterface = [wifiClient interfaceWithName:ifName];
    } else {
      // Use default interface
      wifiInterface = [wifiClient interface];
    }

    if (!wifiInterface) {
      wifi_log("Failed to get Wi-Fi interface");
      return -1;
    }

    wifi_log("Initialized Wi-Fi interface: %s",
             [[wifiInterface interfaceName] UTF8String]);
    return 0;
  }
}

void wifi_cleanup(void) {
  @autoreleasepool {
    if (currentSSID) {
      currentSSID = nil;
    }
    wifiInterface = nil;
    wifiClient = nil;
  }
}

int wifi_scan_for_wiiu(char *ssid_out, size_t ssid_out_len) {
  @autoreleasepool {
    if (!wifiInterface) {
      wifi_log("Wi-Fi interface not initialized");
      return -1;
    }

    wifi_log("Scanning for Wi-Fi networks...");

    NSError *error = nil;
    NSSet<CWNetwork *> *networks =
        [wifiInterface scanForNetworksWithName:nil error:&error];

    if (error) {
      wifi_log("Scan failed: %s", [[error localizedDescription] UTF8String]);
      return -1;
    }

    wifi_log("Found %lu networks", (unsigned long)[networks count]);

    // Look for Wii U networks (they start with "WiiU" typically)
    for (CWNetwork *network in networks) {
      NSString *ssid = [network ssid];
      if (ssid) {
        wifi_log("  Found: %s (RSSI: %ld)", [ssid UTF8String],
                 (long)[network rssiValue]);

        // Check if this looks like a Wii U network
        // Wii U creates SSIDs like "WiiU<code>" during sync
        if ([ssid hasPrefix:@"WiiU"]) {
          wifi_log("  -> Wii U network detected!");
          strncpy(ssid_out, [ssid UTF8String], ssid_out_len - 1);
          ssid_out[ssid_out_len - 1] = '\0';
          return 0;
        }
      }
    }

    wifi_log("No Wii U networks found");
    return -1;
  }
}

int wifi_associate(const char *ssid, const uint8_t *bssid, const uint8_t *psk) {
  @autoreleasepool {
    if (!wifiInterface) {
      wifi_log("Wi-Fi interface not initialized");
      return -1;
    }

    NSString *ssidStr = [NSString stringWithUTF8String:ssid];

    // Scan to find the specific network
    NSError *error = nil;
    NSSet<CWNetwork *> *networks =
        [wifiInterface scanForNetworksWithName:ssidStr error:&error];

    if (error || [networks count] == 0) {
      wifi_log("Could not find network: %s", ssid);
      return -1;
    }

    CWNetwork *targetNetwork = [networks anyObject];

    // Convert PSK to string (assuming it's a hex-encoded 32-byte key)
    // For WPA2-PSK, we need to convert the raw PSK to a passphrase or use it
    // directly This is a simplification - real implementation may need
    // adjustment
    char pskHex[65];
    for (int i = 0; i < 32; i++) {
      sprintf(&pskHex[i * 2], "%02x", psk[i]);
    }
    pskHex[64] = '\0';
    NSString *password = [NSString stringWithUTF8String:pskHex];

    wifi_log("Attempting to associate with: %s", ssid);

    BOOL success = [wifiInterface associateToNetwork:targetNetwork
                                            password:password
                                               error:&error];

    if (!success || error) {
      wifi_log("Association failed: %s",
               error ? [[error localizedDescription] UTF8String]
                     : "unknown error");
      return -1;
    }

    currentSSID = ssidStr;
    wifi_log("Successfully associated with: %s", ssid);
    return 0;
  }
}

int wifi_disassociate(void) {
  @autoreleasepool {
    if (!wifiInterface) {
      return -1;
    }

    [wifiInterface disassociate];
    currentSSID = nil;
    wifi_log("Disassociated from Wi-Fi network");
    return 0;
  }
}

int wifi_is_connected(void) {
  @autoreleasepool {
    if (!wifiInterface) {
      return 0;
    }

    NSString *currentNetwork = [wifiInterface ssid];
    return currentNetwork != nil ? 1 : 0;
  }
}

int wifi_get_ip_address(char *ip_out, size_t ip_out_len) {
  @autoreleasepool {
    // Use SystemConfiguration to get IP address
    SCDynamicStoreRef store =
        SCDynamicStoreCreate(NULL, CFSTR("vanilla-pipe"), NULL, NULL);
    if (!store) {
      return -1;
    }

    NSString *interfaceName = [wifiInterface interfaceName];
    NSString *key = [NSString
        stringWithFormat:@"State:/Network/Interface/%@/IPv4", interfaceName];

    CFDictionaryRef ipInfo =
        SCDynamicStoreCopyValue(store, (__bridge CFStringRef)key);
    CFRelease(store);

    if (!ipInfo) {
      return -1;
    }

    NSArray *addresses = ((__bridge NSDictionary *)ipInfo)[@"Addresses"];
    CFRelease(ipInfo);

    if ([addresses count] > 0) {
      NSString *ip = addresses[0];
      strncpy(ip_out, [ip UTF8String], ip_out_len - 1);
      ip_out[ip_out_len - 1] = '\0';
      return 0;
    }

    return -1;
  }
}

int wifi_configure_network(const char *interface_name) {
  @autoreleasepool {
    // For now, we rely on macOS's built-in DHCP
    // The Wii U should assign us an IP via DHCP

    wifi_log("Network configuration: relying on system DHCP");

    // TODO: If needed, implement static IP configuration using
    // SCNetworkConfiguration The Wii U typically uses 192.168.1.x range

    return 0;
  }
}
