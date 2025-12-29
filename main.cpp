#include <idevice++/bindings.hpp>
#include <idevice++/core_device_proxy.hpp>
#include <idevice++/dvt/remote_server.hpp>
#include <idevice++/dvt/screenshot.hpp>
#include <idevice++/ffi.hpp>
#include <idevice++/heartbeat.hpp>
#include <idevice++/installation_proxy.hpp>
#include <idevice++/lockdown.hpp>
#include <idevice++/provider.hpp>
#include <idevice++/readwrite.hpp>
#include <idevice++/rsd.hpp>
#include <idevice++/usbmuxd.hpp>

#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <netinet/in.h>
#include <print>
#include <string>
#include <sys/socket.h>
#include <thread>

using namespace IdeviceFFI;
using namespace std;

class HeartBeatThread {
public:
  HeartBeatThread(HeartbeatClientHandle *heartbeat)
      : m_hb(Heartbeat::adopt(heartbeat)), m_stop(false),
        m_initialCompleted(false) {}

  ~HeartBeatThread() {
    stop();
    join();
  }

  void start() { m_thread = std::thread(&HeartBeatThread::run, this); }

  void stop() { m_stop.store(true); }

  void join() {
    if (m_thread.joinable())
      m_thread.join();
  }

  bool initialCompleted() const { return m_initialCompleted.load(); }

private:
  void run() {
    std::cerr << "Heartbeat thread started\n";
    try {
      uint64_t interval = 15;

      while (!m_stop.load()) {
        Result result = m_hb.get_marco(interval);
        if (result.is_err()) {
          std::cerr << "Failed to get marco: " << result.unwrap_err().message
                    << "\n";
          break;
        }

        interval = result.unwrap();
        std::cerr << "Received marco, new interval: " << interval << "\n";

        Result polo_result = m_hb.send_polo();
        if (polo_result.is_err()) {
          std::cerr << "Failed to send polo: "
                    << polo_result.unwrap_err().message << "\n";
          break;
        }
        std::cerr << "Sent polo successfully\n";

        interval += 5;
        m_initialCompleted.store(true);

        // sleep up to 'interval' seconds, but wake early if stopped
        for (uint64_t i = 0; i < interval && !m_stop.load(); ++i)
          std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    } catch (const std::exception &e) {
      std::cerr << "Heartbeat error: " << e.what() << "\n";
    }
  }

  Heartbeat m_hb;
  std::thread m_thread;
  std::atomic<bool> m_stop;
  std::atomic<bool> m_initialCompleted;
};

struct InitRes {
  IdeviceHandle *deviceHandle;
  AfcClientHandle *afcClient;
  bool success;
  HeartBeatThread *heartbeatThread;
};

void init_idescriptor_device(const std::string &ip,
                             const std::string &pairing_file_path,
                             InitRes &initRes) {

  // Creating a UsbmuxdConnectionHandle doesn't change anything
  // UsbmuxdConnectionHandle *usbmuxd_conn = nullptr;
  // UsbmuxdAddrHandle *addr_handle = nullptr;
  IdeviceProviderHandle *provider = nullptr;
  LockdowndClientHandle *lockdown = nullptr;
  IdeviceSocketHandle *socket = nullptr;
  AfcClientHandle *afc_client = nullptr;
  IdeviceHandle *deviceHandle = nullptr;
  HeartbeatClientHandle *heartbeat = nullptr;
  IdevicePairingFile *pairing_file = nullptr;
  HeartBeatThread *heartbeatThread = nullptr;
  IdeviceFfiError *err = nullptr;

  // err =
  //     idevice_usbmuxd_new_default_connection(0, &usbmuxd_conn);
  // if (err) {
  //   println("Failed to connect to usbmuxd");
  // }

  // err = idevice_usbmuxd_default_addr_new(&addr_handle);
  // if (err) {
  //   println("Failed to create address handle");
  //   goto cleanup;
  // }

  // Create IPv4 sockaddr
  struct sockaddr_in addr_in;
  memset(&addr_in, 0, sizeof(addr_in));
  addr_in.sin_family = AF_INET;
  addr_in.sin_port = htons(0); // Port doesn't matter for provider
  inet_pton(AF_INET, ip.c_str(), &addr_in.sin_addr);
  idevice_pairing_file_read(pairing_file_path.c_str(), &pairing_file);
  err = idevice_tcp_provider_new((const idevice_sockaddr *)&addr_in,
                                 const_cast<IdevicePairingFile *>(pairing_file),
                                 "foo", &provider);
  if (err) {
    println("Failed to create wireless provider");
    goto cleanup;
  }

  if (err) {
    println("Failed to create provider");
    goto cleanup;
  }

  err = lockdownd_connect(provider, &lockdown);
  if (err) {
    println("Failed to connect to lockdown");
    goto cleanup;
  }

  err = idevice_provider_get_pairing_file(provider, &pairing_file);
  if (err) {
    println("Failed to get pairing file");
    goto cleanup;
  }

  err = lockdownd_start_session(lockdown, pairing_file);
  if (err) {
    println("Failed to start lockdown session");
    goto cleanup;
  }

  uint16_t heartbeat_port;
  bool heartbeat_ssl;

  // Start heartbeat client to keep connection alive
  err = heartbeat_connect(provider, &heartbeat);
  if (err) {
    printf("Failed to start Heartbeat service");
    goto cleanup;
  }
  heartbeatThread = new HeartBeatThread(heartbeat);
  heartbeatThread->start();

  if (err) {
    println("Failed to connect to Heartbeat client");
    goto cleanup;
  }

  println("Heartbeat client created successfully");
  uint16_t afc_port;
  bool afc_ssl;
  err = lockdownd_start_service(lockdown, "com.apple.afc", &afc_port, &afc_ssl);
  if (err) {
    println("Failed to start AFC service");
    goto cleanup;
  }

  // 6. Create AFC client from provider
  err = afc_client_connect(provider, &afc_client);
  if (err) {
    println("Failed to create AFC client");
    goto cleanup;
  }

  initRes.success = true;
  initRes.deviceHandle = deviceHandle;
  initRes.afcClient = afc_client;
  initRes.heartbeatThread = heartbeatThread;
  return;
cleanup:
  println("Initialization failed");
  // Cleanup on error
}

int main(int argc, char **argv) {

  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <device_ip> <pairing_file>\n";
    return 1;
  }

  std::string target_ip = argv[1];
  std::string pairing_file = argv[2];
  bool wait = argc > 3 && std::string(argv[3]) == "--wait";

  InitRes initRes = {};
  init_idescriptor_device(target_ip, pairing_file, initRes);

  if (!initRes.success) {
    std::cerr << "Failed to initialize iDescriptor device\n";
    return 1;
  }

  while (initRes.heartbeatThread->initialCompleted() == false) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  if (wait) {
    println("Sleeping for 10 seconds");
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }

  // Uncomment to test AFC get file info
  // AfcFileInfo info = {};
  // afc_get_file_info(initRes.afcClient, "/DCIM", &info);

  // printf("Size: %zu, Blocks: %zu, Created: %lld, Modified: %lld\n",
  // info.size,
  //        info.blocks, info.creation, info.modified);

  char **info = nullptr;
  size_t count = 0;
  afc_list_directory(initRes.afcClient, "/", &info, &count);
  std::cout << "Contents of /:\n";
  if (count == 0) {
    std::cout << "  (empty)\n";
  } else {
    for (size_t i = 0; i < count; ++i) {
      std::cout << "  " << info[i] << "\n";
    }
  }

  return 0;
}