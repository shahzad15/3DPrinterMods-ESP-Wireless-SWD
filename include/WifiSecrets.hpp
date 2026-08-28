#pragma once

// Local provisioning file. It is intentionally excluded by .gitignore.
namespace local_wifi {
inline constexpr char kSsid[] = "Internet4G";
inline constexpr char kPassword[] = "Dell@224";

inline constexpr unsigned char kAddress[4] = {192, 168, 1, 192};
inline constexpr unsigned char kGateway[4] = {192, 168, 1, 254};
inline constexpr unsigned char kSubnet[4] = {255, 255, 255, 0};
inline constexpr unsigned char kDns[4] = {192, 168, 1, 254};
} // namespace local_wifi
