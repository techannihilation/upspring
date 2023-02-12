#pragma once

#include <cstdint>
#include <vector>
#include <string>

class Image {
 public:
  // Constructors
  Image()
      : ilid_(),
        has_error_(true),
        error_(),
        width_(),
        height_(),
        bpp_(),
        deepth_(),
        channels_(){};

  bool load(const std::vector<std::uint8_t>& par_buffer);
  bool load(const std::string& par_file);

  bool create(int par_width, int par_height);
  bool create(int par_width, int par_height, float par_red, float par_green, float par_blue);  // RGB

  // Destructor
  virtual ~Image();

  // Copy
  Image(const Image& rhs) = default;
  Image& operator=(const Image& rhs);

  // Move
  Image(Image&& rhs) = default;
  Image& operator=(Image&& rhs) noexcept;

  // Error handling
  inline const bool has_error() const { return has_error_; };
  inline const std::string& error() const { return error_; };

  bool save(const std::string& pFile) const;

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
  const std::uint8_t* data() const;
  inline uint size() const { return static_cast<uint>(width_) * height_ * bpp_; }

  /**
   * Image manipulation
   */
  bool clear_color(float pRed, float pGreen, float pBlue, float pAlpha);

  bool alpha_to_zero();
  bool non_alpha_to_zero();
  bool mirror();
  bool flip();
  bool FlipNonDDS(const std::string& path);

  bool add_alpha();
  bool blit(const Image& pSrc, int pDx, int pDy, int pDz, int pSx, int pSy, int pSz, int pWidth,
            int pHeight, int pDepth);

 protected:
  uint ilid_;

  bool has_error_;
  std::string error_;

  int width_, height_, bpp_, deepth_, channels_;

  bool load_from_memory_(const std::vector<std::uint8_t>& par_buffer);
  void image_infos_();
};
