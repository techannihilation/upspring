#pragma once

#include <string>
#include <filesystem>

namespace ups {

class config {
  config() : app_path_(){};

  std::string app_path_;

 public:
  static config& get() {
    static config instance;
    return instance;
  }

  // Singleton no copy.
  config(config const&) = delete;
  void operator=(config const&) = delete;

  void app_path(std::filesystem::path par_app_path) { app_path_ = std::filesystem::canonical(par_app_path); }
  std::filesystem::path app_path() { return app_path_; }
};

};  // namespace ups
