#pragma once

#include <string>

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

  void app_path(std::string par_app_path) { app_path_ = std::move(par_app_path); }
  std::string app_path() { return app_path_; }
};

};  // namespace ups
