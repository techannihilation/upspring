#pragma once

#include <list>
#include <string>

std::list<std::string>* FindFiles(const std::string& searchPattern, bool recursive = false,
                                  const std::string& path = std::string());
