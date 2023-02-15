#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <string>

class Image {
 public:
  // Constructors
  Image()
      : ilid_(),
        has_error_(),
        error_(),
        width_(),
        height_(),
        bpp_(),
        deepth_(),
        channels_(),
        name_(),
        path_() {};

  virtual ~Image();

  bool load(const std::vector<std::uint8_t>& par_buffer);
  bool load(const std::string& par_file);

  bool create(int par_width, int par_height, int par_channels = 4);
  bool create(int par_width, int par_height, float par_red, float par_green,
              float par_blue);  // RGB

  std::shared_ptr<Image> clone() const;

  bool save(const std::string& par_file);

  // Copy
  Image(const Image& rhs) = delete;
  Image& operator=(const Image& rhs)= delete;

  // Move
  Image(Image&& rhs) noexcept = delete;
  Image& operator=(Image&& rhs) noexcept = delete;

  // Error handling
  inline const bool has_error() const { return this == nullptr or has_error_; };
  inline const std::string& error() const { return error_; };

  // Image info accessors
  inline uint id() const { return ilid_; }
  inline int width() const { return width_; }
  inline int height() const { return height_; }
  inline int bpp() const { return bpp_; }
  inline int deepth() const { return deepth_; }
  inline int channels() const { return channels_; }
  inline bool has_alpha() const { return channels_ > 3; }

  // Image Data
  std::uint8_t* data();
  inline uint size() const { return static_cast<uint>(width_) * height_ * bpp_; }

  /**
   * Image manipulation
   */
  bool clear_color(float pRed, float pGreen, float pBlue, float pAlpha);

  bool add_alpha();
  bool clear_none_alpha();
  bool add_transparent_alpha();

  bool mirror();
  bool flip();

  bool blit(const std::shared_ptr<Image> par_src, int par_dx, int par_dy, int par_dz, int par_sx, int par_sy, int par_sz,
            int par_width, int par_height, int par_depth);

  /**
   * Name/path getter/setter
   */
 void name(const std::string& par_name) { name_ = par_name; }
 const std::string& name() const { return name_; }

 void path(const std::string& par_path) { path_ = par_path; }
 const std::string& path() const { return path_; }

 protected:
  uint ilid_;

  bool has_error_;
  std::string error_;

  int width_, height_, bpp_, deepth_, channels_;

  std::string name_, path_;

  bool load_from_memory_(const std::vector<std::uint8_t>& par_buffer);
  void image_infos_();
};
