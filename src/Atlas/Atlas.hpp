#pragma once

#include <vector>
#include <string>
#include "../Image.h"

#include <TXPK/Core/IPacker.hpp>

class Atlas {
 public:
  Atlas();
  Atlas(const std::vector<Image>& pImages);
  ~Atlas() = default;

  static Atlas make_from_archive(const std::string& archive_par, const std::string& savepath_par);

 private:
  std::unique_ptr<txpk::IPacker> mTxPacker;
};