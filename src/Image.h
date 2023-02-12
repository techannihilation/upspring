#pragma once

#include "EditorDef.h"

class Image {
 public:
  // Constructors
  Image()
      : mILID(),
        mHasError(true),
        mError(),
        mWidth(),
        mHeight(),
        mBytesPerPixel(),
        mDeepth(),
        mChannels(){};
  Image(const std::vector<uchar>& pBuf);
  Image(const std::string& pFile);
  Image(int pWidth, int pHeight);
  Image(int pWidth, int pHeight, float pRed, float pGreen, float pBlue);  // RGB

  // Destructor
  virtual ~Image();

  // Copy
  Image(const Image& other) = default;
  Image& operator=(const Image& other);

  // Move
  Image(Image&& other) = default;
  Image& operator=(Image&& other) noexcept;

  // Error handling
  inline const bool HasError() const { return mHasError; };
  inline const std::string& Error() const { return mError; };

  bool Save(const std::string& pFile) const;

  // Image info accessors
  inline uint ID() const { return mILID; }
  inline int Width() const { return mWidth; }
  inline int Height() const { return mHeight; }
  inline int BytesPerPixel() const { return mBytesPerPixel; }
  inline int Deepth() const { return mDeepth; }
  inline int Channels() const { return mChannels; }
  inline bool HasAlpha() const { return mChannels > 3; }

  // Image Data
  const std::uint8_t* Data() const;
  inline uint Size() const { return static_cast<uint>(mWidth) * mHeight * mBytesPerPixel; }

  /**
   * Image manipulation
   */
  bool ClearColor(float pRed, float pGreen, float pBlue, float pAlpha);

  bool SetAlphaZero();
  bool SetNonAlphaZero();
  bool Mirror();
  bool Flip();
  bool FlipNonDDS(const std::string& path);

  bool AddAlpha();
  bool RemoveAlpha();
  bool Blit(const Image& pSrc, int pDx, int pDy, int pDz, int pSx, int pSy, int pSz, int pWidth,
            int pHeight, int pDepth);

 protected:
  uint mILID;

  bool mHasError;
  std::string mError;

  int mWidth, mHeight, mBytesPerPixel, mDeepth, mChannels;

 private:
  void LoadFromMemory_(const std::vector<uchar>& pBuf);
  void GetImageInfos_();
};
