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
        owidth_(),
        oheight_(),
        name_(),
        path_(),
        is_team_color_(){};

  virtual ~Image();

  bool load(const std::vector<std::uint8_t>& par_buffer);
  bool load(const std::string& par_file);

  // For Swig.
  bool load_file(const std::string& par_file) { return load(par_file); }

  bool create(int par_width, int par_height, int par_channels = 4);
  bool create(int par_width, int par_height, float par_red, float par_green,
              float par_blue);  // RGB

  std::shared_ptr<Image> clone() const;

  bool save(const std::string& par_file);

  // Copy
  Image(const Image& rhs) = delete;
  Image& operator=(const Image& rhs) = delete;

  // Move
  Image(Image&& rhs) noexcept = delete;
  Image& operator=(Image&& rhs) noexcept = delete;

  // Error handling
  inline const bool has_error() const { return this == nullptr or has_error_; };
  inline const std::string& error() const { return error_; };

  // Image info accessors
  inline std::uint32_t id() const { return ilid_; }
  inline int width() const { return width_; }
  inline int height() const { return height_; }
  inline int owidth() const { return owidth_; }
  inline int oheight() const { return oheight_; }
  inline void owidth(int par_owidth) { owidth_ = par_owidth; }
  inline void oheight(int par_oheight) { oheight_ = par_oheight; }
  inline int bpp() const { return bpp_; }
  inline int deepth() const { return deepth_; }
  inline int channels() const { return channels_; }
  inline bool has_alpha() const { return channels_ > 3; }

  inline bool is_team_color() const { return is_team_color_; }
  inline void is_team_color(bool par_tc) { is_team_color_ = par_tc; }

  // Image Data
  std::uint8_t* data();
  inline std::uint32_t size() const { return static_cast<std::uint32_t>(width_) * height_ * bpp_; }

  /**
   * Image manipulation
   */
  bool clear_color(float pRed, float pGreen, float pBlue, float pAlpha);

  bool add_alpha();
  bool threedo_to_s3o();
  bool add_opaque_alpha();

  bool to_power_of_two();

  bool mirror();
  bool flip();

  bool blit(const std::shared_ptr<Image> par_src, int par_dx, int par_dy, int par_dz, int par_sx,
            int par_sy, int par_sz, int par_width, int par_height, int par_depth);

  /**
   * Name/path getter/setter
   */
  void name(const std::string& par_name) { name_ = par_name; }
  const std::string& name() const { return name_; }

  void path(const std::string& par_path) { path_ = par_path; }
  const std::string& path() const { return path_; }

 protected:
  std::uint32_t ilid_;

  bool has_error_;
  std::string error_;

  int width_, height_, bpp_, deepth_, channels_;

  int owidth_, oheight_;

  std::string name_, path_;

  bool is_team_color_;

  bool load_from_memory_(const std::vector<std::uint8_t>& par_buffer);
  void image_infos_();
};

typedef std::shared_ptr<Image> ImagePtr;
